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
                Self::show_expects_grid(ui, test);
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

    fn show_expects_grid(ui: &mut eframe::egui::Ui, test: &daqcore::hil::run::HilRunningTest) {
        for (index, ipe) in test.in_progress_expects.iter().enumerate() {
            ui.push_id(("expectation", index), |ui| {
                eframe::egui::Grid::new("expectation_summary")
                    .num_columns(4)
                    .striped(true)
                    .show(ui, |ui| {
                        ui.strong("Message");
                        ui.strong("Window (ms)");
                        ui.strong("Status");
                        ui.strong("Signals");
                        ui.end_row();
                        Self::show_expect_row(ui, ipe);
                    });
                let count = ipe.messages.len() - usize::from(ipe.selected_message_index.is_some());
                eframe::egui::CollapsingHeader::new(format!("Other matching messages ({count})"))
                    .id_salt("message_history")
                    .show(ui, |ui| {
                        if count == 0 {
                            ui.label("No other matching messages received.");
                            return;
                        }
                        let line_height = ui.text_style_height(&eframe::egui::TextStyle::Body);
                        let row_height = (line_height + ui.spacing().item_spacing.y)
                            * (ipe.expect.signals.len() + 1) as f32;
                        eframe::egui::ScrollArea::vertical()
                            .id_salt("history_scroll")
                            .max_height(240.0)
                            .show_rows(ui, row_height, count, |ui, rows| {
                                for row in rows {
                                    // Map the visible history row past the selected message
                                    // without filtering or copying the full history each frame.
                                    let index = row
                                        + usize::from(
                                            ipe.selected_message_index.is_some_and(|i| row >= i),
                                        );
                                    ui.push_id(index, |ui| {
                                        ui.allocate_ui_with_layout(
                                            eframe::egui::vec2(ui.available_width(), row_height),
                                            eframe::egui::Layout::top_down(
                                                eframe::egui::Align::Min,
                                            ),
                                            |ui| {
                                                Self::show_captured_message(
                                                    ui,
                                                    ipe,
                                                    &ipe.messages[index],
                                                )
                                            },
                                        );
                                    });
                                }
                            });
                    });
                ui.add_space(4.0);
            });
        }
    }

    fn show_expect_row(ui: &mut eframe::egui::Ui, ipe: &daqcore::hil::run::InProgressExpect) {
        ui.label(&ipe.expect.msg_name);
        ui.label(format!(
            "{:.0} - {:.0}",
            ipe.expect.window[0], ipe.expect.window[1]
        ));
        ui.colored_label(expect_color(ipe.result), ipe.result.as_str());
        ui.vertical(|ui| {
            if let Some(message) = ipe.selected_message() {
                Self::show_captured_message(ui, ipe, message);
            } else {
                ui.label("No matching messages received.");
                for (name, range) in &ipe.expect.signals {
                    ui.label(format!("{name}: expected [{}, {}]", range[0], range[1]));
                }
            }
        });
        ui.end_row();
    }

    fn show_captured_message(
        ui: &mut eframe::egui::Ui,
        ipe: &daqcore::hil::run::InProgressExpect,
        message: &daqcore::hil::run::CapturedMessage,
    ) {
        let color = if message.is_valid() {
            eframe::egui::Color32::GREEN
        } else {
            eframe::egui::Color32::RED
        };
        ui.colored_label(
            color,
            format!(
                "{} ms — {}",
                message.elapsed_ms,
                if message.is_valid() {
                    "Valid"
                } else {
                    "Invalid"
                }
            ),
        );
        for (name, range) in &ipe.expect.signals {
            let failed = message.failures.iter().any(|f| f.name() == name);
            let value = message
                .signals
                .get(name)
                .copied()
                .flatten()
                .map(|v| v.to_string())
                .unwrap_or_else(|| "missing".to_owned());
            let detail = format!(
                "{name}: {value}; expected [{}, {}]{}",
                range[0],
                range[1],
                if failed { " (invalid)" } else { "" }
            );
            let text = eframe::egui::RichText::new(&detail).color(if failed {
                eframe::egui::Color32::RED
            } else {
                ui.visuals().text_color()
            });
            ui.add(eframe::egui::Label::new(text).truncate())
                .on_hover_text(detail);
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
