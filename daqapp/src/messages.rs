use crate::connection;
use std::fmt;

pub enum MsgFromUi {
    DbcSelected {
        bus_name: BusName,
        path: std::path::PathBuf,
    },
    Connect(connection::ConnectionSource),
    AddSendMessage(AddSendMessage),
    DeleteSendMessage { msg_id: u32 },
    UpdateLogFolder(std::path::PathBuf),
}

pub enum MsgFromCan {
    ParsedMessage(ParsedMessage),
    UnparsedMessage(UnparsedMessage),
    Disconnection,
    ConnectionSuccessful,
    ConnectionFailed(String),
    MessageSent {
        msg_id: u32,
        timestamp: chrono::DateTime<chrono::Local>,
        amount_left: Option<SendAmount>,
    },
    BusLoad {
        load_1s: f32,
        load_5s: f32,
        load_10s: f32,
        load_30s: f32,
    },
}

#[derive(Clone, Copy, Debug, PartialEq)]
// whoever is code reviewing this pls tell me to delete this later but also read below
// use xcan as default bus when serial driver, and when parsing since you only ever try to use one dbc
// try to guess the can bus name from the name of the dbc file if it is vcan/scan/mcan, or just stick with xcan
pub enum BusName {
    XCAN = 0, // used as default for unknown bus
    VCAN = 1,
    MCAN = 2,
    SCAN = 3,
}

// can also use crate strum for enum to string conversion, but manual implementation of display trait also works?
// idk how much you care about adding a dependency just for this small thing
impl fmt::Display for BusName {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        match self {
            BusName::XCAN => write!(f, "XCAN"),
            BusName::VCAN => write!(f, "VCAN"),
            BusName::MCAN => write!(f, "MCAN"),
            BusName::SCAN => write!(f, "SCAN"),
        }
    }
}

#[derive(Clone, Copy, Debug)]
pub enum SendAmount {
    Infinite { period: usize },
    Once,
    Finite { amount: usize, period: usize },
}

impl SendAmount {
    pub fn subtract_one(&self) -> Option<Self> {
        match self {
            SendAmount::Infinite { period } => Some(SendAmount::Infinite { period: *period }),
            SendAmount::Once => None,
            SendAmount::Finite { amount, period } => {
                if *amount > 1 {
                    Some(SendAmount::Finite {
                        amount: *amount - 1,
                        period: *period,
                    })
                } else {
                    None
                }
            }
        }
    }

    pub fn display(&self) -> String {
        match self {
            SendAmount::Infinite { period } => format!("∞ ({} ms period)", period),
            SendAmount::Once => "Once".to_string(),
            SendAmount::Finite { amount, period } => {
                format!("{} times ({} ms period)", amount, period)
            }
        }
    }
}

pub struct AddSendMessage {
    pub amount: SendAmount,
    pub msg_id: u32, // without the extended ID flag
    pub is_msg_id_extended: bool,
    pub msg_bytes: Vec<u8>,
}

#[derive(Clone)]
pub struct ParsedMessage {
    pub timestamp: chrono::DateTime<chrono::Local>,
    pub bus_name: BusName,
    pub raw_bytes: Vec<u8>,
    pub decoded: can_decode::DecodedMessage,
}

#[derive(Clone)]
pub struct UnparsedMessage {
    pub timestamp: chrono::DateTime<chrono::Local>,
    pub bus_name: BusName,
    pub raw_bytes: Vec<u8>,
    pub msg_id: u32, // without the extended ID flag
}
