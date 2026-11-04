//! Live CAN monitoring and injection over stdio.
//!
//! Received frames stream to stdout as JSON Lines. Stdin accepts raw CAN
//! `ID:HEXDATA` lines, or `fil watch`-style `BUS[@TIME_MS]:ID:HEXDATA` lines
//! when a SuperDBC artifact is selected for decoding.

mod injection;
mod record;

use daqcore::{can_thread, connection};
use std::{
    io, path, process,
    sync::{
        Arc,
        atomic::{AtomicBool, Ordering},
        mpsc,
    },
    thread,
};

/// Request ID for the CLI's single SuperDBC selection request.
const DATABASE_REQUEST_ID: u64 = 1;

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
    let source = connection_source(&args);
    let superdbc_path = args.superdbc;
    let requested_bus = args.bus;

    let (event_tx, event_rx) = mpsc::channel();
    let mut worker =
        match can_thread::spawn_can_thread(can_thread::CanThreadConfig::default(), event_tx) {
            Ok(worker) => worker,
            Err(error) => {
                log::error!("failed to start the CAN worker: {error}");
                return process::ExitCode::FAILURE;
            }
        };

    if let Some(path) = &superdbc_path {
        let selection = can_thread::CanThreadCommand::SelectDatabase {
            request_id: DATABASE_REQUEST_ID,
            selection: can_thread::DatabaseRequest::Load {
                path: path.clone(),
                preferred_bus: requested_bus,
            },
        };
        if let Err(error) = worker.command(selection) {
            log::error!("failed to send the SuperDBC selection: {error}");
            return process::ExitCode::FAILURE;
        }
    }
    if let Err(error) = worker.command(can_thread::CanThreadCommand::Connect(Some(source))) {
        log::error!("failed to send the CAN source selection: {error}");
        return process::ExitCode::FAILURE;
    }

    let monitor = Monitor {
        events: &event_rx,
        output: &mut io::stdout().lock(),
        superdbc_path: superdbc_path.as_deref(),
        requested_bus,
        command_sender: worker.sender(),
        quit_requested: Arc::new(AtomicBool::new(false)),
    };
    let exit = monitor.run();

    if worker.stop().is_err() {
        log::warn!("CAN worker stopped unexpectedly");
    }
    exit
}

fn connection_source(args: &WatchArgs) -> connection::ConnectionSource {
    if let Some(path) = &args.serial {
        return connection::ConnectionSource::Serial(path.clone(), args.speed.into());
    }
    if let Some(port) = args.udp {
        return connection::ConnectionSource::Udp(port);
    }
    if args.simulated {
        return connection::ConnectionSource::Simulated(true);
    }
    // The clap source group guarantees exactly one source option.
    connection::ConnectionSource::Loopback
}

/// A resolved SuperDBC bus used to label received frames.
struct SelectedBus {
    id: u8,
    name: String,
}

/// Consumes worker events until a failure or stdout closure ends the stream.
struct Monitor<'a, W> {
    events: &'a mpsc::Receiver<can_thread::CanThreadEvent>,
    output: &'a mut W,
    superdbc_path: Option<&'a path::Path>,
    requested_bus: Option<u8>,
    command_sender: mpsc::Sender<can_thread::CanThreadCommand>,
    quit_requested: Arc<AtomicBool>,
}

impl<W: io::Write> Monitor<'_, W> {
    fn run(self) -> process::ExitCode {
        let mut sequence = 0u64;
        let mut selected: Option<SelectedBus> = None;
        let mut last_connection_error: Option<String> = None;

        if self.superdbc_path.is_none() {
            log::info!("watching raw CAN; JSON Lines on stdout");
            self.start_injections(injection::InjectionContext::Raw);
        }

        loop {
            let event = match self.events.recv() {
                Ok(event) => event,
                Err(error) => {
                    if self.quit_requested.load(Ordering::SeqCst) {
                        return process::ExitCode::SUCCESS;
                    }
                    log::error!("CAN worker stopped: {error}");
                    return process::ExitCode::FAILURE;
                }
            };

            match event {
                can_thread::CanThreadEvent::DatabaseSelected { result, .. } => {
                    let (database, bus_id) = match result {
                        Ok(can_thread::DatabaseUpdate::Loaded { database, bus_id }) => {
                            (database, bus_id)
                        }
                        Ok(can_thread::DatabaseUpdate::Bus { bus_id }) => {
                            log::error!("CAN worker selected bus {bus_id} without a database");
                            return process::ExitCode::FAILURE;
                        }
                        Err(error) => {
                            if let Some(path) = self.superdbc_path {
                                log::error!("SuperDBC load failed ({}): {error}", path.display());
                            } else {
                                log::error!("SuperDBC load failed: {error}");
                            }
                            return process::ExitCode::FAILURE;
                        }
                    };

                    if let Some(requested) = self.requested_bus
                        && requested != bus_id
                    {
                        log::error!("SuperDBC has no bus {requested}");
                        return process::ExitCode::FAILURE;
                    }

                    let Some(bus) = database.bus_by_id(bus_id) else {
                        log::error!("SuperDBC has no bus {bus_id}");
                        return process::ExitCode::FAILURE;
                    };

                    let bus_name = bus.name().to_owned();
                    log::info!("decoding bus {bus_id} ({bus_name}); JSON Lines on stdout");
                    self.start_injections(injection::InjectionContext::Defined {
                        database,
                        bus_id,
                        bus_name: bus_name.clone(),
                    });
                    selected = Some(SelectedBus {
                        id: bus_id,
                        name: bus_name,
                    });
                }
                can_thread::CanThreadEvent::Frame(parsed) => {
                    if self.superdbc_path.is_some() && selected.is_none() {
                        log::error!("received a frame before the SuperDBC selection completed");
                        return process::ExitCode::FAILURE;
                    }

                    let bus = selected.as_ref().map(|bus| (bus.id, bus.name.as_str()));
                    let record = record::FrameRecord::from_frame(&parsed, sequence, bus);
                    if let Err(error) = record.write_json_line(self.output) {
                        if error.kind() == io::ErrorKind::BrokenPipe {
                            // A closed consumer, such as `head`, ends the stream normally.
                            return process::ExitCode::SUCCESS;
                        }
                        log::error!("failed to write a frame record: {error}");
                        return process::ExitCode::FAILURE;
                    }
                    sequence = sequence.wrapping_add(1);
                }
                can_thread::CanThreadEvent::MessageSent { identity, .. } => {
                    log::info!("sent CAN frame 0x{identity:x}");
                }
                can_thread::CanThreadEvent::SendFailed {
                    identity, error, ..
                } => {
                    log::warn!("failed to send CAN frame 0x{identity:x}: {error}");
                }
                can_thread::CanThreadEvent::SourceSelected(source) => {
                    let name =
                        source.map_or_else(|| "disconnected".to_owned(), |s| s.display_name());
                    log::info!("CAN source selected: {name}");
                }
                can_thread::CanThreadEvent::ConnectionSuccessful => {
                    last_connection_error = None;
                    log::info!("CAN connection established");
                }
                can_thread::CanThreadEvent::ConnectionFailed(error) => {
                    if last_connection_error.as_deref() != Some(error.as_str()) {
                        log::warn!("CAN connection failed, retrying: {error}");
                        last_connection_error = Some(error);
                    }
                }
                can_thread::CanThreadEvent::Diagnostic(message) => {
                    log::warn!("CAN worker diagnostic: {message}");
                }
                _ => {}
            }
        }
    }

    fn start_injections(&self, context: injection::InjectionContext) {
        let injections = injection::StdinInjections::new(
            context,
            self.command_sender.clone(),
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
