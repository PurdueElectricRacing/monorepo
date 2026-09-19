use indexmap::IndexMap;
use serde::{Deserialize, Deserializer, Serialize, Serializer};

pub const FORMATTER_CONFIG_FILE: &str = "formatter_config.json";

#[derive(Debug, Clone, PartialEq, Eq)]
pub enum Formatting {
    Hex,
    Binary,
    Decimal(usize), // Number of decimal places
}

// Message name/pattern -> signal name/pattern -> formatting
type FormatterConfig = IndexMap<String, IndexMap<String, Formatting>>;
type CompiledFormatterConfig = Vec<(
    globset::GlobMatcher,
    Vec<(globset::GlobMatcher, Formatting)>,
)>;

impl Serialize for Formatting {
    fn serialize<S>(&self, serializer: S) -> Result<S::Ok, S::Error>
    where
        S: Serializer,
    {
        match self {
            Formatting::Hex => serializer.serialize_str("hex"),
            Formatting::Binary => serializer.serialize_str("binary"),
            Formatting::Decimal(places) => serializer.serialize_u64(*places as u64),
        }
    }
}

impl<'de> Deserialize<'de> for Formatting {
    fn deserialize<D>(deserializer: D) -> Result<Self, D::Error>
    where
        D: Deserializer<'de>,
    {
        struct FormattingVisitor;

        impl<'de> serde::de::Visitor<'de> for FormattingVisitor {
            type Value = Formatting;

            fn expecting(&self, formatter: &mut std::fmt::Formatter) -> std::fmt::Result {
                formatter.write_str(r#"a non-negative integer, "hex", or "binary""#)
            }

            fn visit_i64<E>(self, value: i64) -> Result<Self::Value, E>
            where
                E: serde::de::Error,
            {
                if value < 0 {
                    return Err(E::custom("decimal places must be non-negative"));
                }

                let value =
                    usize::try_from(value).map_err(|_| E::custom("decimal places out of range"))?;

                Ok(Formatting::Decimal(value))
            }

            fn visit_u64<E>(self, value: u64) -> Result<Self::Value, E>
            where
                E: serde::de::Error,
            {
                let value =
                    usize::try_from(value).map_err(|_| E::custom("decimal places out of range"))?;

                Ok(Formatting::Decimal(value))
            }

            fn visit_str<E>(self, value: &str) -> Result<Self::Value, E>
            where
                E: serde::de::Error,
            {
                match value {
                    "hex" => Ok(Formatting::Hex),
                    "binary" => Ok(Formatting::Binary),
                    _ => Err(E::unknown_variant(value, &["<integer>", "hex", "binary"])),
                }
            }
        }

        deserializer.deserialize_any(FormattingVisitor)
    }
}

impl Formatting {
    pub fn expected_decimals(&self) -> usize {
        match self {
            Formatting::Hex | Formatting::Binary => 0,
            Formatting::Decimal(places) => *places,
        }
    }
}

pub struct Formatter {
    compiled_config: CompiledFormatterConfig,
}

impl Formatter {
    pub fn new(config: FormatterConfig) -> Result<Self, globset::Error> {
        let mut compiled_config = Vec::new();
        for (msg_pattern, signal_map) in &config {
            let msg_glob = globset::Glob::new(msg_pattern)?.compile_matcher();
            let mut compiled_signal_map = Vec::new();
            for (signal_pattern, formatting) in signal_map {
                let signal_glob = globset::Glob::new(signal_pattern)?.compile_matcher();
                compiled_signal_map.push((signal_glob, formatting.clone()));
            }
            compiled_config.push((msg_glob, compiled_signal_map));
        }
        Ok(Self { compiled_config })
    }

    pub fn new_from_file(path: &str) -> Result<Self, Box<dyn std::error::Error>> {
        let config_str = std::fs::read_to_string(path)?;
        let config: FormatterConfig = serde_json::from_str(&config_str)?;
        Self::new(config).map_err(|e| e.into())
    }

    pub fn try_load() -> Option<Self> {
        Self::new_from_file(FORMATTER_CONFIG_FILE)
            .map_err(|e| {
                log::error!(
                    "Failed to load formatter config from {}: {}",
                    FORMATTER_CONFIG_FILE,
                    e
                );
                e
            })
            .ok()
    }

    /// Formats a signal value based on the message and signal name, using the first matching pattern in the config.
    /// If no pattern matches, returns a default formatted string (enum label if available, otherwise physical value with 2 decimal places).
    /// If there is a unit associated with the signal, it will be appended to the formatted value.
    /// Hex and binary formatting require the signal definition to determine the number of bits and value type.
    pub fn format(
        &self,
        msg_name: &str,
        signal_name: &str,
        sig_def: Option<&can_dbc::Signal>,
        unit: Option<&str>,
        value: &can_decode::DecodedSignalValue,
    ) -> String {
        for (msg_glob, signal_vec) in &self.compiled_config {
            if msg_glob.is_match(msg_name) {
                for (signal_glob, formatting) in signal_vec {
                    if signal_glob.is_match(signal_name) {
                        let have_enough_info = match formatting {
                            Formatting::Hex | Formatting::Binary => sig_def.is_some(),
                            Formatting::Decimal(_) => true,
                        };
                        if have_enough_info {
                            let raw = match formatting {
                                Formatting::Hex => format_hex(sig_def.as_ref().unwrap(), value),
                                Formatting::Binary => {
                                    format_binary(sig_def.as_ref().unwrap(), value)
                                }
                                Formatting::Decimal(places) => {
                                    format!("{:.*}", *places, value.physical)
                                }
                            };
                            let maybe_unit = unit
                                .or_else(|| sig_def.map(|s| s.unit.as_str()))
                                .filter(|u| !u.is_empty());
                            if let Some(u) = maybe_unit
                                && !u.is_empty()
                            {
                                return format!("{} {}", raw, u);
                            } else {
                                return raw;
                            }
                        }
                    }
                }
            }
        }

        let maybe_unit = unit
            .or_else(|| sig_def.map(|s| s.unit.as_str()))
            .filter(|u| !u.is_empty());
        default_format(maybe_unit, value)
    }

    pub fn expected_decimals(&self, msg_name: &str, signal_name: &str) -> usize {
        for (msg_glob, signal_vec) in &self.compiled_config {
            if msg_glob.is_match(msg_name) {
                for (signal_glob, formatting) in signal_vec {
                    if signal_glob.is_match(signal_name) {
                        return formatting.expected_decimals();
                    }
                }
            }
        }
        2 // Default to 2 decimal places if no match is found
    }
}

pub fn try_format(
    formatter: &Option<Formatter>,
    msg_name: &str,
    signal_name: &str,
    sig_def: Option<&can_dbc::Signal>,
    unit: Option<&str>,
    value: &can_decode::DecodedSignalValue,
) -> String {
    if let Some(fmt) = formatter {
        fmt.format(msg_name, signal_name, sig_def, unit, value)
    } else {
        let maybe_unit = unit
            .or_else(|| sig_def.map(|s| s.unit.as_str()))
            .filter(|u| !u.is_empty());
        default_format(maybe_unit, value)
    }
}

pub fn default_format(unit: Option<&str>, value: &can_decode::DecodedSignalValue) -> String {
    if let Some(enum_label) = &value.enum_label {
        format!("{} ({})", enum_label, value.int_rounded())
    } else if let Some(u) = unit
        && !u.is_empty()
    {
        format!("{:.2} {}", value.physical, u)
    } else {
        format!("{:.2}", value.physical)
    }
}

fn format_hex(sig_def: &can_dbc::Signal, value: &can_decode::DecodedSignalValue) -> String {
    let bits = sig_def.size.clamp(1, 64) as u32;
    let nybbles = bits.div_ceil(4) as usize;

    match sig_def.value_type {
        can_dbc::ValueType::Unsigned => {
            let mask = if bits == 64 {
                u64::MAX
            } else {
                (1u64 << bits) - 1
            };

            let val = value.int_rounded() as u64 & mask;

            format!("0x{:0width$X}", val, width = nybbles)
        }

        can_dbc::ValueType::Signed => {
            let val = value.int_rounded();

            if val < 0 {
                format!("-0x{:0width$X}", (-val) as u64, width = nybbles)
            } else {
                format!("0x{:0width$X}", val as u64, width = nybbles)
            }
        }
    }
}

fn format_binary(sig_def: &can_dbc::Signal, value: &can_decode::DecodedSignalValue) -> String {
    let bits = sig_def.size.clamp(1, 64) as usize;

    match sig_def.value_type {
        can_dbc::ValueType::Unsigned => {
            let mask = if bits == 64 {
                u64::MAX
            } else {
                (1u64 << bits) - 1
            };

            let val = value.int_rounded() as u64 & mask;

            format!("0b{:0width$b}", val, width = bits)
        }

        can_dbc::ValueType::Signed => {
            let val = value.int_rounded();

            if val < 0 {
                format!("-0b{:0width$b}", (-val) as u64, width = bits)
            } else {
                format!("0b{:0width$b}", val as u64, width = bits)
            }
        }
    }
}
