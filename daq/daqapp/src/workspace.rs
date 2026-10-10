use crate::{action, app, telemetry, ui, widgets};

pub fn show(app: &mut app::DAQApp, ctx: &eframe::egui::Context) {
    ui::timeline::show(app, ctx);
    app.session.evict();
    let start = app.session.timeline().start();
    let removed = app
        .bus_load_samples
        .partition_point(|s| s.timestamp < start);
    app.bus_load_samples.drain(..removed);
    let view = telemetry::TelemetryView::new(&app.session);
    eframe::egui::CentralPanel::default().show(ctx, |ui| {
        if app.tile_tree.is_empty() {
            ui.vertical_centered(|ui| {
                ui.label("No widgets in workspace yet.");
                ui.label("CMD+S to toggle the sidebar.");
                ui.label("CMD+P to toggle the command palette.");
            });
        } else {
            let mut behavior = WorkspaceTileBehavior {
                view: &view,
                bus_load: &app.bus_load_samples,
                action_queue: &mut app.action_queue,
                parser: app.parser.as_ref(),
                ui_to_can_tx: app.ui_to_can_tx.clone(),
                formatter: &app.value_formatter,
                connection_status: &app.connection_status,
            };
            app.tile_tree.ui(&mut behavior, ui);
        }
    });
}

struct WorkspaceTileBehavior<'a> {
    view: &'a telemetry::TelemetryView<'a>,
    bus_load: &'a [telemetry::BusLoadSample],
    action_queue: &'a mut Vec<action::AppAction>,
    parser: Option<&'a app::ParserInfo>,
    ui_to_can_tx: std::sync::mpsc::Sender<daqcore::can_thread::CanThreadCommand>,
    formatter: &'a Option<daqcore::formatter::Formatter>,
    connection_status: &'a app::ConnectionStatus,
}

impl egui_tiles::Behavior<widgets::Widget> for WorkspaceTileBehavior<'_> {
    fn pane_ui(
        &mut self,
        ui: &mut eframe::egui::Ui,
        _tile_id: egui_tiles::TileId,
        widget: &mut widgets::Widget,
    ) -> egui_tiles::UiResponse {
        widget.show(
            ui,
            widgets::WidgetContext {
                view: self.view,
                bus_load: self.bus_load,
                action_queue: self.action_queue,
                parser: self.parser,
                ui_to_can_tx: self.ui_to_can_tx.clone(),
                formatter: self.formatter,
                connection_status: self.connection_status,
            },
        )
    }

    fn tab_title_for_pane(&mut self, widget: &widgets::Widget) -> eframe::egui::WidgetText {
        widget.title().into()
    }

    fn tab_bar_color(&self, visuals: &eframe::egui::Visuals) -> eframe::egui::Color32 {
        visuals.window_fill
    }

    fn simplification_options(&self) -> egui_tiles::SimplificationOptions {
        egui_tiles::SimplificationOptions {
            all_panes_must_have_tabs: true,
            ..Default::default()
        }
    }

    fn is_tab_closable(
        &self,
        _tiles: &egui_tiles::Tiles<widgets::Widget>,
        _tile_id: egui_tiles::TileId,
    ) -> bool {
        true
    }
}
