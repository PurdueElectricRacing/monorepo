use eframe::egui;

/// Shared DBC message search UI state used by Send UI, Jitter, etc.
#[derive(Default)]
pub struct DbcMsgPickerState {
    search_text: String,
    search_results: Vec<daqcore::can::MessageId>,
}

impl DbcMsgPickerState {
    /// Refresh [`Self::search_results`] from [`Self::search_text`] using the same rules as before:
    /// empty clears results, `*` lists all messages, otherwise filter by name and hex ID substring.
    pub fn refresh_results(&mut self, parser: &daqcore::superdbc::BusDatabase) {
        self.search_results.clear();
        if self.search_text.is_empty() {
            return;
        }
        let search = self.search_text.to_lowercase();
        self.search_results.extend(
            parser
                .msg_defs()
                .iter()
                .filter(|m| {
                    search.trim() == "*"
                        || m.name.to_lowercase().contains(&search)
                        || format!("0x{:03x}", m.id.raw()).contains(&search)
                })
                .map(|m| m.id),
        );
    }

    /// Search field, hints, and result buttons. Returns [`Some`] when the user picked a message
    /// (search buffer is cleared on pick).
    pub fn show(
        &mut self,
        ui: &mut egui::Ui,
        parser: &daqcore::superdbc::BusDatabase,
        selected_msg_is_none: bool,
    ) -> Option<daqcore::superdbc::MessageDef> {
        ui.horizontal(|ui| {
            ui.label("Search:");
            if ui
                .add(egui::TextEdit::singleline(&mut self.search_text).hint_text("Message name..."))
                .changed()
            {
                self.refresh_results(parser);
            }
        });

        ui.add_space(8.0);

        if self.search_results.is_empty() && !self.search_text.is_empty() {
            ui.label(egui::RichText::new("No messages found.").italics().weak());
            return None;
        }

        if self.search_text.is_empty() && selected_msg_is_none {
            ui.label(
                egui::RichText::new(
                    "Start typing to search for messages... (Use * to show all messages.)",
                )
                .italics()
                .weak(),
            );
            return None;
        }

        let mut picked = None;
        for id in &self.search_results {
            let Some(msg) = parser.bus().message(*id) else {
                continue;
            };
            if ui
                .button(format!("{} (0x{:03X})", msg.name, msg.id.raw()))
                .clicked()
            {
                picked = Some(msg.clone());
                break;
            }
        }

        if picked.is_some() {
            self.search_text.clear();
            self.search_results.clear();
        }

        picked
    }
}

pub fn no_dbc_placeholder(ui: &mut egui::Ui) {
    ui.vertical_centered(|ui| {
        ui.label("No SuperDBC selected yet.");
        ui.label("CMD+S to toggle the sidebar.");
        ui.label("Use the sidebar to select a SuperDBC JSON file");
    });
}
