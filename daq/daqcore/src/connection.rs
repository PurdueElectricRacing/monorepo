use crate::fil::config::BuiltNetwork;
use std::collections::HashMap;
use std::path::PathBuf;

#[derive(serde::Serialize, serde::Deserialize, Clone, Debug, PartialEq, Eq)]
pub struct FilRunOptions {
    pub duration_ms: u64,
    pub max_instructions: u64,
    pub quantum: u64,
    pub refresh_ms: u32,
    pub adc_decimation: u16,
    pub extra_live_filters: String,
    pub strict_mmio: bool,
    pub wall_pacing: bool,
    pub trace_instructions: bool,
    pub detect_spin: bool,
}

impl Default for FilRunOptions {
    fn default() -> Self {
        Self {
            duration_ms: 0,
            max_instructions: 50_000_000,
            quantum: 1024,
            refresh_ms: 1,
            adc_decimation: 1,
            extra_live_filters: String::new(),
            strict_mmio: false,
            wall_pacing: true,
            trace_instructions: false,
            detect_spin: false,
        }
    }
}

#[derive(serde::Serialize, serde::Deserialize, Clone, Debug, PartialEq)]
pub enum ConnectionSource {
    Serial(String, CanBusSpeed),
    Udp(u16),
    Simulated(bool), // true for connected, false for disconnected
    Fil {
        executable: PathBuf,
        network: PathBuf,
        /// Bus used for outgoing Message Sender frames.
        #[serde(default = "default_fil_bus")]
        bus: String,
        /// Bus whose CAN transmissions are viewed, or `None` for all buses.
        #[serde(default)]
        trace_bus: Option<String>,
        /// Per-board firmware ELF overrides keyed by board name.
        #[serde(default)]
        elf_overrides: HashMap<String, PathBuf>,
        /// Board names excluded from the emulated network.
        #[serde(default)]
        disabled_boards: Vec<String>,
        /// Widget-built network used instead of `network` when present.
        #[serde(default)]
        built_network: Option<Box<BuiltNetwork>>,
        #[serde(default)]
        run_options: Box<FilRunOptions>,
    },
    Loopback,
}

fn default_fil_bus() -> String {
    "0".into()
}

#[derive(serde::Serialize, serde::Deserialize, Copy, Clone, PartialEq, Debug, Default)]
pub enum CanBusSpeed {
    Kbps250,
    #[default]
    Kbps500,
}

impl ConnectionSource {
    pub fn display_name(&self) -> String {
        match self {
            ConnectionSource::Serial(path, speed) => {
                format!("Serial: {} ({})", path, speed.display_name())
            }
            ConnectionSource::Udp(port) => format!("UDP: {}", port),
            ConnectionSource::Simulated(connected) => {
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
