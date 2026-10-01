use crate::can::driver::{CanDriver, DriverError, DriverReadError, DriverResult};
use crate::connection::CanBusSpeed;
use crate::messages::{FilAdcInstance, FilExpectationEvent, FilExpectationStatus, FilGpioPort};
use slcan::CanFrame;
use std::collections::HashSet;
use std::io::{BufReader, Read, Write};
use std::process::{Child, ChildStdin, Command, Stdio};
use std::sync::atomic::{AtomicU32, Ordering};
use std::sync::mpsc::{self, Receiver, RecvTimeoutError};
use std::sync::{Arc, Mutex, RwLock};
use std::thread;
use std::time::{Duration, Instant};

const FIL_READ_TIMEOUT_MS: u64 = 1;
const FIL_MAX_FRAMES_PER_POLL: usize = 256;
const FIL_MAX_EXPECTATION_EVENTS: usize = 256;
const FIL_STARTUP_TIMEOUT: Duration = Duration::from_secs(30);
const FIL_SHUTDOWN_TIMEOUT: Duration = Duration::from_millis(250);
const MAGIC: &[u8; 4] = b"FILN";
const VERSION: u8 = 1;
const MAX_PAYLOAD: usize = 65_536;
const CAN_INJECT: u8 = 1;
const ADC_SET: u8 = 2;
const GPIO_SET: u8 = 3;
const STOP: u8 = 4;
const HELLO: u8 = 0x80;
const REPLY: u8 = 0x81;
const TRACE: u8 = 0x82;
const END: u8 = 0x83;

#[derive(Debug)]
pub struct FilGpioEvent {
    pub board: String,
    pub port: FilGpioPort,
    pub pin: u8,
    pub value: Option<bool>,
    pub output: bool,
}

#[derive(Clone, Debug)]
struct FilCanEvent {
    source: String,
    frame: CanFrame,
}

#[derive(Debug)]
struct Frame {
    kind: u8,
    request_id: u32,
    payload: Vec<u8>,
}

fn fil_network_args(options: &crate::settings::FilRunOptions) -> DriverResult<Vec<String>> {
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
        "--transport".into(),
        "stdio".into(),
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
    for filter in [
        "can_tx",
        "gpio_input",
        "gpio_output",
        "expectation_pending",
        "expectation_pass",
        "expectation_fail",
        "expectation_incomplete",
    ] {
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
    Ok(args)
}

fn read_frame(reader: &mut impl Read) -> Result<Option<Frame>, String> {
    let mut header = [0u8; 16];
    let mut first = [0u8; 1];
    loop {
        match reader.read(&mut first) {
            Ok(0) => return Ok(None),
            Ok(1) => {
                header[0] = first[0];
                break;
            }
            Ok(_) => unreachable!(),
            Err(e) if e.kind() == std::io::ErrorKind::Interrupted => continue,
            Err(e) => return Err(format!("Failed to read FIL frame header: {e}")),
        }
    }
    reader
        .read_exact(&mut header[1..])
        .map_err(|e| format!("Truncated FIL frame header: {e}"))?;
    if &header[0..4] != MAGIC {
        return Err("Invalid FIL frame magic".into());
    }
    if header[4] != VERSION {
        return Err(format!("Unsupported FIL protocol version {}", header[4]));
    }
    if header[6..8] != [0, 0] {
        return Err("FIL frame has nonzero flags".into());
    }
    let length = u32::from_le_bytes(header[8..12].try_into().unwrap()) as usize;
    if length > MAX_PAYLOAD {
        return Err(format!("FIL frame payload exceeds {MAX_PAYLOAD} bytes"));
    }
    let request_id = u32::from_le_bytes(header[12..16].try_into().unwrap());
    let mut payload = vec![0; length];
    reader
        .read_exact(&mut payload)
        .map_err(|e| format!("Truncated FIL frame payload: {e}"))?;
    Ok(Some(Frame {
        kind: header[5],
        request_id,
        payload,
    }))
}

fn write_frame(
    writer: &mut impl Write,
    kind: u8,
    request_id: u32,
    payload: &[u8],
) -> std::io::Result<()> {
    if payload.len() > MAX_PAYLOAD {
        return Err(std::io::Error::new(
            std::io::ErrorKind::InvalidInput,
            "FIL frame payload exceeds 65536 bytes",
        ));
    }
    let mut header = [0u8; 16];
    header[0..4].copy_from_slice(MAGIC);
    header[4] = VERSION;
    header[5] = kind;
    header[8..12].copy_from_slice(&(payload.len() as u32).to_le_bytes());
    header[12..16].copy_from_slice(&request_id.to_le_bytes());
    writer.write_all(&header)?;
    writer.write_all(payload)?;
    writer.flush()
}

struct PayloadReader<'a> {
    bytes: &'a [u8],
    at: usize,
}
impl<'a> PayloadReader<'a> {
    fn new(bytes: &'a [u8]) -> Self {
        Self { bytes, at: 0 }
    }
    fn take(&mut self, n: usize) -> Result<&'a [u8], String> {
        let end = self
            .at
            .checked_add(n)
            .ok_or("FIL payload length overflow")?;
        let out = self
            .bytes
            .get(self.at..end)
            .ok_or("Truncated FIL payload")?;
        self.at = end;
        Ok(out)
    }
    fn u16(&mut self) -> Result<u16, String> {
        Ok(u16::from_le_bytes(self.take(2)?.try_into().unwrap()))
    }
    fn u32(&mut self) -> Result<u32, String> {
        Ok(u32::from_le_bytes(self.take(4)?.try_into().unwrap()))
    }
    fn u64(&mut self) -> Result<u64, String> {
        Ok(u64::from_le_bytes(self.take(8)?.try_into().unwrap()))
    }
    fn string(&mut self) -> Result<&'a str, String> {
        let n = self.u16()? as usize;
        std::str::from_utf8(self.take(n)?).map_err(|e| format!("FIL string is not UTF-8: {e}"))
    }
    fn done(&self) -> Result<(), String> {
        if self.at == self.bytes.len() {
            Ok(())
        } else {
            Err("Unexpected trailing bytes in FIL payload".into())
        }
    }
}

fn put_string(out: &mut Vec<u8>, value: &str) -> DriverResult<()> {
    let len = u16::try_from(value.len())
        .map_err(|_| DriverError::WriteError("FIL string exceeds 65535 bytes".into()))?;
    out.extend_from_slice(&len.to_le_bytes());
    out.extend_from_slice(value.as_bytes());
    Ok(())
}
fn request_payload(
    kind: u8,
    bus_or_board: &str,
    second: Option<&str>,
    a: u32,
    b: u8,
    c: Option<&[u8]>,
) -> DriverResult<Vec<u8>> {
    let mut payload = Vec::new();
    match kind {
        CAN_INJECT => {
            put_string(&mut payload, bus_or_board)?;
            payload.extend_from_slice(&a.to_le_bytes());
            payload.push(b);
            let data = c.unwrap_or_default();
            payload.push(
                u8::try_from(data.len())
                    .map_err(|_| DriverError::WriteError("CAN payload too long".into()))?,
            );
            payload.extend_from_slice(data);
        }
        ADC_SET => {
            put_string(&mut payload, bus_or_board)?;
            put_string(&mut payload, second.unwrap_or_default())?;
            payload.push(b);
            payload.extend_from_slice(&(a as u16).to_le_bytes());
        }
        GPIO_SET => {
            put_string(&mut payload, bus_or_board)?;
            put_string(&mut payload, second.unwrap_or_default())?;
            payload.push(b);
            payload.push(a as u8);
        }
        STOP => {}
        _ => unreachable!(),
    }
    if payload.len() > MAX_PAYLOAD {
        return Err(DriverError::WriteError(
            "FIL request payload too large".into(),
        ));
    }
    Ok(payload)
}

#[derive(Clone, Copy)]
struct TraceFields<'a>([Option<&'a str>; 20]);

impl<'a> TraceFields<'a> {
    fn read(reader: &mut PayloadReader<'a>) -> Result<Self, String> {
        let count = reader.u16()? as usize;
        let mut fields = Self([None; 20]);
        for _ in 0..count {
            let key = reader.string()?;
            let value = reader.string()?;
            if let Some(index) = Self::index(key) {
                fields.0[index] = Some(value);
            }
        }
        Ok(fields)
    }

    fn index(key: &str) -> Option<usize> {
        Some(match key {
            "id" => 0,
            "extended" => 1,
            "fd" => 2,
            "data" => 3,
            "pin" => 4,
            "value" => 5,
            "check_id" => 6,
            "script" => 7,
            "expected_bus" => 8,
            "expected_id" => 9,
            "expected_extended" => 10,
            "expected_data" => 11,
            "window_start_ns" => 12,
            "window_end_ns" => 13,
            "matched_bus" => 14,
            "matched_id" => 15,
            "matched_data" => 16,
            "matched_origin" => 17,
            "matched_time_ns" => 18,
            "reason" => 19,
            _ => return None,
        })
    }

    fn required(&self, key: &str) -> Result<&'a str, String> {
        self.optional(key)
            .ok_or_else(|| format!("FIL trace is missing '{key}' field"))
    }

    fn optional(&self, key: &str) -> Option<&'a str> {
        Self::index(key).and_then(|index| self.0[index])
    }
}
fn parse_hex_u32(s: &str) -> Result<u32, String> {
    u32::from_str_radix(s.trim_start_matches("0x"), 16)
        .map_err(|e| format!("Invalid FIL trace identifier '{s}': {e}"))
}
fn parse_hex_data(s: &str) -> Result<Vec<u8>, String> {
    fn nibble(byte: u8) -> Option<u8> {
        match byte {
            b'0'..=b'9' => Some(byte - b'0'),
            b'a'..=b'f' => Some(byte - b'a' + 10),
            b'A'..=b'F' => Some(byte - b'A' + 10),
            _ => None,
        }
    }

    let bytes = s.as_bytes();
    if !bytes.len().is_multiple_of(2) {
        return Err("FIL trace data has an odd number of hex digits".into());
    }
    bytes
        .chunks_exact(2)
        .map(|pair| {
            let high = nibble(pair[0]).ok_or("Invalid FIL trace data")?;
            let low = nibble(pair[1]).ok_or("Invalid FIL trace data")?;
            Ok((high << 4) | low)
        })
        .collect()
}
fn parse_trace(
    frame: Frame,
    can_tx: &mpsc::Sender<Result<FilCanEvent, String>>,
    gpio_tx: &mpsc::Sender<FilGpioEvent>,
    expectation_tx: &mpsc::Sender<FilExpectationEvent>,
    trace_bus: &Arc<RwLock<Option<String>>>,
) -> Result<(), String> {
    if frame.request_id != 0 {
        return Err("FIL TRACE has nonzero request ID".into());
    }
    let mut r = PayloadReader::new(&frame.payload);
    let _time = r.u64()?;
    let _sequence = r.u64()?;
    let source = r.string()?;
    let kind = r.string()?;
    let fields = TraceFields::read(&mut r)?;
    r.done()?;
    if kind == "can_tx" {
        if source
            .split_once('/')
            .is_none_or(|(bus, endpoint)| bus.is_empty() || endpoint.is_empty())
        {
            return Ok(());
        }
        let id = parse_hex_u32(fields.required("id")?)?;
        if fields.optional("fd").unwrap_or("false") == "true" {
            return Ok(());
        }
        let extended = fields.optional("extended").unwrap_or("false") == "true";
        let data = parse_hex_data(fields.required("data")?)?;
        let frame_id = if extended {
            slcan::ExtendedId::new(id).map(slcan::Id::Extended)
        } else {
            u16::try_from(id)
                .ok()
                .and_then(|v| slcan::StandardId::new(v))
                .map(slcan::Id::Standard)
        }
        .ok_or("Invalid CAN identifier in FIL trace")?;
        let can = slcan::Can2Frame::new_data(frame_id, &data)
            .ok_or_else(|| "Invalid CAN data in FIL trace".to_owned())?
            .into();
        let selected_matches = {
            let selected = trace_bus
                .read()
                .unwrap_or_else(std::sync::PoisonError::into_inner);
            trace_source_matches(source, selected.as_deref())
        };
        if selected_matches {
            can_tx
                .send(Ok(FilCanEvent {
                    source: source.into(),
                    frame: can,
                }))
                .map_err(|_| "FIL CAN event receiver stopped".to_owned())?;
        }
    } else if kind == "gpio_input" || kind == "gpio_output" {
        let (board, port) = source
            .rsplit_once('.')
            .ok_or("Malformed GPIO trace source")?;
        let value = match fields.required("value")? {
            "0" => Some(false),
            "1" => Some(true),
            "release" => None,
            v => return Err(format!("Invalid GPIO trace value '{v}'")),
        };
        let port = FilGpioPort::parse(port).ok_or("Unknown GPIO port in FIL trace")?;
        let pin = fields
            .required("pin")?
            .parse::<u8>()
            .map_err(|_| "Invalid GPIO pin in FIL trace")?;
        if pin > 15 {
            return Err("GPIO pin in FIL trace is out of range".into());
        }
        gpio_tx
            .send(FilGpioEvent {
                board: board.into(),
                port,
                pin,
                value,
                output: kind == "gpio_output",
            })
            .map_err(|_| "FIL GPIO event receiver stopped")?;
    } else if let Some(status) = match kind {
        "expectation_pending" => Some(FilExpectationStatus::Pending),
        "expectation_pass" => Some(FilExpectationStatus::Pass),
        "expectation_fail" => Some(FilExpectationStatus::Fail),
        "expectation_incomplete" => Some(FilExpectationStatus::Incomplete),
        _ => None,
    } {
        let id = |key| parse_hex_u32(fields.required(key)?);
        let data = |key| parse_hex_data(fields.required(key)?);
        let event = FilExpectationEvent {
            check_id: fields.required("check_id")?.into(),
            script: fields.required("script")?.into(),
            status,
            expected_bus: fields.required("expected_bus")?.into(),
            expected_id: id("expected_id")?,
            expected_extended: fields.required("expected_extended")? == "true",
            expected_data: data("expected_data")?,
            window_start_ns: fields
                .required("window_start_ns")?
                .parse()
                .map_err(|_| "Invalid expectation start time")?,
            window_end_ns: fields
                .required("window_end_ns")?
                .parse()
                .map_err(|_| "Invalid expectation end time")?,
            matched_bus: fields.optional("matched_bus").map(str::to_owned),
            matched_id: fields
                .optional("matched_id")
                .and_then(|v| parse_hex_u32(v).ok()),
            matched_data: fields
                .optional("matched_data")
                .and_then(|v| parse_hex_data(v).ok()),
            matched_origin: fields.optional("matched_origin").map(str::to_owned),
            matched_time_ns: fields
                .optional("matched_time_ns")
                .and_then(|v| v.parse().ok()),
            reason: fields.optional("reason").map(str::to_owned),
        };
        expectation_tx
            .send(event)
            .map_err(|_| "FIL expectation event receiver stopped".to_owned())?;
    }
    Ok(())
}

fn parse_hello(frame: &Frame) -> Result<(), String> {
    if frame.kind != HELLO || frame.request_id != 0 {
        return Err("FIL service did not start with a valid HELLO frame".into());
    }
    let mut reader = PayloadReader::new(&frame.payload);
    reader.string()?;
    reader.u32()?;
    reader.u32()?;
    reader.done()
}

fn parse_reply(frame: &Frame, outstanding: &Mutex<HashSet<u32>>) -> Result<(), String> {
    if frame.request_id == 0 {
        return Err("FIL REPLY has zero request ID".into());
    }
    let mut reader = PayloadReader::new(&frame.payload);
    let status = reader.u16()?;
    reader.u64()?;
    let diagnostic = reader.string()?;
    reader.done()?;
    if status > 3 {
        return Err(format!("FIL REPLY has unknown status {status}"));
    }
    if !outstanding
        .lock()
        .unwrap_or_else(std::sync::PoisonError::into_inner)
        .remove(&frame.request_id)
    {
        return Err(format!(
            "FIL REPLY for unknown request {}",
            frame.request_id
        ));
    }
    if status != 0 {
        log::warn!(
            "FIL request {} rejected (status {status}): {diagnostic}",
            frame.request_id
        );
    }
    Ok(())
}

fn trace_source_matches(source: &str, trace_bus: Option<&str>) -> bool {
    trace_bus.is_none_or(|bus| {
        source
            .strip_prefix(bus)
            .is_some_and(|suffix| suffix.starts_with('/') && suffix.len() > 1)
    })
}

pub struct FilDriver {
    child: Child,
    input: Arc<Mutex<ChildStdin>>,
    output: Receiver<Result<FilCanEvent, String>>,
    gpio_output: Receiver<FilGpioEvent>,
    expectation_output: Receiver<FilExpectationEvent>,
    bus: String,
    trace_bus: Arc<RwLock<Option<String>>>,
    request_id: AtomicU32,
    outstanding_requests: Arc<Mutex<HashSet<u32>>>,
    connected: bool,
}
impl FilDriver {
    pub fn new(
        executable: &std::path::Path,
        network: &std::path::Path,
        bus: &str,
        trace_bus: &Option<String>,
        elf_overrides: &std::collections::HashMap<String, std::path::PathBuf>,
        disabled_boards: &[String],
        built_network: &Option<crate::fil_config::BuiltNetwork>,
        run_options: &crate::settings::FilRunOptions,
    ) -> DriverResult<Self> {
        if !executable.is_file() {
            return Err(DriverError::ConnectionFailed(format!(
                "FIL executable does not exist: {}",
                executable.display()
            )));
        }
        if bus.is_empty() || bus.contains([':', '\n', '\r']) {
            return Err(DriverError::ConnectionFailed(
                "FIL bus name is empty or contains an invalid character".into(),
            ));
        }
        let effective_network = match built_network {
            Some(spec) => {
                let (path, warnings) =
                    crate::fil_config::build_network(spec, executable).map_err(|e| {
                        DriverError::ConnectionFailed(format!("Invalid built FIL network: {e}"))
                    })?;
                for warning in warnings {
                    log::warn!("FIL: {warning}");
                }
                path
            }
            None => {
                if !network.is_file() {
                    return Err(DriverError::ConnectionFailed(format!(
                        "FIL network config does not exist: {}",
                        network.display()
                    )));
                }
                let disabled: std::collections::HashSet<String> =
                    disabled_boards.iter().cloned().collect();
                crate::fil_config::materialize_network(network, elf_overrides, &disabled).map_err(
                    |e| {
                        DriverError::ConnectionFailed(format!(
                            "Invalid FIL network {}: {e}",
                            network.display()
                        ))
                    },
                )?
            }
        };
        if !effective_network.is_file() {
            return Err(DriverError::ConnectionFailed(format!(
                "FIL network config does not exist: {}",
                effective_network.display()
            )));
        }
        let options = fil_network_args(run_options)?;
        let mut child = Command::new(executable)
            .arg("serve-network")
            .arg(&effective_network)
            .args(options)
            .stdin(Stdio::piped())
            .stdout(Stdio::piped())
            .stderr(Stdio::piped())
            .spawn()
            .map_err(|e| DriverError::ConnectionFailed(format!("Failed to launch FIL: {e}")))?;
        let stdout = child
            .stdout
            .take()
            .ok_or_else(|| DriverError::ConnectionFailed("Failed to capture FIL output".into()))?;
        let stdin = child.stdin.take().ok_or_else(|| {
            DriverError::ConnectionFailed("Failed to open FIL request input".into())
        })?;
        let stderr = child
            .stderr
            .take()
            .ok_or_else(|| DriverError::ConnectionFailed("Failed to capture FIL stderr".into()))?;
        thread::spawn(move || {
            for line in std::io::BufRead::lines(BufReader::new(stderr)) {
                match line {
                    Ok(line) => log::warn!("FIL: {line}"),
                    Err(e) => {
                        log::warn!("Failed to read FIL stderr: {e}");
                        break;
                    }
                }
            }
        });
        let (output_tx, output) = mpsc::channel();
        let (gpio_tx, gpio_output) = mpsc::channel();
        let (expectation_tx, expectation_output) = mpsc::channel();
        let (startup_tx, startup_rx) = mpsc::channel();
        let trace_bus = Arc::new(RwLock::new(trace_bus.clone()));
        let reader_trace_bus = Arc::clone(&trace_bus);
        let outstanding_requests = Arc::new(Mutex::new(HashSet::new()));
        let reader_outstanding = Arc::clone(&outstanding_requests);
        thread::spawn(move || {
            let mut reader = BufReader::new(stdout);
            let mut startup_tx = Some(startup_tx);
            loop {
                let frame = match read_frame(&mut reader) {
                    Ok(Some(frame)) => frame,
                    Ok(None) => {
                        let error = if startup_tx.is_some() {
                            "FIL process exited before HELLO"
                        } else {
                            "FIL process exited before END"
                        }
                        .to_owned();
                        if let Some(tx) = startup_tx.take() {
                            let _ = tx.send(Err(error.clone()));
                        }
                        let _ = output_tx.send(Err(error));
                        break;
                    }
                    Err(error) => {
                        if let Some(tx) = startup_tx.take() {
                            let _ = tx.send(Err(error.clone()));
                        }
                        let _ = output_tx.send(Err(error));
                        break;
                    }
                };
                if let Some(tx) = startup_tx.take() {
                    match parse_hello(&frame) {
                        Ok(()) => {
                            let _ = tx.send(Ok(()));
                            continue;
                        }
                        Err(error) => {
                            let _ = tx.send(Err(error.clone()));
                            let _ = output_tx.send(Err(error));
                            break;
                        }
                    }
                }
                let result = match frame.kind {
                    HELLO => Err("Unexpected duplicate FIL HELLO".into()),
                    TRACE => parse_trace(
                        frame,
                        &output_tx,
                        &gpio_tx,
                        &expectation_tx,
                        &reader_trace_bus,
                    ),
                    REPLY => parse_reply(&frame, &reader_outstanding),
                    END => {
                        if frame.request_id != 0 {
                            Err("FIL END has nonzero request ID".into())
                        } else {
                            let mut r = PayloadReader::new(&frame.payload);
                            let parsed = r
                                .u64()
                                .and_then(|_| r.take(1).map(|bytes| bytes[0]))
                                .and_then(|reason| r.take(1).map(|bytes| (reason, bytes[0])))
                                .and_then(|(reason, exit_code)| {
                                    r.string().map(|diagnostic| (reason, exit_code, diagnostic))
                                })
                                .and_then(|values| r.done().map(|_| values));
                            match parsed {
                                Ok((reason, exit_code, diagnostic)) if reason <= 4 => {
                                    Err(if diagnostic.is_empty() {
                                        format!(
                                            "FIL service ended (reason {reason}, exit code {exit_code})"
                                        )
                                    } else {
                                        format!("FIL service ended: {diagnostic}")
                                    })
                                }
                                Ok((reason, _, _)) => {
                                    Err(format!("FIL END has unknown reason {reason}"))
                                }
                                Err(error) => Err(format!("Malformed FIL END: {error}")),
                            }
                        }
                    }
                    kind => Err(format!("Unexpected FIL server message kind 0x{kind:02x}")),
                };
                if let Err(error) = result {
                    let _ = output_tx.send(Err(error));
                    break;
                }
            }
        });
        match startup_rx.recv_timeout(FIL_STARTUP_TIMEOUT) {
            Ok(Ok(())) => {}
            Ok(Err(error)) => {
                let _ = child.kill();
                let _ = child.wait();
                return Err(DriverError::ConnectionFailed(format!(
                    "FIL failed to start serve-network: {error}"
                )));
            }
            Err(RecvTimeoutError::Timeout) => {
                let _ = child.kill();
                let _ = child.wait();
                return Err(DriverError::ConnectionFailed(
                    "Timed out waiting for FIL serve-network HELLO".into(),
                ));
            }
            Err(RecvTimeoutError::Disconnected) => {
                let _ = child.kill();
                let _ = child.wait();
                return Err(DriverError::ConnectionFailed(
                    "FIL output reader stopped before HELLO".into(),
                ));
            }
        }
        Ok(Self {
            child,
            input: Arc::new(Mutex::new(stdin)),
            output,
            gpio_output,
            expectation_output,
            bus: bus.into(),
            trace_bus,
            request_id: AtomicU32::new(1),
            outstanding_requests,
            connected: true,
        })
    }
    fn reserve_request_id(&self) -> u32 {
        let mut outstanding = self
            .outstanding_requests
            .lock()
            .unwrap_or_else(std::sync::PoisonError::into_inner);
        loop {
            let id = self.request_id.fetch_add(1, Ordering::Relaxed);
            if id != 0 && outstanding.insert(id) {
                return id;
            }
        }
    }

    fn release_request_id(&self, id: u32) {
        self.outstanding_requests
            .lock()
            .unwrap_or_else(std::sync::PoisonError::into_inner)
            .remove(&id);
    }

    fn send_request(&mut self, kind: u8, payload: &[u8]) -> DriverResult<()> {
        let id = self.reserve_request_id();
        let result = write_frame(
            &mut *self
                .input
                .lock()
                .unwrap_or_else(std::sync::PoisonError::into_inner),
            kind,
            id,
            payload,
        );
        match result {
            Ok(()) => Ok(()),
            Err(error) => {
                self.release_request_id(id);
                self.connected = false;
                Err(DriverError::WriteError(format!(
                    "Failed to send request to FIL: {error}"
                )))
            }
        }
    }
    pub fn set_trace_bus(&mut self, trace_bus: Option<String>) {
        *self
            .trace_bus
            .write()
            .unwrap_or_else(std::sync::PoisonError::into_inner) = trace_bus;
    }
    pub fn take_gpio_events(&mut self) -> Vec<FilGpioEvent> {
        self.gpio_output.try_iter().take(256).collect()
    }
    pub fn take_expectation_events(&mut self) -> Vec<FilExpectationEvent> {
        self.expectation_output
            .try_iter()
            .take(FIL_MAX_EXPECTATION_EVENTS)
            .collect()
    }
    pub fn set_adc(
        &mut self,
        board: &str,
        instance: FilAdcInstance,
        channel: u8,
        value: u16,
    ) -> DriverResult<()> {
        if board.is_empty() || channel > 19 || value > 4095 {
            return Err(DriverError::WriteError(
                "ADC injection requires a board, ADC instance, channel 0..19, and value 0..4095"
                    .into(),
            ));
        }
        let payload = request_payload(
            ADC_SET,
            board,
            Some(instance.as_str()),
            u32::from(value),
            channel,
            None,
        )?;
        self.send_request(ADC_SET, &payload)
    }
    pub fn set_gpio(
        &mut self,
        board: &str,
        port: FilGpioPort,
        pin: u8,
        value: Option<bool>,
    ) -> DriverResult<()> {
        if board.is_empty() || pin > 15 {
            return Err(DriverError::WriteError(
                "GPIO control requires a board, GPIO port, and pin 0..15".into(),
            ));
        }
        let action: u8 = match value {
            Some(false) => 0,
            Some(true) => 1,
            None => 2,
        };
        let payload = request_payload(
            GPIO_SET,
            board,
            Some(port.as_str()),
            u32::from(action),
            pin,
            None,
        )?;
        self.send_request(GPIO_SET, &payload)
    }
}
fn format_fil_injection(bus: &str, frame: CanFrame) -> DriverResult<Vec<u8>> {
    let CanFrame::Can2(frame) = frame else {
        return Err(DriverError::WriteError(
            "FIL injection does not yet support CAN FD frames".into(),
        ));
    };
    let (id, extended) = match frame.id() {
        slcan::Id::Standard(id) => (u32::from(id.as_raw()), false),
        slcan::Id::Extended(id) => (id.as_raw(), true),
    };
    let data = frame.data().ok_or_else(|| {
        DriverError::WriteError("FIL injection does not support remote frames".into())
    })?;
    request_payload(CAN_INJECT, bus, None, id, u8::from(extended), Some(data))
}
fn receive_fil_frames(
    output: &Receiver<Result<FilCanEvent, String>>,
    timeout: Duration,
    trace_bus: Option<&str>,
) -> DriverResult<Vec<CanFrame>> {
    let deadline = Instant::now() + timeout;
    let mut frames = Vec::new();
    let mut consumed = 0;
    while frames.is_empty() && consumed < FIL_MAX_FRAMES_PER_POLL {
        if consumed > 0 && Instant::now() >= deadline {
            break;
        }
        let event = match output.recv_timeout(deadline.saturating_duration_since(Instant::now())) {
            Ok(Ok(v)) => v,
            Ok(Err(e)) => return Err(DriverError::ReadError(DriverReadError::IoError(e))),
            Err(RecvTimeoutError::Timeout) => break,
            Err(RecvTimeoutError::Disconnected) => {
                return Err(DriverError::ReadError(DriverReadError::IoError(
                    "FIL output reader stopped".into(),
                )));
            }
        };
        consumed += 1;
        if trace_source_matches(&event.source, trace_bus) {
            frames.push(event.frame);
        }
    }
    if frames.is_empty() {
        return Err(DriverError::ReadError(DriverReadError::Timeout));
    }
    while frames.len() < FIL_MAX_FRAMES_PER_POLL && consumed < FIL_MAX_FRAMES_PER_POLL {
        let Ok(result) = output.try_recv() else {
            break;
        };
        consumed += 1;
        match result {
            Ok(e) if trace_source_matches(&e.source, trace_bus) => frames.push(e.frame),
            Ok(_) => {}
            Err(_) => break,
        }
    }
    Ok(frames)
}
impl CanDriver for FilDriver {
    fn needs_read_retry_sleep(&self) -> bool {
        false
    }
    fn read_frames(&mut self) -> DriverResult<Vec<CanFrame>> {
        let bus = self
            .trace_bus
            .read()
            .unwrap_or_else(std::sync::PoisonError::into_inner);
        let result = receive_fil_frames(
            &self.output,
            Duration::from_millis(FIL_READ_TIMEOUT_MS),
            bus.as_deref(),
        );
        if matches!(
            result,
            Err(DriverError::ReadError(DriverReadError::IoError(_)))
        ) {
            self.connected = false;
        }
        result
    }
    fn write_frame(&mut self, frame: CanFrame) -> DriverResult<()> {
        let payload = format_fil_injection(&self.bus, frame)?;
        self.send_request(CAN_INJECT, &payload)
    }
    fn is_connected(&self) -> bool {
        self.connected
    }
    fn bus_speed(&self) -> Option<CanBusSpeed> {
        Some(CanBusSpeed::Kbps500)
    }
    fn close(&mut self) -> DriverResult<()> {
        let deadline = Instant::now() + FIL_SHUTDOWN_TIMEOUT;
        let was_connected = std::mem::replace(&mut self.connected, false);
        if was_connected {
            let payload = Vec::new();
            let id = self.reserve_request_id();
            let input = Arc::clone(&self.input);
            let (written_tx, written_rx) = mpsc::channel();
            thread::spawn(move || {
                let result = write_frame(
                    &mut *input
                        .lock()
                        .unwrap_or_else(std::sync::PoisonError::into_inner),
                    STOP,
                    id,
                    &payload,
                );
                let _ = written_tx.send(result);
            });
            if !matches!(
                written_rx.recv_timeout(deadline.saturating_duration_since(Instant::now())),
                Ok(Ok(()))
            ) {
                self.release_request_id(id);
            }
        }
        loop {
            match self.child.try_wait() {
                Ok(Some(_)) => return Ok(()),
                Ok(None) if Instant::now() < deadline => thread::sleep(Duration::from_millis(10)),
                _ => break,
            }
        }
        let _ = self.child.kill();
        let _ = self.child.wait();
        Ok(())
    }
}
impl Drop for FilDriver {
    fn drop(&mut self) {
        let _ = self.close();
    }
}

#[cfg(test)]
mod fil_tests {
    use super::*;
    use crate::messages::{FilAdcInstance, FilGpioPort};
    #[test]
    fn args_preserve_options_and_filters() {
        let mut o = crate::settings::FilRunOptions::default();
        o.extra_live_filters = "can_rx, irq".into();
        o.strict_mmio = true;
        o.wall_pacing = false;
        o.loop_batching = false;
        o.trace_instructions = true;
        o.detect_spin = true;
        let a = fil_network_args(&o).unwrap();
        assert!(a.iter().any(|x| x == "--transport"));
        assert!(!a.iter().any(|x| x == "--control-stdin"));
        for f in [
            "--strict-mmio",
            "--no-wall-pacing",
            "--no-loop-batching",
            "--trace-instr",
            "--detect-spin",
            "can_tx",
            "gpio_input",
            "expectation_pending",
            "can_rx",
            "irq",
        ] {
            assert!(a.iter().any(|x| x == f), "missing {f}");
        }
    }
    struct ShortReader<'a> {
        bytes: &'a [u8],
        offset: usize,
    }

    impl Read for ShortReader<'_> {
        fn read(&mut self, output: &mut [u8]) -> std::io::Result<usize> {
            if self.offset == self.bytes.len() {
                return Ok(0);
            }
            let count = output.len().min(3).min(self.bytes.len() - self.offset);
            output[..count].copy_from_slice(&self.bytes[self.offset..self.offset + count]);
            self.offset += count;
            Ok(count)
        }
    }

    #[test]
    fn codec_handles_fragmented_frames_and_rejects_invalid_or_truncated_input() {
        let mut bytes = Vec::new();
        write_frame(&mut bytes, CAN_INJECT, 7, &[1, 2]).unwrap();
        let mut reader = ShortReader {
            bytes: &bytes,
            offset: 0,
        };
        let frame = read_frame(&mut reader).unwrap().unwrap();
        assert_eq!(frame.kind, CAN_INJECT);
        assert_eq!(frame.request_id, 7);
        assert_eq!(frame.payload, [1, 2]);
        assert!(read_frame(&mut reader).unwrap().is_none());

        let mut invalid: &[u8] = b"bad";
        assert!(read_frame(&mut invalid).is_err());
        let mut truncated: &[u8] = &bytes[..bytes.len() - 1];
        assert!(read_frame(&mut truncated).is_err());
    }
    #[test]
    fn binary_request_encoding_is_typed() {
        let data = [0xde, 0xad];
        let p = request_payload(CAN_INJECT, "vehicle", None, 0x123, 0, Some(&data)).unwrap();
        assert_eq!(
            p,
            [
                7, 0, b'v', b'e', b'h', b'i', b'c', b'l', b'e', 0x23, 1, 0, 0, 0, 2, 0xde, 0xad
            ]
        );
        let a = request_payload(ADC_SET, "board", Some("ADC1"), 2048, 3, None).unwrap();
        assert_eq!(&a[a.len() - 3..], &[3, 0, 8]);
    }
    #[test]
    fn maps_binary_can_gpio_and_expectation_traces() {
        let (ct, cr) = mpsc::channel();
        let (gt, gr) = mpsc::channel();
        let (et, er) = mpsc::channel();
        let bus = Arc::new(RwLock::new(None));

        parse_trace(
            trace_frame(
                "vehicle/main.FDCAN1",
                "can_tx",
                &[("id", "0x123"), ("data", "0102")],
            ),
            &ct,
            &gt,
            &et,
            &bus,
        )
        .unwrap();
        let can = cr.try_recv().unwrap().unwrap();
        assert_eq!(can.source, "vehicle/main.FDCAN1");
        let slcan::CanFrame::Can2(can_frame) = can.frame else {
            panic!("expected classic CAN frame")
        };
        assert_eq!(can_frame.data(), Some(&[1, 2][..]));

        parse_trace(
            trace_frame(
                "dashboard.FDCAN1",
                "can_tx",
                &[("id", "73"), ("dlc", "4"), ("length", "4")],
            ),
            &ct,
            &gt,
            &et,
            &bus,
        )
        .unwrap();
        assert!(
            cr.try_recv().is_err(),
            "device-level CAN traces are not bus frames"
        );

        parse_trace(
            trace_frame(
                "dashboard.GPIOA",
                "gpio_output",
                &[("pin", "3"), ("value", "1")],
            ),
            &ct,
            &gt,
            &et,
            &bus,
        )
        .unwrap();
        let gpio = gr.try_recv().unwrap();
        assert_eq!(gpio.board, "dashboard");
        assert_eq!(gpio.port, FilGpioPort::GpioA);
        assert_eq!(gpio.pin, 3);
        assert_eq!(gpio.value, Some(true));
        assert!(gpio.output);

        parse_trace(
            trace_frame(
                "expectation/check",
                "expectation_pass",
                &[
                    ("check_id", "stimulus/0/script with spaces/expect/1"),
                    ("script", "script with spaces"),
                    ("expected_bus", "vehicle"),
                    ("expected_id", "0x321"),
                    ("expected_extended", "true"),
                    ("expected_data", "deaf"),
                    ("window_start_ns", "100"),
                    ("window_end_ns", "200"),
                    ("matched_bus", "vehicle"),
                    ("matched_id", "0x321"),
                    ("matched_data", "deaf"),
                    ("matched_origin", "dashboard/FDCAN1"),
                    ("matched_time_ns", "150"),
                ],
            ),
            &ct,
            &gt,
            &et,
            &bus,
        )
        .unwrap();
        let expectation = er.try_recv().unwrap();
        assert_eq!(
            expectation.check_id,
            "stimulus/0/script with spaces/expect/1"
        );
        assert_eq!(expectation.script, "script with spaces");
        assert_eq!(expectation.status, FilExpectationStatus::Pass);
        assert!(expectation.expected_extended);
        assert_eq!(expectation.expected_data, [0xde, 0xaf]);
        assert_eq!(expectation.window_start_ns, 100);
        assert_eq!(expectation.window_end_ns, 200);
        assert_eq!(expectation.matched_time_ns, Some(150));
        assert_eq!(
            expectation.matched_origin.as_deref(),
            Some("dashboard/FDCAN1")
        );
        assert_eq!(FilAdcInstance::Adc1.as_str(), "ADC1");
    }

    #[test]
    fn binary_trace_hex_parser_rejects_non_ascii_without_panicking() {
        assert!(parse_hex_data("a€").is_err());
        assert!(parse_hex_data("a").is_err());
    }

    #[test]
    fn hello_is_validated_and_failed_replies_are_nonfatal() {
        let hello = Frame {
            kind: HELLO,
            request_id: 0,
            payload: {
                let mut payload = Vec::new();
                put_test_string(&mut payload, "network");
                payload.extend_from_slice(&6u32.to_le_bytes());
                payload.extend_from_slice(&2u32.to_le_bytes());
                payload
            },
        };
        assert!(parse_hello(&hello).is_ok());

        let outstanding = Mutex::new(HashSet::from([9]));
        let reply = Frame {
            kind: REPLY,
            request_id: 9,
            payload: {
                let mut payload = Vec::new();
                payload.extend_from_slice(&2u16.to_le_bytes());
                payload.extend_from_slice(&123u64.to_le_bytes());
                put_test_string(&mut payload, "unknown bus");
                payload
            },
        };
        assert!(parse_reply(&reply, &outstanding).is_ok());
        assert!(outstanding.lock().unwrap().is_empty());
        assert!(parse_reply(&reply, &outstanding).is_err());
    }

    fn trace_frame(source: &str, kind: &str, fields: &[(&str, &str)]) -> Frame {
        let mut payload = Vec::new();
        payload.extend_from_slice(&1u64.to_le_bytes());
        payload.extend_from_slice(&2u64.to_le_bytes());
        put_test_string(&mut payload, source);
        put_test_string(&mut payload, kind);
        payload.extend_from_slice(&(fields.len() as u16).to_le_bytes());
        for (key, value) in fields {
            put_test_string(&mut payload, key);
            put_test_string(&mut payload, value);
        }
        Frame {
            kind: TRACE,
            request_id: 0,
            payload,
        }
    }

    fn put_test_string(p: &mut Vec<u8>, s: &str) {
        p.extend_from_slice(&(s.len() as u16).to_le_bytes());
        p.extend_from_slice(s.as_bytes());
    }
}
