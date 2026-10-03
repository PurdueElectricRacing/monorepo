//! Drivers are single-owner I/O adapters; no transport type escapes this module.
use crate::{
    can,
    connection::{CanBusSpeed, ConnectionSource},
    frame::CanFrame,
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
pub trait Driver {
    fn read_frames(&mut self) -> DriverResult<Vec<CanFrame>>;
    fn write_frame(&mut self, frame: CanFrame) -> DriverResult<()>;
    fn bus_speed(&self) -> Option<CanBusSpeed> {
        None
    }
    fn close(&mut self) -> DriverResult<()> {
        Ok(())
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
        ConnectionSource::Simulated(true, path) => Ok(Box::new(SimulatedDriver {
            parser: path
                .as_ref()
                .and_then(|p| can_decode::Parser::from_dbc_file(p).ok()),
            next: Instant::now(),
        })),
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
    buf.chunks_exact(16)
        .map(|bytes| {
            let identity = u32::from_le_bytes(bytes[4..8].try_into().unwrap());
            let extended = identity & log_parse::consts::IS_EID_MASK != 0;
            let id = identity & can::EXTENDED_ID_MASK;
            CanFrame::data(id, extended, bytes[8..16].to_vec()).map_err(DriverError::Read)
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
        self.next += Duration::from_millis(5);
        let mut rng = rand::rng();
        let msg = self
            .parser
            .as_ref()
            .and_then(|p| p.msg_defs().choose(&mut rng).cloned());
        let (id, extended, size) = match msg {
            Some(m) => (
                can::can_dbc_to_u32_without_extid_flag(&m.id),
                matches!(m.id, can_dbc::MessageId::Extended(_)),
                m.size as usize,
            ),
            None => (rng.random_range(0..=can::STANDARD_ID_MASK), false, 8),
        };
        if size > 8 {
            return Err(DriverError::Timeout);
        }
        let mut data = vec![0; size];
        rng.fill_bytes(&mut data);
        Ok(vec![
            CanFrame::data(id, extended, data).map_err(DriverError::Read)?,
        ])
    }
    fn write_frame(&mut self, _: CanFrame) -> DriverResult<()> {
        Ok(())
    }
}
mod serial {
    use crate::{
        can::driver::{Driver, DriverError, DriverResult, io_read},
        connection::CanBusSpeed,
        frame::{CanFrame, FrameKind},
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
    fn identity(id: slcan::Id) -> (u32, bool) {
        match id {
            slcan::Id::Standard(id) => (id.as_raw() as u32, false),
            slcan::Id::Extended(id) => (id.as_raw(), true),
        }
    }
    fn from_wire(frame: slcan::CanFrame) -> CanFrame {
        match frame {
            slcan::CanFrame::Can2(f) => {
                let (msg_id, is_msg_id_extended) = identity(f.id());
                CanFrame {
                    msg_id,
                    is_msg_id_extended,
                    kind: if f.is_remote() {
                        FrameKind::Remote
                    } else {
                        FrameKind::Data
                    },
                    dlc: f.dlc() as u8,
                    data: f.data().unwrap_or(&[]).to_vec(),
                }
            }
            slcan::CanFrame::CanFd(f) => {
                let (msg_id, is_msg_id_extended) = identity(f.id());
                CanFrame {
                    msg_id,
                    is_msg_id_extended,
                    kind: FrameKind::Fd {
                        bit_rate_switched: f.is_bit_rate_switched(),
                    },
                    dlc: f.dlc() as u8,
                    data: f.data().to_vec(),
                }
            }
        }
    }
    impl Driver for SerialDriver {
        fn read_frames(&mut self) -> DriverResult<Vec<CanFrame>> {
            self.socket
                .read()
                .map(|f| vec![from_wire(f)])
                .map_err(|e| match e {
                    slcan::ReadError::Io(e) => io_read(e),
                    other => DriverError::Read(format!("{other:?}")),
                })
        }
        fn write_frame(&mut self, f: CanFrame) -> DriverResult<()> {
            let id = if f.is_msg_id_extended {
                slcan::ExtendedId::new(f.msg_id).map(slcan::Id::Extended)
            } else {
                u16::try_from(f.msg_id)
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
