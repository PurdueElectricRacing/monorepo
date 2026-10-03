use crate::app;
use crate::settings;
use eframe::egui;

pub struct LogParser {
    pub title: String,
    pub logs_dir: Option<std::path::PathBuf>,
    pub output_dir: Option<std::path::PathBuf>,

    output_prefix: String,
    bus_0_name: String,
    bus_1_name: String,

    bus_0_dbc: Option<std::path::PathBuf>,
    bus_0_use_override: bool,
    bus_1_dbc: Option<std::path::PathBuf>,
    bus_1_use_override: bool,

    parse_to_ui_rx: Option<std::sync::mpsc::Receiver<MsgFromParserThread>>,
    parse_text: String,
}

enum MsgFromParserThread {
    FatalExit(String),
    SuccessExit(String),
    Update(String),
}

impl LogParser {
    pub fn new(instance_num: usize) -> Self {
        Self {
            title: format!("Log Parser #{}", instance_num),
            logs_dir: None,
            output_dir: None,
            output_prefix: "out".to_string(),
            bus_0_name: "VCAN".into(),
            bus_1_name: "MCAN".into(),
            bus_0_dbc: None,
            bus_0_use_override: false,
            bus_1_dbc: None,
            bus_1_use_override: false,
            parse_to_ui_rx: None,
            parse_text: String::new(),
        }
    }

    fn select_logs_dir(&mut self) {
        if let Some(path) = rfd::FileDialog::new().pick_folder() {
            self.logs_dir = Some(path);
        }
    }

    fn select_output_dir(&mut self) {
        if let Some(path) = rfd::FileDialog::new().pick_folder() {
            self.output_dir = Some(path);
        }
    }

    fn select_bus_dbc(current: &mut Option<std::path::PathBuf>) {
        let mut dialog = rfd::FileDialog::new().add_filter("SuperDBC JSON", &["json"]);
        if let Some(dir) = settings::database_dir() {
            dialog = dialog.set_directory(dir);
        }
        if let Some(path) = dialog.pick_file() {
            *current = Some(path);
        }
    }

    fn parse_logs(&mut self, sidebar_parser: Option<&app::ParserInfo>) {
        let logs_dir = match &self.logs_dir {
            Some(p) => p,
            None => {
                // TODO: make persistent log directories
                self.parse_text = "Error: Logs directory not selected".to_string();
                log::error!("{}", self.parse_text);
                return;
            }
        };

        let output_dir = match &self.output_dir {
            Some(p) => p,
            None => {
                self.parse_text = "Error: Output directory not selected".to_string();
                log::error!("{}", self.parse_text);
                return;
            }
        };

        let database_path_bus_0 = if self.bus_0_use_override {
            match &self.bus_0_dbc {
                Some(p) => p.clone(),
                None => {
                    self.parse_text =
                        "Error: BUS 0 database override enabled but no file selected".to_string();
                    log::error!("{}", self.parse_text);
                    return;
                }
            }
        } else {
            match sidebar_parser {
                Some(p) => p.database_path.clone(),
                None => {
                    self.parse_text = "Error: No database selected for log slot 0".to_string();
                    log::error!("{}", self.parse_text);
                    return;
                }
            }
        };
        let database_path_bus_1 = if self.bus_1_use_override {
            match &self.bus_1_dbc {
                Some(p) => p.clone(),
                None => {
                    self.parse_text =
                        "Error: BUS 1 database override enabled but no file selected".to_string();
                    log::error!("{}", self.parse_text);
                    return;
                }
            }
        } else {
            match sidebar_parser {
                Some(p) => p.database_path.clone(),
                None => {
                    self.parse_text = "Error: No database selected for log slot 1".to_string();
                    log::error!("{}", self.parse_text);
                    return;
                }
            }
        };

        let prefix = if self.output_prefix.trim().is_empty() {
            "out".to_string()
        } else {
            self.output_prefix.trim().to_string()
        };

        let bus_0_name = self.bus_0_name.clone();
        let bus_1_name = self.bus_1_name.clone();
        let logs_dir = logs_dir.clone();
        let output_dir = output_dir.clone();

        let (parse_to_ui_tx, parse_to_ui_rx) = std::sync::mpsc::channel::<MsgFromParserThread>();
        self.parse_to_ui_rx = Some(parse_to_ui_rx);

        std::thread::spawn(move || {
            log::info!(
                "Using SuperDBC: {:?} for log slot 0 ({bus_0_name})",
                database_path_bus_0
            );
            log::info!(
                "Using SuperDBC: {:?} for log slot 1 ({bus_1_name})",
                database_path_bus_1
            );
            log::info!("Parsing logs from: {}", logs_dir.display());
            log::info!("Output to: {} (prefix: {})", output_dir.display(), prefix);

            let loaded = (|| -> Result<_, String> {
                let db0 = daqcore::superdbc::SuperDbc::load_file(&database_path_bus_0)
                    .map_err(|e| e.to_string())?;
                let db1 = if database_path_bus_0 == database_path_bus_1 {
                    db0.clone()
                } else {
                    daqcore::superdbc::SuperDbc::load_file(&database_path_bus_1)
                        .map_err(|e| e.to_string())?
                };
                let bus0 = db0
                    .bus(&bus_0_name)
                    .ok_or_else(|| format!("Database has no {bus_0_name} bus"))?
                    .bus_id;
                let bus1 = db1
                    .bus(&bus_1_name)
                    .ok_or_else(|| format!("Database has no {bus_1_name} bus"))?
                    .bus_id;
                Ok((db0.bind(bus0).unwrap(), db1.bind(bus1).unwrap()))
            })();
            let (parser_bus_0, parser_bus_1) = match loaded {
                Ok(pair) => pair,
                Err(e) => {
                    let _ = parse_to_ui_tx.send(MsgFromParserThread::FatalExit(e));
                    return;
                }
            };

            let _ = parse_to_ui_tx.send(MsgFromParserThread::Update("Parsing logs...".to_string()));

            if let Err(error) = daqcore::log_parse::parse_logs_to_tables(
                &logs_dir,
                &output_dir,
                &prefix,
                &parser_bus_0,
                &bus_0_name,
                &parser_bus_1,
                &bus_1_name,
            ) {
                let _ = parse_to_ui_tx.send(MsgFromParserThread::FatalExit(error.to_string()));
                return;
            }

            log::info!("Parsing completed successfully");
            let _ = parse_to_ui_tx.send(MsgFromParserThread::SuccessExit(format!(
                "Parsing completed successfully. Output at: {}",
                output_dir.display()
            )));
        });
    }

    pub fn show(
        &mut self,
        ui: &mut egui::Ui,
        sidebar_parser: Option<&app::ParserInfo>,
    ) -> egui_tiles::UiResponse {
        ui.heading(format!("🔧 {}", self.title));
        ui.separator();

        // Log directory selection
        ui.horizontal(|ui| {
            if ui.button("📁 Select Logs Dir").clicked() {
                self.select_logs_dir();
            }
            match &self.logs_dir {
                Some(p) => ui.label(format!("Logs: {}", p.display())),
                None => ui.label("Logs: None selected"),
            };
        });

        ui.separator();

        // Output directory selection
        ui.horizontal(|ui| {
            if ui.button("📁 Select Output Dir").clicked() {
                self.select_output_dir();
            }
            match &self.output_dir {
                Some(p) => ui.label(format!("Output: {}", p.display())),
                None => ui.label("Output: None selected"),
            };
        });

        // Prefix for output files
        ui.horizontal(|ui| {
            ui.label("Output Prefix:");
            ui.text_edit_singleline(&mut self.output_prefix);
        });

        ui.separator();

        // ── database selection per bus ─────────────────────────────────────────
        ui.label("CAN databases:");

        // BUS 0 — VCAN (BUS ID bit cleared / 0 in firmware)
        ui.horizontal(|ui| {
            ui.checkbox(&mut self.bus_0_use_override, "").on_hover_text(
                "Log slot 0 (normally VCAN).\n\
                     ☑ Use the database selected here.\n\
                     ☐ Fall back to the database selected in the sidebar.",
            );

            let btn = egui::Button::new("📁 Log slot 0");
            if ui
                .add_enabled(self.bus_0_use_override, btn)
                .on_hover_text("Select a database file for log slot 0")
                .clicked()
            {
                Self::select_bus_dbc(&mut self.bus_0_dbc);
            }

            let label_text = if self.bus_0_use_override {
                match &self.bus_0_dbc {
                    Some(p) => p
                        .file_name()
                        .map(|n| n.to_string_lossy().into_owned())
                        .unwrap_or_else(|| p.display().to_string()),
                    None => "None selected".to_string(),
                }
            } else {
                match sidebar_parser {
                    Some(p) => format!(
                        "{} (sidebar)",
                        p.database_path
                            .file_name()
                            .map(|n| n.to_string_lossy().into_owned())
                            .unwrap_or_else(|| p.database_path.display().to_string())
                    ),
                    None => "None selected (sidebar)".to_string(),
                }
            };
            ui.label(label_text);
        });

        // BUS 1 — MCAN
        ui.horizontal(|ui| {
            ui.checkbox(&mut self.bus_1_use_override, "").on_hover_text(
                "Log slot 1 (normally MCAN).\n\
                     ☑ Use the database selected here.\n\
                     ☐ Fall back to the database selected in the sidebar.",
            );

            let btn = egui::Button::new("📁 Log slot 1");
            if ui
                .add_enabled(self.bus_1_use_override, btn)
                .on_hover_text("Select a database file for log slot 1")
                .clicked()
            {
                Self::select_bus_dbc(&mut self.bus_1_dbc);
            }

            let label_text = if self.bus_1_use_override {
                match &self.bus_1_dbc {
                    Some(p) => p
                        .file_name()
                        .map(|n| n.to_string_lossy().into_owned())
                        .unwrap_or_else(|| p.display().to_string()),
                    None => "None selected".to_string(),
                }
            } else {
                match sidebar_parser {
                    Some(p) => format!(
                        "{} (sidebar)",
                        p.database_path
                            .file_name()
                            .map(|n| n.to_string_lossy().into_owned())
                            .unwrap_or_else(|| p.database_path.display().to_string())
                    ),
                    None => "None selected (sidebar)".to_string(),
                }
            };
            ui.label(label_text);
        });

        ui.separator();

        ui.horizontal(|ui| {
            ui.label("Log slot 0 bus:");
            ui.text_edit_singleline(&mut self.bus_0_name);
        });
        ui.horizontal(|ui| {
            ui.label("Log slot 1 bus:");
            ui.text_edit_singleline(&mut self.bus_1_name);
        });

        // Parse button
        let currently_parsing = self.parse_to_ui_rx.is_some();
        if ui
            .add_enabled(!currently_parsing, egui::Button::new("▶ Parse Logs"))
            .clicked()
        {
            self.parse_logs(sidebar_parser);
        }

        // Parser thread messages
        if let Some(rx) = &self.parse_to_ui_rx {
            match rx.try_recv() {
                Ok(msg) => match msg {
                    MsgFromParserThread::FatalExit(text) => {
                        self.parse_text = format!("Error: {}", text);
                        self.parse_to_ui_rx = None;
                    }
                    MsgFromParserThread::SuccessExit(text) => {
                        self.parse_text = text;
                        self.parse_to_ui_rx = None;
                    }
                    MsgFromParserThread::Update(text) => {
                        self.parse_text = text;
                    }
                },
                Err(std::sync::mpsc::TryRecvError::Empty) => {}
                Err(std::sync::mpsc::TryRecvError::Disconnected) => {
                    self.parse_to_ui_rx = None;
                }
            }
        }

        ui.separator();
        ui.label(&self.parse_text);

        egui_tiles::UiResponse::None
    }
}
