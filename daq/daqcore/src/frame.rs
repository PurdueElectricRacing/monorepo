//! Transport-neutral frames; owned by the receiving thread until moved to its caller.
use crate::{Time, can};

/// Complete CAN identity. Ordering follows the flagged DBC representation.
#[derive(Debug, Clone, Copy, PartialEq, Eq, PartialOrd, Ord, Hash)]
pub struct CanIdentity(u32);

/// An ID that exceeds the width allowed by its standard/extended format.
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub struct InvalidCanId {
    pub id: u32,
    pub extended: bool,
}

impl std::fmt::Display for InvalidCanId {
    fn fmt(&self, formatter: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        let format = if self.extended {
            "extended"
        } else {
            "standard"
        };

        write!(formatter, "{format} CAN ID 0x{:X} is out of range", self.id)
    }
}

impl std::error::Error for InvalidCanId {}

impl CanIdentity {
    pub fn new(id: u32, extended: bool) -> Result<Self, InvalidCanId> {
        let maximum_id = if extended {
            can::EXTENDED_ID_MASK
        } else {
            can::STANDARD_ID_MASK
        };

        if id > maximum_id {
            return Err(InvalidCanId { id, extended });
        }

        let identity_flag = if extended { can::EXTENDED_ID_FLAG } else { 0 };

        Ok(Self(id | identity_flag))
    }

    pub fn raw_id(self) -> u32 {
        self.0 & can::EXTENDED_ID_MASK
    }

    pub fn is_extended(self) -> bool {
        self.0 & can::EXTENDED_ID_FLAG != 0
    }

    pub fn dbc_id(self) -> u32 {
        self.0
    }
}

impl std::fmt::Display for CanIdentity {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        let format = if self.is_extended() {
            "extended"
        } else {
            "standard"
        };

        write!(f, "0x{:03X} ({format})", self.raw_id())
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
    pub timestamp: Time,
    pub kind: FrameKind,
    pub dlc: u8,
    pub identity: CanIdentity,
    pub raw_bytes: Vec<u8>,
    pub decoded: Option<can_decode::DecodedMessage>,
}

/// Borrowed decoded view; no frame or decoded-message clone is required for projections.
pub struct DecodedFrame<'a> {
    pub timestamp: Time,
    pub identity: CanIdentity,
    pub raw_bytes: &'a [u8],
    pub decoded: &'a can_decode::DecodedMessage,
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
