use crate::{
    can::driver::{CanDriver, DriverError, DriverResult},
    connection::FilRunOptions,
    frame::{CanFrame, CanIdentity, FrameKind},
};
use std::time::Duration;

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
pub enum FilExpectationStatus {
    Pending,
    Pass,
    Fail,
    Incomplete,
}

#[derive(Clone, Debug, PartialEq, Eq)]
pub struct FilExpectationEvent {
    pub check_id: String,
    pub script: String,
    pub status: FilExpectationStatus,
    pub expected_bus: String,
    pub expected_id: u32,
    pub expected_extended: bool,
    pub expected_data: Vec<u8>,
    pub window_start_ns: u64,
    pub window_end_ns: u64,
    pub matched_bus: Option<String>,
    pub matched_id: Option<u32>,
    pub matched_data: Option<Vec<u8>>,
    pub matched_origin: Option<String>,
    pub matched_time_ns: Option<u64>,
    pub reason: Option<String>,
}

const FIL_MAGIC: &[u8; 4] = b"FILN";
const FIL_VERSION: u8 = 1;
const FIL_MAX_PAYLOAD: usize = 65_536;
const FIL_CAN_INJECT: u8 = 1;
const FIL_ADC_SET: u8 = 2;
const FIL_GPIO_SET: u8 = 3;
const FIL_HELLO: u8 = 0x80;
const FIL_REPLY: u8 = 0x81;
const FIL_TRACE: u8 = 0x82;
const FIL_END: u8 = 0x83;

struct FilWireFrame {
    kind: u8,
    request_id: u32,
    payload: Vec<u8>,
}
fn drain_all_fil_expectation_events(
    events: &std::sync::mpsc::Receiver<FilExpectationEvent>,
) -> Vec<FilExpectationEvent> {
    events.try_iter().collect()
}
fn fil_read_frame(reader: &mut impl std::io::Read) -> Result<Option<FilWireFrame>, String> {
    let mut h = [0u8; 16];
    let mut first = [0];
    loop {
        match reader.read(&mut first) {
            Ok(0) => return Ok(None),
            Ok(1) => {
                h[0] = first[0];
                break;
            }
            Ok(_) => unreachable!(),
            Err(e) if e.kind() == std::io::ErrorKind::Interrupted => continue,
            Err(e) => return Err(e.to_string()),
        }
    }
    reader
        .read_exact(&mut h[1..])
        .map_err(|e| format!("Truncated FIL header: {e}"))?;
    if &h[..4] != FIL_MAGIC || h[4] != FIL_VERSION || h[6..8] != [0, 0] {
        return Err("Invalid FIL binary frame header".into());
    }
    let len = u32::from_le_bytes(h[8..12].try_into().unwrap()) as usize;
    if len > FIL_MAX_PAYLOAD {
        return Err("FIL payload too large".into());
    }
    let mut payload = vec![0; len];
    reader
        .read_exact(&mut payload)
        .map_err(|e| format!("Truncated FIL payload: {e}"))?;
    Ok(Some(FilWireFrame {
        kind: h[5],
        request_id: u32::from_le_bytes(h[12..16].try_into().unwrap()),
        payload,
    }))
}
fn fil_write_frame(
    w: &mut impl std::io::Write,
    kind: u8,
    id: u32,
    payload: &[u8],
) -> std::io::Result<()> {
    if payload.len() > FIL_MAX_PAYLOAD {
        return Err(std::io::Error::new(
            std::io::ErrorKind::InvalidInput,
            "FIL payload too large",
        ));
    }
    let mut h = [0u8; 16];
    h[..4].copy_from_slice(FIL_MAGIC);
    h[4] = FIL_VERSION;
    h[5] = kind;
    h[8..12].copy_from_slice(&(payload.len() as u32).to_le_bytes());
    h[12..16].copy_from_slice(&id.to_le_bytes());
    w.write_all(&h)?;
    w.write_all(payload)?;
    w.flush()
}
fn validate_fil_frame_kind(kind: FrameKind) -> DriverResult<()> {
    if matches!(kind, FrameKind::Data) {
        Ok(())
    } else {
        Err(DriverError::Unsupported(
            "FIL supports CAN 2.0 data frames only".into(),
        ))
    }
}
fn fil_put_string(out: &mut Vec<u8>, s: &str) -> DriverResult<()> {
    let n = u16::try_from(s.len()).map_err(|_| DriverError::Write("FIL string too long".into()))?;
    out.extend_from_slice(&n.to_le_bytes());
    out.extend_from_slice(s.as_bytes());
    Ok(())
}

pub struct FilDriver {
    child: std::process::Child,
    input: std::io::BufWriter<std::process::ChildStdin>,
    request_id: u32,
    reader_join: Option<std::thread::JoinHandle<()>>,
    output: std::sync::mpsc::Receiver<Result<CanFrame, String>>,
    pending_read_error: Option<String>,
    gpio_output: std::sync::mpsc::Receiver<FilGpioEvent>,
    expectation_output: std::sync::mpsc::Receiver<FilExpectationEvent>,
    trace_bus: std::sync::Arc<std::sync::RwLock<Option<String>>>,
    bus: String,
}
impl FilDriver {
    fn send_request(&mut self, kind: u8, payload: &[u8]) -> DriverResult<()> {
        let id = self.request_id;
        self.request_id = self.request_id.wrapping_add(1).max(1);
        fil_write_frame(&mut self.input, kind, id, payload)
            .map_err(|e| DriverError::Write(e.to_string()))
    }
    pub fn new(
        executable: &std::path::Path,
        network: &std::path::Path,
        bus: &str,
        run_options: &FilRunOptions,
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
        let args = serve_network_args(network, run_options)?;
        let mut child = std::process::Command::new(executable)
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
        let reader_join = std::thread::spawn(move || {
            let mut reader = std::io::BufReader::new(stdout);
            let hello =
                fil_read_frame(&mut reader).and_then(|f| f.ok_or("FIL ended before HELLO".into()));
            let valid_hello = hello.as_ref().is_ok_and(|frame| {
                if frame.kind != FIL_HELLO || frame.request_id != 0 {
                    return false;
                }
                let mut at = 0;
                fil_string(&frame.payload, &mut at).is_ok()
                    && fil_u32(&frame.payload, &mut at).is_ok()
                    && fil_u32(&frame.payload, &mut at).is_ok()
                    && at == frame.payload.len()
            });
            if !valid_hello {
                let _ = output_tx.send(Err(hello
                    .err()
                    .unwrap_or_else(|| "Invalid FIL HELLO".into())));
                return;
            }
            loop {
                match fil_read_frame(&mut reader) {
                    Ok(Some(frame)) if frame.kind == FIL_TRACE => {
                        if let Err(error) = parse_fil_wire_trace(
                            frame,
                            &output_tx,
                            &gpio_tx,
                            &expectation_tx,
                            &reader_trace_bus,
                        ) {
                            log::warn!("Invalid FIL trace: {error}");
                        }
                    }
                    Ok(Some(frame)) if frame.kind == FIL_REPLY => {
                        let result = (|| {
                            if frame.request_id == 0 {
                                return Err("FIL REPLY has zero request ID".to_owned());
                            }
                            let mut at = 0;
                            let status = fil_u16(&frame.payload, &mut at)?;
                            let _time = fil_u64(&frame.payload, &mut at)?;
                            let diagnostic = fil_string(&frame.payload, &mut at)?;
                            if at != frame.payload.len() {
                                return Err("trailing FIL REPLY data".into());
                            }
                            if status > 3 {
                                return Err(format!("unknown FIL reply status {status}"));
                            }
                            if status != 0 {
                                log::warn!(
                                    "FIL request {} rejected (status {status}): {diagnostic}",
                                    frame.request_id
                                )
                            }
                            Ok(())
                        })();
                        if let Err(error) = result {
                            let _ = output_tx.send(Err(error));
                            break;
                        }
                    }
                    Ok(Some(frame)) if frame.kind == FIL_END => {
                        if frame.request_id != 0 {
                            let _ = output_tx.send(Err("FIL END has nonzero request ID".into()));
                        }
                        break;
                    }
                    Ok(Some(_)) => {
                        let _ = output_tx.send(Err("Unexpected FIL binary frame".into()));
                        break;
                    }
                    Ok(None) => {
                        let _ = output_tx.send(Err("FIL process exited".into()));
                        break;
                    }
                    Err(error) => {
                        let _ = output_tx.send(Err(error));
                        break;
                    }
                }
            }
        });
        Ok(Self {
            child,
            input: std::io::BufWriter::new(stdin),
            request_id: 1,
            reader_join: Some(reader_join),
            output,
            pending_read_error: None,
            gpio_output,
            expectation_output,
            trace_bus,
            bus: bus.into(),
        })
    }
}
fn serve_network_args(
    network: &std::path::Path,
    options: &FilRunOptions,
) -> DriverResult<Vec<std::ffi::OsString>> {
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
    let mut args: Vec<std::ffi::OsString> = vec![
        "serve-network".into(),
        network.as_os_str().to_owned(),
        "--transport".into(),
        "stdio".into(),
    ];
    for (enabled, flag, value) in [
        (
            options.duration_ms != 0,
            "--duration-ms",
            options.duration_ms.to_string(),
        ),
        (
            options.max_instructions != 50_000_000,
            "--max-instructions",
            options.max_instructions.to_string(),
        ),
        (
            options.quantum != 1024,
            "--quantum",
            options.quantum.to_string(),
        ),
        (
            options.refresh_ms != 1,
            "--refresh-ms",
            options.refresh_ms.to_string(),
        ),
        (
            options.adc_decimation != 1,
            "--adc-decimation",
            options.adc_decimation.to_string(),
        ),
    ] {
        if enabled {
            args.extend([flag.into(), value.into()]);
        }
    }
    if options.strict_mmio {
        args.push("--strict-mmio".into());
    }
    if !options.wall_pacing {
        args.push("--no-wall-pacing".into());
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

fn fil_u16(p: &[u8], at: &mut usize) -> Result<u16, String> {
    let b = p.get(*at..*at + 2).ok_or("truncated u16")?;
    *at += 2;
    Ok(u16::from_le_bytes(b.try_into().unwrap()))
}
fn fil_u32(p: &[u8], at: &mut usize) -> Result<u32, String> {
    let b = p.get(*at..*at + 4).ok_or("truncated u32")?;
    *at += 4;
    Ok(u32::from_le_bytes(b.try_into().unwrap()))
}
fn fil_u64(p: &[u8], at: &mut usize) -> Result<u64, String> {
    let b = p.get(*at..*at + 8).ok_or("truncated u64")?;
    *at += 8;
    Ok(u64::from_le_bytes(b.try_into().unwrap()))
}
fn fil_string<'a>(p: &'a [u8], at: &mut usize) -> Result<&'a str, String> {
    let n = fil_u16(p, at)? as usize;
    let b = p.get(*at..*at + n).ok_or("truncated string")?;
    *at += n;
    std::str::from_utf8(b).map_err(|e| e.to_string())
}
fn parse_fil_wire_trace(
    frame: FilWireFrame,
    can: &std::sync::mpsc::Sender<Result<CanFrame, String>>,
    gpio: &std::sync::mpsc::Sender<FilGpioEvent>,
    expects: &std::sync::mpsc::Sender<FilExpectationEvent>,
    trace_bus: &std::sync::RwLock<Option<String>>,
) -> Result<(), String> {
    if frame.kind != FIL_TRACE {
        return Ok(());
    }
    if frame.request_id != 0 {
        return Err("FIL TRACE request id must be zero".into());
    }
    let p = &frame.payload;
    let mut at = 0;
    let _time = fil_u64(p, &mut at)?;
    let _seq = fil_u64(p, &mut at)?;
    let source = fil_string(p, &mut at)?.to_owned();
    let kind = fil_string(p, &mut at)?.to_owned();
    let count = fil_u16(p, &mut at)? as usize;
    let mut f = std::collections::HashMap::new();
    for _ in 0..count {
        let k = fil_string(p, &mut at)?.to_owned();
        let v = fil_string(p, &mut at)?.to_owned();
        f.insert(k, v);
    }
    if at != p.len() {
        return Err("trailing FIL trace data".into());
    }
    let selected = trace_bus
        .read()
        .unwrap_or_else(std::sync::PoisonError::into_inner)
        .clone();
    if kind == "can_tx" {
        if !trace_source_matches(&source, selected.as_deref()) {
            return Ok(());
        }
        let id = u32::from_str_radix(
            f.get("id")
                .ok_or("CAN trace missing id")?
                .trim_start_matches("0x"),
            16,
        )
        .map_err(|e| e.to_string())?;
        let extended = f.get("extended").is_some_and(|x| x == "true");
        if f.get("fd").is_some_and(|x| x == "true") {
            return Ok(());
        }
        let hex = f.get("data").ok_or("CAN trace missing data")?;
        if hex.len() % 2 != 0 {
            return Err("odd hex length in CAN trace".into());
        }
        let data = hex
            .as_bytes()
            .as_chunks::<2>()
            .0
            .iter()
            .map(|b| {
                u8::from_str_radix(std::str::from_utf8(b).unwrap_or(""), 16)
                    .map_err(|e| e.to_string())
            })
            .collect::<Result<Vec<_>, _>>()?;
        let ident = CanIdentity::new(id, extended).map_err(|e| e.to_string())?;
        can.send(CanFrame::data(ident, data).map_err(|e| e.to_string()))
            .map_err(|e| e.to_string())?;
    } else if kind == "gpio_input" || kind == "gpio_output" {
        let (board, port) = source.rsplit_once('.').ok_or("malformed GPIO source")?;
        let pin = f
            .get("pin")
            .ok_or("missing pin")?
            .parse()
            .map_err(|_| "invalid pin")?;
        let value = match f.get("value").map(String::as_str) {
            Some("0") => Some(false),
            Some("1") => Some(true),
            Some("release") => None,
            _ => return Err("invalid gpio value".into()),
        };
        gpio.send(FilGpioEvent {
            board: board.into(),
            port: port.into(),
            pin,
            value,
            direction: if kind == "gpio_output" {
                FilGpioDirection::Output
            } else {
                FilGpioDirection::Input
            },
        })
        .map_err(|e| e.to_string())?;
    } else if let Some(status) = match kind.as_str() {
        "expectation_pending" => Some(FilExpectationStatus::Pending),
        "expectation_pass" => Some(FilExpectationStatus::Pass),
        "expectation_fail" => Some(FilExpectationStatus::Fail),
        "expectation_incomplete" => Some(FilExpectationStatus::Incomplete),
        _ => None,
    } {
        let get = |k: &str| {
            f.get(k)
                .map(String::as_str)
                .ok_or_else(|| format!("missing {k}"))
        };
        let hex = |k: &str| {
            u32::from_str_radix(get(k)?.trim_start_matches("0x"), 16).map_err(|e| e.to_string())
        };
        let bytes = |k: &str| {
            let s = get(k)?;
            s.as_bytes()
                .as_chunks::<2>()
                .0
                .iter()
                .map(|b| {
                    u8::from_str_radix(std::str::from_utf8(b).unwrap_or(""), 16)
                        .map_err(|e| e.to_string())
                })
                .collect::<Result<Vec<_>, _>>()
        };
        expects
            .send(FilExpectationEvent {
                check_id: get("check_id")?.into(),
                script: get("script")?.into(),
                status,
                expected_bus: get("expected_bus")?.into(),
                expected_id: hex("expected_id")?,
                expected_extended: get("expected_extended")? == "true",
                expected_data: bytes("expected_data")?,
                window_start_ns: get("window_start_ns")?
                    .parse()
                    .map_err(|e: std::num::ParseIntError| e.to_string())?,
                window_end_ns: get("window_end_ns")?
                    .parse()
                    .map_err(|e: std::num::ParseIntError| e.to_string())?,
                matched_bus: f.get("matched_bus").cloned(),
                matched_id: f
                    .get("matched_id")
                    .and_then(|v| u32::from_str_radix(v.trim_start_matches("0x"), 16).ok()),
                matched_data: f.get("matched_data").and_then(|v| {
                    v.as_bytes()
                        .as_chunks::<2>()
                        .0
                        .iter()
                        .map(|b| u8::from_str_radix(std::str::from_utf8(b).ok()?, 16).ok())
                        .collect()
                }),
                matched_origin: f.get("matched_origin").cloned(),
                matched_time_ns: f.get("matched_time_ns").and_then(|v| v.parse().ok()),
                reason: f.get("reason").cloned(),
            })
            .map_err(|e| e.to_string())?;
    }
    Ok(())
}

fn trace_source_matches(source: &str, trace_bus: Option<&str>) -> bool {
    trace_bus.is_none_or(|bus| {
        source
            .strip_prefix(bus)
            .is_some_and(|suffix| suffix.starts_with('/'))
    })
}

impl FilDriver {
    pub fn needs_read_retry_sleep(&self) -> bool {
        false
    }

    pub fn read_frames(&mut self) -> DriverResult<Vec<CanFrame>> {
        use std::sync::mpsc::RecvTimeoutError;
        if let Some(error) = self.pending_read_error.take() {
            return Err(DriverError::Read(error));
        }
        let first = match self.output.recv_timeout(Duration::from_millis(1)) {
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
            match next {
                Ok(frame) => frames.push(frame),
                Err(error) => {
                    // Deliver this batch before reporting the original reader failure.
                    self.pending_read_error = Some(error);
                    break;
                }
            }
        }
        Ok(frames)
    }
    pub fn write_frame(&mut self, frame: CanFrame) -> DriverResult<()> {
        validate_fil_frame_kind(frame.kind)?;
        let mut payload = Vec::new();
        fil_put_string(&mut payload, &self.bus)?;
        payload.extend_from_slice(&frame.identity.raw_id().to_le_bytes());
        payload.push(u8::from(frame.identity.is_extended()));
        payload.push(frame.data.len() as u8);
        payload.extend_from_slice(&frame.data);
        self.send_request(FIL_CAN_INJECT, &payload)
    }
    pub fn close(&mut self) -> DriverResult<()> {
        let _ = self.child.kill();
        let _ = self.child.wait();
        if let Some(reader) = self.reader_join.take() {
            let _ = reader.join();
        }
        Ok(())
    }
    pub fn set_trace_bus(&mut self, trace_bus: Option<String>) -> DriverResult<()> {
        *self
            .trace_bus
            .write()
            .unwrap_or_else(std::sync::PoisonError::into_inner) = trace_bus;
        Ok(())
    }
    pub fn take_gpio_events(&mut self) -> Vec<FilGpioEvent> {
        self.gpio_output.try_iter().take(256).collect()
    }
    pub fn take_expectation_events(&mut self) -> Vec<FilExpectationEvent> {
        self.expectation_output.try_iter().take(256).collect()
    }
    pub fn take_all_expectation_events(&mut self) -> Vec<FilExpectationEvent> {
        drain_all_fil_expectation_events(&self.expectation_output)
    }
    pub fn set_gpio(
        &mut self,
        board: &str,
        port: &str,
        pin: u8,
        value: Option<bool>,
    ) -> DriverResult<()> {
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
        let mut payload = Vec::new();
        fil_put_string(&mut payload, board)?;
        fil_put_string(&mut payload, port)?;
        payload.push(pin);
        payload.push(match value {
            Some(false) => 0,
            Some(true) => 1,
            None => 2,
        });
        self.send_request(FIL_GPIO_SET, &payload)
    }
    pub fn set_adc(
        &mut self,
        board: &str,
        instance: &str,
        channel: u8,
        value: u16,
    ) -> DriverResult<()> {
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
        let mut payload = Vec::new();
        fil_put_string(&mut payload, board)?;
        fil_put_string(&mut payload, instance)?;
        payload.push(channel);
        payload.extend_from_slice(&value.to_le_bytes());
        self.send_request(FIL_ADC_SET, &payload)
    }
}
impl CanDriver for FilDriver {
    fn needs_read_retry_sleep(&self) -> bool {
        FilDriver::needs_read_retry_sleep(self)
    }
    fn read_frames(&mut self) -> DriverResult<Vec<CanFrame>> {
        FilDriver::read_frames(self)
    }
    fn write_frame(&mut self, frame: CanFrame) -> DriverResult<()> {
        FilDriver::write_frame(self, frame)
    }
    fn close(&mut self) -> DriverResult<()> {
        FilDriver::close(self)
    }
}
impl Drop for FilDriver {
    fn drop(&mut self) {
        let _ = self.child.kill();
        let _ = self.child.wait();
    }
}

#[cfg(test)]
mod fil_transport_tests {
    use super::{
        FIL_CAN_INJECT, FIL_TRACE, FilWireFrame, drain_all_fil_expectation_events, fil_put_string,
        fil_read_frame, fil_write_frame, parse_fil_wire_trace, serve_network_args,
        validate_fil_frame_kind,
    };
    use crate::{
        can::driver::{DriverError, FilExpectationEvent, FilExpectationStatus},
        connection::FilRunOptions,
        frame::FrameKind,
    };
    #[test]
    fn default_args_match_fil_stdio_defaults_and_enable_expectation_traces() {
        let network = std::path::Path::new("network.json");
        let args = serve_network_args(network, &FilRunOptions::default()).unwrap();
        assert_eq!(args[0], "serve-network");
        assert_eq!(args[1], network.as_os_str());
        assert_eq!(args[2], "--transport");
        assert_eq!(args[3], "stdio");
        assert!(!args.iter().any(|arg| arg == "--max-instructions"));
        assert!(!args.iter().any(|arg| arg == "--adc-decimation"));
        assert!(!args.iter().any(|arg| arg == "--no-loop-batching"));
        for filter in [
            "expectation_pending",
            "expectation_pass",
            "expectation_fail",
            "expectation_incomplete",
        ] {
            assert!(args.iter().any(|arg| arg == filter));
        }
    }
    #[test]
    fn unsupported_frame_kinds_are_not_reported_as_write_errors() {
        assert!(matches!(
            validate_fil_frame_kind(FrameKind::Remote),
            Err(DriverError::Unsupported(_))
        ));
        assert!(validate_fil_frame_kind(FrameKind::Data).is_ok());
    }
    #[test]
    fn binary_frame_round_trip_and_rejects_bad_magic() {
        let mut bytes = Vec::new();
        fil_write_frame(&mut bytes, FIL_CAN_INJECT, 42, b"payload").unwrap();
        let frame = fil_read_frame(&mut std::io::Cursor::new(bytes.clone()))
            .unwrap()
            .unwrap();
        assert_eq!(frame.kind, FIL_CAN_INJECT);
        assert_eq!(frame.request_id, 42);
        assert_eq!(frame.payload, b"payload");
        bytes[0] = b'X';
        assert!(fil_read_frame(&mut std::io::Cursor::new(bytes)).is_err());
    }
    #[test]
    fn binary_expectation_trace_parses_lifecycle_payload_without_bus_filtering() {
        let mut payload = Vec::new();
        payload.extend_from_slice(&10u64.to_le_bytes());
        payload.extend_from_slice(&2u64.to_le_bytes());
        fil_put_string(&mut payload, "unrelated/script").unwrap();
        fil_put_string(&mut payload, "expectation_pass").unwrap();
        let fields = [
            ("check_id", "stimulus/0/check"),
            ("script", "ready script"),
            ("expected_bus", "vehicle"),
            ("expected_id", "0x321"),
            ("expected_extended", "false"),
            ("expected_data", "0102"),
            ("window_start_ns", "10"),
            ("window_end_ns", "20"),
            ("matched_bus", "vehicle"),
            ("matched_id", "0x321"),
            ("matched_data", "0102"),
            ("matched_origin", "board/FDCAN1"),
            ("matched_time_ns", "15"),
        ];
        payload.extend_from_slice(&(fields.len() as u16).to_le_bytes());
        for (key, value) in fields {
            fil_put_string(&mut payload, key).unwrap();
            fil_put_string(&mut payload, value).unwrap();
        }
        let (can_tx, _) = std::sync::mpsc::channel();
        let (gpio_tx, _) = std::sync::mpsc::channel();
        let (exp_tx, exp_rx) = std::sync::mpsc::channel();
        parse_fil_wire_trace(
            FilWireFrame {
                kind: FIL_TRACE,
                request_id: 0,
                payload,
            },
            &can_tx,
            &gpio_tx,
            &exp_tx,
            &std::sync::RwLock::new(Some("selected-bus".into())),
        )
        .unwrap();
        let event = exp_rx.recv().unwrap();
        assert_eq!(event.status, FilExpectationStatus::Pass);
        assert_eq!(event.script, "ready script");
        assert_eq!(event.matched_time_ns, Some(15));
    }
    #[test]
    fn shutdown_drain_is_not_limited_to_regular_poll_batch() {
        let (tx, rx) = std::sync::mpsc::channel();
        for i in 0..500 {
            tx.send(FilExpectationEvent {
                check_id: i.to_string(),
                script: String::new(),
                status: FilExpectationStatus::Pass,
                expected_bus: String::new(),
                expected_id: 0,
                expected_extended: false,
                expected_data: vec![],
                window_start_ns: 0,
                window_end_ns: 0,
                matched_bus: None,
                matched_id: None,
                matched_data: None,
                matched_origin: None,
                matched_time_ns: None,
                reason: None,
            })
            .unwrap();
        }
        assert_eq!(drain_all_fil_expectation_events(&rx).len(), 500);
        assert!(rx.try_recv().is_err());
    }
}
