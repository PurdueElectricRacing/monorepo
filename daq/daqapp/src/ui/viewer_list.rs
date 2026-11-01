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
        database: Option<&app::DatabaseSelection>,
        view: &telemetry::TelemetryView<'_>,
    ) -> egui_tiles::UiResponse {
        ui.heading(&self.title);
        if view.setpoint_frames.is_empty() {
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
                for frame in view.setpoint_frames.iter().rev().take(200) {
                    if let Some(decoded) = &frame.decoded {
                        let key = decoded.key();
                        let def =
                            database.and_then(|selection| selection.database.message_by_key(key));
                        for (name, sig) in decoded.iter() {
                            let signal_definition = def.and_then(|message| message.signal(name));

                            let value = daqcore::formatter::try_format(
                                formatter,
                                decoded.name(),
                                name,
                                signal_definition,
                                None,
                                sig,
                            );
                            body.row(18.0, |mut row| {
                                row.col(|ui| {
                                    ui.label(frame.timestamp.label());
                                });
                                row.col(|ui| {
                                    ui.label(format!(
                                        "{} (0x{:X})",
                                        decoded.name(),
                                        frame.identity
                                    ));
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
                                ui.label(format!("0x{:X}", frame.identity));
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
