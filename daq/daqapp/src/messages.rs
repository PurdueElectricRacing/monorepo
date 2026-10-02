use crate::{bootloader_protocol, connection, hil};

pub enum MsgFromUi {
    DbcSelected(std::path::PathBuf),
    Connect(connection::ConnectionSource),
    AddSendMessage(AddSendMessage),
    DeleteSendMessage {
        msg_id: u32,
    },
    UpdateLogFolder(std::path::PathBuf),
    Hil(hil::engine::HilCommand),
    StartFirmwareUpdate(bootloader_protocol::FirmwarePackage),
    CancelFirmwareUpdate,
    SetFilAdc {
        board: String,
        instance: FilAdcInstance,
        channel: u8,
        value: u16,
    },
    SetFilGpio {
        board: String,
        port: FilGpioPort,
        pin: u8,
        value: Option<bool>,
    },
    SetFilTraceBus(Option<String>),
    DisconnectFil {
        executable: std::path::PathBuf,
    },
    Disconnect,
}

#[derive(Clone, Copy, Debug, Default, Eq, PartialEq, Hash, serde::Serialize)]
pub enum FilAdcInstance {
    #[default]
    #[serde(rename = "ADC1")]
    Adc1,
    #[serde(rename = "ADC2")]
    Adc2,
    #[serde(rename = "ADC3")]
    Adc3,
    #[serde(rename = "ADC4")]
    Adc4,
}

impl FilAdcInstance {
    pub const ALL: [Self; 4] = [Self::Adc1, Self::Adc2, Self::Adc3, Self::Adc4];

    pub const fn as_str(self) -> &'static str {
        match self {
            Self::Adc1 => "ADC1",
            Self::Adc2 => "ADC2",
            Self::Adc3 => "ADC3",
            Self::Adc4 => "ADC4",
        }
    }

    pub fn parse(value: &str) -> Option<Self> {
        Self::ALL
            .into_iter()
            .find(|instance| instance.as_str() == value)
    }
}

impl std::fmt::Display for FilAdcInstance {
    fn fmt(&self, formatter: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        formatter.write_str(self.as_str())
    }
}

impl<'de> serde::Deserialize<'de> for FilAdcInstance {
    fn deserialize<D>(deserializer: D) -> Result<Self, D::Error>
    where
        D: serde::Deserializer<'de>,
    {
        let value = <serde_json::Value as serde::Deserialize>::deserialize(deserializer)?;
        Ok(value.as_str().and_then(Self::parse).unwrap_or_default())
    }
}

#[derive(Clone, Copy, Debug, Eq, PartialEq, Hash)]
pub enum FilGpioPort {
    GpioA,
    GpioB,
    GpioC,
    GpioD,
    GpioE,
    GpioF,
    GpioG,
}

impl FilGpioPort {
    pub const ALL: [Self; 7] = [
        Self::GpioA,
        Self::GpioB,
        Self::GpioC,
        Self::GpioD,
        Self::GpioE,
        Self::GpioF,
        Self::GpioG,
    ];

    pub const fn as_str(self) -> &'static str {
        match self {
            Self::GpioA => "GPIOA",
            Self::GpioB => "GPIOB",
            Self::GpioC => "GPIOC",
            Self::GpioD => "GPIOD",
            Self::GpioE => "GPIOE",
            Self::GpioF => "GPIOF",
            Self::GpioG => "GPIOG",
        }
    }

    pub fn parse(value: &str) -> Option<Self> {
        Self::ALL.into_iter().find(|port| port.as_str() == value)
    }
}

impl std::fmt::Display for FilGpioPort {
    fn fmt(&self, formatter: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        formatter.write_str(self.as_str())
    }
}

#[derive(Clone, Copy, Debug, Eq, PartialEq)]
pub enum FilGpioDirection {
    Input,
    Output,
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
    Hil(hil::engine::HilSnapshot),
    FirmwareProgress(FirmwareProgress),
    FilGpio {
        board: String,
        port: FilGpioPort,
        pin: u8,
        value: Option<bool>,
        direction: FilGpioDirection,
    },
    FilExpectation(FilExpectationEvent),
}

#[derive(Clone, Debug, PartialEq, Eq)]
pub enum FilExpectationStatus {
    Pending,
    Pass,
    Fail,
    Incomplete,
}

#[derive(Clone, Debug, PartialEq, Eq)]
pub struct FilExpectationEvent {
    pub check_id: String,
    pub script: String,
    pub status: FilExpectationStatus,
    pub expected_bus: String,
    pub expected_id: u32,
    pub expected_extended: bool,
    pub expected_data: Vec<u8>,
    pub window_start_ns: u64,
    pub window_end_ns: u64,
    pub matched_bus: Option<String>,
    pub matched_id: Option<u32>,
    pub matched_data: Option<Vec<u8>>,
    pub matched_origin: Option<String>,
    pub matched_time_ns: Option<u64>,
    pub reason: Option<String>,
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

#[cfg(test)]
mod fil_identifier_tests {
    use crate::messages::{FilAdcInstance, FilGpioPort};

    #[test]
    fn adc_instances_parse_and_serialize_as_protocol_names() {
        for instance in FilAdcInstance::ALL {
            assert_eq!(FilAdcInstance::parse(instance.as_str()), Some(instance));
            assert_eq!(instance.to_string(), instance.as_str());
            assert_eq!(
                serde_json::to_string(&instance).unwrap(),
                format!("\"{}\"", instance.as_str())
            );
        }
        assert_eq!(FilAdcInstance::parse("adc1"), None);
        assert_eq!(FilAdcInstance::parse("ADC5"), None);
        assert_eq!(
            serde_json::from_str::<FilAdcInstance>("\"ADC9\"").unwrap(),
            FilAdcInstance::Adc1
        );
    }

    #[test]
    fn gpio_ports_parse_only_supported_protocol_names() {
        for port in FilGpioPort::ALL {
            assert_eq!(FilGpioPort::parse(port.as_str()), Some(port));
            assert_eq!(port.to_string(), port.as_str());
        }
        assert_eq!(FilGpioPort::parse("gpioa"), None);
        assert_eq!(FilGpioPort::parse("GPIOH"), None);
    }
}
