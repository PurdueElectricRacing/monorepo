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
            adc_decimation: 32,
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
    /// Bus used for outgoing Message Sender frames.
    pub bus: String,
    /// Bus whose CAN transmissions are viewed, or `None` to view all buses.
    pub trace_bus: Option<String>,
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
            trace_bus: None,
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

impl FilSettings {
    pub(super) fn normalize(&mut self) {
        if self.bus.trim().is_empty() {
            self.bus = "vehicle".into();
        }
        self.adc_channel = self.adc_channel.min(19);
        self.adc_value = self.adc_value.min(4095);
        self.run_options.duration_ms = self.run_options.duration_ms.min(u64::MAX / 1_000_000);
        self.run_options.quantum = self.run_options.quantum.max(1);
        self.run_options.refresh_ms = self.run_options.refresh_ms.clamp(1, i32::MAX as u32);
        self.run_options.max_instructions = self.run_options.max_instructions.max(1);
        self.run_options.adc_decimation = self.run_options.adc_decimation.clamp(1, 1024);
        if self.builder.bitrate == 0 {
            self.builder.bitrate = 500_000;
        }
    }
}
