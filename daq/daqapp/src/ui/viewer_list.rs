use crate::{app, formatter, frozen, messages};
use eframe::egui;
use std::collections::VecDeque;

const MAX_MESSAGES: usize = 200;

#[derive(Clone)]
enum Msg {
    Decoded(messages::ParsedMessage),
    Undecoded(messages::UnparsedMessage),
}
type MsgList = VecDeque<Msg>;

pub struct ViewerList {
    pub title: String,
    msgs: frozen::Frozen<MsgList>,
    paused: bool,
}

impl ViewerList {
    pub fn new(instance_num: usize) -> Self {
        Self {
            title: format!("CAN Viewer Table #{}", instance_num),
            msgs: frozen::Frozen::new(VecDeque::with_capacity(MAX_MESSAGES)),
            paused: false,
        }
    }

    pub fn show(
        &mut self,
        ui: &mut egui::Ui,
        formatter: &Option<formatter::Formatter>,
        parser: Option<&app::ParserInfo>,
    ) -> egui_tiles::UiResponse {
        ui.heading(format!("🚗 {}", self.title));

        ui.horizontal(|ui| {
            if ui
                .button(if self.paused { "Resume" } else { "Pause" })
                .clicked()
            {
                self.paused = !self.paused;
                if self.paused {
                    self.msgs.freeze();
                } else {
                    self.msgs.unfreeze();
                }
            }

            if ui.button("Clear").clicked() {
                self.msgs.apply_both(|ms| ms.clear());
            }
        });

        ui.separator();

        egui_extras::TableBuilder::new(ui)
            .striped(true)
            .column(egui_extras::Column::auto().at_least(100.0).resizable(true)) // Timestamp
            .column(egui_extras::Column::auto().at_least(300.0).resizable(true)) // Msg (ID)
            .column(egui_extras::Column::auto().at_least(200.0).resizable(true)) // Signal
            .column(egui_extras::Column::remainder().resizable(true)) // Decoded Signal
            .header(20.0, |mut header| {
                header.col(|ui| {
                    ui.label("Timestamp");
                });
                header.col(|ui| {
                    ui.label("Msg (ID)");
                });
                header.col(|ui| {
                    ui.label("Signal");
                });
                header.col(|ui| {
                    ui.label("Decoded Content");
                });
            })
            .body(|mut body| {
                for msg in self.msgs.get().iter().rev() {
                    match msg {
                        Msg::Decoded(decoded_msg) => {
                            let Some(view) =
                                parser.and_then(|p| decoded_msg.decoded(p.parser.database()))
                            else {
                                continue;
                            };
                            for (sig_name, signal) in view.signals.iter() {
                                body.row(18.0, |mut row| {
                                    row.col(|ui| {
                                        ui.label(
                                            decoded_msg
                                                .timestamp
                                                .format("%H:%M:%S:%3f")
                                                .to_string(),
                                        );
                                    });
                                    row.col(|ui| {
                                        ui.label(format!(
                                            "{} (0x{:X}, bus {}{})",
                                            view.name,
                                            decoded_msg.frame.id.raw(),
                                            decoded_msg.frame.bus.raw(),
                                            if decoded_msg.frame.id.is_extended() {
                                                ", extended"
                                            } else {
                                                ""
                                            }
                                        ));
                                    });
                                    row.col(|ui| {
                                        ui.label(sig_name.to_string());
                                    });
                                    row.col(|ui| {
                                        let sig_def = Some(signal.definition);
                                        {
                                            ui.label(formatter::try_format(
                                                formatter,
                                                &view.name,
                                                sig_name,
                                                sig_def,
                                                Some(&signal.unit),
                                                &signal.value,
                                            ));
                                        }
                                    });
                                });
                            }
                        }
                        Msg::Undecoded(unparsed_msg) => {
                            body.row(18.0, |mut row| {
                                row.col(|ui| {
                                    ui.label(
                                        unparsed_msg.timestamp.format("%H:%M:%S:%3f").to_string(),
                                    );
                                });
                                row.col(|ui| {
                                    ui.label(format!(
                                        "0x{:X}, bus {}{}",
                                        unparsed_msg.frame.id.raw(),
                                        unparsed_msg
                                            .frame
                                            .bus
                                            .map(|bus| bus.raw().to_string())
                                            .unwrap_or_else(|| "?".into()),
                                        if unparsed_msg.frame.id.is_extended() {
                                            ", extended"
                                        } else {
                                            ""
                                        }
                                    ));
                                });
                                row.col(|ui| {
                                    ui.label("(Error: Unknown)");
                                });
                                row.col(|ui| {
                                    let hex_bytes = unparsed_msg
                                        .frame
                                        .data()
                                        .iter()
                                        .map(|b| format!("{:02X}", b))
                                        .collect::<Vec<_>>()
                                        .join(" ");
                                    ui.label(hex_bytes);
                                });
                            });
                        }
                    }
                }
            });

        ui.scroll_to_cursor(Some(egui::Align::Min));

        egui_tiles::UiResponse::None
    }

    fn make_space_for_new_message(&mut self) {
        let msgs = self.msgs.get_mut();
        while msgs.len() >= MAX_MESSAGES - 1 {
            msgs.pop_front();
        }
    }

    pub fn handle_can_message(&mut self, msg: &messages::MsgFromCan) {
        match msg {
            messages::MsgFromCan::ParsedMessage(parsed_msg) => {
                self.make_space_for_new_message();
                self.msgs
                    .get_mut()
                    .push_back(Msg::Decoded(parsed_msg.clone()));
            }
            messages::MsgFromCan::UnparsedMessage(unparsed_msg) => {
                self.make_space_for_new_message();
                self.msgs
                    .get_mut()
                    .push_back(Msg::Undecoded(unparsed_msg.clone()));
            }
            _ => {}
        }
    }
}
