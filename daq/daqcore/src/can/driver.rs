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
pub trait Driver {
    fn read_frames(&mut self) -> DriverResult<Vec<CanFrame>>;
    fn write_frame(&mut self, frame: CanFrame) -> DriverResult<()>;
    fn bus_speed(&self) -> Option<CanBusSpeed> {
        None
    }

    fn close(&mut self) -> DriverResult<()> {
        Ok(())
    }
    fn take_fil_gpio_events(&mut self) -> Vec<FilGpioEvent> {
        Vec::new()
    }
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

struct FilDriver {
    child: std::process::Child,
    input: std::io::BufWriter<std::process::ChildStdin>,
    output: std::sync::mpsc::Receiver<Result<CanFrame, String>>,
    gpio_output: std::sync::mpsc::Receiver<FilGpioEvent>,
    bus: String,
}
impl FilDriver {
    fn new(
        executable: &std::path::Path,
        network: &std::path::Path,
        bus: &str,
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
        let mut child = std::process::Command::new(executable)
            .arg("watch-network")
            .arg(network)
            .args([
                "--duration-ms",
                "0",
                "--max-instructions",
                "18446744073709551615",
                "--live-filter",
                "can_tx",
                "--live-filter",
                "gpio_input",
                "--live-filter",
                "gpio_output",
                "--control-stdin",
            ])
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
            bus: bus.into(),
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
    fn take_fil_gpio_events(&mut self) -> Vec<FilGpioEvent> {
        self.gpio_output.try_iter().take(256).collect()
    }
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
