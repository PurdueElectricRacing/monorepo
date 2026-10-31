// identity bit masks
pub const MAX_JUMP_MS: u32 = 300_000; // 300 seconds
pub const BUS_ID_MASK: u32 = 0x78000000; // BUS ID is in the 3 highest bits of the identity
pub const BUS_ID_OFFSET: u8 = 29; // Lowest bit of BUS ID is 29 bits after the first

pub const BIN_WIDTH_MS: u32 = 1;
