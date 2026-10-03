use crate::{app, telemetry::TelemetryView, ui::dbc_msg_picker};
use eframe::egui;
use egui_plot::{Line, Plot, PlotPoints};
// Makes invalid combinations of id/name/signal name unrepresentable
enum ScopeState {
    PickingMessage {
        picker: dbc_msg_picker::DbcMsgPickerState,
    },
    PickingSignal {
        selected_msg: can_dbc::Message,
    },
    Configured {
        msg_id: u32,
        msg_name: String,
        signal_name: String,
    },
}

impl Default for ScopeState {
    fn default() -> Self {
        ScopeState::PickingMessage {
            picker: dbc_msg_picker::DbcMsgPickerState::default(),
        }
    }
}

pub struct Scope {
    pub title: String,
    instance_num: usize,
    state: ScopeState,
}
impl Scope {
    pub fn new(instance_num: usize, msg_id: u32, msg_name: String, signal_name: String) -> Self {
        Self {
            title: format!("Scope: {signal_name}"),
            instance_num,
            state: ScopeState::Configured {
                msg_id,
                msg_name,
                signal_name,
            },
        }
    }
    pub fn new_empty(instance_num: usize) -> Self {
        Self {
            title: format!("Scope #{instance_num}"),
            instance_num,
            state: ScopeState::default(),
        }
    }
    fn show_picker(&mut self, ui: &mut egui::Ui, parser: &app::ParserInfo) -> bool {
        let state = std::mem::take(&mut self.state);

        let (new_state, just_configured) = match state {
            ScopeState::PickingMessage { mut picker } => {
                let picked = picker.show(ui, &parser.parser, true);
                match picked {
                    Some(msg) => (ScopeState::PickingSignal { selected_msg: msg }, false),
                    None => (ScopeState::PickingMessage { picker }, false),
                }
            }
            ScopeState::PickingSignal { selected_msg } => {
                ui.separator();
                ui.label(
                    egui::RichText::new(format!(
                        "Selected Message: {} (0x{:03X}) — pick a signal:",
                        selected_msg.name,
                        daqcore::can::can_dbc_to_u32_without_extid_flag(&selected_msg.id)
                    ))
                    .strong(),
                );

                let msg_id = daqcore::can::can_dbc_to_u32_without_extid_flag(&selected_msg.id);
                let mut picked_signal = None;
                for sig in &selected_msg.signals {
                    if ui.button(&sig.name).clicked() {
                        picked_signal = Some(sig.name.clone());
                        break;
                    }
                }

                // When the user picks a message + signal it assigns the target and resets plot buffer
                if let Some(signal_name) = picked_signal {
                    (
                        ScopeState::Configured {
                            msg_id,
                            msg_name: selected_msg.name.clone(),
                            signal_name,
                        },
                        true,
                    )
                } else if ui.button("← Back to message search").clicked() {
                    (
                        ScopeState::PickingMessage {
                            picker: dbc_msg_picker::DbcMsgPickerState::default(),
                        },
                        false,
                    )
                } else {
                    (ScopeState::PickingSignal { selected_msg }, false)
                }
            }
            configured @ ScopeState::Configured { .. } => (configured, false),
        };

        self.state = new_state;

        if just_configured && let ScopeState::Configured { signal_name, .. } = &self.state {
            self.title = format!("Scope: {}", signal_name);
        }

        just_configured
    }

    pub fn show(
        &mut self,
        ui: &mut egui::Ui,
        parser: Option<&app::ParserInfo>,
        view: &TelemetryView<'_>,
    ) -> egui_tiles::UiResponse {
        if !matches!(self.state, ScopeState::Configured { .. }) {
            if let Some(parser) = parser {
                self.show_picker(ui, parser);
            } else {
                dbc_msg_picker::no_dbc_placeholder(ui);
            }
            return egui_tiles::UiResponse::None;
        }
        let ScopeState::Configured {
            msg_id,
            msg_name,
            signal_name,
        } = &self.state
        else {
            unreachable!()
        };
        let signal = signal_name.clone();
        let id = *msg_id;
        let points: Vec<[f64; 2]> = view
            .frames
            .iter()
            .filter(|f| f.msg_id == id)
            .filter_map(|f| {
                Some([
                    f.timestamp.secs(view.timeline.start()),
                    f.decoded.as_ref()?.signals.get(&signal)?.value.physical,
                ])
            })
            .collect();
        let mut change = false;
        ui.horizontal(|ui| {
            ui.heading(format!("{}: {msg_name} - {signal}", self.title));
            change = ui.button("Change signal").clicked();
            if ui.button("Export CSV").clicked() {
                if let Some(path) = rfd::FileDialog::new()
                    .set_file_name(format!("{}_data.csv", signal))
                    .add_filter("CSV", &["csv"])
                    .save_file()
                {
                    let mut text = String::from("Time_Seconds,Value\n");
                    for p in &points {
                        text.push_str(&format!("{},{}\n", p[0], p[1]));
                    }
                    if let Err(e) = std::fs::write(path, text) {
                        log::error!("Export failed: {e}");
                    }
                }
            }
        });
        if change {
            self.state = ScopeState::default();
            self.title = format!("Scope #{}", self.instance_num);
            return egui_tiles::UiResponse::None;
        }
        if points.is_empty() {
            ui.label("No retained samples in the selected interval.");
        }
        let points = crate::telemetry::decimate(&points, ui.available_width().max(1.0) as usize);
        Plot::new(&self.title)
            .view_aspect(2.0)
            .include_x(0.0)
            .include_x(view.timeline.end().secs(view.timeline.start()))
            .x_axis_label(format!("Seconds after {}", view.timeline.start().label()))
            .y_axis_label(&signal)
            .show(ui, |plot| {
                plot.line(Line::new(&signal, PlotPoints::from(points)));
            });
        egui_tiles::UiResponse::None
    }
}
