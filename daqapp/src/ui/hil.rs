use crate::{hil, messages};
use eframe::egui;

pub struct Hil {
    pub title: String,
    pub found_presets: Vec<hil::config::PresetInfo>,
    pub found_tests: Vec<hil::config::TestInfo>,
    pub load_errors: Vec<String>,
    pub snapshot: hil::engine::HilSnapshot,
    ui_to_can_tx: std::sync::mpsc::Sender<messages::MsgFromUi>,
}

impl Hil {
    pub fn new(
        instance_num: usize,
        ui_to_can_tx: std::sync::mpsc::Sender<messages::MsgFromUi>,
    ) -> Self {
        let (presets, tests, errors) = hil::config::list_available_tests();

        Self {
            title: format!("HIL #{}", instance_num),
            found_presets: presets,
            found_tests: tests,
            load_errors: errors,
            snapshot: hil::engine::HilSnapshot::idle(),
            ui_to_can_tx,
        }
    }

    pub fn handle_can_message(&mut self, msg: &messages::MsgFromCan) {
        if let messages::MsgFromCan::Hil(snapshot) = msg {
            self.snapshot = snapshot.clone();
        }
    }

    fn send_command(&self, command: hil::engine::HilCommand) {
        if let Err(e) = self.ui_to_can_tx.send(messages::MsgFromUi::Hil(command)) {
            log::error!("Failed to send HIL command to CAN thread: {e}");
        }
    }

    fn reload_tests(&mut self) {
        let (presets, tests, errors) = hil::config::list_available_tests();
        self.found_presets = presets;
        self.found_tests = tests;
        self.load_errors = errors;
    }

    pub fn show(&mut self, ui: &mut egui::Ui) -> egui_tiles::UiResponse {
        egui::ScrollArea::vertical().show(ui, |ui| match self.snapshot.status {
            hil::engine::HilStatus::Idle => {
                self.show_idle(ui);
            }
            hil::engine::HilStatus::Running => {
                self.show_running(ui);
            }
        });

        egui_tiles::UiResponse::None
    }

    fn show_idle(&mut self, ui: &mut egui::Ui) {
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
            ui.colored_label(egui::Color32::RED, error);
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
                self.send_command(hil::engine::HilCommand::StartPreset(preset));
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
                self.send_command(hil::engine::HilCommand::StartTest(test));
            }
        }
    }

    fn show_running(&mut self, ui: &mut egui::Ui) {
        let tests = &self.snapshot.tests;
        let all_finished = tests.iter().all(|t| t.is_finished());
        let mut stop_requested = false;

        ui.add_space(4.0);
        if all_finished {
            ui.horizontal(|ui| {
                if ui.button("Exit").clicked() {
                    stop_requested = true;
                }
                ui.label("HIL finished. Review results below.");
            });
        } else {
            ui.horizontal(|ui| {
                if ui.button("Stop").clicked() {
                    stop_requested = true;
                }
                ui.label(format!("HIL running for {} ms", self.snapshot.elapsed_ms));
            });
        }
        ui.separator();

        if let Some(preset) = &self.snapshot.preset {
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

        if stop_requested {
            self.send_command(hil::engine::HilCommand::Stop);
        }
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
                                format!(
                                    "{}: missing (expected [{}, {}])",
                                    name, range[0], range[1]
                                ),
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

impl Drop for Hil {
    fn drop(&mut self) {
        self.send_command(hil::engine::HilCommand::Stop);
    }
}
