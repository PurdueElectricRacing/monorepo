use crate::{bootloader_protocol::FirmwarePackage, messages};
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
    Skipped,
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
    package_error: Option<String>,
}

impl Bootloader {
    pub fn new(instance_num: usize) -> Self {
        Self {
            title: format!("Bootloader #{}", instance_num),
            manifest_path: None,
            package: None,
            status: "Choose a package to begin".to_string(),
            running: false,
            observations: HashMap::new(),
            selected_targets: HashSet::new(),
            board_statuses: HashMap::new(),
            run_board_names: Vec::new(),
            package_error: None,
        }
    }

    pub fn show(
        &mut self,
        ui: &mut egui::Ui,
        ui_to_can_tx: &std::sync::mpsc::Sender<messages::MsgFromUi>,
    ) -> egui_tiles::UiResponse {
        ui.ctx().request_repaint_after(Duration::from_secs(1));

        egui::ScrollArea::vertical()
            .auto_shrink([false, false])
            .show(ui, |ui| {
                ui.heading("Firmware updates");
                ui.label(
                    egui::RichText::new(
                        "Verify a package, choose responding targets, and update them in sequence.",
                    )
                    .weak(),
                );
                ui.add_space(12.0);

                self.show_package_panel(ui);
                ui.add_space(12.0);

                if let Some(error) = &self.package_error {
                    status_banner(ui, error, ui.visuals().error_fg_color);
                    ui.add_space(12.0);
                }

                let Some(package) = &self.package else {
                    return;
                };
                let images = package.images.clone();
                let now = Instant::now();

                for image in &images {
                    if self.capability_state(&image.name, now) != CapabilityState::Available {
                        self.selected_targets.remove(&image.name);
                    }
                }

                let available_count = images
                    .iter()
                    .filter(|image| {
                        self.capability_state(&image.name, now) == CapabilityState::Available
                    })
                    .count();
                let selected_images: Vec<_> = images
                    .iter()
                    .filter(|image| self.selected_targets.contains(&image.name))
                    .cloned()
                    .collect();

                self.show_target_header(ui, &images, available_count, selected_images.len(), now);
                ui.add_space(6.0);
                self.show_targets(ui, &images, now);
                ui.add_space(12.0);

                if !self.running && !self.board_statuses.is_empty() {
                    let color = if self.status.starts_with("Update failed") {
                        ui.visuals().error_fg_color
                    } else {
                        ui.visuals().text_color()
                    };
                    status_banner(ui, &self.status, color);
                    ui.add_space(8.0);
                }

                if self.running {
                    self.show_run_summary(ui);
                    ui.add_space(8.0);
                    ui.horizontal(|ui| {
                        if ui.button("Cancel update").clicked() {
                            if ui_to_can_tx
                                .send(messages::MsgFromUi::CancelFirmwareUpdate)
                                .is_ok()
                            {
                                self.status = "Cancelling update…".to_string();
                                self.package_error = None;
                            } else {
                                self.package_error = Some(
                                    "Could not request cancellation because the CAN worker is unavailable."
                                        .to_string(),
                                );
                            }
                        }
                        ui.label(
                            egui::RichText::new(
                                "Cancelling stops after the current protocol step; it cannot undo writes.",
                            )
                            .small()
                            .weak(),
                        );
                    });
                } else {
                    ui.horizontal(|ui| {
                        let upload = ui.add_enabled(
                            !selected_images.is_empty(),
                            egui::Button::new(format!(
                                "Upload selected ({})",
                                selected_images.len()
                            )),
                        );
                        if upload.clicked() {
                            let message = messages::MsgFromUi::StartFirmwareUpdate(
                                FirmwarePackage {
                                    images: selected_images.clone(),
                                },
                            );
                            if ui_to_can_tx.send(message).is_ok() {
                                self.begin_run(&images, &selected_images);
                                self.running = true;
                                self.status = "Starting update…".to_string();
                                self.package_error = None;
                            } else {
                                self.package_error = Some(
                                    "Could not start the update because the CAN worker is unavailable."
                                        .to_string(),
                                );
                            }
                        }
                        if selected_images.is_empty() {
                            ui.label(
                                egui::RichText::new(
                                    "Select at least one available target to continue.",
                                )
                                .small()
                                .weak(),
                            );
                        }
                    });
                }
            });

        egui_tiles::UiResponse::None
    }

    fn show_package_panel(&mut self, ui: &mut egui::Ui) {
        egui::Frame::group(ui.style()).show(ui, |ui| {
            ui.set_min_width(ui.available_width());
            ui.horizontal(|ui| {
                ui.vertical(|ui| {
                    ui.strong("Firmware package");
                    if let (Some(path), Some(package)) = (&self.manifest_path, &self.package) {
                        let filename = path
                            .file_name()
                            .map(|name| name.to_string_lossy())
                            .unwrap_or_else(|| path.display().to_string().into());
                        ui.label(egui::RichText::new(filename).monospace())
                            .on_hover_text(path.display().to_string());
                        let total_bytes: usize =
                            package.images.iter().map(|image| image.bytes.len()).sum();
                        ui.label(
                            egui::RichText::new(format!(
                                "{} verified images • {}",
                                package.images.len(),
                                format_bytes(total_bytes)
                            ))
                            .small()
                            .weak(),
                        );
                    } else {
                        ui.label("Choose a manifest.json or firmware_*.tar.gz package.");
                        ui.label(
                            egui::RichText::new(
                                "DaqApp validates every image and checksum before enabling updates.",
                            )
                            .small()
                            .weak(),
                        );
                    }
                });
                ui.with_layout(egui::Layout::right_to_left(egui::Align::Center), |ui| {
                    let label = if self.package.is_some() {
                        "Change package"
                    } else {
                        "Choose package"
                    };
                    if ui.add_enabled(!self.running, egui::Button::new(label)).clicked()
                        && let Some(path) = rfd::FileDialog::new()
                            .add_filter("Firmware package", &["json", "gz"])
                            .pick_file()
                    {
                        self.load_package(path);
                    }
                });
            });
        });
    }

    fn load_package(&mut self, path: std::path::PathBuf) {
        match FirmwarePackage::load(path.clone()) {
            Ok(package) => {
                self.manifest_path = Some(path);
                self.package = Some(package);
                self.status = "Ready to update".to_string();
                self.package_error = None;
                self.selected_targets.clear();
                self.board_statuses.clear();
                self.run_board_names.clear();
            }
            Err(error) => {
                self.manifest_path = None;
                self.package = None;
                self.selected_targets.clear();
                self.board_statuses.clear();
                self.run_board_names.clear();
                self.package_error = Some(format!("Package could not be verified: {error}"));
            }
        }
    }

    fn show_target_header(
        &mut self,
        ui: &mut egui::Ui,
        images: &[crate::bootloader_protocol::FirmwareImage],
        available_count: usize,
        selected_count: usize,
        now: Instant,
    ) {
        ui.horizontal(|ui| {
            ui.strong("Targets");
            ui.label(
                egui::RichText::new(format!(
                    "{available_count} of {} available • {selected_count} selected",
                    images.len()
                ))
                .small()
                .weak(),
            );
            ui.with_layout(egui::Layout::right_to_left(egui::Align::Center), |ui| {
                if ui
                    .add_enabled(
                        !self.running && !self.selected_targets.is_empty(),
                        egui::Button::new("Clear"),
                    )
                    .clicked()
                {
                    self.selected_targets.clear();
                }
                if ui
                    .add_enabled(
                        !self.running && available_count > 0,
                        egui::Button::new("Select available"),
                    )
                    .clicked()
                {
                    self.selected_targets = images
                        .iter()
                        .filter(|image| {
                            self.capability_state(&image.name, now) == CapabilityState::Available
                        })
                        .map(|image| image.name.clone())
                        .collect();
                }
            });
        });
    }

    fn show_targets(
        &mut self,
        ui: &mut egui::Ui,
        images: &[crate::bootloader_protocol::FirmwareImage],
        now: Instant,
    ) {
        egui::Frame::group(ui.style()).show(ui, |ui| {
            egui::ScrollArea::horizontal().show(ui, |ui| {
                egui::Grid::new(egui::Id::new(("bootloader-targets", &self.title)))
                    .striped(true)
                    .num_columns(6)
                    .spacing([14.0, 8.0])
                    .show(ui, |ui| {
                        ui.strong("");
                        ui.strong("Target");
                        ui.strong("Readiness");
                        ui.strong("Application");
                        ui.strong("Bootloader");
                        ui.strong("Update");
                        ui.end_row();

                        for image in images {
                            let capability = self.capability_state(&image.name, now);
                            let available = capability == CapabilityState::Available;
                            let mut selected = self.selected_targets.contains(&image.name);
                            if ui
                                .add_enabled(
                                    !self.running && available,
                                    egui::Checkbox::without_text(&mut selected),
                                )
                                .on_hover_text(if available {
                                    "Include this target in the update"
                                } else {
                                    "A target must be available before it can be selected"
                                })
                                .changed()
                            {
                                if selected {
                                    self.selected_targets.insert(image.name.clone());
                                } else {
                                    self.selected_targets.remove(&image.name);
                                }
                            }

                            ui.strong(display_target_name(&image.name));
                            show_capability(ui, capability);
                            ui.label(
                                egui::RichText::new(self.hash_label(&image.name, true, now))
                                    .monospace(),
                            );
                            ui.label(
                                egui::RichText::new(self.hash_label(&image.name, false, now))
                                    .monospace(),
                            );
                            show_board_status(
                                ui,
                                self.board_statuses
                                    .get(&image.name)
                                    .unwrap_or(&BoardUpdateStatus::Idle),
                            );
                            ui.end_row();
                        }
                    });
            });
        });
    }

    fn show_run_summary(&self, ui: &mut egui::Ui) {
        let mut completed = 0.0;
        for name in &self.run_board_names {
            completed += match self.board_statuses.get(name) {
                Some(BoardUpdateStatus::Completed) => 1.0,
                Some(BoardUpdateStatus::Uploading {
                    sent_bytes,
                    total_bytes,
                    ..
                }) if *total_bytes > 0 => *sent_bytes as f32 / *total_bytes as f32,
                _ => 0.0,
            };
        }
        let total = self.run_board_names.len();
        let fraction = if total == 0 {
            0.0
        } else {
            completed / total as f32
        };
        ui.strong(&self.status);
        ui.add(
            egui::ProgressBar::new(fraction.clamp(0.0, 1.0))
                .text(format!("{completed:.1} of {total} targets"))
                .animate(true),
        );
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
            let resident_git_hash = application
                .then(|| parsed.decoded.signals.get("bootloader_git_hash"))
                .flatten()
                .map(|signal| signal.value.physical.round() as u32)
                .filter(|hash| *hash != 0);
            if git_hash.is_none() && resident_git_hash.is_none() && bootloadable.is_none() {
                return;
            }

            let observations = self.observations.entry(target.to_string()).or_default();
            let previous = if application {
                observations.application
            } else {
                observations.bootloader
            };
            let now = Instant::now();
            let observation = TelemetryObservation {
                git_hash: git_hash.or_else(|| previous.and_then(|previous| previous.git_hash)),
                bootloadable: bootloadable
                    .or_else(|| previous.and_then(|previous| previous.bootloadable)),
                last_seen: now,
            };
            if application {
                observations.application = Some(observation);
                if let Some(resident_git_hash) = resident_git_hash {
                    observations.bootloader = Some(TelemetryObservation {
                        git_hash: Some(resident_git_hash),
                        bootloadable,
                        last_seen: now,
                    });
                }
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
            if error.contains("cancelled") {
                "Update cancelled".to_string()
            } else {
                format!("Update failed: {error}")
            }
        } else if progress.phase == "complete" {
            "Update complete".to_string()
        } else {
            progress.phase.clone()
        };
    }
}

fn status_banner(ui: &mut egui::Ui, message: &str, color: egui::Color32) {
    egui::Frame::group(ui.style())
        .stroke(egui::Stroke::new(1.0_f32, color))
        .show(ui, |ui| {
            ui.set_min_width(ui.available_width());
            ui.colored_label(color, message);
        });
}

fn show_capability(ui: &mut egui::Ui, capability: CapabilityState) {
    match capability {
        CapabilityState::Available => {
            ui.label("Available");
        }
        CapabilityState::NotBootloadable => {
            ui.colored_label(ui.visuals().warn_fg_color, "Not bootloadable")
                .on_hover_text("The target is responding but does not advertise update support.");
        }
        CapabilityState::NoRecentTelemetry => {
            ui.label(egui::RichText::new("Waiting for telemetry").weak())
                .on_hover_text("No target telemetry has been received in the last 12 seconds.");
        }
    }
}

fn display_target_name(name: &str) -> String {
    name.split('_')
        .map(|part| {
            let mut chars = part.chars();
            match chars.next() {
                Some(first) => first.to_uppercase().chain(chars).collect(),
                None => String::new(),
            }
        })
        .collect::<Vec<_>>()
        .join(" ")
}

fn format_bytes(bytes: usize) -> String {
    if bytes >= 1024 * 1024 {
        format!("{:.1} MiB", bytes as f64 / (1024.0 * 1024.0))
    } else if bytes >= 1024 {
        format!("{:.1} KiB", bytes as f64 / 1024.0)
    } else {
        format!("{bytes} B")
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
        let trailing_status = if cancelled {
            BoardUpdateStatus::Cancelled
        } else {
            BoardUpdateStatus::Skipped
        };
        for name in board_names.iter().skip(progress.board_index + 1) {
            if matches!(statuses.get(name), Some(BoardUpdateStatus::Pending)) {
                statuses.insert(name.clone(), trailing_status.clone());
            }
        }
    }
}

fn show_board_status(ui: &mut egui::Ui, status: &BoardUpdateStatus) {
    match status {
        BoardUpdateStatus::Idle => {
            ui.label(egui::RichText::new("Not selected").weak());
        }
        BoardUpdateStatus::Pending => {
            ui.label("Queued");
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
            ui.strong("Complete");
        }
        BoardUpdateStatus::Failed(error) => {
            ui.colored_label(ui.visuals().error_fg_color, "Failed")
                .on_hover_text(error);
        }
        BoardUpdateStatus::Cancelled => {
            ui.colored_label(ui.visuals().warn_fg_color, "Cancelled");
        }
        BoardUpdateStatus::Skipped => {
            ui.label(egui::RichText::new("Skipped").weak());
        }
    };
}
