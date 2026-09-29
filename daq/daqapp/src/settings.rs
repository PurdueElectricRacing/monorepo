use crate::ui::theme;
use daqcore::connection;

pub const SETTINGS_PATH: &str = "settings.json";
pub const DEFAULT_LOG_FOLDER: &str = "logs";

pub fn dbc_dir() -> Option<std::path::PathBuf> {
    let path = std::path::Path::new(env!("CARGO_MANIFEST_DIR")).join("../../dbc");
    path.is_dir().then_some(path)
}
const DEFAULT_UDP_PORT: u16 = 5005;
fn default_window_secs() -> f64 {
    30.0
}
const DEFAULT_CAN_SPEED: daqcore::connection::CanBusSpeed = connection::CanBusSpeed::Kbps500;

pub type FilRunOptions = daqcore::connection::FilRunOptions;

/// Human-readable labels for FIL GPIO and ADC controls, keyed by board name.
/// These are DaqApp-only settings and are not forwarded to FIL.
#[derive(serde::Serialize, serde::Deserialize, Clone, Debug, Default, PartialEq, Eq)]
pub struct FilAnnotations {
    #[serde(skip_serializing_if = "Option::is_none")]
    pub gpio: Option<
        std::collections::HashMap<String, std::collections::HashMap<String, GpioPortAnnotation>>,
    >,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub adc:
        Option<std::collections::HashMap<String, std::collections::HashMap<String, AdcAnnotation>>>,
}

#[derive(serde::Serialize, serde::Deserialize, Clone, Debug, Default, PartialEq, Eq)]
pub struct GpioPortAnnotation {
    #[serde(skip_serializing_if = "Option::is_none")]
    pub label: Option<String>,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub pins: Option<std::collections::HashMap<u8, String>>,
}

#[derive(serde::Serialize, serde::Deserialize, Clone, Debug, Default, PartialEq, Eq)]
pub struct AdcAnnotation {
    #[serde(skip_serializing_if = "Option::is_none")]
    pub label: Option<String>,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub channels: Option<std::collections::HashMap<u8, String>>,
}

impl FilAnnotations {
    pub fn is_empty(&self) -> bool {
        self.gpio
            .as_ref()
            .map_or(true, std::collections::HashMap::is_empty)
            && self
                .adc
                .as_ref()
                .map_or(true, std::collections::HashMap::is_empty)
    }
}

/// All FIL widget state persisted across runs.
#[derive(serde::Serialize, serde::Deserialize, Clone, Debug)]
pub struct FilSettings {
    pub executable: Option<std::path::PathBuf>,
    pub network: Option<std::path::PathBuf>,
    /// Bus used for outgoing Message Sender frames.
    pub bus: String,
    /// Bus whose CAN transmissions are viewed, or `None` to view all buses.
    pub trace_bus: Option<String>,
    /// Per-board firmware ELF overrides keyed by board name (network file mode).
    pub elf_overrides: std::collections::HashMap<String, std::path::PathBuf>,
    /// Board names excluded from the emulated network (network file mode).
    pub disabled_boards: Vec<String>,
    /// Use the widget-built network instead of a network file.
    pub use_builder: bool,
    /// Widget-built FIL network spec.
    pub builder: daqcore::fil_config::BuiltNetwork,
    /// Optional human-readable signal labels for ADC/GPIO controls.
    #[serde(skip_serializing_if = "Option::is_none")]
    pub annotations: Option<FilAnnotations>,
    pub adc_board: String,
    pub adc_instance: String,
    pub adc_channel: u8,
    pub adc_value: u16,
    pub run_options: FilRunOptions,
}

impl Default for FilSettings {
    fn default() -> Self {
        Self {
            executable: None,
            network: None,
            bus: "vehicle".into(),
            trace_bus: None,
            elf_overrides: std::collections::HashMap::new(),
            disabled_boards: Vec::new(),
            use_builder: false,
            builder: daqcore::fil_config::BuiltNetwork::default(),
            annotations: None,
            adc_board: "dashboard".into(),
            adc_instance: "ADC1".into(),
            adc_channel: 0,
            adc_value: 0,
            run_options: FilRunOptions::default(),
        }
    }
}

#[derive(serde::Serialize, serde::Deserialize)]
pub struct Settings {
    pub dbc_path: Option<std::path::PathBuf>,
    pub selected_source: Option<connection::ConnectionSource>,
    pub selected_speed: connection::CanBusSpeed,
    #[serde(default)]
    pub selected_bus: connection::CanBus,
    pub udp_port: u16,
    pub theme: theme::ThemeSelection,
    pub pixels_per_point: Option<f32>,
    pub log_folder: Option<std::path::PathBuf>,
    #[serde(default)]
    pub fil: FilSettings,
    #[serde(default = "default_window_secs")]
    pub window_secs: f64,
}

impl Default for Settings {
    fn default() -> Self {
        Self {
            dbc_path: None,
            selected_source: None,
            selected_speed: DEFAULT_CAN_SPEED,
            selected_bus: connection::CanBus::Vcan,
            udp_port: DEFAULT_UDP_PORT,
            theme: theme::ThemeSelection::Default,
            pixels_per_point: None,
            log_folder: None,
            fil: FilSettings::default(),
            window_secs: 30.0,
        }
    }
}

impl Settings {
    fn path() -> std::path::PathBuf {
        std::path::PathBuf::from(SETTINGS_PATH)
    }

    pub fn load() -> Self {
        let path = Self::path();
        if let Ok(json) = std::fs::read_to_string(&path) {
            let mut settings: Self = serde_json::from_str(&json).unwrap_or_default();
            settings.normalize();
            settings
        } else {
            let default = Settings::default();
            default.save();
            default
        }
    }

    /// Clamp persisted values into valid ranges after loading.
    fn normalize(&mut self) {
        if self.fil.bus.trim().is_empty() {
            self.fil.bus = "vehicle".into();
        }
        self.fil.adc_channel = self.fil.adc_channel.min(19);
        self.fil.adc_value = self.fil.adc_value.min(4095);
        self.fil.run_options.duration_ms =
            self.fil.run_options.duration_ms.min(u64::MAX / 1_000_000);
        self.fil.run_options.quantum = self.fil.run_options.quantum.max(1);
        self.fil.run_options.refresh_ms = self.fil.run_options.refresh_ms.clamp(1, i32::MAX as u32);
        self.fil.run_options.max_instructions = self.fil.run_options.max_instructions.max(1);
        self.fil.run_options.adc_decimation = self.fil.run_options.adc_decimation.clamp(1, 1024);
        if self.fil.builder.bitrate == 0 {
            self.fil.builder.bitrate = 500_000;
        }
    }

    pub fn save(&self) {
        // Expect okay. If it doesn't fail in testing, it shouldn't fail later.
        let json = serde_json::to_string_pretty(self).expect("Failed to serialize settings");
        let path = Self::path();
        std::fs::write(&path, json)
            .unwrap_or_else(|e| log::error!("Failed to write {}: {}", path.display(), e));
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn fil_annotations_load_from_settings_json() {
        let annotations: FilAnnotations = serde_json::from_value(serde_json::json!({
            "gpio": {
                "dashboard": {
                    "GPIOA": {
                        "label": "Dashboard controls",
                        "pins": {"0": "Ignition sense"}
                    }
                }
            },
            "adc": {
                "dashboard": {
                    "ADC1": {
                        "label": "Pedal inputs",
                        "channels": {"0": "Accelerator position"}
                    }
                }
            }
        }))
        .unwrap();

        assert_eq!(
            annotations.gpio.as_ref().unwrap()["dashboard"]["GPIOA"]
                .pins
                .as_ref()
                .unwrap()[&0],
            "Ignition sense"
        );
        assert_eq!(
            annotations.adc.as_ref().unwrap()["dashboard"]["ADC1"]
                .channels
                .as_ref()
                .unwrap()[&0],
            "Accelerator position"
        );

        let mut settings = FilSettings::default();
        settings.annotations = Some(annotations.clone());
        let json = serde_json::to_value(settings).unwrap();
        let loaded: FilSettings = serde_json::from_value(json).unwrap();
        assert_eq!(loaded.annotations, Some(annotations));
    }

    #[test]
    fn fil_annotations_are_optional_for_existing_settings() {
        let settings = FilSettings::default();
        let json = serde_json::to_value(&settings).unwrap();
        assert!(json.get("annotations").is_none());

        let loaded: FilSettings = serde_json::from_value(json).unwrap();
        assert!(loaded.annotations.is_none());
    }
}
