#[derive(serde::Serialize, Clone, Debug, PartialEq)]
pub enum ConnectionSource {
    Serial(String, CanBusSpeed),
    Cannector(CannectorChannel),
    Udp(u16),
    Simulated(bool), // true for connected, false for disconnected
    Loopback,
}

impl<'de> serde::Deserialize<'de> for ConnectionSource {
    fn deserialize<D>(deserializer: D) -> Result<Self, D::Error>
    where
        D: serde::Deserializer<'de>,
    {
        let value = serde_json::Value::deserialize(deserializer)?;
        if value.as_str() == Some("Loopback") {
            return Ok(Self::Loopback);
        }

        let object = value
            .as_object()
            .ok_or_else(|| serde::de::Error::custom("connection source must be an object"))?;
        if let Some(serial) = object.get("Serial") {
            let values = serial.as_array().ok_or_else(|| {
                serde::de::Error::custom("Serial connection source must contain an array")
            })?;
            return match values.as_slice() {
                [path, speed] | [path, speed, _] => Ok(Self::Serial(
                    serde_json::from_value(path.clone()).map_err(serde::de::Error::custom)?,
                    serde_json::from_value(speed.clone()).map_err(serde::de::Error::custom)?,
                )),
                _ => Err(serde::de::Error::custom(
                    "Serial connection source must contain path and speed",
                )),
            };
        }

        if let Some(channel) = object.get("Cannector") {
            return Ok(Self::Cannector(
                serde_json::from_value(channel.clone()).map_err(serde::de::Error::custom)?,
            ));
        }

        if let Some(port) = object.get("Udp") {
            return Ok(Self::Udp(
                serde_json::from_value(port.clone()).map_err(serde::de::Error::custom)?,
            ));
        }

        if let Some(simulated) = object.get("Simulated") {
            let connected = simulated
                .as_bool()
                .ok_or_else(|| serde::de::Error::custom("invalid simulated connection source"))?;
            return Ok(Self::Simulated(connected));
        }

        if object.get("Loopback").is_some() {
            return Ok(Self::Loopback);
        }
        Err(serde::de::Error::custom("unknown connection source"))
    }
}

#[derive(serde::Serialize, serde::Deserialize, Copy, Clone, PartialEq, Eq, Debug)]
pub enum CannectorChannel {
    Vcan,
    Mcan,
    Scan,
}

impl CannectorChannel {
    pub fn display_name(&self) -> String {
        match self {
            CannectorChannel::Vcan => "VCAN input".into(),
            CannectorChannel::Mcan => "MCAN input".into(),
            CannectorChannel::Scan => "SCAN input".into(),
        }
    }

    pub fn options() -> Vec<CannectorChannel> {
        vec![
            CannectorChannel::Vcan,
            CannectorChannel::Mcan,
            CannectorChannel::Scan,
        ]
    }
}

#[derive(serde::Serialize, serde::Deserialize, Copy, Clone, PartialEq, Debug, Default)]
pub enum CanBusSpeed {
    Kbps250,
    #[default]
    Kbps500,
}

impl ConnectionSource {
    pub fn display_name(&self) -> String {
        match self {
            ConnectionSource::Serial(path, speed) => {
                format!("Serial: {} ({})", path, speed.display_name())
            }
            ConnectionSource::Cannector(channel) => {
                format!("CANnector USB: {} (500k, RX only)", channel.display_name())
            }
            ConnectionSource::Udp(port) => format!("UDP: {}", port),
            ConnectionSource::Simulated(connected) => {
                if *connected {
                    "Simulated (connected)".into()
                } else {
                    "Simulated (disconnected)".into()
                }
            }
            ConnectionSource::Loopback => "Loopback".into(),
        }
    }
}

impl CanBusSpeed {
    pub fn display_name(&self) -> String {
        match self {
            CanBusSpeed::Kbps250 => "250k".into(),
            CanBusSpeed::Kbps500 => "500k".into(),
        }
    }

    pub fn to_bps(self) -> u32 {
        match self {
            CanBusSpeed::Kbps250 => 250_000,
            CanBusSpeed::Kbps500 => 500_000,
        }
    }

    pub fn options() -> Vec<CanBusSpeed> {
        vec![CanBusSpeed::Kbps250, CanBusSpeed::Kbps500]
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn cannector_sources_round_trip_settings() {
        for channel in CannectorChannel::options() {
            let source = ConnectionSource::Cannector(channel);
            let json = serde_json::to_string(&source).unwrap();
            assert_eq!(
                serde_json::from_str::<ConnectionSource>(&json).unwrap(),
                source
            );
        }
    }

    #[test]
    fn rejects_unknown_cannector_channels() {
        for json in [r#"{"Cannector":"McanScan"}"#, r#"{"Cannector":"Ccan"}"#] {
            assert!(serde_json::from_str::<ConnectionSource>(json).is_err());
        }
    }

    #[test]
    fn existing_sources_still_deserialize() {
        for json in [
            r#"{"Serial":["/dev/test","Kbps500"]}"#,
            r#"{"Serial":["/dev/test","Kbps500",null]}"#,
        ] {
            assert_eq!(
                serde_json::from_str::<ConnectionSource>(json).unwrap(),
                ConnectionSource::Serial("/dev/test".into(), CanBusSpeed::Kbps500),
            );
        }
        for source in [
            ConnectionSource::Udp(5005),
            ConnectionSource::Loopback,
            ConnectionSource::Simulated(true),
            ConnectionSource::Simulated(false),
        ] {
            let json = serde_json::to_string(&source).unwrap();
            assert_eq!(
                serde_json::from_str::<ConnectionSource>(&json).unwrap(),
                source
            );
        }
    }
}
