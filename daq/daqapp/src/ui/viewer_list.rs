use crate::{app, telemetry};

pub struct ViewerList {
    pub title: String,
}

impl ViewerList {
    pub fn new(instance: usize) -> Self {
        Self {
            title: format!("CAN Viewer List #{instance}"),
        }
    }

    pub fn show(
        &self,
        ui: &mut eframe::egui::Ui,
        formatter: &Option<daqcore::formatter::Formatter>,
        parser: Option<&app::ParserInfo>,
        view: &telemetry::TelemetryView<'_>,
    ) -> egui_tiles::UiResponse {
        ui.heading(&self.title);
        if view.frames.is_empty() {
            ui.label("No retained CAN messages in the selected interval.");
        }
        egui_extras::TableBuilder::new(ui)
            .striped(true)
            .column(egui_extras::Column::auto().at_least(150.0).resizable(true))
            .column(egui_extras::Column::auto().at_least(300.0).resizable(true))
            .column(egui_extras::Column::auto().at_least(250.0).resizable(true))
            .column(egui_extras::Column::remainder().resizable(true))
            .header(20.0, |mut header| {
                for text in ["Timestamp", "Message (ID)", "Signal", "Value"] {
                    header.col(|ui| {
                        ui.label(text);
                    });
                }
            })
            .body(|mut body| {
                for frame in view.frames.iter().rev().take(200) {
                    if let Some(decoded) = &frame.decoded {
                        let id = frame.identity().dbc_id();
                        let def = parser.and_then(|p| p.parser.msg_def(id));
                        for (name, sig) in &decoded.signals {
                            let signal_definition = def.and_then(|message| {
                                message.signals.iter().find(|s| s.name == *name)
                            });

                            let value = daqcore::formatter::try_format(
                                formatter,
                                &decoded.name,
                                name,
                                signal_definition,
                                Some(&sig.unit),
                                &sig.value,
                            );
                            body.row(18.0, |mut row| {
                                row.col(|ui| {
                                    ui.label(frame.timestamp.label());
                                });
                                row.col(|ui| {
                                    ui.label(format!("{} (0x{:X})", decoded.name, frame.msg_id));
                                });
                                row.col(|ui| {
                                    ui.label(name);
                                });
                                row.col(|ui| {
                                    ui.label(value);
                                });
                            });
                        }
                    } else {
                        body.row(18.0, |mut row| {
                            row.col(|ui| {
                                ui.label(frame.timestamp.label());
                            });
                            row.col(|ui| {
                                ui.label(format!("0x{:X}", frame.msg_id));
                            });
                            row.col(|ui| {
                                ui.label("Unknown");
                            });
                            row.col(|ui| {
                                ui.monospace(format!("{:02X?}", frame.raw_bytes));
                            });
                        });
                    }
                }
            });
        egui_tiles::UiResponse::None
    }
}
