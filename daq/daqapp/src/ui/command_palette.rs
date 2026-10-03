use crate::action;

pub struct CommandPalette {
    show_command_palette: bool,
    palette_search: String,
    palette_index: usize,
}

impl CommandPalette {
    pub fn new() -> Self {
        Self {
            show_command_palette: false,
            palette_search: String::new(),
            palette_index: 0,
        }
    }

    pub fn toggle(&mut self) {
        self.show_command_palette = !self.show_command_palette;
        if self.show_command_palette {
            self.palette_search.clear();
            self.palette_index = 0;
        }
    }

    pub fn ui(&mut self, ctx: &eframe::egui::Context) -> Vec<action::AppAction> {
        if !self.show_command_palette {
            return Vec::new();
        }

        let mut action_queue = Vec::new();

        let cmd_list = action::AppAction::cmd_palette_list();

        // filtering Logic
        let filtered_options: Vec<_> = cmd_list
            .iter()
            .filter(|(label, _)| {
                if self.palette_search.is_empty() {
                    true
                } else {
                    label
                        .to_lowercase()
                        .contains(&self.palette_search.to_lowercase())
                }
            })
            .collect();

        if !filtered_options.is_empty() {
            self.palette_index = self.palette_index.min(filtered_options.len() - 1);
        }

        // handle keyboard navigation (maybe needs to be smarter?)
        ctx.input_mut(|i| {
            if i.key_pressed(eframe::egui::Key::ArrowDown) && !filtered_options.is_empty() {
                self.palette_index = (self.palette_index + 1) % filtered_options.len();
            }
            if i.key_pressed(eframe::egui::Key::ArrowUp) && !filtered_options.is_empty() {
                self.palette_index =
                    (self.palette_index + filtered_options.len() - 1) % filtered_options.len();
            }
            if i.key_pressed(eframe::egui::Key::Enter) && !filtered_options.is_empty() {
                action_queue.push(action::AppAction::SpawnWidget(
                    filtered_options[self.palette_index].1.clone(),
                ));
                self.show_command_palette = false;
            }
            if i.key_pressed(eframe::egui::Key::Escape) {
                self.show_command_palette = false;
            }
        });

        // render UI
        eframe::egui::Window::new("Command Palette")
            .anchor(eframe::egui::Align2::CENTER_CENTER, [0.0, 0.0])
            .collapsible(false)
            .resizable(false)
            .title_bar(false)
            .fixed_size([300.0, 400.0])
            .show(ctx, |ui| {
                // Search Input
                ui.horizontal(|ui| {
                    ui.label("🔍");
                    let response = ui.text_edit_singleline(&mut self.palette_search);

                    response.request_focus(); // focus the search box when palette is open

                    if response.changed() {
                        self.palette_index = 0; // reset selection to top when search changes
                    }
                });
                ui.separator();

                // Results List
                ui.vertical_centered_justified(|ui| {
                    if filtered_options.is_empty() {
                        ui.label("No commands found");
                    } else {
                        for (i, (label, widget_type)) in filtered_options.iter().enumerate() {
                            let is_selected = i == self.palette_index;
                            let selection = ui.visuals().selection;

                            // todo this styling is kinda janky, theres probably an easy way to do this
                            let button = eframe::egui::Button::new(*label)
                                .fill(if is_selected {
                                    selection.bg_fill
                                } else {
                                    eframe::egui::Color32::TRANSPARENT
                                })
                                .stroke(if is_selected {
                                    selection.stroke
                                } else {
                                    eframe::egui::Stroke::NONE
                                });

                            let response = ui.add(button);
                            if response.clicked() {
                                action_queue
                                    .push(action::AppAction::SpawnWidget((*widget_type).clone()));
                                self.show_command_palette = false;
                            }
                        }
                    }
                });
            });

        action_queue
    }
}
