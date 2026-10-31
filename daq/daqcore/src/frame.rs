//! Transport-neutral frames; owned by the receiving thread until moved to its caller.

use crate::{can, superdbc, time};
use std::{error, fmt};

/// A 29-bit CAN identifier.
#[derive(Debug, Clone, Copy, PartialEq, Eq, PartialOrd, Ord, Hash)]
pub struct CanIdentity(u32);

/// A CAN ID that exceeds the 29-bit limit.
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub struct InvalidCanId {
    pub id: u32,
}

impl fmt::Display for InvalidCanId {
    fn fmt(&self, formatter: &mut fmt::Formatter<'_>) -> fmt::Result {
        write!(formatter, "CAN ID 0x{:X} is out of range", self.id)
    }
}

impl error::Error for InvalidCanId {}

impl CanIdentity {
    pub fn new(id: u32) -> Result<Self, InvalidCanId> {
        if id > can::EXTENDED_ID_MASK {
            return Err(InvalidCanId { id });
        }

        Ok(Self(id))
    }

    pub fn raw_id(self) -> u32 {
        self.0
    }
}

impl fmt::Display for CanIdentity {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        write!(f, "0x{:03X}", self.0)
    }
}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum FrameKind {
    Data,
    Remote,
    Fd { bit_rate_switched: bool },
}

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct CanFrame {
    pub identity: CanIdentity,
    pub kind: FrameKind,
    /// CAN DLC code (0..8 classic, 0..15 FD).
    pub dlc: u8,
    pub data: Vec<u8>,
}

impl CanFrame {
    pub fn data(identity: CanIdentity, data: Vec<u8>) -> Result<Self, String> {
        if data.len() > 8 {
            return Err("classic CAN payload exceeds 8 bytes".into());
        }

        Ok(Self {
            identity,
            kind: FrameKind::Data,
            dlc: data.len() as u8,
            data,
        })
    }
}

#[derive(Debug, Clone)]
pub struct ParsedFrame {
    pub timestamp: time::Time,
    pub kind: FrameKind,
    pub dlc: u8,
    pub identity: CanIdentity,
    pub raw_bytes: Vec<u8>,
    pub decoded: Option<superdbc::decode::DecodedMessage>,
}

/// Borrowed decoded view; no frame or decoded-message clone is required for projections.
pub struct DecodedFrame<'a> {
    pub timestamp: time::Time,
    pub identity: CanIdentity,
    pub raw_bytes: &'a [u8],
    pub decoded: &'a superdbc::decode::DecodedMessage,
}

impl ParsedFrame {
    pub fn decoded_view(&self) -> Option<DecodedFrame<'_>> {
        Some(DecodedFrame {
            timestamp: self.timestamp,
            identity: self.identity,
            raw_bytes: &self.raw_bytes,
            decoded: self.decoded.as_ref()?,
        })
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn can_identity_accepts_full_29_bit_range() -> Result<(), InvalidCanId> {
        CanIdentity::new(0)?;
        CanIdentity::new(can::EXTENDED_ID_MASK)?;
        let identity = CanIdentity::new(0x1ABCDEF)?;
        assert_eq!(identity.raw_id(), 0x1ABCDEF);
        Ok(())
    }

    #[test]
    fn can_identity_rejects_30th_bit() {
        let error = CanIdentity::new(can::EXTENDED_ID_MASK + 1)
            .expect_err("bit 29 is outside the 29-bit limit");
        assert_eq!(error, InvalidCanId { id: 0x20000000 });
    }

    #[test]
    fn can_identity_orders_numerically() {
        let low = CanIdentity::new(0x100).expect("in range");
        let high = CanIdentity::new(0x1FFFFFFF).expect("in range");
        assert!(low < high);
    }
}
