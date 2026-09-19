use crate::app;
use crate::daq_log_parse;
use eframe::egui;

pub struct LogParser {
    pub title: String,
    pub logs_dir: Option<std::path::PathBuf>,
    pub output_dir: Option<std::path::PathBuf>,

    output_prefix: String,

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
        if let Some(path) = rfd::FileDialog::new()
            .add_filter("DBC Files", &["dbc"])
            .pick_file()
        {
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

        let dbc_path_bus_0 = if self.bus_0_use_override {
            match &self.bus_0_dbc {
                Some(p) => p.clone(),
                None => {
                    self.parse_text =
                        "Error: BUS 0 DBC override enabled but no file selected".to_string();
                    log::error!("{}", self.parse_text);
                    return;
                }
            }
        } else {
            match sidebar_parser {
                Some(p) => p.dbc_path.clone(),
                None => {
                    self.parse_text = "Error: No DBC selected for BUS 0 (VCAN)".to_string();
                    log::error!("{}", self.parse_text);
                    return;
                }
            }
        };
        let dbc_path_bus_1 = if self.bus_1_use_override {
            match &self.bus_1_dbc {
                Some(p) => p.clone(),
                None => {
                    self.parse_text =
                        "Error: BUS 1 DBC override enabled but no file selected".to_string();
                    log::error!("{}", self.parse_text);
                    return;
                }
            }
        } else {
            match sidebar_parser {
                Some(p) => p.dbc_path.clone(),
                None => {
                    self.parse_text = "Error: No DBC selected for BUS 1 (MCAN)".to_string();
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

        let logs_dir = logs_dir.clone();
        let output_dir = output_dir.clone();

        let (parse_to_ui_tx, parse_to_ui_rx) = std::sync::mpsc::channel::<MsgFromParserThread>();
        self.parse_to_ui_rx = Some(parse_to_ui_rx);

        std::thread::spawn(move || {
            log::info!("Using DBC: {:?} for BUS 0 (VCAN)", dbc_path_bus_0);
            log::info!("Using DBC: {:?} for BUS 1 (MCAN)", dbc_path_bus_1);
            log::info!("Parsing logs from: {}", logs_dir.display());
            log::info!("Output to: {} (prefix: {})", output_dir.display(), prefix);

            let Ok(parser_bus_0) = can_decode::Parser::from_dbc_file(&dbc_path_bus_0) else {
                log::error!(
                    "Failed to create CAN parser from DBC file for BUS 0: {:?}",
                    dbc_path_bus_0
                );
                let _ = parse_to_ui_tx.send(MsgFromParserThread::FatalExit(
                    "Failed to create CAN parser from DBC file for BUS 0".to_string(),
                ));
                return;
            };

            let Ok(parser_bus_1) = can_decode::Parser::from_dbc_file(&dbc_path_bus_1) else {
                log::error!(
                    "Failed to create CAN parser from DBC file for BUS 1: {:?}",
                    dbc_path_bus_1
                );
                let _ = parse_to_ui_tx.send(MsgFromParserThread::FatalExit(
                    "Failed to create CAN parser from DBC file for BUS 1".to_string(),
                ));
                return;
            };

            let _ = parse_to_ui_tx.send(MsgFromParserThread::Update("Parsing logs...".to_string()));

            let parsed =
                daq_log_parse::parse::parse_log_files(&logs_dir, &parser_bus_0, &parser_bus_1);
            let chunked_parsed = daq_log_parse::parse::chunk_parsed(parsed);
            let correlated_chunks = daq_log_parse::correlate::time_correlate_chunks(chunked_parsed);

            let mut table_builder = daq_log_parse::table::TableBuilder::new();
            table_builder.create_header(&parser_bus_0, "VCAN");
            table_builder.create_header(&parser_bus_1, "MCAN");
            table_builder.create_and_write_tables(&output_dir, &prefix, correlated_chunks);

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

        // ── DBC selection per bus ─────────────────────────────────────────
        ui.label("DBC Files:");

        // BUS 0 — VCAN (BUS ID bit cleared / 0 in firmware)
        ui.horizontal(|ui| {
            ui.checkbox(&mut self.bus_0_use_override, "").on_hover_text(
                "BUS 0 = VCAN (BUS ID bit cleared/0 in firmware).\n\
                     ☑ Use the DBC selected here.\n\
                     ☐ Fall back to the DBC selected in the sidebar.",
            );

            let btn = egui::Button::new("📁 BUS 0 (VCAN)");
            if ui
                .add_enabled(self.bus_0_use_override, btn)
                .on_hover_text("Select a DBC file for BUS 0 (VCAN)")
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
                        p.dbc_path
                            .file_name()
                            .map(|n| n.to_string_lossy().into_owned())
                            .unwrap_or_else(|| p.dbc_path.display().to_string())
                    ),
                    None => "None selected (sidebar)".to_string(),
                }
            };
            ui.label(label_text);
        });

        // BUS 1 — MCAN
        ui.horizontal(|ui| {
            ui.checkbox(&mut self.bus_1_use_override, "").on_hover_text(
                "BUS 1 = MCAN (BUS ID bit set/1 in firmware).\n\
                     ☑ Use the DBC selected here.\n\
                     ☐ Fall back to the DBC selected in the sidebar.",
            );

            let btn = egui::Button::new("📁 BUS 1 (MCAN)");
            if ui
                .add_enabled(self.bus_1_use_override, btn)
                .on_hover_text("Select a DBC file for BUS 1 (MCAN)")
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
                        p.dbc_path
                            .file_name()
                            .map(|n| n.to_string_lossy().into_owned())
                            .unwrap_or_else(|| p.dbc_path.display().to_string())
                    ),
                    None => "None selected (sidebar)".to_string(),
                }
            };
            ui.label(label_text);
        });

        ui.separator();

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
