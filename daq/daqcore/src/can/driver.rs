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

    fn update_bus(&mut self, _bus: Option<&superdbc::database::Bus>) {}

    fn close(&mut self) -> DriverResult<()> {
        Ok(())
    }
}

pub fn create_driver(
    source: &connection::ConnectionSource,
    bus: Option<&superdbc::database::Bus>,
) -> DriverResult<Box<dyn Driver>> {
    match source {
        connection::ConnectionSource::Serial(path, speed) => {
            Ok(Box::new(serial::SerialDriver::new(path, *speed)?))
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
                .map(|bus| bus.messages().to_vec())
                .unwrap_or_default();
            Ok(Box::new(SimulatedDriver {
                messages,
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

struct SimulatedDriver {
    messages: Vec<superdbc::message::Message>,
    next: time::Instant,
}

impl Driver for SimulatedDriver {
    fn read_frames(&mut self) -> DriverResult<Vec<frame::CanFrame>> {
        use rand::prelude::IndexedRandom as _;
        use rand::Rng as _;
        use rand::RngExt as _;
        let now = time::Instant::now();
        if now < self.next {
            return Err(DriverError::Timeout);
        }
        self.next += time::Duration::from_millis(1);
        let mut rng = rand::rng();

        if self.messages.is_empty() {
            let identity =
                frame::CanIdentity::new(rng.random_range(0..=can::STANDARD_ID_MASK), false)
                    .map_err(|error| DriverError::Read(error.to_string()))?;
            let mut data = vec![0u8; 8];
            rng.fill_bytes(&mut data);
            let frame = frame::CanFrame::data(identity, data).map_err(DriverError::Read)?;
            return Ok(vec![frame]);
        }

        let message = self.messages.choose(&mut rng).unwrap();
        let identity = message.identity();
        let size = usize::from(message.length_bytes());

        if size > 8 {
            return Err(DriverError::Timeout);
        }

        let mut data = vec![0u8; size];
        if message.signals.is_empty() {
            rng.fill_bytes(&mut data);
        } else {
            for signal in &message.signals {
                let unit = signal.unit().trim();
                let is_percentage = unit == "%"
                    || unit.eq_ignore_ascii_case("percent")
                    || unit.eq_ignore_ascii_case("percentage");

                let bits: u64 = if is_percentage {
                    let (min_phys, max_phys) = if let Some(limits) = signal.limits() {
                        let min_p = limits.min.clamp(0.0, 100.0);
                        let max_p = limits.max.clamp(0.0, 100.0);
                        if min_p <= max_p {
                            (min_p, max_p)
                        } else {
                            (0.0, 100.0)
                        }
                    } else {
                        (0.0, 100.0)
                    };

                    match signal.raw_type() {
                        superdbc::message::RawType::Float32 => {
                            let phys = rng.random_range(min_phys as f32..=max_phys as f32);
                            let raw = (phys - signal.offset() as f32) / signal.scale() as f32;
                            u64::from(raw.to_bits())
                        }
                        _ => {
                            let scale = signal.scale();
                            let offset = signal.offset();
                            let r1 = (min_phys - offset) / scale;
                            let r2 = (max_phys - offset) / scale;
                            let lower = (r1.min(r2).ceil() as i128)
                                .clamp(signal.codec.min, signal.codec.max);
                            let upper = (r1.max(r2).floor() as i128)
                                .clamp(signal.codec.min, signal.codec.max);
                            let raw = if lower <= upper {
                                rng.random_range(lower..=upper)
                            } else {
                                (r1.round() as i128).clamp(signal.codec.min, signal.codec.max)
                            };
                            raw as u64
                        }
                    }
                } else {
                    let choices: Vec<i128> = signal.choices().map(|(val, _)| val).collect();
                    if !choices.is_empty() {
                        let &raw = choices.choose(&mut rng).unwrap();
                        raw as u64
                    } else {
                        match signal.raw_type() {
                            superdbc::message::RawType::Float32 => {
                                let (min_p, max_p) = if let Some(limits) = signal.limits() {
                                    (limits.min as f32, limits.max as f32)
                                } else {
                                    (-100.0f32, 100.0f32)
                                };
                                let (min_p, max_p) = if min_p <= max_p {
                                    (min_p, max_p)
                                } else {
                                    (max_p, min_p)
                                };
                                let phys = rng.random_range(min_p..=max_p);
                                let raw = (phys - signal.offset() as f32) / signal.scale() as f32;
                                u64::from(raw.to_bits())
                            }
                            _ => {
                                let (lower, upper) = if let Some(limits) = signal.limits() {
                                    let scale = signal.scale();
                                    let offset = signal.offset();
                                    let r1 = (limits.min - offset) / scale;
                                    let r2 = (limits.max - offset) / scale;
                                    let lower = (r1.min(r2).ceil() as i128)
                                        .clamp(signal.codec.min, signal.codec.max);
                                    let upper = (r1.max(r2).floor() as i128)
                                        .clamp(signal.codec.min, signal.codec.max);
                                    if lower <= upper {
                                        (lower, upper)
                                    } else {
                                        (signal.codec.min, signal.codec.max)
                                    }
                                } else {
                                    (signal.codec.min, signal.codec.max)
                                };
                                let raw = rng.random_range(lower..=upper);
                                raw as u64
                            }
                        }
                    }
                };

                signal.codec.insert(&mut data, bits);
            }
        }

        let frame = frame::CanFrame::data(identity, data).map_err(DriverError::Read)?;

        Ok(vec![frame])
    }

    fn write_frame(&mut self, _: frame::CanFrame) -> DriverResult<()> {
        Ok(())
    }

    fn update_bus(&mut self, bus: Option<&superdbc::database::Bus>) {
        self.messages = bus.map(|b| b.messages().to_vec()).unwrap_or_default();
    }
}
mod serial {
    use std::time;

    use crate::{can, connection, frame};

    pub struct SerialDriver {
        socket: slcan::sync::CanSocket<Box<dyn serialport::SerialPort>>,
        speed: connection::CanBusSpeed,
    }

    impl SerialDriver {
        pub fn new(path: &str, speed: connection::CanBusSpeed) -> can::driver::DriverResult<Self> {
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
            Ok(Self { socket, speed })
        }
    }

    fn identity(id: slcan::Id) -> can::driver::DriverResult<frame::CanIdentity> {
        let identity = match id {
            slcan::Id::Standard(id) => frame::CanIdentity::new(id.as_raw() as u32, false),
            slcan::Id::Extended(id) => frame::CanIdentity::new(id.as_raw(), true),
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
            let id = if f.identity.is_extended() {
                slcan::ExtendedId::new(f.identity.raw_id()).map(slcan::Id::Extended)
            } else {
                u16::try_from(f.identity.raw_id())
                    .ok()
                    .and_then(slcan::StandardId::new)
                    .map(slcan::Id::Standard)
            }
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
}

#[cfg(test)]
mod tests {
    use super::*;
    use std::path;

    #[test]
    fn test_simulated_driver_percentage_range() {
        let path = path::Path::new(env!("CARGO_MANIFEST_DIR")).join("../../superdbc/superdbc_ff4fade.json");
        let database = superdbc::database::Database::load(&path).expect("failed to load database");

        for bus in database.buses() {
            let mut driver = SimulatedDriver {
                messages: bus.messages().to_vec(),
                next: time::Instant::now(),
            };

            let mut checked_percentage_count = 0;

            for _ in 0..1000 {
                // Ensure next time does not cause timeout
                driver.next = time::Instant::now() - time::Duration::from_millis(10);
                let frames = driver.read_frames().expect("read_frames failed");
                for frame in frames {
                    if let Ok(decoded) = bus.decode(frame.identity, &frame.data) {
                        let msg_def = bus.message(frame.identity).expect("message definition missing");
                        for (name, value) in decoded.iter() {
                            let sig_def = msg_def.signal(name).expect("signal definition missing");
                            let unit = sig_def.unit().trim();
                            if unit == "%" || unit.eq_ignore_ascii_case("percent") || unit.eq_ignore_ascii_case("percentage") {
                                let phys = value.physical();
                                assert!(
                                    phys >= 0.0 && phys <= 100.0,
                                    "Signal '{}' percentage value {} is out of 0..=100 range",
                                    name,
                                    phys
                                );
                                checked_percentage_count += 1;
                            }
                        }
                    }
                }
            }

            if bus.messages().iter().any(|m| m.signals.iter().any(|s| s.unit() == "%")) {
                assert!(checked_percentage_count > 0, "Expected to check at least one percentage signal on bus {}", bus.name());
            }
        }
    }

    #[test]
    fn test_simulated_driver_update_bus() {
        let path = path::Path::new(env!("CARGO_MANIFEST_DIR")).join("../../superdbc/superdbc_ff4fade.json");
        let database = superdbc::database::Database::load(&path).expect("failed to load database");

        let mut driver = SimulatedDriver {
            messages: Vec::new(),
            next: time::Instant::now() - time::Duration::from_millis(10),
        };

        // When empty, read_frames returns random fallback frame
        let frames = driver.read_frames().expect("read_frames failed");
        assert_eq!(frames.len(), 1);

        // Update with first bus
        let bus0 = database.buses().first().expect("no bus 0");
        driver.update_bus(Some(bus0));
        assert_eq!(driver.messages.len(), bus0.messages().len());

        driver.next = time::Instant::now() - time::Duration::from_millis(10);
        let frames = driver.read_frames().expect("read_frames failed");
        assert_eq!(frames.len(), 1);
        assert!(bus0.message(frames[0].identity).is_some());

        // Update with None
        driver.update_bus(None);
        assert!(driver.messages.is_empty());
    }
}
