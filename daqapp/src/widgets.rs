use crate::{action, app, formatter, messages, ui};
use eframe::egui;

pub enum Widget {
    ViewerTable(ui::viewer_table::ViewerTable),
    ViewerList(ui::viewer_list::ViewerList),
    Bootloader(ui::bootloader::Bootloader),
    Scope(ui::scope::Scope),
    LogParser(ui::log_parser::LogParser),
    SendUi(ui::send::SendUi),
    BusLoad(ui::bus_load::BusLoad),
    BatteryVoltage(ui::battery::battery_voltage::BatteryVoltage),
    BatteryTemps(ui::battery::battery_temps::BatteryTemps),
    GgPlot(ui::gg_plot::GgPlot),
    GpsPlot(ui::gps_plot::GpsPlot),
    Dynamics(ui::dynamics::Dynamics),
    Jitter(ui::jitter::Jitter),
    Hil(ui::hil::Hil),
}

impl Widget {
    pub fn title(&self) -> &str {
        match self {
            Widget::ViewerTable(w) => &w.title,
            Widget::ViewerList(w) => &w.title,
            Widget::Bootloader(w) => &w.title,
            Widget::Scope(w) => &w.title,
            Widget::LogParser(w) => &w.title,
            Widget::SendUi(w) => &w.title,
            Widget::BusLoad(w) => &w.title,
            Widget::BatteryVoltage(w) => &w.title,
            Widget::BatteryTemps(w) => &w.title,
            Widget::GgPlot(w) => &w.title,
            Widget::GpsPlot(w) => &w.title,
            Widget::Dynamics(w) => &w.title,
            Widget::Jitter(w) => &w.title,
            Widget::Hil(w) => &w.title,
        }
    }

    pub fn show(
        &mut self,
        ui: &mut egui::Ui,
        can_messages: &[messages::MsgFromCan],
        action_queue: &mut Vec<action::AppAction>,
        parser: Option<&app::ParserInfo>,
        _ui_to_can_tx: std::sync::mpsc::Sender<messages::MsgFromUi>,
        formatter: &Option<formatter::Formatter>,
    ) -> egui_tiles::UiResponse {
        let mut received_new_data = false;

        for msg in can_messages {
            self.handle_can_message(msg);
            received_new_data = true;
        }

        // Request repaint only if we received new data
        if received_new_data {
            ui.ctx().request_repaint();
        }

        match self {
            Widget::ViewerTable(w) => w.show(ui, action_queue, formatter, parser),
            Widget::ViewerList(w) => w.show(ui, formatter, parser),
            Widget::Bootloader(w) => w.show(ui),
            Widget::Scope(w) => w.show(ui, parser),
            Widget::LogParser(w) => w.show(ui, parser),
            Widget::SendUi(w) => w.show(ui, parser, formatter),
            Widget::BusLoad(w) => w.show(ui),
            Widget::BatteryVoltage(w) => w.show(ui),
            Widget::BatteryTemps(w) => w.show(ui),
            Widget::GgPlot(w) => w.show(ui),
            Widget::GpsPlot(w) => w.show(ui),
            Widget::Dynamics(w) => w.show(ui),
            Widget::Jitter(w) => w.show(ui, parser),
            Widget::Hil(w) => w.show(ui),
        }
    }

    fn handle_can_message(&mut self, msg: &messages::MsgFromCan) {
        match self {
            // Progress is delivered through the normal CAN->UI message path,
            // alongside decoded traffic and connection events.
            Widget::Bootloader(w) => w.handle_can_message(msg),
            Widget::ViewerTable(w) => w.handle_can_message(msg),
            Widget::ViewerList(w) => w.handle_can_message(msg),
            Widget::Scope(w) => w.handle_can_message(msg),
            Widget::SendUi(w) => w.handle_can_message(msg),
            Widget::BusLoad(w) => w.handle_can_message(msg),
            Widget::BatteryVoltage(w) => w.handle_can_message(msg),
            Widget::BatteryTemps(w) => w.handle_can_message(msg),
            Widget::GgPlot(w) => w.handle_can_message(msg),
            Widget::GpsPlot(w) => w.handle_can_message(msg),
            Widget::Dynamics(w) => w.handle_can_message(msg),
            Widget::Jitter(w) => w.handle_can_message(msg),
            Widget::Hil(w) => w.handle_can_message(msg),
            _ => {}
        }
    }
}
