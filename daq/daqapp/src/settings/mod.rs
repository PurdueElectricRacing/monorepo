mod fil;
mod settings;

pub const SETTINGS_PATH: &str = "settings.json";
pub use fil::{FilRunOptions, FilSettings};
pub use settings::{DEFAULT_LOG_FOLDER, Settings, dbc_dir};
