use crate::{connection, ui::theme};

pub const SETTINGS_PATH: &str = "settings.json";
pub const DEFAULT_LOG_FOLDER: &str = "logs";

pub fn database_dir() -> Option<std::path::PathBuf> {
    let path = std::path::Path::new(env!("CARGO_MANIFEST_DIR")).join("../../dbc");
    path.is_dir().then_some(path)
}
const DEFAULT_UDP_PORT: u16 = 5005;
const DEFAULT_CAN_SPEED: connection::CanBusSpeed = connection::CanBusSpeed::Kbps500;

#[derive(serde::Serialize, serde::Deserialize)]
pub struct Settings {
    #[serde(default, alias = "dbc_path")]
    pub database_path: Option<std::path::PathBuf>,
    #[serde(default)]
    pub database_bus: Option<u8>,
    pub selected_source: Option<connection::ConnectionSource>,
    pub selected_speed: connection::CanBusSpeed,
    pub udp_port: u16,
    pub theme: theme::ThemeSelection,
    pub pixels_per_point: Option<f32>,
    #[serde(default)]
    pub log_folder: Option<std::path::PathBuf>,
}

impl Default for Settings {
    fn default() -> Self {
        Self {
            database_path: None,
            database_bus: None,
            selected_source: None,
            selected_speed: DEFAULT_CAN_SPEED,
            udp_port: DEFAULT_UDP_PORT,
            theme: theme::ThemeSelection::Default,
            pixels_per_point: None,
            log_folder: None,
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

/// Generated hashes are not chronological: order by modification time, then name.
pub fn discover_database() -> Option<std::path::PathBuf> {
    let mut files: Vec<_> = std::fs::read_dir(database_dir()?)
        .ok()?
        .filter_map(Result::ok)
        .filter(|e| {
            let n = e.file_name();
            let n = n.to_string_lossy();
            n.starts_with("superdbc_") && n.ends_with(".json")
        })
        .filter_map(|e| {
            let metadata = e.metadata().ok()?;
            metadata
                .is_file()
                .then_some((metadata.modified().ok()?, e.path()))
        })
        .collect();
    files.sort();
    files.pop().map(|(_, path)| path)
}
