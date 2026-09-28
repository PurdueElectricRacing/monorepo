use crate::{action, app, settings};
use daqcore::connection;
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
    builder_infos: HashMap<std::path::PathBuf, Result<daqcore::fil_config::FilBoardInfo, String>>,
    network_info: Option<daqcore::fil_config::FilNetworkInfo>,
    network_info_error: Option<String>,
    last_loaded_network: Option<std::path::PathBuf>,
    adc_board: String,
    adc_instance: String,
    adc_channel: u8,
    adc_value: u16,
    gpio_board: String,
    gpio_port: String,
    gpio_states: HashMap<(String, String, u8), GpioPinState>,
}

impl FilControl {
    pub fn new(instance_num: usize) -> Self {
        let saved = settings::Settings::load();
        let mut control = Self {
            title: format!("FIL Control #{}", instance_num),
            executable: saved.fil_executable,
            network: saved.fil_network_config,
            bus: saved.fil_bus,
            elf_overrides: saved.fil_elf_overrides,
            disabled_boards: saved.fil_disabled_boards,
            use_builder: saved.fil_use_builder,
            builder: saved.fil_builder,
            builder_infos: HashMap::new(),
            network_info: None,
            network_info_error: None,
            last_loaded_network: None,
            adc_board: saved.fil_adc_board,
            adc_instance: saved.fil_adc_instance,
            adc_channel: saved.fil_adc_channel.min(19),
            adc_value: saved.fil_adc_value.min(4095),
            gpio_board: "dashboard".into(),
            gpio_port: "GPIOA".into(),
            gpio_states: HashMap::new(),
        };
        control.refresh_network_info();
        control.refresh_builder_infos();
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

    /// (Re)load cached info for builder board files missing from the cache.
    fn refresh_builder_infos(&mut self) {
        for board in &self.builder.boards {
            if self.builder_infos.contains_key(&board.board) {
                continue;
            }
            let info = if board.board.is_file() {
                daqcore::fil_config::load_board_info(&board.board)
            } else {
                Err(format!(
                    "Board config does not exist: {}",
                    board.board.display()
                ))
            };
            self.builder_infos.insert(board.board.clone(), info);
        }
    }

    fn builder_board_name(&self, board: &daqcore::fil_config::BuiltBoard) -> String {
        self.builder_infos
            .get(&board.board)
            .and_then(|info| info.as_ref().ok())
            .map(|info| info.name.clone())
            .unwrap_or_else(|| {
                board
                    .board
                    .file_stem()
                    .map(|stem| stem.to_string_lossy().into_owned())
                    .unwrap_or_else(|| "board".into())
            })
    }

    /// Board names available for ADC/GPIO targeting in the active mode.
    fn available_board_names(&self) -> Vec<String> {
        if self.use_builder {
            self.builder
                .boards
                .iter()
                .filter(|board| board.enabled)
                .map(|board| self.builder_board_name(board))
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
        let enabled: Vec<&daqcore::fil_config::BuiltBoard> = self
            .builder
            .boards
            .iter()
            .filter(|board| board.enabled)
            .collect();
        if enabled.is_empty() {
            issues.push("Add and enable at least one board below".into());
        }
        for board in enabled {
            let name = self.builder_board_name(board);
            match self.builder_infos.get(&board.board) {
                Some(Ok(info)) => match &board.elf_override {
                    Some(elf) if !elf.is_file() => issues.push(format!(
                        "Board '{name}' ELF override does not exist: {}",
                        elf.display()
                    )),
                    None if info.default_elf.is_none() => {
                        issues.push(format!("Board '{name}' has no firmware ELF; select one"))
                    }
                    None if !info.default_elf_exists => issues.push(format!(
                        "Board '{name}' ELF does not exist: {}",
                        info.default_elf
                            .as_ref()
                            .map(|elf| elf.display().to_string())
                            .unwrap_or_default()
                    )),
                    _ => {}
                },
                Some(Err(error)) => issues.push(error.clone()),
                None => issues.push(format!("Loading board {}…", board.board.display())),
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
            executable: self.executable.clone(),
            network: self.network.clone(),
            bus: self.bus.clone(),
            elf_overrides: self.elf_overrides.clone(),
            disabled_boards: self.disabled_boards.clone(),
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
                    ui.strong(self.builder_board_name(board));
                    if ui.small_button("Change board…").clicked()
                        && let Some(path) = rfd::FileDialog::new()
                            .add_filter("JSON board config", &["json"])
                            .pick_file()
                    {
                        self.builder.boards[index].board = path.clone();
                        self.builder_infos.remove(&path);
                        self.refresh_builder_infos();
                        self.queue_builder_update(actions);
                    }
                    if ui.small_button("Remove").clicked() {
                        remove_index = Some(index);
                    }
                });
                ui.horizontal(|ui| {
                    ui.label(
                        board
                            .board
                            .file_name()
                            .map(|name| name.to_string_lossy().into_owned())
                            .unwrap_or_else(|| board.board.display().to_string()),
                    )
                    .on_hover_text(board.board.display().to_string());
                    if let Some(Err(error)) = self.builder_infos.get(&board.board) {
                        ui.label(egui::RichText::new(error).color(egui::Color32::LIGHT_RED));
                    }
                });
                ui.horizontal(|ui| {
                    ui.label("ELF:");
                    let effective = match self.builder_infos.get(&board.board) {
                        Some(Ok(info)) => board
                            .elf_override
                            .clone()
                            .or_else(|| info.default_elf.clone()),
                        _ => board.elf_override.clone(),
                    };
                    ui.label(elf_status(&effective, board.elf_override.is_some()));
                    if ui.small_button("Select ELF…").clicked()
                        && let Some(path) = rfd::FileDialog::new()
                            .add_filter("ELF firmware", &["elf"])
                            .pick_file()
                    {
                        self.builder.boards[index].elf_override = Some(path);
                        self.queue_builder_update(actions);
                    }
                    if board.elf_override.is_some() && ui.small_button("Reset").clicked() {
                        self.builder.boards[index].elf_override = None;
                        self.queue_builder_update(actions);
                    }
                });
            });
        }
        if let Some(index) = remove_index {
            let removed = self.builder.boards.remove(index);
            self.builder_infos.remove(&removed.board);
            self.queue_builder_update(actions);
        }
        ui.horizontal(|ui| {
            if ui.button("Add board…").clicked()
                && let Some(path) = rfd::FileDialog::new()
                    .add_filter("JSON board config", &["json"])
                    .pick_file()
            {
                self.builder.boards.push(daqcore::fil_config::BuiltBoard {
                    board: path,
                    elf_override: None,
                    enabled: true,
                });
                self.refresh_builder_infos();
                self.queue_builder_update(actions);
            }
            if ui.button("Export network JSON…").clicked()
                && let Some(path) = rfd::FileDialog::new()
                    .add_filter("JSON network config", &["json"])
                    .set_file_name(format!("{}.json", self.builder.name))
                    .save_file()
            {
                match daqcore::fil_config::export_network(&path, &self.builder) {
                    Ok(()) => log::info!("Exported FIL network to {}", path.display()),
                    Err(error) => log::error!("Failed to export FIL network: {error}"),
                }
            }
        });
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
            }
        } else {
            connection::ConnectionSource::Fil {
                executable,
                network: self.network.clone().expect("checked network"),
                bus: self.bus.clone(),
                elf_overrides: self.elf_overrides.clone(),
                disabled_boards: self.disabled_boards.clone(),
                built_network: None,
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
        self.refresh_builder_infos();
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
                    ui.text_edit_singleline(&mut self.adc_instance);
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
                    ui.text_edit_singleline(&mut self.gpio_port);
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
