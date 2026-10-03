use crate::connection::{CanBusSpeed, ConnectionSource};

use daqcore::can::{CanFrame, MessageId};
use daqcore::superdbc::BusDatabase;
use rand::prelude::*;
use serialport::{ClearBuffer, SerialPort};
use slcan::OperatingMode;
use slcan::sync::CanSocket;
use std::collections::VecDeque;
use std::net::UdpSocket;
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

    fn set_database(&mut self, _database: Option<BusDatabase>) {}

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
            .open(match speed {
                CanBusSpeed::Kbps250 => slcan::NominalBitRate::Rate250Kbit,
                CanBusSpeed::Kbps500 => slcan::NominalBitRate::Rate500Kbit,
            })
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
            .map(|frame| from_slcan(frame).into_iter().collect())
    }

    fn write_frame(&mut self, frame: CanFrame) -> DriverResult<()> {
        self.socket.send(to_slcan(frame)?).map_err(|e| {
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
    pub parser: Option<daqcore::superdbc::BusDatabase>,
}

impl SimulatedDriver {
    fn new(connected: bool, database: Option<BusDatabase>) -> DriverResult<Self> {
        if connected {
            Ok(Self {
                connected,
                parser: database,
            })
        } else {
            Err(DriverError::ConnectionFailed(
                "Simulation is disconnected".into(),
            ))
        }
    }
}

impl Driver for SimulatedDriver {
    fn set_database(&mut self, database: Option<BusDatabase>) {
        self.parser = database;
    }
    fn read_frames(&mut self) -> DriverResult<Vec<CanFrame>> {
        if !self.connected {
            return Err(DriverError::ReadError(DriverReadError::Other(
                "Simulation is disconnected".into(),
            )));
        }
        let mut rng = rand::rng();
        if let Some(msg) = self
            .parser
            .as_ref()
            .and_then(|p| p.msg_defs().choose(&mut rng))
        {
            let mut data = [0u8; 8];
            rng.fill_bytes(&mut data);
            Ok(vec![
                CanFrame::new(msg.id, &data[..msg.length_bytes as usize])
                    .unwrap()
                    .with_bus(self.parser.as_ref().unwrap().bus_id()),
            ])
        } else {
            let id =
                MessageId::from_parts(false, rng.random_range(0..=daqcore::can::STANDARD_ID_MASK))
                    .unwrap();
            let mut data = [0u8; 8];
            rng.fill_bytes(&mut data);
            Ok(vec![CanFrame::new(id, &data).unwrap()])
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

fn from_slcan(frame: slcan::CanFrame) -> Option<CanFrame> {
    match frame {
        slcan::CanFrame::Can2(f) => {
            let id = match f.id() {
                slcan::Id::Standard(id) => MessageId::from_parts(false, id.as_raw() as u32),
                slcan::Id::Extended(id) => MessageId::from_parts(true, id.as_raw()),
            }
            .ok()?;
            if f.is_remote() {
                CanFrame::remote(id, f.dlc() as u8).ok()
            } else {
                CanFrame::new(id, f.data().unwrap_or(&[])).ok()
            }
        }
        slcan::CanFrame::CanFd(_) => {
            log::warn!("CAN FD is unsupported; dropping frame");
            None
        }
    }
}
fn to_slcan(frame: CanFrame) -> DriverResult<slcan::CanFrame> {
    let id = if frame.id.is_extended() {
        slcan::Id::Extended(slcan::ExtendedId::new(frame.id.raw()).unwrap())
    } else {
        slcan::Id::Standard(slcan::StandardId::new(frame.id.raw() as u16).unwrap())
    };
    let converted = if frame.is_remote() {
        slcan::Can2Frame::new_remote(id, frame.len() as usize)
    } else {
        slcan::Can2Frame::new_data(id, frame.data())
    };
    converted
        .map(Into::into)
        .ok_or_else(|| DriverError::WriteError("Invalid classic CAN frame".into()))
}

pub fn parse_udp_buffer(
    buf: &[u8; UDP_MAX_PACKET_SIZE],
    num_bytes: usize,
) -> DriverResult<Vec<CanFrame>> {
    if num_bytes > buf.len()
        || num_bytes < UDP_RAW_FRAME_SIZE
        || !num_bytes.is_multiple_of(UDP_RAW_FRAME_SIZE)
    {
        return Err(DriverError::ReadError(DriverReadError::Other(
            "UDP packet must contain whole 16-byte records".into(),
        )));
    }
    buf[..num_bytes]
        .chunks_exact(UDP_RAW_FRAME_SIZE)
        .map(|chunk| {
            let identity = u32::from_le_bytes(chunk[4..8].try_into().unwrap());
            let data: &[u8; 8] = chunk[8..16].try_into().unwrap();
            CanFrame::from_log_identity(identity, data)
                .map_err(|e| DriverError::ReadError(DriverReadError::Other(e.to_string())))
        })
        .collect()
}

pub fn create_driver(
    source: &ConnectionSource,
    database: Option<BusDatabase>,
) -> DriverResult<Box<dyn Driver>> {
    match source {
        ConnectionSource::Serial(path, speed) => Ok(Box::new(SerialDriver::new(path, *speed)?)),
        ConnectionSource::Udp(port) => Ok(Box::new(UdpDriver::new(*port)?)),
        ConnectionSource::Simulated(connected, _) => {
            Ok(Box::new(SimulatedDriver::new(*connected, database)?))
        }
        ConnectionSource::Loopback => Ok(Box::new(LoopbackDriver::new())),
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    #[test]
    fn udp_keeps_bus_and_extended_ids_below_standard_limit() {
        let mut bytes = [0u8; UDP_MAX_PACKET_SIZE];
        bytes[4..8].copy_from_slice(&0xc0000001u32.to_le_bytes());
        bytes[20..24].copy_from_slice(&1u32.to_le_bytes());
        let frames = parse_udp_buffer(&bytes, 32).unwrap();
        assert!(frames[0].id.is_extended());
        assert_eq!(frames[0].id.raw(), 1);
        assert_eq!(frames[0].bus.unwrap().raw(), 1);
        assert!(!frames[1].id.is_extended());
        assert_eq!(frames[1].bus.unwrap().raw(), 0);
        assert!(parse_udp_buffer(&bytes, 31).is_err());
        assert!(parse_udp_buffer(&bytes, 2049).is_err());
        bytes[4..8].copy_from_slice(&0x20000001u32.to_le_bytes());
        assert!(parse_udp_buffer(&bytes, 16).is_err());
    }
    #[test]
    fn serial_adapter_preserves_short_dlc_remote_kind_and_id() {
        let id = MessageId::from_parts(true, 1).unwrap();
        let frame = CanFrame::new(id, &[1, 2, 3]).unwrap();
        assert_eq!(from_slcan(to_slcan(frame).unwrap()).unwrap(), frame);
        let remote = CanFrame::remote(id, 6).unwrap();
        assert_eq!(from_slcan(to_slcan(remote).unwrap()).unwrap(), remote);
    }
}
