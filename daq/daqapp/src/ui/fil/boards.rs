use super::*;

impl FilControl {
    pub fn available_board_names(&self) -> Vec<String> {
        if self.settings.use_builder {
            self.settings.builder
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
    pub fn enabled_file_boards(&self) -> Vec<&config::FilBoardInfo> {
        let disabled: HashSet<&str> = self.settings.disabled_boards.iter().map(String::as_str).collect();
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
    pub fn show_file_network(&mut self, ui: &mut egui::Ui, actions: &mut Vec<action::AppAction>) {
        ui.horizontal(|ui| {
            if ui.button("Select network").clicked()
                && let Some(path) = rfd::FileDialog::new()
                    .add_filter("JSON network config", &["json"])
                    .pick_file()
            {
                self.settings.network = Some(path);
                self.refresh_network_info();
                self.queue_config_update(actions);
            }
            ui.label(
                self.settings.network
                    .as_ref()
                    .map(|path| path.display().to_string())
                    .unwrap_or_else(|| "None selected".into()),
            );
        });
        ui.add_space(4.0);
        self.show_file_board_selector(ui, actions);
    }
    pub fn show_file_board_selector(
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
                    let mut enabled = !self.settings.disabled_boards.contains(&board.name);
                    if ui.checkbox(&mut enabled, "").changed() {
                        self.set_board_enabled(&board.name, enabled, actions);
                    }
                    ui.label(&board.name);
                    let effective = config::effective_elf(board, &self.settings.elf_overrides);
                    let is_override = self.settings.elf_overrides.contains_key(&board.name);
                    ui.label(elf_status(&effective, is_override));
                    ui.horizontal(|ui| {
                        if ui.small_button("Select ELF…").clicked()
                            && let Some(path) = rfd::FileDialog::new()
                                .add_filter("ELF firmware", &["elf"])
                                .pick_file()
                        {
                            self.settings.elf_overrides.insert(board.name.clone(), path);
                            self.queue_config_update(actions);
                        }
                        if is_override && ui.small_button("Reset").clicked() {
                            self.settings.elf_overrides.remove(&board.name);
                            self.queue_config_update(actions);
                        }
                    });
                    ui.end_row();
                }
            });
    }
    pub fn show_config_board_picker(
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
                let mut changed = false;
                for board in boards {
                    changed |= ui.selectable_value(current, board.clone(), board).changed();
                }
                changed
            })
            .inner
            .unwrap_or(false)
    }
}

