use crate::{connection, ui::theme};

pub const SETTINGS_PATH: &str = "settings.json";
pub const DEFAULT_LOG_FOLDER: &str = "logs";

pub fn dbc_dir() -> Option<std::path::PathBuf> {
    let path = std::path::Path::new(env!("CARGO_MANIFEST_DIR")).join("../../dbc");
    path.is_dir().then_some(path)
}
const DEFAULT_UDP_PORT: u16 = 5005;
const DEFAULT_CAN_SPEED: connection::CanBusSpeed = connection::CanBusSpeed::Kbps500;
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
    pub selected_source: Option<connection::ConnectionSource>,
    pub selected_speed: connection::CanBusSpeed,
    #[serde(default)]
    pub selected_bus: connection::CanBus,
    pub udp_port: u16,
    pub theme: theme::ThemeSelection,
    pub pixels_per_point: Option<f32>,
    #[serde(default)]
    pub log_folder: Option<std::path::PathBuf>,
    pub fil_executable: Option<std::path::PathBuf>,
    #[serde(default)]
    pub fil_network_config: Option<std::path::PathBuf>,
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
            selected_bus: connection::CanBus::Vcan,
            udp_port: DEFAULT_UDP_PORT,
            theme: theme::ThemeSelection::Default,
            pixels_per_point: None,
            log_folder: None,
            fil_executable: None,
            fil_network_config: None,
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
        // Expect okay. If it doesn't fail in testing, it shouldn't fail later.
        let json = serde_json::to_string_pretty(self).expect("Failed to serialize settings");
        let path = Self::path();
        std::fs::write(&path, json)
            .unwrap_or_else(|e| log::error!("Failed to write {}: {}", path.display(), e));
    }
}
