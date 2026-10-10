#[derive(Clone, Copy, Debug, Default, Eq, PartialEq, Hash, serde::Serialize)]
pub enum FilAdcInstance {
    #[default]
    #[serde(rename = "ADC1")]
    Adc1,
    #[serde(rename = "ADC2")]
    Adc2,
    #[serde(rename = "ADC3")]
    Adc3,
    #[serde(rename = "ADC4")]
    Adc4,
}

impl FilAdcInstance {
    pub const ALL: [Self; 4] = [Self::Adc1, Self::Adc2, Self::Adc3, Self::Adc4];

    pub const fn as_str(self) -> &'static str {
        match self {
            Self::Adc1 => "ADC1",
            Self::Adc2 => "ADC2",
            Self::Adc3 => "ADC3",
            Self::Adc4 => "ADC4",
        }
    }

    pub fn parse(value: &str) -> Option<Self> {
        Self::ALL
            .into_iter()
            .find(|instance| instance.as_str() == value)
    }
}

impl std::fmt::Display for FilAdcInstance {
    fn fmt(&self, formatter: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        formatter.write_str(self.as_str())
    }
}

impl<'de> serde::Deserialize<'de> for FilAdcInstance {
    fn deserialize<D>(deserializer: D) -> Result<Self, D::Error>
    where
        D: serde::Deserializer<'de>,
    {
        let value = <serde_json::Value as serde::Deserialize>::deserialize(deserializer)?;
        Ok(value.as_str().and_then(Self::parse).unwrap_or_default())
    }
}

#[derive(Clone, Copy, Debug, Eq, PartialEq, Hash)]
pub enum FilGpioPort {
    GpioA,
    GpioB,
    GpioC,
    GpioD,
    GpioE,
    GpioF,
    GpioG,
}

impl FilGpioPort {
    pub const ALL: [Self; 7] = [
        Self::GpioA,
        Self::GpioB,
        Self::GpioC,
        Self::GpioD,
        Self::GpioE,
        Self::GpioF,
        Self::GpioG,
    ];

    pub const fn as_str(self) -> &'static str {
        match self {
            Self::GpioA => "GPIOA",
            Self::GpioB => "GPIOB",
            Self::GpioC => "GPIOC",
            Self::GpioD => "GPIOD",
            Self::GpioE => "GPIOE",
            Self::GpioF => "GPIOF",
            Self::GpioG => "GPIOG",
        }
    }

    pub fn parse(value: &str) -> Option<Self> {
        Self::ALL.into_iter().find(|port| port.as_str() == value)
    }
}

impl std::fmt::Display for FilGpioPort {
    fn fmt(&self, formatter: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        formatter.write_str(self.as_str())
    }
}

#[cfg(test)]
mod tests {
    use super::{FilAdcInstance, FilGpioPort};

    #[test]
    fn adc_instances_parse_and_serialize_as_protocol_names() {
        for instance in FilAdcInstance::ALL {
            assert_eq!(FilAdcInstance::parse(instance.as_str()), Some(instance));
            assert_eq!(instance.to_string(), instance.as_str());
            assert_eq!(
                serde_json::to_string(&instance).unwrap(),
                format!("\"{}\"", instance.as_str())
            );
        }
        assert_eq!(FilAdcInstance::parse("adc1"), None);
        assert_eq!(FilAdcInstance::parse("ADC5"), None);
        assert_eq!(
            serde_json::from_str::<FilAdcInstance>("\"ADC9\"").unwrap(),
            FilAdcInstance::Adc1
        );
    }

    #[test]
    fn gpio_ports_parse_only_supported_protocol_names() {
        for port in FilGpioPort::ALL {
            assert_eq!(FilGpioPort::parse(port.as_str()), Some(port));
            assert_eq!(port.to_string(), port.as_str());
        }
        assert_eq!(FilGpioPort::parse("gpioa"), None);
        assert_eq!(FilGpioPort::parse("GPIOH"), None);
    }
}
