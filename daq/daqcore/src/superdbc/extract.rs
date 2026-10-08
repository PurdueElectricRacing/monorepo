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

/// Validated byte segments and integer bounds for one signal layout.
#[derive(Debug)]
pub struct Codec {
    segments: Vec<Segment>,
    /// Bitmap of occupied payload bits, used to reject overlapping signals.
    pub occupied: u64,
    /// Inclusive minimum integer value; not a Float32 numeric bound.
    pub min: i128,
    /// Inclusive maximum integer value; not a Float32 numeric bound.
    pub max: i128,
    sign_bit: u64,
    sign_modulus: i128,
}

impl Codec {
    /// Compile a signal's bit traversal into byte-sized extraction/insertion segments.
    ///
    /// `start` and `length` are bit positions and widths; `payload_length` is in
    /// bytes. Little endian starts at the LSB; big endian starts at the MSB using
    /// Motorola sawtooth numbering. `context` identifies the signal in errors.
    /// The loader uses the occupied-bit bitmap to check cross-signal overlaps.
    ///
    /// # Errors
    ///
    /// Returns [`ParseError::InvalidDefinition`] for an invalid start or width,
    /// a Float32 width other than 32, or bits outside the declared payload.
    /// The caller must supply a classic-CAN payload length no larger than eight.
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

            // Adjacent bits within a byte share one mask and shift pair. This
            // keeps the runtime path byte-based even for unaligned signals.
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
            // Motorola numbering descends to bit 0, then jumps to bit 7 of
            // the following byte; little endian simply advances one bit.
            wire_bit = match order {
                ByteOrder::LittleEndian => wire_bit + 1,
                ByteOrder::BigEndian if wire_bit % 8 == 0 => wire_bit + 15,
                ByteOrder::BigEndian => wire_bit - 1,
            };
        }

        // i128 can hold both unsigned 64-bit maxima and the 2^64 modulus
        // needed to interpret signed values without overflowing.
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

    /// Extract unscaled bits into a `u64` using the precompiled byte segments.
    ///
    /// Used after payload length validation; signed and Float32 interpretation
    /// are separate operations. Unused high bits of the result are zero.
    ///
    /// # Panics
    ///
    /// Panics if `data` does not contain every byte referenced by this layout.
    pub fn extract(&self, data: &[u8]) -> u64 {
        let mut value = 0;

        for segment in &self.segments {
            let wire_bits = (data[segment.byte] >> segment.wire_shift) & segment.mask;
            let value_bits = u64::from(wire_bits) << segment.value_shift;
            value |= value_bits;
        }

        value
    }

    /// Interpret extracted bits as a two's-complement integer of the compiled width.
    ///
    /// Use for signed signals after [`Self::extract`]. The `i128` result preserves
    /// every 64-bit signed value. Bits outside the compiled width must be zero.
    pub fn signed(&self, bits: u64) -> i128 {
        let value = i128::from(bits);

        // Subtract 2^width when the sign bit is set. Widening first also makes
        // this work for a full 64-bit signal without sign-extension shifts.
        if bits & self.sign_bit != 0 {
            value - self.sign_modulus
        } else {
            value
        }
    }

    /// OR raw bits into the payload using the precompiled byte segments.
    ///
    /// Only the compiled signal width is used. Callers must range-check values
    /// and supply a zeroed buffer with disjoint signal layouts: this operation
    /// does not clear preexisting bits and is not an in-place signal update.
    ///
    /// # Panics
    ///
    /// Panics if `data` does not contain every byte referenced by this layout.
    pub fn insert(&self, data: &mut [u8], bits: u64) {
        for segment in &self.segments {
            // All signal bits are disjoint and the output was zero-initialized.
            let value_bits = (bits >> segment.value_shift) as u8 & segment.mask;
            let wire_bits = value_bits << segment.wire_shift;
            data[segment.byte] |= wire_bits;
        }
    }
}
