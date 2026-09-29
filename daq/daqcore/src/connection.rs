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
    pub loop_batching: bool,
    pub trace_instructions: bool,
    pub detect_spin: bool,
}
impl Default for FilRunOptions {
    fn default() -> Self {
        Self {
            duration_ms: 0,
            max_instructions: u64::MAX,
            quantum: 1024,
            refresh_ms: 1,
            adc_decimation: 32,
            extra_live_filters: String::new(),
            strict_mmio: false,
            wall_pacing: true,
            loop_batching: true,
            trace_instructions: false,
            detect_spin: false,
        }
    }
}

#[derive(serde::Serialize, Clone, Debug, PartialEq)]
pub enum ConnectionSource {
    Serial(String, CanBusSpeed),
    Udp(u16),
    Simulated(bool, Option<std::path::PathBuf>), // true for connected, false for disconnected, path to dbc file for sim
    Fil {
        executable: std::path::PathBuf,
        network: std::path::PathBuf,
        /// Bus used for outgoing Message Sender frames.
        bus: String,
        /// Bus whose CAN transmissions are viewed, or `None` for all buses.
        trace_bus: Option<String>,
        /// Per-board firmware ELF overrides keyed by board name.
        elf_overrides: std::collections::HashMap<String, std::path::PathBuf>,
        /// Board names excluded from the emulated network.
        disabled_boards: Vec<String>,
        /// Widget-built network used instead of `network` when present.
        built_network: Option<crate::fil_config::BuiltNetwork>,
        #[serde(default)]
        run_options: FilRunOptions,
    },
    Loopback,
}

impl<'de> serde::Deserialize<'de> for ConnectionSource {
    fn deserialize<D>(deserializer: D) -> Result<Self, D::Error>
    where
        D: serde::Deserializer<'de>,
    {
        let value = serde_json::Value::deserialize(deserializer)?;
        if value.as_str() == Some("Loopback") {
            return Ok(Self::Loopback);
        }

        let object = value
            .as_object()
            .ok_or_else(|| serde::de::Error::custom("connection source must be an object"))?;
        if let Some(serial) = object.get("Serial") {
            let values = serial.as_array().ok_or_else(|| {
                serde::de::Error::custom("Serial connection source must contain an array")
            })?;
            return match values.as_slice() {
                [path, speed] | [path, speed, _] => Ok(Self::Serial(
                    serde_json::from_value(path.clone()).map_err(serde::de::Error::custom)?,
                    serde_json::from_value(speed.clone()).map_err(serde::de::Error::custom)?,
                )),
                _ => Err(serde::de::Error::custom(
                    "Serial connection source must contain path and speed",
                )),
            };
        }

        if let Some(port) = object.get("Udp") {
            return Ok(Self::Udp(
                serde_json::from_value(port.clone()).map_err(serde::de::Error::custom)?,
            ));
        }

        if let Some(simulated) = object.get("Simulated") {
            let values: (bool, Option<std::path::PathBuf>) =
                serde_json::from_value(simulated.clone()).map_err(serde::de::Error::custom)?;
            return Ok(Self::Simulated(values.0, values.1));
        }
        if let Some(fil) = object.get("Fil") {
            let values = fil.as_object().ok_or_else(|| {
                serde::de::Error::custom("FIL connection source must contain an object")
            })?;
            let executable = values
                .get("executable")
                .ok_or_else(|| serde::de::Error::custom("FIL executable is missing"))?;
            let network = values
                .get("network")
                .ok_or_else(|| serde::de::Error::custom("FIL network is missing"))?;
            let bus = values
                .get("bus")
                .and_then(serde_json::Value::as_str)
                .unwrap_or("vehicle")
                .to_owned();
            let trace_bus = values
                .get("trace_bus")
                .map(|value| {
                    serde_json::from_value(value.clone()).map_err(serde::de::Error::custom)
                })
                .transpose()?
                .unwrap_or(None);
            let elf_overrides = values
                .get("elf_overrides")
                .map(|value| {
                    serde_json::from_value(value.clone()).map_err(serde::de::Error::custom)
                })
                .transpose()?
                .unwrap_or_default();
            let disabled_boards = values
                .get("disabled_boards")
                .map(|value| {
                    serde_json::from_value(value.clone()).map_err(serde::de::Error::custom)
                })
                .transpose()?
                .unwrap_or_default();
            let built_network = values
                .get("built_network")
                .map(|value| {
                    serde_json::from_value(value.clone()).map_err(serde::de::Error::custom)
                })
                .transpose()?
                .unwrap_or(None);
            let run_options = values
                .get("run_options")
                .map(|value| {
                    serde_json::from_value(value.clone()).map_err(serde::de::Error::custom)
                })
                .transpose()?
                .unwrap_or_default();
            return Ok(Self::Fil {
                executable: serde_json::from_value(executable.clone())
                    .map_err(serde::de::Error::custom)?,
                network: serde_json::from_value(network.clone())
                    .map_err(serde::de::Error::custom)?,
                bus,
                trace_bus,
                elf_overrides,
                disabled_boards,
                built_network,
                run_options,
            });
        }
        if object.get("Loopback").is_some() {
            return Ok(Self::Loopback);
        }
        Err(serde::de::Error::custom("unknown connection source"))
    }
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

#[cfg(test)]
mod tests {
    use super::{ConnectionSource, FilRunOptions};

    #[test]
    fn legacy_fil_connection_defaults_new_options() {
        let source: ConnectionSource = serde_json::from_value(serde_json::json!({
            "Fil": { "executable": "fil", "network": "network", "bus": "vehicle" }
        }))
        .unwrap();
        let ConnectionSource::Fil {
            trace_bus,
            run_options,
            ..
        } = source
        else {
            panic!("expected FIL connection source");
        };
        assert_eq!(trace_bus, None);
        assert_eq!(run_options, FilRunOptions::default());
    }
}
