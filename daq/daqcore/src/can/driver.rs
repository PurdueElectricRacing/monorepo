//! Drivers are single-owner I/O adapters; no transport type escapes this module.
use crate::{
    can,
    connection::{CanBusSpeed, ConnectionSource},
    frame::{CanFrame, CanIdentity},
    log_parse,
};

use std::time::{Duration, Instant};

#[derive(Debug)]
pub enum DriverError {
    ConnectionFailed(String),
    Timeout,
    Read(String),
    Write(String),
    Unsupported(String),
}

impl std::fmt::Display for DriverError {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "{self:?}")
    }
}

impl std::error::Error for DriverError {}
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

    fn read_frames(&mut self) -> DriverResult<Vec<CanFrame>>;
    fn write_frame(&mut self, frame: CanFrame) -> DriverResult<()>;
    fn bus_speed(&self) -> Option<CanBusSpeed> {
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

pub fn create_driver(source: &ConnectionSource) -> DriverResult<Box<dyn Driver>> {
    match source {
        ConnectionSource::Serial(path, speed) => {
            Ok(Box::new(serial::SerialDriver::new(path, *speed)?))
        }
        ConnectionSource::Udp(port) => {
            let socket = std::net::UdpSocket::bind(("0.0.0.0", *port))
                .map_err(|e| DriverError::ConnectionFailed(e.to_string()))?;
            socket
                .set_read_timeout(Some(Duration::from_millis(10)))
                .map_err(|e| DriverError::ConnectionFailed(e.to_string()))?;
            Ok(Box::new(UdpDriver(socket)))
        }
        ConnectionSource::Loopback => Ok(Box::new(LoopbackDriver::default())),
        ConnectionSource::Fil {
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
        ConnectionSource::Simulated(true, path) => {
            let parser = path
                .as_ref()
                .map(|path| can_decode::Parser::from_dbc_file(path))
                .transpose()
                .map_err(|error| DriverError::ConnectionFailed(error.to_string()))?;

            Ok(Box::new(SimulatedDriver {
                parser,
                next: Instant::now(),
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
    queued: Vec<CanFrame>,
}

const FIL_MAGIC: &[u8;4] = b"FILN";
const FIL_VERSION: u8 = 1;
const FIL_MAX_PAYLOAD: usize = 65_536;
const FIL_CAN_INJECT: u8 = 1;
const FIL_ADC_SET: u8 = 2;
const FIL_GPIO_SET: u8 = 3;
const FIL_HELLO: u8 = 0x80;
const FIL_REPLY: u8 = 0x81;
const FIL_TRACE: u8 = 0x82;
const FIL_END: u8 = 0x83;

struct FilWireFrame { kind:u8, request_id:u32, payload:Vec<u8> }
fn fil_read_frame(reader:&mut impl std::io::Read)->Result<Option<FilWireFrame>,String>{
    use std::io::Read; let mut h=[0u8;16]; let mut first=[0];
    loop { match reader.read(&mut first){Ok(0)=>return Ok(None),Ok(1)=>{h[0]=first[0];break},Ok(_)=>unreachable!(),Err(e) if e.kind()==std::io::ErrorKind::Interrupted=>continue,Err(e)=>return Err(e.to_string())} }
    reader.read_exact(&mut h[1..]).map_err(|e|format!("Truncated FIL header: {e}"))?;
    if &h[..4]!=FIL_MAGIC || h[4]!=FIL_VERSION || h[6..8]!=[0,0] {return Err("Invalid FIL binary frame header".into())}
    let len=u32::from_le_bytes(h[8..12].try_into().unwrap()) as usize; if len>FIL_MAX_PAYLOAD{return Err("FIL payload too large".into())}
    let mut payload=vec![0;len];reader.read_exact(&mut payload).map_err(|e|format!("Truncated FIL payload: {e}"))?;
    Ok(Some(FilWireFrame{kind:h[5],request_id:u32::from_le_bytes(h[12..16].try_into().unwrap()),payload}))
}
fn fil_write_frame(w:&mut impl std::io::Write,kind:u8,id:u32,payload:&[u8])->std::io::Result<()>{
    use std::io::Write; if payload.len()>FIL_MAX_PAYLOAD{return Err(std::io::Error::new(std::io::ErrorKind::InvalidInput,"FIL payload too large"))}
    let mut h=[0u8;16];h[..4].copy_from_slice(FIL_MAGIC);h[4]=FIL_VERSION;h[5]=kind;h[8..12].copy_from_slice(&(payload.len() as u32).to_le_bytes());h[12..16].copy_from_slice(&id.to_le_bytes());w.write_all(&h)?;w.write_all(payload)?;w.flush()
}
fn fil_put_string(out:&mut Vec<u8>,s:&str)->DriverResult<()>{let n=u16::try_from(s.len()).map_err(|_|DriverError::Write("FIL string too long".into()))?;out.extend_from_slice(&n.to_le_bytes());out.extend_from_slice(s.as_bytes());Ok(())}

struct FilDriver {
    child: std::process::Child,
    input: std::io::BufWriter<std::process::ChildStdin>,
    request_id:u32,
    output: std::sync::mpsc::Receiver<Result<CanFrame, String>>,
    gpio_output: std::sync::mpsc::Receiver<FilGpioEvent>,
    expectation_output: std::sync::mpsc::Receiver<FilExpectationEvent>,
    trace_bus: std::sync::Arc<std::sync::RwLock<Option<String>>>,
    bus: String,
}
impl FilDriver {
    fn send_request(&mut self, kind:u8, payload:&[u8])->DriverResult<()> {
        let id=self.request_id; self.request_id=self.request_id.wrapping_add(1).max(1);
        fil_write_frame(&mut self.input,kind,id,payload).map_err(|e|DriverError::Write(e.to_string()))
    }
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
            let mut reader = std::io::BufReader::new(stdout);
            let hello = fil_read_frame(&mut reader).and_then(|f| f.ok_or("FIL ended before HELLO".into()));
            if !matches!(hello, Ok(ref f) if f.kind == FIL_HELLO && f.request_id == 0) {
                let _ = output_tx.send(Err(hello.err().unwrap_or_else(|| "Invalid FIL HELLO".into())));
                return;
            }
            loop {
                match fil_read_frame(&mut reader) {
                    Ok(Some(frame)) if frame.kind == FIL_TRACE => {
                        if let Err(error) = parse_fil_wire_trace(frame, &output_tx, &gpio_tx, &expectation_tx, &reader_trace_bus) {
                            log::warn!("Invalid FIL trace: {error}");
                        }
                    }
                    Ok(Some(frame)) if frame.kind == FIL_REPLY => {}
                    Ok(Some(frame)) if frame.kind == FIL_END => break,
                    Ok(Some(_)) => { let _ = output_tx.send(Err("Unexpected FIL binary frame".into())); break; }
                    Ok(None) => { let _ = output_tx.send(Err("FIL process exited".into())); break; }
                    Err(error) => { let _ = output_tx.send(Err(error)); break; }
                }
            }
        });
        Ok(Self {
            child,
            input: std::io::BufWriter::new(stdin),
            request_id: 1,
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
        "--transport".into(), "stdio".into(),
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
    Ok(args)
}

fn fil_u16(p:&[u8],at:&mut usize)->Result<u16,String>{let b=p.get(*at..*at+2).ok_or("truncated u16")?;*at+=2;Ok(u16::from_le_bytes(b.try_into().unwrap()))}
fn fil_u64(p:&[u8],at:&mut usize)->Result<u64,String>{let b=p.get(*at..*at+8).ok_or("truncated u64")?;*at+=8;Ok(u64::from_le_bytes(b.try_into().unwrap()))}
fn fil_string<'a>(p:&'a[u8],at:&mut usize)->Result<&'a str,String>{let n=fil_u16(p,at)? as usize;let b=p.get(*at..*at+n).ok_or("truncated string")?;*at+=n;std::str::from_utf8(b).map_err(|e|e.to_string())}
fn parse_fil_wire_trace(frame:FilWireFrame,can:&std::sync::mpsc::Sender<Result<CanFrame,String>>,gpio:&std::sync::mpsc::Sender<FilGpioEvent>,expects:&std::sync::mpsc::Sender<FilExpectationEvent>,trace_bus:&std::sync::RwLock<Option<String>>)->Result<(),String>{
    if frame.kind!=FIL_TRACE{return Ok(())} if frame.request_id!=0{return Err("FIL TRACE request id must be zero".into())}
    let p=&frame.payload;let mut at=0;let _time=fil_u64(p,&mut at)?;let _seq=fil_u64(p,&mut at)?;let source=fil_string(p,&mut at)?.to_owned();let kind=fil_string(p,&mut at)?.to_owned();let count=fil_u16(p,&mut at)? as usize;let mut f=std::collections::HashMap::new();for _ in 0..count{let k=fil_string(p,&mut at)?.to_owned();let v=fil_string(p,&mut at)?.to_owned();f.insert(k,v);}if at!=p.len(){return Err("trailing FIL trace data".into())}
    let selected=trace_bus.read().unwrap_or_else(std::sync::PoisonError::into_inner).clone();
    if kind=="can_tx" {if !trace_source_matches(&source,selected.as_deref()){return Ok(())} let id=u32::from_str_radix(f.get("id").ok_or("CAN trace missing id")?.trim_start_matches("0x"),16).map_err(|e|e.to_string())?;let extended=f.get("extended").is_some_and(|x|x=="true");if f.get("fd").is_some_and(|x|x=="true"){return Ok(())}let data=f.get("data").ok_or("CAN trace missing data")?.as_bytes().chunks_exact(2).map(|b|u8::from_str_radix(std::str::from_utf8(b).unwrap_or(""),16).map_err(|e|e.to_string())).collect::<Result<Vec<_>,_>>()?;let ident=CanIdentity::new(id,extended).map_err(|e|e.to_string())?;can.send(CanFrame::data(ident,data).map_err(|e|e.to_string())).map_err(|e|e.to_string())?;}
    else if kind=="gpio_input"||kind=="gpio_output" {let (board,port)=source.rsplit_once('.').ok_or("malformed GPIO source")?;let pin=f.get("pin").ok_or("missing pin")?.parse().map_err(|_|"invalid pin")?;let value=match f.get("value").map(String::as_str){Some("0")=>Some(false),Some("1")=>Some(true),Some("release")=>None,_=>return Err("invalid gpio value".into())};gpio.send(FilGpioEvent{board:board.into(),port:port.into(),pin,value,direction:if kind=="gpio_output"{FilGpioDirection::Output}else{FilGpioDirection::Input}}).map_err(|e|e.to_string())?;}
    else if let Some(status)=match kind.as_str(){"expectation_pending"=>Some(FilExpectationStatus::Pending),"expectation_pass"=>Some(FilExpectationStatus::Pass),"expectation_fail"=>Some(FilExpectationStatus::Fail),"expectation_incomplete"=>Some(FilExpectationStatus::Incomplete),_=>None}{let get=|k:&str|f.get(k).map(String::as_str).ok_or_else(||format!("missing {k}"));let hex=|k:&str|u32::from_str_radix(get(k)?.trim_start_matches("0x"),16).map_err(|e|e.to_string());let bytes=|k:&str|{let s=get(k)?;s.as_bytes().chunks_exact(2).map(|b|u8::from_str_radix(std::str::from_utf8(b).unwrap_or(""),16).map_err(|e|e.to_string())).collect::<Result<Vec<_>,_>>()};expects.send(FilExpectationEvent{check_id:get("check_id")?.into(),script:get("script")?.into(),status,expected_bus:get("expected_bus")?.into(),expected_id:hex("expected_id")?,expected_extended:get("expected_extended")?=="true",expected_data:bytes("expected_data")?,window_start_ns:get("window_start_ns")?.parse().map_err(|e:std::num::ParseIntError|e.to_string())?,window_end_ns:get("window_end_ns")?.parse().map_err(|e:std::num::ParseIntError|e.to_string())?,matched_bus:f.get("matched_bus").cloned(),matched_id:f.get("matched_id").and_then(|v|u32::from_str_radix(v.trim_start_matches("0x"),16).ok()),matched_data:f.get("matched_data").and_then(|v|v.as_bytes().chunks_exact(2).map(|b|u8::from_str_radix(std::str::from_utf8(b).ok()?,16).ok()).collect()),matched_origin:f.get("matched_origin").cloned(),matched_time_ns:f.get("matched_time_ns").and_then(|v|v.parse().ok()),reason:f.get("reason").cloned()}).map_err(|e|e.to_string())?;}
    Ok(())
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
        if !matches!(frame.kind, crate::frame::FrameKind::Data) { return Err(DriverError::Write("FIL supports CAN 2.0 data frames only".into())); }
        let mut payload=Vec::new(); fil_put_string(&mut payload,&self.bus)?; payload.extend_from_slice(&frame.identity.raw_id().to_le_bytes()); payload.push(u8::from(frame.identity.is_extended())); payload.push(frame.data.len() as u8); payload.extend_from_slice(&frame.data); self.send_request(FIL_CAN_INJECT,&payload)
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
        let mut payload=Vec::new(); fil_put_string(&mut payload,board)?; fil_put_string(&mut payload,port)?; payload.push(pin); payload.push(match value{Some(false)=>0,Some(true)=>1,None=>2}); self.send_request(FIL_GPIO_SET,&payload)
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
        let mut payload=Vec::new(); fil_put_string(&mut payload,board)?; fil_put_string(&mut payload,instance)?; payload.push(channel); payload.extend_from_slice(&value.to_le_bytes()); self.send_request(FIL_ADC_SET,&payload)
    }
}
impl Drop for FilDriver {
    fn drop(&mut self) {
        let _ = self.child.kill();
        let _ = self.child.wait();
    }
}

impl Driver for LoopbackDriver {
    fn read_frames(&mut self) -> DriverResult<Vec<CanFrame>> {
        if self.queued.is_empty() {
            Err(DriverError::Timeout)
        } else {
            Ok(std::mem::take(&mut self.queued))
        }
    }

    fn write_frame(&mut self, frame: CanFrame) -> DriverResult<()> {
        self.queued.push(frame);
        Ok(())
    }
}

struct UdpDriver(std::net::UdpSocket);

impl Driver for UdpDriver {
    fn read_frames(&mut self) -> DriverResult<Vec<CanFrame>> {
        let mut buf = [0; 65536];
        let len = self.0.recv(&mut buf).map_err(io_read)?;
        parse_udp_buffer(&buf[..len])
    }

    fn write_frame(&mut self, _: CanFrame) -> DriverResult<()> {
        Err(DriverError::Unsupported(
            "UDP transmission is unsupported".into(),
        ))
    }
}

fn io_read(e: std::io::Error) -> DriverError {
    if matches!(
        e.kind(),
        std::io::ErrorKind::WouldBlock | std::io::ErrorKind::TimedOut
    ) {
        DriverError::Timeout
    } else {
        DriverError::Read(e.to_string())
    }
}

fn parse_udp_buffer(buf: &[u8]) -> DriverResult<Vec<CanFrame>> {
    if buf.len() < 16 || !buf.len().is_multiple_of(16) {
        return Err(DriverError::Read(
            "UDP packet must contain complete 16-byte records".into(),
        ));
    }
    buf.as_chunks::<16>()
        .0
        .iter()
        .map(|bytes| {
            let identity_bytes = [bytes[4], bytes[5], bytes[6], bytes[7]];
            let identity = u32::from_le_bytes(identity_bytes);
            let extended = identity & log_parse::consts::IS_EID_MASK != 0;
            let transport_flags = log_parse::consts::IS_EID_MASK | log_parse::consts::BUS_ID_MASK;
            let id = identity & !transport_flags;
            let identity = CanIdentity::new(id, extended)
                .map_err(|error| DriverError::Read(error.to_string()))?;
            CanFrame::data(identity, bytes[8..16].to_vec()).map_err(DriverError::Read)
        })
        .collect()
}

struct SimulatedDriver {
    parser: Option<can_decode::Parser>,
    next: Instant,
}

impl Driver for SimulatedDriver {
    fn read_frames(&mut self) -> DriverResult<Vec<CanFrame>> {
        use rand::prelude::*;
        let now = Instant::now();
        if now < self.next {
            return Err(DriverError::Timeout);
        }
        self.next += Duration::from_millis(1);
        let mut rng = rand::rng();
        let msg = self
            .parser
            .as_ref()
            .and_then(|p| p.msg_defs().choose(&mut rng).cloned());
        let (identity, size) = match msg {
            Some(message) => {
                let identity = can::can_dbc_identity(&message.id)
                    .map_err(|error| DriverError::Read(error.to_string()))?;

                (identity, message.size as usize)
            }
            None => {
                let id = rng.random_range(0..=can::STANDARD_ID_MASK);
                let identity = CanIdentity::new(id, false)
                    .map_err(|error| DriverError::Read(error.to_string()))?;

                (identity, 8)
            }
        };

        if size > 8 {
            return Err(DriverError::Timeout);
        }

        let mut data = vec![0; size];
        rng.fill_bytes(&mut data);
        let frame = CanFrame::data(identity, data).map_err(DriverError::Read)?;

        Ok(vec![frame])
    }

    fn write_frame(&mut self, _: CanFrame) -> DriverResult<()> {
        Ok(())
    }
}
mod serial {
    use crate::{
        can::driver::{Driver, DriverError, DriverResult, io_read},
        connection::CanBusSpeed,
        frame::{CanFrame, CanIdentity, FrameKind},
    };
    use std::time::Duration;

    use slcan::sync::CanSocket;
    pub struct SerialDriver {
        socket: CanSocket<Box<dyn serialport::SerialPort>>,
        speed: CanBusSpeed,
    }

    impl SerialDriver {
        pub fn new(path: &str, speed: CanBusSpeed) -> DriverResult<Self> {
            let port = serialport::new(path, 115200)
                .timeout(Duration::from_millis(10))
                .open()
                .map_err(|e| DriverError::ConnectionFailed(e.to_string()))?;
            let _ = port.clear(serialport::ClearBuffer::All);
            let mut socket = CanSocket::new(port);
            socket
                .set_operating_mode(slcan::OperatingMode::Normal)
                .map_err(|e| DriverError::ConnectionFailed(e.to_string()))?;
            let bitrate = match speed {
                CanBusSpeed::Kbps250 => slcan::NominalBitRate::Rate250Kbit,
                CanBusSpeed::Kbps500 => slcan::NominalBitRate::Rate500Kbit,
            };
            socket
                .open(bitrate)
                .map_err(|e| DriverError::ConnectionFailed(e.to_string()))?;
            Ok(Self { socket, speed })
        }
    }

    fn identity(id: slcan::Id) -> DriverResult<CanIdentity> {
        let identity = match id {
            slcan::Id::Standard(id) => CanIdentity::new(id.as_raw() as u32, false),
            slcan::Id::Extended(id) => CanIdentity::new(id.as_raw(), true),
        };

        identity.map_err(|error| DriverError::Read(error.to_string()))
    }

    fn from_wire(frame: slcan::CanFrame) -> DriverResult<CanFrame> {
        let frame = match frame {
            slcan::CanFrame::Can2(f) => {
                let identity = identity(f.id())?;
                let kind = if f.is_remote() {
                    FrameKind::Remote
                } else {
                    FrameKind::Data
                };

                CanFrame {
                    identity,
                    kind,
                    dlc: f.dlc() as u8,
                    data: f.data().unwrap_or(&[]).to_vec(),
                }
            }
            slcan::CanFrame::CanFd(f) => {
                let identity = identity(f.id())?;
                CanFrame {
                    identity,
                    kind: FrameKind::Fd {
                        bit_rate_switched: f.is_bit_rate_switched(),
                    },
                    dlc: f.dlc() as u8,
                    data: f.data().to_vec(),
                }
            }
        };

        Ok(frame)
    }

    impl Driver for SerialDriver {
        fn read_frames(&mut self) -> DriverResult<Vec<CanFrame>> {
            self.socket
                .read()
                .map_err(|e| match e {
                    slcan::ReadError::Io(e) => io_read(e),
                    other => DriverError::Read(format!("{other:?}")),
                })
                .and_then(from_wire)
                .map(|frame| vec![frame])
        }

        fn write_frame(&mut self, f: CanFrame) -> DriverResult<()> {
            let id = if f.identity.is_extended() {
                slcan::ExtendedId::new(f.identity.raw_id()).map(slcan::Id::Extended)
            } else {
                u16::try_from(f.identity.raw_id())
                    .ok()
                    .and_then(slcan::StandardId::new)
                    .map(slcan::Id::Standard)
            }
            .ok_or_else(|| DriverError::Write("invalid ID".into()))?;
            let wire: slcan::CanFrame = match f.kind {
                FrameKind::Data => slcan::Can2Frame::new_data(id, &f.data).map(Into::into),
                FrameKind::Remote => {
                    slcan::Can2Frame::new_remote(id, f.dlc as usize).map(Into::into)
                }
                FrameKind::Fd { bit_rate_switched } => slcan::CanFdFrame::new(id, &f.data)
                    .map(|f| f.with_bit_rate_switched(bit_rate_switched).into()),
            }
            .ok_or_else(|| DriverError::Write("invalid payload".into()))?;
            self.socket
                .send(wire)
                .map_err(|e| DriverError::Write(e.to_string()))
        }

        fn bus_speed(&self) -> Option<CanBusSpeed> {
            Some(self.speed)
        }

        fn close(&mut self) -> DriverResult<()> {
            self.socket
                .close()
                .map_err(|e| DriverError::Write(e.to_string()))
        }
    }
}
