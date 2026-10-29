use crate::fil::{
    annotations, config,
    messages::{FilAdcInstance, FilGpioPort},
};
use crate::{action, app, settings};
use daqcore::connection;
use eframe::egui;
use std::{
    collections::{HashMap, HashSet},
    sync::mpsc,
    thread,
    time::Duration,
};

pub const FIL_EXPECTATION_HISTORY_LIMIT: usize = 500;

#[derive(Clone, Copy, Default)]
pub struct GpioPinState {
    pub input: Option<bool>,
    pub output: Option<bool>,
}
pub struct FilControl {
    pub title: String,
    /// Persisted FIL configuration, mirrored from the app settings.
    settings: settings::FilSettings,
    network_info: Option<config::FilNetworkInfo>,
    network_info_error: Option<String>,
    network_info_rx: Option<(
        std::path::PathBuf,
        mpsc::Receiver<Result<config::FilNetworkInfo, String>>,
    )>,
    last_loaded_network: Option<std::path::PathBuf>,
    import_rx: Option<mpsc::Receiver<Result<config::BuiltBoard, String>>>,
    export_rx: Option<mpsc::Receiver<Result<(), String>>>,
    operation_error: Option<String>,
    annotations: annotations::FilAnnotations,
    annotation_error: Option<String>,
    gpio_board: String,
    gpio_port: FilGpioPort,
    gpio_states: HashMap<(String, FilGpioPort, u8), GpioPinState>,
    expectations: HashMap<String, daqcore::can::driver::FilExpectationEvent>,
    expectation_order: Vec<String>,
}
mod boards;
mod builder;
mod events;
mod expectations;
mod panels;
mod state;
mod validation;

use events::clear_expectations;
use expectations::{expectation_bytes, expectation_ms, expectation_name};
use panels::{elf_status, network_result_is_current};

impl FilControl {
    pub fn new(instance_num: usize, fil: &settings::FilSettings) -> Self {
        let mut settings = fil.clone();
        settings.normalize();
        let (annotations, annotation_error) = match annotations::load() {
            Ok(annotations) => (annotations, None),
            Err(error) => (annotations::FilAnnotations::default(), Some(error)),
        };
        let mut control = Self {
            title: format!("FIL Control #{}", instance_num),
            settings,
            network_info: None,
            network_info_error: None,
            network_info_rx: None,
            last_loaded_network: None,
            import_rx: None,
            export_rx: None,
            operation_error: None,
            annotations,
            annotation_error,
            gpio_board: "dashboard".into(),
            gpio_port: FilGpioPort::GpioA,
            gpio_states: HashMap::new(),
            expectations: HashMap::new(),
            expectation_order: Vec::new(),
        };
        control.refresh_network_info();
        control
    }
    fn queue_config_update(&self, actions: &mut Vec<action::AppAction>) {
        actions.push(action::AppAction::UpdateFilConfig {
            fil: self.settings.clone(),
        });
    }
    fn queue_builder_update(&self, actions: &mut Vec<action::AppAction>) {
        actions.push(action::AppAction::UpdateFilBuilder {
            use_builder: self.settings.use_builder,
            builder: self.settings.builder.clone(),
        });
    }
    fn set_board_enabled(
        &mut self,
        board: &str,
        enabled: bool,
        actions: &mut Vec<action::AppAction>,
    ) {
        self.settings
            .disabled_boards
            .retain(|disabled| disabled != board);
        if !enabled {
            self.settings.disabled_boards.push(board.into());
        }
        self.queue_config_update(actions);
    }
    fn connect_source(&self) -> Option<connection::ConnectionSource> {
        self.settings.connection_source()
    }
}
