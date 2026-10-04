use crate::{app, telemetry, ui::dbc_msg_picker};

// Makes invalid combinations of id/name/signal name unrepresentable
enum ScopeState {
    PickingMessage {
        picker: dbc_msg_picker::DbcMsgPickerState,
    },
    PickingSignal {
        selected_msg: can_dbc::Message,
    },
    Configured {
        identity: daqcore::frame::CanIdentity,
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
    pub fn new(
        instance_num: usize,
        identity: daqcore::frame::CanIdentity,
        msg_name: String,
        signal_name: String,
    ) -> Self {
        Self {
            title: format!("Scope: {signal_name}"),
            instance_num,
            state: ScopeState::Configured {
                identity,
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

    fn show_picker(&mut self, ui: &mut eframe::egui::Ui, parser: &app::ParserInfo) -> bool {
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
                    eframe::egui::RichText::new(format!(
                        "Selected Message: {} ({}) — pick a signal:",
                        selected_msg.name,
                        daqcore::can::can_dbc_identity(&selected_msg.id)
                    ))
                    .strong(),
                );

                let identity = daqcore::can::can_dbc_identity(&selected_msg.id);
                let mut picked_signal = None;
                for sig in &selected_msg.signals {
                    if ui.button(&sig.name).clicked() {
                        picked_signal = Some(sig.name.clone());
                        break;
                    }
                }

                // Choosing a signal configures the query against shared history.
                if let Some(signal_name) = picked_signal {
                    (
                        ScopeState::Configured {
                            identity,
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
        ui: &mut eframe::egui::Ui,
        parser: Option<&app::ParserInfo>,
        view: &telemetry::TelemetryView<'_>,
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
            identity,
            msg_name,
            signal_name,
        } = &self.state
        else {
            return egui_tiles::UiResponse::None;
        };

        let signal = signal_name.clone();
        let id = *identity;
        let start = view.timeline.start();
        let duration = view.timeline.end().secs(start).max(0.001);
        let playhead = view.timeline.setpoint().secs(start);

        let points: Vec<[f64; 2]> = scope_frames(view.plot_frames, id)
            .filter_map(|frame| {
                let decoded = frame.decoded.as_ref()?;
                let value = decoded.signals.get(&signal)?.value.physical;

                Some([frame.timestamp.secs(start), value])
            })
            .collect();
        let mut change = false;
        ui.horizontal(|ui| {
            ui.heading(format!(
                "{}: {msg_name} ({identity}) - {signal}",
                self.title
            ));
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

        let pixels = ui.available_width().max(1.0) as usize;
        let points = telemetry::decimate(&points, pixels);

        egui_plot::Plot::new(&self.title)
            .view_aspect(2.0)
            .allow_zoom([false, true])
            .allow_drag([false, true])
            .allow_scroll([false, true])
            .x_axis_formatter(|mark, _| {
                let offset_ms = (mark.value * 1000.0).round() as i64;
                start.offset(offset_ms).label()
            })
            .x_axis_label("Time")
            .y_axis_label(&signal)
            .show(ui, |plot| {
                plot.set_plot_bounds_x(0.0..=duration);
                plot.line(egui_plot::Line::new(
                    &signal,
                    egui_plot::PlotPoints::from(points),
                ));
                plot.vline(
                    egui_plot::VLine::new("Playhead", playhead)
                        .color(eframe::egui::Color32::YELLOW),
                );
            });
        egui_tiles::UiResponse::None
    }
}

fn scope_frames(
    frames: &[daqcore::ParsedFrame],
    identity: daqcore::frame::CanIdentity,
) -> impl Iterator<Item = &daqcore::ParsedFrame> {
    frames
        .iter()
        .filter(move |frame| frame.identity() == identity)
}
