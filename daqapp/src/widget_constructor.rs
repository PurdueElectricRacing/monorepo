use crate::{messages, ui, widget_ids, widgets};

#[derive(Eq, PartialEq, Hash, Clone, Copy)]
pub enum WidgetKind {
    ViewerTable,
    ViewerList,
    Bootloader,
    Scope,
    LogParser,
    SendUi,
    BusLoad,
    BatteryVoltage,
    BatteryTemps,
    GgPlot,
    GpsPlot,
    Dynamics,
    Jitter,
    Hil,
}

#[derive(Eq, PartialEq, Clone)]
pub enum WidgetConstructor {
    ViewerTable,
    ViewerList,
    Bootloader,
    Scope {
        msg_id: u32,
        msg_name: String,
        signal_name: String,
    },
    ScopeEmpty,
    LogParser,
    SendUi,
    BusLoad,
    BatteryVoltage,
    BatteryTemps,
    GgPlot,
    GpsPlot,
    Dynamics,
    Jitter,
    Hil,
}

impl WidgetConstructor {
    // Maps constructor variants to widget kinds with Scope and ScopeEmpty sharing WidgetKind Scope and the same instance counter
    pub fn kind(&self) -> WidgetKind {
        match self {
            WidgetConstructor::ViewerTable => WidgetKind::ViewerTable,
            WidgetConstructor::ViewerList => WidgetKind::ViewerList,
            WidgetConstructor::Bootloader => WidgetKind::Bootloader,
            WidgetConstructor::Scope { .. } | WidgetConstructor::ScopeEmpty => WidgetKind::Scope,
            WidgetConstructor::LogParser => WidgetKind::LogParser,
            WidgetConstructor::SendUi => WidgetKind::SendUi,
            WidgetConstructor::BusLoad => WidgetKind::BusLoad,
            WidgetConstructor::BatteryVoltage => WidgetKind::BatteryVoltage,
            WidgetConstructor::BatteryTemps => WidgetKind::BatteryTemps,
            WidgetConstructor::GgPlot => WidgetKind::GgPlot,
            WidgetConstructor::GpsPlot => WidgetKind::GpsPlot,
            WidgetConstructor::Dynamics => WidgetKind::Dynamics,
            WidgetConstructor::Jitter => WidgetKind::Jitter,
            WidgetConstructor::Hil => WidgetKind::Hil,
        }
    }

    pub fn create(
        self,
        widget_ids: &mut widget_ids::WidgetIds,
        ui_to_can_tx: std::sync::mpsc::Sender<messages::MsgFromUi>,
    ) -> widgets::Widget {
        let id = widget_ids.next(self.kind());
        match self {
            WidgetConstructor::ViewerTable => {
                widgets::Widget::ViewerTable(ui::viewer_table::ViewerTable::new(id))
            }
            WidgetConstructor::ViewerList => {
                widgets::Widget::ViewerList(ui::viewer_list::ViewerList::new(id))
            }
            WidgetConstructor::Bootloader => {
                widgets::Widget::Bootloader(ui::bootloader::Bootloader::new(id))
            }
            WidgetConstructor::Scope {
                msg_id,
                msg_name,
                signal_name,
            } => widgets::Widget::Scope(ui::scope::Scope::new(id, msg_id, msg_name, signal_name)),
            WidgetConstructor::ScopeEmpty => {
                widgets::Widget::Scope(ui::scope::Scope::new_empty(id))
            }
            WidgetConstructor::LogParser => {
                widgets::Widget::LogParser(ui::log_parser::LogParser::new(id))
            }
            WidgetConstructor::SendUi => {
                widgets::Widget::SendUi(ui::send::SendUi::new(id, ui_to_can_tx))
            }
            WidgetConstructor::BusLoad => widgets::Widget::BusLoad(ui::bus_load::BusLoad::new(id)),
            WidgetConstructor::BatteryVoltage => widgets::Widget::BatteryVoltage(
                ui::battery::battery_voltage::BatteryVoltage::new(id),
            ),
            WidgetConstructor::BatteryTemps => {
                widgets::Widget::BatteryTemps(ui::battery::battery_temps::BatteryTemps::new(id))
            }
            WidgetConstructor::GgPlot => widgets::Widget::GgPlot(ui::gg_plot::GgPlot::new(id)),
            WidgetConstructor::GpsPlot => widgets::Widget::GpsPlot(ui::gps_plot::GpsPlot::new(id)),
            WidgetConstructor::Dynamics => {
                widgets::Widget::Dynamics(ui::dynamics::Dynamics::new(id))
            }
            WidgetConstructor::Jitter => widgets::Widget::Jitter(ui::jitter::Jitter::new(id)),
            WidgetConstructor::Hil => widgets::Widget::Hil(ui::hil::Hil::new(id)),
        }
    }
}
