//! Message identities, validated signal definitions, and presentation metadata.

use std::collections::{BTreeMap, HashMap};

use crate::frame::CanIdentity;

use crate::superdbc::{
    error::ParseError,
    extract::Codec,
    model::{MessageModel, SignalModel},
};

/// Complete lookup key within one active database version.
#[derive(Clone, Copy, Debug, PartialEq, Eq, Hash)]
pub struct MessageKey {
    /// Bus containing the message; numeric IDs may repeat across buses.
    pub bus_id: u8,
    /// CAN identifier including the standard/extended distinction.
    pub identity: CanIdentity,
}

/// Numeric interpretation of the signal bits before scaling and offset.
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum RawType {
    /// Unsigned integer of the declared bit width.
    Unsigned,
    /// Two's-complement signed integer of the declared bit width.
    Signed,
    /// IEEE-754 single-precision value occupying exactly 32 bits.
    Float32,
}

/// Unscaled wire value. `i128` holds every signed/unsigned 64-bit integer.
#[derive(Clone, Copy, Debug, PartialEq)]
pub enum RawValue {
    /// Exact signed or unsigned integer value, without scaling or offset.
    Integer(i128),
    /// IEEE-754 wire value, interpreted as a float rather than an integer bit pattern.
    Float32(f32),
}

/// Bit traversal within the CAN payload; selected independently for each signal.
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum ByteOrder {
    /// Start at the least significant signal bit and advance through increasing bit numbers.
    LittleEndian,
    /// Start at the most significant signal bit using Motorola sawtooth numbering.
    BigEndian,
}

/// Physical limits for presentation; the codec enforces wire representability.
#[derive(Clone, Copy, Debug, PartialEq)]
pub struct SignalLimits {
    /// Suggested minimum physical value.
    pub min: f64,
    /// Suggested maximum physical value.
    pub max: f64,
}

/// Presentation hint. Decimal precision in loaded definitions is 0..=7.
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum DisplayFormat {
    /// Hexadecimal presentation.
    Hex,
    /// Binary presentation.
    Binary,
    /// Integer presentation.
    Integer,
    /// Decimal presentation with the given number of fractional digits.
    Decimal(u8),
}

/// Message metadata and compiled signal codecs, validated during loading.
#[derive(Debug)]
pub struct Message {
    key: MessageKey,
    name: String,
    length_bytes: u8,
    transmitter: String,
    receivers: Vec<String>,
    nominal_period_ms: Option<u32>,
    priority: u8,
    description: String,
    /// Signal definitions in declaration order; indexes must remain consistent.
    pub signals: Vec<SignalDefinition>,
    /// Signal names mapped to positions in `signals`.
    pub signal_index: HashMap<String, usize>,
}

impl Message {
    /// Validate a deserialized message and compile its signal layouts.
    ///
    /// Used by the database loader; callers normally obtain definitions through
    /// [`crate::superdbc::database::Database`]. `context` identifies the message
    /// in validation errors. Cross-message uniqueness is checked by the loader.
    ///
    /// # Errors
    ///
    /// Returns [`ParseError::InvalidDefinition`] for invalid metadata or CAN
    /// identity, duplicate signal names, invalid signal definitions, overlapping
    /// bits, or layouts extending beyond the declared payload.
    pub fn compile(bus_id: u8, model: MessageModel, context: &str) -> Result<Self, ParseError> {
        nonempty(&model.message_name, context, "message_name")?;
        nonempty(&model.transmitter, context, "transmitter")?;

        for receiver in &model.receivers {
            nonempty(receiver, context, "receiver")?;
        }

        if model.length_bytes > 8 {
            return Err(ParseError::definition(
                context,
                "length_bytes must be 0..=8",
            ));
        }

        if model.priority > 5 {
            return Err(ParseError::definition(context, "priority must be 0..=5"));
        }

        if model.nominal_period_ms == Some(0) {
            return Err(ParseError::definition(
                context,
                "nominal_period_ms must be positive or null",
            ));
        }

        let identity = CanIdentity::new(model.id, model.is_extended_id)
            .map_err(|error| ParseError::definition(context, error.to_string()))?;
        let mut signals = Vec::with_capacity(model.signals.len());
        let mut signal_index = HashMap::with_capacity(model.signals.len());
        let mut occupied = 0u64;

        for signal in model.signals {
            let signal_context = format!("{context} / signal {:?}", signal.signal_name);

            if signal_index
                .insert(signal.signal_name.clone(), signals.len())
                .is_some()
            {
                return Err(ParseError::definition(
                    signal_context,
                    "duplicate signal name",
                ));
            }

            let signal = SignalDefinition::compile(signal, model.length_bytes, &signal_context)?;

            // Compare actual occupied wire bits, so overlaps are rejected even
            // when signals use different byte orders.
            if occupied & signal.codec.occupied != 0 {
                return Err(ParseError::definition(
                    signal_context,
                    "signal overlaps another signal's wire bits",
                ));
            }
            occupied |= signal.codec.occupied;
            signals.push(signal);
        }

        Ok(Self {
            key: MessageKey { bus_id, identity },
            name: model.message_name,
            length_bytes: model.length_bytes,
            transmitter: model.transmitter,
            receivers: model.receivers,
            nominal_period_ms: model.nominal_period_ms,
            priority: model.priority,
            description: model.description,
            signals,
            signal_index,
        })
    }

    /// Return the full lookup key, including bus ID and standard/extended CAN identity.
    pub fn key(&self) -> MessageKey {
        self.key
    }

    /// Return the CAN identity; use [`Self::key`] when the bus must also be identified.
    pub fn identity(&self) -> CanIdentity {
        self.key.identity
    }

    /// Return the message name declared in the artifact.
    pub fn name(&self) -> &str {
        &self.name
    }

    /// Return the declared payload length (0 through 8), also the length produced by encoding.
    pub fn length_bytes(&self) -> u8 {
        self.length_bytes
    }

    /// Return the transmitting node name declared in the artifact.
    pub fn transmitter(&self) -> &str {
        &self.transmitter
    }

    /// Borrow the declared receiving node names in artifact order.
    pub fn receivers(&self) -> &[String] {
        &self.receivers
    }

    /// Return the nominal send period in milliseconds, or `None` when unspecified; no sends are scheduled.
    pub fn nominal_period_ms(&self) -> Option<u32> {
        self.nominal_period_ms
    }

    /// Return the declared priority (0 through 5); the codec does not schedule or reorder traffic.
    pub fn priority(&self) -> u8 {
        self.priority
    }

    /// Return the message description, which may be empty.
    pub fn description(&self) -> &str {
        &self.description
    }

    /// Borrow signal definitions in declaration order for inspecting metadata or building input controls.
    pub fn signals(&self) -> &[SignalDefinition] {
        &self.signals
    }

    /// Look up a signal definition by its exact, case-sensitive name; return `None` if absent.
    pub fn signal(&self, name: &str) -> Option<&SignalDefinition> {
        self.signal_index
            .get(name)
            .map(|&index| &self.signals[index])
    }
}

/// Signal metadata. Bit layout, numeric ranges and choices are validated at load.
#[derive(Debug)]
pub struct SignalDefinition {
    name: String,
    description: String,
    data_type: String,
    raw_type: RawType,
    start_bit: u8,
    bit_length: u8,
    byte_order: ByteOrder,
    scale: f64,
    offset: f64,
    limits: Option<SignalLimits>,
    unit: String,
    display_format: Option<DisplayFormat>,
    choices: BTreeMap<i128, String>,
    /// Compiled layout used for extraction, insertion, and integer range checks.
    pub codec: Codec,
}

impl SignalDefinition {
    fn compile(model: SignalModel, payload_length: u8, context: &str) -> Result<Self, ParseError> {
        nonempty(&model.signal_name, context, "signal_name")?;
        nonempty(&model.data_type, context, "data_type")?;
        let raw_type = match model.raw_type.as_str() {
            "unsigned" => RawType::Unsigned,
            "signed" => RawType::Signed,
            "float32" => RawType::Float32,
            _ => {
                return Err(ParseError::definition(context, "unsupported raw_type"));
            }
        };
        let byte_order = match model.byte_order.as_str() {
            "little_endian" => ByteOrder::LittleEndian,
            "big_endian" => ByteOrder::BigEndian,
            _ => {
                return Err(ParseError::definition(context, "unsupported byte_order"));
            }
        };
        if !model.scale.is_finite() || model.scale == 0.0 || !model.offset.is_finite() {
            return Err(ParseError::definition(
                context,
                "scale must be finite and nonzero; offset must be finite",
            ));
        }

        let limits = model.limits.map(|limits| SignalLimits {
            min: limits.min,
            max: limits.max,
        });
        if limits.is_some_and(|limits| {
            !limits.min.is_finite() || !limits.max.is_finite() || limits.min > limits.max
        }) {
            return Err(ParseError::definition(
                context,
                "limits must be finite and ordered",
            ));
        }

        // Validate and compile once; runtime codecs can then work by byte
        // segments without checking the layout or walking individual bits.
        let codec = Codec::compile(
            model.start_bit,
            model.bit_length,
            byte_order,
            raw_type,
            payload_length,
            context,
        )?;
        let mut choices = BTreeMap::new();

        let raw_choices = model.choices.map(|choices| choices.0).unwrap_or_default();

        for (key, label) in raw_choices {
            // The canonical decimal syntax also prevents two textual keys
            // (e.g. "01" and "1") from aliasing the same numeric value.
            let raw = key.parse::<i128>().map_err(|_| {
                ParseError::definition(context, format!("invalid integer choice key {key:?}"))
            })?;

            if raw.to_string() != key
                || raw < codec.min
                || raw > codec.max
                || raw_type == RawType::Float32
            {
                return Err(ParseError::definition(
                    context,
                    format!(
                        "choice key {key:?} is noncanonical or outside the integer signal range"
                    ),
                ));
            }

            choices.insert(raw, label);
        }

        // Presentation hints are parsed into metadata; they never affect
        // raw extraction, scaling, or encoder range checks.
        let display_format = match model.display_format.as_deref() {
            None => None,
            Some("hex") => Some(DisplayFormat::Hex),
            Some("binary") => Some(DisplayFormat::Binary),
            Some("integer") => Some(DisplayFormat::Integer),
            Some(format @ ("0f" | "1f" | "2f" | "3f" | "4f" | "5f" | "6f" | "7f")) => {
                let precision = format.as_bytes()[0] - b'0';
                Some(DisplayFormat::Decimal(precision))
            }
            Some(format) => {
                return Err(ParseError::definition(
                    context,
                    format!("unsupported display_format {format:?}"),
                ));
            }
        };

        Ok(Self {
            name: model.signal_name,
            description: model.description,
            data_type: model.data_type,
            raw_type,
            start_bit: model.start_bit,
            bit_length: model.bit_length,
            byte_order,
            scale: model.scale,
            offset: model.offset,
            limits,
            unit: model.unit,
            display_format,
            choices,
            codec,
        })
    }

    /// Return the signal name used by encoding inputs and decoded-result lookup.
    pub fn name(&self) -> &str {
        &self.name
    }

    /// Return the signal description, which may be empty.
    pub fn description(&self) -> &str {
        &self.description
    }

    /// Return the declared logical data-type name; [`Self::raw_type`] determines wire interpretation.
    pub fn data_type(&self) -> &str {
        &self.data_type
    }

    /// Return the numeric wire type before physical scaling and offset.
    pub fn raw_type(&self) -> RawType {
        self.raw_type
    }

    /// Return the payload bit index (0 through 63), with each byte numbered LSB first.
    ///
    /// Bits 0 through 7 are in the first byte, 8 through 15 in the second, and so on.
    /// For little endian this is the signal's LSB. For big endian it is the MSB;
    /// traversal descends within each byte, then jumps from bit 0 to bit 7 of
    /// the next byte (Motorola sawtooth numbering).
    pub fn start_bit(&self) -> u8 {
        self.start_bit
    }

    /// Return the number of occupied wire bits (1 through 64; exactly 32 for Float32).
    pub fn bit_length(&self) -> u8 {
        self.bit_length
    }

    /// Return the bit traversal order for this signal; a message may mix signal byte orders.
    pub fn byte_order(&self) -> ByteOrder {
        self.byte_order
    }

    /// Return the finite, nonzero multiplier in `physical = raw * scale + offset`; it may be negative.
    pub fn scale(&self) -> f64 {
        self.scale
    }

    /// Return the finite additive offset in `physical = raw * scale + offset`.
    pub fn offset(&self) -> f64 {
        self.offset
    }

    /// Return optional physical bounds for presentation or consumer-side checks.
    ///
    /// Encoding enforces wire representability, not these limits; decoding also
    /// retains telemetry outside these bounds.
    pub fn limits(&self) -> Option<SignalLimits> {
        self.limits
    }

    /// Return the physical unit label, which may be empty for a unitless signal.
    pub fn unit(&self) -> &str {
        &self.unit
    }

    /// Return the optional presentation hint, including decimal precisions 0 through 7.
    ///
    /// The codec does not format values or change their numeric interpretation.
    pub fn display_format(&self) -> Option<DisplayFormat> {
        self.display_format
    }

    /// Iterate over raw integer values and their labels in ascending numeric order.
    ///
    /// Use these mappings for enumerated controls or displays. The keys are
    /// unscaled integers, not physical values; Float32 signals have no choices.
    pub fn choices(&self) -> impl Iterator<Item = (i128, &str)> {
        self.choices
            .iter()
            .map(|(&raw, label)| (raw, label.as_str()))
    }

    /// Look up an enum label by its exact, unscaled integer value.
    ///
    /// Returns `None` for an unmapped value. Scaling and offset are not applied
    /// to the lookup key.
    pub fn choice_label(&self, raw: i128) -> Option<&str> {
        self.choices.get(&raw).map(String::as_str)
    }
}

/// Check that a definition field contains at least one character.
///
/// Used during loading to attach `context` and the field name to an error.
/// This does not trim whitespace or otherwise normalize the value.
///
/// # Errors
///
/// Returns [`ParseError::InvalidDefinition`] when `value` is empty.
pub fn nonempty(value: &str, context: &str, field: &str) -> Result<(), ParseError> {
    if value.is_empty() {
        Err(ParseError::definition(
            context,
            format!("{field} must not be empty"),
        ))
    } else {
        Ok(())
    }
}
