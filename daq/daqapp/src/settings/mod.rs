mod fil;

use crate::ui::theme;
use daqcore::connection;
use std::path::PathBuf;

pub const SETTINGS_PATH: &str = "settings.json";

pub use fil::{FilRunOptions, FilSettings};

pub const DEFAULT_LOG_FOLDER: &str = "logs";
const DEFAULT_UDP_PORT: u16 = 5005;
const DEFAULT_CAN_SPEED: connection::CanBusSpeed = connection::CanBusSpeed::Kbps500;

pub fn dbc_dir() -> Option<std::path::PathBuf> {
    let path = std::path::Path::new(env!("CARGO_MANIFEST_DIR")).join("../../dbc");
    path.is_dir().then_some(path)
}

fn default_window_secs() -> f64 {
    30.0
}

#[derive(serde::Serialize, serde::Deserialize)]
pub struct Settings {
    pub dbc_path: Option<PathBuf>,
    pub selected_source: Option<connection::ConnectionSource>,
    pub selected_speed: connection::CanBusSpeed,
    #[serde(default)]
    pub selected_bus: connection::CanBus,
    pub udp_port: u16,
    pub theme: theme::ThemeSelection,
    pub pixels_per_point: Option<f32>,
    pub log_folder: Option<PathBuf>,
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
    fn path() -> PathBuf {
        PathBuf::from(SETTINGS_PATH)
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

    fn normalize(&mut self) {
        self.fil.normalize();
        if let Some(connection::ConnectionSource::Fil { bus, .. }) = &mut self.selected_source {
            if bus.trim().is_empty() {
                *bus = "vehicle".into();
            }
        }
    }

    pub fn save(&self) {
        let json = serde_json::to_string_pretty(self).expect("Failed to serialize settings");
        let path = Self::path();
        std::fs::write(&path, json)
            .unwrap_or_else(|e| log::error!("Failed to write {}: {}", path.display(), e));
    }
}
