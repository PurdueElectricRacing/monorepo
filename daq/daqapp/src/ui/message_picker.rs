/// Shared search state. Definitions stay borrowed from the selected database.
#[derive(Default)]
pub struct MessagePickerState {
    search_text: String,
}

impl MessagePickerState {
    pub fn show(
        &mut self,
        ui: &mut eframe::egui::Ui,
        bus: &daqcore::superdbc::database::Bus,
        selected_msg_is_none: bool,
    ) -> Option<daqcore::superdbc::message::MessageKey> {
        ui.horizontal(|ui| {
            ui.label("Search:");
            ui.add(
                eframe::egui::TextEdit::singleline(&mut self.search_text)
                    .hint_text("Message name or CAN ID…"),
            );
        });
        ui.add_space(8.0);
        if self.search_text.is_empty() {
            if selected_msg_is_none {
                ui.label(
                    eframe::egui::RichText::new(
                        "Start typing to search for messages… (Use * to show all messages.)",
                    )
                    .italics()
                    .weak(),
                );
            }
            return None;
        }
        let search = self.search_text.to_lowercase();
        let mut found = false;
        let mut picked = None;
        for message in bus.messages().iter().filter(|message| {
            search.trim() == "*"
                || message.name().to_lowercase().contains(&search)
                || format!("0x{:03x}", message.identity()).contains(&search)
        }) {
            found = true;
            if ui
                .button(format!("{} (0x{:03X})", message.name(), message.identity()))
                .clicked()
            {
                picked = Some(message.key());
            }
        }
        if !found {
            ui.label(
                eframe::egui::RichText::new("No messages found.")
                    .italics()
                    .weak(),
            );
        }
        if picked.is_some() {
            self.search_text.clear();
        }
        picked
    }
}

pub fn no_database_placeholder(ui: &mut eframe::egui::Ui) {
    ui.vertical_centered(|ui| {
        ui.label("No active SuperDBC selection.");
        ui.label("CMD+S to toggle the sidebar.");
        ui.label("Use the sidebar to select a SuperDBC file and CAN bus.");
    });
}
