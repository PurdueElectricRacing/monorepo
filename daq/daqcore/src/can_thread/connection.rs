use crate::{
    can::driver::{self, Driver, DriverError, DriverResult},
    connection::{self, ConnectionSource},
    frame::CanFrame,
};

use std::time::{Duration, Instant};

pub struct ConnectionManager {
    source: Option<ConnectionSource>,
    driver: Option<Box<dyn Driver>>,
    retry_at: Instant,
}

impl ConnectionManager {
    pub fn new(now: Instant) -> Self {
        Self {
            source: None,
            driver: None,
            retry_at: now,
        }
    }

    pub fn select(&mut self, source: Option<ConnectionSource>, now: Instant) {
        self.close();
        self.source = source;
        self.retry_at = now;
    }

    pub fn connect(&mut self, now: Instant) -> Option<Result<(), String>> {
        if self.driver.is_some() || now < self.retry_at {
            return None;
        }

        let source = self.source.as_ref()?;
        self.retry_at = now + Duration::from_millis(200);

        let result = match driver::create_driver(source) {
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

    pub fn read(&mut self) -> DriverResult<Vec<CanFrame>> {
        let driver = self.driver.as_mut().ok_or(DriverError::Timeout)?;

        driver.read_frames()
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
    ) -> DriverResult<()> {
        let driver = self
            .driver
            .as_mut()
            .ok_or_else(|| DriverError::Write("disconnected".into()))?;
        driver.set_adc(board, instance, channel, value)
    }

    pub fn write(&mut self, frame: CanFrame) -> DriverResult<()> {
        let driver = self
            .driver
            .as_mut()
            .ok_or_else(|| DriverError::Write("disconnected".into()))?;

        driver.write_frame(frame)
    }

    pub fn failed(&mut self, now: Instant) {
        self.close();
        if matches!(self.source, Some(ConnectionSource::Fil { .. })) {
            self.source = None;
        } else {
            self.retry_at = now + Duration::from_millis(200);
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
