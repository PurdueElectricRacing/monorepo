use super::*;

impl FilControl {
    pub fn show_builder(&mut self, ui: &mut egui::Ui, actions: &mut Vec<action::AppAction>) {
        ui.horizontal(|ui| {
            ui.label("Name:");
            if ui
                .text_edit_singleline(&mut self.settings.builder.name)
                .changed()
            {
                self.queue_builder_update(actions);
            }
            ui.label("Bus ID:");
            if ui
                .text_edit_singleline(&mut self.settings.builder.bus)
                .on_hover_text("Use the SuperDBC numeric bus ID, for example 0 for VCAN")
                .changed()
            {
                self.queue_builder_update(actions);
            }
            ui.label("Bitrate:");
            if ui
                .add(
                    egui::DragValue::new(&mut self.settings.builder.bitrate)
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
                    if !self.settings.builder.stimuli.contains(&path) {
                        self.settings.builder.stimuli.push(path);
                        changed = true;
                    }
                }
                if changed {
                    self.queue_builder_update(actions);
                }
            }
        });
        let mut remove_stimulus = None;
        for (index, stimulus) in self.settings.builder.stimuli.iter().enumerate() {
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
            self.settings.builder.stimuli.remove(index);
            self.queue_builder_update(actions);
        }
        ui.add_space(4.0);
        let enabled_count = self
            .settings
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
        for (index, board) in self.settings.builder.boards.clone().iter().enumerate() {
            ui.group(|ui| {
                ui.horizontal(|ui| {
                    let mut enabled = board.enabled;
                    if ui.checkbox(&mut enabled, "Run").changed() {
                        self.settings.builder.boards[index].enabled = enabled;
                        self.queue_builder_update(actions);
                    }
                    ui.label("Name:");
                    let mut name = board.name.clone();
                    if ui.text_edit_singleline(&mut name).changed() {
                        self.settings.builder.boards[index].name = name;
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
                        let entry = &mut self.settings.builder.boards[index];
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
                        config::effective_mcu(board, self.settings.executable.as_deref());
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
                        self.settings.builder.boards[index].mcu = path;
                        self.queue_builder_update(actions);
                    }
                    if !board.mcu.as_os_str().is_empty() && ui.small_button("Auto").clicked() {
                        self.settings.builder.boards[index].mcu = std::path::PathBuf::new();
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
                            self.settings.builder.boards[index].can_instances = set;
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
                        self.settings.builder.boards[index].vector_base = base;
                        self.queue_builder_update(actions);
                    }
                });
            });
        }
        if let Some(index) = remove_index {
            self.settings.builder.boards.remove(index);
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
                self.settings.builder.boards.push(entry);
                self.queue_builder_update(actions);
            }
            if ui
                .add_enabled(
                    self.import_rx.is_none(),
                    egui::Button::new("Import board file…"),
                )
                .clicked()
                && let Some(path) = rfd::FileDialog::new()
                    .add_filter("JSON board config", &["json"])
                    .pick_file()
            {
                self.import_board_file(path);
            }
            if ui
                .add_enabled(
                    self.export_rx.is_none(),
                    egui::Button::new("Export network JSON…"),
                )
                .clicked()
                && let Some(path) = rfd::FileDialog::new()
                    .add_filter("JSON network config", &["json"])
                    .set_file_name(format!("{}.json", self.settings.builder.name))
                    .save_file()
            {
                let executable = self.settings.executable.clone().unwrap_or_default();
                let builder = self.settings.builder.clone();
                let (tx, rx) = mpsc::channel();
                self.export_rx = Some(rx);
                self.operation_error = None;
                thread::spawn(move || {
                    let _ = tx.send(config::export_network(&path, &builder, &executable));
                });
            }
        });
    }
    pub fn import_board_file(&mut self, path: std::path::PathBuf) {
        let (tx, rx) = mpsc::channel();
        self.import_rx = Some(rx);
        self.operation_error = None;
        thread::spawn(move || {
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
            let result = config::load_board_info(&entry.board)
                .map(|_| config::migrate_built_board(&entry))
                .and_then(|board| {
                    if board.board.as_os_str().is_empty() {
                        Ok(board)
                    } else {
                        Err(format!("Unable to migrate {}", board.board.display()))
                    }
                });
            let _ = tx.send(result);
        });
    }
}
