//! Owned decoded values, retaining both physical values and raw wire values.

use indexmap::IndexMap;

use crate::superdbc::{
    error::DecodeError,
    message::{Message, MessageKey, RawType, RawValue},
};

/// A decoded frame whose names, labels, and signal values are fully owned.
///
/// Signals retain declaration order.
#[derive(Debug, Clone)]
pub struct DecodedMessage {
    key: MessageKey,
    name: String,
    tx_node: String,
    signals: IndexMap<String, DecodedSignalValue>,
}

impl DecodedMessage {
    /// Return the original bus ID and standard/extended CAN identity.
    pub fn key(&self) -> MessageKey {
        self.key
    }

    /// Borrow the message name copied from the definition when decoding.
    pub fn name(&self) -> &str {
        &self.name
    }

    /// Borrow the transmitting node name copied from the definition when decoding.
    pub fn tx_node(&self) -> &str {
        &self.tx_node
    }

    /// Look up a decoded signal by its exact, case-sensitive name; return `None` if absent.
    pub fn signal(&self, name: &str) -> Option<&DecodedSignalValue> {
        self.signals.get(name)
    }

    /// Iterate over borrowed signal names and values in definition declaration order.
    pub fn iter(&self) -> impl Iterator<Item = (&str, &DecodedSignalValue)> {
        self.signals
            .iter()
            .map(|(name, value)| (name.as_str(), value))
    }

    /// Return the number of decoded signals, including signals with enum labels.
    pub fn len(&self) -> usize {
        self.signals.len()
    }

    /// Return whether the message definition contained no signals.
    pub fn is_empty(&self) -> bool {
        self.signals.is_empty()
    }
}

/// A signal's scaled physical value, original raw value, and optional enum label.
///
/// Raw integers are exact even when conversion to a physical `f64` loses precision.
#[derive(Debug, Clone)]
pub struct DecodedSignalValue {
    physical: f64,
    raw: RawValue,
    enum_label: Option<String>,
}

impl DecodedSignalValue {
    /// Return `raw * scale + offset` as a physical `f64` for plots or numeric displays.
    ///
    /// Large integers can lose precision here; use [`Self::raw`] when exactness
    /// matters. Non-finite float telemetry or arithmetic overflow may yield NaN
    /// or infinity, and declared physical limits are not enforced.
    pub fn physical(&self) -> f64 {
        self.physical
    }

    /// Return the unscaled wire value for exact inspection or raw encoding.
    ///
    /// Integers retain their full signed or unsigned 64-bit value in `i128`.
    /// Float32 values retain the interpreted IEEE-754 value, including non-finite
    /// telemetry, which encoders reject.
    pub fn raw(&self) -> RawValue {
        self.raw
    }

    /// Borrow the choice label matched against the exact raw integer during decoding.
    ///
    /// Returns `None` for unmapped values and all Float32 signals.
    pub fn enum_label(&self) -> Option<&str> {
        self.enum_label.as_deref()
    }

    /// Return an integer suitable for integer, hexadecimal, or binary presentation.
    ///
    /// Integer signals return their exact unscaled raw integer. Float32 signals
    /// instead round the scaled physical value with ties away from zero, then
    /// convert to `i128`. That conversion saturates at `i128::MIN`/`i128::MAX`,
    /// including infinities, with NaN producing zero. Use [`Self::raw`] for the
    /// original Float32 value rather than this presentation conversion.
    pub fn int_rounded(&self) -> i128 {
        match self.raw {
            RawValue::Integer(raw) => raw,
            RawValue::Float32(_) => self.physical.round() as i128,
        }
    }
}

impl Message {
    /// Decode a payload using this definition, returning fully owned signal values.
    ///
    /// Accepts the declared byte length through eight bytes; trailing padding is
    /// ignored. Signals appear in declaration order.
    /// 
    /// Integer values are extracted exactly, signed values use two's complement,
    /// and Float32 bits are interpreted before applying `raw * scale + offset`.
    /// Choice labels use exact raw integers rather than scaled physical values.
    ///
    /// Non-finite Float32 values and out-of-limit physical values are retained as
    /// telemetry. Large raw integers may lose precision in the physical `f64`, but
    /// remain exact in [`DecodedSignalValue::raw`]. Results remain usable after the
    /// database or input payload is dropped.
    ///
    /// # Errors
    ///
    /// Returns [`DecodeError::InvalidPayloadLength`] when the payload is shorter
    /// than the declared length or longer than eight bytes. Layout validation was
    /// already performed when loading the definition.
    pub fn decode(&self, data: &[u8]) -> Result<DecodedMessage, DecodeError> {
        if data.len() < usize::from(self.length_bytes()) || data.len() > 8 {
            return Err(DecodeError::InvalidPayloadLength {
                key: self.key(),
                expected_min: self.length_bytes(),
                actual: data.len(),
            });
        }

        // Restrict extraction to declared bytes; padding belongs to the log or
        // transport and must not alter the decoded message.
        let data = &data[..usize::from(self.length_bytes())];
        let mut signals = IndexMap::with_capacity(self.signals.len());

        for signal in &self.signals {
            let bits = signal.codec.extract(data);
            let raw = match signal.raw_type() {
                RawType::Unsigned => RawValue::Integer(i128::from(bits)),
                RawType::Signed => RawValue::Integer(signal.codec.signed(bits)),
                RawType::Float32 => RawValue::Float32(f32::from_bits(bits as u32)),
            };
            // Keep the exact raw value alongside the approximate physical f64.
            // Enum keys refer to wire integers, so resolve labels before scaling.
            let (numeric_raw, enum_label) = match raw {
                RawValue::Integer(value) => {
                    (value as f64, signal.choice_label(value).map(str::to_owned))
                }
                RawValue::Float32(value) => (f64::from(value), None),
            };
            signals.insert(
                signal.name().to_owned(),
                DecodedSignalValue {
                    physical: numeric_raw * signal.scale() + signal.offset(),
                    raw,
                    enum_label,
                },
            );
        }

        // Own names and labels so consumers can retain frames across a runtime
        // database replacement without borrowing its definitions.
        Ok(DecodedMessage {
            key: self.key(),
            name: self.name().to_owned(),
            tx_node: self.transmitter().to_owned(),
            signals,
        })
    }
}
