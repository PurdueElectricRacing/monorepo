use crate::{
    action, connection, formatter, messages, settings, shortcuts, ui, util, widget_ids, widgets,
    workspace,
};
use eframe::egui;

const UI_SCALE_STEP: f32 = 0.2;
#[derive(Clone)]
pub struct ParserInfo {
    pub database_path: std::path::PathBuf,
    pub parser: daqcore::superdbc::BusDatabase,
}

impl ParserInfo {
    // Returns None if parsing fails (missing file, invalid file, etc)
    pub fn new(database_path: std::path::PathBuf) -> Option<Self> {
        let db = daqcore::superdbc::SuperDbc::load_file(&database_path)
            .map_err(|e| log::error!("Failed to load {}: {e}", database_path.display()))
            .ok()?;
        let bus = db.bus("VCAN").or_else(|| db.buses().first())?.bus_id;
        Some(Self {
            database_path,
            parser: db.bind(bus)?,
        })
    }
    pub fn new_maybe(path: Option<std::path::PathBuf>, bus: Option<u8>) -> Option<Self> {
        let path = match path {
            Some(p)
                if p.extension()
                    .and_then(|e| e.to_str())
                    .is_some_and(|e| e.eq_ignore_ascii_case("dbc")) =>
            {
                crate::settings::discover_database()?
            }
            Some(p) => p,
            None => crate::settings::discover_database()?,
        };
        let mut info = Self::new(path)?;
        if let Some(bus) = bus {
            info.parser = info
                .parser
                .database()
                .bind(daqcore::can::BusId::new(bus).ok()?)?;
        }
        Some(info)
    }
}

#[derive(Debug, PartialEq, Clone)]
pub enum ConnectionStatus {
    Disconnected,
    Connected,
    Error(String),
}

pub struct DAQApp {
    pub connection_status: ConnectionStatus,
    pub value_formatter: Option<formatter::Formatter>,
    pub is_sidebar_open: bool,
    pub command_palette: ui::command_palette::CommandPalette,
    pub tile_tree: egui_tiles::Tree<widgets::Widget>,
    pub widget_ids: widget_ids::WidgetIds,
    pub can_to_ui_rx: std::sync::mpsc::Receiver<messages::MsgFromCan>,
    pub ui_to_can_tx: std::sync::mpsc::Sender<messages::MsgFromUi>,
    pub action_queue: Vec<action::AppAction>,
    pub selected_source: Option<connection::ConnectionSource>,
    pub theme: egui::Style,
    pub theme_selection: ui::theme::ThemeSelection,
    pub pixels_per_point: Option<f32>,
    pub serial_ports: Vec<serialport::SerialPortInfo>,
    pub parser: Option<ParserInfo>,
    pub pending_database: Option<ParserInfo>,
    pub database_error: Option<String>,
    pub can_bus_speed: connection::CanBusSpeed,
    pub udp_port: u16,
    pub can_messages: Vec<messages::MsgFromCan>,
    pub log_folder: Option<std::path::PathBuf>,
}

impl DAQApp {
    pub fn select_database(&mut self, info: ParserInfo) {
        if self
            .ui_to_can_tx
            .send(messages::MsgFromUi::DatabaseSelected(info.parser.clone()))
            .is_ok()
        {
            self.pending_database = Some(info);
            self.database_error = None;
        }
    }
    pub fn save_settings(&self) {
        let settings = settings::Settings {
            database_path: self.parser.as_ref().map(|p| p.database_path.clone()),
            database_bus: self.parser.as_ref().map(|p| p.parser.bus_id().raw()),
            selected_source: self.selected_source.clone(),
            selected_speed: self.can_bus_speed,
            udp_port: self.udp_port,
            theme: self.theme_selection,
            pixels_per_point: self.pixels_per_point,
            log_folder: self.log_folder.clone(),
        };
        settings.save();
    }

    pub fn new(
        can_to_ui_rx: std::sync::mpsc::Receiver<messages::MsgFromCan>,
        ui_to_can_tx: std::sync::mpsc::Sender<messages::MsgFromUi>,
        settings: settings::Settings,
        cc: &eframe::CreationContext,
    ) -> Self {
        let theme_selection = settings.theme;
        let theme_style = theme_selection.get_style();
        ui::theme::store_theme(&cc.egui_ctx, theme_selection.get_colors());

        egui_extras::install_image_loaders(&cc.egui_ctx);

        let pending_database =
            ParserInfo::new_maybe(settings.database_path.clone(), settings.database_bus);
        if let Some(info) = &pending_database {
            let _ = ui_to_can_tx.send(messages::MsgFromUi::DatabaseSelected(info.parser.clone()));
        }
        let database_error = if pending_database.is_none() && settings.database_path.is_some() {
            Some("Could not load the configured CAN database; select a SuperDBC JSON file.".into())
        } else {
            None
        };
        Self {
            connection_status: ConnectionStatus::Disconnected,
            value_formatter: formatter::Formatter::try_load(),
            is_sidebar_open: true,
            command_palette: ui::command_palette::CommandPalette::new(),
            tile_tree: egui_tiles::Tree::empty("workspace_tree"),
            widget_ids: widget_ids::WidgetIds::new(),
            can_to_ui_rx,
            ui_to_can_tx,
            action_queue: Vec::new(),
            selected_source: settings.selected_source.clone(),
            theme: theme_style,
            theme_selection,
            pixels_per_point: settings.pixels_per_point,
            serial_ports: util::get_available_serial_ports(),
            parser: None,
            pending_database,
            database_error,
            can_bus_speed: settings.selected_speed,
            udp_port: settings.udp_port,
            can_messages: Vec::new(),
            log_folder: settings.log_folder,
        }
    }

    fn add_widget_to_tree(&mut self, widget: widgets::Widget) {
        let new_tile_id = self.tile_tree.tiles.insert_pane(widget);

        // No root yet, this becomes the root
        let Some(root_id) = self.tile_tree.root else {
            self.tile_tree.root = Some(new_tile_id);
            return;
        };

        // Check if root is already a tab container
        let Some(egui_tiles::Tile::Container(egui_tiles::Container::Tabs(tabs))) =
            self.tile_tree.tiles.get_mut(root_id)
        else {
            // Root is not a tab container, create one
            let tab_container = self
                .tile_tree
                .tiles
                .insert_tab_tile(vec![root_id, new_tile_id]);
            self.tile_tree.root = Some(tab_container);
            return;
        };

        // Root is already a tab container, add to it
        tabs.add_child(new_tile_id);
        tabs.set_active(new_tile_id);
    }

    pub fn connect_can(&mut self) {
        let Some(source) = &self.selected_source else {
            return;
        };

        self.connection_status = ConnectionStatus::Disconnected;

        let _ = self
            .ui_to_can_tx
            .send(messages::MsgFromUi::Connect(source.clone()));
    }

    pub fn handle_action(&mut self, action: action::AppAction, ctx: &egui::Context) {
        match action {
            action::AppAction::SpawnWidget(widget_type) => {
                let kind = widget_type.kind();
                let existing_count = self
                    .tile_tree
                    .tiles
                    .tiles()
                    .filter(|tile| matches!(tile, egui_tiles::Tile::Pane(w) if w.kind() == kind))
                    .count();
                match widget_type.create(
                    &mut self.widget_ids,
                    self.ui_to_can_tx.clone(),
                    existing_count,
                ) {
                    Some(widget) => self.add_widget_to_tree(widget),
                    None => {
                        log::warn!("Maximum number of {:?} widgets already open", kind)
                    }
                }
            }
            action::AppAction::ToggleSidebar => {
                self.is_sidebar_open = !self.is_sidebar_open;
            }
            action::AppAction::ToggleCommandPalette => {
                self.command_palette.toggle();
            }
            action::AppAction::CloseActiveWidget => {
                self.close_active_widget();
            }
            action::AppAction::IncreaseScale => {
                let current_scale = self
                    .pixels_per_point
                    .unwrap_or_else(|| ctx.pixels_per_point());
                self.pixels_per_point = Some(current_scale + UI_SCALE_STEP);
                self.save_settings();
            }
            action::AppAction::DecreaseScale => {
                let current_scale = self
                    .pixels_per_point
                    .unwrap_or_else(|| ctx.pixels_per_point());
                self.pixels_per_point = Some(current_scale - UI_SCALE_STEP);
                self.save_settings();
            }
        }
    }

    pub fn toggle_theme(&mut self, ctx: &egui::Context) {
        self.theme_selection = self.theme_selection.next();
        self.theme = self.theme_selection.get_style();
        ui::theme::store_theme(ctx, self.theme_selection.get_colors());
    }

    // Close the currently active widget in the tile tree
    pub fn close_active_widget(&mut self) {
        let active_tiles = self.tile_tree.active_tiles();

        for tile_id in active_tiles {
            if let Some(egui_tiles::Tile::Pane(_)) = self.tile_tree.tiles.get(tile_id) {
                self.tile_tree.tiles.remove(tile_id);
                break;
            }
        }
    }
}

impl eframe::App for DAQApp {
    fn update(&mut self, ctx: &egui::Context, _: &mut eframe::Frame) {
        self.can_messages.clear();
        while let Ok(msg) = self.can_to_ui_rx.try_recv() {
            match &msg {
                messages::MsgFromCan::DatabaseActivated { generation, bus } => {
                    if self.pending_database.as_ref().is_some_and(|p| {
                        p.parser.database().generation() == *generation && p.parser.bus_id() == *bus
                    }) {
                        self.parser = self.pending_database.take();
                        self.database_error = None;
                        self.can_messages.clear();
                        for (_, tile) in self.tile_tree.tiles.iter_mut() {
                            if let egui_tiles::Tile::Pane(widget) = tile {
                                widget.reset_database(self.ui_to_can_tx.clone());
                            }
                        }
                        self.save_settings();
                    }
                }
                messages::MsgFromCan::ConnectionFailed(port) => {
                    self.connection_status =
                        ConnectionStatus::Error(format!("Failed to connect to {port}"));
                }
                messages::MsgFromCan::ConnectionSuccessful => {
                    self.connection_status = ConnectionStatus::Connected;
                }
                messages::MsgFromCan::Disconnection => {
                    self.connection_status = ConnectionStatus::Disconnected;
                }
                messages::MsgFromCan::ParsedMessage(_)
                | messages::MsgFromCan::UnparsedMessage(_)
                | messages::MsgFromCan::MessageSent { .. }
                | messages::MsgFromCan::BusLoad { .. }
                | messages::MsgFromCan::Hil(_)
                | messages::MsgFromCan::FirmwareProgress(_) => {
                    // Nothing special to do here, the message will be handled
                    // in the individual widgets
                }
            }
            self.can_messages.push(msg);
        }
        if let Some(ppp) = self.pixels_per_point {
            ctx.set_pixels_per_point(ppp);
        }
        ctx.set_style(self.theme.clone());

        // Handle keyboard shortcuts
        self.action_queue
            .extend(shortcuts::ShortcutHandler::check_shortcuts(ctx));

        // Command Palette UI and action generation
        self.action_queue.extend(self.command_palette.ui(ctx));

        // Drain the action queue and handle all actions
        for action in std::mem::take(&mut self.action_queue) {
            self.handle_action(action, ctx);
        }

        // Render the most recent state of the UI
        ui::sidebar::show(self, ctx);
        workspace::show(self, ctx);
        ctx.request_repaint();
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    #[test]
    fn persisted_bus_uses_schema_id_and_explicit_load_failures_do_not_fall_back() {
        let path = std::path::Path::new(env!("CARGO_MANIFEST_DIR"))
            .join("../daqcore/tests/fixtures/superdbc.json");
        let info = ParserInfo::new_maybe(Some(path.clone()), Some(2)).unwrap();
        assert_eq!(info.parser.bus().name, "CCAN");
        assert_eq!(info.parser.bus_id().raw(), 2);
        assert!(ParserInfo::new_maybe(Some(path.clone()), Some(7)).is_none());
        assert!(ParserInfo::new_maybe(Some(path), Some(8)).is_none());
        let missing =
            std::env::temp_dir().join(format!("superdbc-missing-{}.json", std::process::id()));
        assert!(ParserInfo::new_maybe(Some(missing), None).is_none());
    }

    #[test]
    fn old_settings_key_migrates_and_missing_bus_defaults_to_none() {
        let mut old = serde_json::to_value(settings::Settings::default()).unwrap();
        let object = old.as_object_mut().unwrap();
        object.remove("database_path");
        object.remove("database_bus");
        object.insert("dbc_path".into(), serde_json::json!("legacy/VCAN.dbc"));
        let loaded: settings::Settings = serde_json::from_value(old).unwrap();
        assert_eq!(
            loaded.database_path.unwrap(),
            std::path::PathBuf::from("legacy/VCAN.dbc")
        );
        assert_eq!(loaded.database_bus, None);
    }
}
