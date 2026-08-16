//! Firmware package selection and CAN update progress.

use crate::{bootloader_protocol::FirmwarePackage, messages};
use eframe::egui;
use std::collections::{HashMap, HashSet};

const CAPABILITY_TIMEOUT: std::time::Duration = std::time::Duration::from_secs(12);

#[derive(Clone, Copy)]
struct TargetCapability {
    bootloadable: bool,
    last_seen: std::time::Instant,
}

pub struct Bootloader {
    pub title: String,
    manifest_path: Option<std::path::PathBuf>,
    package: Option<FirmwarePackage>,
    status: String,
    progress: Option<messages::FirmwareProgress>,
    running: bool,
    capabilities: HashMap<String, TargetCapability>,
    selected_targets: HashSet<String>,
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
            capabilities: HashMap::new(),
            selected_targets: HashSet::new(),
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
            ui.label("Available targets:");

            for image in &package.images {
                let available = self
                    .capabilities
                    .get(&image.name)
                    .is_some_and(|capability| {
                        capability.bootloadable
                            && capability.last_seen.elapsed() <= CAPABILITY_TIMEOUT
                    });
                if !available {
                    self.selected_targets.remove(&image.name);
                }
                let mut selected = self.selected_targets.contains(&image.name);
                if ui
                    .add_enabled(
                        !self.running && available,
                        egui::Checkbox::new(&mut selected, &image.name),
                    )
                    .changed()
                {
                    if selected {
                        self.selected_targets.insert(image.name.clone());
                    } else {
                        self.selected_targets.remove(&image.name);
                    }
                }
                if !available {
                    ui.small("Not bootloadable or no recent capability telemetry");
                }
            }

            let selected_images: Vec<_> = package
                .images
                .iter()
                .filter(|image| {
                    self.selected_targets.contains(&image.name)
                        && self
                            .capabilities
                            .get(&image.name)
                            .is_some_and(|capability| {
                                capability.bootloadable
                                    && capability.last_seen.elapsed() <= CAPABILITY_TIMEOUT
                            })
                })
                .cloned()
                .collect();
            let selected_count = selected_images.len();
            if ui
                .add_enabled(
                    !self.running && selected_count > 0,
                    egui::Button::new(format!("Upload selected ({selected_count})")),
                )
                .clicked()
                && ui_to_can_tx
                    .send(messages::MsgFromUi::StartFirmwareUpdate(FirmwarePackage {
                        images: selected_images,
                    }))
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
        if let messages::MsgFromCan::ParsedMessage(parsed) = msg {
            let (target, capability_signal) = match parsed.decoded.name.as_str() {
                "main_version" => ("main_module", "bootloadable"),
                "dash_version" => ("dashboard", "bootloadable"),
                "torque_vector_version" => ("torque_vector", "bootloadable"),
                "abox_version" => ("a_box", "bootloadable"),
                "front_driveline_version" => ("front_driveline", "bootloadable"),
                "rear_driveline_version" => ("rear_driveline", "bootloadable"),
                "bl_main_module_info" => ("main_module", "flags"),
                "bl_dashboard_info" => ("dashboard", "flags"),
                "bl_torque_vector_info" => ("torque_vector", "flags"),
                "bl_a_box_info" => ("a_box", "flags"),
                "bl_front_driveline_info" => ("front_driveline", "flags"),
                "bl_rear_driveline_info" => ("rear_driveline", "flags"),
                _ => return,
            };

            if let Some(signal) = parsed.decoded.signals.get(capability_signal) {
                let raw_value = signal.value.physical.round() as u32;
                let bootloadable = if capability_signal == "flags" {
                    (raw_value & 1) != 0
                } else {
                    raw_value != 0
                };
                self.capabilities.insert(
                    target.to_string(),
                    TargetCapability {
                        bootloadable,
                        last_seen: std::time::Instant::now(),
                    },
                );
                if !bootloadable {
                    self.selected_targets.remove(target);
                }
            }
            return;
        }

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
