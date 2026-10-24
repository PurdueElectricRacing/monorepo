use crate::{app, settings};
use std::{path, sync::mpsc, thread};

pub struct ParquetExport {
    pub title: String,
    pub logs_dir: Option<path::PathBuf>,
    pub output_dir: Option<path::PathBuf>,
    dataset_name: String,

    database_override: Option<path::PathBuf>,
    use_override: bool,

    export_result_rx: Option<mpsc::Receiver<Result<String, String>>>,
    status_text: String,
}

impl ParquetExport {
    pub fn new(instance_num: usize) -> Self {
        Self {
            title: format!("Parquet Export #{instance_num}"),
            logs_dir: None,
            output_dir: None,
            dataset_name: "dataset".to_owned(),
            database_override: None,
            use_override: false,
            export_result_rx: None,
            status_text: String::new(),
        }
    }

    // Use the same database selection for the label and the export worker.
    fn database_path(
        &self,
        sidebar_database: Option<&app::DatabaseSelection>,
    ) -> Option<path::PathBuf> {
        if self.use_override {
            self.database_override.clone()
        } else {
            sidebar_database
                .and_then(|selection| selection.source())
                .map(|source| source.path)
        }
    }

    fn start_export(&mut self, sidebar_database: Option<&app::DatabaseSelection>) {
        let Some(logs_dir) = self.logs_dir.clone() else {
            self.status_text = "Error: Logs directory not selected".to_owned();
            return;
        };

        let Some(output_dir) = self.output_dir.clone() else {
            self.status_text = "Error: Output directory not selected".to_owned();
            return;
        };

        let database_path = self.database_path(sidebar_database);
        let Some(database_path) = database_path else {
            self.status_text = "Error: No SuperDBC file selected".to_owned();
            return;
        };

        // Keep the dataset inside the selected output parent.
        let dataset_name = self.dataset_name.trim();
        let mut components = path::Path::new(dataset_name).components();
        if !matches!(components.next(), Some(path::Component::Normal(_)))
            || components.next().is_some()
        {
            self.status_text = "Error: Enter a dataset name without path separators".to_owned();
            return;
        }
        let output_dir = output_dir.join(dataset_name);

        let (tx, rx) = mpsc::channel();
        self.export_result_rx = Some(rx);
        self.status_text = "Exporting Parquet…".to_owned();

        // Loading and exporting run off the UI thread; only the result is sent back.
        thread::spawn(move || {
            let result = daqcore::superdbc::database::Database::load(&database_path)
                .map_err(|error| error.to_string())
                .and_then(|database| {
                    daqcore::log_parse::parquet_export::parse_logs_to_parquet(
                        &logs_dir,
                        &output_dir,
                        &database,
                    )
                    .map_err(|error| error.to_string())
                })
                .map(|_| {
                    format!(
                        "Parquet export completed successfully. Output at: {}",
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

        // Source logs and destination dataset.
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
            if ui.button("📁 Select Output Parent").clicked()
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
            ui.label("Dataset Name:");
            ui.text_edit_singleline(&mut self.dataset_name);
        });
        ui.label("The dataset is created inside the selected output parent.");

        ui.separator();

        // Use the sidebar database unless this widget has its own override.
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

            let path = self.database_path(sidebar_database);
            ui.label(path.as_ref().map_or_else(
                || "None selected".to_owned(),
                |path| path.display().to_string(),
            ));
        });

        if ui
            .add_enabled(
                self.export_result_rx.is_none(),
                eframe::egui::Button::new("▶ Export Parquet"),
            )
            .clicked()
        {
            self.start_export(sidebar_database);
        }

        // Poll without blocking so the widget remains responsive during export.
        if let Some(rx) = &self.export_result_rx {
            match rx.try_recv() {
                Ok(result) => {
                    self.status_text = result.unwrap_or_else(|error| format!("Error: {error}"));
                    self.export_result_rx = None;
                }
                Err(mpsc::TryRecvError::Disconnected) => {
                    self.status_text =
                        "Error: Parquet exporter stopped before reporting a result".to_owned();
                    self.export_result_rx = None;
                }
                Err(mpsc::TryRecvError::Empty) => {}
            }
        }

        ui.label(&self.status_text);

        egui_tiles::UiResponse::None
    }
}
