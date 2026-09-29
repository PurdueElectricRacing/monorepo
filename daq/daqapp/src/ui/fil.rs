use crate::{action, app, settings};
use daqcore::{connection, fil_config};
use eframe::egui;
use std::collections::{HashMap, HashSet};

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
    elf_overrides: HashMap<String, std::path::PathBuf>,
    disabled_boards: Vec<String>,
    use_builder: bool,
    builder: daqcore::fil_config::BuiltNetwork,
    network_info: Option<daqcore::fil_config::FilNetworkInfo>,
    network_info_error: Option<String>,
    last_loaded_network: Option<std::path::PathBuf>,
    adc_board: String,
    adc_instance: String,
    adc_channel: u8,
    adc_value: u16,
    run_options: settings::FilRunOptions,
    gpio_board: String,
    gpio_port: String,
    gpio_states: HashMap<(String, String, u8), GpioPinState>,
}

impl FilControl {
    pub fn new(instance_num: usize) -> Self {
        let saved = settings::Settings::load().fil;
        let mut control = Self {
            title: format!("FIL Control #{}", instance_num),
            executable: saved.executable,
            network: saved.network,
            bus: saved.bus,
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
            gpio_board: "dashboard".into(),
            gpio_port: "GPIOA".into(),
            gpio_states: HashMap::new(),
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
        match daqcore::fil_config::load_network_info(&network) {
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

    fn builder_board_name(board: &daqcore::fil_config::BuiltBoard) -> String {
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

    fn enabled_file_boards(&self) -> Vec<&daqcore::fil_config::FilBoardInfo> {
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
        if self.enabled_file_boards().is_empty() {
            issues.push("Enable at least one board below".into());
        }
        for board in &info.boards {
            if self.disabled_boards.contains(&board.name) {
                continue;
            }
            match daqcore::fil_config::effective_elf(board, &self.elf_overrides) {
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
        let enabled: Vec<&daqcore::fil_config::BuiltBoard> = self
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
            match daqcore::fil_config::effective_mcu(board, self.executable.as_deref()) {
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
                if !daqcore::fil_config::FIL_CAN_INSTANCES.contains(&instance) {
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
                let state = self
                    .gpio_states
                    .entry((board.clone(), port.clone(), *pin))
                    .or_default();
                match direction {
                    daqcore::can_thread::FilGpioDirection::Input => state.input = *value,
                    daqcore::can_thread::FilGpioDirection::Output => state.output = *value,
                }
            }
            daqcore::can_thread::CanThreadEvent::Disconnection
            | daqcore::can_thread::CanThreadEvent::ConnectionSuccessful
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
                elf_overrides: self.elf_overrides.clone(),
                disabled_boards: self.disabled_boards.clone(),
                use_builder: self.use_builder,
                builder: self.builder.clone(),
                adc_board: self.adc_board.clone(),
                adc_instance: self.adc_instance.clone(),
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
                    let effective = daqcore::fil_config::effective_elf(board, &self.elf_overrides);
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
                    let effective =
                        daqcore::fil_config::effective_mcu(board, self.executable.as_deref());
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
                        vec![daqcore::fil_config::FIL_CAN_INSTANCES[0]]
                    } else {
                        board.can_instances.iter().map(String::as_str).collect()
                    };
                    for instance in daqcore::fil_config::FIL_CAN_INSTANCES {
                        let mut checked = current.contains(&instance);
                        if ui.checkbox(&mut checked, instance).changed() {
                            let mut set: Vec<String> = daqcore::fil_config::FIL_CAN_INSTANCES
                                .into_iter()
                                .filter(|candidate| {
                                    (*candidate == instance && checked)
                                        || (*candidate != instance && current.contains(candidate))
                                })
                                .map(str::to_owned)
                                .collect();
                            if set.len() == 1 && set[0] == daqcore::fil_config::FIL_CAN_INSTANCES[0]
                            {
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
                let mut entry = daqcore::fil_config::BuiltBoard {
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
                match daqcore::fil_config::export_network(&path, &self.builder, &executable) {
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
        let entry = daqcore::fil_config::BuiltBoard {
            name: String::new(),
            elf: std::path::PathBuf::new(),
            mcu: std::path::PathBuf::new(),
            can_instances: Vec::new(),
            vector_base: String::new(),
            enabled: true,
            board: path,
            elf_override: None,
        };
        let entry = daqcore::fil_config::migrate_built_board(&entry);
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
        } else if egui::ComboBox::from_id_salt(("fil_bus", &self.title))
            .selected_text(&self.bus)
            .show_ui(ui, |ui| {
                for bus in &buses {
                    ui.selectable_value(&mut self.bus, bus.clone(), bus);
                }
            })
            .response
            .changed()
        {
            self.queue_config_update(actions);
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
                for board in &boards {
                    ui.selectable_value(current, board.clone(), board);
                }
            })
            .response
            .changed()
    }

    fn connect_source(&self) -> connection::ConnectionSource {
        let executable = self.executable.clone().expect("checked executable");
        if self.use_builder {
            connection::ConnectionSource::Fil {
                executable,
                network: std::path::PathBuf::new(),
                bus: self.builder.bus.clone(),
                elf_overrides: HashMap::new(),
                disabled_boards: Vec::new(),
                built_network: Some(self.builder.clone()),
                run_options: self.run_options.clone(),
            }
        } else {
            connection::ConnectionSource::Fil {
                executable,
                network: self.network.clone().expect("checked network"),
                bus: self.bus.clone(),
                elf_overrides: self.elf_overrides.clone(),
                disabled_boards: self.disabled_boards.clone(),
                built_network: None,
                run_options: self.run_options.clone(),
            }
        }
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
                    if ui.button("Disconnect").clicked()
                        && let Some(executable) = self.executable.clone()
                    {
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
                ui.heading("ADC injection");
                ui.horizontal(|ui| {
                    ui.label("Board:");
                    let mut board = std::mem::take(&mut self.adc_board);
                    if self.show_config_board_picker(ui, "fil_adc_board", &mut board) {
                        actions.push(action::AppAction::UpdateFilAdc {
                            board: board.clone(),
                            instance: self.adc_instance.clone(),
                            channel: self.adc_channel,
                            value: self.adc_value,
                        });
                    }
                    self.adc_board = board;
                    ui.label("Instance:");
                    egui::ComboBox::from_id_salt(("fil_adc_instance", &self.title))
                        .selected_text(self.adc_instance.as_str())
                        .show_ui(ui, |ui| {
                            for instance in daqcore::fil_config::FIL_ADC_INSTANCES {
                                ui.selectable_value(
                                    &mut self.adc_instance,
                                    instance.to_owned(),
                                    instance,
                                );
                            }
                        });
                });
                ui.horizontal(|ui| {
                    ui.label("Channel:");
                    ui.add(egui::DragValue::new(&mut self.adc_channel).range(0..=19));
                    ui.label("Raw value:");
                    ui.add(egui::Slider::new(&mut self.adc_value, 0..=4095));
                    if ui.button("Inject").clicked() {
                        actions.push(action::AppAction::UpdateFilAdc {
                            board: self.adc_board.clone(),
                            instance: self.adc_instance.clone(),
                            channel: self.adc_channel,
                            value: self.adc_value,
                        });
                        let _ = tx.send(daqcore::can_thread::CanThreadCommand::SetFilAdc {
                            board: self.adc_board.clone(),
                            instance: self.adc_instance.clone(),
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
                    egui::ComboBox::from_id_salt(("fil_gpio_port", &self.title))
                        .selected_text(self.gpio_port.as_str())
                        .show_ui(ui, |ui| {
                            for port in daqcore::fil_config::FIL_GPIO_PORTS {
                                ui.selectable_value(&mut self.gpio_port, port.to_owned(), port);
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
                                .get(&(self.gpio_board.clone(), self.gpio_port.clone(), pin))
                                .copied()
                                .unwrap_or_default();
                            ui.label(format!("{}{}", self.gpio_port, pin));
                            ui.label(level_text(state.input, "Released"));
                            ui.label(level_text(state.output, "Unknown"));
                            ui.horizontal(|ui| {
                                for (label, value) in [
                                    ("Low", Some(false)),
                                    ("High", Some(true)),
                                    ("Release", None),
                                ] {
                                    if ui.small_button(label).clicked() {
                                        let _ = tx.send(
                                            daqcore::can_thread::CanThreadCommand::SetFilGpio {
                                                board: self.gpio_board.clone(),
                                                port: self.gpio_port.clone(),
                                                pin,
                                                value,
                                            },
                                        );
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

fn level_text(value: Option<bool>, unset: &str) -> egui::RichText {
    match value {
        Some(true) => egui::RichText::new("HIGH").color(egui::Color32::LIGHT_GREEN),
        Some(false) => egui::RichText::new("LOW").color(egui::Color32::LIGHT_RED),
        None => egui::RichText::new(unset).color(egui::Color32::GRAY),
    }
}
