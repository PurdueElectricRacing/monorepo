use crate::{
    action, paths, settings, shortcuts, telemetry, ui, util, widget_ids, widgets, workspace,
};
use daqcore::can_thread;
use std::{error, mem, path, sync::mpsc};

const UI_SCALE_STEP: f32 = 0.2;

pub struct DAQApp {
    pub can_state: can_thread::state::CanState,
    pub value_formatter: Option<daqcore::formatter::Formatter>,
    pub is_sidebar_open: bool,
    pub command_palette: ui::command_palette::CommandPalette,
    pub tile_tree: egui_tiles::Tree<widgets::Widget>,
    pub widget_ids: widget_ids::WidgetIds,
    pub can_to_ui_rx: mpsc::Receiver<can_thread::CanThreadEvent>,
    pub ui_to_can_tx: mpsc::Sender<can_thread::CanThreadCommand>,
    pub action_queue: Vec<action::AppAction>,
    pub selected_source: Option<daqcore::connection::ConnectionSource>,
    pub theme: eframe::egui::Style,
    pub theme_selection: ui::theme::ThemeSelection,
    pub pixels_per_point: Option<f32>,
    pub serial_ports: Vec<serialport::SerialPortInfo>,
    pub can_bus_speed: daqcore::connection::CanBusSpeed,
    pub udp_port: u16,
    pub session: daqcore::session::Session,
    pub bus_load_samples: Vec<telemetry::BusLoadSample>,
    pub can_thread: can_thread::CanThreadHandle,
    hil_snapshot: daqcore::hil::engine::HilSnapshot,
    pub diagnostic: Option<String>,
    pub log_folder: Option<path::PathBuf>,
}

impl DAQApp {
    pub fn save_settings(&self) {
        let settings = settings::Settings {
            superdbc_path: self
                .can_state
                .database
                .as_ref()
                .and_then(|selection| selection.source())
                .map(|source| source.path),
            bus_id: self
                .can_state
                .database
                .as_ref()
                .map(|selection| selection.bus_id),
            selected_source: self.selected_source.clone(),
            selected_speed: self.can_bus_speed,
            udp_port: self.udp_port,
            theme: self.theme_selection,
            pixels_per_point: self.pixels_per_point,
            log_folder: self.log_folder.clone(),
            window_secs: self.session.timeline().window_secs(),
        };
        settings.save();
    }

    pub fn new(
        can_to_ui_rx: mpsc::Receiver<can_thread::CanThreadEvent>,
        can_thread: can_thread::CanThreadHandle,
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

        let mut app = Self {
            can_state: can_thread::state::CanState::default(),
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
            can_bus_speed: settings.selected_speed,
            udp_port: settings.udp_port,
            session,
            bus_load_samples: Vec::new(),
            hil_snapshot: daqcore::hil::engine::HilSnapshot::idle(),
            diagnostic: None,
            log_folder: settings.log_folder,
        };
        if let Some(path) = settings.superdbc_path {
            app.request_database(&path, settings.bus_id);
        }
        Ok(app)
    }

    pub fn selection_pending(&self) -> bool {
        self.can_state.selection_pending()
    }

    pub fn request_database(&mut self, path: &path::Path, preferred_bus: Option<u8>) {
        self.request_selection(can_thread::DatabaseRequest::Load {
            path: path.to_owned(),
            preferred_bus,
        });
    }

    pub fn request_bus(&mut self, bus_id: u8) {
        self.request_selection(can_thread::DatabaseRequest::Bus { bus_id });
    }

    fn request_selection(&mut self, selection: can_thread::DatabaseRequest) {
        if let Err(error) = self
            .can_state
            .request_selection(&self.ui_to_can_tx, selection)
        {
            self.diagnostic = Some(error);
        }
    }

    fn apply_database_result(
        &mut self,
        request_id: u64,
        result: Result<can_thread::DatabaseUpdate, String>,
    ) -> bool {
        match self.can_state.apply_database_result(request_id, result) {
            Ok(true) => {}
            Ok(false) => return false,
            Err(error) => {
                self.diagnostic = Some(error);
                return false;
            }
        }
        self.session.reset(daqcore::time::Time::now());
        self.bus_load_samples.clear();
        self.hil_snapshot = daqcore::hil::engine::HilSnapshot::idle();
        self.diagnostic = None;
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

    pub fn connect_can(&mut self) {
        let Some(source) = &self.selected_source else {
            return;
        };

        if let Err(error) = self.can_state.connect(&self.ui_to_can_tx, source.clone()) {
            self.diagnostic = Some(error);
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
                ) {
                    Some(mut widget) => {
                        widget.handle_operational_event(&can_thread::CanThreadEvent::Hil(
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
        while let Ok(event) = self.can_to_ui_rx.try_recv() {
            for tile in self.tile_tree.tiles.tiles_mut() {
                if let egui_tiles::Tile::Pane(widget) = tile {
                    widget.handle_operational_event(&event);
                }
            }

            if self.can_state.observe_connection(&event) {
                self.session.reset(daqcore::time::Time::now());
                self.bus_load_samples.clear();
            }
            match event {
                can_thread::CanThreadEvent::DatabaseSelected { request_id, result } => {
                    if self.apply_database_result(request_id, result) {
                        self.save_settings();
                    }
                }
                can_thread::CanThreadEvent::Frame(frame) => {
                    self.session.ingest_frame(frame);
                }
                can_thread::CanThreadEvent::BusLoad {
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
                can_thread::CanThreadEvent::Hil(snapshot) => self.hil_snapshot = snapshot,
                can_thread::CanThreadEvent::Diagnostic(error)
                | can_thread::CanThreadEvent::SendFailed { error, .. } => {
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
