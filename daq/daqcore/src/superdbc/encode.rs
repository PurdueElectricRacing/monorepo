use crate::superdbc::{
    error::EncodeError,
    message::{Message, RawType, RawValue, SignalDefinition},
};

impl Message {
    /// Encode physical values, rounding integer raw values with ties away from
    /// zero. For exact large integers use [`Self::encode_raw`] instead.
    pub fn encode(&self, values: &[(&str, f64)]) -> Result<Vec<u8>, EncodeError> {
        self.encode_values(values, |signal, physical| {
            let numeric_raw = (physical - signal.offset()) / signal.scale();

            if !physical.is_finite() || !numeric_raw.is_finite() {
                return Err(EncodeError::NonFiniteValue {
                    key: self.key(),
                    signal: signal.name().to_owned(),
                });
            }
            match signal.raw_type() {
                RawType::Float32 => {
                    let raw = numeric_raw as f32;

                    if !raw.is_finite() {
                        return Err(EncodeError::Float32Overflow {
                            key: self.key(),
                            signal: signal.name().to_owned(),
                        });
                    }

                    Ok(u64::from(raw.to_bits()))
                }
                _ => {
                    let rounded = numeric_raw.round();
                    // Upper-exclusive powers of two remain exact in f64 even
                    // when a 64-bit maximum integer itself does not.
                    if rounded < signal.codec.min as f64 || rounded >= (signal.codec.max + 1) as f64
                    {
                        return Err(EncodeError::PhysicalOutOfRange {
                            key: self.key(),
                            signal: signal.name().to_owned(),
                            physical,
                        });
                    }

                    Ok(rounded as i128 as u64)
                }
            }
        })
    }

    /// Encode exact unscaled integers or finite Float32 wire values.
    pub fn encode_raw(&self, values: &[(&str, RawValue)]) -> Result<Vec<u8>, EncodeError> {
        self.encode_values(values, |signal, raw| match (signal.raw_type(), raw) {
            (RawType::Unsigned | RawType::Signed, RawValue::Integer(value)) => {
                if value < signal.codec.min || value > signal.codec.max {
                    return Err(EncodeError::RawOutOfRange {
                        key: self.key(),
                        signal: signal.name().to_owned(),
                        value,
                        min: signal.codec.min,
                        max: signal.codec.max,
                    });
                }

                Ok(value as u64)
            }
            (RawType::Float32, RawValue::Float32(value)) => {
                if !value.is_finite() {
                    return Err(EncodeError::NonFiniteValue {
                        key: self.key(),
                        signal: signal.name().to_owned(),
                    });
                }

                Ok(u64::from(value.to_bits()))
            }
            _ => Err(EncodeError::TypeMismatch {
                key: self.key(),
                signal: signal.name().to_owned(),
                expected: signal.raw_type(),
            }),
        })
    }

    fn encode_values<T: Copy>(
        &self,
        values: &[(&str, T)],
        encode: impl Fn(&SignalDefinition, T) -> Result<u64, EncodeError>,
    ) -> Result<Vec<u8>, EncodeError> {
        let mut ordered = vec![None; self.signals.len()];

        for &(name, value) in values {
            let &index = self
                .signal_index
                .get(name)
                .ok_or_else(|| EncodeError::UnknownSignal {
                    key: self.key(),
                    signal: name.to_owned(),
                })?;

            if ordered[index].replace(value).is_some() {
                return Err(EncodeError::DuplicateSignal {
                    key: self.key(),
                    signal: name.to_owned(),
                });
            }
        }

        let mut data = [0u8; 8];

        for (signal, value) in self.signals.iter().zip(ordered) {
            let value = value.ok_or_else(|| EncodeError::MissingSignal {
                key: self.key(),
                signal: signal.name().to_owned(),
            })?;
            signal.codec.insert(&mut data, encode(signal, value)?);
        }

        Ok(data[..usize::from(self.length_bytes())].to_vec())
    }
}
