//! Drivers are single-owner I/O adapters; no transport type escapes this module.

use crate::{can, connection, frame, log_parse, superdbc};
use std::{error, fmt, io, mem, net, time};

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
pub trait Driver {
    fn read_frames(&mut self) -> DriverResult<Vec<frame::CanFrame>>;
    fn write_frame(&mut self, frame: frame::CanFrame) -> DriverResult<()>;
    fn bus_speed(&self) -> Option<connection::CanBusSpeed> {
        None
    }

    fn close(&mut self) -> DriverResult<()> {
        Ok(())
    }
}

pub fn create_driver(
    source: &connection::ConnectionSource,
    bus: Option<&superdbc::database::Bus>,
) -> DriverResult<Box<dyn Driver>> {
    let extended = bus.and_then(|bus| bus.is_extended_id());
    match source {
        connection::ConnectionSource::Serial(path, speed) => {
            Ok(Box::new(serial::SerialDriver::new(path, *speed, extended)?))
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
        connection::ConnectionSource::Simulated(true) => {
            let messages = bus
                .map(|bus| {
                    bus.messages()
                        .iter()
                        .map(|message| (message.identity(), usize::from(message.length_bytes())))
                        .collect()
                })
                .unwrap_or_default();
            Ok(Box::new(SimulatedDriver {
                messages,
                id_range: simulated_id_range(extended),
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

impl Driver for LoopbackDriver {
    fn read_frames(&mut self) -> DriverResult<Vec<frame::CanFrame>> {
        if self.queued.is_empty() {
            Err(DriverError::Timeout)
        } else {
            Ok(mem::take(&mut self.queued))
        }
    }

    fn write_frame(&mut self, frame: frame::CanFrame) -> DriverResult<()> {
        self.queued.push(frame);
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

    fn write_frame(&mut self, _: frame::CanFrame) -> DriverResult<()> {
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
            let identity = raw
                .can_identity()
                .map_err(|error| DriverError::Read(error.to_string()))?;
            frame::CanFrame::data(identity, raw.data.to_vec()).map_err(DriverError::Read)
        })
        .collect()
}

/// Choose the random ID range for a simulated bus without message definitions.
///
/// A bus-level extended framing draws from the full 29-bit range; standard
/// framing, or a schema 1.1 artifact without the field, stays within 11-bit IDs.
fn simulated_id_range(extended: Option<bool>) -> std::ops::RangeInclusive<u32> {
    match extended {
        Some(true) => 0..=can::EXTENDED_ID_MASK,
        _ => 0..=can::STANDARD_ID_MASK,
    }
}

struct SimulatedDriver {
    messages: Vec<(frame::CanIdentity, usize)>,
    id_range: std::ops::RangeInclusive<u32>,
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
        let (identity, size) = match self.messages.choose(&mut rng).copied() {
            Some(message) => message,
            None => {
                let identity = frame::CanIdentity::new(rng.random_range(self.id_range.clone()))
                    .map_err(|error| DriverError::Read(error.to_string()))?;
                (identity, 8)
            }
        };

        if size > 8 {
            return Err(DriverError::Timeout);
        }

        let mut data = vec![0; size];
        rng.fill_bytes(&mut data);
        let frame = frame::CanFrame::data(identity, data).map_err(DriverError::Read)?;

        Ok(vec![frame])
    }

    fn write_frame(&mut self, _: frame::CanFrame) -> DriverResult<()> {
        Ok(())
    }
}
mod serial {
    use std::time;

    use crate::{can, connection, frame};

    pub struct SerialDriver {
        socket: slcan::sync::CanSocket<Box<dyn serialport::SerialPort>>,
        speed: connection::CanBusSpeed,
        extended: Option<bool>,
    }

    impl SerialDriver {
        pub fn new(
            path: &str,
            speed: connection::CanBusSpeed,
            extended: Option<bool>,
        ) -> can::driver::DriverResult<Self> {
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
            Ok(Self {
                socket,
                speed,
                extended,
            })
        }
    }

    /// Map a 29-bit numeric CAN identity onto the bus wire framing.
    ///
    /// `Some(true)` sends every frame with an extended (29-bit) ID;
    /// `Some(false)` requires the ID to fit in 11 standard bits. `None`
    /// preserves the legacy 1.1 artifact behavior: the raw width alone
    /// decides, and wide IDs fall back to extended framing.
    fn to_wire_id(raw_id: u32, extended: Option<bool>) -> Option<slcan::Id> {
        match extended {
            Some(true) => slcan::ExtendedId::new(raw_id).map(slcan::Id::Extended),
            Some(false) => u16::try_from(raw_id)
                .ok()
                .and_then(slcan::StandardId::new)
                .map(slcan::Id::Standard),
            None => {
                if raw_id > can::STANDARD_ID_MASK {
                    slcan::ExtendedId::new(raw_id).map(slcan::Id::Extended)
                } else {
                    u16::try_from(raw_id)
                        .ok()
                        .and_then(slcan::StandardId::new)
                        .map(slcan::Id::Standard)
                }
            }
        }
    }

    fn identity(id: slcan::Id) -> can::driver::DriverResult<frame::CanIdentity> {
        let identity = match id {
            slcan::Id::Standard(id) => frame::CanIdentity::new(id.as_raw() as u32),
            slcan::Id::Extended(id) => frame::CanIdentity::new(id.as_raw()),
        };

        identity.map_err(|error| can::driver::DriverError::Read(error.to_string()))
    }

    fn from_wire(frame: slcan::CanFrame) -> can::driver::DriverResult<frame::CanFrame> {
        let frame = match frame {
            slcan::CanFrame::Can2(f) => {
                let identity = identity(f.id())?;
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
                let identity = identity(f.id())?;
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

        fn write_frame(&mut self, f: frame::CanFrame) -> can::driver::DriverResult<()> {
            let id = to_wire_id(f.identity.raw_id(), self.extended)
                .ok_or_else(|| can::driver::DriverError::Write("invalid ID".into()))?;
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

    #[cfg(test)]
    mod tests {
        use super::to_wire_id;

        #[test]
        fn bus_level_extended_frames_use_extended_ids() {
            assert!(matches!(
                to_wire_id(0x123, Some(true)),
                Some(slcan::Id::Extended(_))
            ));
            assert!(matches!(
                to_wire_id(0x12345678, Some(true)),
                Some(slcan::Id::Extended(_))
            ));
        }

        #[test]
        fn bus_level_standard_frames_require_11_bit_ids() {
            assert!(matches!(
                to_wire_id(0x123, Some(false)),
                Some(slcan::Id::Standard(_))
            ));
            assert_eq!(to_wire_id(0x800, Some(false)), None);
        }

        #[test]
        fn legacy_width_heuristic_applies_without_bus_level_framing() {
            assert!(matches!(
                to_wire_id(0x123, None),
                Some(slcan::Id::Standard(_))
            ));
            assert!(matches!(
                to_wire_id(0x12345678, None),
                Some(slcan::Id::Extended(_))
            ));
        }
    }
}

#[cfg(test)]
mod tests {
    use super::simulated_id_range;
    use crate::can;

    #[test]
    fn simulated_ids_follow_bus_level_framing() {
        assert_eq!(simulated_id_range(Some(true)), 0..=can::EXTENDED_ID_MASK);
        assert_eq!(simulated_id_range(Some(false)), 0..=can::STANDARD_ID_MASK);
        assert_eq!(simulated_id_range(None), 0..=can::STANDARD_ID_MASK);
    }
}
