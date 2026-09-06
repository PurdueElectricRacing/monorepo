mod action;
mod app;
mod assets;
mod can;
mod connection;
mod daq_log_parse;
mod formatter;
mod frozen;
mod hil;
mod messages;
mod settings;
mod shortcuts;
mod theme;
mod ui;
mod util;
mod widget_constructor;
mod widget_ids;
mod widgets;
mod workspace;

fn main() -> eframe::Result<()> {
    env_logger::Builder::from_default_env()
        .filter_level(log::LevelFilter::Info)
        .init();

    let (can_to_ui_tx, can_to_ui_rx) = std::sync::mpsc::channel::<messages::MsgFromCan>();
    let (ui_to_can_tx, ui_to_can_rx) = std::sync::mpsc::channel::<messages::MsgFromUi>();

    let settings = settings::Settings::load();
    // Send default DBC path to CAN thread
    if let Some(ref dbc_path) = settings.dbc_paths[0] {
        ui_to_can_tx
            .send(messages::MsgFromUi::DbcSelected {
                bus_name: messages::BusName::XCAN,
                path: dbc_path.clone(),
            })
            .expect("Failed to send DBC path to CAN thread");
    }
    if let Some(ref selected_source) = settings.selected_source {
        ui_to_can_tx
            .send(messages::MsgFromUi::Connect(selected_source.clone()))
            .expect("Failed to send connect message to CAN thread");
    }

    let log_folder = settings
        .log_folder
        .clone()
        .unwrap_or_else(|| std::path::PathBuf::from(settings::DEFAULT_LOG_FOLDER));
    let _can_thread = can::thread::start_can_thread(
        can_to_ui_tx,
        ui_to_can_rx,
        settings.selected_source.clone(),
        log_folder,
    );

    let per_img = eframe::icon_data::from_png_bytes(assets::PER_LOGO_BYTES)
        .expect("Failed to load logo image");
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
                ui_to_can_tx,
                settings,
                cc,
            )))
        }),
    )
}
