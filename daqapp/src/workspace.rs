use crate::{action, app, formatter, messages, widgets};
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
                bus_parsers: &app.bus_parsers,
                ui_to_can_tx: app.ui_to_can_tx.clone(),
                formatter: &app.value_formatter,
            };
            app.tile_tree.ui(&mut behavior, ui);
        }
    });
}

struct WorkspaceTileBehavior<'a> {
    can_messages: &'a [messages::MsgFromCan],
    action_queue: &'a mut Vec<action::AppAction>,
    bus_parsers: &'a Vec<Option<app::ParserInfo>>,
    ui_to_can_tx: std::sync::mpsc::Sender<messages::MsgFromUi>,
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
            self.bus_parsers,
            self.ui_to_can_tx.clone(),
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
