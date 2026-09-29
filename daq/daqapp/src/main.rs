mod action;
mod app;
mod messages;
mod assets;
mod fil_annotations;
mod paths;
mod settings;
mod shortcuts;
mod telemetry;
mod ui;
mod util;
mod widget_constructor;
mod widget_ids;
mod widgets;
mod workspace;

fn main() -> Result<(), Box<dyn std::error::Error>> {
    env_logger::Builder::from_default_env()
        .filter_level(log::LevelFilter::Info)
        .init();

    let settings = settings::Settings::load();
    let (can_to_ui_tx, can_to_ui_rx) = std::sync::mpsc::channel();
    let log_folder = settings
        .log_folder
        .clone()
        .unwrap_or_else(|| settings::DEFAULT_LOG_FOLDER.into());
    let hil_dir = std::env::current_dir()?.join("hil_config");

    let config = daqcore::can_thread::CanThreadConfig {
        dbc_path: settings.dbc_path.clone(),
        log_folder: Some(log_folder),
        hil_dir,
    };

    let can_thread = daqcore::can_thread::spawn_can_thread(config, can_to_ui_tx)?;

    if let Some(source) = settings.selected_source.clone() {
        let command = daqcore::can_thread::CanThreadCommand::Connect(Some(source));

        if let Err(error) = can_thread.command(command) {
            log::error!("Failed to submit initial connection: {error}");
        }
    }

    let per_img = eframe::icon_data::from_png_bytes(assets::PER_LOGO_BYTES)?;
    let native_options = eframe::NativeOptions {
        viewport: eframe::egui::ViewportBuilder::default().with_icon(per_img),
        ..Default::default()
    };

    eframe::run_native(
        "PER - DaqApp2",
        native_options,
        Box::new(|cc| {
            Ok(Box::new(app::DAQApp::new(
                can_to_ui_rx,
                can_thread,
                settings,
                cc,
            )?))
        }),
    )?;

    Ok(())
}
