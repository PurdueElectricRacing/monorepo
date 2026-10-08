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
    pub bus_id: u8,
    pub identity: CanIdentity,
}

#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum RawType {
    Unsigned,
    Signed,
    Float32,
}

/// Unscaled wire value. `i128` holds every signed/unsigned 64-bit integer.
#[derive(Clone, Copy, Debug, PartialEq)]
pub enum RawValue {
    Integer(i128),
    Float32(f32),
}

#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum ByteOrder {
    LittleEndian,
    BigEndian,
}

/// Physical limits for presentation; the codec enforces wire representability.
#[derive(Clone, Copy, Debug, PartialEq)]
pub struct SignalLimits {
    pub min: f64,
    pub max: f64,
}

/// Presentation hint. Decimal precision in loaded definitions is 0..=7.
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum DisplayFormat {
    Hex,
    Binary,
    Integer,
    Decimal(u8),
}

/// Immutable metadata and compiled signal codecs.
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
    pub signals: Vec<SignalDefinition>,
    pub signal_index: HashMap<String, usize>,
}

impl Message {
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

    pub fn key(&self) -> MessageKey {
        self.key
    }

    pub fn identity(&self) -> CanIdentity {
        self.key.identity
    }

    pub fn name(&self) -> &str {
        &self.name
    }

    pub fn length_bytes(&self) -> u8 {
        self.length_bytes
    }

    pub fn transmitter(&self) -> &str {
        &self.transmitter
    }

    pub fn receivers(&self) -> &[String] {
        &self.receivers
    }

    pub fn nominal_period_ms(&self) -> Option<u32> {
        self.nominal_period_ms
    }

    pub fn priority(&self) -> u8 {
        self.priority
    }

    pub fn description(&self) -> &str {
        &self.description
    }

    pub fn signals(&self) -> &[SignalDefinition] {
        &self.signals
    }

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

    pub fn name(&self) -> &str {
        &self.name
    }

    pub fn description(&self) -> &str {
        &self.description
    }

    pub fn data_type(&self) -> &str {
        &self.data_type
    }

    pub fn raw_type(&self) -> RawType {
        self.raw_type
    }

    pub fn start_bit(&self) -> u8 {
        self.start_bit
    }

    pub fn bit_length(&self) -> u8 {
        self.bit_length
    }

    pub fn byte_order(&self) -> ByteOrder {
        self.byte_order
    }

    pub fn scale(&self) -> f64 {
        self.scale
    }

    pub fn offset(&self) -> f64 {
        self.offset
    }

    pub fn limits(&self) -> Option<SignalLimits> {
        self.limits
    }

    pub fn unit(&self) -> &str {
        &self.unit
    }

    pub fn display_format(&self) -> Option<DisplayFormat> {
        self.display_format
    }

    pub fn choices(&self) -> impl Iterator<Item = (i128, &str)> {
        self.choices
            .iter()
            .map(|(&raw, label)| (raw, label.as_str()))
    }

    pub fn choice_label(&self, raw: i128) -> Option<&str> {
        self.choices.get(&raw).map(String::as_str)
    }
}

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
