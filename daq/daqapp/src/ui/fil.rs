use crate::{action, app, connection, messages, settings};
use eframe::egui;
use std::collections::HashMap;

#[derive(Clone, Copy, Default)]
struct GpioPinState {
    input: Option<bool>,
    output: Option<bool>,
}

pub struct FilControl {
    pub title: String,
    executable: Option<std::path::PathBuf>,
    network: Option<std::path::PathBuf>,
    bus: String,
    adc_board: String,
    adc_instance: String,
    adc_channel: u8,
    adc_value: u16,
    gpio_board: String,
    gpio_port: String,
    gpio_states: HashMap<(String, String, u8), GpioPinState>,
}

impl FilControl {
    pub fn new(instance_num: usize) -> Self {
        let saved = settings::Settings::load();
        Self {
            title: format!("FIL Control #{}", instance_num),
            executable: saved.fil_executable,
            network: saved.fil_network_config,
            bus: saved.fil_bus,
            adc_board: saved.fil_adc_board,
            adc_instance: saved.fil_adc_instance,
            adc_channel: saved.fil_adc_channel.min(19),
            adc_value: saved.fil_adc_value.min(4095),
            gpio_board: "dashboard".into(),
            gpio_port: "GPIOA".into(),
            gpio_states: HashMap::new(),
        }
    }

    pub fn handle_can_message(&mut self, msg: &messages::MsgFromCan) {
        match msg {
            messages::MsgFromCan::FilGpio {
                board,
                port,
                pin,
                value,
                direction,
            } => {
                let state = self
                    .gpio_states
                    .entry((board.clone(), port.clone(), *pin))
                    .or_default();
                match direction {
                    messages::FilGpioDirection::Input => state.input = *value,
                    messages::FilGpioDirection::Output => state.output = *value,
                }
            }
            messages::MsgFromCan::Disconnection
            | messages::MsgFromCan::ConnectionSuccessful
            | messages::MsgFromCan::ConnectionFailed(_) => self.gpio_states.clear(),
            _ => {}
        }
    }

    fn queue_config_update(&self, actions: &mut Vec<action::AppAction>) {
        actions.push(action::AppAction::UpdateFilConfig {
            executable: self.executable.clone(),
            network: self.network.clone(),
            bus: self.bus.clone(),
        });
    }

    pub fn show(
        &mut self,
        ui: &mut egui::Ui,
        actions: &mut Vec<action::AppAction>,
        tx: &std::sync::mpsc::Sender<messages::MsgFromUi>,
        connection_status: &app::ConnectionStatus,
    ) -> egui_tiles::UiResponse {
        egui::ScrollArea::vertical().show(ui, |ui| {
            ui.heading("FIL real-time emulator");
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
                        self.executable = Some(path);
                        self.queue_config_update(actions);
                    }
                    ui.label(
                        self.executable
                            .as_ref()
                            .map(|path| path.display().to_string())
                            .unwrap_or_else(|| "None selected".into()),
                    );
                });
                ui.horizontal(|ui| {
                    if ui.button("Select network").clicked()
                        && let Some(path) = rfd::FileDialog::new()
                            .add_filter("JSON network config", &["json"])
                            .pick_file()
                    {
                        self.network = Some(path);
                        self.queue_config_update(actions);
                    }
                    ui.label(
                        self.network
                            .as_ref()
                            .map(|path| path.display().to_string())
                            .unwrap_or_else(|| "None selected".into()),
                    );
                });
                ui.horizontal(|ui| {
                    ui.label("Message Sender bus:");
                    if ui.text_edit_singleline(&mut self.bus).changed() {
                        self.queue_config_update(actions);
                    }
                });
                ui.horizontal(|ui| {
                    let can_connect = self.executable.is_some() && self.network.is_some();
                    if ui
                        .add_enabled(can_connect, egui::Button::new("Connect / Restart"))
                        .clicked()
                    {
                        let source = connection::ConnectionSource::Fil {
                            executable: self.executable.clone().expect("checked executable"),
                            network: self.network.clone().expect("checked network"),
                            bus: self.bus.clone(),
                        };
                        actions.push(action::AppAction::ConnectFil(source));
                    }
                    if ui.button("Disconnect").clicked()
                        && let (Some(executable), Some(network)) =
                            (self.executable.clone(), self.network.clone())
                    {
                        let _ = tx.send(messages::MsgFromUi::DisconnectFil {
                            executable,
                            network,
                        });
                    }
                });
            });

            ui.add_space(8.0);
            ui.group(|ui| {
                ui.heading("ADC injection");
                ui.horizontal(|ui| {
                    ui.label("Board:");
                    ui.text_edit_singleline(&mut self.adc_board);
                    ui.label("Instance:");
                    ui.text_edit_singleline(&mut self.adc_instance);
                });
                ui.horizontal(|ui| {
                    ui.label("Channel:");
                    ui.add(egui::DragValue::new(&mut self.adc_channel).range(0..=19));
                    ui.label("Raw value:");
                    ui.add(egui::Slider::new(&mut self.adc_value, 0..=4095));
                    if ui.button("Inject").clicked() {
                        actions.push(action::AppAction::UpdateFilAdc {
                            board: self.adc_board.clone(),
                            instance: self.adc_instance.clone(),
                            channel: self.adc_channel,
                            value: self.adc_value,
                        });
                        let _ = tx.send(messages::MsgFromUi::SetFilAdc {
                            board: self.adc_board.clone(),
                            instance: self.adc_instance.clone(),
                            channel: self.adc_channel,
                            value: self.adc_value,
                        });
                    }
                });
            });

            ui.add_space(8.0);
            ui.group(|ui| {
                ui.heading("GPIO live view and input control");
                ui.horizontal(|ui| {
                    ui.label("Board:");
                    ui.text_edit_singleline(&mut self.gpio_board);
                    ui.label("Port:");
                    ui.text_edit_singleline(&mut self.gpio_port);
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
                                .get(&(self.gpio_board.clone(), self.gpio_port.clone(), pin))
                                .copied()
                                .unwrap_or_default();
                            ui.label(format!("{}{}", self.gpio_port, pin));
                            ui.label(level_text(state.input, "Released"));
                            ui.label(level_text(state.output, "Unknown"));
                            ui.horizontal(|ui| {
                                for (label, value) in [
                                    ("Low", Some(false)),
                                    ("High", Some(true)),
                                    ("Release", None),
                                ] {
                                    if ui.small_button(label).clicked() {
                                        let _ = tx.send(messages::MsgFromUi::SetFilGpio {
                                            board: self.gpio_board.clone(),
                                            port: self.gpio_port.clone(),
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

fn level_text(value: Option<bool>, unset: &str) -> egui::RichText {
    match value {
        Some(true) => egui::RichText::new("HIGH").color(egui::Color32::LIGHT_GREEN),
        Some(false) => egui::RichText::new("LOW").color(egui::Color32::LIGHT_RED),
        None => egui::RichText::new(unset).color(egui::Color32::GRAY),
    }
}
