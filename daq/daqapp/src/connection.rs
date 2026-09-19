#[derive(serde::Serialize, Clone, Debug, PartialEq)]
pub enum ConnectionSource {
    Serial(String, CanBusSpeed),
    Udp(u16),
    Simulated(bool, Option<std::path::PathBuf>), // true for connected, false for disconnected, path to dbc file for sim
    Loopback,
}

impl<'de> serde::Deserialize<'de> for ConnectionSource {
    fn deserialize<D>(deserializer: D) -> Result<Self, D::Error>
    where
        D: serde::Deserializer<'de>,
    {
        let value = serde_json::Value::deserialize(deserializer)?;
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
        if let Some(port) = object.get("Udp") {
            return Ok(Self::Udp(
                serde_json::from_value(port.clone()).map_err(serde::de::Error::custom)?,
            ));
        }
        if let Some(simulated) = object.get("Simulated") {
            let values: (bool, Option<std::path::PathBuf>) =
                serde_json::from_value(simulated.clone()).map_err(serde::de::Error::custom)?;
            return Ok(Self::Simulated(values.0, values.1));
        }
        if object.get("Loopback").is_some() {
            return Ok(Self::Loopback);
        }
        Err(serde::de::Error::custom("unknown connection source"))
    }
}

#[derive(serde::Serialize, serde::Deserialize, Copy, Clone, PartialEq, Debug)]

pub enum CanBusSpeed {
    Kbps250,
    Kbps500,
}

impl ConnectionSource {
    pub fn display_name(&self) -> String {
        match self {
            ConnectionSource::Serial(path, speed) => {
                format!("Serial: {} ({})", path, speed.display_name())
            }
            ConnectionSource::Udp(port) => format!("UDP: {}", port),
            ConnectionSource::Simulated(connected, _) => {
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

    pub fn to_slcan_bitrate(self) -> slcan::NominalBitRate {
        match self {
            CanBusSpeed::Kbps250 => slcan::NominalBitRate::Rate250Kbit,
            CanBusSpeed::Kbps500 => slcan::NominalBitRate::Rate500Kbit,
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

impl Default for CanBusSpeed {
    fn default() -> Self {
        CanBusSpeed::Kbps500
    }
}
