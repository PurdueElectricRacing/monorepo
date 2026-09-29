use crate::can::driver::{CanDriver, DriverError, DriverReadError, DriverResult};
use crate::connection::CanBusSpeed;
use crate::messages::{FilAdcInstance, FilExpectationEvent, FilExpectationStatus, FilGpioPort};
use slcan::CanFrame;
use std::io::{BufRead, BufReader, BufWriter, Write};
use std::process::{Child, ChildStdin, Command, Stdio};
use std::sync::mpsc::{self, Receiver, RecvTimeoutError};
use std::sync::{Arc, RwLock};
use std::thread;
use std::time::{Duration, Instant};

const FIL_READ_TIMEOUT_MS: u64 = 1;
const FIL_MAX_FRAMES_PER_POLL: usize = 256;
const FIL_MAX_EXPECTATION_EVENTS: usize = 256;

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

fn watch_network_args(options: &crate::settings::FilRunOptions) -> DriverResult<Vec<String>> {
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
    args.push("--control-stdin".into());
    Ok(args)
}

pub struct FilDriver {
    child: Child,
    input: BufWriter<ChildStdin>,
    output: Receiver<Result<FilCanEvent, String>>,
    gpio_output: Receiver<FilGpioEvent>,
    expectation_output: Receiver<FilExpectationEvent>,
    bus: String,
    trace_bus: Arc<RwLock<Option<String>>>,
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
                    crate::fil_config::build_network(spec, executable).map_err(|error| {
                        DriverError::ConnectionFailed(format!("Invalid built FIL network: {error}"))
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
                    |error| {
                        DriverError::ConnectionFailed(format!(
                            "Invalid FIL network {}: {error}",
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
        let options = watch_network_args(run_options)?;
        let mut child = Command::new(executable)
            .arg("watch-network")
            .arg(&effective_network)
            .args(options)
            .stdin(Stdio::piped())
            .stdout(Stdio::piped())
            .stderr(Stdio::piped())
            .spawn()
            .map_err(|error| {
                DriverError::ConnectionFailed(format!("Failed to launch FIL: {error}"))
            })?;
        let stdout = child
            .stdout
            .take()
            .ok_or_else(|| DriverError::ConnectionFailed("Failed to capture FIL output".into()))?;
        let stdin = child.stdin.take().ok_or_else(|| {
            DriverError::ConnectionFailed("Failed to open FIL control input".into())
        })?;
        let stderr = child
            .stderr
            .take()
            .ok_or_else(|| DriverError::ConnectionFailed("Failed to capture FIL stderr".into()))?;
        thread::spawn(move || {
            for line in BufReader::new(stderr).lines() {
                match line {
                    Ok(line) => log::warn!("FIL: {line}"),
                    Err(error) => {
                        log::warn!("Failed to read FIL stderr: {error}");
                        break;
                    }
                }
            }
        });
        let (output_tx, output) = mpsc::channel();
        let (gpio_output_tx, gpio_output) = mpsc::channel();
        let (expectation_tx, expectation_output) = mpsc::channel();
        let trace_bus = Arc::new(RwLock::new(trace_bus.clone()));
        let reader_trace_bus = Arc::clone(&trace_bus);
        thread::spawn(move || {
            let mut reader = BufReader::new(stdout);
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
                            if expectation_tx.send(event).is_err() {
                                break;
                            }
                            continue;
                        }
                        if let Some(event) = parse_fil_gpio(&line) {
                            if gpio_output_tx.send(event).is_err() {
                                break;
                            }
                            continue;
                        }
                        if let Some(event) = parse_fil_can_tx(&line) {
                            let selected_bus = reader_trace_bus
                                .read()
                                .unwrap_or_else(std::sync::PoisonError::into_inner)
                                .clone();
                            if trace_source_matches(&event.source, selected_bus.as_deref())
                                && output_tx.send(Ok(event)).is_err()
                            {
                                break;
                            }
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
            input: BufWriter::new(stdin),
            output,
            gpio_output,
            expectation_output,
            bus: bus.into(),
            trace_bus,
            connected: true,
        })
    }
}

fn parse_fil_expectation(line: &str) -> Option<FilExpectationEvent> {
    const KEYS: &[&str] = &[
        "check_id",
        "script",
        "stimulus_index",
        "expect_index",
        "status",
        "expected_bus",
        "expected_id",
        "expected_extended",
        "expected_data",
        "window_start_ns",
        "window_end_ns",
        "matched_bus",
        "matched_id",
        "matched_data",
        "matched_origin",
        "matched_time_ns",
        "reason",
    ];
    let tokens: Vec<&str> = line.split_ascii_whitespace().collect();
    let kind_index = tokens.iter().position(|token| {
        matches!(
            *token,
            "expectation_pending"
                | "expectation_pass"
                | "expectation_fail"
                | "expectation_incomplete"
        )
    })?;
    let status = match tokens[kind_index] {
        "expectation_pending" => FilExpectationStatus::Pending,
        "expectation_pass" => FilExpectationStatus::Pass,
        "expectation_fail" => FilExpectationStatus::Fail,
        "expectation_incomplete" => FilExpectationStatus::Incomplete,
        _ => return None,
    };
    let mut fields = std::collections::HashMap::<&str, String>::new();
    let mut current: Option<&str> = None;
    for token in tokens.iter().skip(kind_index + 1) {
        if let Some((key, value)) = token.split_once('=')
            && KEYS.contains(&key)
        {
            current = Some(key);
            fields.insert(key, value.to_owned());
        } else if let Some(key) = current {
            let value = fields.get_mut(key)?;
            value.push(' ');
            value.push_str(token);
        }
    }
    let value = |name: &str| fields.get(name).map(String::as_str);
    let parse_id = |v: &str| u32::from_str_radix(v.trim_start_matches("0x"), 16).ok();
    let parse_data = |v: &str| {
        let bytes = v.trim();
        if bytes.len() % 2 != 0 {
            return None;
        }
        bytes
            .as_bytes()
            .chunks_exact(2)
            .map(|pair| u8::from_str_radix(std::str::from_utf8(pair).ok()?, 16).ok())
            .collect::<Option<Vec<u8>>>()
    };
    Some(FilExpectationEvent {
        check_id: value("check_id")?.to_owned(),
        script: value("script")?.to_owned(),
        status,
        expected_bus: value("expected_bus")?.to_owned(),
        expected_id: parse_id(value("expected_id")?)?,
        expected_extended: value("expected_extended")?.parse().ok()?,
        expected_data: parse_data(value("expected_data")?)?,
        window_start_ns: value("window_start_ns")?.parse().ok()?,
        window_end_ns: value("window_end_ns")?.parse().ok()?,
        matched_bus: value("matched_bus").map(str::to_owned),
        matched_id: value("matched_id").and_then(parse_id),
        matched_data: value("matched_data").and_then(parse_data),
        matched_origin: value("matched_origin").map(str::to_owned),
        matched_time_ns: value("matched_time_ns").and_then(|v| v.parse().ok()),
        reason: value("reason").map(str::to_owned),
    })
}

fn parse_fil_can_tx(line: &str) -> Option<FilCanEvent> {
    let mut fields = line.split_ascii_whitespace();
    let mut source = None;
    loop {
        let field = fields.next()?;
        if field == "can_tx" {
            break;
        }
        source = Some(field);
    }
    let source = source?;
    let (bus, node) = source.split_once('/')?;
    if bus.is_empty() || node.is_empty() {
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
                let bytes = value.as_bytes();
                if !bytes.len().is_multiple_of(2) || bytes.len() > 16 {
                    return None;
                }
                let decoded = bytes
                    .chunks_exact(2)
                    .map(|pair| {
                        std::str::from_utf8(pair)
                            .ok()
                            .and_then(|hex| u8::from_str_radix(hex, 16).ok())
                    })
                    .collect::<Option<Vec<_>>>()?;
                data = Some(decoded);
            }
            _ => {}
        }
    }
    let id = id?;
    let data = data?;
    if fd {
        return None;
    }
    let frame_id = if extended {
        slcan::ExtendedId::new(id).map(slcan::Id::Extended)?
    } else {
        slcan::StandardId::new(u16::try_from(id).ok()?).map(slcan::Id::Standard)?
    };
    let frame = slcan::Can2Frame::new_data(frame_id, &data).map(Into::into)?;
    Some(FilCanEvent {
        source: source.into(),
        frame,
    })
}

fn trace_source_matches(source: &str, trace_bus: Option<&str>) -> bool {
    trace_bus.is_none_or(|bus| {
        source
            .strip_prefix(bus)
            .is_some_and(|suffix| suffix.starts_with('/') && suffix.len() > 1)
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
        port: FilGpioPort::parse(port)?,
        pin: pin?,
        value: value?,
        output: kind == "gpio_output",
    })
}

fn format_fil_injection(bus: &str, frame: CanFrame) -> DriverResult<String> {
    let CanFrame::Can2(frame) = frame else {
        return Err(DriverError::WriteError(
            "FIL injection does not yet support CAN FD frames".into(),
        ));
    };
    let id = match frame.id() {
        slcan::Id::Standard(id) => u32::from(id.as_raw()),
        slcan::Id::Extended(id) => id.as_raw(),
    };
    let data = frame.data().ok_or_else(|| {
        DriverError::WriteError("FIL injection does not support remote frames".into())
    })?;
    let mut command = format!("{bus}:0x{id:x}:");
    for byte in data {
        command.push_str(&format!("{byte:02x}"));
    }
    Ok(command)
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
        let remaining = deadline.saturating_duration_since(Instant::now());
        let event = match output.recv_timeout(remaining) {
            Ok(Ok(event)) => event,
            Ok(Err(error)) => {
                return Err(DriverError::ReadError(DriverReadError::IoError(error)));
            }
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
            Ok(event) if trace_source_matches(&event.source, trace_bus) => {
                frames.push(event.frame);
            }
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
        let trace_bus = self
            .trace_bus
            .read()
            .unwrap_or_else(std::sync::PoisonError::into_inner)
            .clone();
        let result = receive_fil_frames(
            &self.output,
            Duration::from_millis(FIL_READ_TIMEOUT_MS),
            trace_bus.as_deref(),
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
        let command = format_fil_injection(&self.bus, frame)?;
        writeln!(self.input, "{command}")
            .and_then(|_| self.input.flush())
            .map_err(|error| {
                self.connected = false;
                DriverError::WriteError(format!("Failed to send frame to FIL: {error}"))
            })
    }

    fn is_connected(&self) -> bool {
        self.connected
    }

    fn bus_speed(&self) -> Option<CanBusSpeed> {
        Some(CanBusSpeed::Kbps500)
    }

    fn close(&mut self) -> DriverResult<()> {
        self.connected = false;
        let _ = self.child.kill();
        let _ = self.child.wait();
        Ok(())
    }
}

fn format_adc_injection(
    board: &str,
    instance: FilAdcInstance,
    channel: u8,
    value: u16,
) -> DriverResult<String> {
    if board.is_empty() || channel > 19 || value > 4095 || board.contains(char::is_whitespace) {
        return Err(DriverError::WriteError(
            "ADC injection requires a board, ADC instance, channel 0..19, and value 0..4095".into(),
        ));
    }
    Ok(format!(
        "adc {board} {} {channel} {value}",
        instance.as_str()
    ))
}

fn format_gpio_injection(
    board: &str,
    port: FilGpioPort,
    pin: u8,
    value: Option<bool>,
) -> DriverResult<String> {
    if board.is_empty() || pin > 15 || board.contains(char::is_whitespace) {
        return Err(DriverError::WriteError(
            "GPIO control requires a board, GPIO port, and pin 0..15".into(),
        ));
    }
    let value = match value {
        Some(false) => "0",
        Some(true) => "1",
        None => "release",
    };
    Ok(format!("gpio {board} {} {pin} {value}", port.as_str()))
}

impl FilDriver {
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
        let command = format_adc_injection(board, instance, channel, value)?;
        writeln!(self.input, "{command}")
            .and_then(|_| self.input.flush())
            .map_err(|error| {
                self.connected = false;
                DriverError::WriteError(format!("Failed to send ADC value to FIL: {error}"))
            })
    }

    pub fn set_gpio(
        &mut self,
        board: &str,
        port: FilGpioPort,
        pin: u8,
        value: Option<bool>,
    ) -> DriverResult<()> {
        let command = format_gpio_injection(board, port, pin, value)?;
        writeln!(self.input, "{command}")
            .and_then(|_| self.input.flush())
            .map_err(|error| {
                self.connected = false;
                DriverError::WriteError(format!("Failed to control FIL GPIO: {error}"))
            })
    }
}

impl Drop for FilDriver {
    fn drop(&mut self) {
        let _ = self.child.kill();
        let _ = self.child.wait();
    }
}

#[cfg(test)]
mod fil_tests {
    use crate::can::driver::fil::{
        DriverError, DriverReadError, FIL_MAX_FRAMES_PER_POLL, FilCanEvent, format_adc_injection,
        format_fil_injection, format_gpio_injection, parse_fil_can_tx, parse_fil_expectation,
        parse_fil_gpio, receive_fil_frames, trace_source_matches, watch_network_args,
    };
    use crate::messages::{FilAdcInstance, FilGpioPort};
    use std::sync::mpsc;
    use std::time::Duration;

    #[test]
    fn default_options_keep_live_can_and_gpio_with_pacing() {
        let options = crate::settings::FilRunOptions::default();
        assert_eq!(options.adc_decimation, 32);
        let args = watch_network_args(&options).unwrap();
        assert!(
            args.windows(2)
                .any(|pair| pair == ["--adc-decimation", "32"])
        );
        assert!(!args.iter().any(|arg| arg == "--no-wall-pacing"));
        assert!(!args.iter().any(|arg| arg == "--no-loop-batching"));
        for flag in [
            "can_tx",
            "gpio_input",
            "gpio_output",
            "expectation_pending",
            "expectation_pass",
            "expectation_fail",
            "expectation_incomplete",
            "--control-stdin",
        ] {
            assert!(args.iter().any(|arg| arg == flag));
        }
    }

    #[test]
    fn forwards_run_options_and_keeps_required_controls() {
        let mut options = crate::settings::FilRunOptions::default();
        options.adc_decimation = 8;
        options.extra_live_filters = "can_rx, irq".into();
        options.strict_mmio = true;
        options.wall_pacing = false;
        options.loop_batching = false;
        options.trace_instructions = true;
        options.detect_spin = true;
        let args = watch_network_args(&options).unwrap();
        for flag in [
            "--adc-decimation",
            "8",
            "--strict-mmio",
            "--no-wall-pacing",
            "--no-loop-batching",
            "--trace-instr",
            "--detect-spin",
            "--control-stdin",
            "can_tx",
            "gpio_input",
            "gpio_output",
            "expectation_pending",
            "expectation_pass",
            "expectation_fail",
            "expectation_incomplete",
            "can_rx",
            "irq",
        ] {
            assert!(args.iter().any(|arg| arg == flag), "missing {flag}");
        }
        options.adc_decimation = 0;
        assert!(watch_network_args(&options).is_err());
    }

    #[test]
    fn parses_expectation_lifecycle_and_spaced_script_names_independent_of_bus() {
        let pending = parse_fil_expectation("[12.345 ms] expectation/a script with spaces expectation_pending check_id=stimulus/0/a script with spaces/expect/1 script=a script with spaces expected_bus=vehicle expected_id=0x321 expected_extended=false expected_data=0102 window_start_ns=1000000 window_end_ns=2000000").unwrap();
        assert_eq!(pending.check_id, "stimulus/0/a script with spaces/expect/1");
        assert_eq!(pending.script, "a script with spaces");
        assert_eq!(
            pending.status,
            crate::messages::FilExpectationStatus::Pending
        );
        assert_eq!(pending.window_end_ns, 2_000_000);
        let pass = parse_fil_expectation("[12.346 ms] expectation/a script expectation_pass check_id=stimulus/0/a/expect/1 script=a expected_bus=vehicle expected_id=0x321 expected_extended=true expected_data=0102 window_start_ns=1000000 window_end_ns=2000000 matched_bus=vehicle matched_id=0x321 matched_data=0102 matched_origin=dashboard/FDCAN1 matched_time_ns=1234567").unwrap();
        assert_eq!(pass.status, crate::messages::FilExpectationStatus::Pass);
        assert!(pass.expected_extended);
        assert!(!pending.expected_extended);
        for (record_type, expected) in [
            (
                "expectation_fail",
                crate::messages::FilExpectationStatus::Fail,
            ),
            (
                "expectation_incomplete",
                crate::messages::FilExpectationStatus::Incomplete,
            ),
        ] {
            let line = format!(
                "[12.346 ms] expectation/a {record_type} check_id=stimulus/0/a/expect/1 script=a expected_bus=vehicle expected_id=0x321 expected_extended=false expected_data=0102 window_start_ns=1000000 window_end_ns=2000000"
            );
            assert_eq!(parse_fil_expectation(&line).unwrap().status, expected);
        }
        assert_eq!(pass.matched_time_ns, Some(1_234_567));
        assert_eq!(pass.matched_origin.as_deref(), Some("dashboard/FDCAN1"));
        assert!(
            parse_fil_expectation("[0.000 ms] x expectation_fail check_id=x expected_id=bad")
                .is_none()
        );
        assert!(parse_fil_expectation("[0.000 ms] x expectation_pass check_id=x script=x expected_bus=b expected_id=0x1 expected_data=0 window_start_ns=1 window_end_ns=2").is_none());
    }

    #[test]
    fn parses_fil_live_can_tx_records() {
        let vcan = parse_fil_can_tx(
            "[5.000 ms] vcan/main.FDCAN1  can_tx id=0x123 extended=false fd=false brs=false dlc=4 length=4 data=01020304",
        )
        .expect("valid FIL frame");
        let mcan = parse_fil_can_tx(
            "[5.000 ms] mcan/main.FDCAN1  can_tx id=0x123 extended=false fd=false brs=false dlc=4 length=4 data=05060708",
        )
        .expect("valid FIL frame on another bus");
        let slcan::CanFrame::Can2(frame) = &vcan.frame else {
            panic!("expected CAN 2.0 frame")
        };
        assert_eq!(vcan.source, "vcan/main.FDCAN1");
        assert!(matches!(
            frame.id(),
            slcan::Id::Standard(id) if id.as_raw() == 0x123
        ));
        assert_eq!(frame.data(), Some(&[1, 2, 3, 4][..]));
        let slcan::CanFrame::Can2(mcan_frame) = &mcan.frame else {
            panic!("expected CAN 2.0 frame")
        };
        assert_eq!(mcan_frame.id(), frame.id());
        assert!(!trace_source_matches(&mcan.source, Some("vcan")));
        assert!(trace_source_matches(&vcan.source, Some("vcan")));
        assert!(!trace_source_matches(&vcan.source, Some("vc")));
        assert!(trace_source_matches(&mcan.source, None));
        assert!(!trace_source_matches("malformed", Some("vcan")));

        let (sender, receiver) = mpsc::channel();
        sender.send(Ok(mcan.clone())).unwrap();
        sender.send(Ok(vcan.clone())).unwrap();
        let frames = receive_fil_frames(&receiver, Duration::from_millis(20), Some("vcan"))
            .expect("receive only the selected bus");
        assert_eq!(frames.len(), 1);
        assert_eq!(frames[0], vcan.frame);

        let (sender, receiver) = mpsc::channel();
        sender.send(Ok(mcan)).unwrap();
        sender.send(Ok(vcan)).unwrap();
        let frames = receive_fil_frames(&receiver, Duration::ZERO, None)
            .expect("receive all buses by default");
        assert_eq!(frames.len(), 2);
    }

    #[test]
    fn ignores_non_can_and_rejects_can_fd_payloads() {
        assert!(parse_fil_can_tx("[1.000 ms] world world_start boards=6").is_none());
        assert!(parse_fil_can_tx("[5.000 ms] source can_tx id=0x123 data=01").is_none());
        assert!(
            parse_fil_can_tx(
                "[5.000 ms] mcan/main.FDCAN1 can_tx id=0x123 extended=false fd=true data=000102030405060708",
            )
            .is_none()
        );
    }

    #[test]
    fn formats_frames_for_fil_stdin_control() {
        let id = slcan::StandardId::new(0x123).expect("standard id");
        let frame = slcan::Can2Frame::new_data(id, &[1, 2, 0xab, 0xcd]).expect("data frame");
        assert_eq!(
            format_fil_injection("vehicle", frame.into()).expect("valid injection"),
            "vehicle:0x123:0102abcd"
        );
    }

    #[test]
    fn quiet_fil_output_returns_promptly() {
        let (_sender, receiver) = mpsc::channel();
        let started = std::time::Instant::now();
        assert!(matches!(
            receive_fil_frames(&receiver, Duration::from_millis(1), None),
            Err(DriverError::ReadError(DriverReadError::Timeout))
        ));
        assert!(started.elapsed() < Duration::from_millis(100));
    }

    #[test]
    fn sustained_fil_output_is_processed_in_bounded_batches() {
        let (sender, receiver) = mpsc::channel();
        let id = slcan::StandardId::new(0x123).expect("standard id");
        let frame = slcan::Can2Frame::new_data(id, &[1]).expect("data frame");
        for _ in 0..(FIL_MAX_FRAMES_PER_POLL + 1) {
            sender
                .send(Ok(FilCanEvent {
                    source: "vehicle/main.FDCAN1".into(),
                    frame: frame.clone().into(),
                }))
                .expect("queue FIL frame");
        }

        let frames =
            receive_fil_frames(&receiver, Duration::ZERO, None).expect("receive FIL frames");
        assert_eq!(frames.len(), FIL_MAX_FRAMES_PER_POLL);
        assert!(receiver.try_recv().is_ok(), "leaves excess traffic queued");
    }

    #[test]
    fn formats_typed_adc_and_gpio_commands() {
        assert_eq!(
            format_adc_injection("dashboard", FilAdcInstance::Adc1, 1, 2048).unwrap(),
            "adc dashboard ADC1 1 2048"
        );
        assert_eq!(
            format_gpio_injection("dashboard", FilGpioPort::GpioA, 3, None).unwrap(),
            "gpio dashboard GPIOA 3 release"
        );
    }

    #[test]
    fn parses_fil_gpio_records() {
        let output = parse_fil_gpio("[12.000 ms] dashboard.GPIOA  gpio_output pin=3 value=1")
            .expect("GPIO output");
        assert_eq!(output.board, "dashboard");
        assert_eq!(output.port, FilGpioPort::GpioA);
        assert_eq!(output.pin, 3);
        assert_eq!(output.value, Some(true));
        assert!(output.output);

        let input = parse_fil_gpio("[13.000 ms] dashboard.GPIOA  gpio_input pin=3 value=release")
            .expect("GPIO input release");
        assert_eq!(input.value, None);
        assert!(!input.output);
        assert!(parse_fil_gpio("[14.000 ms] dashboard.GPIOH gpio_input pin=3 value=1").is_none());
        assert!(parse_fil_gpio("[15.000 ms] dashboard.gpioa gpio_input pin=3 value=1").is_none());
    }
}
