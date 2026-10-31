use crate::{
    can,
    can::driver::{ActiveDriver, DriverError, DriverResult, FilExpectationEvent, FilGpioEvent},
    connection, frame, superdbc,
};

use std::time;

pub struct ConnectionManager {
    source: Option<connection::ConnectionSource>,
    driver: Option<ActiveDriver>,
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
        let _ = self.close();
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

    pub fn source(&self) -> Option<&connection::ConnectionSource> {
        self.source.as_ref()
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

    pub fn set_fil_bus(&mut self, selected_bus: Option<u8>) -> DriverResult<()> {
        let Some(connection::ConnectionSource::Fil { bus, trace_bus, .. }) = self.source.as_mut()
        else {
            // Database selection is global, but non-FIL connection sources have no bus
            // selection state to synchronize.
            return Ok(());
        };

        // FIL source configuration uses decimal-string bus keys; worker commands use IDs.
        let source_bus = selected_bus.map(|bus_id| bus_id.to_string());

        // Keep the selected source current even while disconnected, so the next connection
        // starts with precisely the same injection fallback and trace filter.
        if let Some(selected_bus) = &source_bus {
            *bus = selected_bus.clone();
            *trace_bus = Some(selected_bus.clone());
        } else {
            *trace_bus = None;
        }

        if let Some(driver) = self.driver.as_mut()
            && let ActiveDriver::Fil(fil) = driver
        {
            fil.set_bus_selection(selected_bus)?;
        }
        Ok(())
    }

    pub fn take_fil_gpio_events(&mut self) -> Vec<FilGpioEvent> {
        match self.driver.as_mut() {
            Some(ActiveDriver::Fil(fil)) => fil.take_gpio_events(),
            _ => Vec::new(),
        }
    }

    pub fn take_fil_expectation_events(&mut self) -> Vec<FilExpectationEvent> {
        match self.driver.as_mut() {
            Some(ActiveDriver::Fil(fil)) => fil.take_expectation_events(),
            _ => Vec::new(),
        }
    }

    pub fn set_gpio(
        &mut self,
        board: &str,
        port: &str,
        pin: u8,
        value: Option<bool>,
    ) -> DriverResult<()> {
        match self.driver.as_mut() {
            Some(ActiveDriver::Fil(fil)) => fil.set_gpio(board, port, pin, value),
            Some(ActiveDriver::Can(_)) => Err(DriverError::Unsupported(
                "GPIO control is not supported by this source".into(),
            )),
            None => Err(DriverError::Write("disconnected".into())),
        }
    }

    pub fn set_adc(
        &mut self,
        board: &str,
        instance: &str,
        channel: u8,
        value: u16,
    ) -> can::driver::DriverResult<()> {
        match self.driver.as_mut() {
            Some(ActiveDriver::Fil(fil)) => fil.set_adc(board, instance, channel, value),
            Some(ActiveDriver::Can(_)) => Err(DriverError::Unsupported(
                "ADC injection is not supported by this source".into(),
            )),
            None => Err(can::driver::DriverError::Write("disconnected".into())),
        }
    }

    pub fn write(&mut self, frame: frame::CanFrame) -> can::driver::DriverResult<()> {
        let driver = self
            .driver
            .as_mut()
            .ok_or_else(|| can::driver::DriverError::Write("disconnected".into()))?;

        driver.write_frame(frame)
    }

    pub fn failed(&mut self, now: time::Instant) -> Vec<crate::can::driver::FilExpectationEvent> {
        let was_fil = self.driver.as_ref().is_some_and(ActiveDriver::is_fil);
        let expectations = self.close();
        if was_fil {
            self.source = None;
        } else {
            self.retry_at = now + time::Duration::from_millis(200);
        }
        expectations
    }

    pub fn close(&mut self) -> Vec<FilExpectationEvent> {
        if let Some(mut driver) = self.driver.take() {
            let _ = driver.close();
            match &mut driver {
                ActiveDriver::Fil(fil) => fil.take_all_expectation_events(),
                ActiveDriver::Can(_) => Vec::new(),
            }
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

#[cfg(test)]
mod tests {
    use super::ConnectionManager;
    use crate::connection::ConnectionSource;
    use std::time;

    fn fil_source() -> ConnectionSource {
        serde_json::from_value(serde_json::json!({
            "Fil": { "executable": "fil", "network": "network", "bus": "fallback" }
        }))
        .expect("valid FIL source")
    }

    #[test]
    fn fil_bus_selection_updates_source_while_disconnected() {
        let now = time::Instant::now();
        let mut manager = ConnectionManager::new(now);
        manager.select(Some(fil_source()), now);

        manager
            .set_fil_bus(Some(1))
            .expect("ID 1 should be stored while disconnected");
        let Some(ConnectionSource::Fil { bus, trace_bus, .. }) = manager.source.as_ref() else {
            panic!("expected FIL source");
        };
        assert_eq!(bus, "1");
        assert_eq!(trace_bus.as_deref(), Some("1"));

        manager
            .set_fil_bus(Some(0))
            .expect("ID 0 should be stored while disconnected");
        let Some(ConnectionSource::Fil { bus, trace_bus, .. }) = manager.source.as_ref() else {
            panic!("expected FIL source");
        };
        assert_eq!(bus, "0");
        assert_eq!(trace_bus.as_deref(), Some("0"));

        manager
            .set_fil_bus(None)
            .expect("clearing trace selection should succeed");
        let Some(ConnectionSource::Fil { bus, trace_bus, .. }) = manager.source.as_ref() else {
            panic!("expected FIL source");
        };
        assert_eq!(bus, "0");
        assert_eq!(*trace_bus, None);
    }
}
