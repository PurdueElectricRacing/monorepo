use super::driver::{Driver, DriverError, DriverReadError, DriverResult, FilGpioEvent};
use crate::connection::CanBusSpeed;
use slcan::CanFrame;
use std::io::{BufRead, BufReader, BufWriter, Write};
use std::process::{Child, ChildStdin, Command, Stdio};
use std::sync::mpsc::{self, Receiver, RecvTimeoutError};
use std::thread;
use std::time::Duration;

const FIL_READ_TIMEOUT_MS: u64 = 1;
const FIL_MAX_FRAMES_PER_POLL: usize = 256;

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
    for filter in ["can_tx", "gpio_input", "gpio_output"] {
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
    output: Receiver<Result<CanFrame, String>>,
    gpio_output: Receiver<FilGpioEvent>,
    bus: String,
    connected: bool,
}

impl FilDriver {
    pub fn new(
        executable: &std::path::Path,
        network: &std::path::Path,
        bus: &str,
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
                        if let Some(event) = parse_fil_gpio(&line) {
                            if gpio_output_tx.send(event).is_err() {
                                break;
                            }
                            continue;
                        }
                        if let Some(frame) = parse_fil_can_tx(&line) {
                            if output_tx.send(Ok(frame)).is_err() {
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
            bus: bus.into(),
            connected: true,
        })
    }
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
    slcan::Can2Frame::new_data(frame_id, &data).map(Into::into)
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
    output: &Receiver<Result<CanFrame, String>>,
    timeout: Duration,
) -> DriverResult<Vec<CanFrame>> {
    let first = match output.recv_timeout(timeout) {
        Ok(result) => result,
        Err(RecvTimeoutError::Timeout) => {
            return Err(DriverError::ReadError(DriverReadError::Timeout));
        }
        Err(RecvTimeoutError::Disconnected) => Err("FIL output reader stopped".into()),
    };
    let mut frames = match first {
        Ok(frame) => vec![frame],
        Err(error) => return Err(DriverError::ReadError(DriverReadError::IoError(error))),
    };
    while frames.len() < FIL_MAX_FRAMES_PER_POLL {
        let Ok(result) = output.try_recv() else {
            break;
        };
        match result {
            Ok(frame) => frames.push(frame),
            Err(_) => break,
        }
    }
    Ok(frames)
}

impl Driver for FilDriver {
    fn needs_read_retry_sleep(&self) -> bool {
        false
    }

    fn read_frames(&mut self) -> DriverResult<Vec<CanFrame>> {
        let result = receive_fil_frames(&self.output, Duration::from_millis(FIL_READ_TIMEOUT_MS));
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

impl FilDriver {
    pub fn take_gpio_events(&mut self) -> Vec<FilGpioEvent> {
        self.gpio_output.try_iter().take(256).collect()
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
            return Err(DriverError::WriteError(
                "ADC injection requires a board, ADC instance, channel 0..19, and value 0..4095"
                    .into(),
            ));
        }
        writeln!(self.input, "adc {board} {instance} {channel} {value}")
            .and_then(|_| self.input.flush())
            .map_err(|error| {
                self.connected = false;
                DriverError::WriteError(format!("Failed to send ADC value to FIL: {error}"))
            })
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
            return Err(DriverError::WriteError(
                "GPIO control requires a board, GPIO port, and pin 0..15".into(),
            ));
        }
        let value = match value {
            Some(false) => "0",
            Some(true) => "1",
            None => "release",
        };
        writeln!(self.input, "gpio {board} {port} {pin} {value}")
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
    use super::{
        DriverError, DriverReadError, format_fil_injection, parse_fil_can_tx, parse_fil_gpio,
        receive_fil_frames, watch_network_args,
    };
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
        for flag in ["can_tx", "gpio_input", "gpio_output", "--control-stdin"] {
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
            "can_rx",
            "irq",
        ] {
            assert!(args.iter().any(|arg| arg == flag), "missing {flag}");
        }
        options.adc_decimation = 0;
        assert!(watch_network_args(&options).is_err());
    }

    #[test]
    fn parses_fil_live_can_tx_records() {
        let frame = parse_fil_can_tx(
            "[5.000 ms] vehicle/main.FDCAN1  can_tx id=0x123 extended=false fd=false brs=false dlc=4 length=4 data=01020304",
        ).expect("valid FIL frame");
        let slcan::CanFrame::Can2(frame) = frame else {
            panic!("expected CAN 2.0 frame")
        };
        assert!(matches!(
            frame.id(),
            slcan::Id::Standard(id) if id.as_raw() == 0x123
        ));
        assert_eq!(frame.data(), Some(&[1, 2, 3, 4][..]));
    }

    #[test]
    fn ignores_non_can_and_rejects_can_fd_payloads() {
        assert!(parse_fil_can_tx("[1.000 ms] world world_start boards=6").is_none());
        assert!(
            parse_fil_can_tx(
                "[5.000 ms] bus can_tx id=0x123 extended=false fd=true data=000102030405060708",
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
            receive_fil_frames(&receiver, Duration::from_millis(1)),
            Err(DriverError::ReadError(DriverReadError::Timeout))
        ));
        assert!(started.elapsed() < Duration::from_millis(100));
    }

    #[test]
    fn sustained_fil_output_is_processed_in_bounded_batches() {
        let (sender, receiver) = mpsc::channel();
        let id = slcan::StandardId::new(0x123).expect("standard id");
        let frame = slcan::Can2Frame::new_data(id, &[1]).expect("data frame");
        for _ in 0..(super::FIL_MAX_FRAMES_PER_POLL + 1) {
            sender
                .send(Ok(frame.clone().into()))
                .expect("queue FIL frame");
        }

        let frames = receive_fil_frames(&receiver, Duration::ZERO).expect("receive FIL frames");
        assert_eq!(frames.len(), super::FIL_MAX_FRAMES_PER_POLL);
        assert!(receiver.try_recv().is_ok(), "leaves excess traffic queued");
    }

    #[test]
    fn parses_fil_gpio_records() {
        let output = parse_fil_gpio("[12.000 ms] dashboard.GPIOA  gpio_output pin=3 value=1")
            .expect("GPIO output");
        assert_eq!(output.board, "dashboard");
        assert_eq!(output.port, "GPIOA");
        assert_eq!(output.pin, 3);
        assert_eq!(output.value, Some(true));
        assert!(output.output);

        let input = parse_fil_gpio("[13.000 ms] dashboard.GPIOA  gpio_input pin=3 value=release")
            .expect("GPIO input release");
        assert_eq!(input.value, None);
        assert!(!input.output);
    }
}
