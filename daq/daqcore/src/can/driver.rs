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
pub use fil::{FilExpectationEvent, FilExpectationStatus, FilGpioDirection, FilGpioEvent};
pub trait CanDriver {
    /// Whether the CAN worker should add a retry delay after an empty/timeout read.
    /// Drivers with their own bounded receive wait (notably FIL) return false.
    fn needs_read_retry_sleep(&self) -> bool;

    fn read_frames(&mut self) -> DriverResult<Vec<frame::CanFrame>>;
    fn write_frame(&mut self, frame: frame::CanFrame) -> DriverResult<()>;
    fn bus_speed(&self) -> Option<connection::CanBusSpeed> {
        None
    }

    fn close(&mut self) -> DriverResult<()> {
        Ok(())
    }
}

pub enum ActiveDriver {
    Can(Box<dyn CanDriver>),
    Fil(fil::FilDriver),
}

impl ActiveDriver {
    pub fn read_frames(&mut self) -> DriverResult<Vec<CanFrame>> {
        match self {
            Self::Can(driver) => driver.read_frames(),
            Self::Fil(driver) => driver.read_frames(),
        }
    }
    pub fn write_frame(&mut self, frame: CanFrame) -> DriverResult<()> {
        match self {
            Self::Can(driver) => driver.write_frame(frame),
            Self::Fil(driver) => driver.write_frame(frame),
        }
    }
    pub fn bus_speed(&self) -> Option<CanBusSpeed> {
        match self {
            Self::Can(driver) => driver.bus_speed(),
            Self::Fil(driver) => driver.bus_speed(),
        }
    }
    pub fn needs_read_retry_sleep(&self) -> bool {
        match self {
            Self::Can(driver) => driver.needs_read_retry_sleep(),
            Self::Fil(driver) => driver.needs_read_retry_sleep(),
        }
    }
    pub fn close(&mut self) -> DriverResult<()> {
        match self {
            Self::Can(driver) => driver.close(),
            Self::Fil(driver) => driver.close(),
        }
    }
    pub fn is_fil(&self) -> bool {
        matches!(self, Self::Fil(_))
    }
    pub fn set_fil_trace_bus(&mut self, trace_bus: Option<String>) -> DriverResult<()> {
        match self {
            Self::Fil(driver) => driver.set_trace_bus(trace_bus),
            Self::Can(_) => Ok(()),
        }
    }
    pub fn take_fil_gpio_events(&mut self) -> Vec<FilGpioEvent> {
        match self {
            Self::Fil(driver) => driver.take_gpio_events(),
            Self::Can(_) => Vec::new(),
        }
    }
    pub fn take_fil_expectation_events(&mut self) -> Vec<FilExpectationEvent> {
        match self {
            Self::Fil(driver) => driver.take_expectation_events(),
            Self::Can(_) => Vec::new(),
        }
    }
    pub fn take_all_fil_expectation_events(&mut self) -> Vec<FilExpectationEvent> {
        match self {
            Self::Fil(driver) => driver.take_all_expectation_events(),
            Self::Can(_) => Vec::new(),
        }
    }
    pub fn set_gpio(
        &mut self,
        board: &str,
        port: &str,
        pin: u8,
        value: Option<bool>,
    ) -> DriverResult<()> {
        match self {
            Self::Fil(driver) => driver.set_gpio(board, port, pin, value),
            Self::Can(_) => Err(DriverError::Unsupported(
                "GPIO control is not supported by this source".into(),
            )),
        }
    }
    pub fn set_adc(
        &mut self,
        board: &str,
        instance: &str,
        channel: u8,
        value: u16,
    ) -> DriverResult<()> {
        match self {
            Self::Fil(driver) => driver.set_adc(board, instance, channel, value),
            Self::Can(_) => Err(DriverError::Unsupported(
                "ADC injection is not supported by this source".into(),
            )),
        }
    }
}

pub fn create_driver(
    source: &connection::ConnectionSource,
    bus: Option<&superdbc::database::Bus>,
) -> DriverResult<ActiveDriver> {
    match source {
        connection::ConnectionSource::Serial(path, speed) => Ok(ActiveDriver::Can(Box::new(
            serial::SerialDriver::new(path, *speed)?,
        ))),
        connection::ConnectionSource::Udp(port) => {
            let socket = net::UdpSocket::bind(("0.0.0.0", *port))
                .map_err(|e| DriverError::ConnectionFailed(e.to_string()))?;
            socket
                .set_read_timeout(Some(time::Duration::from_millis(10)))
                .map_err(|e| DriverError::ConnectionFailed(e.to_string()))?;
            Ok(ActiveDriver::Can(Box::new(UdpDriver(socket))))
        }
        connection::ConnectionSource::Loopback => Ok(ActiveDriver::Can(Box::new(LoopbackDriver::default()))),
        connection::ConnectionSource::Fil {
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
                crate::fil::config::build_network(spec, executable).map(|(path, _)| path)
            } else {
                let disabled = disabled_boards
                    .iter()
                    .cloned()
                    .collect::<std::collections::HashSet<_>>();
                crate::fil::config::materialize_network(network, elf_overrides, &disabled)
            }
            .map_err(|error| {
                DriverError::ConnectionFailed(format!("Invalid FIL network: {error}"))
            })?;
            Ok(ActiveDriver::Fil(fil::FilDriver::new(
                executable,
                &effective_network,
                bus,
                run_options,
                trace_bus.clone(),
            )?))
        }
        connection::ConnectionSource::Simulated(true) => {
            let messages = bus
                .map(|bus| {
                    bus.messages()
                        .iter()
                        .map(|message| (message.identity(), usize::from(message.length_bytes())))
                        .collect()
                })
                .unwrap_or_default();
            Ok(ActiveDriver::Can(Box::new(SimulatedDriver {
                messages,
                next: time::Instant::now(),
            })))
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

mod fil;

impl CanDriver for LoopbackDriver {
    fn needs_read_retry_sleep(&self) -> bool {
        true
    }
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

impl CanDriver for UdpDriver {
    fn needs_read_retry_sleep(&self) -> bool {
        true
    }
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
            let identity_bytes = [bytes[4], bytes[5], bytes[6], bytes[7]];
            let identity = u32::from_le_bytes(identity_bytes);
            let extended = identity & log_parse::consts::IS_EID_MASK != 0;
            let transport_flags = log_parse::consts::IS_EID_MASK | log_parse::consts::BUS_ID_MASK;
            let id = identity & !transport_flags;
            let identity = frame::CanIdentity::new(id, extended)
                .map_err(|error| DriverError::Read(error.to_string()))?;
            frame::CanFrame::data(identity, bytes[8..16].to_vec()).map_err(DriverError::Read)
        })
        .collect()
}

struct SimulatedDriver {
    messages: Vec<(frame::CanIdentity, usize)>,
    next: time::Instant,
}

impl CanDriver for SimulatedDriver {
    fn needs_read_retry_sleep(&self) -> bool {
        true
    }
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
                let identity =
                    frame::CanIdentity::new(rng.random_range(0..=can::STANDARD_ID_MASK), false)
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

    impl can::driver::CanDriver for SerialDriver {
        fn needs_read_retry_sleep(&self) -> bool {
            true
        }
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
