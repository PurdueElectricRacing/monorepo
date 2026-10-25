// identity bit masks
pub const IS_EID_MASK: u32 = 0x20000000;
pub const MAX_JUMP_MS: u32 = 300_000; // 300 seconds
pub const BUS_ID_MASK: u32 = 0xC0000000; // BUS ID is in the 2 highest bits of the identity
pub const BUS_ID_OFFSET: u8 = 30; // Lowest bit of BUS ID is 30 bits after the first

pub const BIN_WIDTH_MS: u32 = 1;
