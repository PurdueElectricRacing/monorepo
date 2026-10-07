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
    pub fn connection_source(
        executable: std::path::PathBuf,
        network: Option<std::path::PathBuf>,
        bus: String,
        trace_bus: Option<String>,
        elf_overrides: std::collections::HashMap<String, std::path::PathBuf>,
        disabled_boards: Vec<String>,
        use_builder: bool,
        builder: config::BuiltNetwork,
        run_options: FilRunOptions,
    ) -> Option<connection::ConnectionSource> {
        if use_builder {
            Some(connection::ConnectionSource::Fil {
                executable,
                network: std::path::PathBuf::new(),
                bus: builder.bus.clone(),
                trace_bus,
                elf_overrides: std::collections::HashMap::new(),
                disabled_boards: Vec::new(),
                built_network: Some(builder),
                run_options,
            })
        } else {
            Some(connection::ConnectionSource::Fil {
                executable,
                network: network?,
                bus,
                trace_bus,
                elf_overrides,
                disabled_boards,
                built_network: None,
                run_options,
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
