use crate::{connection, theme};

pub const SETTINGS_PATH: &str = "settings.json";
pub const DEFAULT_LOG_FOLDER: &str = "logs";
const DEFAULT_UDP_PORT: u16 = 5005;
const DEFAULT_CAN_SPEED: connection::CanBusSpeed = connection::CanBusSpeed::Kbps500;

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
        }
    }
}

impl Settings {
    pub fn load() -> Self {
        if let Ok(json) = std::fs::read_to_string(SETTINGS_PATH) {
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
        std::fs::write(SETTINGS_PATH, json)
            .unwrap_or_else(|e| log::error!("Failed to write {}: {}", SETTINGS_PATH, e));
    }
}
