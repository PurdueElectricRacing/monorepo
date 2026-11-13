use crate::{connection, driver, frame, superdbc};
use std::time;

pub struct ConnectionManager {
    source: Option<connection::ConnectionSource>,
    driver: Option<Box<dyn driver::Driver>>,
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
        buses: &[&superdbc::database::Bus],
    ) -> Option<Result<(), String>> {
        if self.driver.is_some() || now < self.retry_at {
            return None;
        }

        let source = self.source.as_ref()?;
        self.retry_at = now + time::Duration::from_millis(200);

        let result = match driver::create_driver(source, buses) {
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

    pub fn driver_speed(&self) -> Option<connection::CanBusSpeed> {
        self.driver.as_ref().and_then(|d| d.bus_speed())
    }

    pub fn read(&mut self) -> driver::DriverResult<Vec<frame::CanFrame>> {
        let driver = self
            .driver
            .as_mut()
            .ok_or(driver::DriverError::Timeout)?;

        driver.read_frames()
    }

    pub fn write(&mut self, frame: frame::CanFrameTx) -> driver::DriverResult<()> {
        let driver = self
            .driver
            .as_mut()
            .ok_or_else(|| driver::DriverError::Write("disconnected".into()))?;

        driver.write_frame(frame)
    }

    pub fn failed(&mut self, now: time::Instant) {
        self.close();
        self.retry_at = now + time::Duration::from_millis(200);
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
