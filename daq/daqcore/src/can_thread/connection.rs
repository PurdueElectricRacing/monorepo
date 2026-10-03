use crate::connection;
use crate::{
    can::driver::{self, Driver, DriverError, DriverResult},
    connection::ConnectionSource,
    frame::CanFrame,
};
use std::time::{Duration, Instant};
pub(super) struct ConnectionManager {
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
    #[cfg(test)]
    pub fn with_driver(driver: Box<dyn Driver>) -> Self {
        Self {
            source: None,
            driver: Some(driver),
            retry_at: Instant::now(),
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

#[cfg(test)]
mod tests {
    use super::*;
    #[test]
    fn reconnect_deadline_and_explicit_disconnect() {
        let now = Instant::now();
        let mut connection = ConnectionManager::new(now);
        connection.select(Some(ConnectionSource::Loopback), now);
        assert!(connection.connect(now).unwrap().is_ok());
        assert!(connection.connected());
        connection.failed(now);
        assert!(!connection.connected());
        assert!(
            connection
                .connect(now + Duration::from_millis(199))
                .is_none()
        );
        assert!(
            connection
                .connect(now + Duration::from_millis(200))
                .unwrap()
                .is_ok()
        );
        connection.select(None, now);
        assert!(!connection.connected());
        assert!(connection.connect(now + Duration::from_secs(10)).is_none());
    }
}
