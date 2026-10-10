use crate::{
    can::driver::{
        self, ActiveDriver, DriverError, DriverResult, FilExpectationEvent, FilGpioEvent,
    },
    connection::{self, ConnectionSource},
    frame::CanFrame,
};

use std::time::{Duration, Instant};

pub struct ConnectionManager {
    source: Option<ConnectionSource>,
    driver: Option<ActiveDriver>,
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
        let _ = self.close();
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

    pub fn take_fil_gpio_events(&mut self) -> Vec<FilGpioEvent> {
        self.driver
            .as_mut()
            .map(|d| d.take_fil_gpio_events())
            .unwrap_or_default()
    }

    pub fn take_fil_expectation_events(&mut self) -> Vec<FilExpectationEvent> {
        self.driver
            .as_mut()
            .map(|d| d.take_fil_expectation_events())
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

    pub fn failed(&mut self, now: Instant) -> Vec<FilExpectationEvent> {
        let was_fil = self.driver.as_ref().is_some_and(ActiveDriver::is_fil);
        let expectations = self.close();
        if was_fil {
            self.source = None;
        } else {
            self.retry_at = now + Duration::from_millis(200);
        }
        expectations
    }

    pub fn close(&mut self) -> Vec<FilExpectationEvent> {
        if let Some(mut driver) = self.driver.take() {
            let _ = driver.close();
            driver.take_all_fil_expectation_events()
        } else {
            Vec::new()
        }
    }
}

impl Drop for ConnectionManager {
    fn drop(&mut self) {
        let _ = self.close();
    }
}
