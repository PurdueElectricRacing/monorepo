//! Live CAN monitoring and injection over stdio.

mod injection;
mod record;

use daqcore::{can_thread, connection};
use std::{io, path, process, sync::mpsc, thread};

/// Live CAN source options; exactly one source is required.
#[derive(clap::Args)]
#[command(group = clap::ArgGroup::new("source").required(true))]
pub struct WatchArgs {
    /// SuperDBC JSON artifact that names buses and decodes messages;
    /// without it frames are emitted and injected as raw CAN
    #[arg(long)]
    superdbc: Option<path::PathBuf>,

    /// SuperDBC bus ID to decode; defaults to bus 0, or the first bus if 0 is absent
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

    let (quit_tx, quit_rx) = mpsc::channel();
    let monitor = Monitor {
        can_to_ui_rx: &event_rx,
        ui_to_can_tx: worker.sender(),
        output: &mut io::stdout().lock(),
        superdbc_path: args.superdbc,
        requested_bus: args.bus,
        state: can_thread::state::CanState::default(),
        sequence: 0,
        quit_tx,
        quit_rx,
    };
    let exit = monitor.run(source);

    if worker.stop().is_err() {
        log::warn!("CAN worker stopped unexpectedly");
    }
    exit
}

struct Monitor<'a, W> {
    can_to_ui_rx: &'a mpsc::Receiver<can_thread::CanThreadEvent>,
    ui_to_can_tx: mpsc::Sender<can_thread::CanThreadCommand>,
    output: &'a mut W,
    superdbc_path: Option<path::PathBuf>,
    requested_bus: Option<u8>,
    state: can_thread::state::CanState,
    sequence: u64,
    quit_tx: mpsc::Sender<()>,
    quit_rx: mpsc::Receiver<()>,
}

impl<W: io::Write> Monitor<'_, W> {
    fn run(mut self, source: connection::ConnectionSource) -> process::ExitCode {
        self.request_database();
        self.connect_can(source);
        if self.superdbc_path.is_none() {
            log::info!("watching raw CAN; JSON Lines on stdout");
            self.spawn_injection_reader(injection::InjectionContext::Raw);
        }

        loop {
            let event = match self.can_to_ui_rx.recv() {
                Ok(event) => event,
                Err(error) => {
                    if self.quit_rx.try_recv().is_ok() {
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
        let Some(path) = &self.superdbc_path else {
            return;
        };
        if let Err(error) = self.state.request_selection(
            &self.ui_to_can_tx,
            can_thread::DatabaseRequest::Load {
                path: path.clone(),
                preferred_bus: self.requested_bus,
            },
        ) {
            log::error!("failed to request the SuperDBC: {error}");
        }
    }

    fn connect_can(&mut self, source: connection::ConnectionSource) {
        if let Err(error) = self.state.connect(&self.ui_to_can_tx, source) {
            log::error!("failed to select the CAN source: {error}");
        }
    }

    fn handle_event(&mut self, event: can_thread::CanThreadEvent) -> Option<process::ExitCode> {
        let previous_status = self.state.connection_status.clone();
        if self.state.observe_connection(&event) {
            log::info!("CAN source changed");
        }
        if self.state.connection_status != previous_status {
            log::info!("CAN connection: {:?}", self.state.connection_status);
        }
        match event {
            can_thread::CanThreadEvent::DatabaseSelected { request_id, result } => {
                let changed = match self.state.apply_database_result(request_id, result) {
                    Ok(changed) => changed,
                    Err(error) => {
                        log::error!("SuperDBC selection failed: {error}");
                        return Some(process::ExitCode::FAILURE);
                    }
                };
                if !changed {
                    return None;
                }
                let selection = self.state.database.as_ref()?;
                if self
                    .requested_bus
                    .is_some_and(|requested| requested != selection.bus_id)
                {
                    log::error!("SuperDBC does not contain the requested bus");
                    return Some(process::ExitCode::FAILURE);
                }
                let bus = selection.bus()?;
                self.spawn_injection_reader(injection::InjectionContext::Defined {
                    database: Box::new(selection.database.clone()),
                    bus_id: selection.bus_id,
                    bus_name: bus.name().to_owned(),
                });
            }
            can_thread::CanThreadEvent::Frame(frame) => {
                let bus = self.state.database.as_ref().and_then(|selection| {
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
            _ => {}
        }
        None
    }

    fn spawn_injection_reader(&self, context: injection::InjectionContext) {
        let injections = injection::StdinInjections::new(
            context,
            self.ui_to_can_tx.clone(),
            self.quit_tx.clone(),
        );
        let spawned = thread::Builder::new()
            .name("daqcli-stdin".into())
            .spawn(move || injections.run());
        if let Err(error) = spawned {
            log::error!("failed to start the stdin injection reader: {error}");
        }
    }
}
