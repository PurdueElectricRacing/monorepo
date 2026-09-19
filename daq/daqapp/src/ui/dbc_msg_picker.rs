use crate::util;
use eframe::egui;

/// Shared DBC message search UI state used by Send UI, Jitter, etc.
#[derive(Default)]
pub struct DbcMsgPickerState {
    search_text: String,
    search_results: Vec<can_dbc::Message>,
}

impl DbcMsgPickerState {
    /// Refresh [`Self::search_results`] from [`Self::search_text`] using the same rules as before:
    /// empty clears results, `*` lists all messages, otherwise filter by name and hex ID substring.
    pub fn refresh_results(&mut self, parser: &can_decode::Parser) {
        if self.search_text.is_empty() {
            self.search_results.clear();
        } else if self.search_text.trim() == "*" {
            self.search_results = parser.msg_defs().clone();
        } else {
            let search_lower = self.search_text.to_lowercase();
            self.search_results = parser
                .msg_defs()
                .iter()
                .filter(|msg| {
                    let id_str = format!(
                        "0x{:03X}",
                        util::can::can_dbc_to_u32_without_extid_flag(&msg.id)
                    );
                    id_str.contains(&search_lower)
                        || msg.name.to_lowercase().contains(&search_lower)
                })
                .cloned()
                .collect();
        }
    }

    /// Search field, hints, and result buttons. Returns [`Some`] when the user picked a message
    /// (search buffer is cleared on pick).
    pub fn show(
        &mut self,
        ui: &mut egui::Ui,
        parser: &can_decode::Parser,
        selected_msg_is_none: bool,
    ) -> Option<can_dbc::Message> {
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
        for msg in &self.search_results {
            if ui
                .button(format!(
                    "{} (0x{:03X})",
                    msg.name,
                    util::can::can_dbc_to_u32_without_extid_flag(&msg.id)
                ))
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
        ui.label("No DBC selected yet.");
        ui.label("CMD+S to toggle the sidebar.");
        ui.label("Use the sidebar to select a DBC file");
    });
}
