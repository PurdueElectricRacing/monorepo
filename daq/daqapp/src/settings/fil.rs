use crate::fil::config;
use crate::fil::messages::FilAdcInstance;
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
    pub builder: config::BuiltNetwork,
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
            builder: config::BuiltNetwork::default(),
            adc_board: "dashboard".into(),
            adc_instance: FilAdcInstance::Adc1,
            adc_channel: 0,
            adc_value: 0,
            run_options: FilRunOptions::default(),
        }
    }
}

impl FilSettings {
    pub fn connection_source(&self) -> Option<connection::ConnectionSource> {
        let executable = self.executable.clone()?;
        if self.use_builder {
            Some(connection::ConnectionSource::Fil {
                executable,
                network: std::path::PathBuf::new(),
                bus: self.builder.bus.clone(),
                trace_bus: self.trace_bus.clone(),
                elf_overrides: std::collections::HashMap::new(),
                disabled_boards: Vec::new(),
                built_network: Some(Box::new(self.builder.clone())),
                run_options: Box::new(self.run_options.clone()),
            })
        } else {
            Some(connection::ConnectionSource::Fil {
                executable,
                network: self.network.clone()?,
                bus: self.bus.clone(),
                trace_bus: self.trace_bus.clone(),
                elf_overrides: self.elf_overrides.clone(),
                disabled_boards: self.disabled_boards.clone(),
                built_network: None,
                run_options: Box::new(self.run_options.clone()),
            })
        }
    }

    pub fn normalize(&mut self) {
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
