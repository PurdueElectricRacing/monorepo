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
pub use fil::{FilExpectationEvent, FilExpectationStatus, FilGpioDirection, FilGpioEvent};
pub trait CanDriver {
    /// Whether the CAN worker should add a retry delay after an empty/timeout read.
    /// Drivers with their own bounded receive wait (notably FIL) return false.
    fn needs_read_retry_sleep(&self) -> bool;

    fn read_frames(&mut self) -> DriverResult<Vec<CanFrame>>;
    fn write_frame(&mut self, frame: CanFrame) -> DriverResult<()>;
    fn bus_speed(&self) -> Option<CanBusSpeed> {
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

pub fn create_driver(source: &ConnectionSource) -> DriverResult<ActiveDriver> {
    match source {
        ConnectionSource::Serial(path, speed) => Ok(ActiveDriver::Can(Box::new(
            serial::SerialDriver::new(path, *speed)?,
        ))),
        ConnectionSource::Udp(port) => {
            let socket = std::net::UdpSocket::bind(("0.0.0.0", *port))
                .map_err(|e| DriverError::ConnectionFailed(e.to_string()))?;
            socket
                .set_read_timeout(Some(Duration::from_millis(10)))
                .map_err(|e| DriverError::ConnectionFailed(e.to_string()))?;
            Ok(ActiveDriver::Can(Box::new(UdpDriver(socket))))
        }
        ConnectionSource::Loopback => Ok(ActiveDriver::Can(Box::new(LoopbackDriver::default()))),
        ConnectionSource::Fil {
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
        ConnectionSource::Simulated(true, path) => {
            let parser = path
                .as_ref()
                .map(|path| can_decode::Parser::from_dbc_file(path))
                .transpose()
                .map_err(|error| DriverError::ConnectionFailed(error.to_string()))?;

            Ok(ActiveDriver::Can(Box::new(SimulatedDriver {
                parser,
                next: Instant::now(),
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
    queued: Vec<CanFrame>,
}

mod fil;

impl CanDriver for LoopbackDriver {
    fn needs_read_retry_sleep(&self) -> bool {
        true
    }
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

impl CanDriver for UdpDriver {
    fn needs_read_retry_sleep(&self) -> bool {
        true
    }
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

impl CanDriver for SimulatedDriver {
    fn needs_read_retry_sleep(&self) -> bool {
        true
    }
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
        can::driver::{CanDriver, DriverError, DriverResult, io_read},
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

    impl CanDriver for SerialDriver {
        fn needs_read_retry_sleep(&self) -> bool {
            true
        }
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
