use std::fmt;

pub const EXTENDED_ID_FLAG: u32 = 0x8000_0000;
pub const STANDARD_ID_MASK: u32 = 0x7ff;
pub const EXTENDED_ID_MASK: u32 = 0x1fff_ffff;

#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub struct FrameError(pub &'static str);
impl fmt::Display for FrameError {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        f.write_str(self.0)
    }
}
impl std::error::Error for FrameError {}

#[derive(Clone, Copy, Debug, PartialEq, Eq, PartialOrd, Ord, Hash)]
pub struct BusId(u8);
impl BusId {
    pub fn new(id: u8) -> Result<Self, FrameError> {
        if id <= 7 {
            Ok(Self(id))
        } else {
            Err(FrameError("bus ID exceeds 7"))
        }
    }
    pub const fn raw(self) -> u8 {
        self.0
    }
}

#[derive(Clone, Copy, Debug, PartialEq, Eq, PartialOrd, Ord, Hash)]
pub struct MessageId(u32);
impl MessageId {
    pub fn from_parts(is_extended: bool, id: u32) -> Result<Self, FrameError> {
        let mask = if is_extended {
            EXTENDED_ID_MASK
        } else {
            STANDARD_ID_MASK
        };
        if id > mask {
            return Err(FrameError("CAN ID exceeds its standard/extended range"));
        }
        Ok(Self(id | if is_extended { EXTENDED_ID_FLAG } else { 0 }))
    }
    pub fn from_wire_u32(wire: u32) -> Result<Self, FrameError> {
        if wire & !(EXTENDED_ID_FLAG | EXTENDED_ID_MASK) != 0 {
            return Err(FrameError("reserved CAN ID bits are set"));
        }
        Self::from_parts(wire & EXTENDED_ID_FLAG != 0, wire & EXTENDED_ID_MASK)
    }
    pub const fn to_wire_u32(self) -> u32 {
        self.0
    }
    pub const fn raw(self) -> u32 {
        self.0 & EXTENDED_ID_MASK
    }
    pub const fn is_extended(self) -> bool {
        self.0 & EXTENDED_ID_FLAG != 0
    }
}

#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub struct CanFrame {
    pub id: MessageId,
    pub bus: Option<BusId>,
    data: [u8; 8],
    len: u8,
    remote: bool,
}
impl CanFrame {
    pub fn new(id: MessageId, data: &[u8]) -> Result<Self, FrameError> {
        if data.len() > 8 {
            return Err(FrameError("classic CAN payload exceeds 8 bytes"));
        }
        let mut bytes = [0; 8];
        bytes[..data.len()].copy_from_slice(data);
        Ok(Self {
            id,
            bus: None,
            data: bytes,
            len: data.len() as u8,
            remote: false,
        })
    }
    pub fn remote(id: MessageId, dlc: u8) -> Result<Self, FrameError> {
        if dlc > 8 {
            return Err(FrameError("remote DLC exceeds 8"));
        }
        Ok(Self {
            id,
            bus: None,
            data: [0; 8],
            len: dlc,
            remote: true,
        })
    }
    pub fn data(&self) -> &[u8] {
        if self.remote {
            &[]
        } else {
            &self.data[..self.len as usize]
        }
    }
    pub const fn len(&self) -> u8 {
        self.len
    }
    pub const fn is_empty(&self) -> bool {
        self.len == 0
    }
    pub const fn is_remote(&self) -> bool {
        self.remote
    }
    pub fn with_bus(mut self, bus: BusId) -> Self {
        self.bus = Some(bus);
        self
    }

    /// Legacy timestamped_frame_t: bus in bit 31, EID in bit 30, bit 29 reserved.
    pub fn from_log_identity(identity: u32, data: &[u8; 8]) -> Result<Self, FrameError> {
        if identity & 0x2000_0000 != 0 {
            return Err(FrameError("reserved log identity bit is set"));
        }
        let id = MessageId::from_parts(identity & 0x4000_0000 != 0, identity & EXTENDED_ID_MASK)?;
        Ok(Self::new(id, data)?.with_bus(BusId((identity >> 31) as u8)))
    }
    pub fn log_identity(&self, default_bus: BusId) -> Result<u32, FrameError> {
        let bus = self.bus.unwrap_or(default_bus).raw();
        if bus > 1 {
            return Err(FrameError("legacy logs support only bus slots 0 and 1"));
        }
        if self.remote {
            return Err(FrameError("legacy logs cannot represent remote frames"));
        }
        Ok(self.id.raw()
            | if self.id.is_extended() {
                0x4000_0000
            } else {
                0
            }
            | (bus as u32) << 31)
    }
}
