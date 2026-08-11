use crate::{bootloader_protocol, connection};

pub enum MsgFromUi {
    DbcSelected(std::path::PathBuf),
    Connect(connection::ConnectionSource),
    AddSendMessage(AddSendMessage),
    DeleteSendMessage { msg_id: u32 },
    UpdateLogFolder(std::path::PathBuf),
    StartFirmwareUpdate(bootloader_protocol::FirmwarePackage),
    CancelFirmwareUpdate,
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
    FirmwareProgress(FirmwareProgress),
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
    pub raw_bytes: Vec<u8>,
    pub decoded: can_decode::DecodedMessage,
}

#[derive(Clone)]
pub struct UnparsedMessage {
    pub timestamp: chrono::DateTime<chrono::Local>,
    pub raw_bytes: Vec<u8>,
    pub msg_id: u32, // without the extended ID flag
}

#[derive(Clone, Debug)]
pub struct FirmwareProgress {
    pub board: String,
    pub board_index: usize,
    pub board_count: usize,
    pub phase: String,
    pub sent_bytes: usize,
    pub total_bytes: usize,
    pub error: Option<String>,
}
