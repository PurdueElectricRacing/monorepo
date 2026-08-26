use crate::{app, messages, ui::dbc_msg_picker, util};
use eframe::egui;
use egui_plot::{Line, Plot, PlotPoints};
use std::collections::VecDeque;

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
    window: VecDeque<(f64, f64)>, // (time, value)
    window_duration_seconds: f64,
    decimation_factor: u64,
    decimation_counter: u64,
    reference_time: Option<chrono::DateTime<chrono::Local>>,
    is_paused: bool,
}

impl Scope {
    pub fn new(instance_num: usize, msg_id: u32, msg_name: String, signal_name: String) -> Self {
        let title = format!("Scope: {}", signal_name);
        Self {
            title,
            instance_num,
            state: ScopeState::Configured {
                msg_id,
                msg_name,
                signal_name,
            },
            window: VecDeque::new(),
            window_duration_seconds: 10.0, // Default 10 seconds
            decimation_factor: 0,
            decimation_counter: 0,
            reference_time: None,
            is_paused: false,
        }
    }

    // New constructor, used by sidebar, opens into picker ui
    pub fn new_empty(instance_num: usize) -> Self {
        let title = format!("Scope #{}", instance_num);
        Self {
            title,
            instance_num,
            state: ScopeState::default(),
            window: VecDeque::new(),
            window_duration_seconds: 10.0, // Default 10 seconds
            decimation_factor: 0,
            decimation_counter: 0,
            reference_time: None,
            is_paused: false,
        }
    }

    pub fn add_point(&mut self, timestamp: chrono::DateTime<chrono::Local>, value: f64) {
        if self.is_paused {
            return;
        }

        let next_counter = self.decimation_counter + 1;
        if next_counter < self.decimation_factor {
            self.decimation_counter = next_counter;
            return;
        }
        self.decimation_counter = 0;

        // Initialize reference time on first accepted sample
        let reference = *self.reference_time.get_or_insert(timestamp);

        // Calculate relative time in seconds
        let relative_time = (timestamp - reference).num_milliseconds() as f64 / 1000.0;

        self.window.push_back((relative_time, value));

        // Remove old data outside time window
        let cutoff_time = relative_time - self.window_duration_seconds;
        while let Some((oldest_time, _)) = self.window.front() {
            if *oldest_time < cutoff_time {
                self.window.pop_front();
            } else {
                break;
            }
        }
    }

    fn export_csv(&self) {
        // Create CSV content from the window data
        let mut csv_content = String::from("Time_Seconds,Value\n");
        for (relative_time, value) in &self.window {
            csv_content.push_str(&format!("{},{}\n", relative_time, value));
        }

        // Open file dialog to save CSV
        if let Some(path) = rfd::FileDialog::new()
            .set_file_name(format!("{}_data.csv", self.title.replace(" ", "_")))
            .add_filter("CSV Files", &["csv"])
            .save_file()
        {
            if let Err(e) = std::fs::write(&path, csv_content) {
                log::error!("Failed to save CSV file: {}", e);
            } else {
                log::info!("CSV exported to: {}", path.display());
            }
        }
    }

    // Takes self.state so we can check/update it w/o causing borrower checker issues while ui is being used, returns true if state changes to Configured, so show() knows to reset plot buffer
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
                        util::can::can_dbc_to_u32_without_extid_flag(&selected_msg.id)
                    ))
                    .strong(),
                );

                let msg_id = util::can::can_dbc_to_u32_without_extid_flag(&selected_msg.id);
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
    ) -> egui_tiles::UiResponse {
        // Since no signal is assigned ask the user to pick a signal
        if !matches!(self.state, ScopeState::Configured { .. }) {
            ui.heading(format!("📊 {}: No signal selected", self.title));
            ui.separator();

            let Some(parser) = parser else {
                dbc_msg_picker::no_dbc_placeholder(ui);
                return egui_tiles::UiResponse::None;
            };

            // Clears buffered data and goes back to picker ui so new signal can b chosen
            if self.show_picker(ui, parser) {
                self.window.clear();
                self.reference_time = None;
            }
            return egui_tiles::UiResponse::None;
        }

        let ScopeState::Configured {
            msg_name,
            signal_name,
            ..
        } = &self.state
        else {
            unreachable!("checked above");
        };
        let msg_name = msg_name.clone();
        let signal_name = signal_name.clone();

        // Horizontal container (heading + new Change Signal button)
        let mut signal_changed = false;
        ui.horizontal(|ui| {
            ui.heading(format!("📊 {}: {} - {}", self.title, msg_name, signal_name));
            ui.with_layout(egui::Layout::right_to_left(egui::Align::Center), |ui| {
                if ui.button("🔀 Change Signal").clicked() {
                    self.state = ScopeState::default();
                    // Signal is gone so fall back until a new one is picked
                    self.title = format!("Scope #{}", self.instance_num);
                    self.window.clear();
                    self.reference_time = None;
                    signal_changed = true;
                }
            });
        });

        // If button clicked, stop here for the frame and let next frame handle showing the picker
        if signal_changed {
            return egui_tiles::UiResponse::None;
        }

        // Horizontal container
        ui.horizontal(|ui| {
            // Pause/Resume button
            let pause_text = if self.is_paused {
                "▶ Resume"
            } else {
                "⏸ Pause"
            };
            if ui.button(pause_text).clicked() {
                self.is_paused = !self.is_paused;
            }

            ui.separator();

            // Window duration slider
            ui.label("Window Duration:");
            ui.add(
                egui::Slider::new(&mut self.window_duration_seconds, 1.0..=3000.0)
                    .suffix(" seconds"),
            );

            ui.separator();

            // Decimation factor slider
            ui.label("Decimation:");
            ui.add(egui::Slider::new(&mut self.decimation_factor, 0..=500));

            ui.separator();

            // Export button
            if ui.button("📄 Export CSV").clicked() {
                self.export_csv();
            }

            ui.separator();

            // Clear button
            if ui.button("🗑 Clear").clicked() {
                self.window.clear();
                self.reference_time = None;
            }

            ui.separator();
        });

        ui.separator();

        Plot::new(&self.title)
            .view_aspect(2.0)
            .auto_bounds(egui::Vec2b::TRUE)
            .x_axis_label("Time (seconds)")
            .y_axis_label(&signal_name)
            .show(ui, |plot_ui| {
                if self.window.is_empty() {
                    return;
                }

                let points: PlotPoints = self
                    .window
                    .iter()
                    .map(|(time, value)| [*time, *value])
                    .collect();

                let line = Line::new(&signal_name, points)
                    .color(egui::Color32::from_rgb(100, 200, 100))
                    .stroke(egui::Stroke::new(
                        2.0_f32,
                        egui::Color32::from_rgb(100, 200, 100),
                    ));

                plot_ui.line(line);
            });

        egui_tiles::UiResponse::None
    }

    pub fn handle_can_message(&mut self, msg: &messages::MsgFromCan) {
        // If no signal is assigned, there's nothing to plot
        let ScopeState::Configured {
            msg_id: target_msg_id,
            signal_name,
            ..
        } = &self.state
        else {
            return;
        };

        if let messages::MsgFromCan::ParsedMessage(parsed_msg) = msg {
            if parsed_msg.decoded.msg_id != *target_msg_id {
                return;
            }

            let Some(signal) = parsed_msg.decoded.signals.get(signal_name) else {
                return;
            };

            self.add_point(parsed_msg.timestamp, signal.value.physical);
        }
    }
}
