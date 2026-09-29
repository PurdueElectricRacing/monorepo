use crate::can::driver::can::{LoopbackDriver, SimulatedDriver};
use crate::can::driver::{FilDriver, SerialDriver, UdpDriver};
use crate::connection::{CanBusSpeed, ConnectionSource};
use slcan::CanFrame;

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

pub trait CanDriver {
    fn read_frames(&mut self) -> DriverResult<Vec<CanFrame>>;

    fn write_frame(&mut self, frame: CanFrame) -> DriverResult<()>;

    fn is_connected(&self) -> bool;

    fn bus_speed(&self) -> Option<CanBusSpeed>;

    fn close(&mut self) -> DriverResult<()>;

    /// Whether the CAN thread should sleep after a read timeout.
    ///
    /// FIL overrides this because it already performs its own bounded receive wait.
    fn needs_read_retry_sleep(&self) -> bool {
        true
    }
}

pub enum ActiveDriver {
    Can(Box<dyn CanDriver>),
    Fil(FilDriver),
}

impl ActiveDriver {
    pub fn as_mut(&mut self) -> &mut dyn CanDriver {
        match self {
            Self::Can(driver) => driver.as_mut(),
            Self::Fil(driver) => driver,
        }
    }

    pub fn fil_mut(&mut self) -> Option<&mut FilDriver> {
        match self {
            Self::Fil(driver) => Some(driver),
            Self::Can(_) => None,
        }
    }
}

impl std::ops::Deref for ActiveDriver {
    type Target = dyn CanDriver;

    fn deref(&self) -> &Self::Target {
        match self {
            Self::Can(driver) => driver.as_ref(),
            Self::Fil(driver) => driver,
        }
    }
}

impl std::ops::DerefMut for ActiveDriver {
    fn deref_mut(&mut self) -> &mut Self::Target {
        match self {
            Self::Can(driver) => driver.as_mut(),
            Self::Fil(driver) => driver,
        }
    }
}

pub fn create_driver(source: &ConnectionSource) -> DriverResult<ActiveDriver> {
    match source {
        ConnectionSource::Serial(path, speed) => Ok(ActiveDriver::Can(Box::new(
            SerialDriver::new(path, *speed)?,
        ))),
        ConnectionSource::Udp(port) => Ok(ActiveDriver::Can(Box::new(UdpDriver::new(*port)?))),
        ConnectionSource::Simulated(connected, dbc_path) => Ok(ActiveDriver::Can(Box::new(
            SimulatedDriver::new(*connected, dbc_path.clone())?,
        ))),
        ConnectionSource::Fil {
            executable,
            network,
            bus,
            trace_bus,
            elf_overrides,
            disabled_boards,
            built_network,
            run_options,
        } => Ok(ActiveDriver::Fil(FilDriver::new(
            executable,
            network,
            bus,
            trace_bus,
            elf_overrides,
            disabled_boards,
            built_network,
            run_options,
        )?)),
        ConnectionSource::Loopback => Ok(ActiveDriver::Can(Box::new(LoopbackDriver::new()))),
    }
}
