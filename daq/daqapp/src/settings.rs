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
