//! Byte segments compiled once. The runtime path never walks individual bits.
use crate::superdbc::{
    error::ParseError,
    message::{ByteOrder, RawType},
};

#[derive(Debug)]
struct Segment {
    byte: usize,
    wire_shift: u8,
    value_shift: u8,
    mask: u8,
}

#[derive(Debug)]
pub struct Codec {
    segments: Vec<Segment>,
    pub occupied: u64,
    pub min: i128,
    pub max: i128,
    sign_bit: u64,
    sign_modulus: i128,
}

impl Codec {
    pub fn compile(
        start: u8,
        length: u8,
        order: ByteOrder,
        raw_type: RawType,
        payload_length: u8,
        context: &str,
    ) -> Result<Self, ParseError> {
        if start > 63 || !(1..=64).contains(&length) {
            return Err(ParseError::definition(
                context,
                "start_bit must be 0..=63 and bit_length 1..=64",
            ));
        }

        if raw_type == RawType::Float32 && length != 32 {
            return Err(ParseError::definition(
                context,
                "Float32 requires exactly 32 bits",
            ));
        }

        let mut segments: Vec<Segment> = Vec::new();
        let mut wire_bit = u16::from(start);
        let mut occupied = 0;

        for i in 0..length {
            if wire_bit >= u16::from(payload_length) * 8 {
                return Err(ParseError::definition(
                    context,
                    "signal occupies bits outside the declared payload",
                ));
            }
            occupied |= 1u64 << wire_bit;
            let value_shift = match order {
                ByteOrder::LittleEndian => i,
                ByteOrder::BigEndian => length - 1 - i,
            };
            let byte = usize::from(wire_bit / 8);
            let wire_shift = (wire_bit % 8) as u8;

            if let Some(last) = segments.last_mut().filter(|last| last.byte == byte) {
                last.wire_shift = last.wire_shift.min(wire_shift);
                last.value_shift = last.value_shift.min(value_shift);
                last.mask = ((u16::from(last.mask) << 1) | 1) as u8;
            } else {
                segments.push(Segment {
                    byte,
                    wire_shift,
                    value_shift,
                    mask: 1,
                });
            }
            wire_bit = match order {
                ByteOrder::LittleEndian => wire_bit + 1,
                ByteOrder::BigEndian if wire_bit % 8 == 0 => wire_bit + 15,
                ByteOrder::BigEndian => wire_bit - 1,
            };
        }

        let (min, max) = match raw_type {
            RawType::Signed => (-(1i128 << (length - 1)), (1i128 << (length - 1)) - 1),
            _ => (0, (1i128 << length) - 1),
        };
        Ok(Self {
            segments,
            occupied,
            min,
            max,
            sign_bit: 1u64 << (length - 1),
            sign_modulus: 1i128 << length,
        })
    }

    pub fn extract(&self, data: &[u8]) -> u64 {
        let mut value = 0;

        for segment in &self.segments {
            let wire_bits = (data[segment.byte] >> segment.wire_shift) & segment.mask;
            let value_bits = u64::from(wire_bits) << segment.value_shift;
            value |= value_bits;
        }

        value
    }

    pub fn signed(&self, bits: u64) -> i128 {
        let value = i128::from(bits);

        if bits & self.sign_bit != 0 {
            value - self.sign_modulus
        } else {
            value
        }
    }

    pub fn insert(&self, data: &mut [u8], bits: u64) {
        for segment in &self.segments {
            // All signal bits are disjoint and the output was zero-initialized.
            let value_bits = (bits >> segment.value_shift) as u8 & segment.mask;
            let wire_bits = value_bits << segment.wire_shift;
            data[segment.byte] |= wire_bits;
        }
    }
}
