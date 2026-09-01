use crate::{
    action, connection, formatter, messages, settings, shortcuts, theme, ui, util, widget_ids,
    widgets, workspace,
};
use eframe::egui;

const UI_SCALE_STEP: f32 = 0.2;
pub struct ParserInfo {
    pub dbc_path: std::path::PathBuf,
    pub bus_name: messages::BusName,
    pub parser: can_decode::Parser,
}

impl ParserInfo {
    // Returns None if parsing fails (missing file, invalid file, etc)
    pub fn new(dbc_path: std::path::PathBuf) -> Option<Self> {
        let parser = can_decode::Parser::from_dbc_file(&dbc_path)
            .map_err(|e| {
                log::error!("Failed to parse DBC file at {}: {}", dbc_path.display(), e);
                e
            })
            .ok()?;
        // Guess the bus name from the DBC file name
        let file_name = dbc_path.file_stem()?.to_str()?;
        let bus_name = if file_name.contains("VCAN") {
            messages::BusName::VCAN
        } else if file_name.contains("SCAN") {
            messages::BusName::SCAN
        } else if file_name.contains("MCAN") {
            messages::BusName::MCAN
        } else {
            messages::BusName::XCAN
        };
        Some(Self {
            dbc_path,
            bus_name,
            parser,
        })
    }
    pub fn new_maybe(dbc_path: Option<std::path::PathBuf>) -> Option<Self> {
        dbc_path.and_then(Self::new)
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
    pub theme_selection: theme::ThemeSelection,
    pub pixels_per_point: Option<f32>,
    pub serial_ports: Vec<serialport::SerialPortInfo>,
    pub bus_parsers: Vec<Option<ParserInfo>>, // default, VCAN, MCAN, SCAN
    pub can_bus_speed: connection::CanBusSpeed,
    pub udp_port: u16,
    pub can_messages: Vec<messages::MsgFromCan>,
    pub log_folder: Option<std::path::PathBuf>,
}

impl DAQApp {
    pub fn save_settings(&self) {
        let settings = settings::Settings {
            // vector of dbc paths for each bus, in order: default, VCAN, MCAN, SCAN
            dbc_paths: Vec::from([ 
                self.bus_parsers[0].as_ref().map(|p| p.dbc_path.clone()),
                self.bus_parsers[1].as_ref().map(|p| p.dbc_path.clone()),
                self.bus_parsers[2].as_ref().map(|p| p.dbc_path.clone()),
                self.bus_parsers[3].as_ref().map(|p| p.dbc_path.clone()),
            ]),
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

        egui_extras::install_image_loaders(&cc.egui_ctx);

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
            selected_source: settings.selected_source,
            theme: theme_style,
            theme_selection,
            pixels_per_point: settings.pixels_per_point,
            serial_ports: util::get_available_serial_ports(),
            bus_parsers: vec![
                ParserInfo::new_maybe(settings.dbc_paths[messages::BusName::XCAN as usize].clone()),
                ParserInfo::new_maybe(settings.dbc_paths[messages::BusName::VCAN as usize].clone()),
                ParserInfo::new_maybe(settings.dbc_paths[messages::BusName::MCAN as usize].clone()),
                ParserInfo::new_maybe(settings.dbc_paths[messages::BusName::SCAN as usize].clone()),
            ],
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
                let widget = widget_type.create(&mut self.widget_ids, self.ui_to_can_tx.clone());
                self.add_widget_to_tree(widget);
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

    pub fn toggle_theme(&mut self) {
        self.theme_selection = self.theme_selection.next();
        self.theme = self.theme_selection.get_style();
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
                | messages::MsgFromCan::BusLoad { .. } => {
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
