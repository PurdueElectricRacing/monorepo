use crate::{
    action, connection, formatter, messages, settings, shortcuts, ui, util, widget_ids, widgets,
    workspace,
};

const MAX_CAN_MESSAGES_PER_UPDATE: usize = 2_048;
use eframe::egui;

const UI_SCALE_STEP: f32 = 0.2;
pub struct ParserInfo {
    pub dbc_path: std::path::PathBuf,
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
        Some(Self { dbc_path, parser })
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
    pub theme_selection: ui::theme::ThemeSelection,
    pub pixels_per_point: Option<f32>,
    pub serial_ports: Vec<serialport::SerialPortInfo>,
    pub parser: Option<ParserInfo>,
    pub can_bus_speed: connection::CanBusSpeed,
    pub can_bus: connection::CanBus,
    pub udp_port: u16,
    pub can_messages: Vec<messages::MsgFromCan>,
    pub log_folder: Option<std::path::PathBuf>,
    pub fil: settings::FilSettings,
}

impl DAQApp {
    pub fn save_settings(&self) {
        let settings = settings::Settings {
            dbc_path: self.parser.as_ref().map(|p| p.dbc_path.clone()),
            selected_source: self.selected_source.clone(),
            selected_speed: self.can_bus_speed,
            selected_bus: self.can_bus,
            udp_port: self.udp_port,
            theme: self.theme_selection,
            pixels_per_point: self.pixels_per_point,
            log_folder: self.log_folder.clone(),
            fil: self.fil.clone(),
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
            parser: ParserInfo::new_maybe(settings.dbc_path),
            can_bus_speed: settings.selected_speed,
            can_bus: settings.selected_bus,
            udp_port: settings.udp_port,
            can_messages: Vec::new(),
            log_folder: settings.log_folder,
            fil: settings.fil,
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

    /// FIL connection source from the current settings, including per-board
    /// ELF overrides and board selection. Returns `None` when no executable
    /// or network config is selected yet.
    pub fn fil_connect_source(&self) -> Option<connection::ConnectionSource> {
        let executable = self.fil.executable.clone()?;
        if self.fil.use_builder {
            return Some(connection::ConnectionSource::Fil {
                executable,
                network: std::path::PathBuf::new(),
                bus: self.fil.builder.bus.clone(),
                elf_overrides: std::collections::HashMap::new(),
                disabled_boards: Vec::new(),
                built_network: Some(self.fil.builder.clone()),
                run_options: self.fil.run_options.clone(),
            });
        }
        Some(connection::ConnectionSource::Fil {
            executable,
            network: self.fil.network.clone()?,
            bus: self.fil.bus.clone(),
            elf_overrides: self.fil.elf_overrides.clone(),
            disabled_boards: self.fil.disabled_boards.clone(),
            built_network: None,
            run_options: self.fil.run_options.clone(),
        })
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
            action::AppAction::UpdateFilConfig { fil } => {
                self.fil = fil;
                self.save_settings();
            }
            action::AppAction::ConnectFil(source) => {
                self.selected_source = Some(source);
                self.connect_can();
                self.save_settings();
            }
            action::AppAction::UpdateFilBuilder {
                use_builder,
                builder,
            } => {
                self.fil.use_builder = use_builder;
                self.fil.builder = builder;
                self.save_settings();
            }
            action::AppAction::UpdateFilAdc {
                board,
                instance,
                channel,
                value,
            } => {
                self.fil.adc_board = board;
                self.fil.adc_instance = instance;
                self.fil.adc_channel = channel;
                self.fil.adc_value = value;
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
        while self.can_messages.len() < MAX_CAN_MESSAGES_PER_UPDATE {
            let Ok(msg) = self.can_to_ui_rx.try_recv() else {
                break;
            };
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
                | messages::MsgFromCan::BusLoad { .. }
                | messages::MsgFromCan::Hil(_)
                | messages::MsgFromCan::FirmwareProgress(_)
                | messages::MsgFromCan::FilGpio { .. } => {
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
