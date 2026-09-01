use crate::{hil, messages};
use eframe::egui;

pub struct Hil {
    pub title: String,
    pub found_presets: Vec<hil::config::PresetInfo>,
    pub found_tests: Vec<hil::config::TestInfo>,
    pub load_errors: Vec<String>,
    pub start_error: Option<String>,
    pub hil_state: hil::run::HilState,
}

impl Hil {
    pub fn new(instance_num: usize) -> Self {
        let (presets, tests, errors) = hil::config::list_available_tests();

        Self {
            title: format!("HIL #{}", instance_num),
            found_presets: presets,
            found_tests: tests,
            load_errors: errors,
            start_error: None,
            hil_state: hil::run::HilState::Idle,
        }
    }

    pub fn handle_can_message(&mut self, msg: &messages::MsgFromCan) {
        if let messages::MsgFromCan::ParsedMessage(parsed) = msg
            && let hil::run::HilState::Running {
                start_time, tests, ..
            } = &mut self.hil_state
        {
            for test in tests {
                test.process_can(parsed, *start_time);
            }
        }
    }

    fn reload_tests(&mut self) {
        let (presets, tests, errors) = hil::config::list_available_tests();
        self.found_presets = presets;
        self.found_tests = tests;
        self.load_errors = errors;
        self.start_error = None;
    }

    fn try_start_from_test(&mut self, test: &hil::config::TestInfo) {
        self.start_error = None;

        let test = match hil::run::HilRunningTest::new(test) {
            Ok(test) => test,
            Err(err) => {
                self.start_error = Some(format!("Error starting test: {}", err));
                return;
            }
        };

        self.hil_state = hil::run::HilState::Running {
            start_time: std::time::Instant::now(),
            preset_info: None,
            tests: vec![test],
        };
    }

    fn try_start_from_preset(&mut self, preset: &hil::config::PresetInfo) {
        self.start_error = None;

        let mut tests = Vec::new();
        for test_name in &preset.tests {
            let test_info = match self.found_tests.iter().find(|t| t.basename == *test_name) {
                Some(info) => info,
                None => {
                    self.start_error = Some(format!(
                        "Error starting preset: Test '{}' not found",
                        test_name
                    ));
                    return;
                }
            };
            let test = match hil::run::HilRunningTest::new(test_info) {
                Ok(test) => test,
                Err(err) => {
                    self.start_error = Some(format!("Error starting preset: {}", err));
                    return;
                }
            };
            tests.push(test);
        }

        self.hil_state = hil::run::HilState::Running {
            start_time: std::time::Instant::now(),
            preset_info: Some(preset.clone()),
            tests,
        };
    }

    pub fn show(&mut self, ui: &mut egui::Ui) -> egui_tiles::UiResponse {
        let mut idle_requested = false;

        egui::ScrollArea::vertical().show(ui, |ui| match &mut self.hil_state {
            hil::run::HilState::Idle => {
                ui.label("HIL is idle. Select a preset or test to run.");
                if ui.button("Reload Tests").clicked() {
                    self.reload_tests();
                }
                ui.separator();
                if !self.load_errors.is_empty() {
                    ui.label("Errors loading tests:");
                    for err in &self.load_errors {
                        ui.label(format!("- {}", err));
                    }
                    ui.separator();
                }

                if let Some(err) = &self.start_error {
                    ui.colored_label(egui::Color32::RED, err);
                    ui.separator();
                }

                if !self.found_presets.is_empty() {
                    ui.label("Presets:");
                    let mut selected_preset = None;
                    for preset in &self.found_presets {
                        let preset_info = format!("{} - {}", preset.name, preset.tests.join(", "));
                        if ui.button(&preset_info).clicked() {
                            selected_preset = Some(preset.clone());
                        }
                    }
                    if let Some(preset) = selected_preset {
                        self.try_start_from_preset(&preset);
                    }
                    ui.separator();
                }
                if !self.found_tests.is_empty() {
                    ui.label("Individual Tests:");
                    let mut selected_test = None;
                    for test in &self.found_tests {
                        let test_info =
                            format!("{} [{}]: {}", test.name, test.basename, test.description);
                        if ui.button(&test_info).clicked() {
                            selected_test = Some(test.clone());
                        }
                    }
                    if let Some(test) = selected_test {
                        self.try_start_from_test(&test);
                    }
                }
            }
            hil::run::HilState::Running {
                start_time,
                preset_info,
                tests,
            } => {
                // Actually run the test
                tests
                    .iter_mut()
                    .for_each(|t| t.update_expect_statuses(*start_time));
                let all_finished = tests.iter().all(|t| t.is_finished());

                ui.add_space(4.0);
                if all_finished {
                    // ui.label("HIL finished! Review the results below.");
                    ui.horizontal(|ui| {
                        if ui.button("Exit").clicked() {
                            idle_requested = true;
                        }
                        ui.label("HIL finished! Review the results below.");
                    });
                } else {
                    ui.horizontal(|ui| {
                        if ui.button("Stop").clicked() {
                            idle_requested = true;
                        }
                        let time_since_start = start_time.elapsed().as_millis();
                        ui.label(format!(
                            "HIL is running... {:.0} ms since start",
                            time_since_start
                        ));
                    });
                }
                ui.separator();

                if let Some(preset) = preset_info {
                    ui.label(format!(
                        "Preset: {} ({} tests)",
                        preset.name,
                        preset.tests.len()
                    ));
                    Self::show_preset_summary(ui, tests);
                }
                ui.separator();

                for test in tests {
                    Self::show_test(ui, test);
                    ui.separator();
                }
            }
        });

        if idle_requested {
            self.hil_state = hil::run::HilState::Idle;
        }

        egui_tiles::UiResponse::None
    }

    fn show_preset_summary(ui: &mut egui::Ui, tests: &[hil::run::HilRunningTest]) {
        if tests.is_empty() || !tests.iter().all(|t| t.is_finished()) {
            return;
        }

        let (mut total_passed, mut total_expects, mut subtests_all_passed) = (0, 0, 0);
        for t in tests {
            let total = t.in_progress_expects.len();
            let passed = t
                .in_progress_expects
                .iter()
                .filter(|e| matches!(e.result, hil::run::ExpectResult::Passed))
                .count();
            total_passed += passed;
            total_expects += total;
            if passed == total {
                subtests_all_passed += 1;
            }
        }

        let color = if total_passed == total_expects {
            egui::Color32::GREEN
        } else {
            egui::Color32::RED
        };

        ui.colored_label(
            color,
            format!(
                "Preset finished: {}/{} expects passed, {}/{} subtests fully passed",
                total_passed,
                total_expects,
                subtests_all_passed,
                tests.len()
            ),
        );
    }

    fn show_test(ui: &mut egui::Ui, test: &hil::run::HilRunningTest) {
        egui::CollapsingHeader::new(&test.test_info.name)
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

    fn show_test_progress(ui: &mut egui::Ui, test: &hil::run::HilRunningTest) {
        let (not_in_window, in_progress, completed) = test.expect_counts();
        let total = not_in_window + in_progress + completed;
        if total > 0 {
            let frac = completed as f32 / total as f32;
            ui.add(egui::ProgressBar::new(frac).text(format!("{}/{} complete", completed, total)));
        }
    }

    fn show_test_finished_summary(ui: &mut egui::Ui, test: &hil::run::HilRunningTest) {
        if !test.is_finished() {
            return;
        }

        let total = test.in_progress_expects.len();
        let passed = test
            .in_progress_expects
            .iter()
            .filter(|e| matches!(e.result, hil::run::ExpectResult::Passed))
            .count();

        let color = if passed == total {
            egui::Color32::GREEN
        } else {
            egui::Color32::RED
        };

        ui.colored_label(
            color,
            format!("Test finished: {} passed of {} expects", passed, total),
        );
    }

    fn show_expects_grid(ui: &mut egui::Ui, test: &hil::run::HilRunningTest) {
        egui::Grid::new(format!("expects_{}", test.test_info.basename))
            .num_columns(4)
            .striped(true)
            .show(ui, |ui| {
                ui.strong("Message");
                ui.strong("Window (ms)");
                ui.strong("Status");
                ui.strong("Signals");
                ui.end_row();

                for ipe in &test.in_progress_expects {
                    Self::show_expect_row(ui, ipe);
                }
            });
    }

    fn show_expect_row(ui: &mut egui::Ui, ipe: &hil::run::InProgressExpect) {
        ui.label(&ipe.expect.msg_name);
        ui.label(format!(
            "{:.0} - {:.0}",
            ipe.expect.window[0], ipe.expect.window[1]
        ));
        ui.colored_label(ipe.result.as_color32(), ipe.result.as_str());

        if ipe.expect.signals.is_empty() {
            ui.label("—");
        } else {
            ui.vertical(|ui| {
                for (name, range) in &ipe.expect.signals {
                    match ipe.failures.iter().find(|f| f.name() == name) {
                        Some(hil::run::SignalFailure::OutOfRange { value, range, .. }) => {
                            ui.colored_label(
                                egui::Color32::RED,
                                format!("{}: {} outside [{}, {}]", name, value, range[0], range[1]),
                            );
                        }
                        Some(hil::run::SignalFailure::MissingSignal { range, .. }) => {
                            ui.colored_label(
                                egui::Color32::RED,
                                format!("{}: missing (expected [{}, {}])", name, range[0], range[1]),
                            );
                        }
                        None => {
                            ui.label(format!("{}: [{}, {}]", name, range[0], range[1]));
                        }
                    }
                }
            });
        }
        ui.end_row();
    }
}
