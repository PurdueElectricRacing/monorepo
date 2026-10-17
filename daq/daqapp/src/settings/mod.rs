mod app;
mod fil;

pub const SETTINGS_PATH: &str = "settings.json";
pub use app::dbc_dir as database_dir;
pub use app::{DEFAULT_LOG_FOLDER, Settings};
pub use fil::{FilRunOptions, FilSettings};
