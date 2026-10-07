use indexmap::IndexMap;

use crate::superdbc::{DecodeError, Message, MessageKey, RawType, RawValue};

/// Owned telemetry, independent of the database that produced it.
#[derive(Debug, Clone)]
pub struct DecodedMessage {
    key: MessageKey,
    name: String,
    tx_node: String,
    signals: IndexMap<String, DecodedSignalValue>,
}

impl DecodedMessage {
    pub fn key(&self) -> MessageKey {
        self.key
    }

    pub fn name(&self) -> &str {
        &self.name
    }

    pub fn tx_node(&self) -> &str {
        &self.tx_node
    }

    pub fn signal(&self, name: &str) -> Option<&DecodedSignalValue> {
        self.signals.get(name)
    }

    pub fn iter(&self) -> impl Iterator<Item = (&str, &DecodedSignalValue)> {
        self.signals
            .iter()
            .map(|(name, value)| (name.as_str(), value))
    }

    pub fn len(&self) -> usize {
        self.signals.len()
    }

    pub fn is_empty(&self) -> bool {
        self.signals.is_empty()
    }
}

#[derive(Debug, Clone)]
pub struct DecodedSignalValue {
    physical: f64,
    raw: RawValue,
    enum_label: Option<String>,
}

impl DecodedSignalValue {
    pub fn physical(&self) -> f64 {
        self.physical
    }

    pub fn raw(&self) -> RawValue {
        self.raw
    }

    pub fn enum_label(&self) -> Option<&str> {
        self.enum_label.as_deref()
    }

    /// Exact raw integer for integer signals; rounded physical value for Float32.
    /// Float-to-integer conversion saturates, with NaN producing zero.
    pub fn int_rounded(&self) -> i128 {
        match self.raw {
            RawValue::Integer(raw) => raw,
            RawValue::Float32(_) => self.physical.round() as i128,
        }
    }
}

impl Message {
    /// Decode declared bytes, accepting and ignoring padding up to eight bytes.
    /// Non-finite Float32 values and out-of-limit physical values are telemetry.
    pub fn decode(&self, data: &[u8]) -> Result<DecodedMessage, DecodeError> {
        if data.len() < usize::from(self.length_bytes()) || data.len() > 8 {
            return Err(DecodeError::InvalidPayloadLength {
                key: self.key(),
                expected_min: self.length_bytes(),
                actual: data.len(),
            });
        }

        let data = &data[..usize::from(self.length_bytes())];
        let mut signals = IndexMap::with_capacity(self.signals.len());

        for signal in &self.signals {
            let bits = signal.codec.extract(data);
            let raw = match signal.raw_type() {
                RawType::Unsigned => RawValue::Integer(i128::from(bits)),
                RawType::Signed => RawValue::Integer(signal.codec.signed(bits)),
                RawType::Float32 => RawValue::Float32(f32::from_bits(bits as u32)),
            };
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

        Ok(DecodedMessage {
            key: self.key(),
            name: self.name().to_owned(),
            tx_node: self.transmitter().to_owned(),
            signals,
        })
    }
}
