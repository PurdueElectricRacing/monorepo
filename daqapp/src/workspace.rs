use crate::{action, app, connection, formatter, messages, widgets};
use eframe::egui;

pub fn show(app: &mut app::DAQApp, ctx: &egui::Context) {
    egui::CentralPanel::default().show(ctx, |ui| {
        if app.tile_tree.is_empty() {
            ui.vertical_centered(|ui| {
                ui.label("No widgets in workspace yet.");
                ui.label("CMD+S to toggle the sidebar.");
                ui.label("CMD+P to toggle the command palette.");
            });
        } else {
            let mut behavior = WorkspaceTileBehavior {
                can_messages: &app.can_messages,
                action_queue: &mut app.action_queue,
                parser: app.parser.as_ref(),
                ui_to_can_tx: app.ui_to_can_tx.clone(),
                active_bus: (app.connection_status == app::ConnectionStatus::Connected)
                    .then(|| {
                        app.selected_source
                            .as_ref()
                            .and_then(connection::ConnectionSource::can_bus)
                    })
                    .flatten(),
                formatter: &app.value_formatter,
            };
            app.tile_tree.ui(&mut behavior, ui);
        }
    });
}

struct WorkspaceTileBehavior<'a> {
    can_messages: &'a [messages::MsgFromCan],
    action_queue: &'a mut Vec<action::AppAction>,
    parser: Option<&'a app::ParserInfo>,
    ui_to_can_tx: std::sync::mpsc::Sender<messages::MsgFromUi>,
    active_bus: Option<connection::CanBus>,
    formatter: &'a Option<formatter::Formatter>,
}

impl egui_tiles::Behavior<widgets::Widget> for WorkspaceTileBehavior<'_> {
    fn pane_ui(
        &mut self,
        ui: &mut egui::Ui,
        _tile_id: egui_tiles::TileId,
        widget: &mut widgets::Widget,
    ) -> egui_tiles::UiResponse {
        widget.show(
            ui,
            self.can_messages,
            self.action_queue,
            self.parser,
            self.ui_to_can_tx.clone(),
            self.active_bus,
            self.formatter,
        )
    }

    fn tab_title_for_pane(&mut self, widget: &widgets::Widget) -> egui::WidgetText {
        widget.title().into()
    }

    fn tab_bar_color(&self, visuals: &egui::Visuals) -> egui::Color32 {
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
