use crate::{connection, ui::theme};

pub const SETTINGS_PATH: &str = "settings.json";
pub const DEFAULT_LOG_FOLDER: &str = "logs";

pub fn dbc_dir() -> Option<std::path::PathBuf> {
    let path = std::path::Path::new(env!("CARGO_MANIFEST_DIR")).join("../../dbc");
    path.is_dir().then_some(path)
}
const DEFAULT_UDP_PORT: u16 = 5005;
const DEFAULT_CAN_SPEED: connection::CanBusSpeed = connection::CanBusSpeed::Kbps500;

/// Options forwarded to FIL's watch-network command. The live CAN/GPIO filters
/// and stdin control remain enabled because DaqApp requires them.
#[derive(serde::Serialize, serde::Deserialize, Clone, Debug, PartialEq, Eq)]
pub struct FilRunOptions {
    pub duration_ms: u64,
    pub max_instructions: u64,
    pub quantum: u64,
    pub refresh_ms: u32,
    pub adc_decimation: u16,
    pub extra_live_filters: String,
    pub strict_mmio: bool,
    pub wall_pacing: bool,
    pub loop_batching: bool,
    pub trace_instructions: bool,
    pub detect_spin: bool,
}

impl Default for FilRunOptions {
    fn default() -> Self {
        Self {
            duration_ms: 0,
            max_instructions: u64::MAX,
            quantum: 1024,
            refresh_ms: 1,
            adc_decimation: 1,
            extra_live_filters: String::new(),
            strict_mmio: false,
            wall_pacing: true,
            loop_batching: true,
            trace_instructions: false,
            detect_spin: false,
        }
    }
}

/// All FIL widget state persisted across runs.
#[derive(serde::Serialize, serde::Deserialize, Clone, Debug)]
pub struct FilSettings {
    pub executable: Option<std::path::PathBuf>,
    pub network: Option<std::path::PathBuf>,
    pub bus: String,
    /// Per-board firmware ELF overrides keyed by board name (network file mode).
    pub elf_overrides: std::collections::HashMap<String, std::path::PathBuf>,
    /// Board names excluded from the emulated network (network file mode).
    pub disabled_boards: Vec<String>,
    /// Use the widget-built network instead of a network file.
    pub use_builder: bool,
    /// Widget-built FIL network spec.
    pub builder: crate::fil_config::BuiltNetwork,
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
            elf_overrides: std::collections::HashMap::new(),
            disabled_boards: Vec::new(),
            use_builder: false,
            builder: crate::fil_config::BuiltNetwork::default(),
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
    pub selected_bus: connection::CanBus,
    pub udp_port: u16,
    pub theme: theme::ThemeSelection,
    pub pixels_per_point: Option<f32>,
    pub log_folder: Option<std::path::PathBuf>,
    pub fil: FilSettings,
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
