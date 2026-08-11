//! Firmware package selection and CAN update progress.

use crate::{bootloader_protocol::FirmwarePackage, messages};
use eframe::egui;

pub struct Bootloader {
    pub title: String,
    manifest_path: Option<std::path::PathBuf>,
    package: Option<FirmwarePackage>,
    status: String,
    progress: Option<messages::FirmwareProgress>,
    running: bool,
}

impl Bootloader {
    pub fn new(instance_num: usize) -> Self {
        Self {
            title: format!("Bootloader #{}", instance_num),
            manifest_path: None,
            package: None,
            status: "Select manifest.json or firmware_*.tar.gz from per_build.py --package"
                .to_string(),
            progress: None,
            running: false,
        }
    }

    pub fn show(
        &mut self,
        ui: &mut egui::Ui,
        ui_to_can_tx: &std::sync::mpsc::Sender<messages::MsgFromUi>,
    ) -> egui_tiles::UiResponse {
        ui.heading(format!("🔧 {}", self.title));
        ui.separator();

        // Replacing the displayed package mid-update would not replace the
        // package already owned by the CAN thread, so selection is disabled.
        if ui
            .add_enabled(!self.running, egui::Button::new("Select firmware manifest"))
            .clicked()
        {
            if let Some(path) = rfd::FileDialog::new()
                .add_filter("Firmware package", &["json", "gz"])
                .pick_file()
            {
                match FirmwarePackage::load(path.clone()) {
                    Ok(package) => {
                        self.manifest_path = Some(path);
                        self.status = format!("{} board images verified", package.images.len());
                        self.package = Some(package);
                        self.progress = None;
                    }
                    Err(error) => {
                        self.status = format!("Invalid package: {error}");
                        self.manifest_path = None;
                        if !self.running {
                            self.package = None;
                        }
                    }
                }
            }
        }

        if let Some(path) = &self.manifest_path {
            ui.label(format!("Manifest: {}", path.display()));
        }
        ui.label(&self.status);

        if let Some(package) = &self.package {
            ui.label(format!("Verified images: {}", package.images.len()));
            if ui
                .add_enabled(!self.running, egui::Button::new("Upload all boards"))
                .clicked()
                && ui_to_can_tx
                    .send(messages::MsgFromUi::StartFirmwareUpdate(package.clone()))
                    .is_ok()
            {
                self.running = true;
                self.status = "Starting update...".to_string();
            }
        }
        // Cancellation stops the host state machine; it cannot undo target writes.
        if ui
            .add_enabled(self.running, egui::Button::new("Cancel"))
            .clicked()
        {
            let _ = ui_to_can_tx.send(messages::MsgFromUi::CancelFirmwareUpdate);
            self.status = "Cancelling...".to_string();
        }

        if let Some(progress) = &self.progress {
            ui.separator();
            ui.label(format!(
                "Board {}/{}: {} ({})",
                progress.board_index + 1,
                progress.board_count,
                progress.board,
                progress.phase
            ));
            let fraction = if progress.total_bytes == 0 {
                0.0
            } else {
                progress.sent_bytes as f32 / progress.total_bytes as f32
            };
            ui.add(egui::ProgressBar::new(fraction.clamp(0.0, 1.0)).show_percentage());
            ui.label(format!(
                "{} / {} bytes",
                progress.sent_bytes, progress.total_bytes
            ));
            if let Some(error) = &progress.error {
                ui.colored_label(egui::Color32::RED, error);
            }
        }

        egui_tiles::UiResponse::None
    }

    pub fn handle_can_message(&mut self, msg: &messages::MsgFromCan) {
        let messages::MsgFromCan::FirmwareProgress(progress) = msg else {
            return;
        };
        self.progress = Some(progress.clone());
        self.running = progress.error.is_none() && progress.phase != "complete";
        self.status = if let Some(error) = &progress.error {
            format!("Update failed: {error}")
        } else {
            progress.phase.clone()
        };
    }
}
