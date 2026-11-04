//! Live CAN monitoring and injection over stdio.
//!
//! Received frames stream to stdout as JSON Lines. Stdin accepts raw CAN
//! `ID:HEXDATA` lines, or `fil watch`-style `BUS[@TIME_MS]:ID:HEXDATA` lines
//! when a SuperDBC artifact is selected for decoding.

use daqcore::{can, can_thread, connection, frame, superdbc};
use std::{
    io::{self, BufRead as _},
    path, process,
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
            self.start_injections(InjectionContext::Raw);
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
                    self.start_injections(InjectionContext::Defined {
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
                    let record = FrameRecord::from_frame(&parsed, sequence, bus);
                    if let Err(error) = write_record(self.output, &record) {
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

    fn start_injections(&self, context: InjectionContext) {
        let injections = StdinInjections {
            context,
            sender: self.command_sender.clone(),
            quit_requested: Arc::clone(&self.quit_requested),
        };
        let spawned = thread::Builder::new()
            .name("daqcli-stdin".into())
            .spawn(move || injections.run());
        if let Err(error) = spawned {
            log::error!("failed to start the stdin injection reader: {error}");
        }
    }
}

fn write_record(output: &mut impl io::Write, record: &FrameRecord<'_>) -> io::Result<()> {
    serde_json::to_writer(&mut *output, record)?;
    output.write_all(b"\n")?;
    output.flush()
}

/// Decoding context for stdin injection lines.
enum InjectionContext {
    /// No SuperDBC artifact; `ID:HEXDATA` lines only.
    Raw,
    /// SuperDBC-selected bus; `BUS[@TIME_MS]:ID:HEXDATA` lines allowed.
    Defined {
        database: Box<superdbc::database::Database>,
        bus_id: u8,
        bus_name: String,
    },
}

/// Reads CAN injection commands from stdin for as long as it is open.
struct StdinInjections {
    context: InjectionContext,
    sender: mpsc::Sender<can_thread::CanThreadCommand>,
    quit_requested: Arc<AtomicBool>,
}

impl StdinInjections {
    fn run(self) {
        for line in io::stdin().lock().lines() {
            let line = match line {
                Ok(line) => line,
                Err(error) => {
                    log::error!("failed to read stdin: {error}");
                    return;
                }
            };
            let line = line.trim();
            if line.is_empty() {
                continue;
            }
            if line == "quit" || line == "exit" {
                self.quit_requested.store(true, Ordering::SeqCst);
                // A closed worker is reported by the monitor, not from here.
                let _ = self.sender.send(can_thread::CanThreadCommand::Stop);
                return;
            }
            match parse_injection(line, &self.context) {
                Ok(command) => {
                    if self.sender.send(command).is_err() {
                        // The worker already stopped; the monitor reports the shutdown.
                        return;
                    }
                }
                Err(error) => log::warn!("ignored stdin CAN command: {error}"),
            }
        }
        log::info!("stdin closed; continuing without injection input");
    }
}

/// Parses a stdin injection line, validating against daqcore frame rules.
fn parse_injection(
    line: &str,
    context: &InjectionContext,
) -> Result<can_thread::CanThreadCommand, String> {
    let mut fields = line.splitn(3, ':');
    let first = fields.next().ok_or_else(|| injection_format(context))?;
    let second = fields.next().ok_or_else(|| injection_format(context))?;
    let third = fields.next();

    let (id_text, data_text) = match third {
        None => (first, second),
        Some(data_text) => {
            match context {
                InjectionContext::Defined {
                    bus_id, bus_name, ..
                } => validate_injection_bus(first, *bus_id, bus_name)?,
                InjectionContext::Raw => {
                    return Err("bus prefixes require --superdbc; use ID:HEXDATA".to_owned());
                }
            }
            (second, data_text)
        }
    };

    let identity = parse_unsigned(id_text)
        .filter(|identity| *identity <= u64::from(can::EXTENDED_ID_MASK))
        .ok_or_else(|| "invalid CAN injection identifier".to_owned())?;
    let identity =
        u32::try_from(identity).map_err(|_| "invalid CAN injection identifier".to_owned())?;

    let is_extended_id = match context {
        InjectionContext::Defined {
            database, bus_id, ..
        } => database.message(*bus_id, identity).map_or(
            identity > can::STANDARD_ID_MASK,
            superdbc::message::Message::is_extended_id,
        ),
        InjectionContext::Raw => identity > can::STANDARD_ID_MASK,
    };

    let data = parse_hex_bytes(data_text)?;
    let frame = frame::CanFrameTx::data(identity, is_extended_id, data)?;
    Ok(can_thread::CanThreadCommand::SendOnce(frame))
}

fn injection_format(context: &InjectionContext) -> String {
    match context {
        InjectionContext::Raw => "expected ID:HEXDATA".to_owned(),
        InjectionContext::Defined { .. } => {
            "expected BUS[@TIME_MS]:ID:HEXDATA or ID:HEXDATA".to_owned()
        }
    }
}

/// Validates a `BUS[@TIME_MS]` prefix against the selected SuperDBC bus.
fn validate_injection_bus(endpoint: &str, bus_id: u8, bus_name: &str) -> Result<(), String> {
    let (bus, at_time) = match endpoint.split_once('@') {
        Some((bus, at_time)) => (bus, Some(at_time)),
        None => (endpoint, None),
    };
    if let Some(at_time) = at_time {
        if at_time.is_empty() || parse_unsigned(at_time).is_none() {
            return Err("invalid CAN injection time".to_owned());
        }
        // `fil watch` parses a time prefix but injects stdin frames immediately too.
        log::debug!("ignoring stdin injection time; sending immediately");
    }
    let numeric = bus.parse::<u8>().ok();
    if bus != bus_name && numeric != Some(bus_id) {
        return Err(format!(
            "injection names bus {bus}, but {bus_name} is selected"
        ));
    }
    Ok(())
}

fn parse_unsigned(text: &str) -> Option<u64> {
    match text.strip_prefix("0x").or_else(|| text.strip_prefix("0X")) {
        Some(digits) => u64::from_str_radix(digits, 16).ok(),
        None => text.parse::<u64>().ok(),
    }
}

fn parse_hex_bytes(text: &str) -> Result<Vec<u8>, String> {
    if !text.len().is_multiple_of(2) {
        return Err("CAN injection payload must contain complete hex bytes".to_owned());
    }
    text.as_bytes()
        .as_chunks::<2>()
        .0
        .iter()
        .map(|pair| {
            let high = hex_nibble(pair[0])
                .ok_or_else(|| "CAN injection payload is not hexadecimal".to_owned())?;
            let low = hex_nibble(pair[1])
                .ok_or_else(|| "CAN injection payload is not hexadecimal".to_owned())?;
            Ok((high << 4) | low)
        })
        .collect()
}

fn hex_nibble(byte: u8) -> Option<u8> {
    match byte {
        b'0'..=b'9' => Some(byte - b'0'),
        b'a'..=b'f' => Some(byte - b'a' + 10),
        b'A'..=b'F' => Some(byte - b'A' + 10),
        _ => None,
    }
}

/// One JSON Lines record describing a received CAN frame.
#[derive(serde::Serialize)]
struct FrameRecord<'a> {
    /// Host receive time as Unix milliseconds.
    time_ms: i64,
    /// Zero-based position of the frame in this stream.
    sequence: u64,
    /// SuperDBC bus ID; omitted when no SuperDBC is selected.
    #[serde(skip_serializing_if = "Option::is_none")]
    bus_id: Option<u8>,
    /// SuperDBC bus name; omitted when no SuperDBC is selected.
    #[serde(skip_serializing_if = "Option::is_none")]
    bus: Option<&'a str>,
    /// CAN identifier as lowercase hexadecimal with a `0x` prefix.
    id: String,
    remote: bool,
    fd: bool,
    brs: bool,
    dlc: u8,
    /// Payload byte count.
    length: usize,
    /// Payload bytes as lowercase hexadecimal without separators.
    data: String,
    /// SuperDBC message name; omitted when the frame is not defined.
    #[serde(skip_serializing_if = "Option::is_none")]
    message: Option<&'a str>,
    /// SuperDBC transmitting node; omitted when the frame is not defined.
    #[serde(skip_serializing_if = "Option::is_none")]
    tx_node: Option<&'a str>,
    /// Decoded signal values; omitted when the frame is not defined.
    #[serde(skip_serializing_if = "Option::is_none")]
    signals: Option<serde_json::Map<String, serde_json::Value>>,
}

impl<'a> FrameRecord<'a> {
    fn from_frame(
        parsed: &'a frame::ParsedFrame,
        sequence: u64,
        bus: Option<(u8, &'a str)>,
    ) -> Self {
        let decoded = parsed.decoded.as_ref();
        let flags = FrameFlags::from_kind(parsed.kind);
        Self {
            time_ms: parsed.timestamp.unix_millis(),
            sequence,
            bus_id: bus.map(|(bus_id, _)| bus_id),
            bus: bus.map(|(_, bus_name)| bus_name),
            id: format!("0x{:x}", parsed.identity),
            remote: flags.remote,
            fd: flags.fd,
            brs: flags.brs,
            dlc: parsed.dlc,
            length: parsed.raw_bytes.len(),
            data: hex_encode(&parsed.raw_bytes),
            message: decoded.map(superdbc::decode::DecodedMessage::name),
            tx_node: decoded.map(superdbc::decode::DecodedMessage::tx_node),
            signals: decoded.map(signal_map),
        }
    }
}

struct FrameFlags {
    remote: bool,
    fd: bool,
    brs: bool,
}

impl FrameFlags {
    fn from_kind(kind: frame::FrameKind) -> Self {
        match kind {
            frame::FrameKind::Data => Self {
                remote: false,
                fd: false,
                brs: false,
            },
            frame::FrameKind::Remote => Self {
                remote: true,
                fd: false,
                brs: false,
            },
            frame::FrameKind::Fd { bit_rate_switched } => Self {
                remote: false,
                fd: true,
                brs: bit_rate_switched,
            },
        }
    }
}

fn signal_map(
    decoded: &superdbc::decode::DecodedMessage,
) -> serde_json::Map<String, serde_json::Value> {
    decoded
        .iter()
        .map(|(name, value)| (name.to_owned(), signal_value(value)))
        .collect()
}

fn signal_value(value: &superdbc::decode::DecodedSignalValue) -> serde_json::Value {
    if let Some(label) = value.enum_label() {
        return serde_json::Value::String(label.to_owned());
    }
    match serde_json::Number::from_f64(value.physical()) {
        Some(number) => serde_json::Value::Number(number),
        // JSON cannot represent non-finite telemetry.
        None => serde_json::Value::Null,
    }
}

fn hex_encode(bytes: &[u8]) -> String {
    const HEXADECIMAL: &[u8; 16] = b"0123456789abcdef";
    let mut encoded = String::with_capacity(bytes.len() * 2);
    for byte in bytes {
        // Nibbles are in 0..=15, so both lookups are in bounds.
        encoded.push(char::from(HEXADECIMAL[usize::from(byte >> 4)]));
        encoded.push(char::from(HEXADECIMAL[usize::from(byte & 0x0f)]));
    }
    encoded
}
