//! Drivers are single-owner I/O adapters; no transport type escapes this module.

pub mod cannector;
pub mod loopback;
pub mod serial;
pub mod simulated;
pub mod udp;

use crate::{connection, frame, log_parse, superdbc};
use std::{error, fmt, io, mem, time};

pub const READ_TIMEOUT: time::Duration = time::Duration::from_millis(10);
pub const WRITE_TIMEOUT: time::Duration = time::Duration::from_millis(10);

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
    buses: &[&superdbc::database::Bus],
) -> DriverResult<Box<dyn Driver>> {
    match source {
        connection::ConnectionSource::Serial(path, speed) => {
            Ok(Box::new(serial::SerialDriver::new(path, *speed)?))
        }
        connection::ConnectionSource::Udp(port) => Ok(Box::new(udp::UdpDriver::new(*port)?)),
        connection::ConnectionSource::Cannector => Ok(Box::new(cannector::CannectorDriver::new()?)),
        connection::ConnectionSource::Loopback => Ok(Box::new(loopback::LoopbackDriver::default())),
        connection::ConnectionSource::Simulated(true) => {
            Ok(Box::new(simulated::SimulatedDriver::new(buses)))
        }
        _ => Err(DriverError::ConnectionFailed(format!(
            "source unavailable: {}",
            source.display_name()
        ))),
    }
}

pub fn io_read(error: io::Error) -> DriverError {
    if matches!(
        error.kind(),
        io::ErrorKind::WouldBlock | io::ErrorKind::TimedOut
    ) {
        return DriverError::Timeout;
    }
    DriverError::Read(error.to_string())
}

pub fn parse_daq_buffer(buf: &[u8]) -> DriverResult<Vec<frame::CanFrame>> {
    const TIMESTAMPED_FRAME_SIZE: usize = mem::size_of::<log_parse::parse::TimestampedFrame>();

    if buf.len() < TIMESTAMPED_FRAME_SIZE || !buf.len().is_multiple_of(TIMESTAMPED_FRAME_SIZE) {
        return Err(DriverError::Read(
            "DAQ packet must contain complete frame records".into(),
        ));
    }
    buf.as_chunks::<TIMESTAMPED_FRAME_SIZE>()
        .0
        .iter()
        .map(|bytes| {
            let raw = bytemuck::try_pod_read_unaligned::<log_parse::parse::TimestampedFrame>(bytes)
                .map_err(|error| DriverError::Read(error.to_string()))?;
            frame::CanFrame::data(Some(raw.bus_id()), raw.can_identity(), raw.data.to_vec())
                .map_err(DriverError::Read)
        })
        .collect()
}
