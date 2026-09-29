use crate::{can, connection, frame, superdbc};
use std::time;

pub struct ConnectionManager {
    source: Option<connection::ConnectionSource>,
    driver: Option<Box<dyn can::driver::Driver>>,
    retry_at: time::Instant,
}

impl ConnectionManager {
    pub fn new(now: time::Instant) -> Self {
        Self {
            source: None,
            driver: None,
            retry_at: now,
        }
    }

    pub fn select(&mut self, source: Option<connection::ConnectionSource>, now: time::Instant) {
        self.close();
        self.source = source;
        self.retry_at = now;
    }

    pub fn connect(
        &mut self,
        now: time::Instant,
        bus: Option<&superdbc::database::Bus>,
    ) -> Option<Result<(), String>> {
        if self.driver.is_some() || now < self.retry_at {
            return None;
        }

        let source = self.source.as_ref()?;
        self.retry_at = now + time::Duration::from_millis(200);

        let result = match can::driver::create_driver(source, bus) {
            Ok(driver) => {
                self.driver = Some(driver);
                Ok(())
            }
            Err(error) => Err(format!("{}: {error}", source.display_name())),
        };

        Some(result)
    }

    pub fn connected(&self) -> bool {
        self.driver.is_some()
    }

    pub fn speed(&self) -> connection::CanBusSpeed {
        self.driver
            .as_ref()
            .and_then(|d| d.bus_speed())
            .unwrap_or_default()
    }

    pub fn read(&mut self) -> can::driver::DriverResult<Vec<frame::CanFrame>> {
        let driver = self
            .driver
            .as_mut()
            .ok_or(can::driver::DriverError::Timeout)?;

        driver.read_frames()
    }

    pub fn needs_read_retry_sleep(&self) -> bool {
        self.driver
            .as_ref()
            .is_none_or(|driver| driver.needs_read_retry_sleep())
    }

    pub fn set_fil_trace_bus(&mut self, trace_bus: Option<String>) -> DriverResult<()> {
        self.driver
            .as_mut()
            .ok_or_else(|| DriverError::Write("disconnected".into()))?
            .set_fil_trace_bus(trace_bus)
    }

    pub fn take_fil_gpio_events(&mut self) -> Vec<crate::can::driver::FilGpioEvent> {
        self.driver
            .as_mut()
            .map(|d| d.take_fil_gpio_events())
            .unwrap_or_default()
    }

    pub fn set_gpio(
        &mut self,
        board: &str,
        port: &str,
        pin: u8,
        value: Option<bool>,
    ) -> DriverResult<()> {
        let driver = self
            .driver
            .as_mut()
            .ok_or_else(|| DriverError::Write("disconnected".into()))?;
        driver.set_gpio(board, port, pin, value)
    }

    pub fn set_adc(
        &mut self,
        board: &str,
        instance: &str,
        channel: u8,
        value: u16,
    ) -> can::driver::DriverResult<()> {
        let driver = self
            .driver
            .as_mut()
            .ok_or_else(|| can::driver::DriverError::Write("disconnected".into()))?;
        driver.set_adc(board, instance, channel, value)
    }

    pub fn write(&mut self, frame: frame::CanFrame) -> can::driver::DriverResult<()> {
        let driver = self
            .driver
            .as_mut()
            .ok_or_else(|| can::driver::DriverError::Write("disconnected".into()))?;

        driver.write_frame(frame)
    }

    pub fn failed(&mut self, now: time::Instant) {
        self.close();
        if matches!(self.source, Some(connection::ConnectionSource::Fil { .. })) {
            self.source = None;
        } else {
            self.retry_at = now + time::Duration::from_millis(200);
        }
    }

    pub fn close(&mut self) {
        if let Some(mut driver) = self.driver.take() {
            let _ = driver.close();
        }
    }
}

impl Drop for ConnectionManager {
    fn drop(&mut self) {
        self.close();
    }
}
