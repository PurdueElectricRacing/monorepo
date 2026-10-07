use crate::{action, app, telemetry, ui, widget_constructor};

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
    FilControl(Box<ui::fil::FilControl>),
}

pub struct WidgetContext<'a> {
    pub view: &'a telemetry::TelemetryView<'a>,
    pub bus_load: &'a [telemetry::BusLoadSample],
    pub action_queue: &'a mut Vec<action::AppAction>,
    pub parser: Option<&'a app::ParserInfo>,
    pub ui_to_can_tx: std::sync::mpsc::Sender<daqcore::can_thread::CanThreadCommand>,
    pub formatter: &'a Option<daqcore::formatter::Formatter>,
    pub connection_status: &'a app::ConnectionStatus,
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
            Widget::FilControl(w) => &w.title,
        }
    }

    pub fn kind(&self) -> widget_constructor::WidgetKind {
        match self {
            Widget::ViewerTable(_) => widget_constructor::WidgetKind::ViewerTable,
            Widget::ViewerList(_) => widget_constructor::WidgetKind::ViewerList,
            Widget::Bootloader(_) => widget_constructor::WidgetKind::Bootloader,
            Widget::Scope(_) => widget_constructor::WidgetKind::Scope,
            Widget::LogParser(_) => widget_constructor::WidgetKind::LogParser,
            Widget::SendUi(_) => widget_constructor::WidgetKind::SendUi,
            Widget::BusLoad(_) => widget_constructor::WidgetKind::BusLoad,
            Widget::BatteryVoltage(_) => widget_constructor::WidgetKind::BatteryVoltage,
            Widget::BatteryTemps(_) => widget_constructor::WidgetKind::BatteryTemps,
            Widget::GgPlot(_) => widget_constructor::WidgetKind::GgPlot,
            Widget::GpsPlot(_) => widget_constructor::WidgetKind::GpsPlot,
            Widget::Dynamics(_) => widget_constructor::WidgetKind::Dynamics,
            Widget::Jitter(_) => widget_constructor::WidgetKind::Jitter,
            Widget::Hil(_) => widget_constructor::WidgetKind::Hil,
            Widget::FilControl(_) => widget_constructor::WidgetKind::FilControl,
        }
    }

    pub fn show(
        &mut self,
        ui: &mut eframe::egui::Ui,
        context: WidgetContext<'_>,
    ) -> egui_tiles::UiResponse {
        match self {
            Widget::ViewerTable(w) => w.show(
                ui,
                context.action_queue,
                context.formatter,
                context.parser,
                context.view,
            ),
            Widget::ViewerList(w) => w.show(ui, context.formatter, context.parser, context.view),
            Widget::Bootloader(w) => w.show(ui, &context.ui_to_can_tx),
            Widget::Scope(w) => w.show(ui, context.parser, context.view),
            Widget::LogParser(w) => w.show(ui, context.parser),
            Widget::SendUi(w) => w.show(ui, context.parser, context.formatter),
            Widget::BusLoad(w) => w.show(ui, context.bus_load, context.view),
            Widget::BatteryVoltage(w) => w.show(ui, context.view),
            Widget::BatteryTemps(w) => w.show(ui, context.view),
            Widget::GgPlot(w) => w.show(ui, context.view),
            Widget::GpsPlot(w) => w.show(ui, context.view),
            Widget::Dynamics(w) => w.show(ui, context.view),
            Widget::Jitter(w) => w.show(ui, context.parser, context.view),
            Widget::Hil(w) => w.show(ui),
            Widget::FilControl(w) => w.show(
                ui,
                context.action_queue,
                &context.ui_to_can_tx,
                context.connection_status,
            ),
        }
    }

    pub fn handle_operational_event(&mut self, event: &daqcore::can_thread::CanThreadEvent) {
        match self {
            Widget::Bootloader(w) => w.handle_can_message(event),
            Widget::SendUi(w) => w.handle_can_message(event),
            Widget::Hil(w) => w.handle_can_message(event),
            Widget::FilControl(w) => w.handle_can_message(event),
            _ => {}
        }
    }
}
