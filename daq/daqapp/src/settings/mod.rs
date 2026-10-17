mod fil;
mod settings;

pub const SETTINGS_PATH: &str = "settings.json";
pub use fil::{FilRunOptions, FilSettings};
pub use settings::dbc_dir as database_dir;
pub use settings::{DEFAULT_LOG_FOLDER, Settings};
