//! Firmware package selection and CAN update progress.

use crate::{bootloader_protocol::FirmwarePackage, connection, messages};
use eframe::egui;
use std::collections::{HashMap, HashSet};
use std::time::{Duration, Instant};

const CAPABILITY_TIMEOUT: Duration = Duration::from_secs(12);

#[derive(Clone, Copy)]
struct TelemetryObservation {
    git_hash: Option<u32>,
    bootloadable: Option<bool>,
    last_seen: Instant,
}

#[derive(Default)]
struct TargetObservations {
    application: Option<TelemetryObservation>,
    bootloader: Option<TelemetryObservation>,
}

#[derive(Clone, Debug, PartialEq)]
enum BoardUpdateStatus {
    Idle,
    Pending,
    Uploading {
        phase: String,
        sent_bytes: usize,
        total_bytes: usize,
    },
    Completed,
    Failed(String),
    Cancelled,
}

#[derive(Clone, Copy, Debug, PartialEq, Eq)]
enum CapabilityState {
    Available,
    NotBootloadable,
    NoRecentTelemetry,
}

pub struct Bootloader {
    pub title: String,
    manifest_path: Option<std::path::PathBuf>,
    package: Option<FirmwarePackage>,
    status: String,
    running: bool,
    observations: HashMap<String, TargetObservations>,
    selected_targets: HashSet<String>,
    board_statuses: HashMap<String, BoardUpdateStatus>,
    run_board_names: Vec<String>,
}

impl Bootloader {
    pub fn new(instance_num: usize) -> Self {
        Self {
            title: format!("Bootloader #{}", instance_num),
            manifest_path: None,
            package: None,
            status: "Select manifest.json or firmware_*.tar.gz from firmware/build_firmware.py --package"
                .to_string(),
            running: false,
            observations: HashMap::new(),
            selected_targets: HashSet::new(),
            board_statuses: HashMap::new(),
            run_board_names: Vec::new(),
        }
    }

    pub fn show(
        &mut self,
        ui: &mut egui::Ui,
        ui_to_can_tx: &std::sync::mpsc::Sender<messages::MsgFromUi>,
        active_bus: Option<connection::CanBus>,
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
                        self.selected_targets.clear();
                        self.board_statuses.clear();
                        self.run_board_names.clear();
                        self.running = false;
                    }
                    Err(error) => {
                        self.status = format!("Invalid package: {error}");
                        self.manifest_path = None;
                        if !self.running {
                            self.package = None;
                            self.selected_targets.clear();
                            self.board_statuses.clear();
                            self.run_board_names.clear();
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
            let images = package.images.clone();
            let now = Instant::now();

            egui::Grid::new(egui::Id::new(("bootloader-targets", &self.title)))
                .striped(true)
                .num_columns(7)
                .spacing([8.0, 3.0])
                .show(ui, |ui| {
                    ui.strong("Node");
                    ui.strong("Bus");
                    ui.strong("Application git hash");
                    ui.strong("Bootloader git hash");
                    ui.strong("Availability");
                    ui.strong("Select");
                    ui.strong("Update status");
                    ui.end_row();

                    for image in &images {
                        let bus_matches = active_bus == Some(image.bus);
                        let capability = self.capability_state(&image.name, now);
                        let available = capability == CapabilityState::Available && bus_matches;
                        if !available {
                            self.selected_targets.remove(&image.name);
                        }

                        ui.label(&image.name);
                        ui.label(image.bus.display_name());
                        ui.label(self.hash_label(&image.name, true, now));
                        ui.label(self.hash_label(&image.name, false, now));

                        let availability = if !bus_matches {
                            format!("Inactive bus (need {})", image.bus.display_name())
                        } else {
                            match capability {
                                CapabilityState::Available => "Available".to_string(),
                                CapabilityState::NotBootloadable => "Not bootloadable".to_string(),
                                CapabilityState::NoRecentTelemetry => {
                                    "No recent telemetry".to_string()
                                }
                            }
                        };
                        ui.label(availability);

                        let mut selected = self.selected_targets.contains(&image.name);
                        if ui
                            .add_enabled(
                                !self.running && available,
                                egui::Checkbox::without_text(&mut selected),
                            )
                            .changed()
                        {
                            if selected {
                                self.selected_targets.insert(image.name.clone());
                            } else {
                                self.selected_targets.remove(&image.name);
                            }
                        }

                        show_board_status(
                            ui,
                            self.board_statuses
                                .get(&image.name)
                                .unwrap_or(&BoardUpdateStatus::Idle),
                        );
                        ui.end_row();
                    }
                });

            let selected_images: Vec<_> = images
                .iter()
                .filter(|image| {
                    self.selected_targets.contains(&image.name)
                        && self.capability_state(&image.name, now) == CapabilityState::Available
                        && active_bus == Some(image.bus)
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
            {
                if let Some(active_bus) = active_bus {
                    if ui_to_can_tx
                        .send(messages::MsgFromUi::StartFirmwareUpdate(
                            FirmwarePackage {
                                images: selected_images.clone(),
                            },
                            active_bus,
                        ))
                        .is_ok()
                    {
                        self.begin_run(&images, &selected_images);
                        self.running = true;
                        self.status = "Starting update...".to_string();
                    }
                }
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

        egui_tiles::UiResponse::None
    }

    fn begin_run(
        &mut self,
        package_images: &[crate::bootloader_protocol::FirmwareImage],
        selected_images: &[crate::bootloader_protocol::FirmwareImage],
    ) {
        self.board_statuses = package_images
            .iter()
            .map(|image| (image.name.clone(), BoardUpdateStatus::Idle))
            .collect();
        self.run_board_names = selected_images
            .iter()
            .map(|image| image.name.clone())
            .collect();
        for image in selected_images {
            self.board_statuses
                .insert(image.name.clone(), BoardUpdateStatus::Pending);
        }
    }

    fn capability_state(&self, target: &str, now: Instant) -> CapabilityState {
        capability_state(self.observations.get(target), now, CAPABILITY_TIMEOUT)
    }

    fn hash_label(&self, target: &str, application: bool, now: Instant) -> String {
        let observation = self.observations.get(target).and_then(|observations| {
            if application {
                observations.application
            } else {
                observations.bootloader
            }
        });
        let Some(observation) = observation else {
            return "—".to_string();
        };
        let Some(git_hash) = observation.git_hash else {
            return "—".to_string();
        };
        let suffix = if now.duration_since(observation.last_seen) > CAPABILITY_TIMEOUT {
            " (stale)"
        } else {
            ""
        };
        format!("0x{git_hash:08X}{suffix}")
    }

    pub fn handle_can_message(&mut self, msg: &messages::MsgFromCan) {
        if let messages::MsgFromCan::ParsedMessage(parsed) = msg {
            let (target, application) = match parsed.decoded.name.as_str() {
                "main_version" => ("main_module", true),
                "dash_version" => ("dashboard", true),
                "torque_vector_version" => ("torque_vector", true),
                "abox_version" => ("a_box", true),
                "front_driveline_version" => ("front_driveline", true),
                "rear_driveline_version" => ("rear_driveline", true),
                "bl_main_module_info" => ("main_module", false),
                "bl_dashboard_info" => ("dashboard", false),
                "bl_torque_vector_info" => ("torque_vector", false),
                "bl_a_box_info" => ("a_box", false),
                "bl_front_driveline_info" => ("front_driveline", false),
                "bl_rear_driveline_info" => ("rear_driveline", false),
                _ => return,
            };

            let git_hash = parsed
                .decoded
                .signals
                .get("git_hash")
                .map(|signal| signal.value.physical.round() as u32);
            let bootloadable = parsed
                .decoded
                .signals
                .get(if application { "bootloadable" } else { "flags" })
                .map(|signal| {
                    let raw_value = signal.value.physical.round() as u32;
                    if application {
                        raw_value != 0
                    } else {
                        (raw_value & 1) != 0
                    }
                });
            if git_hash.is_none() && bootloadable.is_none() {
                return;
            }

            let observations = self.observations.entry(target.to_string()).or_default();
            let previous = if application {
                observations.application
            } else {
                observations.bootloader
            };
            let observation = TelemetryObservation {
                git_hash: git_hash.or_else(|| previous.and_then(|previous| previous.git_hash)),
                bootloadable: bootloadable
                    .or_else(|| previous.and_then(|previous| previous.bootloadable)),
                last_seen: Instant::now(),
            };
            if application {
                observations.application = Some(observation);
            } else {
                observations.bootloader = Some(observation);
            }
            if self.capability_state(target, Instant::now()) == CapabilityState::NotBootloadable {
                self.selected_targets.remove(target);
            }
            return;
        }

        let messages::MsgFromCan::FirmwareProgress(progress) = msg else {
            return;
        };
        apply_progress(&mut self.board_statuses, &self.run_board_names, progress);
        self.running = progress.error.is_none() && progress.phase != "complete";
        self.status = if let Some(error) = &progress.error {
            format!("Update failed: {error}")
        } else {
            progress.phase.clone()
        };
    }
}

fn capability_state(
    observations: Option<&TargetObservations>,
    now: Instant,
    timeout: Duration,
) -> CapabilityState {
    let Some(observations) = observations else {
        return CapabilityState::NoRecentTelemetry;
    };
    let recent_application = observations
        .application
        .filter(|observation| now.duration_since(observation.last_seen) <= timeout);
    let recent_bootloader = observations
        .bootloader
        .filter(|observation| now.duration_since(observation.last_seen) <= timeout);
    let Some(observation) = recent_application.or(recent_bootloader) else {
        return CapabilityState::NoRecentTelemetry;
    };
    if observation.bootloadable == Some(true) {
        CapabilityState::Available
    } else {
        CapabilityState::NotBootloadable
    }
}

fn apply_progress(
    statuses: &mut HashMap<String, BoardUpdateStatus>,
    board_names: &[String],
    progress: &messages::FirmwareProgress,
) {
    if progress.phase == "complete" {
        for status in statuses.values_mut() {
            if !matches!(status, BoardUpdateStatus::Idle) {
                *status = BoardUpdateStatus::Completed;
            }
        }
        return;
    }

    if progress.board.is_empty() {
        if let Some(error) = &progress.error {
            for name in board_names {
                if matches!(statuses.get(name), Some(BoardUpdateStatus::Pending)) {
                    statuses.insert(name.clone(), BoardUpdateStatus::Failed(error.clone()));
                }
            }
        }
        return;
    }

    for name in board_names.iter().take(progress.board_index) {
        if matches!(
            statuses.get(name),
            Some(BoardUpdateStatus::Pending | BoardUpdateStatus::Uploading { .. })
        ) {
            statuses.insert(name.clone(), BoardUpdateStatus::Completed);
        }
    }

    let cancelled = progress
        .error
        .as_deref()
        .is_some_and(|error| error.contains("cancelled"));
    if let Some(status) = statuses.get_mut(&progress.board) {
        *status = if let Some(error) = &progress.error {
            if cancelled {
                BoardUpdateStatus::Cancelled
            } else {
                BoardUpdateStatus::Failed(error.clone())
            }
        } else {
            BoardUpdateStatus::Uploading {
                phase: progress.phase.clone(),
                sent_bytes: progress.sent_bytes,
                total_bytes: progress.total_bytes,
            }
        };
    }

    if progress.error.is_some() {
        for name in board_names.iter().skip(progress.board_index + 1) {
            if matches!(statuses.get(name), Some(BoardUpdateStatus::Pending)) {
                statuses.insert(name.clone(), BoardUpdateStatus::Cancelled);
            }
        }
    }
}

fn show_board_status(ui: &mut egui::Ui, status: &BoardUpdateStatus) {
    match status {
        BoardUpdateStatus::Idle => {
            ui.label("Not selected");
        }
        BoardUpdateStatus::Pending => {
            ui.label("Pending");
        }
        BoardUpdateStatus::Uploading {
            phase,
            sent_bytes,
            total_bytes,
        } => {
            ui.vertical(|ui| {
                ui.label(phase);
                let fraction = if *total_bytes == 0 {
                    0.0
                } else {
                    *sent_bytes as f32 / *total_bytes as f32
                };
                ui.add(
                    egui::ProgressBar::new(fraction.clamp(0.0, 1.0))
                        .desired_width(115.0)
                        .show_percentage(),
                );
            });
        }
        BoardUpdateStatus::Completed => {
            ui.label("Completed");
        }
        BoardUpdateStatus::Failed(error) => {
            ui.colored_label(egui::Color32::RED, format!("Failed: {error}"));
        }
        BoardUpdateStatus::Cancelled => {
            ui.label("Cancelled");
        }
    };
}
