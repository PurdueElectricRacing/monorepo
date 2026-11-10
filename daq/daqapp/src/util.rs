pub fn get_available_serial_ports() -> Vec<serialport::SerialPortInfo> {
    match serialport::available_ports() {
        Ok(ports) => ports
            .into_iter()
            .filter(|p| {
                let name = p.port_name.to_lowercase();
                if cfg!(target_os = "windows") {
                    name.starts_with("com")
                } else {
                    name.starts_with("/dev/tty.usbmodem") || name.starts_with("/dev/ttyacm")
                }
            })
            .collect(),
        Err(err) => {
            log::error!("Error listing serial ports: {}", err);
            Vec::new()
        }
    }
}

pub fn is_cannector_available() -> bool {
    match rusb::devices() {
        Ok(devices) => devices
            .iter()
            .any(|device| match device.device_descriptor() {
                Ok(desc) => {
                    desc.vendor_id() == daqcore::can::driver::CannectorDriver::VID
                        && desc.product_id() == daqcore::can::driver::CannectorDriver::PID
                }
                Err(err) => {
                    log::warn!("Failed to read USB descriptor: {}", err);
                    false
                }
            }),
        Err(err) => {
            log::error!("Error listing USB devices: {}", err);
            false
        }
    }
}

// Linear interpolation helper
pub fn lerp(a: f64, b: f64, t: f64) -> f64 {
    a + (b - a) * t
}
