use crate::{
    action, messages::FilAdcInstance, paths, settings, shortcuts, telemetry, ui, util, widget_ids,
    widgets, workspace,
};
const MAX_CAN_EVENTS_PER_UPDATE: usize = 2_048;

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
    pub value_formatter: Option<daqcore::formatter::Formatter>,
    pub is_sidebar_open: bool,
    pub command_palette: ui::command_palette::CommandPalette,
    pub tile_tree: egui_tiles::Tree<widgets::Widget>,
    pub widget_ids: widget_ids::WidgetIds,
    pub can_to_ui_rx: std::sync::mpsc::Receiver<daqcore::can_thread::CanThreadEvent>,
    pub ui_to_can_tx: std::sync::mpsc::Sender<daqcore::can_thread::CanThreadCommand>,
    pub action_queue: Vec<action::AppAction>,
    pub selected_source: Option<daqcore::connection::ConnectionSource>,
    pub theme: eframe::egui::Style,
    pub theme_selection: ui::theme::ThemeSelection,
    pub pixels_per_point: Option<f32>,
    pub serial_ports: Vec<serialport::SerialPortInfo>,
    pub parser: Option<ParserInfo>,
    pub can_bus_speed: daqcore::connection::CanBusSpeed,
    pub can_bus: daqcore::connection::CanBus,
    pub udp_port: u16,
    pub session: daqcore::Session,
    pub bus_load_samples: Vec<telemetry::BusLoadSample>,
    pub can_thread: daqcore::can_thread::CanThreadHandle,
    hil_snapshot: daqcore::hil::engine::HilSnapshot,
    active_source: Option<daqcore::connection::ConnectionSource>,
    pub diagnostic: Option<String>,
    pub log_folder: Option<std::path::PathBuf>,
    pub fil_executable: Option<std::path::PathBuf>,
    pub fil_network_config: Option<std::path::PathBuf>,
    pub fil_bus: String,
    pub fil_elf_overrides: std::collections::HashMap<String, std::path::PathBuf>,
    pub fil_disabled_boards: Vec<String>,
    pub fil_use_builder: bool,
    pub fil_builder: daqcore::fil_config::BuiltNetwork,
    pub fil_adc_board: String,
    pub fil_adc_instance: FilAdcInstance,
    pub fil_adc_channel: u8,
    pub fil_adc_value: u16,
    pub fil_run_options: settings::FilRunOptions,
    pub fil_trace_bus: Option<String>,
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
            window_secs: self.session.timeline().window_secs(),
            fil: settings::FilSettings {
                executable: self.fil_executable.clone(),
                network: self.fil_network_config.clone(),
                bus: self.fil_bus.clone(),
                elf_overrides: self.fil_elf_overrides.clone(),
                disabled_boards: self.fil_disabled_boards.clone(),
                use_builder: self.fil_use_builder,
                builder: self.fil_builder.clone(),
                adc_board: self.fil_adc_board.clone(),
                adc_instance: self.fil_adc_instance,
                adc_channel: self.fil_adc_channel,
                adc_value: self.fil_adc_value,
                run_options: self.fil_run_options.clone(),
                trace_bus: self.fil_trace_bus.clone(),
            },
        };
        settings.save();
    }

    pub fn new(
        can_to_ui_rx: std::sync::mpsc::Receiver<daqcore::can_thread::CanThreadEvent>,
        can_thread: daqcore::can_thread::CanThreadHandle,
        settings: settings::Settings,
        cc: &eframe::CreationContext,
    ) -> Result<Self, Box<dyn std::error::Error + Send + Sync>> {
        let theme_selection = settings.theme;
        let theme_style = theme_selection.get_style();
        ui::theme::store_theme(&cc.egui_ctx, theme_selection.get_colors());

        egui_extras::install_image_loaders(&cc.egui_ctx);

        let window_secs = if settings.window_secs.is_finite() && settings.window_secs >= 0.0 {
            settings.window_secs
        } else {
            30.0
        };

        let session = daqcore::Session::live(daqcore::Time::now(), window_secs)?;

        let fil = settings.fil;
        Ok(Self {
            connection_status: ConnectionStatus::Disconnected,
            value_formatter: load_formatter(),
            is_sidebar_open: true,
            command_palette: ui::command_palette::CommandPalette::new(),
            tile_tree: egui_tiles::Tree::empty("workspace_tree"),
            widget_ids: widget_ids::WidgetIds::new(),
            can_to_ui_rx,
            ui_to_can_tx: can_thread.sender(),
            can_thread,
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
            session,
            bus_load_samples: Vec::new(),
            hil_snapshot: daqcore::hil::engine::HilSnapshot::idle(),
            active_source: None,
            diagnostic: None,
            log_folder: settings.log_folder,
            fil_executable: fil.executable,
            fil_network_config: fil.network,
            fil_elf_overrides: fil.elf_overrides,
            fil_disabled_boards: fil.disabled_boards,
            fil_use_builder: fil.use_builder,
            fil_builder: fil.builder,
            fil_bus: fil.bus,
            fil_adc_board: fil.adc_board,
            fil_adc_instance: fil.adc_instance,
            fil_adc_channel: fil.adc_channel.min(19),
            fil_adc_value: fil.adc_value.min(4095),
            fil_run_options: fil.run_options,
            fil_trace_bus: fil.trace_bus,
        })
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
    pub fn fil_connect_source(&self) -> Option<daqcore::connection::ConnectionSource> {
        let executable = self.fil_executable.clone()?;
        if self.fil_use_builder {
            return Some(daqcore::connection::ConnectionSource::Fil {
                executable,
                network: std::path::PathBuf::new(),
                bus: self.fil_builder.bus.clone(),
                trace_bus: self.fil_trace_bus.clone(),
                elf_overrides: std::collections::HashMap::new(),
                disabled_boards: Vec::new(),
                built_network: Some(self.fil_builder.clone()),
                run_options: self.fil_run_options.clone(),
            });
        }
        Some(daqcore::connection::ConnectionSource::Fil {
            executable,
            network: self.fil_network_config.clone()?,
            bus: self.fil_bus.clone(),
            trace_bus: self.fil_trace_bus.clone(),
            elf_overrides: self.fil_elf_overrides.clone(),
            disabled_boards: self.fil_disabled_boards.clone(),
            built_network: None,
            run_options: self.fil_run_options.clone(),
        })
    }

    pub fn connect_can(&mut self) {
        let Some(source) = &self.selected_source else {
            return;
        };

        self.connection_status = ConnectionStatus::Disconnected;

        let _ = self
            .ui_to_can_tx
            .send(daqcore::can_thread::CanThreadCommand::Connect(Some(
                source.clone(),
            )));
    }

    pub fn handle_action(&mut self, action: action::AppAction, ctx: &eframe::egui::Context) {
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
                    Some(mut widget) => {
                        widget.handle_operational_event(&daqcore::can_thread::CanThreadEvent::Hil(
                            self.hil_snapshot.clone(),
                        ));
                        self.add_widget_to_tree(widget);
                    }
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
                self.fil_executable = fil.executable;
                self.fil_network_config = fil.network;
                self.fil_bus = fil.bus;
                self.fil_elf_overrides = fil.elf_overrides;
                self.fil_disabled_boards = fil.disabled_boards;
                self.fil_use_builder = fil.use_builder;
                self.fil_builder = fil.builder;
                self.fil_adc_board = fil.adc_board;
                self.fil_adc_instance = fil.adc_instance;
                self.fil_adc_channel = fil.adc_channel.min(19);
                self.fil_adc_value = fil.adc_value.min(4095);
                self.fil_run_options = fil.run_options;
                self.fil_trace_bus = fil.trace_bus;
                if let Some(daqcore::connection::ConnectionSource::Fil { trace_bus, .. }) =
                    self.selected_source.as_mut()
                {
                    *trace_bus = self.fil_trace_bus.clone();
                }
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
                self.fil_use_builder = use_builder;
                self.fil_builder = builder;
                self.save_settings();
            }
            action::AppAction::UpdateFilAdc {
                board,
                instance,
                channel,
                value,
            } => {
                self.fil_adc_board = board;
                self.fil_adc_instance = instance;
                self.fil_adc_channel = channel;
                self.fil_adc_value = value;
                self.save_settings();
            }
        }
    }

    pub fn toggle_theme(&mut self, ctx: &eframe::egui::Context) {
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
    fn update(&mut self, ctx: &eframe::egui::Context, _: &mut eframe::Frame) {
        for _ in 0..MAX_CAN_EVENTS_PER_UPDATE {
            let Ok(event) = self.can_to_ui_rx.try_recv() else {
                break;
            };
            for tile in self.tile_tree.tiles.tiles_mut() {
                if let egui_tiles::Tile::Pane(widget) = tile {
                    widget.handle_operational_event(&event);
                }
            }
            match event {
                daqcore::can_thread::CanThreadEvent::Frame(frame) => {
                    self.session.ingest_frame(frame);
                }
                daqcore::can_thread::CanThreadEvent::SourceSelected(Some(source)) => {
                    if self.active_source.as_ref() != Some(&source) {
                        self.session.reset(daqcore::Time::now());
                        self.bus_load_samples.clear();
                    }
                    self.active_source = Some(source);
                }
                daqcore::can_thread::CanThreadEvent::ConnectionFailed(error) => {
                    self.connection_status = ConnectionStatus::Error(error)
                }
                daqcore::can_thread::CanThreadEvent::ConnectionSuccessful => {
                    self.connection_status = ConnectionStatus::Connected
                }
                daqcore::can_thread::CanThreadEvent::Disconnection => {
                    self.connection_status = ConnectionStatus::Disconnected;
                }
                daqcore::can_thread::CanThreadEvent::BusLoad {
                    timestamp,
                    load_1s,
                    load_5s,
                    load_10s,
                    load_30s,
                } => {
                    let sample = telemetry::BusLoadSample {
                        timestamp,
                        values: [load_1s, load_5s, load_10s, load_30s],
                    };

                    let index = self
                        .bus_load_samples
                        .partition_point(|s| s.timestamp <= timestamp);
                    self.bus_load_samples.insert(index, sample);
                }
                daqcore::can_thread::CanThreadEvent::Hil(snapshot) => self.hil_snapshot = snapshot,
                daqcore::can_thread::CanThreadEvent::Diagnostic(error)
                | daqcore::can_thread::CanThreadEvent::SendFailed { error, .. } => {
                    log::error!("{error}");
                    self.diagnostic = Some(error);
                }
                _ => {}
            }
        }
        self.session.evict();
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

pub fn load_formatter() -> Option<daqcore::formatter::Formatter> {
    let local = paths::read_file("formatter_config.json");
    if let Some(config) = local {
        match daqcore::formatter::Formatter::from_json(&config) {
            Ok(f) => return Some(f),
            Err(e) => log::warn!("Invalid formatter configuration: {e}"),
        }
    }
    daqcore::formatter::Formatter::from_json(include_str!("../formatter_config.json"))
        .map_err(|e| log::error!("Invalid embedded formatter: {e}"))
        .ok()
}

impl Drop for DAQApp {
    fn drop(&mut self) {
        if self.can_thread.stop().is_err() {
            log::error!("CAN worker panicked during shutdown");
        }
    }
}
