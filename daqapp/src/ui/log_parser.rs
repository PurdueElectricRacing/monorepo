use crate::app;
use crate::messages;
use crate::daq_log_parse;
use eframe::egui;
use serde::de;

pub struct LogParser {
    pub title: String,
    pub logs_dir: Option<std::path::PathBuf>,
    pub output_dir: Option<std::path::PathBuf>,

    output_prefix: String,

    vcan_override: bool,
    mcan_override: bool,
    scan_override: bool,

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
            vcan_override: false,
            mcan_override: false,
            scan_override: false,
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

    fn parse_logs(&mut self, log_parsers: &Vec<&app::ParserInfo>) {
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

        let prefix = if self.output_prefix.trim().is_empty() {
            "out".to_string()
        } else {
            self.output_prefix.trim().to_string()
        };

        let logs_dir = logs_dir.clone();
        let output_dir = output_dir.clone();

        let (parse_to_ui_tx, parse_to_ui_rx) = std::sync::mpsc::channel::<MsgFromParserThread>();
        self.parse_to_ui_rx = Some(parse_to_ui_rx);

        let dbc_paths: Vec<std::path::PathBuf> = log_parsers
            .iter()
            .map(|parser| parser.dbc_path.clone())
            .collect();
        std::thread::spawn(move || {
            log::info!("Using DBC: {:?} for BUS 1 (VCAN)", dbc_paths[messages::BusName::VCAN as usize]);
            log::info!("Using DBC: {:?} for BUS 2 (MCAN)", dbc_paths[messages::BusName::MCAN as usize]);
            log::info!("Using DBC: {:?} for BUS 3 (SCAN)", dbc_paths[messages::BusName::SCAN as usize]);
            log::info!("Parsing logs from: {}", logs_dir.display());
            log::info!("Output to: {} (prefix: {})", output_dir.display(), prefix);

            let mut parsers = Vec::new();

            for dbc_path in dbc_paths {
                match can_decode::Parser::from_dbc_file(&dbc_path) {
                    Ok(parser) => parsers.push(parser),
                    Err(e) => {
                        log::error!("Failed to create CAN parser from DBC file: {:?}", e);
                        let _ = parse_to_ui_tx.send(MsgFromParserThread::FatalExit(
                            format!("Failed to create CAN parser from DBC file: {:?}", e),
                        ));
                        return;
                    }
                }
            }

            let _ = parse_to_ui_tx.send(MsgFromParserThread::Update("Parsing logs...".to_string()));

            let parsed = daq_log_parse::parse::parse_log_files(&logs_dir, &parsers);
            let chunked_parsed = daq_log_parse::parse::chunk_parsed(parsed);
            let correlated_chunks = daq_log_parse::correlate::time_correlate_chunks(chunked_parsed);

            let mut table_builder = daq_log_parse::table::TableBuilder::new();

            table_builder.create_header(&parsers[messages::BusName::VCAN as usize], messages::BusName::VCAN);
            table_builder.create_header(&parsers[messages::BusName::MCAN as usize], messages::BusName::MCAN);
            table_builder.create_header(&parsers[messages::BusName::SCAN as usize], messages::BusName::SCAN);

            table_builder.create_and_write_tables(&output_dir, &prefix, correlated_chunks);

            log::info!("Parsing completed successfully");
            let _ = parse_to_ui_tx.send(MsgFromParserThread::SuccessExit(format!(
                "Parsing completed successfully. Output at: {}",
                output_dir.display()
            )));
        });
    }

    fn dbc_override_button(&mut self, ui: &mut egui::Ui, bus_name: messages::BusName) {
        // mutable reference to the correct override
        let override_ref = match bus_name {
            messages::BusName::VCAN => &mut self.vcan_override,
            messages::BusName::MCAN => &mut self.mcan_override,
            messages::BusName::SCAN => &mut self.scan_override,
            _ => {
                log::error!("Invalid bus name for DBC override button: {}", bus_name);
                return;
            }
        };

        ui.horizontal(|ui| {
            ui.checkbox(override_ref, format!("{} Override", bus_name)).on_hover_text(format!(
                "BUS {} = {}.\n\
                     ☑ Use the bus specific DBC selected in the sidebar.\n\
                     ☐ Fall back to the default DBC selected in the sidebar.",
                bus_name as u8,
                bus_name
            ));
        });
    }

    pub fn show(
        &mut self,
        ui: &mut egui::Ui,
        bus_parsers: &Vec<Option<app::ParserInfo>>,
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

        // DBC selection now lives in sidebar, individual overrides are still available here
        self.dbc_override_button(ui, messages::BusName::VCAN);
        self.dbc_override_button(ui, messages::BusName::MCAN);
        self.dbc_override_button(ui, messages::BusName::SCAN);

        ui.separator();

        // Parse button
        let currently_parsing = self.parse_to_ui_rx.is_some();
        if ui
            .add_enabled(!currently_parsing, egui::Button::new("▶ Parse Logs"))
            .clicked()
        {
            // select log parsers based on overrides
            
            let default_parser = match &bus_parsers[messages::BusName::XCAN as usize] {
                Some(p) => p,
                None => {
                    self.parse_text = "Error: No default parser selected".to_string();
                    log::error!("{}", self.parse_text);
                    return egui_tiles::UiResponse::None;
                }
            };

            let log_parsers = Vec::from([
                default_parser,
                if self.vcan_override {
                    match &bus_parsers[messages::BusName::VCAN as usize] {
                        Some(p) => p,
                        None => {
                            log::error!("VCAN override selected but no parser available for VCAN, using default parser");
                            default_parser
                        }
                    }
                } else {
                    default_parser
                },
                if self.mcan_override {
                    match &bus_parsers[messages::BusName::MCAN as usize] {
                        Some(p) => p,
                        None => {
                            log::error!("MCAN override selected but no parser available for MCAN, using default parser");
                            default_parser
                        }
                    }
                } else {
                    default_parser
                },
                if self.scan_override {
                    match &bus_parsers[messages::BusName::SCAN as usize] {
                        Some(p) => p,
                        None => {
                            log::error!("SCAN override selected but no parser available for SCAN, using default parser");
                            default_parser
                        }
                    }
                } else {
                    default_parser
                }
            ]);
            self.parse_logs(&log_parsers);
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
