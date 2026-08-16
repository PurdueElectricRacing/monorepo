use crate::widget_constructor;

pub enum AppAction {
    SpawnWidget(widget_constructor::WidgetConstructor),
    ToggleSidebar,
    ToggleCommandPalette,
    CloseActiveWidget,
    IncreaseScale,
    DecreaseScale,
}

impl AppAction {
    pub fn cmd_palette_list() -> Vec<(&'static str, widget_constructor::WidgetConstructor)> {
        vec![
            (
                "Spawn CAN Table",
                widget_constructor::WidgetConstructor::ViewerTable,
            ),
            (
                "Spawn CAN List",
                widget_constructor::WidgetConstructor::ViewerList,
            ),
            (
                "Spawn Scope",
                widget_constructor::WidgetConstructor::ScopeEmpty,
            ),
            (
                "Spawn Bootloader",
                widget_constructor::WidgetConstructor::Bootloader,
            ),
            (
                "Spawn Log Parser",
                widget_constructor::WidgetConstructor::LogParser,
            ),
            (
                "Spawn Send UI",
                widget_constructor::WidgetConstructor::SendUi,
            ),
            (
                "Spawn Bus Load",
                widget_constructor::WidgetConstructor::BusLoad,
            ),
            (
                "Spawn Battery Voltage",
                widget_constructor::WidgetConstructor::BatteryVoltage,
            ),
            (
                "Spawn Battery Temps",
                widget_constructor::WidgetConstructor::BatteryTemps,
            ),
            (
                "Spawn G-G Plot",
                widget_constructor::WidgetConstructor::GgPlot,
            ),
            (
                "Spawn GPS Plot",
                widget_constructor::WidgetConstructor::GpsPlot,
            ),
            (
                "Spawn Dynamics",
                widget_constructor::WidgetConstructor::Dynamics,
            ),
            (
                "Spawn Jitter",
                widget_constructor::WidgetConstructor::Jitter,
            ),
            ("Spawn HIL", widget_constructor::WidgetConstructor::Hil),
        ]
    }
}
