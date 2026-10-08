//! Physical and exact raw encoding methods for validated message definitions.

use crate::superdbc::{
    error::EncodeError,
    message::{Message, RawType, RawValue, SignalDefinition},
};

impl Message {
    /// Encode named physical values into a payload of the declared byte length.
    ///
    /// Supply every signal exactly once, in any order. Each input is converted with
    /// `(physical - offset) / scale`. Integer raw values are rounded with ties away
    /// from zero; Float32 raw values are rounded to `f32` precision before packing.
    /// Unused payload bits are zero. Physical limits and display formats are metadata
    /// and do not constrain this operation; wire representability does.
    ///
    /// `f64` cannot represent every integer above 2^53. Use [`Self::encode_raw`] for
    /// exact large integers or when the input is already an unscaled wire value.
    /// Values are never silently clamped or defaulted.
    ///
    /// # Errors
    ///
    /// Returns an error for unknown, duplicate, or missing signal names, non-finite
    /// physical values or inverse-scaled results, integer values outside the wire
    /// range after rounding, or conversion overflowing Float32.
    ///
    /// # Example
    ///
    /// For a message containing only an unsigned `temperature` signal with scale
    /// 0.1 and offset 0, physical 25.0 and raw integer 250 encode the same payload:
    ///
    /// ```no_run
    /// use daqcore::superdbc::error::EncodeError;
    /// use daqcore::superdbc::message::{Message, RawValue};
    ///
    /// # fn example(message: &Message) -> Result<(), EncodeError> {
    /// let physical = message.encode(&[("temperature", 25.0)])?;
    /// let raw = message.encode_raw(&[("temperature", RawValue::Integer(250))])?;
    /// assert_eq!(physical, raw);
    /// # Ok(())
    /// # }
    /// ```
    pub fn encode(&self, values: &[(&str, f64)]) -> Result<Vec<u8>, EncodeError> {
        self.encode_values(values, |signal, physical| {
            // Undo physical interpretation before converting to integer or IEEE
            // bits; Float32 signals also have scale and offset.
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

                    // Casting through i128 preserves negative two's-complement
                    // bits; insertion masks them to the declared signal width.
                    Ok(rounded as i128 as u64)
                }
            }
        })
    }

    /// Encode exact, unscaled integers or finite Float32 wire values.
    ///
    /// No scaling or offset is applied. Use this for exact counters, bitfields, or
    /// constructing test payloads. Supply every signal exactly once, in any order.
    /// [`RawValue::Integer`] serves both signed and unsigned integer signals and is
    /// checked against their bit-width range; [`RawValue::Float32`] serves Float32
    /// signals and preserves the finite value's bit pattern, including signed zero.
    ///
    /// Returns the declared payload length with unused bits zero. Physical limits
    /// and display formats do not affect encoding. See [`Self::encode`] for a
    /// comparison with physical encoding.
    ///
    /// # Errors
    ///
    /// Returns an error for unknown, duplicate, or missing signals, a mismatched
    /// raw-value type, an integer outside the signal's wire range, or a non-finite
    /// Float32 value. Decoding accepts non-finite telemetry, but encoding rejects it.
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

                // Keep the low two's-complement bits for signed values. The
                // compiled segments select only the signal's declared width.
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
        // Both APIs share name validation and packing. T is f64 or RawValue;
        // Copy lets slots own values from the borrowed input, while the supplied
        // conversion function handles only the numeric differences.
        let mut ordered = vec![None; self.signals.len()];

        // Resolve arbitrary input order into definition positions. Replacing an
        // occupied slot detects duplicates without another set of names.
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

        // Validated layouts are disjoint, so insertion can OR bits into this
        // zeroed classic-CAN buffer; gaps remain zero in the final payload.
        let mut data = [0u8; 8];

        // Empty slots identify missing signals. Convert and pack in declaration
        // order so the two entry points use the same completeness checks.
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
