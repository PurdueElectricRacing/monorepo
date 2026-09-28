#[derive(serde::Serialize, serde::Deserialize, Clone, Debug, PartialEq)]
pub enum ConnectionSource {
    Serial(String, CanBusSpeed),
    Udp(u16),
    Simulated(bool, Option<std::path::PathBuf>), // true for connected, false for disconnected, path to dbc file for sim
    Fil {
        executable: std::path::PathBuf,
        network: std::path::PathBuf,
        bus: String,
        /// Per-board firmware ELF overrides keyed by board name.
        elf_overrides: std::collections::HashMap<String, std::path::PathBuf>,
        /// Board names excluded from the emulated network.
        disabled_boards: Vec<String>,
        /// Widget-built network used instead of `network` when present.
        built_network: Option<crate::fil_config::BuiltNetwork>,
    },
    Loopback,
}

#[derive(serde::Serialize, serde::Deserialize, Copy, Clone, PartialEq, Eq, Debug, Default)]
#[serde(rename_all = "UPPERCASE")]
pub enum CanBus {
    #[default]
    Vcan,
    Scan,
}

impl CanBus {
    pub fn display_name(self) -> &'static str {
        match self {
            Self::Vcan => "VCAN",
            Self::Scan => "SCAN",
        }
    }

    pub fn options() -> [Self; 2] {
        [Self::Vcan, Self::Scan]
    }
}

#[derive(serde::Serialize, serde::Deserialize, Copy, Clone, PartialEq, Debug)]

pub enum CanBusSpeed {
    Kbps250,
    Kbps500,
}

impl ConnectionSource {
    pub fn display_name(&self) -> String {
        match self {
            ConnectionSource::Serial(path, speed) => {
                format!("Serial: {} ({})", path, speed.display_name())
            }
            ConnectionSource::Udp(port) => format!("UDP: {}", port),
            ConnectionSource::Simulated(connected, _) => {
                if *connected {
                    "Simulated (connected)".into()
                } else {
                    "Simulated (disconnected)".into()
                }
            }
            ConnectionSource::Fil {
                network,
                built_network,
                ..
            } => {
                if let Some(built) = built_network {
                    format!("FIL: {} (built)", built.name)
                } else {
                    format!(
                        "FIL: {}",
                        network
                            .file_name()
                            .unwrap_or(network.as_os_str())
                            .to_string_lossy()
                    )
                }
            }
            ConnectionSource::Loopback => "Loopback".into(),
        }
    }
}

impl CanBusSpeed {
    pub fn display_name(&self) -> String {
        match self {
            CanBusSpeed::Kbps250 => "250k".into(),
            CanBusSpeed::Kbps500 => "500k".into(),
        }
    }

    pub fn to_slcan_bitrate(self) -> slcan::NominalBitRate {
        match self {
            CanBusSpeed::Kbps250 => slcan::NominalBitRate::Rate250Kbit,
            CanBusSpeed::Kbps500 => slcan::NominalBitRate::Rate500Kbit,
        }
    }

    pub fn to_bps(self) -> u32 {
        match self {
            CanBusSpeed::Kbps250 => 250_000,
            CanBusSpeed::Kbps500 => 500_000,
        }
    }

    pub fn options() -> Vec<CanBusSpeed> {
        vec![CanBusSpeed::Kbps250, CanBusSpeed::Kbps500]
    }
}

impl Default for CanBusSpeed {
    fn default() -> Self {
        CanBusSpeed::Kbps500
    }
}
