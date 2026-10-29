use super::*;

impl FilControl {
    pub fn show(
        &mut self,
        ui: &mut egui::Ui,
        actions: &mut Vec<action::AppAction>,
        tx: &std::sync::mpsc::Sender<daqcore::can_thread::CanThreadCommand>,
        connection_status: &app::ConnectionStatus,
    ) -> egui_tiles::UiResponse {
        self.refresh_network_info();
        self.poll_network_info(actions);
        self.poll_file_operations(actions);
        if self.has_pending_file_operation() {
            ui.ctx().request_repaint_after(Duration::from_millis(50));
        }
        egui::ScrollArea::vertical().show(ui, |ui| {
            ui.heading("FIL real-time emulator");
            if self.network_info_rx.is_some() {
                ui.label("Loading network config…");
            }
            if self.import_rx.is_some() {
                ui.label("Importing board file…");
            }
            if self.export_rx.is_some() {
                ui.label("Exporting network…");
            }
            if let Some(error) = &self.operation_error {
                ui.colored_label(egui::Color32::LIGHT_RED, error);
            }
            if let Some(error) = &self.annotation_error {
                ui.colored_label(
                    egui::Color32::LIGHT_RED,
                    format!("FIL annotation config unavailable: {error}"),
                );
            }
            let status = match connection_status {
                app::ConnectionStatus::Disconnected => {
                    egui::RichText::new("● Disconnected").color(egui::Color32::GRAY)
                }
                app::ConnectionStatus::Connected => {
                    egui::RichText::new("● Connected").color(egui::Color32::GREEN)
                }
                app::ConnectionStatus::Error(error) => {
                    egui::RichText::new(format!("● {error}")).color(egui::Color32::RED)
                }
            };
            ui.label(status);

            ui.group(|ui| {
                ui.heading("Process configuration");
                ui.horizontal(|ui| {
                    if ui.button("Select FIL executable").clicked()
                        && let Some(path) = rfd::FileDialog::new().pick_file()
                    {
                        self.settings.executable = Some(path);
                        self.queue_config_update(actions);
                    }
                    ui.label(
                        self.settings.executable
                            .as_ref()
                            .map(|path| path.display().to_string())
                            .unwrap_or_else(|| "None selected".into()),
                    );
                });
                ui.horizontal(|ui| {
                    ui.label("Network source:");
                    if ui.radio(!self.settings.use_builder, "Network file").clicked() {
                        self.settings.use_builder = false;
                        self.queue_builder_update(actions);
                    }
                    if ui.radio(self.settings.use_builder, "Build network").clicked() {
                        self.settings.use_builder = true;
                        self.queue_builder_update(actions);
                    }
                });
                if self.settings.use_builder {
                    self.show_builder(ui, actions);
                } else {
                    self.show_file_network(ui, actions);
                }
                ui.small("The sidebar Bus selection controls CAN viewing and Message Sender.");
                ui.collapsing("Run options", |ui| {
                    let mut changed = false;
                    ui.horizontal(|ui| {
                        ui.label("Duration (ms, 0 = unlimited):");
                        changed |= ui
                            .add(
                                egui::DragValue::new(&mut self.settings.run_options.duration_ms)
                                    .range(0..=u64::MAX / 1_000_000),
                            )
                            .changed();
                    });
                    ui.horizontal(|ui| {
                        ui.label("Max instructions per board:");
                        changed |= ui
                            .add(
                                egui::DragValue::new(&mut self.settings.run_options.max_instructions)
                                    .range(1..=u64::MAX),
                            )
                            .changed();
                    });
                    ui.horizontal(|ui| {
                        ui.label("Instruction quantum:");
                        changed |= ui
                            .add(
                                egui::DragValue::new(&mut self.settings.run_options.quantum)
                                    .range(1..=u64::MAX),
                            )
                            .changed();
                    });
                    ui.horizontal(|ui| {
                        ui.label("Refresh interval (ms):");
                        changed |= ui
                            .add(
                                egui::DragValue::new(&mut self.settings.run_options.refresh_ms)
                                    .range(1..=i32::MAX as u32),
                            )
                            .changed();
                    });
                    ui.horizontal(|ui| {
                        ui.label("ADC decimation (1 = every scan):");
                        changed |= ui
                            .add(
                                egui::DragValue::new(&mut self.settings.run_options.adc_decimation)
                                    .range(1..=1024),
                            )
                            .changed();
                    });
                    if self.settings.run_options.adc_decimation > 1 {
                        ui.small(
                            "Decimation skips ADC scans; use 1 for pedal and fault validation.",
                        );
                    }
                    ui.horizontal(|ui| {
                        ui.label("Additional live trace filters (comma-separated):");
                        changed |= ui
                            .text_edit_singleline(&mut self.settings.run_options.extra_live_filters)
                            .changed();
                    });
                    changed |= ui
                        .checkbox(&mut self.settings.run_options.strict_mmio, "Strict MMIO")
                        .changed();
                    changed |= ui
                        .checkbox(&mut self.settings.run_options.wall_pacing, "Wall-clock pacing")
                        .changed();
                    changed |= ui
                        .checkbox(
                            &mut self.settings.run_options.trace_instructions,
                            "Trace instructions",
                        )
                        .changed();
                    changed |= ui
                        .checkbox(&mut self.settings.run_options.detect_spin, "Detect spin loops")
                        .changed();
                    if changed {
                        self.queue_config_update(actions);
                    }
                });
                ui.add_space(4.0);
                ui.horizontal(|ui| {
                    let issues = self.connect_issues();
                    if ui
                        .add_enabled(issues.is_empty(), egui::Button::new("Connect / Restart"))
                        .clicked()
                    {
                        if let Some(source) = self.connect_source() {
                            actions.push(action::AppAction::ConnectFil(source));
                        } else {
                            self.operation_error = Some(
                                "Unable to create a connection source from the selected settings"
                                    .into(),
                            );
                        }
                    }
                    if ui.button("Disconnect").clicked() && self.settings.executable.is_some() {
                        let _ = tx.send(daqcore::can_thread::CanThreadCommand::Connect(None));
                    }
                });
                for issue in self.connect_issues() {
                    ui.label(
                        egui::RichText::new(format!("⚠ {issue}")).color(egui::Color32::YELLOW),
                    );
                }
            });

            ui.add_space(8.0);
            ui.group(|ui| {
                ui.horizontal(|ui| {
                    ui.heading("CAN expectations");
                    if ui.button("Clear").clicked() {
                        clear_expectations(
                            &mut self.expectations,
                            &mut self.expectation_order,
                        );
                    }
                });
                let count = |status| {
                    self.expectations
                        .values()
                        .filter(|event| event.status == status)
                        .count()
                };
                ui.label(format!(
                    "Pending: {}   Pass: {}   Fail: {}   Incomplete: {}",
                    count(daqcore::can::driver::FilExpectationStatus::Pending),
                    count(daqcore::can::driver::FilExpectationStatus::Pass),
                    count(daqcore::can::driver::FilExpectationStatus::Fail),
                    count(daqcore::can::driver::FilExpectationStatus::Incomplete)
                ));
                if self.expectation_order.is_empty() {
                    ui.weak("No FIL expectation trace records received.");
                } else {
                    egui::ScrollArea::vertical()
                        .max_height(180.0)
                        .show(ui, |ui| {
                            ui.style_mut().wrap_mode = Some(egui::TextWrapMode::Wrap);
                            for id in &self.expectation_order {
                                let Some(event) = self.expectations.get(id) else {
                                    continue;
                                };
                                let (status, color) = match event.status {
                                    daqcore::can::driver::FilExpectationStatus::Pending => {
                                        ("Pending", egui::Color32::YELLOW)
                                    }
                                    daqcore::can::driver::FilExpectationStatus::Pass => {
                                        ("Passed", egui::Color32::LIGHT_GREEN)
                                    }
                                    daqcore::can::driver::FilExpectationStatus::Fail => {
                                        ("Failed", egui::Color32::LIGHT_RED)
                                    }
                                    daqcore::can::driver::FilExpectationStatus::Incomplete => {
                                        ("Incomplete", egui::Color32::GRAY)
                                    }
                                };
                                egui::CollapsingHeader::new(
                                    egui::RichText::new(format!(
                                        "{status} · {}",
                                        expectation_name(event)
                                    ))
                                    .color(color),
                                )
                                .id_salt(("fil_expectation", &self.title, id))
                                .show(ui, |ui| {
                                    ui.label(format!(
                                        "Expected: {} · 0x{:X} · {}",
                                        event.expected_bus,
                                        event.expected_id,
                                        if event.expected_extended {
                                            "extended"
                                        } else {
                                            "standard"
                                        },
                                    ))
                                    .on_hover_text(format!("{}\n{}", event.check_id, event.script));
                                    ui.label(format!(
                                        "Data: {}",
                                        expectation_bytes(&event.expected_data)
                                    ));
                                    ui.label(format!(
                                        "Window: {}–{} ms",
                                        expectation_ms(event.window_start_ns),
                                        expectation_ms(event.window_end_ns),
                                    ))
                                    .on_hover_text(format!(
                                        "{}–{} ns (inclusive)",
                                        event.window_start_ns, event.window_end_ns
                                    ));
                                    if let Some(reason) = &event.reason {
                                        ui.label(format!("Result: {reason}"));
                                    } else {
                                        ui.label(match event.status {
                                            daqcore::can::driver::FilExpectationStatus::Pending => {
                                                "Result: waiting for matching firmware output"
                                            }
                                            daqcore::can::driver::FilExpectationStatus::Pass => {
                                                "Result: matching firmware output received"
                                            }
                                            daqcore::can::driver::FilExpectationStatus::Fail => {
                                                "Result: no match within the window"
                                            }
                                            daqcore::can::driver::FilExpectationStatus::Incomplete => {
                                                "Result: run ended before the window resolved"
                                            }
                                        });
                                    }
                                    if let Some(bus) = &event.matched_bus {
                                        let frame = event
                                            .matched_id
                                            .map(|id| format!(" · 0x{id:X}"))
                                            .unwrap_or_default();
                                        ui.label(format!("Matched: {bus}{frame}"));
                                    }
                                    if let Some(data) = &event.matched_data {
                                        ui.label(format!(
                                            "Matched data: {}",
                                            expectation_bytes(data)
                                        ));
                                    }
                                    if let Some(origin) = &event.matched_origin {
                                        ui.label(format!("From: {origin}"));
                                    }
                                    if let Some(time) = event.matched_time_ns {
                                        ui.label(format!("At: {} ms", expectation_ms(time)))
                                            .on_hover_text(format!("{time} ns"));
                                    }
                                })
                                .header_response
                                .on_hover_text(format!("{}\n{}", event.check_id, event.script));
                            }
                        });
                }
            });

            ui.add_space(8.0);
            ui.group(|ui| {

                ui.heading("ADC injection");
                ui.horizontal(|ui| {
                    ui.label("Board:");
                    let mut board = std::mem::take(&mut self.settings.adc_board);
                    if self.show_config_board_picker(ui, "fil_adc_board", &mut board) {
                        actions.push(action::AppAction::UpdateFilAdc {
                            board: board.clone(),
                            instance: self.settings.adc_instance,
                            channel: self.settings.adc_channel,
                            value: self.settings.adc_value,
                        });
                    }
                    self.settings.adc_board = board;
                    ui.label("Instance:");
                    let adc_annotation = self
                        .annotations
                        .adc
                        .as_ref()
                        .and_then(|boards| boards.get(&self.settings.adc_board))
                        .and_then(|instances| instances.get(self.settings.adc_instance.as_str()));
                    let selected_instance = annotated_text(
                        self.settings.adc_instance.as_str(),
                        adc_annotation.and_then(|annotation| annotation.label.as_deref()),
                    );
                    let instance_labels: Vec<(FilAdcInstance, String)> = FilAdcInstance::ALL
                        .into_iter()
                        .map(|instance| {
                            let label = self
                                .annotations
                                .adc
                                .as_ref()
                                .and_then(|boards| boards.get(&self.settings.adc_board))
                                .and_then(|instances| instances.get(instance.as_str()))
                                .and_then(|annotation| annotation.label.as_deref());
                            (instance, annotated_text(instance.as_str(), label))
                        })
                        .collect();

                    egui::ComboBox::from_id_salt(("fil_adc_instance", &self.title))
                        .selected_text(selected_instance)
                        .show_ui(ui, |ui| {
                            for (instance, label) in &instance_labels {
                                ui.selectable_value(&mut self.settings.adc_instance, *instance, label);
                            }
                        });
                });
                ui.horizontal(|ui| {
                    ui.label("Channel:");
                    let channel_annotations = self
                        .annotations
                        .adc
                        .as_ref()
                        .and_then(|boards| boards.get(&self.settings.adc_board))
                        .and_then(|instances| instances.get(self.settings.adc_instance.as_str()))
                        .and_then(|annotation| annotation.channels.as_ref());
                    let selected_channel = annotated_text(
                        &self.settings.adc_channel.to_string(),
                        channel_annotations
                            .and_then(|channels| channels.get(&self.settings.adc_channel))
                            .map(String::as_str),
                    );
                    egui::ComboBox::from_id_salt(("fil_adc_channel", &self.title))
                        .selected_text(selected_channel)
                        .show_ui(ui, |ui| {
                            for channel in 0..=19u8 {
                                let label = annotated_text(
                                    &channel.to_string(),
                                    channel_annotations
                                        .and_then(|channels| channels.get(&channel))
                                        .map(String::as_str),
                                );
                                ui.selectable_value(&mut self.settings.adc_channel, channel, label);
                            }
                        });
                    ui.label("Raw value:");
                    ui.add(egui::Slider::new(&mut self.settings.adc_value, 0..=4095));
                    if ui.button("Inject").clicked() {
                        actions.push(action::AppAction::UpdateFilAdc {
                            board: self.settings.adc_board.clone(),
                            instance: self.settings.adc_instance,
                            channel: self.settings.adc_channel,
                            value: self.settings.adc_value,
                        });
                        let _ = tx.send(daqcore::can_thread::CanThreadCommand::SetFilAdc {
                            board: self.settings.adc_board.clone(),
                            instance: self.settings.adc_instance.as_str().to_owned(),
                            channel: self.settings.adc_channel,
                            value: self.settings.adc_value,
                        });
                    }
                });
            });

            ui.add_space(8.0);
            ui.group(|ui| {
                ui.heading("GPIO live view and input control");
                ui.horizontal(|ui| {
                    ui.label("Board:");
                    let mut board = std::mem::take(&mut self.gpio_board);
                    let changed = self.show_config_board_picker(ui, "fil_gpio_board", &mut board);
                    self.gpio_board = board;
                    let _ = changed;
                    ui.label("Port:");
                    let port_labels: Vec<(FilGpioPort, String)> = FilGpioPort::ALL
                        .into_iter()

                        .map(|port| {
                            let label = self
                                .annotations
                                .gpio
                                .as_ref()
                                .and_then(|boards| boards.get(&self.gpio_board))
                                .and_then(|ports| ports.get(port.as_str()))
                                .and_then(|annotation| annotation.label.as_deref());
                            (port, annotated_text(port.as_str(), label))
                        })
                        .collect();
                    let selected_port = port_labels
                        .iter()
                        .find(|(port, _)| port == &self.gpio_port)
                        .map(|(_, label)| label.as_str())
                        .unwrap_or(self.gpio_port.as_str());
                    egui::ComboBox::from_id_salt(("fil_gpio_port", &self.title))
                        .selected_text(selected_port)
                        .show_ui(ui, |ui| {
                            for (port, label) in &port_labels {
                                ui.selectable_value(&mut self.gpio_port, *port, label);
                            }
                        });
                });
                egui::Grid::new(("fil_gpio_grid", &self.title))
                    .striped(true)
                    .show(ui, |ui| {
                        ui.strong("Pin");
                        ui.strong("Input override");
                        ui.strong("Firmware output");
                        ui.strong("Drive input");
                        ui.end_row();
                        for pin in 0..16u8 {
                            let state = self
                                .gpio_states
                                .get(&(self.gpio_board.clone(), self.gpio_port, pin))
                                .copied()
                                .unwrap_or_default();
                            let pin_label = self
                                .annotations
                                .gpio
                                .as_ref()
                                .and_then(|boards| boards.get(&self.gpio_board))
                                .and_then(|ports| ports.get(self.gpio_port.as_str()))
                                .and_then(|annotation| annotation.pins.as_ref())
                                .and_then(|pins| pins.get(&pin))
                                .map(String::as_str);
                            ui.label(annotated_text(
                                &format!("{}{}", self.gpio_port.as_str(), pin),
                                pin_label,
                            ));
                            ui.label(level_text(state.input, "Released"));
                            ui.label(level_text(state.output, "Unknown"));
                            ui.horizontal(|ui| {
                                for (label, value) in [
                                    ("Low", Some(false)),
                                    ("High", Some(true)),
                                    ("Release", None),
                                ] {
                                    if ui.small_button(label).clicked() {
                                        let _ = tx.send(daqcore::can_thread::CanThreadCommand::SetFilGpio {
                                            board: self.gpio_board.clone(),
                                            port: self.gpio_port.as_str().to_owned(),
                                            pin,
                                            value,
                                        });

                                    }
                                }
                            });
                            ui.end_row();
                        }
                    });
            });
        });
        egui_tiles::UiResponse::None
    }
}

pub fn network_result_is_current(
    selected_network: Option<&std::path::Path>,
    result_network: &std::path::Path,
) -> bool {
    selected_network == Some(result_network)
}
pub fn annotated_text(identifier: &str, annotation: Option<&str>) -> String {
    match annotation.map(str::trim).filter(|label| !label.is_empty()) {
        Some(label) => format!("{identifier} — {label}"),
        None => identifier.to_owned(),
    }
}
pub fn elf_status(effective: &Option<std::path::PathBuf>, is_override: bool) -> egui::RichText {
    match effective {
        Some(elf) if elf.is_file() => {
            let text = if is_override {
                format!("⚙ {}", elf.display())
            } else {
                elf.display().to_string()
            };
            egui::RichText::new(text)
        }
        Some(elf) => egui::RichText::new(format!("{} (missing)", elf.display()))
            .color(egui::Color32::LIGHT_RED),
        None => egui::RichText::new("No ELF configured").color(egui::Color32::LIGHT_RED),
    }
}
pub fn level_text(value: Option<bool>, unset: &str) -> egui::RichText {
    match value {
        Some(true) => egui::RichText::new("HIGH").color(egui::Color32::LIGHT_GREEN),
        Some(false) => egui::RichText::new("LOW").color(egui::Color32::LIGHT_RED),
        None => egui::RichText::new(unset).color(egui::Color32::GRAY),
    }
}
