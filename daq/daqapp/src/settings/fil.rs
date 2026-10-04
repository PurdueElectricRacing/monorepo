use crate::fil::messages::FilAdcInstance;
use crate::fil::config as fil_config;
use daqcore::connection;

pub type FilRunOptions = connection::FilRunOptions;

/// All FIL widget state persisted across runs.
#[derive(serde::Serialize, serde::Deserialize, Clone, Debug)]
pub struct FilSettings {
    pub executable: Option<std::path::PathBuf>,
    pub network: Option<std::path::PathBuf>,
    pub bus: String,
    pub trace_bus: Option<String>,
    pub elf_overrides: std::collections::HashMap<String, std::path::PathBuf>,
    pub disabled_boards: Vec<String>,
    pub use_builder: bool,
    pub builder: fil_config::BuiltNetwork,
    pub adc_board: String,
    pub adc_instance: FilAdcInstance,
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
            builder: fil_config::BuiltNetwork::default(),
            adc_board: "dashboard".into(),
            adc_instance: FilAdcInstance::Adc1,
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

