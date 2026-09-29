//! Drivers are single-owner I/O adapters; no transport type escapes this module.

use crate::{can, connection, frame, log_parse, superdbc};
use std::{error, fmt, io, mem, net, time};

use rand::RngExt as _;
use rand::seq::IteratorRandom as _;

#[derive(Debug)]
pub enum DriverError {
    ConnectionFailed(String),
    Timeout,
    Read(String),
    Write(String),
    Unsupported(String),
}

impl fmt::Display for DriverError {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        write!(f, "{self:?}")
    }
}

impl error::Error for DriverError {}
pub type DriverResult<T> = Result<T, DriverError>;
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum FilGpioDirection {
    Input,
    Output,
}
#[derive(Clone, Debug)]
pub struct FilGpioEvent {
    pub board: String,
    pub port: String,
    pub pin: u8,
    pub value: Option<bool>,
    pub direction: FilGpioDirection,
}

#[derive(Clone, Debug, PartialEq, Eq)]
pub enum FilExpectationStatus { Pending, Pass, Fail, Incomplete }

#[derive(Clone, Debug, PartialEq, Eq)]
pub struct FilExpectationEvent {
    pub check_id: String, pub script: String, pub status: FilExpectationStatus,
    pub expected_bus: String, pub expected_id: u32, pub expected_extended: bool,
    pub expected_data: Vec<u8>, pub window_start_ns: u64, pub window_end_ns: u64,
    pub matched_bus: Option<String>, pub matched_id: Option<u32>,
    pub matched_data: Option<Vec<u8>>, pub matched_origin: Option<String>,
    pub matched_time_ns: Option<u64>, pub reason: Option<String>,
}
pub trait Driver {
    /// Whether the CAN worker should add a retry delay after an empty/timeout read.
    /// Drivers with their own bounded receive wait (notably FIL) return false.
    fn needs_read_retry_sleep(&self) -> bool {
        true
    }

    fn read_frames(&mut self) -> DriverResult<Vec<frame::CanFrame>>;
    fn write_frame(&mut self, tx_frame: frame::CanFrameTx) -> DriverResult<()>;

    fn bus_speed(&self) -> Option<connection::CanBusSpeed> {
        None
    }

    fn close(&mut self) -> DriverResult<()> {
        Ok(())
    }
    fn set_fil_trace_bus(&mut self, _trace_bus: Option<String>) -> DriverResult<()> {
        Ok(())
    }
    fn take_fil_gpio_events(&mut self) -> Vec<FilGpioEvent> {
        Vec::new()
    }
    fn take_fil_expectation_events(&mut self) -> Vec<FilExpectationEvent> { Vec::new() }
    fn set_gpio(
        &mut self,
        _board: &str,
        _port: &str,
        _pin: u8,
        _value: Option<bool>,
    ) -> DriverResult<()> {
        Err(DriverError::Unsupported(
            "GPIO control is not supported by this source".into(),
        ))
    }
    fn set_adc(
        &mut self,
        _board: &str,
        _instance: &str,
        _channel: u8,
        _value: u16,
    ) -> DriverResult<()> {
        Err(DriverError::Unsupported(
            "ADC injection is not supported by this source".into(),
        ))
    }
}

pub fn create_driver(
    source: &connection::ConnectionSource,
    bus: Option<&superdbc::database::Bus>,
) -> DriverResult<Box<dyn Driver>> {
    match source {
        connection::ConnectionSource::Serial(path, speed) => {
            Ok(Box::new(serial::SerialDriver::new(path, *speed)?))
        }
        connection::ConnectionSource::Udp(port) => {
            let socket = net::UdpSocket::bind(("0.0.0.0", *port))
                .map_err(|e| DriverError::ConnectionFailed(e.to_string()))?;
            socket
                .set_read_timeout(Some(time::Duration::from_millis(10)))
                .map_err(|e| DriverError::ConnectionFailed(e.to_string()))?;
            Ok(Box::new(UdpDriver(socket)))
        }
        connection::ConnectionSource::Loopback => Ok(Box::new(LoopbackDriver::default())),
        connection::ConnectionSource::Fil {
            executable,
            network,
            bus,
            elf_overrides,
            disabled_boards,
            built_network,
            run_options,
            trace_bus,
        } => {
            let effective_network = if let Some(spec) = built_network {
                crate::fil_config::build_network(spec, executable).map(|(path, _)| path)
            } else {
                let disabled = disabled_boards
                    .iter()
                    .cloned()
                    .collect::<std::collections::HashSet<_>>();
                crate::fil_config::materialize_network(network, elf_overrides, &disabled)
            }
            .map_err(|error| {
                DriverError::ConnectionFailed(format!("Invalid FIL network: {error}"))
            })?;
            Ok(Box::new(FilDriver::new(
                executable,
                &effective_network,
                bus,
                run_options,
                trace_bus.clone(),
            )?))
        }
        connection::ConnectionSource::Simulated(true) => {
            let messages = bus.map(|bus| bus.messages().to_vec()).unwrap_or_default();
            Ok(Box::new(SimulatedDriver {
                messages,
                next: time::Instant::now(),
            }))
        }
        _ => Err(DriverError::ConnectionFailed(format!(
            "source unavailable: {}",
            source.display_name()
        ))),
    }
}

#[derive(Default)]
struct LoopbackDriver {
    queued: Vec<frame::CanFrame>,
}

struct FilDriver {
    child: std::process::Child,
    input: std::io::BufWriter<std::process::ChildStdin>,
    output: std::sync::mpsc::Receiver<Result<CanFrame, String>>,
    gpio_output: std::sync::mpsc::Receiver<FilGpioEvent>,
    expectation_output: std::sync::mpsc::Receiver<FilExpectationEvent>,
    trace_bus: std::sync::Arc<std::sync::RwLock<Option<String>>>,
    bus: String,
}
impl FilDriver {
    fn new(
        executable: &std::path::Path,
        network: &std::path::Path,
        bus: &str,
        run_options: &crate::connection::FilRunOptions,
        trace_bus: Option<String>,
    ) -> DriverResult<Self> {
        if !executable.is_file() {
            return Err(DriverError::ConnectionFailed(format!(
                "FIL executable does not exist: {}",
                executable.display()
            )));
        }
        if !network.is_file() {
            return Err(DriverError::ConnectionFailed(format!(
                "FIL network config does not exist: {}",
                network.display()
            )));
        }
        if bus.is_empty() || bus.contains([':', '\n', '\r']) {
            return Err(DriverError::ConnectionFailed(
                "FIL bus name is empty or contains an invalid character".into(),
            ));
        }
        let args = watch_network_args(run_options)?;
        let mut child = std::process::Command::new(executable)
            .arg("watch-network")
            .arg(network)
            .args(args)
            .stdin(std::process::Stdio::piped())
            .stdout(std::process::Stdio::piped())
            .stderr(std::process::Stdio::piped())
            .spawn()
            .map_err(|e| DriverError::ConnectionFailed(format!("Failed to launch FIL: {e}")))?;
        let stdout = child
            .stdout
            .take()
            .ok_or_else(|| DriverError::ConnectionFailed("Failed to capture FIL output".into()))?;
        let stdin = child.stdin.take().ok_or_else(|| {
            DriverError::ConnectionFailed("Failed to open FIL control input".into())
        })?;
        if let Some(stderr) = child.stderr.take() {
            std::thread::spawn(move || {
                use std::io::BufRead;
                for line in std::io::BufReader::new(stderr).lines() {
                    match line {
                        Ok(line) => log::warn!("FIL: {line}"),
                        Err(error) => {
                            log::warn!("Failed to read FIL stderr: {error}");
                            break;
                        }
                    }
                }
            });
        }
        let (output_tx, output) = std::sync::mpsc::channel();
        let (gpio_tx, gpio_output) = std::sync::mpsc::channel();
        let (expectation_tx, expectation_output) = std::sync::mpsc::channel();
        let trace_bus = std::sync::Arc::new(std::sync::RwLock::new(trace_bus));
        let reader_trace_bus = trace_bus.clone();
        std::thread::spawn(move || {
            use std::io::BufRead;
            let mut reader = std::io::BufReader::new(stdout);
            let mut line = String::new();
            loop {
                line.clear();
                match reader.read_line(&mut line) {
                    Ok(0) => {
                        let _ = output_tx.send(Err("FIL process exited".into()));
                        break;
                    }
                    Ok(_) => {
                        if let Some(event) = parse_fil_expectation(&line) {
                            if expectation_tx.send(event).is_err() { break; }
                            continue;
                        }
                        let selected_bus = reader_trace_bus
                            .read()
                            .unwrap_or_else(std::sync::PoisonError::into_inner)
                            .clone();
                        let Some(source) = line.split_ascii_whitespace().nth(2) else {
                            continue;
                        };
                        if !trace_source_matches(source, selected_bus.as_deref()) {
                            continue;
                        }
                        if let Some(event) = parse_fil_gpio(&line) {
                            if gpio_tx.send(event).is_err() {
                                break;
                            }
                            continue;
                        }
                        if let Some(frame) = parse_fil_can_tx(&line)
                            && output_tx.send(Ok(frame)).is_err()
                        {
                            break;
                        }
                    }
                    Err(error) => {
                        let _ = output_tx.send(Err(format!("Failed to read FIL output: {error}")));
                        break;
                    }
                }
            }
        });
        Ok(Self {
            child,
            input: std::io::BufWriter::new(stdin),
            output,
            gpio_output,
            expectation_output,
            trace_bus,
            bus: bus.into(),
        })
    }
}
fn watch_network_args(options: &crate::connection::FilRunOptions) -> DriverResult<Vec<String>> {
    if options.duration_ms > u64::MAX / 1_000_000
        || options.max_instructions == 0
        || options.quantum == 0
        || options.refresh_ms == 0
        || options.refresh_ms > i32::MAX as u32
        || !(1..=1024).contains(&options.adc_decimation)
    {
        return Err(DriverError::ConnectionFailed(
            "Invalid FIL run options: duration, instruction limit, quantum, or refresh interval"
                .into(),
        ));
    }
    let mut args = vec![
        "--duration-ms".into(),
        options.duration_ms.to_string(),
        "--max-instructions".into(),
        options.max_instructions.to_string(),
        "--quantum".into(),
        options.quantum.to_string(),
        "--refresh-ms".into(),
        options.refresh_ms.to_string(),
        "--adc-decimation".into(),
        options.adc_decimation.to_string(),
    ];
    if options.strict_mmio {
        args.push("--strict-mmio".into());
    }
    if !options.wall_pacing {
        args.push("--no-wall-pacing".into());
    }
    if !options.loop_batching {
        args.push("--no-loop-batching".into());
    }
    if options.trace_instructions {
        args.push("--trace-instr".into());
    }
    if options.detect_spin {
        args.push("--detect-spin".into());
    }
    for filter in ["can_tx", "gpio_input", "gpio_output", "expectation_pending", "expectation_pass", "expectation_fail", "expectation_incomplete"] {
        args.extend(["--live-filter".into(), filter.into()]);
    }
    for filter in options
        .extra_live_filters
        .split(',')
        .map(str::trim)
        .filter(|s| !s.is_empty())
    {
        if filter.chars().any(char::is_whitespace) || filter.starts_with('-') {
            return Err(DriverError::ConnectionFailed(format!(
                "Invalid FIL live filter: {filter}"
            )));
        }
        args.extend(["--live-filter".into(), filter.into()]);
    }
    args.push("--control-stdin".into());
    Ok(args)
}

fn parse_fil_expectation(line: &str) -> Option<FilExpectationEvent> {
    const KEYS: &[&str] = &["check_id","script","expected_bus","expected_id","expected_extended","expected_data","window_start_ns","window_end_ns","matched_bus","matched_id","matched_data","matched_origin","matched_time_ns","reason"];
    let tokens: Vec<_> = line.split_ascii_whitespace().collect();
    let index = tokens.iter().position(|t| matches!(*t,"expectation_pending"|"expectation_pass"|"expectation_fail"|"expectation_incomplete"))?;
    let status = match tokens[index] { "expectation_pending"=>FilExpectationStatus::Pending,"expectation_pass"=>FilExpectationStatus::Pass,"expectation_fail"=>FilExpectationStatus::Fail,_=>FilExpectationStatus::Incomplete };
    let mut fields = std::collections::HashMap::<&str,String>::new(); let mut current=None;
    for token in tokens.iter().skip(index+1) {
        if let Some((key,value))=token.split_once('=') && KEYS.contains(&key) { current=Some(key); fields.insert(key,value.to_owned()); }
        else if let Some(key)=current { let value=fields.get_mut(key)?; value.push(' '); value.push_str(token); }
    }
    let v=|key:&str| fields.get(key).map(String::as_str);
    let id=|s:&str| u32::from_str_radix(s.trim_start_matches("0x"),16).ok();
    let data=|s:&str| { let b=s.trim(); if b.len()%2!=0{return None} b.as_bytes().chunks_exact(2).map(|c|u8::from_str_radix(std::str::from_utf8(c).ok()?,16).ok()).collect::<Option<Vec<_>>>() };
    Some(FilExpectationEvent { check_id:v("check_id")?.to_owned(),script:v("script")?.to_owned(),status,expected_bus:v("expected_bus")?.to_owned(),expected_id:id(v("expected_id")?)?,expected_extended:v("expected_extended")?.parse().ok()?,expected_data:data(v("expected_data")?)?,window_start_ns:v("window_start_ns")?.parse().ok()?,window_end_ns:v("window_end_ns")?.parse().ok()?,matched_bus:v("matched_bus").map(str::to_owned),matched_id:v("matched_id").and_then(id),matched_data:v("matched_data").and_then(data),matched_origin:v("matched_origin").map(str::to_owned),matched_time_ns:v("matched_time_ns").and_then(|s|s.parse().ok()),reason:v("reason").map(str::to_owned)})
}

fn parse_fil_can_tx(line: &str) -> Option<CanFrame> {
    let mut fields = line.split_ascii_whitespace();
    if !fields.any(|field| field == "can_tx") {
        return None;
    }
    let mut id = None;
    let mut extended = false;
    let mut fd = false;
    let mut data = None;
    for field in fields {
        let (key, value) = field.split_once('=')?;
        match key {
            "id" => id = u32::from_str_radix(value.trim_start_matches("0x"), 16).ok(),
            "extended" => extended = value == "true",
            "fd" => fd = value == "true",
            "data" => {
                if value.len() % 2 != 0 || value.len() > 16 {
                    return None;
                }
                data = Some(
                    value
                        .as_bytes()
                        .chunks_exact(2)
                        .map(|pair| {
                            std::str::from_utf8(pair)
                                .ok()
                                .and_then(|hex| u8::from_str_radix(hex, 16).ok())
                        })
                        .collect::<Option<Vec<_>>>()?,
                );
            }
            _ => {}
        }
    }
    if fd {
        return None;
    }
    let identity = crate::frame::CanIdentity::new(id?, extended).ok()?;
    CanFrame::data(identity, data?).ok()
}
fn trace_source_matches(source: &str, trace_bus: Option<&str>) -> bool {
    trace_bus.is_none_or(|bus| {
        source
            .strip_prefix(bus)
            .is_some_and(|suffix| suffix.starts_with('/'))
    })
}

fn parse_fil_gpio(line: &str) -> Option<FilGpioEvent> {
    let mut fields = line.split_ascii_whitespace();
    let _time = fields.next()?;
    let _unit = fields.next()?;
    let source = fields.next()?;
    let kind = fields.next()?;
    if kind != "gpio_input" && kind != "gpio_output" {
        return None;
    }
    let (board, port) = source.rsplit_once('.')?;
    let mut pin = None;
    let mut value = None;
    for field in fields {
        let (key, raw) = field.split_once('=')?;
        match key {
            "pin" => pin = raw.parse::<u8>().ok(),
            "value" => {
                value = match raw {
                    "0" => Some(Some(false)),
                    "1" => Some(Some(true)),
                    "release" => Some(None),
                    _ => return None,
                }
            }
            _ => {}
        }
    }
    Some(FilGpioEvent {
        board: board.into(),
        port: port.into(),
        pin: pin?,
        value: value?,
        direction: if kind == "gpio_output" {
            FilGpioDirection::Output
        } else {
            FilGpioDirection::Input
        },
    })
}

fn format_fil_injection(bus: &str, frame: CanFrame) -> DriverResult<String> {
    use crate::frame::FrameKind;
    if frame.kind != FrameKind::Data {
        return Err(DriverError::Write(
            "FIL injection supports only CAN 2.0 data frames".into(),
        ));
    }
    let mut command = format!("{bus}:0x{:x}:", frame.identity.raw_id());
    for byte in frame.data {
        command.push_str(&format!("{byte:02x}"));
    }
    Ok(command)
}
impl Driver for FilDriver {
    fn needs_read_retry_sleep(&self) -> bool {
        false
    }

    fn read_frames(&mut self) -> DriverResult<Vec<CanFrame>> {
        use std::sync::mpsc::RecvTimeoutError;
        let first = match self.output.recv_timeout(Duration::from_millis(50)) {
            Ok(frame) => frame,
            Err(RecvTimeoutError::Timeout) => return Err(DriverError::Timeout),
            Err(RecvTimeoutError::Disconnected) => {
                return Err(DriverError::Read("FIL output reader stopped".into()));
            }
        };
        let mut frames = match first {
            Ok(frame) => vec![frame],
            Err(error) => return Err(DriverError::Read(error)),
        };
        while frames.len() < 256 {
            let Ok(next) = self.output.try_recv() else {
                break;
            };
            if let Ok(frame) = next {
                frames.push(frame);
            }
        }
        Ok(frames)
    }
    fn write_frame(&mut self, frame: CanFrame) -> DriverResult<()> {
        use std::io::Write;
        let command = format_fil_injection(&self.bus, frame)?;
        writeln!(self.input, "{command}")
            .and_then(|_| self.input.flush())
            .map_err(|e| DriverError::Write(format!("Failed to send frame to FIL: {e}")))
    }
    fn close(&mut self) -> DriverResult<()> {
        let _ = self.child.kill();
        let _ = self.child.wait();
        Ok(())
    }
    fn set_fil_trace_bus(&mut self, trace_bus: Option<String>) -> DriverResult<()> {
        *self
            .trace_bus
            .write()
            .unwrap_or_else(std::sync::PoisonError::into_inner) = trace_bus;
        Ok(())
    }
    fn take_fil_gpio_events(&mut self) -> Vec<FilGpioEvent> {
        self.gpio_output.try_iter().take(256).collect()
    }
    fn take_fil_expectation_events(&mut self) -> Vec<FilExpectationEvent> { self.expectation_output.try_iter().take(256).collect() }
    fn set_gpio(
        &mut self,
        board: &str,
        port: &str,
        pin: u8,
        value: Option<bool>,
    ) -> DriverResult<()> {
        use std::io::Write;
        if board.is_empty()
            || port.is_empty()
            || pin > 15
            || board.contains(char::is_whitespace)
            || port.contains(char::is_whitespace)
        {
            return Err(DriverError::Write(
                "GPIO control requires board, port, and pin 0..15".into(),
            ));
        }
        let value = value
            .map(|v| if v { "1" } else { "0" })
            .unwrap_or("release");
        writeln!(self.input, "gpio {board} {port} {pin} {value}")
            .and_then(|_| self.input.flush())
            .map_err(|e| DriverError::Write(format!("Failed to send GPIO value to FIL: {e}")))
    }
    fn set_adc(
        &mut self,
        board: &str,
        instance: &str,
        channel: u8,
        value: u16,
    ) -> DriverResult<()> {
        use std::io::Write;
        if board.is_empty()
            || instance.is_empty()
            || channel > 19
            || value > 4095
            || board.contains(char::is_whitespace)
            || instance.contains(char::is_whitespace)
        {
            return Err(DriverError::Write(
                "ADC injection requires a board, ADC instance, channel 0..19, and value 0..4095"
                    .into(),
            ));
        }
        writeln!(self.input, "adc {board} {instance} {channel} {value}")
            .and_then(|_| self.input.flush())
            .map_err(|e| DriverError::Write(format!("Failed to send ADC value to FIL: {e}")))
    }
}
impl Drop for FilDriver {
    fn drop(&mut self) {
        let _ = self.child.kill();
        let _ = self.child.wait();
    }
}

impl Driver for LoopbackDriver {
    fn read_frames(&mut self) -> DriverResult<Vec<frame::CanFrame>> {
        if self.queued.is_empty() {
            Err(DriverError::Timeout)
        } else {
            Ok(mem::take(&mut self.queued))
        }
    }

    fn write_frame(&mut self, tx_frame: frame::CanFrameTx) -> DriverResult<()> {
        self.queued.push(tx_frame.frame);
        Ok(())
    }
}

struct UdpDriver(net::UdpSocket);

impl Driver for UdpDriver {
    fn read_frames(&mut self) -> DriverResult<Vec<frame::CanFrame>> {
        let mut buf = [0; 65536];
        let len = self.0.recv(&mut buf).map_err(io_read)?;
        parse_udp_buffer(&buf[..len])
    }

    fn write_frame(&mut self, _: frame::CanFrameTx) -> DriverResult<()> {
        Err(DriverError::Unsupported(
            "UDP transmission is unsupported".into(),
        ))
    }
}

fn io_read(e: io::Error) -> DriverError {
    if matches!(
        e.kind(),
        io::ErrorKind::WouldBlock | io::ErrorKind::TimedOut
    ) {
        DriverError::Timeout
    } else {
        DriverError::Read(e.to_string())
    }
}

fn parse_udp_buffer(buf: &[u8]) -> DriverResult<Vec<frame::CanFrame>> {
    if buf.len() < 16 || !buf.len().is_multiple_of(16) {
        return Err(DriverError::Read(
            "UDP packet must contain complete 16-byte records".into(),
        ));
    }
    buf.as_chunks::<16>()
        .0
        .iter()
        .map(|bytes| {
            let raw = bytemuck::try_pod_read_unaligned::<log_parse::parse::RawFrame>(bytes)
                .map_err(|error| DriverError::Read(error.to_string()))?;
            frame::CanFrame::data(raw.can_identity(), raw.data.to_vec()).map_err(DriverError::Read)
        })
        .collect()
}

struct SimulatedDriver {
    messages: Vec<superdbc::message::Message>,
    next: time::Instant,
}

impl Driver for SimulatedDriver {
    fn read_frames(&mut self) -> DriverResult<Vec<frame::CanFrame>> {
        use rand::Rng as _;
        use rand::RngExt as _;
        use rand::prelude::IndexedRandom as _;

        let now = time::Instant::now();
        if now < self.next {
            return Err(DriverError::Timeout);
        }
        self.next += time::Duration::from_millis(1);
        let mut rng = rand::rng();

        let messages = &self.messages;

        let Some(message) = messages.choose(&mut rng) else {
            let identity = rng.random_range(0..=can::STANDARD_ID_MASK);
            let mut data = vec![0u8; 8];
            rng.fill_bytes(&mut data);
            let frame = frame::CanFrame::data(identity, data).map_err(DriverError::Read)?;
            return Ok(vec![frame]);
        };

        Self::message_frame(message, &mut rng).map(|frame| vec![frame])
    }

    fn write_frame(&mut self, _: frame::CanFrameTx) -> DriverResult<()> {
        Ok(())
    }
}

impl SimulatedDriver {
    fn message_frame(
        message: &superdbc::message::Message,
        rng: &mut impl rand::Rng,
    ) -> DriverResult<frame::CanFrame> {
        let identity = message.identity();
        let size = usize::from(message.length_bytes());

        if size > 8 {
            return Err(DriverError::Timeout);
        }

        let mut data = vec![0u8; size];
        if message.signals.is_empty() {
            rng.fill_bytes(&mut data);
        } else {
            for signal in &message.signals {
                let bits = Self::signal_bits(signal, rng);

                signal.codec.insert(&mut data, bits);
            }
        }

        let frame = frame::CanFrame::data(identity, data).map_err(DriverError::Read)?;

        Ok(frame)
    }

    fn signal_bits(signal: &superdbc::message::SignalDefinition, rng: &mut impl rand::Rng) -> u64 {
        if let Some((raw, _)) = signal.choices().choose(rng) {
            return raw as u64;
        }
        match signal.raw_type() {
            superdbc::message::RawType::Float32 => Self::float_bits(signal, rng),
            _ => Self::integer_bits(signal, rng),
        }
    }

    // Sample a representable raw integer within physical limits when possible
    fn integer_bits(signal: &superdbc::message::SignalDefinition, rng: &mut impl rand::Rng) -> u64 {
        let (mut lower, mut upper) = (signal.codec.min, signal.codec.max);
        if let Some(limits) = signal.limits() {
            let r1 = (limits.min - signal.offset()) / signal.scale();
            let r2 = (limits.max - signal.offset()) / signal.scale();
            let low = r1.min(r2).ceil();
            let high = r1.max(r2).floor();
            // Limits are presentation metadata and may contain no wire value.
            // In that case retain the full representable range.
            if low <= high && low <= upper as f64 && high >= lower as f64 {
                let candidate_lower = lower.max(low as i128);
                let candidate_upper = upper.min(high as i128);
                if candidate_lower <= candidate_upper {
                    lower = candidate_lower;
                    upper = candidate_upper;
                }
            }
        }
        rng.random_range(lower..=upper) as u64
    }

    // Sample a finite raw float within physical limits when possible and return its wire bits
    fn float_bits(signal: &superdbc::message::SignalDefinition, rng: &mut impl rand::Rng) -> u64 {
        let (r1, r2) = match signal.limits() {
            Some(limits) => (
                (limits.min - signal.offset()) / signal.scale(),
                (limits.max - signal.offset()) / signal.scale(),
            ),
            None => (-f64::from(f32::MAX), f64::from(f32::MAX)),
        };
        let low = r1.min(r2).max(-f64::from(f32::MAX));
        let high = r1.max(r2).min(f64::from(f32::MAX));
        let mut lower = low as f32;
        let mut upper = high as f32;
        if f64::from(lower) < low {
            lower = lower.next_up();
        }
        if f64::from(upper) > high {
            upper = upper.next_down();
        }
        if !lower.is_finite() || !upper.is_finite() || lower > upper {
            lower = -f32::MAX;
            upper = f32::MAX;
        }
        let raw = rng.random_range(f64::from(lower)..=f64::from(upper)) as f32;
        u64::from(raw.to_bits())
    }
}

mod serial {
    use std::time;

    use crate::{can, connection, frame};

    pub struct SerialDriver {
        socket: slcan::sync::CanSocket<Box<dyn serialport::SerialPort>>,
        speed: connection::CanBusSpeed,
    }

    impl SerialDriver {
        pub fn new(path: &str, speed: connection::CanBusSpeed) -> can::driver::DriverResult<Self> {
            let port = serialport::new(path, 115200)
                .timeout(time::Duration::from_millis(10))
                .open()
                .map_err(|e| can::driver::DriverError::ConnectionFailed(e.to_string()))?;
            let _ = port.clear(serialport::ClearBuffer::All);
            let mut socket = slcan::sync::CanSocket::new(port);
            socket
                .set_operating_mode(slcan::OperatingMode::Normal)
                .map_err(|e| can::driver::DriverError::ConnectionFailed(e.to_string()))?;
            let bitrate = match speed {
                connection::CanBusSpeed::Kbps250 => slcan::NominalBitRate::Rate250Kbit,
                connection::CanBusSpeed::Kbps500 => slcan::NominalBitRate::Rate500Kbit,
            };
            socket
                .open(bitrate)
                .map_err(|e| can::driver::DriverError::ConnectionFailed(e.to_string()))?;
            Ok(Self { socket, speed })
        }
    }

    fn to_wire_id(raw_id: u32, is_extended_id: bool) -> Option<slcan::Id> {
        if is_extended_id {
            slcan::ExtendedId::new(raw_id).map(slcan::Id::Extended)
        } else {
            u16::try_from(raw_id)
                .ok()
                .and_then(slcan::StandardId::new)
                .map(slcan::Id::Standard)
        }
    }

    fn identity(id: slcan::Id) -> frame::CanIdentity {
        match id {
            slcan::Id::Standard(id) => id.as_raw() as u32,
            slcan::Id::Extended(id) => id.as_raw(),
        }
    }

    fn from_wire(frame: slcan::CanFrame) -> can::driver::DriverResult<frame::CanFrame> {
        let frame = match frame {
            slcan::CanFrame::Can2(f) => {
                let identity = identity(f.id());
                let kind = if f.is_remote() {
                    frame::FrameKind::Remote
                } else {
                    frame::FrameKind::Data
                };

                frame::CanFrame {
                    identity,
                    kind,
                    dlc: f.dlc() as u8,
                    data: f.data().unwrap_or(&[]).to_vec(),
                }
            }
            slcan::CanFrame::CanFd(f) => {
                let identity = identity(f.id());
                frame::CanFrame {
                    identity,
                    kind: frame::FrameKind::Fd {
                        bit_rate_switched: f.is_bit_rate_switched(),
                    },
                    dlc: f.dlc() as u8,
                    data: f.data().to_vec(),
                }
            }
        };

        Ok(frame)
    }

    impl can::driver::Driver for SerialDriver {
        fn read_frames(&mut self) -> can::driver::DriverResult<Vec<frame::CanFrame>> {
            self.socket
                .read()
                .map_err(|e| match e {
                    slcan::ReadError::Io(e) => can::driver::io_read(e),
                    other => can::driver::DriverError::Read(format!("{other:?}")),
                })
                .and_then(from_wire)
                .map(|frame| vec![frame])
        }

        fn write_frame(&mut self, tx_frame: frame::CanFrameTx) -> can::driver::DriverResult<()> {
            let id = to_wire_id(tx_frame.frame.identity, tx_frame.is_extended_id)
                .ok_or_else(|| can::driver::DriverError::Write("invalid ID".into()))?;
            let f = tx_frame.frame;
            let wire: slcan::CanFrame = match f.kind {
                frame::FrameKind::Data => slcan::Can2Frame::new_data(id, &f.data).map(Into::into),
                frame::FrameKind::Remote => {
                    slcan::Can2Frame::new_remote(id, f.dlc as usize).map(Into::into)
                }
                frame::FrameKind::Fd { bit_rate_switched } => slcan::CanFdFrame::new(id, &f.data)
                    .map(|f| f.with_bit_rate_switched(bit_rate_switched).into()),
            }
            .ok_or_else(|| can::driver::DriverError::Write("invalid payload".into()))?;
            self.socket
                .send(wire)
                .map_err(|e| can::driver::DriverError::Write(e.to_string()))
        }

        fn bus_speed(&self) -> Option<connection::CanBusSpeed> {
            Some(self.speed)
        }

        fn close(&mut self) -> can::driver::DriverResult<()> {
            self.socket
                .close()
                .map_err(|e| can::driver::DriverError::Write(e.to_string()))
        }
    }
}
