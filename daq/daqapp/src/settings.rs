use crate::ui::theme;

pub const SETTINGS_PATH: &str = "settings.json";
pub const DEFAULT_LOG_FOLDER: &str = "logs";

pub fn dbc_dir() -> Option<std::path::PathBuf> {
    let path = std::path::Path::new(env!("CARGO_MANIFEST_DIR")).join("../../dbc");
    path.is_dir().then_some(path)
}

const DEFAULT_UDP_PORT: u16 = 5005;
const DEFAULT_CAN_SPEED: daqcore::connection::CanBusSpeed =
    daqcore::connection::CanBusSpeed::Kbps500;
fn default_window_secs() -> f64 {
    30.0
}
fn default_fil_bus() -> String {
    "vehicle".into()
}
fn default_fil_adc_board() -> String {
    "dashboard".into()
}
fn default_fil_adc_instance() -> String {
    "ADC1".into()
}

#[derive(serde::Serialize, serde::Deserialize)]
pub struct Settings {
    pub dbc_path: Option<std::path::PathBuf>,
    pub selected_source: Option<daqcore::connection::ConnectionSource>,
    pub selected_speed: daqcore::connection::CanBusSpeed,
    pub udp_port: u16,
    pub theme: theme::ThemeSelection,
    pub pixels_per_point: Option<f32>,
    pub log_folder: Option<std::path::PathBuf>,
    #[serde(default = "default_window_secs")]
    pub window_secs: f64,
    #[serde(default)]
    pub fil_executable: Option<std::path::PathBuf>,
    #[serde(default)]
    pub fil_network_config: Option<std::path::PathBuf>,
    /// Per-board firmware ELF overrides keyed by board name.
    #[serde(default)]
    pub fil_elf_overrides: std::collections::HashMap<String, std::path::PathBuf>,
    /// Board names excluded from the emulated FIL network.
    #[serde(default)]
    pub fil_disabled_boards: Vec<String>,
    /// Use the widget-built network instead of a network file.
    #[serde(default)]
    pub fil_use_builder: bool,
    /// Widget-built FIL network spec.
    #[serde(default)]
    pub fil_builder: daqcore::fil_config::BuiltNetwork,
    #[serde(default = "default_fil_bus")]
    pub fil_bus: String,
    #[serde(default = "default_fil_adc_board")]
    pub fil_adc_board: String,
    #[serde(default = "default_fil_adc_instance")]
    pub fil_adc_instance: String,
    #[serde(default)]
    pub fil_adc_channel: u8,
    #[serde(default)]
    pub fil_adc_value: u16,
}

impl Default for Settings {
    fn default() -> Self {
        Self {
            dbc_path: None,
            selected_source: None,
            selected_speed: DEFAULT_CAN_SPEED,
            udp_port: DEFAULT_UDP_PORT,
            theme: theme::ThemeSelection::Default,
            pixels_per_point: None,
            log_folder: None,
            window_secs: 30.0,
            fil_executable: None,
            fil_network_config: None,
            fil_elf_overrides: std::collections::HashMap::new(),
            fil_disabled_boards: Vec::new(),
            fil_use_builder: false,
            fil_builder: daqcore::fil_config::BuiltNetwork::default(),
            fil_bus: default_fil_bus(),
            fil_adc_board: default_fil_adc_board(),
            fil_adc_instance: default_fil_adc_instance(),
            fil_adc_channel: 0,
            fil_adc_value: 0,
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
            serde_json::from_str(&json).unwrap_or_default()
        } else {
            let default = Settings::default();
            default.save();
            default
        }
    }

    pub fn save(&self) {
        let json = match serde_json::to_string_pretty(self) {
            Ok(json) => json,
            Err(error) => {
                log::error!("Failed to serialize settings: {error}");
                return;
            }
        };

        let path = Self::path();
        if let Err(error) = std::fs::write(&path, json) {
            log::error!("Failed to write {}: {error}", path.display());
        }
    }
}
