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
        Some(match driver::create_driver(source) {
            Ok(driver) => {
                self.driver = Some(driver);
                Ok(())
            }
            Err(e) => Err(format!("{}: {e}", source.display_name())),
        })
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
        self.driver
            .as_mut()
            .ok_or(DriverError::Timeout)?
            .read_frames()
    }
    pub fn write(&mut self, frame: CanFrame) -> DriverResult<()> {
        self.driver
            .as_mut()
            .ok_or_else(|| DriverError::Write("disconnected".into()))?
            .write_frame(frame)
    }
    pub fn failed(&mut self, now: Instant) {
        self.close();
        self.retry_at = now + Duration::from_millis(200);
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
