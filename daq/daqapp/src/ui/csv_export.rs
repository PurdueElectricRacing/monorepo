use crate::{app, settings};
use std::{path, sync::mpsc, thread};

pub struct CsvExport {
    pub title: String,
    pub logs_dir: Option<path::PathBuf>,
    pub output_dir: Option<path::PathBuf>,
    output_prefix: String,
    database_override: Option<path::PathBuf>,
    use_override: bool,
    parse_to_ui_rx: Option<mpsc::Receiver<Result<String, String>>>,
    parse_text: String,
}

impl CsvExport {
    pub fn new(instance_num: usize) -> Self {
        Self {
            title: format!("Log Parser #{instance_num}"),
            logs_dir: None,
            output_dir: None,
            output_prefix: "out".to_owned(),
            database_override: None,
            use_override: false,
            parse_to_ui_rx: None,
            parse_text: String::new(),
        }
    }

    fn parse_logs(&mut self, sidebar_database: Option<&app::DatabaseSelection>) {
        let Some(logs_dir) = self.logs_dir.clone() else {
            self.parse_text = "Error: Logs directory not selected".to_owned();
            return;
        };
        let Some(output_dir) = self.output_dir.clone() else {
            self.parse_text = "Error: Output directory not selected".to_owned();
            return;
        };
        let path = if self.use_override {
            self.database_override.clone()
        } else {
            sidebar_database
                .and_then(|selection| selection.source())
                .map(|source| source.path)
        };
        let Some(path) = path else {
            self.parse_text = "Error: No SuperDBC file selected".to_owned();
            return;
        };
        let prefix = match self.output_prefix.trim() {
            "" => "out",
            prefix => prefix,
        }
        .to_owned();
        let (tx, rx) = mpsc::channel();
        self.parse_to_ui_rx = Some(rx);
        self.parse_text = "Parsing logs…".to_owned();
        thread::spawn(move || {
            let result = daqcore::superdbc::database::Database::load(&path)
                .map_err(|error| error.to_string())
                .and_then(|database| {
                    daqcore::log_parse::csv_export::parse_logs_to_tables(
                        &logs_dir,
                        &output_dir,
                        &prefix,
                        &database,
                    )
                    .map_err(|error| error.to_string())
                })
                .map(|()| {
                    format!(
                        "Parsing completed successfully. Output at: {}",
                        output_dir.display()
                    )
                });
            let _ = tx.send(result);
        });
    }

    pub fn show(
        &mut self,
        ui: &mut eframe::egui::Ui,
        sidebar_database: Option<&app::DatabaseSelection>,
    ) -> egui_tiles::UiResponse {
        ui.heading(format!("🔧 {}", self.title));
        ui.separator();
        ui.horizontal(|ui| {
            if ui.button("📁 Select Logs Dir").clicked()
                && let Some(path) = rfd::FileDialog::new().pick_folder()
            {
                self.logs_dir = Some(path);
            }
            ui.label(self.logs_dir.as_ref().map_or_else(
                || "Logs: None selected".to_owned(),
                |path| format!("Logs: {}", path.display()),
            ));
        });
        ui.horizontal(|ui| {
            if ui.button("📁 Select Output Dir").clicked()
                && let Some(path) = rfd::FileDialog::new().pick_folder()
            {
                self.output_dir = Some(path);
            }
            ui.label(self.output_dir.as_ref().map_or_else(
                || "Output: None selected".to_owned(),
                |path| format!("Output: {}", path.display()),
            ));
        });
        ui.horizontal(|ui| {
            ui.label("Output Prefix:");
            ui.text_edit_singleline(&mut self.output_prefix);
        });
        ui.separator();
        ui.checkbox(&mut self.use_override, "Override sidebar SuperDBC file");
        ui.horizontal(|ui| {
            if ui
                .add_enabled(
                    self.use_override,
                    eframe::egui::Button::new("📁 Select SuperDBC"),
                )
                .clicked()
            {
                let mut dialog = rfd::FileDialog::new().add_filter("SuperDBC JSON", &["json"]);
                if let Some(dir) = settings::database_dir() {
                    dialog = dialog.set_directory(dir);
                }
                if let Some(path) = dialog.pick_file() {
                    self.database_override = Some(path);
                }
            }
            let path = if self.use_override {
                self.database_override.clone()
            } else {
                sidebar_database
                    .and_then(|selection| selection.source())
                    .map(|source| source.path)
            };
            ui.label(path.as_ref().map_or_else(
                || "None selected".to_owned(),
                |path| path.display().to_string(),
            ));
        });
        if ui
            .add_enabled(
                self.parse_to_ui_rx.is_none(),
                eframe::egui::Button::new("▶ Parse Logs"),
            )
            .clicked()
        {
            self.parse_logs(sidebar_database);
        }
        if let Some(rx) = &self.parse_to_ui_rx {
            match rx.try_recv() {
                Ok(result) => {
                    self.parse_text = result.unwrap_or_else(|error| format!("Error: {error}"));
                    self.parse_to_ui_rx = None;
                }
                Err(mpsc::TryRecvError::Disconnected) => {
                    self.parse_text =
                        "Error: Log parser stopped before reporting a result".to_owned();
                    self.parse_to_ui_rx = None;
                }
                Err(mpsc::TryRecvError::Empty) => {}
            }
        }
        ui.label(&self.parse_text);
        egui_tiles::UiResponse::None
    }
}
