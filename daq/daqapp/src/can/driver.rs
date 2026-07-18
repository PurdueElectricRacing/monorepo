use crate::connection::{CanBusSpeed, ConnectionSource};
use crate::util;
use rand::prelude::*;
use serialport::{ClearBuffer, SerialPort};
use slcan::sync::CanSocket;
use slcan::{CanFrame, OperatingMode};
use std::collections::VecDeque;
use std::io::{BufRead, BufReader};
use std::net::UdpSocket;
use std::process::{Child, ChildStdout, Command, Stdio};
use std::time::Duration;

const SERIAL_BAUD_RATE: u32 = 115_200;
const SERIAL_TIMEOUT_MS: u64 = 10;

const UDP_RAW_FRAME_SIZE: usize = 16; // 4 bytes ticks_ms + 4 bytes identity + 8 bytes payload
const UDP_MAX_PACKET_SIZE: usize = 2048;

pub type DriverResult<T> = Result<T, DriverError>;

#[derive(Debug)]
pub enum DriverReadError {
    Timeout,
    IoError(String),
    Other(String),
}

#[derive(Debug)]
pub enum DriverError {
    ConnectionFailed(String),
    ReadError(DriverReadError),
    WriteError(String),
}

pub trait Driver {
    fn read_frames(&mut self) -> DriverResult<Vec<CanFrame>>;

    fn write_frame(&mut self, frame: CanFrame) -> DriverResult<()>;

    fn is_connected(&self) -> bool;

    fn bus_speed(&self) -> Option<CanBusSpeed>;

    fn close(&mut self) -> DriverResult<()>;
}

/// Serial CAN driver using SLCAN protocol
pub struct SerialDriver {
    socket: CanSocket<Box<dyn SerialPort>>,
    connected: bool,
    bus_speed: CanBusSpeed,
}

impl SerialDriver {
    pub fn new(port_path: &str, speed: CanBusSpeed) -> DriverResult<Self> {
        let port = serialport::new(port_path, SERIAL_BAUD_RATE)
            .timeout(Duration::from_millis(SERIAL_TIMEOUT_MS))
            .open()
            .map_err(|e| {
                DriverError::ConnectionFailed(format!("Failed to open port {}: {}", port_path, e))
            })?;

        let _ = port.clear(ClearBuffer::All);
        let mut socket = CanSocket::new(port);

        socket
            .set_operating_mode(OperatingMode::Normal)
            .map_err(|e| {
                DriverError::ConnectionFailed(format!("Failed to set operating mode: {}", e))
            })?;

        socket
            .open(speed.to_slcan_bitrate())
            .map_err(|e| DriverError::ConnectionFailed(format!("Failed to open CAN: {}", e)))?;

        Ok(Self {
            socket,
            connected: true,
            bus_speed: speed,
        })
    }
}

impl Driver for SerialDriver {
    fn read_frames(&mut self) -> DriverResult<Vec<CanFrame>> {
        self.socket
            .read()
            .map_err(|e| match e {
                slcan::ReadError::Io(io_err) => {
                    if io_err.kind() == std::io::ErrorKind::WouldBlock
                        || io_err.kind() == std::io::ErrorKind::TimedOut
                    {
                        DriverError::ReadError(DriverReadError::Timeout)
                    } else {
                        self.connected = false;
                        DriverError::ReadError(DriverReadError::IoError(format!(
                            "I/O error: {}",
                            io_err
                        )))
                    }
                }
                other => {
                    self.connected = false;
                    DriverError::ReadError(DriverReadError::Other(format!(
                        "Read error: {:?}",
                        other
                    )))
                }
            })
            .map(|frame| vec![frame]) // wrap single frame in a vector for consistency with UDP driver
    }

    fn write_frame(&mut self, frame: CanFrame) -> DriverResult<()> {
        self.socket.send(frame).map_err(|e| {
            self.connected = false;
            DriverError::WriteError(format!("Failed to write frame: {}", e))
        })
    }

    fn is_connected(&self) -> bool {
        self.connected
    }

    fn bus_speed(&self) -> Option<CanBusSpeed> {
        Some(self.bus_speed)
    }

    fn close(&mut self) -> DriverResult<()> {
        self.socket
            .close()
            .map_err(|e| DriverError::WriteError(format!("Failed to close: {}", e)))?;
        self.connected = false;
        Ok(())
    }
}

/// UDP CAN driver (placeholder for future implementation)
pub struct UdpDriver {
    port: u16,
    socket: UdpSocket,
    connected: bool,
}

impl UdpDriver {
    pub fn new(port: u16) -> DriverResult<Self> {
        let udp_addr = format!("0.0.0.0:{}", port);
        let socket = UdpSocket::bind(udp_addr).map_err(|e| {
            DriverError::ConnectionFailed(format!("Failed to bind to port {}: {}", port, e))
        })?;
        // use a short read timeout instead of nonblocking so recv_from returns from timeout??
        // i think it should also work in nonblocking mode i just put it like this for testing
        socket.set_broadcast(true).map_err(|e| {
            DriverError::ConnectionFailed(format!("Failed to set broadcast: {}", e))
        })?;

        socket
            .set_read_timeout(Some(Duration::from_millis(5000)))
            .map_err(|e| {
                DriverError::ConnectionFailed(format!("Failed to set read timeout: {}", e))
            })?;

        Ok(Self {
            port,
            socket,
            connected: true,
        })
    }
}

impl Driver for UdpDriver {
    fn read_frames(&mut self) -> DriverResult<Vec<CanFrame>> {
        let mut buf = [0; UDP_MAX_PACKET_SIZE];
        match self.socket.recv_from(&mut buf) {
            Ok((num_bytes, _src_port)) => parse_udp_buffer(&buf, num_bytes),
            Err(e) => {
                log::warn!("{}", e);
                if e.kind() == std::io::ErrorKind::WouldBlock
                    || e.kind() == std::io::ErrorKind::TimedOut
                {
                    Err(DriverError::ReadError(DriverReadError::Timeout))
                } else {
                    self.connected = false;
                    Err(DriverError::ReadError(DriverReadError::IoError(format!(
                        "UDP I/O error: {}",
                        e
                    ))))
                }
            }
        }
    }

    fn write_frame(&mut self, _frame: CanFrame) -> DriverResult<()> {
        log::error!("UDP write requested but not supported; ignoring frame.");
        Ok(())
    }

    fn is_connected(&self) -> bool {
        self.connected
    }

    fn bus_speed(&self) -> Option<CanBusSpeed> {
        None
    }

    fn close(&mut self) -> DriverResult<()> {
        self.connected = false;
        Ok(())
    }
}

struct SimulatedDriver {
    connected: bool,
    pub parser: Option<can_decode::Parser>,
}

impl SimulatedDriver {
    fn new(connected: bool, dbc_path: Option<std::path::PathBuf>) -> DriverResult<Self> {
        if connected {
            Ok(Self {
                connected,
                parser: dbc_path.and_then(|path| can_decode::Parser::from_dbc_file(&path).ok()),
            })
        } else {
            Err(DriverError::ConnectionFailed(
                "Simulated driver initialized as disconnected".into(),
            ))
        }
    }
}

impl Driver for SimulatedDriver {
    fn read_frames(&mut self) -> DriverResult<Vec<CanFrame>> {
        if self.connected {
            let mut rng = rand::rng();

            let random_msg = self.parser.as_ref().and_then(|p| {
                let msgs = p.msg_defs();
                if msgs.is_empty() {
                    None
                } else {
                    Some(msgs.choose(&mut rng).expect("msgs is not empty").clone())
                }
            });

            if let Some(msg) = random_msg {
                let mut data = vec![0u8; msg.size as usize];
                rng.fill_bytes(&mut data);
                let id = match msg.id {
                    can_dbc::MessageId::Extended(id) => slcan::Id::Extended(
                        slcan::ExtendedId::new(id).expect("invalid extended id"),
                    ),
                    can_dbc::MessageId::Standard(id) => slcan::Id::Standard(
                        slcan::StandardId::new(id).expect("invalid standard id"),
                    ),
                };
                let can_frame = slcan::Can2Frame::new_data(id, &data)
                    .expect("failed to create CAN frame from random data");
                Ok(vec![can_frame.into()])
            } else {
                // If no DBC is loaded, just return random frames with random IDs and data
                let id = rng.random_range(0..=util::can::STANDARD_ID_MASK) as u16;
                let sid = slcan::StandardId::new(id).expect("invalid standard id");
                let mut data = [0u8; 8];
                rng.fill_bytes(&mut data);
                let can2 = slcan::Can2Frame::new_data(sid, &data)
                    .expect("failed to create CAN frame from random data");
                Ok(vec![can2.into()])
            }
        } else {
            Err(DriverError::ReadError(DriverReadError::Other(
                "Simulated driver is disconnected".into(),
            )))
        }
    }

    fn write_frame(&mut self, _frame: CanFrame) -> DriverResult<()> {
        if self.connected {
            Ok(())
        } else {
            Err(DriverError::WriteError(
                "Simulated driver is disconnected".into(),
            ))
        }
    }

    fn is_connected(&self) -> bool {
        self.connected
    }

    fn bus_speed(&self) -> Option<CanBusSpeed> {
        None
    }

    fn close(&mut self) -> DriverResult<()> {
        self.connected = false;
        Ok(())
    }
}

struct LoopbackDriver {
    connected: bool,
    queued_frames: VecDeque<CanFrame>,
}

struct FilDriver {
    child: Child,
    output: BufReader<ChildStdout>,
    connected: bool,
}

impl FilDriver {
    fn new(executable: &std::path::Path, network: &std::path::Path) -> DriverResult<Self> {
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
        let mut child = Command::new(executable)
            .arg("watch-network")
            .arg(network)
            .args([
                "--duration-ms",
                "86400000",
                "--max-instructions",
                "2000000000000",
                "--live-filter",
                "can_tx",
            ])
            .stdout(Stdio::piped())
            .stderr(Stdio::null())
            .spawn()
            .map_err(|error| {
                DriverError::ConnectionFailed(format!("Failed to launch FIL: {error}"))
            })?;
        let stdout = child
            .stdout
            .take()
            .ok_or_else(|| DriverError::ConnectionFailed("Failed to capture FIL output".into()))?;
        Ok(Self {
            child,
            output: BufReader::new(stdout),
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

impl Driver for FilDriver {
    fn read_frames(&mut self) -> DriverResult<Vec<CanFrame>> {
        loop {
            let mut line = String::new();
            match self.output.read_line(&mut line) {
                Ok(0) => {
                    self.connected = false;
                    return Err(DriverError::ReadError(DriverReadError::IoError(
                        "FIL process exited".into(),
                    )));
                }
                Ok(_) => {
                    if let Some(frame) = parse_fil_can_tx(&line) {
                        return Ok(vec![frame]);
                    }
                }
                Err(error) => {
                    self.connected = false;
                    return Err(DriverError::ReadError(DriverReadError::IoError(format!(
                        "Failed to read FIL output: {error}"
                    ))));
                }
            }
        }
    }

    fn write_frame(&mut self, _frame: CanFrame) -> DriverResult<()> {
        Err(DriverError::WriteError(
            "The FIL live connection is receive-only".into(),
        ))
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

impl Drop for FilDriver {
    fn drop(&mut self) {
        let _ = self.child.kill();
        let _ = self.child.wait();
    }
}

impl LoopbackDriver {
    fn new() -> Self {
        Self {
            connected: true,
            queued_frames: VecDeque::new(),
        }
    }
}

impl Driver for LoopbackDriver {
    fn read_frames(&mut self) -> DriverResult<Vec<CanFrame>> {
        if !self.connected {
            return Err(DriverError::ReadError(DriverReadError::Other(
                "Loopback driver is disconnected".into(),
            )));
        }

        if self.queued_frames.is_empty() {
            return Err(DriverError::ReadError(DriverReadError::Timeout));
        }

        Ok(self.queued_frames.drain(..).collect())
    }

    fn write_frame(&mut self, frame: CanFrame) -> DriverResult<()> {
        if self.connected {
            self.queued_frames.push_back(frame);
            Ok(())
        } else {
            Err(DriverError::WriteError(
                "Loopback driver is disconnected".into(),
            ))
        }
    }

    fn is_connected(&self) -> bool {
        self.connected
    }

    fn bus_speed(&self) -> Option<CanBusSpeed> {
        None
    }

    fn close(&mut self) -> DriverResult<()> {
        self.connected = false;
        self.queued_frames.clear();
        Ok(())
    }
}

pub fn parse_udp_buffer(
    buf: &[u8; UDP_MAX_PACKET_SIZE],
    num_bytes: usize,
) -> DriverResult<Vec<CanFrame>> {
    if num_bytes < UDP_RAW_FRAME_SIZE {
        return Err(DriverError::ReadError(DriverReadError::Other(format!(
            "Received packet too small: {} bytes",
            num_bytes
        ))));
    } else if !num_bytes.is_multiple_of(UDP_RAW_FRAME_SIZE) {
        log::warn!(
            "Received packet of size {} which is not a multiple of raw frame size {}; some data may be ignored",
            num_bytes,
            UDP_RAW_FRAME_SIZE
        );
    }

    let x = num_bytes / UDP_RAW_FRAME_SIZE;
    println!("Parsing UDP frame: {num_bytes} ({x})");

    let mut frames = Vec::with_capacity(num_bytes / UDP_RAW_FRAME_SIZE);
    let mask_id = (1u32 << 29) - 1;

    // TODO: use the daq_parse way with bytemuck?
    let mut chunks = buf[..num_bytes].chunks_exact(UDP_RAW_FRAME_SIZE);
    for chunk in &mut chunks {
        // Parse can frame from UDP packet according to new timestamped frame format
        // Format: [4 bytes ticks_ms] [4 bytes identity] [8 bytes payload]
        // Identity format: [1 bit bus ID] [1 bit isExtID] [1 bit reserved] [29 bits CAN ID]
        // (definitions from spmc.h)
        let identity = u32::from_le_bytes(chunk[4..8].try_into().unwrap());
        let payload = &chunk[8..16];

        let id = identity & mask_id;

        let frame = if id <= 0x7FF {
            let sid = slcan::StandardId::new(id as u16).ok_or_else(|| {
                DriverError::ReadError(DriverReadError::Other("invalid standard id".into()))
            })?;

            let can2 = slcan::Can2Frame::new_data(sid, payload).ok_or_else(|| {
                DriverError::ReadError(DriverReadError::Other("invalid CAN2 data".into()))
            })?;

            can2.into()
        } else {
            let eid = slcan::ExtendedId::new(id).ok_or_else(|| {
                DriverError::ReadError(DriverReadError::Other("invalid extended id".into()))
            })?;

            let can2 = slcan::Can2Frame::new_data(eid, payload).ok_or_else(|| {
                DriverError::ReadError(DriverReadError::Other("invalid CAN2 data".into()))
            })?;

            can2.into()
        };

        frames.push(frame);
    }

    let remainder = chunks.remainder();
    if !remainder.is_empty() {
        log::warn!(
            "UDP packet had {} extra bytes (not a full CAN frame)",
            remainder.len()
        );
    }

    Ok(frames)
}

pub fn create_driver(source: &ConnectionSource) -> DriverResult<Box<dyn Driver>> {
    match source {
        ConnectionSource::Serial(path, speed) => Ok(Box::new(SerialDriver::new(path, *speed)?)),
        ConnectionSource::Udp(port) => Ok(Box::new(UdpDriver::new(*port)?)),
        ConnectionSource::Simulated(connected, dbc_path) => Ok(Box::new(SimulatedDriver::new(
            *connected,
            dbc_path.clone(),
        )?)),
        ConnectionSource::Fil {
            executable,
            network,
        } => Ok(Box::new(FilDriver::new(executable, network)?)),
        ConnectionSource::Loopback => Ok(Box::new(LoopbackDriver::new())),
    }
}

#[cfg(test)]
mod fil_tests {
    use super::parse_fil_can_tx;

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
}
