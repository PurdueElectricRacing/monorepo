pub const EXTENDED_ID_FLAG: u32 = 0x80000000;
pub const STANDARD_ID_MASK: u32 = 0x7FF;
pub const EXTENDED_ID_MASK: u32 = 0x1FFFFFFF;

// Converts a can_dbc::MessageId to a u32, setting the extended ID flag if it's an extended ID.
// The extended ID flag is the highest bit (32nd bit) of the u32.
// Standard IDs (11 bits) will have this bit unset, while extended IDs (29 bits) will have this bit set.
// Generally use this version when interfacing with `can_decode`.
pub fn can_dbc_to_u32_with_extid_flag(msg_id: &can_dbc::MessageId) -> u32 {
    match msg_id {
        can_dbc::MessageId::Standard(id) => *id as u32,
        can_dbc::MessageId::Extended(id) => *id | EXTENDED_ID_FLAG,
    }
}

// Converts a can_dbc::MessageId to a u32 without setting the extended ID flag.
// Ex: a 29-bit extended ID will use at most 29 bits while the other version of this function
// would set the highest bit to indicate it's an extended ID.
// Generally use this version when showing output to the user or logging.
pub fn can_dbc_to_u32_without_extid_flag(msg_id: &can_dbc::MessageId) -> u32 {
    match msg_id {
        can_dbc::MessageId::Standard(id) => *id as u32 & STANDARD_ID_MASK,
        can_dbc::MessageId::Extended(id) => *id & EXTENDED_ID_MASK,
    }
}
