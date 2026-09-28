use crate::{action, app, assets, settings, util, widget_constructor};

pub fn select_dbc(
    app: &mut app::DAQApp,
    ui_to_can_tx: &std::sync::mpsc::Sender<daqcore::can_thread::CanThreadCommand>,
) {
    let mut dialog = rfd::FileDialog::new().add_filter("DBC Files", &["dbc"]);
    if let Some(dir) = settings::dbc_dir() {
        dialog = dialog.set_directory(dir);
    }

    if let Some(path) = dialog.pick_file()
        && let Some(parser) = app::ParserInfo::new(path.clone())
    {
        match ui_to_can_tx.send(daqcore::can_thread::CanThreadCommand::DbcSelected(path)) {
            Ok(()) => {
                app.parser = Some(parser);
                app.save_settings();
            }
            Err(error) => log::error!("Failed to submit DBC reload: {error}"),
        }
    }
}

pub fn show(app: &mut app::DAQApp, ctx: &eframe::egui::Context) {
    eframe::egui::SidePanel::left("left_sidebar")
        .resizable(true)
        .show_animated(ctx, app.is_sidebar_open, |ui| {
            ui.horizontal(|ui| {
                ui.add(
                    eframe::egui::Image::from_bytes(assets::PER_LOGO_PATH, assets::PER_LOGO_BYTES)
                        .show_loading_spinner(true)
                        .corner_radius(5),
                );
                ui.heading("Side bar");
            });
            ui.separator();

            let theme_label = format!("🎨 Theme: {}", app.theme_selection.get_name());

            if ui.button(theme_label).clicked() {
                app.toggle_theme(ui.ctx());
                app.save_settings();
            }

            ui.separator();

            if ui.button("Add CAN Viewer Table").clicked() {
                app.action_queue.push(action::AppAction::SpawnWidget(
                    widget_constructor::WidgetConstructor::ViewerTable,
                ));
            }

            if ui.button("Add FIL Control").clicked() {
                app.action_queue.push(action::AppAction::SpawnWidget(
                    widget_constructor::WidgetConstructor::FilControl,
                ));
            }

            if ui.button("Add CAN Viewer List").clicked() {
                app.action_queue.push(action::AppAction::SpawnWidget(
                    widget_constructor::WidgetConstructor::ViewerList,
                ));
            }

            if ui.button("Add Scope").clicked() {
                app.action_queue.push(action::AppAction::SpawnWidget(
                    widget_constructor::WidgetConstructor::ScopeEmpty,
                ));
            }

            if ui.button("Add Bootloader").clicked() {
                app.action_queue.push(action::AppAction::SpawnWidget(
                    widget_constructor::WidgetConstructor::Bootloader,
                ));
            }

            if ui.button("Add Log Parser").clicked() {
                app.action_queue.push(action::AppAction::SpawnWidget(
                    widget_constructor::WidgetConstructor::LogParser,
                ));
            }

            if ui.button("Add Message Sender").clicked() {
                app.action_queue.push(action::AppAction::SpawnWidget(
                    widget_constructor::WidgetConstructor::SendUi,
                ));
            }

            if ui.button("Add Bus Load").clicked() {
                app.action_queue.push(action::AppAction::SpawnWidget(
                    widget_constructor::WidgetConstructor::BusLoad,
                ));
            }

            if ui.button("Add Battery Voltage").clicked() {
                app.action_queue.push(action::AppAction::SpawnWidget(
                    widget_constructor::WidgetConstructor::BatteryVoltage,
                ));
            }

            if ui.button("Add Battery Temps").clicked() {
                app.action_queue.push(action::AppAction::SpawnWidget(
                    widget_constructor::WidgetConstructor::BatteryTemps,
                ));
            }

            if ui.button("Add G-G Plot").clicked() {
                app.action_queue.push(action::AppAction::SpawnWidget(
                    widget_constructor::WidgetConstructor::GgPlot,
                ));
            }

            if ui.button("Add GPS Plot").clicked() {
                app.action_queue.push(action::AppAction::SpawnWidget(
                    widget_constructor::WidgetConstructor::GpsPlot,
                ));
            }

            if ui.button("Add Dynamics").clicked() {
                app.action_queue.push(action::AppAction::SpawnWidget(
                    widget_constructor::WidgetConstructor::Dynamics,
                ));
            }

            if ui.button("Add Jitter").clicked() {
                app.action_queue.push(action::AppAction::SpawnWidget(
                    widget_constructor::WidgetConstructor::Jitter,
                ));
            }

            if ui.button("Add HIL").clicked() {
                app.action_queue.push(action::AppAction::SpawnWidget(
                    widget_constructor::WidgetConstructor::Hil,
                ));
            }

            ui.separator();
            ui.heading("Connection Settings");

            ui.horizontal(|ui| {
                ui.label("CAN Speed:");
                let speed_options = daqcore::connection::CanBusSpeed::options();
                let selected_speed = app.can_bus_speed;
                eframe::egui::ComboBox::from_id_salt("can_speed_combo")
                    .selected_text(selected_speed.display_name())
                    .show_ui(ui, |ui| {
                        for speed in speed_options {
                            if ui
                                .selectable_value(
                                    &mut app.can_bus_speed,
                                    speed,
                                    speed.display_name(),
                                )
                                .changed()
                            {
                                app.serial_ports = util::get_available_serial_ports();
                                app.save_settings();
                            }
                        }
                    });
            });

            ui.horizontal(|ui| {
                ui.label("UDP Port:");
                if ui
                    .add(eframe::egui::DragValue::new(&mut app.udp_port).range(1..=65535))
                    .changed()
                {
                    app.save_settings();
                }
            });

            ui.horizontal(|ui| {
                let selected_text = match &app.selected_source {
                    Some(daqcore::connection::ConnectionSource::Serial(path, speed)) => format!(
                        "Serial: {} ({} {})", path, app.can_bus.display_name(), speed.display_name()
                    ),
                    Some(daqcore::connection::ConnectionSource::Fil { .. }) => "FIL".into(),
                    Some(connection_source) => connection_source.display_name(),
                    None => "Select Source".to_string(),
                };

                eframe::egui::ComboBox::from_label("Source")
                    .selected_text(selected_text)
                    .show_ui(ui, |ui| {
                        ui.label("Serial Ports");
                        let ports: Vec<_> = app
                            .serial_ports
                            .iter()
                            .map(|p| p.port_name.clone())
                            .collect();
                        for port_name in ports {
                            let source = daqcore::connection::ConnectionSource::Serial(
                                port_name.clone(),
                                app.can_bus_speed,
                            );
                            if ui
                                .selectable_value(
                                    &mut app.selected_source,
                                    Some(source.clone()),
                                    format!("{} ({})", port_name, app.can_bus_speed.display_name()),
                                )
                                .changed()
                            {
                                app.connect_can();
                                app.save_settings();
                            }
                        }
                        ui.separator();
                        ui.label("Network");
                        let udp_source = daqcore::connection::ConnectionSource::Udp(app.udp_port);
                        if ui
                            .selectable_value(
                                &mut app.selected_source,
                                Some(udp_source.clone()),
                                format!("UDP ({})", app.udp_port),
                            )
                            .changed()
                        {
                            app.connect_can();
                            app.save_settings();
                        }
                        ui.separator();
                        ui.label("FIL Control widget");
                        if let Some(fil_source) = app.fil_connect_source() {
                            if ui
                                .selectable_value(&mut app.selected_source, Some(fil_source), "FIL")
                                .changed()
                            {
                                app.connect_can();
                                app.save_settings();
                            }
                        } else {
                            ui.add_enabled(
                                false,
                                eframe::egui::Button::new("FIL (configure in FIL Control widget)"),
                            );
                        }
                        ui.separator();
                        ui.label("Simulated");
                        let dbc_path = app.parser.as_ref().map(|p| p.dbc_path.clone());
                        let sim_sources = [
                            (true, "Simulated (connected)"),
                            (false, "Simulated (disconnected)"),
                        ];

                        for (connected, label) in sim_sources {
                            let sim_source = daqcore::connection::ConnectionSource::Simulated(
                                connected,
                                dbc_path.clone(),
                            );

                            if ui
                                .selectable_value(
                                    &mut app.selected_source,
                                    Some(sim_source.clone()),
                                    label,
                                )
                                .changed()
                            {
                                app.connect_can();
                                app.save_settings();
                            }
                        }
                        ui.separator();
                        ui.label("Development");
                        let loopback_source = daqcore::connection::ConnectionSource::Loopback;
                        if ui
                            .selectable_value(
                                &mut app.selected_source,
                                Some(loopback_source),
                                "Loopback",
                            )
                            .changed()
                        {
                            app.connect_can();
                            app.save_settings();
                        }
                    });

                if ui.button("🔄").clicked() {
                    app.serial_ports = util::get_available_serial_ports();
                }
            });

            ui.horizontal(|ui| {
                // Connection status indicator
                let (status_icon, status_color) = match &app.connection_status {
                    app::ConnectionStatus::Disconnected => {
                        ("⚪ Disconnected".to_string(), eframe::egui::Color32::GRAY)
                    }
                    app::ConnectionStatus::Connected => {
                        ("🟢 Connected".to_string(), eframe::egui::Color32::GREEN)
                    }
                    app::ConnectionStatus::Error(e) => {
                        (format!("🔴 Error: {}", e), eframe::egui::Color32::RED)
                    }
                };
                ui.label(eframe::egui::RichText::new(status_icon).color(status_color));
            });

            ui.horizontal(|ui| {
                // Clone the sender so we don’t borrow app immutably yet
                let ui_to_can_tx = app.ui_to_can_tx.clone();

                if ui.button("📁 Select DBC").clicked() {
                    select_dbc(app, &ui_to_can_tx); // mutable borrow is fine
                }

                if let Some(path) = app.parser.as_ref().map(|p| &p.dbc_path) {
                    let dbc_name = path
                        .file_name()
                        .map(|n| n.to_string_lossy())
                        .unwrap_or_else(|| path.display().to_string().into());
                    ui.label(format!("{}", dbc_name));
                } else {
                    ui.label("DBC: None selected");
                }
            });

            ui.horizontal(|ui| {
                if ui.button("Select Log Folder").clicked()
                    && let Some(path) = rfd::FileDialog::new().pick_folder()
                {
                    let command =
                        daqcore::can_thread::CanThreadCommand::UpdateLogFolder(path.clone());

                    if let Err(error) = app.ui_to_can_tx.send(command) {
                        log::error!("Failed to send log folder update: {error}");
                        app.diagnostic = Some(error.to_string());
                        return;
                    }

                    app.log_folder = Some(path);
                    app.save_settings();
                }

                let log_display = app
                    .log_folder
                    .clone()
                    .unwrap_or_else(|| std::path::PathBuf::from(settings::DEFAULT_LOG_FOLDER));

                ui.label(log_display.display().to_string());
            });

            ui.separator();

            if ui.button("Reload formatter").clicked() {
                app.value_formatter = app::load_formatter();
            }
        });
}
