//! Transport-neutral frames; owned by the receiving thread until moved to its caller.
use crate::{Time, can};
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum FrameKind {
    Data,
    Remote,
    Fd { bit_rate_switched: bool },
}
#[derive(Debug, Clone, PartialEq, Eq)]
pub struct CanFrame {
    pub msg_id: u32,
    pub is_msg_id_extended: bool,
    pub kind: FrameKind,
    /// CAN DLC code (0..8 classic, 0..15 FD).
    pub dlc: u8,
    pub data: Vec<u8>,
}
impl CanFrame {
    pub fn data(msg_id: u32, extended: bool, data: Vec<u8>) -> Result<Self, String> {
        if msg_id
            > if extended {
                can::EXTENDED_ID_MASK
            } else {
                can::STANDARD_ID_MASK
            }
        {
            return Err("CAN ID is out of range".into());
        }
        if data.len() > 8 {
            return Err("classic CAN payload exceeds 8 bytes".into());
        }
        Ok(Self {
            msg_id,
            is_msg_id_extended: extended,
            kind: FrameKind::Data,
            dlc: data.len() as u8,
            data,
        })
    }
    pub fn decode_id(&self) -> u32 {
        self.msg_id
            | if self.is_msg_id_extended {
                can::EXTENDED_ID_FLAG
            } else {
                0
            }
    }
}
#[derive(Debug, Clone)]
pub struct ParsedFrame {
    pub timestamp: Time,
    pub kind: FrameKind,
    pub dlc: u8,
    pub msg_id: u32,
    pub is_msg_id_extended: bool,
    pub raw_bytes: Vec<u8>,
    pub decoded: Option<can_decode::DecodedMessage>,
}

/// Borrowed decoded view; no frame or decoded-message clone is required for projections.
pub struct DecodedFrame<'a> {
    pub timestamp: Time,
    pub msg_id: u32,
    pub is_msg_id_extended: bool,
    pub raw_bytes: &'a [u8],
    pub decoded: &'a can_decode::DecodedMessage,
}
impl ParsedFrame {
    /// Identity key compatible with DBC IDs; distinguishes standard and extended frames.
    pub fn identity(&self) -> u32 {
        self.msg_id
            | if self.is_msg_id_extended {
                can::EXTENDED_ID_FLAG
            } else {
                0
            }
    }

    pub fn decoded_view(&self) -> Option<DecodedFrame<'_>> {
        Some(DecodedFrame {
            timestamp: self.timestamp,
            msg_id: self.msg_id,
            is_msg_id_extended: self.is_msg_id_extended,
            raw_bytes: &self.raw_bytes,
            decoded: self.decoded.as_ref()?,
        })
    }
}
