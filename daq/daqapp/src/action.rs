use crate::widget_constructor;

pub enum AppAction {
    SpawnWidget(widget_constructor::WidgetConstructor),
    ToggleSidebar,
    ToggleCommandPalette,
    CloseActiveWidget,
    IncreaseScale,
    DecreaseScale,
    UpdateFilConfig {
        executable: Option<std::path::PathBuf>,
        network: Option<std::path::PathBuf>,
        bus: String,
        elf_overrides: std::collections::HashMap<String, std::path::PathBuf>,
        disabled_boards: Vec<String>,
    },
    ConnectFil(daqcore::connection::ConnectionSource),
    UpdateFilBuilder {
        use_builder: bool,
        builder: daqcore::fil_config::BuiltNetwork,
    },
    UpdateFilAdc {
        board: String,
        instance: String,
        channel: u8,
        value: u16,
    },
}

impl AppAction {
    pub fn cmd_palette_list() -> Vec<(&'static str, widget_constructor::WidgetConstructor)> {
        vec![
            (
                "Spawn FIL Control",
                widget_constructor::WidgetConstructor::FilControl,
            ),
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
