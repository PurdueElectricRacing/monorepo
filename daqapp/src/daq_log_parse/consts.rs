use crate::messages::BusName;

// identity bit masks
pub const IS_EID_MASK: u32 = 0x20000000;
pub const MAX_JUMP_MS: u32 = 300_000; // 300 seconds
pub const BUS_ID_MASK: u32 = 0xC0000000; // BUS ID is in the highest 2 bits of the identity
pub const RAW_CAN_ID_MASK: u32 = 0x1FFF_FFFF; // 29-bit raw CAN ID field

pub const BIN_WIDTH_MS: u32 = 1;

pub fn bus_name_from_identity(identity: u32) -> BusName {
    match ((identity & BUS_ID_MASK) >> 30) as u8 {
        1 => BusName::VCAN,
        2 => BusName::MCAN,
        3 => BusName::SCAN,
        _ => BusName::XCAN, // defeault to XCAN for 0 and any other unexpected values
    }
}

pub fn identity_with_bus_id(identity: u32, bus_name: BusName) -> u32 {
    (bus_name as u32) << 30 | (identity & RAW_CAN_ID_MASK)
}

pub fn can_id_from_identity(identity: u32) -> u32 {
    identity & RAW_CAN_ID_MASK
}
