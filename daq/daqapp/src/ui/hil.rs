use std::{path, sync::mpsc};

pub struct Hil {
    pub title: String,
    pub found_presets: Vec<daqcore::hil::config::PresetInfo>,
    pub found_tests: Vec<daqcore::hil::config::TestInfo>,
    pub load_errors: Vec<String>,
    pub snapshot: daqcore::hil::engine::HilSnapshot,
    ui_to_can_tx: mpsc::Sender<daqcore::can_thread::CanThreadCommand>,
}

impl Hil {
    pub fn new(ui_to_can_tx: mpsc::Sender<daqcore::can_thread::CanThreadCommand>) -> Self {
        let (presets, tests, errors) =
            daqcore::hil::config::list_available_tests(path::Path::new("hil_config"));

        Self {
            title: "HIL".to_string(),
            found_presets: presets,
            found_tests: tests,
            load_errors: errors,
            snapshot: daqcore::hil::engine::HilSnapshot::idle(),
            ui_to_can_tx,
        }
    }

    pub fn handle_can_message(&mut self, msg: &daqcore::can_thread::CanThreadEvent) {
        if let daqcore::can_thread::CanThreadEvent::Hil(snapshot) = msg {
            self.snapshot = snapshot.clone();
        }
    }

    fn send_command(&self, command: daqcore::hil::engine::HilCommand) {
        if let Err(error) = self
            .ui_to_can_tx
            .send(daqcore::can_thread::CanThreadCommand::Hil(command))
        {
            log::error!("Failed to submit HIL command: {error}");
        }
    }

    fn reload_tests(&mut self) {
        let (presets, tests, errors) =
            daqcore::hil::config::list_available_tests(path::Path::new("hil_config"));
        self.found_presets = presets;
        self.found_tests = tests;
        self.load_errors = errors;
        self.snapshot.start_error = None;
    }

    pub fn show(&mut self, ui: &mut eframe::egui::Ui) -> egui_tiles::UiResponse {
        eframe::egui::ScrollArea::vertical().show(ui, |ui| match self.snapshot.status {
            daqcore::hil::engine::HilStatus::Idle => {
                self.show_idle(ui);
            }
            daqcore::hil::engine::HilStatus::Running => {
                self.show_running(ui);
            }
        });

        egui_tiles::UiResponse::None
    }

    fn show_idle(&mut self, ui: &mut eframe::egui::Ui) {
        ui.label("HIL is idle. Select a preset or test to start.");
        if ui.button("Reload Tests").clicked() {
            self.reload_tests();
        }
        ui.separator();

        if !self.load_errors.is_empty() {
            ui.label("Errors loading tests:");
            for error in &self.load_errors {
                ui.label(format!("- {}", error));
            }
            ui.separator();
        }

        if let Some(error) = &self.snapshot.start_error {
            ui.colored_label(eframe::egui::Color32::RED, error);
            ui.separator();
        }

        if !self.found_presets.is_empty() {
            ui.label(" Presets:");
            let mut selected_preset = None;
            for preset in &self.found_presets {
                let preset_info = format!("{} - {}", preset.name, preset.tests.join(", "));
                if ui.button(&preset_info).clicked() {
                    selected_preset = Some(preset.clone());
                }
            }

            if let Some(preset) = selected_preset {
                self.send_command(daqcore::hil::engine::HilCommand::StartPreset(preset));
            }
            ui.separator();
        }

        if !self.found_tests.is_empty() {
            ui.label(" Individual Tests:");
            let mut selected_test = None;
            for test in &self.found_tests {
                let test_info = format!("{} [{}]: {}", test.name, test.basename, test.description);
                if ui.button(&test_info).clicked() {
                    selected_test = Some(test.clone());
                }
            }

            if let Some(test) = selected_test {
                self.send_command(daqcore::hil::engine::HilCommand::StartTest(test));
            }
        }
    }

    fn show_running(&mut self, ui: &mut eframe::egui::Ui) {
        let tests = &self.snapshot.tests;
        let all_finished = self.snapshot.finished;
        let mut stop_requested = false;

        ui.add_space(4.0);
        if self.snapshot.capture_complete {
            ui.horizontal(|ui| {
                if ui.button("Exit").clicked() {
                    stop_requested = true;
                }
                if self.snapshot.skipped_from.is_some() {
                    ui.label("HIL stopped after a failed test. Review results below.");
                } else {
                    ui.label("HIL finished. Review results below.");
                }
            });
        } else {
            ui.horizontal(|ui| {
                if ui.button("Stop").clicked() {
                    stop_requested = true;
                }
                if all_finished {
                    // Although all tests have resolved, inform the user that
                    // the HIL engine is still capturing messages for debugging
                    // purposes.
                    let end = tests
                        .iter()
                        .flat_map(|t| &t.in_progress_expects)
                        .map(|e| e.expect.window[1] as u128)
                        .max()
                        .unwrap_or(0);
                    ui.label(format!(
                        "Results decided; collecting messages until {end} ms ({} ms elapsed)",
                        self.snapshot.elapsed_ms
                    ));
                } else {
                    ui.label(format!("HIL running for {} ms", self.snapshot.elapsed_ms));
                }
            });
        }
        ui.separator();

        if let Some(preset) = &self.snapshot.preset {
            ui.label(format!(
                "Preset: {} ({} tests)",
                preset.name,
                preset.tests.len()
            ));
            if all_finished {
                let executed_count = self.snapshot.skipped_from.unwrap_or(tests.len());

                Self::show_preset_summary(ui, &tests[..executed_count]);

                let skipped_count = tests.len() - executed_count;
                ui.label(format!(
                    "{} tests executed; {} skipped",
                    executed_count, skipped_count
                ));
            }
        }
        ui.separator();

        for (index, test) in tests.iter().enumerate() {
            let skipped = match self.snapshot.skipped_from {
                Some(first_skipped) => index >= first_skipped,
                None => false,
            };

            if skipped {
                ui.colored_label(
                    eframe::egui::Color32::GRAY,
                    format!(
                        "{} — skipped after an earlier test failed",
                        test.test_info.name
                    ),
                );
            } else {
                ui.push_id(("hil_test", index), |ui| Self::show_test(ui, test));
            }

            ui.separator();
        }

        if stop_requested {
            self.send_command(daqcore::hil::engine::HilCommand::Stop);
        }
    }

    fn show_preset_summary(ui: &mut eframe::egui::Ui, tests: &[daqcore::hil::run::HilRunningTest]) {
        if tests.is_empty() || !tests.iter().all(|t| t.is_finished()) {
            return;
        }

        let (mut total_passed, mut total_expects, mut subtests_all_passed) = (0, 0, 0);
        for t in tests {
            let total = t.in_progress_expects.len();
            let passed = t
                .in_progress_expects
                .iter()
                .filter(|e| matches!(e.result, daqcore::hil::run::ExpectResult::Passed))
                .count();
            total_passed += passed;
            total_expects += total;
            if passed == total {
                subtests_all_passed += 1;
            }
        }

        let color = if total_passed == total_expects {
            eframe::egui::Color32::GREEN
        } else {
            eframe::egui::Color32::RED
        };

        ui.colored_label(
            color,
            format!(
                "Executed tests: {}/{} expects passed, {}/{} subtests fully passed",
                total_passed,
                total_expects,
                subtests_all_passed,
                tests.len()
            ),
        );
    }

    fn show_test(ui: &mut eframe::egui::Ui, test: &daqcore::hil::run::HilRunningTest) {
        eframe::egui::CollapsingHeader::new(&test.test_info.name)
            .id_salt(&test.test_info.basename)
            .default_open(true)
            .show(ui, |ui| {
                ui.label(&test.test_info.description);

                Self::show_test_progress(ui, test);

                ui.label(format!("TX remaining: {}", test.tx_remaining.len()));

                Self::show_test_finished_summary(ui, test);

                ui.add_space(4.0);
                Self::show_expects_table(ui, test);
            });
    }

    fn show_test_progress(ui: &mut eframe::egui::Ui, test: &daqcore::hil::run::HilRunningTest) {
        let (not_in_window, in_progress, completed) = test.expect_counts();
        let total = not_in_window + in_progress + completed;
        if total > 0 {
            let frac = completed as f32 / total as f32;
            ui.add(
                eframe::egui::ProgressBar::new(frac)
                    .text(format!("{}/{} complete", completed, total)),
            );
        }
    }

    fn show_test_finished_summary(
        ui: &mut eframe::egui::Ui,
        test: &daqcore::hil::run::HilRunningTest,
    ) {
        if !test.is_finished() {
            return;
        }

        let total = test.in_progress_expects.len();
        let passed = test
            .in_progress_expects
            .iter()
            .filter(|e| matches!(e.result, daqcore::hil::run::ExpectResult::Passed))
            .count();

        let color = if passed == total {
            eframe::egui::Color32::GREEN
        } else {
            eframe::egui::Color32::RED
        };

        ui.colored_label(
            color,
            format!("Test finished: {} passed of {} expects", passed, total),
        );
    }

    fn show_expects_table(ui: &mut eframe::egui::Ui, test: &daqcore::hil::run::HilRunningTest) {
        // Only row addresses are built here; message contents are drawn lazily
        // by the table for the rows currently in view.
        let mut rows = Vec::new();
        let mut expanded = Vec::new();
        for (index, ipe) in test.in_progress_expects.iter().enumerate() {
            let id = ui.id().with(("hil_history", index));
            let other_count = ipe
                .messages
                .len()
                .saturating_sub(usize::from(ipe.selected_message_index.is_some()));
            let open = other_count > 0
                && ui
                    .ctx()
                    .data_mut(|data| data.get_temp::<bool>(id).unwrap_or(false));
            expanded.push((id, open));
            rows.push((index, None));
            if open {
                rows.extend(
                    (0..ipe.messages.len())
                        .filter(|message| Some(*message) != ipe.selected_message_index)
                        .map(|message| (index, Some(message))),
                );
            }
        }
        let line_height =
            ui.text_style_height(&eframe::egui::TextStyle::Body) + ui.spacing().item_spacing.y;
        let heights = rows.iter().map(|(index, message)| {
            let signals = test.in_progress_expects[*index].expect.signals.len().max(1);
            let lines = if message.is_none() {
                // Ensures that each row is always at least 3 lines tall.
                (signals * 2).max(3)
            } else {
                signals
            };
            ((lines as f32) * line_height) + 8.0
        });
        egui_extras::TableBuilder::new(ui)
            .id_salt("hil_messages")
            .cell_layout(eframe::egui::Layout::top_down(eframe::egui::Align::Min))
            // Message
            .column(
                egui_extras::Column::initial(210.0)
                    .at_least(120.0)
                    .resizable(true)
                    .clip(true),
            )
            // Time (ms)
            .column(
                egui_extras::Column::initial(105.0)
                    .at_least(75.0)
                    .resizable(true)
                    .clip(true),
            )
            // Validation
            .column(
                egui_extras::Column::initial(155.0)
                    .at_least(100.0)
                    .resizable(true)
                    .clip(true),
            )
            // Signals
            .column(egui_extras::Column::remainder().at_least(150.0).clip(true))
            .max_scroll_height(400.0)
            .auto_shrink([false, true])
            .header(24.0, |mut header| {
                for label in ["Message", "Time (ms)", "Validation", "Signals"] {
                    header.col(|ui| {
                        ui.strong(label);
                    });
                }
            })
            .body(|body| {
                body.heterogeneous_rows(heights, |mut row| {
                    let (expect_index, message_index) = rows[row.index()];
                    let ipe = &test.in_progress_expects[expect_index];
                    let primary = message_index.is_none();
                    let message = message_index
                        .map(|i| &ipe.messages[i])
                        .or_else(|| ipe.selected_message());
                    let tint = if primary {
                        // Blue
                        eframe::egui::Color32::from_rgba_unmultiplied(80, 140, 220, 28)
                    } else if message.is_some_and(|m| m.is_valid()) {
                        // Green
                        eframe::egui::Color32::from_rgba_unmultiplied(60, 180, 120, 24)
                    } else {
                        // Red
                        eframe::egui::Color32::from_rgba_unmultiplied(220, 80, 90, 24)
                    };
                    row.col(|ui| {
                        Self::paint_message_cell(ui, tint);
                        if primary {
                            let count = ipe
                                .messages
                                .len()
                                .saturating_sub(usize::from(ipe.selected_message_index.is_some()));
                            let (id, open) = expanded[expect_index];
                            let arrow = if open { "v" } else { ">" };
                            let response = ui.add_enabled(
                                count > 0,
                                eframe::egui::Button::new(format!(
                                    "{arrow} {}",
                                    ipe.expect.msg_name
                                ))
                                .frame(false),
                            );
                            if response.clicked() {
                                ui.ctx().data_mut(|data| data.insert_temp(id, !open));
                            }
                            response
                                .on_hover_text(format!("Show or hide {count} other receptions"));
                            ui.weak(format!(
                                "Window: {:.2} - {:.2} ms",
                                ipe.expect.window[0], ipe.expect.window[1]
                            ));
                            ui.weak(format!("{} received", ipe.messages.len()));
                        }
                    });
                    row.col(|ui| {
                        Self::paint_message_cell(ui, tint);
                        if let Some(message) = message {
                            ui.monospace(message.elapsed_ms.to_string());
                        } else {
                            ui.weak("-");
                        }
                    });
                    row.col(|ui| {
                        Self::paint_message_cell(ui, tint);
                        if primary {
                            ui.colored_label(expect_color(ipe.result), ipe.result.as_str());
                        }
                        if let Some(message) = message {
                            let (color, label) = if message.is_valid() {
                                (eframe::egui::Color32::GREEN, "Valid")
                            } else {
                                (eframe::egui::Color32::RED, "Invalid")
                            };
                            ui.colored_label(color, label);
                        } else {
                            ui.weak("No message received");
                        }
                    });
                    row.col(|ui| {
                        Self::paint_message_cell(ui, tint);
                        Self::show_message_signals(ui, ipe, message, primary);
                    });
                });
            });
    }

    fn paint_message_cell(ui: &mut eframe::egui::Ui, tint: eframe::egui::Color32) {
        // Extend into the cell spacing so each reception has a continuous tint.
        let rect = ui.max_rect().expand2(ui.spacing().item_spacing * 0.5);
        ui.painter().rect_filled(rect, 0.0, tint);
        ui.add_space(4.0);
    }

    fn show_message_signals(
        ui: &mut eframe::egui::Ui,
        ipe: &daqcore::hil::run::InProgressExpect,
        message: Option<&daqcore::hil::run::CapturedMessage>,
        show_expected: bool,
    ) {
        if ipe.expect.signals.is_empty() {
            ui.weak("Message presence only");
        }
        for (name, range) in &ipe.expect.signals {
            if let Some(message) = message {
                let raw_value = message.signals.get(name).copied().flatten();
                let failed = !daqcore::hil::run::signal_in_range(raw_value, *range);
                let value = raw_value
                    .map(|value| format!("{value:.2}"))
                    .unwrap_or_else(|| "missing".to_owned());
                let detail = format!("{name}: {value} | {}", if failed { "Fail" } else { "Pass" });
                let color = if failed {
                    eframe::egui::Color32::RED
                } else {
                    eframe::egui::Color32::GREEN
                };
                ui.add(
                    eframe::egui::Label::new(eframe::egui::RichText::new(&detail).color(color))
                        .truncate(),
                )
                .on_hover_text(detail);
            } else {
                ui.label(name);
            }
            if show_expected {
                ui.add(
                    eframe::egui::Label::new(
                        eframe::egui::RichText::new(format!(
                            "Expected: [{:.2}, {:.2}]",
                            range[0], range[1]
                        ))
                        .weak(),
                    )
                    .truncate(),
                );
            }
        }
    }
}

impl Drop for Hil {
    fn drop(&mut self) {
        if let Err(e) = self
            .ui_to_can_tx
            .send(daqcore::can_thread::CanThreadCommand::Hil(
                daqcore::hil::engine::HilCommand::Stop,
            ))
        {
            log::error!("Failed to send HIL stop command on drop: {}", e);
        }
    }
}

fn expect_color(result: daqcore::hil::run::ExpectResult) -> eframe::egui::Color32 {
    match result {
        daqcore::hil::run::ExpectResult::NotInWindow => eframe::egui::Color32::GRAY,
        daqcore::hil::run::ExpectResult::InProgress => eframe::egui::Color32::YELLOW,
        daqcore::hil::run::ExpectResult::Passed => eframe::egui::Color32::GREEN,
        daqcore::hil::run::ExpectResult::FailedNoMessage
        | daqcore::hil::run::ExpectResult::FailedValueOutOfRange => eframe::egui::Color32::RED,
    }
}
