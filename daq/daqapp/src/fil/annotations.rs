//! DaqApp-only labels for FIL GPIO ports/pins and ADC instances/channels.
//!
//! These annotations live in `fil_annotations.json`, separate from runtime
//! settings and FIL's own configs, which reject unknown board-config fields.

use std::{collections::HashMap, path::Path};

pub const CONFIG_PATH: &str = "fil_annotations.json";

#[derive(serde::Serialize, serde::Deserialize, Clone, Debug, Default, PartialEq, Eq)]
pub struct FilAnnotations {
    #[serde(skip_serializing_if = "Option::is_none")]
    pub gpio: Option<HashMap<String, HashMap<String, GpioPortAnnotation>>>,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub adc: Option<HashMap<String, HashMap<String, AdcAnnotation>>>,
}

#[derive(serde::Serialize, serde::Deserialize, Clone, Debug, Default, PartialEq, Eq)]
pub struct GpioPortAnnotation {
    #[serde(skip_serializing_if = "Option::is_none")]
    pub label: Option<String>,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub pins: Option<HashMap<u8, String>>,
}

#[derive(serde::Serialize, serde::Deserialize, Clone, Debug, Default, PartialEq, Eq)]
pub struct AdcAnnotation {
    #[serde(skip_serializing_if = "Option::is_none")]
    pub label: Option<String>,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub channels: Option<HashMap<u8, String>>,
}

pub fn load() -> Result<FilAnnotations, String> {
    load_from_path(Path::new(CONFIG_PATH))
}

fn load_from_path(path: &Path) -> Result<FilAnnotations, String> {
    let content = match std::fs::read_to_string(path) {
        Ok(content) => content,
        Err(error) if error.kind() == std::io::ErrorKind::NotFound => {
            log::warn!(
                "{} is missing; using the embedded PER annotations",
                path.display()
            );
            include_str!("../../fil_annotations.json").to_owned()
        }
        Err(error) => return Err(format!("Failed to read {}: {error}", path.display())),
    };
    serde_json::from_str(&content)
        .map_err(|error| format!("Failed to parse {}: {error}", path.display()))
}

#[cfg(test)]
mod tests {
    use super::*;



    #[test]
    fn missing_config_uses_embedded_per_annotations() {
        let annotations = load_from_path(Path::new("missing-fil-annotations.json")).unwrap();
        assert!(
            annotations
                .gpio
                .as_ref()
                .unwrap()
                .contains_key("front_driveline")
        );
    }

}
