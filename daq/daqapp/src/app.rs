use crate::{
    action, paths, settings, shortcuts, telemetry, ui, util, widget_ids, widgets, workspace,
};

use std::{error, mem, path, sync::mpsc};
const MAX_CAN_EVENTS_PER_UPDATE: usize = 2_048;

const UI_SCALE_STEP: f32 = 0.2;

pub struct DatabaseSelection {
    pub database: daqcore::superdbc::database::Database,
    pub bus_id: u8,
}

impl DatabaseSelection {
    pub fn bus(&self) -> Option<&daqcore::superdbc::database::Bus> {
        self.database.bus_by_id(self.bus_id)
    }

    pub fn source(&self) -> Option<daqcore::can_thread::DatabaseSource> {
        Some(daqcore::can_thread::DatabaseSource {
            path: self.database.path()?.to_owned(),
            bus_id: self.bus_id,
        })
    }
}

fn apply_fil_bus(source: &mut daqcore::connection::ConnectionSource, bus_id: Option<u8>) {
    if let daqcore::connection::ConnectionSource::Fil {
        bus: injection_bus,
        trace_bus,
        ..
    } = source
    {
        if let Some(bus_id) = bus_id {
            let bus = bus_id.to_string();
            *injection_bus = bus.clone();
            *trace_bus = Some(bus);
        } else {
            *trace_bus = None;
        }
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
    pub can_to_ui_rx: mpsc::Receiver<daqcore::can_thread::CanThreadEvent>,
    pub ui_to_can_tx: mpsc::Sender<daqcore::can_thread::CanThreadCommand>,
    pub action_queue: Vec<action::AppAction>,
    pub selected_source: Option<daqcore::connection::ConnectionSource>,
    pub theme: eframe::egui::Style,
    pub theme_selection: ui::theme::ThemeSelection,
    pub pixels_per_point: Option<f32>,
    pub serial_ports: Vec<serialport::SerialPortInfo>,
    pub database: Option<DatabaseSelection>,
    pending_database: Option<u64>,
    selection_request: u64,
    pub can_bus_speed: daqcore::connection::CanBusSpeed,
    pub udp_port: u16,
    pub session: daqcore::session::Session,
    pub bus_load_samples: Vec<telemetry::BusLoadSample>,
    pub can_thread: daqcore::can_thread::CanThreadHandle,
    hil_snapshot: daqcore::hil::engine::HilSnapshot,
    active_source: Option<daqcore::connection::ConnectionSource>,
    pub diagnostic: Option<String>,
    pub log_folder: Option<path::PathBuf>,
    pub fil: settings::FilSettings,
}

impl DAQApp {
    pub fn save_settings(&self) {
        let settings = settings::Settings {
            superdbc_path: self
                .database
                .as_ref()
                .and_then(|selection| selection.source())
                .map(|source| source.path),
            bus_id: self.database.as_ref().map(|selection| selection.bus_id),
            selected_source: self.selected_source.clone(),
            selected_speed: self.can_bus_speed,
            udp_port: self.udp_port,
            theme: self.theme_selection,
            pixels_per_point: self.pixels_per_point,
            log_folder: self.log_folder.clone(),
            window_secs: self.session.timeline().window_secs(),
            fil: self.fil.clone(),
        };
        settings.save();
    }

    pub fn new(
        can_to_ui_rx: mpsc::Receiver<daqcore::can_thread::CanThreadEvent>,
        can_thread: daqcore::can_thread::CanThreadHandle,
        settings: settings::Settings,
        cc: &eframe::CreationContext,
    ) -> Result<Self, Box<dyn error::Error + Send + Sync>> {
        let theme_selection = settings.theme;
        let theme_style = theme_selection.get_style();
        ui::theme::store_theme(&cc.egui_ctx, theme_selection.get_colors());

        egui_extras::install_image_loaders(&cc.egui_ctx);

        let window_secs = if settings.window_secs.is_finite() && settings.window_secs >= 0.0 {
            settings.window_secs
        } else {
            30.0
        };

        let session = daqcore::session::Session::live(daqcore::time::Time::now(), window_secs)?;

        let fil = settings.fil;
        let mut app = Self {
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
            database: None,
            pending_database: None,
            selection_request: 0,
            can_bus_speed: settings.selected_speed,
            udp_port: settings.udp_port,
            session,
            bus_load_samples: Vec::new(),
            hil_snapshot: daqcore::hil::engine::HilSnapshot::idle(),
            active_source: None,
            diagnostic: None,
            log_folder: settings.log_folder,
            fil,
        };
        if let Some(path) = settings.superdbc_path {
            app.request_database(&path, settings.bus_id);
        }
        Ok(app)
    }

    pub fn selection_pending(&self) -> bool {
        self.pending_database.is_some()
    }

    pub fn request_database(&mut self, path: &path::Path, preferred_bus: Option<u8>) {
        self.request_selection(daqcore::can_thread::DatabaseRequest::Load {
            path: path.to_owned(),
            preferred_bus,
        });
    }

    pub fn request_bus(&mut self, bus_id: u8) {
        let Some(selection) = &self.database else {
            return;
        };
        if selection.bus_id == bus_id {
            return;
        }
        if selection.database.bus_by_id(bus_id).is_none() {
            self.diagnostic = Some(format!("SuperDBC has no bus {bus_id}"));
            return;
        }
        self.request_selection(daqcore::can_thread::DatabaseRequest::Bus { bus_id });
    }

    fn request_selection(&mut self, selection: daqcore::can_thread::DatabaseRequest) {
        if self.selection_pending() {
            return;
        }
        self.selection_request += 1;
        let request_id = self.selection_request;
        let command = daqcore::can_thread::CanThreadCommand::SelectDatabase {
            request_id,
            selection,
        };
        match self.ui_to_can_tx.send(command) {
            Ok(()) => self.pending_database = Some(request_id),
            Err(error) => self.diagnostic = Some(error.to_string()),
        }
    }

    fn apply_database_result(
        &mut self,
        request_id: u64,
        result: Result<daqcore::can_thread::DatabaseUpdate, String>,
    ) -> bool {
        if self.pending_database != Some(request_id) {
            return false;
        }
        self.pending_database = None;
        match result {
            Ok(daqcore::can_thread::DatabaseUpdate::Loaded { database, bus_id }) => {
                self.database = Some(DatabaseSelection {
                    database: *database,
                    bus_id,
                });
            }
            Ok(daqcore::can_thread::DatabaseUpdate::Bus { bus_id }) => {
                let Some(selection) = &mut self.database else {
                    self.diagnostic = Some("No UI database is loaded".to_owned());
                    return false;
                };
                selection.bus_id = bus_id;
            }
            Err(error) => {
                self.diagnostic = Some(error);
                return false;
            }
        }
        self.session.reset(daqcore::time::Time::now());
        self.bus_load_samples.clear();
        self.hil_snapshot = daqcore::hil::engine::HilSnapshot::idle();
        self.diagnostic = None;
        self.synchronize_fil_bus();
        for tile in self.tile_tree.tiles.tiles_mut() {
            if let egui_tiles::Tile::Pane(widget) = tile {
                widget.reset_database();
            }
        }
        true
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
    /// or network config is selected yet. The sidebar database selection
    /// controls the injection bus and trace filter using its numeric bus ID;
    /// no database means all trace buses and retains the injection fallback.
    pub fn fil_connect_source(&self) -> Option<daqcore::connection::ConnectionSource> {
        let mut source = self.fil.connection_source()?;
        apply_fil_bus(&mut source, self.database_bus());
        Some(source)
    }

    fn database_bus(&self) -> Option<u8> {
        self.database
            .as_ref()
            .and_then(|selection| selection.bus().map(|_| selection.bus_id))
    }

    fn synchronize_fil_bus(&mut self) {
        let bus = self.database_bus();
        if let Some(source) = &mut self.selected_source {
            apply_fil_bus(source, bus);
        }
        if let Some(source) = &mut self.active_source {
            apply_fil_bus(source, bus);
        }
    }

    pub fn connect_can(&mut self) {
        let bus = self.database_bus();
        let Some(source) = &mut self.selected_source else {
            return;
        };
        apply_fil_bus(source, bus);

        self.connection_status = ConnectionStatus::Disconnected;

        if let Err(error) = self
            .ui_to_can_tx
            .send(daqcore::can_thread::CanThreadCommand::Connect(Some(
                source.clone(),
            )))
        {
            self.diagnostic = Some(error.to_string());
        }
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
                    &self.fil,
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
                self.fil = fil;
                self.fil.normalize();
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
                daqcore::can_thread::CanThreadEvent::DatabaseSelected { request_id, result } => {
                    if self.apply_database_result(request_id, result) {
                        self.save_settings();
                    }
                }
                daqcore::can_thread::CanThreadEvent::Frame(frame) => {
                    self.session.ingest_frame(frame);
                }
                daqcore::can_thread::CanThreadEvent::SourceSelected(Some(source)) => {
                    if self.active_source.as_ref() != Some(&source) {
                        self.session.reset(daqcore::time::Time::now());
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
        for action in mem::take(&mut self.action_queue) {
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

#[cfg(test)]
mod tests {
    use super::apply_fil_bus;
    use daqcore::connection::{ConnectionSource, FilRunOptions};
    use std::{collections::HashMap, path::PathBuf};

    fn fil_source() -> ConnectionSource {
        ConnectionSource::Fil {
            executable: PathBuf::from("fil"),
            network: PathBuf::from("network"),
            bus: "fallback-injection-bus".to_owned(),
            trace_bus: Some("old-trace-bus".to_owned()),
            elf_overrides: HashMap::new(),
            disabled_boards: Vec::new(),
            built_network: None,
            run_options: Box::new(FilRunOptions::default()),
        }
    }

    #[test]
    fn selected_sidebar_bus_id_sets_injection_and_trace_bus_ids() {
        for (bus_id, expected) in [(0_u8, "0"), (1_u8, "1")] {
            let mut source = fil_source();
            apply_fil_bus(&mut source, Some(bus_id));

            let ConnectionSource::Fil { bus, trace_bus, .. } = source else {
                unreachable!();
            };
            assert_eq!(bus, expected);
            assert_eq!(trace_bus.as_deref(), Some(expected));
        }
    }

    #[test]
    fn absent_database_clears_trace_filter_and_retains_injection_fallback() {
        let mut source = fil_source();
        apply_fil_bus(&mut source, None);

        let ConnectionSource::Fil { bus, trace_bus, .. } = source else {
            unreachable!();
        };
        assert_eq!(bus, "fallback-injection-bus");
        assert_eq!(trace_bus, None);
    }

    #[test]
    fn non_fil_sources_are_unchanged() {
        let mut source = ConnectionSource::Loopback;
        apply_fil_bus(&mut source, Some(0));
        assert_eq!(source, ConnectionSource::Loopback);
    }
}
