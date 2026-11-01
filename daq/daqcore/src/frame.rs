//! Transport-neutral frames; owned by the receiving thread until moved to its caller.

use crate::{superdbc, time};

/// A 11-bit or 29-bit CAN identifier.
pub type CanIdentity = u32;

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

/// A frame to transmit, with its required CAN wire framing.
#[derive(Debug, Clone, PartialEq, Eq)]
pub struct CanFrameTx {
    pub frame: CanFrame,
    pub is_extended_id: bool,
}

impl CanFrameTx {
    pub fn data(
        identity: CanIdentity,
        is_extended_id: bool,
        data: Vec<u8>,
    ) -> Result<Self, String> {
        Ok(Self {
            frame: CanFrame::data(identity, data)?,
            is_extended_id,
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
