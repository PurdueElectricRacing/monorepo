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
    fn write_frame(&mut self, tx_frame: frame::CanFrameTx) -> DriverResult<()>;

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
            let messages = bus.map(|bus| bus.messages().to_vec()).unwrap_or_default();
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

    fn write_frame(&mut self, tx_frame: frame::CanFrameTx) -> DriverResult<()> {
        self.queued.push(tx_frame.frame);
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

    fn write_frame(&mut self, _: frame::CanFrameTx) -> DriverResult<()> {
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
            frame::CanFrame::data(raw.can_identity(), raw.data.to_vec()).map_err(DriverError::Read)
        })
        .collect()
}

struct SimulatedDriver {
    messages: Vec<superdbc::message::Message>,
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

        let messages = &self.messages;

        let Some(message) = messages.choose(&mut rng) else {
            let identity = rng.random_range(0..=can::STANDARD_ID_MASK);
            let mut data = vec![0u8; 8];
            rng.fill_bytes(&mut data);
            let frame = frame::CanFrame::data(identity, data).map_err(DriverError::Read)?;
            return Ok(vec![frame]);
        };

        Self::message_frame(message, &mut rng).map(|frame| vec![frame])
    }

    fn write_frame(&mut self, _: frame::CanFrameTx) -> DriverResult<()> {
        Ok(())
    }
}

impl SimulatedDriver {
    fn message_frame(
        message: &superdbc::message::Message,
        rng: &mut impl rand::Rng,
    ) -> DriverResult<frame::CanFrame> {
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
                let bits = Self::signal_bits(signal, rng);

                signal.codec.insert(&mut data, bits);
            }
        }

        let frame = frame::CanFrame::data(identity, data).map_err(DriverError::Read)?;

        Ok(frame)
    }

    fn signal_bits(signal: &superdbc::message::SignalDefinition, rng: &mut impl rand::Rng) -> u64 {
        use rand::seq::IteratorRandom as _;
        if let Some((raw, _)) = signal.choices().choose(rng) {
            return raw as u64;
        }
        match signal.raw_type() {
            superdbc::message::RawType::Float32 => Self::float_bits(signal, rng),
            _ => Self::integer_bits(signal, rng),
        }
    }

    fn integer_bits(signal: &superdbc::message::SignalDefinition, rng: &mut impl rand::Rng) -> u64 {
        use rand::RngExt as _;
        let (mut lower, mut upper) = (signal.codec.min, signal.codec.max);
        if let Some(limits) = signal.limits() {
            let r1 = (limits.min - signal.offset()) / signal.scale();
            let r2 = (limits.max - signal.offset()) / signal.scale();
            let low = r1.min(r2).ceil();
            let high = r1.max(r2).floor();
            // Limits are presentation metadata and may contain no wire value.
            // In that case retain the full representable range.
            if low <= high && low <= upper as f64 && high >= lower as f64 {
                let candidate_lower = lower.max(low as i128);
                let candidate_upper = upper.min(high as i128);
                if candidate_lower <= candidate_upper {
                    lower = candidate_lower;
                    upper = candidate_upper;
                }
            }
        }
        rng.random_range(lower..=upper) as u64
    }

    fn float_bits(signal: &superdbc::message::SignalDefinition, rng: &mut impl rand::Rng) -> u64 {
        use rand::RngExt as _;
        let (min, max) = signal
            .limits()
            .map(|limits| (limits.min, limits.max))
            .unwrap_or((-100.0, 100.0));
        let r1 = (min - signal.offset()) / signal.scale();
        let r2 = (max - signal.offset()) / signal.scale();
        let low = r1.min(r2).max(-f64::from(f32::MAX));
        let high = r1.max(r2).min(f64::from(f32::MAX));
        let mut lower = low as f32;
        let mut upper = high as f32;
        if f64::from(lower) < low {
            lower = lower.next_up();
        }
        if f64::from(upper) > high {
            upper = upper.next_down();
        }
        if !lower.is_finite() || !upper.is_finite() || lower > upper {
            lower = -100.0;
            upper = 100.0;
        }
        // f64 sampling avoids overflow of the width of a finite f32 range.
        let raw = rng.random_range(f64::from(lower)..=f64::from(upper)) as f32;
        u64::from(raw.to_bits())
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

    fn to_wire_id(raw_id: u32, is_extended_id: bool) -> Option<slcan::Id> {
        if is_extended_id {
            slcan::ExtendedId::new(raw_id).map(slcan::Id::Extended)
        } else {
            u16::try_from(raw_id)
                .ok()
                .and_then(slcan::StandardId::new)
                .map(slcan::Id::Standard)
        }
    }

    fn identity(id: slcan::Id) -> frame::CanIdentity {
        match id {
            slcan::Id::Standard(id) => id.as_raw() as u32,
            slcan::Id::Extended(id) => id.as_raw(),
        }
    }

    fn from_wire(frame: slcan::CanFrame) -> can::driver::DriverResult<frame::CanFrame> {
        let frame = match frame {
            slcan::CanFrame::Can2(f) => {
                let identity = identity(f.id());
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
                let identity = identity(f.id());
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

        fn write_frame(&mut self, tx_frame: frame::CanFrameTx) -> can::driver::DriverResult<()> {
            let id = to_wire_id(tx_frame.frame.identity, tx_frame.is_extended_id)
                .ok_or_else(|| can::driver::DriverError::Write("invalid ID".into()))?;
            let f = tx_frame.frame;
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
