use crate::fil::{
    annotations, config,
    messages::{FilAdcInstance, FilGpioPort},
};
use crate::{action, app, settings};
use daqcore::connection;
use eframe::egui;
use std::collections::{HashMap, HashSet};

const FIL_EXPECTATION_HISTORY_LIMIT: usize = 500;

fn clear_expectations(
    events: &mut HashMap<String, daqcore::can::driver::FilExpectationEvent>,
    order: &mut Vec<String>,
) {
    events.clear();
    order.clear();
}
fn update_expectations(
    events: &mut HashMap<String, daqcore::can::driver::FilExpectationEvent>,
    order: &mut Vec<String>,
    event: daqcore::can::driver::FilExpectationEvent,
) {
    if !events.contains_key(&event.check_id) {
        order.push(event.check_id.clone());
    }
    events.insert(event.check_id.clone(), event);
    while order.len() > FIL_EXPECTATION_HISTORY_LIMIT {
        let id = order.remove(0);
        events.remove(&id);
    }
}

#[derive(Clone, Copy, Default)]
struct GpioPinState {
    input: Option<bool>,
    output: Option<bool>,
}

pub struct FilControl {
    pub title: String,
    executable: Option<std::path::PathBuf>,
    network: Option<std::path::PathBuf>,
    bus: String,
    trace_bus: Option<String>,
    elf_overrides: HashMap<String, std::path::PathBuf>,
    disabled_boards: Vec<String>,
    use_builder: bool,
    builder: config::BuiltNetwork,
    network_info: Option<config::FilNetworkInfo>,
    network_info_error: Option<String>,
    last_loaded_network: Option<std::path::PathBuf>,
    adc_board: String,
    adc_instance: FilAdcInstance,
    adc_channel: u8,
    adc_value: u16,
    run_options: settings::FilRunOptions,
    annotations: annotations::FilAnnotations,
    annotation_error: Option<String>,
    gpio_board: String,
    gpio_port: FilGpioPort,
    gpio_states: HashMap<(String, FilGpioPort, u8), GpioPinState>,
    expectations: HashMap<String, daqcore::can::driver::FilExpectationEvent>,
    expectation_order: Vec<String>,
}

impl FilControl {
    pub fn new(instance_num: usize) -> Self {
        let saved = settings::Settings::load().fil;
        let (annotations, annotation_error) = match annotations::load() {
            Ok(annotations) => (annotations, None),
            Err(error) => (annotations::FilAnnotations::default(), Some(error)),
        };
        let mut control = Self {
            title: format!("FIL Control #{}", instance_num),
            executable: saved.executable,
            network: saved.network,
            bus: saved.bus,
            trace_bus: saved.trace_bus,
            elf_overrides: saved.elf_overrides,
            disabled_boards: saved.disabled_boards,
            use_builder: saved.use_builder,
            builder: saved.builder,
            network_info: None,
            network_info_error: None,
            last_loaded_network: None,
            adc_board: saved.adc_board,
            adc_instance: saved.adc_instance,
            adc_channel: saved.adc_channel.min(19),
            adc_value: saved.adc_value.min(4095),
            run_options: saved.run_options,
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

    /// Reload cached network info when the selected network file changes.
    /// Derived defaults (bus, ADC/GPIO board) follow the newly selected
    /// network only when the current value is not one of its boards/buses,
    /// so arbitrary board and bus names work without board-specific assumptions.
    fn refresh_network_info(&mut self) {
        if self.last_loaded_network == self.network {
            return;
        }
        self.last_loaded_network = self.network.clone();
        self.network_info = None;
        self.network_info_error = None;
        let Some(network) = self.network.clone() else {
            return;
        };
        match config::load_network_info(&network) {
            Ok(info) => {
                if !info.buses.contains(&self.bus) && !info.buses.is_empty() {
                    self.bus = info.buses[0].clone();
                }
                let boards: Vec<String> =
                    info.boards.iter().map(|board| board.name.clone()).collect();
                if !boards.contains(&self.adc_board) && !boards.is_empty() {
                    self.adc_board = boards[0].clone();
                }
                if !boards.contains(&self.gpio_board) && !boards.is_empty() {
                    self.gpio_board = boards[0].clone();
                }
                self.network_info = Some(info);
            }
            Err(error) => self.network_info_error = Some(error),
        }
    }

    fn builder_board_name(board: &config::BuiltBoard) -> String {
        if !board.name.trim().is_empty() {
            return board.name.clone();
        }
        board
            .elf
            .file_stem()
            .map(|stem| stem.to_string_lossy().into_owned())
            .filter(|stem| !stem.is_empty())
            .unwrap_or_else(|| "board".into())
    }

    /// Board names available for ADC/GPIO targeting in the active mode.
    fn available_board_names(&self) -> Vec<String> {
        if self.use_builder {
            self.builder
                .boards
                .iter()
                .filter(|board| board.enabled)
                .map(Self::builder_board_name)
                .collect()
        } else {
            self.network_info
                .as_ref()
                .map(|info| info.boards.iter().map(|board| board.name.clone()).collect())
                .unwrap_or_default()
        }
    }

    fn enabled_file_boards(&self) -> Vec<&config::FilBoardInfo> {
        let disabled: HashSet<&str> = self.disabled_boards.iter().map(String::as_str).collect();
        self.network_info
            .as_ref()
            .map(|info| {
                info.boards
                    .iter()
                    .filter(|board| !disabled.contains(board.name.as_str()))
                    .collect()
            })
            .unwrap_or_default()
    }

    /// Reasons the Connect button is currently unavailable.
    fn connect_issues(&self) -> Vec<String> {
        if self.executable.is_none() {
            return vec!["Select a FIL executable first".into()];
        }
        if self.use_builder {
            return self.builder_issues();
        }
        if self.network.is_none() {
            return vec!["Select a network config first".into()];
        }
        if let Some(error) = &self.network_info_error {
            return vec![error.clone()];
        }
        let Some(info) = &self.network_info else {
            return vec!["Loading network config…".into()];
        };
        let mut issues = Vec::new();
        if self
            .trace_bus
            .as_ref()
            .is_some_and(|bus| !info.buses.contains(bus))
        {
            issues.push(format!(
                "View bus '{}' is not declared in this network",
                self.trace_bus.as_deref().unwrap_or_default()
            ));
        }
        if self.enabled_file_boards().is_empty() {
            issues.push("Enable at least one board below".into());
        }
        for board in &info.boards {
            if self.disabled_boards.contains(&board.name) {
                continue;
            }
            match config::effective_elf(board, &self.elf_overrides) {
                Some(elf) if elf.is_file() => {}
                Some(elf) => issues.push(format!(
                    "Board '{}' ELF does not exist: {}",
                    board.name,
                    elf.display()
                )),
                None => issues.push(format!(
                    "Board '{}' has no firmware ELF; select one below",
                    board.name
                )),
            }
        }
        issues
    }

    fn builder_issues(&self) -> Vec<String> {
        let mut issues = Vec::new();
        if self.builder.name.trim().is_empty() {
            issues.push("Give the built network a name".into());
        }
        if self.builder.bus.trim().is_empty() {
            issues.push("Give the built network a bus name".into());
        }
        if self
            .trace_bus
            .as_ref()
            .is_some_and(|bus| bus != &self.builder.bus)
        {
            issues.push(format!(
                "View bus '{}' is not declared in the built network",
                self.trace_bus.as_deref().unwrap_or_default()
            ));
        }
        if self.builder.bitrate == 0 {
            issues.push("Built network bitrate must be nonzero".into());
        }
        for stimulus in &self.builder.stimuli {
            if !stimulus.is_file() {
                issues.push(format!(
                    "Stimulus script does not exist: {}",
                    stimulus.display()
                ));
            }
        }
        let enabled: Vec<&config::BuiltBoard> = self
            .builder
            .boards
            .iter()
            .filter(|board| board.enabled)
            .collect();
        if enabled.is_empty() {
            issues.push("Add and enable at least one board below".into());
        }
        let mut names = HashSet::new();
        for board in enabled {
            if !board.board.as_os_str().is_empty() {
                if !board.board.is_file() {
                    issues.push(format!(
                        "Board config does not exist: {}",
                        board.board.display()
                    ));
                }
                continue;
            }
            let name = Self::builder_board_name(board);
            if board.name.trim().is_empty() && board.elf.as_os_str().is_empty() {
                issues.push("A built board needs a name and an ELF".into());
                continue;
            }
            if !names.insert(name.clone()) {
                issues.push(format!("Duplicate board name '{name}'"));
            }
            if !board.elf.is_file() {
                issues.push(format!(
                    "Board '{name}' ELF does not exist: {}",
                    board.elf.display()
                ));
            }
            match config::effective_mcu(board, self.executable.as_deref()) {
                Some(mcu) if mcu.is_file() => {}
                Some(mcu) => issues.push(format!(
                    "Board '{name}' MCU config does not exist: {}",
                    mcu.display()
                )),
                None => issues.push(format!(
                    "Board '{name}' has no MCU config; pick one or point the FIL executable at a fil build"
                )),
            }
            let instances = if board.can_instances.is_empty() {
                vec!["FDCAN1"]
            } else {
                board.can_instances.iter().map(String::as_str).collect()
            };
            for instance in instances {
                if !config::FIL_CAN_INSTANCES.contains(&instance) {
                    issues.push(format!(
                        "Board '{name}' has unknown CAN instance '{instance}'"
                    ));
                }
            }
        }
        issues
    }

    pub fn handle_can_message(&mut self, msg: &daqcore::can_thread::CanThreadEvent) {
        match msg {
            daqcore::can_thread::CanThreadEvent::FilGpio {
                board,
                port,
                pin,
                value,
                direction,
            } => {
                let Some(port) = FilGpioPort::parse(port) else {
                    return;
                };
                let state = self
                    .gpio_states
                    .entry((board.clone(), port, *pin))
                    .or_default();
                match direction {
                    daqcore::can_thread::FilGpioDirection::Input => state.input = *value,
                    daqcore::can_thread::FilGpioDirection::Output => state.output = *value,
                }
            }
            daqcore::can_thread::CanThreadEvent::FilExpectation(event) => update_expectations(
                &mut self.expectations,
                &mut self.expectation_order,
                event.clone(),
            ),
            daqcore::can_thread::CanThreadEvent::ConnectionSuccessful => {
                self.gpio_states.clear();
                clear_expectations(&mut self.expectations, &mut self.expectation_order);
            }
            daqcore::can_thread::CanThreadEvent::Disconnection
            | daqcore::can_thread::CanThreadEvent::ConnectionFailed(_) => self.gpio_states.clear(),
            _ => {}
        }
    }

    fn queue_config_update(&self, actions: &mut Vec<action::AppAction>) {
        actions.push(action::AppAction::UpdateFilConfig {
            fil: settings::FilSettings {
                executable: self.executable.clone(),
                network: self.network.clone(),
                bus: self.bus.clone(),
                trace_bus: self.trace_bus.clone(),
                elf_overrides: self.elf_overrides.clone(),
                disabled_boards: self.disabled_boards.clone(),
                use_builder: self.use_builder,
                builder: self.builder.clone(),
                adc_board: self.adc_board.clone(),
                adc_instance: self.adc_instance,
                adc_channel: self.adc_channel,
                adc_value: self.adc_value,
                run_options: self.run_options.clone(),
            },
        });
    }

    fn queue_builder_update(&self, actions: &mut Vec<action::AppAction>) {
        actions.push(action::AppAction::UpdateFilBuilder {
            use_builder: self.use_builder,
            builder: self.builder.clone(),
        });
    }

    fn set_board_enabled(
        &mut self,
        board: &str,
        enabled: bool,
        actions: &mut Vec<action::AppAction>,
    ) {
        self.disabled_boards.retain(|disabled| disabled != board);
        if !enabled {
            self.disabled_boards.push(board.into());
        }
        self.queue_config_update(actions);
    }

    fn show_file_network(&mut self, ui: &mut egui::Ui, actions: &mut Vec<action::AppAction>) {
        ui.horizontal(|ui| {
            if ui.button("Select network").clicked()
                && let Some(path) = rfd::FileDialog::new()
                    .add_filter("JSON network config", &["json"])
                    .pick_file()
            {
                self.network = Some(path);
                self.refresh_network_info();
                self.queue_config_update(actions);
            }
            ui.label(
                self.network
                    .as_ref()
                    .map(|path| path.display().to_string())
                    .unwrap_or_else(|| "None selected".into()),
            );
        });
        ui.horizontal(|ui| {
            self.show_bus_selector(ui, actions);
        });
        ui.add_space(4.0);
        self.show_file_board_selector(ui, actions);
    }

    fn show_file_board_selector(
        &mut self,
        ui: &mut egui::Ui,
        actions: &mut Vec<action::AppAction>,
    ) {
        let Some(info) = self.network_info.clone() else {
            if let Some(error) = self.network_info_error.clone() {
                ui.label(egui::RichText::new(error).color(egui::Color32::LIGHT_RED));
            }
            return;
        };
        ui.strong(format!(
            "Boards in {} ({} enabled)",
            info.name,
            self.enabled_file_boards().len()
        ));
        egui::Grid::new(("fil_board_grid", &self.title))
            .striped(true)
            .show(ui, |ui| {
                ui.strong("Run");
                ui.strong("Board");
                ui.strong("Firmware ELF");
                ui.strong("");
                ui.end_row();
                for board in &info.boards {
                    let mut enabled = !self.disabled_boards.contains(&board.name);
                    if ui.checkbox(&mut enabled, "").changed() {
                        self.set_board_enabled(&board.name, enabled, actions);
                    }
                    ui.label(&board.name);
                    let effective = config::effective_elf(board, &self.elf_overrides);
                    let is_override = self.elf_overrides.contains_key(&board.name);
                    ui.label(elf_status(&effective, is_override));
                    ui.horizontal(|ui| {
                        if ui.small_button("Select ELF…").clicked()
                            && let Some(path) = rfd::FileDialog::new()
                                .add_filter("ELF firmware", &["elf"])
                                .pick_file()
                        {
                            self.elf_overrides.insert(board.name.clone(), path);
                            self.queue_config_update(actions);
                        }
                        if is_override && ui.small_button("Reset").clicked() {
                            self.elf_overrides.remove(&board.name);
                            self.queue_config_update(actions);
                        }
                    });
                    ui.end_row();
                }
            });
    }

    fn show_builder(&mut self, ui: &mut egui::Ui, actions: &mut Vec<action::AppAction>) {
        ui.horizontal(|ui| {
            ui.label("Name:");
            if ui.text_edit_singleline(&mut self.builder.name).changed() {
                self.queue_builder_update(actions);
            }
            ui.label("Bus:");
            if ui.text_edit_singleline(&mut self.builder.bus).changed() {
                self.queue_builder_update(actions);
            }
            ui.label("Bitrate:");
            if ui
                .add(
                    egui::DragValue::new(&mut self.builder.bitrate)
                        .range(1..=10_000_000)
                        .suffix(" bit/s"),
                )
                .changed()
            {
                self.queue_builder_update(actions);
            }
        });
        ui.horizontal(|ui| {
            ui.strong("Stimulus scripts:");
            if ui.button("Add stimulus scripts…").clicked()
                && let Some(paths) = rfd::FileDialog::new()
                    .add_filter("FIL stimulus JSON", &["json"])
                    .pick_files()
            {
                let mut changed = false;
                for path in paths {
                    if !self.builder.stimuli.contains(&path) {
                        self.builder.stimuli.push(path);
                        changed = true;
                    }
                }
                if changed {
                    self.queue_builder_update(actions);
                }
            }
        });
        let mut remove_stimulus = None;
        for (index, stimulus) in self.builder.stimuli.iter().enumerate() {
            ui.horizontal(|ui| {
                let label = stimulus.display().to_string();
                if stimulus.is_file() {
                    ui.label(label);
                } else {
                    ui.label(
                        egui::RichText::new(format!("{label} (missing)"))
                            .color(egui::Color32::LIGHT_RED),
                    );
                }
                if ui.small_button("Remove").clicked() {
                    remove_stimulus = Some(index);
                }
            });
        }
        if let Some(index) = remove_stimulus {
            self.builder.stimuli.remove(index);
            self.queue_builder_update(actions);
        }
        ui.add_space(4.0);
        let enabled_count = self
            .builder
            .boards
            .iter()
            .filter(|board| board.enabled)
            .count();
        ui.strong(format!(
            "Boards ({} enabled, no JSON needed)",
            enabled_count
        ));
        let mut remove_index = None;
        for (index, board) in self.builder.boards.clone().iter().enumerate() {
            ui.group(|ui| {
                ui.horizontal(|ui| {
                    let mut enabled = board.enabled;
                    if ui.checkbox(&mut enabled, "Run").changed() {
                        self.builder.boards[index].enabled = enabled;
                        self.queue_builder_update(actions);
                    }
                    ui.label("Name:");
                    let mut name = board.name.clone();
                    if ui.text_edit_singleline(&mut name).changed() {
                        self.builder.boards[index].name = name;
                        self.queue_builder_update(actions);
                    }
                    if ui.small_button("Remove").clicked() {
                        remove_index = Some(index);
                    }
                });
                ui.horizontal(|ui| {
                    ui.label("ELF:");
                    let elf = if board.elf.as_os_str().is_empty() {
                        None
                    } else {
                        Some(board.elf.clone())
                    };
                    ui.label(elf_status(&elf, false));
                    if ui.small_button("Select ELF…").clicked()
                        && let Some(path) = rfd::FileDialog::new()
                            .add_filter("ELF firmware", &["elf"])
                            .pick_file()
                    {
                        let entry = &mut self.builder.boards[index];
                        entry.elf = path;
                        if entry.name.trim().is_empty() {
                            entry.name = Self::builder_board_name(entry);
                        }
                        self.queue_builder_update(actions);
                    }
                });
                ui.horizontal(|ui| {
                    ui.label("MCU:");
                    let effective = config::effective_mcu(board, self.executable.as_deref());
                    match &effective {
                        Some(mcu) if mcu.is_file() => {
                            ui.label(mcu.display().to_string()).on_hover_text(
                                if board.mcu.as_os_str().is_empty() {
                                    "Auto-located next to the FIL executable"
                                } else {
                                    "Explicit MCU config"
                                },
                            );
                        }
                        _ => {
                            ui.label(
                                egui::RichText::new("No MCU config found")
                                    .color(egui::Color32::LIGHT_RED),
                            );
                        }
                    }
                    if ui.small_button("Select MCU…").clicked()
                        && let Some(path) = rfd::FileDialog::new()
                            .add_filter("JSON MCU config", &["json"])
                            .pick_file()
                    {
                        self.builder.boards[index].mcu = path;
                        self.queue_builder_update(actions);
                    }
                    if !board.mcu.as_os_str().is_empty() && ui.small_button("Auto").clicked() {
                        self.builder.boards[index].mcu = std::path::PathBuf::new();
                        self.queue_builder_update(actions);
                    }
                });
                ui.horizontal(|ui| {
                    ui.label("CAN:");
                    let current: Vec<&str> = if board.can_instances.is_empty() {
                        vec![config::FIL_CAN_INSTANCES[0]]
                    } else {
                        board.can_instances.iter().map(String::as_str).collect()
                    };
                    for instance in config::FIL_CAN_INSTANCES {
                        let mut checked = current.contains(&instance);
                        if ui.checkbox(&mut checked, instance).changed() {
                            let mut set: Vec<String> = config::FIL_CAN_INSTANCES
                                .into_iter()
                                .filter(|candidate| {
                                    (*candidate == instance && checked)
                                        || (*candidate != instance && current.contains(candidate))
                                })
                                .map(str::to_owned)
                                .collect();
                            if set.len() == 1 && set[0] == config::FIL_CAN_INSTANCES[0] {
                                set.clear();
                            }
                            self.builder.boards[index].can_instances = set;
                            self.queue_builder_update(actions);
                        }
                    }
                    ui.label("Vector base (optional):");
                    let mut base = board.vector_base.clone();
                    if ui
                        .text_edit_singleline(&mut base)
                        .on_hover_text("Empty means the FIL default")
                        .changed()
                    {
                        self.builder.boards[index].vector_base = base;
                        self.queue_builder_update(actions);
                    }
                });
            });
        }
        if let Some(index) = remove_index {
            self.builder.boards.remove(index);
            self.queue_builder_update(actions);
        }
        ui.horizontal(|ui| {
            if ui.button("Add board…").clicked()
                && let Some(path) = rfd::FileDialog::new()
                    .add_filter("ELF firmware", &["elf"])
                    .pick_file()
            {
                let mut entry = config::BuiltBoard {
                    name: String::new(),
                    elf: path,
                    mcu: std::path::PathBuf::new(),
                    can_instances: Vec::new(),
                    vector_base: String::new(),
                    enabled: true,
                    board: std::path::PathBuf::new(),
                    elf_override: None,
                };
                entry.name = Self::builder_board_name(&entry);
                self.builder.boards.push(entry);
                self.queue_builder_update(actions);
            }
            if ui.button("Import board file…").clicked()
                && let Some(path) = rfd::FileDialog::new()
                    .add_filter("JSON board config", &["json"])
                    .pick_file()
            {
                self.import_board_file(path, actions);
            }
            if ui.button("Export network JSON…").clicked()
                && let Some(path) = rfd::FileDialog::new()
                    .add_filter("JSON network config", &["json"])
                    .set_file_name(format!("{}.json", self.builder.name))
                    .save_file()
            {
                let executable = self.executable.clone().unwrap_or_default();
                match config::export_network(&path, &self.builder, &executable) {
                    Ok(()) => log::info!("Exported FIL network to {}", path.display()),
                    Err(error) => log::error!("Failed to export FIL network: {error}"),
                }
            }
        });
    }

    /// Prefill a builder entry from an existing board JSON file.
    fn import_board_file(
        &mut self,
        path: std::path::PathBuf,
        actions: &mut Vec<action::AppAction>,
    ) {
        let entry = config::BuiltBoard {
            name: String::new(),
            elf: std::path::PathBuf::new(),
            mcu: std::path::PathBuf::new(),
            can_instances: Vec::new(),
            vector_base: String::new(),
            enabled: true,
            board: path,
            elf_override: None,
        };
        let entry = config::migrate_built_board(&entry);
        if entry.board.as_os_str().is_empty() {
            self.builder.boards.push(entry);
            self.queue_builder_update(actions);
        } else {
            log::error!("Failed to import board file: {}", entry.board.display());
        }
    }

    fn show_bus_selector(&mut self, ui: &mut egui::Ui, actions: &mut Vec<action::AppAction>) {
        ui.label("Message Sender bus:");
        let buses = self
            .network_info
            .as_ref()
            .map(|info| info.buses.clone())
            .unwrap_or_default();
        if buses.is_empty() {
            if ui.text_edit_singleline(&mut self.bus).changed() {
                self.queue_config_update(actions);
            }
        } else {
            let changed = egui::ComboBox::from_id_salt(("fil_bus", &self.title))
                .selected_text(&self.bus)
                .show_ui(ui, |ui| {
                    buses.iter().fold(false, |changed, bus| {
                        ui.selectable_value(&mut self.bus, bus.clone(), bus)
                            .changed()
                            || changed
                    })
                })
                .inner
                .unwrap_or(false);
            if changed {
                self.queue_config_update(actions);
            }
        }
    }

    fn available_trace_buses(&self) -> Vec<String> {
        if self.use_builder {
            if self.builder.bus.trim().is_empty() {
                Vec::new()
            } else {
                vec![self.builder.bus.clone()]
            }
        } else {
            self.network_info
                .as_ref()
                .map(|info| info.buses.clone())
                .unwrap_or_default()
        }
    }

    fn show_trace_bus_selector(
        &mut self,
        ui: &mut egui::Ui,
        actions: &mut Vec<action::AppAction>,
        tx: &std::sync::mpsc::Sender<daqcore::can_thread::CanThreadCommand>,
    ) {
        let buses = self.available_trace_buses();
        let changed = ui
            .horizontal(|ui| {
                ui.label("View/trace CAN bus:");
                egui::ComboBox::from_id_salt(("fil_trace_bus", &self.title))
                    .selected_text(self.trace_bus.as_deref().unwrap_or("All buses"))
                    .show_ui(ui, |ui| {
                        let mut changed = ui
                            .selectable_value(&mut self.trace_bus, None, "All buses")
                            .changed();
                        for bus in &buses {
                            changed |= ui
                                .selectable_value(&mut self.trace_bus, Some(bus.clone()), bus)
                                .changed();
                        }
                        changed
                    })
                    .inner
                    .unwrap_or(false)
            })
            .inner;
        ui.small("Message Sender bus remains the outgoing injection target.");
        if changed {
            self.queue_config_update(actions);
            let _ = tx.send(daqcore::can_thread::CanThreadCommand::SetFilTraceBus(
                self.trace_bus.clone(),
            ));
        }
    }

    /// Board picker bound to the boards in the active network, falling back
    /// to free text when no network is loaded.
    fn show_config_board_picker(
        &mut self,
        ui: &mut egui::Ui,
        id: &str,
        current: &mut String,
    ) -> bool {
        let boards = self.available_board_names();
        if boards.is_empty() {
            return ui.text_edit_singleline(current).changed();
        }
        egui::ComboBox::from_id_salt((id, &self.title))
            .selected_text(current.as_str())
            .show_ui(ui, |ui| {
                boards.iter().fold(false, |changed, board| {
                    ui.selectable_value(current, board.clone(), board).changed() || changed
                })
            })
            .inner
            .unwrap_or(false)
    }

    fn connect_source(&self) -> connection::ConnectionSource {
        settings::FilSettings::connection_source(
            self.executable.clone().expect("checked executable"),
            self.network.clone(),
            self.bus.clone(),
            self.trace_bus.clone(),
            self.elf_overrides.clone(),
            self.disabled_boards.clone(),
            self.use_builder,
            self.builder.clone(),
            self.run_options.clone(),
        )
        .expect("checked network")
    }

    pub fn show(
        &mut self,
        ui: &mut egui::Ui,
        actions: &mut Vec<action::AppAction>,
        tx: &std::sync::mpsc::Sender<daqcore::can_thread::CanThreadCommand>,
        connection_status: &app::ConnectionStatus,
    ) -> egui_tiles::UiResponse {
        self.refresh_network_info();
        egui::ScrollArea::vertical().show(ui, |ui| {
            ui.heading("FIL real-time emulator");
            if let Some(error) = &self.annotation_error {
                ui.colored_label(
                    egui::Color32::LIGHT_RED,
                    format!("FIL annotation config unavailable: {error}"),
                );
            }
            let status = match connection_status {
                app::ConnectionStatus::Disconnected => {
                    egui::RichText::new("● Disconnected").color(egui::Color32::GRAY)
                }
                app::ConnectionStatus::Connected => {
                    egui::RichText::new("● Connected").color(egui::Color32::GREEN)
                }
                app::ConnectionStatus::Error(error) => {
                    egui::RichText::new(format!("● {error}")).color(egui::Color32::RED)
                }
            };
            ui.label(status);

            ui.group(|ui| {
                ui.heading("Process configuration");
                ui.horizontal(|ui| {
                    if ui.button("Select FIL executable").clicked()
                        && let Some(path) = rfd::FileDialog::new().pick_file()
                    {
                        self.executable = Some(path);
                        self.queue_config_update(actions);
                    }
                    ui.label(
                        self.executable
                            .as_ref()
                            .map(|path| path.display().to_string())
                            .unwrap_or_else(|| "None selected".into()),
                    );
                });
                ui.horizontal(|ui| {
                    ui.label("Network source:");
                    if ui.radio(!self.use_builder, "Network file").clicked() {
                        self.use_builder = false;
                        self.queue_builder_update(actions);
                    }
                    if ui.radio(self.use_builder, "Build network").clicked() {
                        self.use_builder = true;
                        self.queue_builder_update(actions);
                    }
                });
                if self.use_builder {
                    self.show_builder(ui, actions);
                } else {
                    self.show_file_network(ui, actions);
                }
                self.show_trace_bus_selector(ui, actions, tx);
                ui.collapsing("Run options", |ui| {
                    let mut changed = false;
                    ui.horizontal(|ui| {
                        ui.label("Duration (ms, 0 = unlimited):");
                        changed |= ui
                            .add(
                                egui::DragValue::new(&mut self.run_options.duration_ms)
                                    .range(0..=u64::MAX / 1_000_000),
                            )
                            .changed();
                    });
                    ui.horizontal(|ui| {
                        ui.label("Max instructions per board:");
                        changed |= ui
                            .add(
                                egui::DragValue::new(&mut self.run_options.max_instructions)
                                    .range(1..=u64::MAX),
                            )
                            .changed();
                    });
                    ui.horizontal(|ui| {
                        ui.label("Instruction quantum:");
                        changed |= ui
                            .add(
                                egui::DragValue::new(&mut self.run_options.quantum)
                                    .range(1..=u64::MAX),
                            )
                            .changed();
                    });
                    ui.horizontal(|ui| {
                        ui.label("Refresh interval (ms):");
                        changed |= ui
                            .add(
                                egui::DragValue::new(&mut self.run_options.refresh_ms)
                                    .range(1..=i32::MAX as u32),
                            )
                            .changed();
                    });
                    ui.horizontal(|ui| {
                        ui.label("ADC decimation (1 = every scan):");
                        changed |= ui
                            .add(
                                egui::DragValue::new(&mut self.run_options.adc_decimation)
                                    .range(1..=1024),
                            )
                            .changed();
                    });
                    if self.run_options.adc_decimation > 1 {
                        ui.small(
                            "Decimation skips ADC scans; use 1 for pedal and fault validation.",
                        );
                    }
                    ui.horizontal(|ui| {
                        ui.label("Additional live trace filters (comma-separated):");
                        changed |= ui
                            .text_edit_singleline(&mut self.run_options.extra_live_filters)
                            .changed();
                    });
                    changed |= ui
                        .checkbox(&mut self.run_options.strict_mmio, "Strict MMIO")
                        .changed();
                    changed |= ui
                        .checkbox(&mut self.run_options.wall_pacing, "Wall-clock pacing")
                        .changed();
                    changed |= ui
                        .checkbox(&mut self.run_options.loop_batching, "Loop batching")
                        .changed();
                    changed |= ui
                        .checkbox(
                            &mut self.run_options.trace_instructions,
                            "Trace instructions",
                        )
                        .changed();
                    changed |= ui
                        .checkbox(&mut self.run_options.detect_spin, "Detect spin loops")
                        .changed();
                    if changed {
                        self.queue_config_update(actions);
                    }
                });
                ui.add_space(4.0);
                ui.horizontal(|ui| {
                    let issues = self.connect_issues();
                    if ui
                        .add_enabled(issues.is_empty(), egui::Button::new("Connect / Restart"))
                        .clicked()
                    {
                        actions.push(action::AppAction::ConnectFil(self.connect_source()));
                    }
                    if ui.button("Disconnect").clicked() && self.executable.is_some() {
                        let _ = tx.send(daqcore::can_thread::CanThreadCommand::Connect(None));
                    }
                });
                for issue in self.connect_issues() {
                    ui.label(
                        egui::RichText::new(format!("⚠ {issue}")).color(egui::Color32::YELLOW),
                    );
                }
            });

            ui.add_space(8.0);
            ui.group(|ui| {
                ui.horizontal(|ui| {
                    ui.heading("CAN expectations");
                    if ui.button("Clear").clicked() {
                        clear_expectations(
                            &mut self.expectations,
                            &mut self.expectation_order,
                        );
                    }
                });
                let count = |status| {
                    self.expectations
                        .values()
                        .filter(|event| event.status == status)
                        .count()
                };
                ui.label(format!(
                    "Pending: {}   Pass: {}   Fail: {}   Incomplete: {}",
                    count(daqcore::can::driver::FilExpectationStatus::Pending),
                    count(daqcore::can::driver::FilExpectationStatus::Pass),
                    count(daqcore::can::driver::FilExpectationStatus::Fail),
                    count(daqcore::can::driver::FilExpectationStatus::Incomplete)
                ));
                if self.expectation_order.is_empty() {
                    ui.weak("No FIL expectation trace records received.");
                } else {
                    egui::ScrollArea::vertical()
                        .max_height(180.0)
                        .show(ui, |ui| {
                            ui.style_mut().wrap_mode = Some(egui::TextWrapMode::Wrap);
                            for id in &self.expectation_order {
                                let Some(event) = self.expectations.get(id) else {
                                    continue;
                                };
                                let (status, color) = match event.status {
                                    daqcore::can::driver::FilExpectationStatus::Pending => {
                                        ("Pending", egui::Color32::YELLOW)
                                    }
                                    daqcore::can::driver::FilExpectationStatus::Pass => {
                                        ("Passed", egui::Color32::LIGHT_GREEN)
                                    }
                                    daqcore::can::driver::FilExpectationStatus::Fail => {
                                        ("Failed", egui::Color32::LIGHT_RED)
                                    }
                                    daqcore::can::driver::FilExpectationStatus::Incomplete => {
                                        ("Incomplete", egui::Color32::GRAY)
                                    }
                                };
                                egui::CollapsingHeader::new(
                                    egui::RichText::new(format!(
                                        "{status} · {}",
                                        expectation_name(event)
                                    ))
                                    .color(color),
                                )
                                .id_salt(("fil_expectation", &self.title, id))
                                .show(ui, |ui| {
                                    ui.label(format!(
                                        "Expected: {} · 0x{:X} · {}",
                                        event.expected_bus,
                                        event.expected_id,
                                        if event.expected_extended {
                                            "extended"
                                        } else {
                                            "standard"
                                        },
                                    ))
                                    .on_hover_text(format!("{}\n{}", event.check_id, event.script));
                                    ui.label(format!(
                                        "Data: {}",
                                        expectation_bytes(&event.expected_data)
                                    ));
                                    ui.label(format!(
                                        "Window: {}–{} ms",
                                        expectation_ms(event.window_start_ns),
                                        expectation_ms(event.window_end_ns),
                                    ))
                                    .on_hover_text(format!(
                                        "{}–{} ns (inclusive)",
                                        event.window_start_ns, event.window_end_ns
                                    ));
                                    if let Some(reason) = &event.reason {
                                        ui.label(format!("Result: {reason}"));
                                    } else {
                                        ui.label(match event.status {
                                            daqcore::can::driver::FilExpectationStatus::Pending => {
                                                "Result: waiting for matching firmware output"
                                            }
                                            daqcore::can::driver::FilExpectationStatus::Pass => {
                                                "Result: matching firmware output received"
                                            }
                                            daqcore::can::driver::FilExpectationStatus::Fail => {
                                                "Result: no match within the window"
                                            }
                                            daqcore::can::driver::FilExpectationStatus::Incomplete => {
                                                "Result: run ended before the window resolved"
                                            }
                                        });
                                    }
                                    if let Some(bus) = &event.matched_bus {
                                        let frame = event
                                            .matched_id
                                            .map(|id| format!(" · 0x{id:X}"))
                                            .unwrap_or_default();
                                        ui.label(format!("Matched: {bus}{frame}"));
                                    }
                                    if let Some(data) = &event.matched_data {
                                        ui.label(format!(
                                            "Matched data: {}",
                                            expectation_bytes(data)
                                        ));
                                    }
                                    if let Some(origin) = &event.matched_origin {
                                        ui.label(format!("From: {origin}"));
                                    }
                                    if let Some(time) = event.matched_time_ns {
                                        ui.label(format!("At: {} ms", expectation_ms(time)))
                                            .on_hover_text(format!("{time} ns"));
                                    }
                                })
                                .header_response
                                .on_hover_text(format!("{}\n{}", event.check_id, event.script));
                            }
                        });
                }
            });

            ui.add_space(8.0);
            ui.group(|ui| {

                ui.heading("ADC injection");
                ui.horizontal(|ui| {
                    ui.label("Board:");
                    let mut board = std::mem::take(&mut self.adc_board);
                    if self.show_config_board_picker(ui, "fil_adc_board", &mut board) {
                        actions.push(action::AppAction::UpdateFilAdc {
                            board: board.clone(),
                            instance: self.adc_instance,
                            channel: self.adc_channel,
                            value: self.adc_value,
                        });
                    }
                    self.adc_board = board;
                    ui.label("Instance:");
                    let adc_annotation = self
                        .annotations
                        .adc
                        .as_ref()
                        .and_then(|boards| boards.get(&self.adc_board))
                        .and_then(|instances| instances.get(self.adc_instance.as_str()));
                    let selected_instance = annotated_text(
                        self.adc_instance.as_str(),
                        adc_annotation.and_then(|annotation| annotation.label.as_deref()),
                    );
                    let instance_labels: Vec<(FilAdcInstance, String)> = FilAdcInstance::ALL
                        .into_iter()
                        .map(|instance| {
                            let label = self
                                .annotations
                                .adc
                                .as_ref()
                                .and_then(|boards| boards.get(&self.adc_board))
                                .and_then(|instances| instances.get(instance.as_str()))
                                .and_then(|annotation| annotation.label.as_deref());
                            (instance, annotated_text(instance.as_str(), label))
                        })
                        .collect();

                    egui::ComboBox::from_id_salt(("fil_adc_instance", &self.title))
                        .selected_text(selected_instance)
                        .show_ui(ui, |ui| {
                            for (instance, label) in &instance_labels {
                                ui.selectable_value(&mut self.adc_instance, *instance, label);
                            }
                        });
                });
                ui.horizontal(|ui| {
                    ui.label("Channel:");
                    let channel_annotations = self
                        .annotations
                        .adc
                        .as_ref()
                        .and_then(|boards| boards.get(&self.adc_board))
                        .and_then(|instances| instances.get(self.adc_instance.as_str()))
                        .and_then(|annotation| annotation.channels.as_ref());
                    let selected_channel = annotated_text(
                        &self.adc_channel.to_string(),
                        channel_annotations
                            .and_then(|channels| channels.get(&self.adc_channel))
                            .map(String::as_str),
                    );
                    egui::ComboBox::from_id_salt(("fil_adc_channel", &self.title))
                        .selected_text(selected_channel)
                        .show_ui(ui, |ui| {
                            for channel in 0..=19u8 {
                                let label = annotated_text(
                                    &channel.to_string(),
                                    channel_annotations
                                        .and_then(|channels| channels.get(&channel))
                                        .map(String::as_str),
                                );
                                ui.selectable_value(&mut self.adc_channel, channel, label);
                            }
                        });
                    ui.label("Raw value:");
                    ui.add(egui::Slider::new(&mut self.adc_value, 0..=4095));
                    if ui.button("Inject").clicked() {
                        actions.push(action::AppAction::UpdateFilAdc {
                            board: self.adc_board.clone(),
                            instance: self.adc_instance,
                            channel: self.adc_channel,
                            value: self.adc_value,
                        });
                        let _ = tx.send(daqcore::can_thread::CanThreadCommand::SetFilAdc {
                            board: self.adc_board.clone(),
                            instance: self.adc_instance.as_str().to_owned(),
                            channel: self.adc_channel,
                            value: self.adc_value,
                        });
                    }
                });
            });

            ui.add_space(8.0);
            ui.group(|ui| {
                ui.heading("GPIO live view and input control");
                ui.horizontal(|ui| {
                    ui.label("Board:");
                    let mut board = std::mem::take(&mut self.gpio_board);
                    let changed = self.show_config_board_picker(ui, "fil_gpio_board", &mut board);
                    self.gpio_board = board;
                    let _ = changed;
                    ui.label("Port:");
                    let port_labels: Vec<(FilGpioPort, String)> = FilGpioPort::ALL
                        .into_iter()

                        .map(|port| {
                            let label = self
                                .annotations
                                .gpio
                                .as_ref()
                                .and_then(|boards| boards.get(&self.gpio_board))
                                .and_then(|ports| ports.get(port.as_str()))
                                .and_then(|annotation| annotation.label.as_deref());
                            (port, annotated_text(port.as_str(), label))
                        })
                        .collect();
                    let selected_port = port_labels
                        .iter()
                        .find(|(port, _)| port == &self.gpio_port)
                        .map(|(_, label)| label.as_str())
                        .unwrap_or(self.gpio_port.as_str());
                    egui::ComboBox::from_id_salt(("fil_gpio_port", &self.title))
                        .selected_text(selected_port)
                        .show_ui(ui, |ui| {
                            for (port, label) in &port_labels {
                                ui.selectable_value(&mut self.gpio_port, *port, label);
                            }
                        });
                });
                egui::Grid::new(("fil_gpio_grid", &self.title))
                    .striped(true)
                    .show(ui, |ui| {
                        ui.strong("Pin");
                        ui.strong("Input override");
                        ui.strong("Firmware output");
                        ui.strong("Drive input");
                        ui.end_row();
                        for pin in 0..16u8 {
                            let state = self
                                .gpio_states
                                .get(&(self.gpio_board.clone(), self.gpio_port, pin))
                                .copied()
                                .unwrap_or_default();
                            let pin_label = self
                                .annotations
                                .gpio
                                .as_ref()
                                .and_then(|boards| boards.get(&self.gpio_board))
                                .and_then(|ports| ports.get(self.gpio_port.as_str()))
                                .and_then(|annotation| annotation.pins.as_ref())
                                .and_then(|pins| pins.get(&pin))
                                .map(String::as_str);
                            ui.label(annotated_text(
                                &format!("{}{}", self.gpio_port.as_str(), pin),
                                pin_label,
                            ));
                            ui.label(level_text(state.input, "Released"));
                            ui.label(level_text(state.output, "Unknown"));
                            ui.horizontal(|ui| {
                                for (label, value) in [
                                    ("Low", Some(false)),
                                    ("High", Some(true)),
                                    ("Release", None),
                                ] {
                                    if ui.small_button(label).clicked() {
                                        let _ = tx.send(daqcore::can_thread::CanThreadCommand::SetFilGpio {
                                            board: self.gpio_board.clone(),
                                            port: self.gpio_port.as_str().to_owned(),
                                            pin,
                                            value,
                                        });

                                    }
                                }
                            });
                            ui.end_row();
                        }
                    });
            });
        });
        egui_tiles::UiResponse::None
    }
}

fn annotated_text(identifier: &str, annotation: Option<&str>) -> String {
    match annotation.map(str::trim).filter(|label| !label.is_empty()) {
        Some(label) => format!("{identifier} — {label}"),
        None => identifier.to_owned(),
    }
}

fn elf_status(effective: &Option<std::path::PathBuf>, is_override: bool) -> egui::RichText {
    match effective {
        Some(elf) if elf.is_file() => {
            let text = if is_override {
                format!("⚙ {}", elf.display())
            } else {
                elf.display().to_string()
            };
            egui::RichText::new(text)
        }
        Some(elf) => egui::RichText::new(format!("{} (missing)", elf.display()))
            .color(egui::Color32::LIGHT_RED),
        None => egui::RichText::new("No ELF configured").color(egui::Color32::LIGHT_RED),
    }
}

fn expectation_name(event: &daqcore::can::driver::FilExpectationEvent) -> String {
    let script = event
        .script
        .rsplit(['/', '\\'])
        .next()
        .unwrap_or(&event.script);
    let index = event.check_id.rsplit("/expect/").next().unwrap_or("?");
    let attachment = event
        .check_id
        .strip_prefix("stimulus/")
        .and_then(|rest| rest.split('/').next())
        .unwrap_or("?");
    format!("{script} · check {index} · script {attachment}")
}

fn expectation_bytes(data: &[u8]) -> String {
    if data.is_empty() {
        return "(empty)".into();
    }
    data.iter()
        .map(|byte| format!("{byte:02X}"))
        .collect::<Vec<_>>()
        .join(" ")
}

fn expectation_ms(ns: u64) -> String {
    let whole = ns / 1_000_000;
    let remainder = ns % 1_000_000;
    if remainder == 0 {
        whole.to_string()
    } else {
        format!("{whole}.{:06}", remainder)
            .trim_end_matches('0')
            .to_owned()
    }
}

#[cfg(test)]
mod expectation_tests {
    use super::*;

    fn event(
        status: daqcore::can::driver::FilExpectationStatus,
    ) -> daqcore::can::driver::FilExpectationEvent {
        daqcore::can::driver::FilExpectationEvent {
            check_id: "stimulus/0/script/expect/0".into(),
            script: "script".into(),
            status,
            expected_bus: "vehicle".into(),
            expected_id: 0x321,
            expected_extended: false,
            expected_data: vec![1, 2],
            window_start_ns: 10,
            window_end_ns: 20,
            matched_bus: None,
            matched_id: None,
            matched_data: None,
            matched_origin: None,
            matched_time_ns: None,
            reason: None,
        }
    }

    #[test]
    fn history_is_bounded() {
        let mut events = HashMap::new();
        let mut order = Vec::new();
        for index in 0..=FIL_EXPECTATION_HISTORY_LIMIT {
            let mut check = event(daqcore::can::driver::FilExpectationStatus::Pending);
            check.check_id = index.to_string();
            update_expectations(&mut events, &mut order, check);
        }
        assert_eq!(events.len(), FIL_EXPECTATION_HISTORY_LIMIT);
        assert_eq!(order.len(), FIL_EXPECTATION_HISTORY_LIMIT);
        assert!(!events.contains_key("0"));
    }

    #[test]
    fn completed_run_history_is_retained_until_explicit_reset() {
        let mut events = HashMap::new();
        let mut order = Vec::new();
        update_expectations(
            &mut events,
            &mut order,
            event(daqcore::can::driver::FilExpectationStatus::Fail),
        );
        assert_eq!(events.len(), 1);
        clear_expectations(&mut events, &mut order);
        assert!(events.is_empty());
        assert!(order.is_empty());
    }

    #[test]
    fn lifecycle_replaces_pending_check_without_duplicate_history() {
        let mut events = HashMap::new();
        let mut order = Vec::new();
        update_expectations(
            &mut events,
            &mut order,
            event(daqcore::can::driver::FilExpectationStatus::Pending),
        );
        let mut passed = event(daqcore::can::driver::FilExpectationStatus::Pass);
        passed.matched_bus = Some("vehicle".into());
        passed.matched_id = Some(0x321);
        passed.matched_data = Some(vec![1, 2]);
        passed.matched_time_ns = Some(15);
        update_expectations(&mut events, &mut order, passed);
        assert_eq!(order.len(), 1);
        assert_eq!(events.len(), 1);
        let stored = events.values().next().unwrap();
        assert_eq!(
            stored.status,
            daqcore::can::driver::FilExpectationStatus::Pass
        );
        assert_eq!(stored.matched_time_ns, Some(15));
        clear_expectations(&mut events, &mut order);
        assert!(events.is_empty());
        assert!(order.is_empty());
    }
}

fn level_text(value: Option<bool>, unset: &str) -> egui::RichText {
    match value {
        Some(true) => egui::RichText::new("HIGH").color(egui::Color32::LIGHT_GREEN),
        Some(false) => egui::RichText::new("LOW").color(egui::Color32::LIGHT_RED),
        None => egui::RichText::new(unset).color(egui::Color32::GRAY),
    }
}
