use crate::{action, app, settings};
use eframe::egui;

pub struct FilControl {
    pub title: String,
    executable: Option<std::path::PathBuf>,
    network: Option<std::path::PathBuf>,
    bus: String,
    adc_board: String,
    adc_instance: String,
    adc_channel: u8,
    adc_value: u16,
}

impl FilControl {
    pub fn new(instance_num: usize) -> Self {
        let saved = settings::Settings::load();
        Self {
            title: format!("FIL Control #{instance_num}"),
            executable: saved.fil_executable,
            network: saved.fil_network_config,
            bus: saved.fil_bus,
            adc_board: saved.fil_adc_board,
            adc_instance: saved.fil_adc_instance,
            adc_channel: saved.fil_adc_channel.min(19),
            adc_value: saved.fil_adc_value.min(4095),
        }
    }

    pub fn handle_can_message(&mut self, _event: &daqcore::can_thread::CanThreadEvent) {}

    fn update_config(&self, actions: &mut Vec<action::AppAction>) {
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
        tx: &std::sync::mpsc::Sender<daqcore::can_thread::CanThreadCommand>,
        status: &app::ConnectionStatus,
    ) -> egui_tiles::UiResponse {
        egui::ScrollArea::vertical().show(ui, |ui| {
            ui.heading("FIL real-time emulator");
            ui.label(match status {
                app::ConnectionStatus::Disconnected => {
                    egui::RichText::new("● Disconnected").color(egui::Color32::GRAY)
                }
                app::ConnectionStatus::Connected => {
                    egui::RichText::new("● Connected").color(egui::Color32::GREEN)
                }
                app::ConnectionStatus::Error(error) => {
                    egui::RichText::new(format!("● {error}")).color(egui::Color32::RED)
                }
            });
            ui.group(|ui| {
                ui.heading("Process configuration");
                ui.horizontal(|ui| {
                    if ui.button("Select FIL executable").clicked()
                        && let Some(path) = rfd::FileDialog::new().pick_file()
                    {
                        self.executable = Some(path);
                        self.update_config(actions);
                    }
                    ui.label(
                        self.executable
                            .as_ref()
                            .map(|p| p.display().to_string())
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
                        self.update_config(actions);
                    }
                    ui.label(
                        self.network
                            .as_ref()
                            .map(|p| p.display().to_string())
                            .unwrap_or_else(|| "None selected".into()),
                    );
                });
                ui.horizontal(|ui| {
                    ui.label("Message Sender bus:");
                    if ui.text_edit_singleline(&mut self.bus).changed() {
                        self.update_config(actions);
                    }
                });
                ui.horizontal(|ui| {
                    let can_connect = self.executable.is_some() && self.network.is_some();
                    if ui
                        .add_enabled(can_connect, egui::Button::new("Connect / Restart"))
                        .clicked()
                    {
                        actions.push(action::AppAction::ConnectFil(
                            daqcore::connection::ConnectionSource::Fil {
                                executable: self
                                    .executable
                                    .clone()
                                    .expect("enabled only with executable"),
                                network: self.network.clone().expect("enabled only with network"),
                                bus: self.bus.clone(),
                            },
                        ));
                    }
                    if ui.button("Disconnect").clicked() {
                        let _ = tx.send(daqcore::can_thread::CanThreadCommand::Connect(None));
                    }
                });
            });
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
                        let _ = tx.send(daqcore::can_thread::CanThreadCommand::SetFilAdc {
                            board: self.adc_board.clone(),
                            instance: self.adc_instance.clone(),
                            channel: self.adc_channel,
                            value: self.adc_value,
                        });
                    }
                });
            });
        });
        egui_tiles::UiResponse::None
    }
}
