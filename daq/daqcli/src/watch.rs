//! Live CAN monitoring and injection over stdio.
//!
//! `watch` is the headless counterpart to DaqApp's live CAN pipeline: it drives
//! the same daqcore can thread worker, applies the same database, bus, and
//! connection state transitions, streams frames as JSON Lines, and accepts
//! CAN injection commands on stdin.

mod injection;
mod record;

use daqcore::{can_thread, connection, superdbc};
use std::{
    io, path, process,
    sync::{
        Arc,
        atomic::{AtomicBool, Ordering},
        mpsc,
    },
    thread,
};

/// Live CAN source options; exactly one source is required.
#[derive(clap::Args)]
#[command(group = clap::ArgGroup::new("source").required(true))]
pub struct WatchArgs {
    /// SuperDBC JSON artifact that names buses and decodes messages;
    /// without it frames are emitted and injected as raw CAN
    #[arg(long)]
    superdbc: Option<path::PathBuf>,

    /// SuperDBC bus ID to decode; defaults to the first bus in the artifact
    #[arg(long, requires = "superdbc")]
    bus: Option<u8>,

    /// Serial slcan adapter path, for example /dev/tty.usbmodem101
    #[arg(long, group = "source")]
    serial: Option<String>,

    /// Serial adapter CAN bit rate
    #[arg(long, value_enum, default_value_t = Speed::Kbps500, requires = "serial")]
    speed: Speed,

    /// UDP port that receives 16-byte timestamped frames
    #[arg(long, group = "source")]
    udp: Option<u16>,

    /// Emit random frames for every message defined on the selected bus
    #[arg(long, group = "source")]
    simulated: bool,

    /// Echo frames transmitted by the adapter back as received frames
    #[arg(long, group = "source")]
    loopback: bool,
}

impl WatchArgs {
    fn connection_source(&self) -> connection::ConnectionSource {
        if let Some(path) = &self.serial {
            return connection::ConnectionSource::Serial(path.clone(), self.speed.into());
        }
        if let Some(port) = self.udp {
            return connection::ConnectionSource::Udp(port);
        }
        if self.simulated {
            return connection::ConnectionSource::Simulated(true);
        }
        // The clap source group guarantees exactly one source option.
        connection::ConnectionSource::Loopback
    }
}

#[derive(Clone, Copy, clap::ValueEnum)]
enum Speed {
    #[value(name = "250k")]
    Kbps250,
    #[value(name = "500k")]
    Kbps500,
}

impl From<Speed> for connection::CanBusSpeed {
    fn from(speed: Speed) -> Self {
        match speed {
            Speed::Kbps250 => connection::CanBusSpeed::Kbps250,
            Speed::Kbps500 => connection::CanBusSpeed::Kbps500,
        }
    }
}

/// Streams frames from a live CAN source to stdout as JSON Lines.
pub fn run(args: WatchArgs) -> process::ExitCode {
    let source = args.connection_source();

    let (event_tx, event_rx) = mpsc::channel();
    let mut worker =
        match can_thread::spawn_can_thread(can_thread::CanThreadConfig::default(), event_tx) {
            Ok(worker) => worker,
            Err(error) => {
                log::error!("failed to start the CAN worker: {error}");
                return process::ExitCode::FAILURE;
            }
        };

    let monitor = Monitor {
        can_to_ui_rx: &event_rx,
        ui_to_can_tx: worker.sender(),
        output: &mut io::stdout().lock(),
        superdbc_path: args.superdbc,
        requested_bus: args.bus,
        pending_database: None,
        selection_request: 0,
        database: None,
        connection_status: ConnectionStatus::Disconnected,
        active_source: None,
        sequence: 0,
        quit_requested: Arc::new(AtomicBool::new(false)),
    };
    let exit = monitor.run(source);

    if worker.stop().is_err() {
        log::warn!("CAN worker stopped unexpectedly");
    }
    exit
}

/// Connection lifecycle mirrored from DaqApp.
#[derive(Debug, PartialEq, Clone)]
enum ConnectionStatus {
    Disconnected,
    Connected,
    Error(String),
}

/// The SuperDBC database and bus used to label frames, mirroring DaqApp.
struct DatabaseSelection {
    database: superdbc::database::Database,
    bus_id: u8,
}

impl DatabaseSelection {
    fn bus(&self) -> Option<&superdbc::database::Bus> {
        self.database.bus_by_id(self.bus_id)
    }
}

/// Headless counterpart to DaqApp's can thread handling.
struct Monitor<'a, W> {
    can_to_ui_rx: &'a mpsc::Receiver<can_thread::CanThreadEvent>,
    ui_to_can_tx: mpsc::Sender<can_thread::CanThreadCommand>,
    output: &'a mut W,
    superdbc_path: Option<path::PathBuf>,
    requested_bus: Option<u8>,
    pending_database: Option<u64>,
    selection_request: u64,
    database: Option<DatabaseSelection>,
    connection_status: ConnectionStatus,
    active_source: Option<connection::ConnectionSource>,
    sequence: u64,
    quit_requested: Arc<AtomicBool>,
}

impl<W: io::Write> Monitor<'_, W> {
    fn run(mut self, source: connection::ConnectionSource) -> process::ExitCode {
        if self.superdbc_path.is_none() {
            log::info!("watching raw CAN; JSON Lines on stdout");
            self.spawn_injection_reader(injection::InjectionContext::Raw);
        }
        self.request_database();
        self.connect_can(source);

        loop {
            let event = match self.can_to_ui_rx.recv() {
                Ok(event) => event,
                Err(error) => {
                    if self.quit_requested.load(Ordering::SeqCst) {
                        return process::ExitCode::SUCCESS;
                    }
                    log::error!("CAN worker stopped: {error}");
                    return process::ExitCode::FAILURE;
                }
            };
            if let Some(exit) = self.handle_event(event) {
                return exit;
            }
        }
    }

    fn request_database(&mut self) {
        if self.pending_database.is_some() {
            return;
        }
        let Some(path) = &self.superdbc_path else {
            return;
        };
        self.selection_request += 1;
        let request_id = self.selection_request;
        let command = can_thread::CanThreadCommand::SelectDatabase {
            request_id,
            selection: can_thread::DatabaseRequest::Load {
                path: path.clone(),
                preferred_bus: self.requested_bus,
            },
        };
        match self.ui_to_can_tx.send(command) {
            Ok(()) => self.pending_database = Some(request_id),
            Err(error) => log::error!("failed to request the SuperDBC: {error}"),
        }
    }

    fn connect_can(&mut self, source: connection::ConnectionSource) {
        self.set_connection_status(ConnectionStatus::Disconnected);
        let command = can_thread::CanThreadCommand::Connect(Some(source));
        if let Err(error) = self.ui_to_can_tx.send(command) {
            log::error!("failed to select the CAN source: {error}");
        }
    }

    fn apply_database_result(
        &mut self,
        request_id: u64,
        result: Result<can_thread::DatabaseUpdate, String>,
    ) -> Result<(), String> {
        if self.pending_database != Some(request_id) {
            return Ok(());
        }
        self.pending_database = None;
        match result {
            Ok(can_thread::DatabaseUpdate::Loaded { database, bus_id }) => {
                if let Some(requested) = self.requested_bus
                    && requested != bus_id
                {
                    return Err(format!("SuperDBC has no bus {requested}"));
                }
                let selection = DatabaseSelection {
                    database: *database,
                    bus_id,
                };
                let Some(bus) = selection.bus() else {
                    return Err(format!("SuperDBC has no bus {bus_id}"));
                };
                log::info!("decoding bus {bus_id} ({})", bus.name());
                self.spawn_injection_reader(injection::InjectionContext::Defined {
                    database: Box::new(selection.database.clone()),
                    bus_id,
                    bus_name: bus.name().to_owned(),
                });
                self.database = Some(selection);
                Ok(())
            }
            Ok(can_thread::DatabaseUpdate::Bus { bus_id }) => Err(format!(
                "CAN worker selected bus {bus_id} without a database"
            )),
            Err(error) => Err(error),
        }
    }

    fn handle_event(&mut self, event: can_thread::CanThreadEvent) -> Option<process::ExitCode> {
        match event {
            can_thread::CanThreadEvent::DatabaseSelected { request_id, result } => {
                if let Err(error) = self.apply_database_result(request_id, result) {
                    if let Some(path) = &self.superdbc_path {
                        log::error!("SuperDBC load failed ({}): {error}", path.display());
                    } else {
                        log::error!("SuperDBC load failed: {error}");
                    }
                    return Some(process::ExitCode::FAILURE);
                }
            }
            can_thread::CanThreadEvent::Frame(frame) => {
                let bus = self.database.as_ref().and_then(|selection| {
                    selection.bus().map(|bus| (selection.bus_id, bus.name()))
                });
                let record = record::FrameRecord::from_frame(&frame, self.sequence, bus);
                if let Err(error) = record.write_json_line(self.output) {
                    if error.kind() == io::ErrorKind::BrokenPipe {
                        // A closed consumer, such as `head`, ends the stream normally.
                        return Some(process::ExitCode::SUCCESS);
                    }
                    log::error!("failed to write a frame record: {error}");
                    return Some(process::ExitCode::FAILURE);
                }
                self.sequence = self.sequence.wrapping_add(1);
            }
            can_thread::CanThreadEvent::MessageSent { identity, .. } => {
                log::info!("sent CAN frame 0x{identity:x}");
            }
            can_thread::CanThreadEvent::SendFailed {
                identity, error, ..
            } => {
                log::warn!("failed to send CAN frame 0x{identity:x}: {error}");
            }
            can_thread::CanThreadEvent::Diagnostic(message) => {
                log::warn!("CAN worker diagnostic: {message}");
            }
            can_thread::CanThreadEvent::SourceSelected(Some(source)) => {
                if self.active_source.as_ref() != Some(&source) {
                    log::info!("CAN source selected: {}", source.display_name());
                }
                self.active_source = Some(source);
            }
            can_thread::CanThreadEvent::ConnectionSuccessful => {
                self.set_connection_status(ConnectionStatus::Connected);
            }
            can_thread::CanThreadEvent::ConnectionFailed(error) => {
                self.set_connection_status(ConnectionStatus::Error(error));
            }
            can_thread::CanThreadEvent::Disconnection => {
                self.set_connection_status(ConnectionStatus::Disconnected);
            }
            _ => {}
        }
        None
    }

    fn set_connection_status(&mut self, status: ConnectionStatus) {
        if self.connection_status == status {
            return;
        }
        match &status {
            ConnectionStatus::Disconnected => log::info!("CAN disconnected"),
            ConnectionStatus::Connected => log::info!("CAN connection established"),
            ConnectionStatus::Error(error) => {
                log::warn!("CAN connection failed, retrying: {error}");
            }
        }
        self.connection_status = status;
    }

    fn spawn_injection_reader(&self, context: injection::InjectionContext) {
        let injections = injection::StdinInjections::new(
            context,
            self.ui_to_can_tx.clone(),
            Arc::clone(&self.quit_requested),
        );
        let spawned = thread::Builder::new()
            .name("daqcli-stdin".into())
            .spawn(move || injections.run());
        if let Err(error) = spawned {
            log::error!("failed to start the stdin injection reader: {error}");
        }
    }
}
