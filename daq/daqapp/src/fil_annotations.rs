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
            include_str!("../fil_annotations.json").to_owned()
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
    fn bundled_per_annotations_parse() {
        let annotations: FilAnnotations =
            serde_json::from_str(include_str!("../fil_annotations.json")).unwrap();
        let dashboard_gpio = &annotations.gpio.as_ref().unwrap()["dashboard"]["GPIOC"];
        assert_eq!(
            dashboard_gpio.pins.as_ref().unwrap()[&2],
            "Throttle 1 analog input"
        );
        assert_eq!(
            annotations.adc.as_ref().unwrap()["dashboard"]["ADC1"]
                .channels
                .as_ref()
                .unwrap()[&8],
            "Throttle 1"
        );

        let gpio = annotations.gpio.as_ref().unwrap();
        for board in [
            "main_module",
            "dashboard",
            "torque_vector",
            "a_box",
            "front_driveline",
            "rear_driveline",
        ] {
            assert!(
                gpio.contains_key(board),
                "missing GPIO annotations for {board}"
            );
        }
        let adc = annotations.adc.as_ref().unwrap();
        assert_eq!(
            adc["a_box"]["ADC1"].channels.as_ref().unwrap()[&1],
            "ISENSE"
        );
        assert_eq!(
            adc["front_driveline"]["ADC4"].channels.as_ref().unwrap()[&3],
            "Right shockpot"
        );
        assert!(!adc.contains_key("main_module"));
        assert!(!adc.contains_key("torque_vector"));
    }

    #[test]
    fn standalone_config_loads_from_disk() {
        let path = Path::new(env!("CARGO_MANIFEST_DIR")).join(CONFIG_PATH);
        let annotations = load_from_path(&path).unwrap();
        assert!(annotations.gpio.as_ref().unwrap().contains_key("dashboard"));
    }

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

    #[test]
    fn minimal_annotation_entries_can_omit_unset_sections() {
        let annotations: FilAnnotations =
            serde_json::from_str(r#"{"gpio":{"board":{"GPIOA":{"pins":{"0":"Signal"}}}}}"#)
                .unwrap();
        let pin = &annotations.gpio.as_ref().unwrap()["board"]["GPIOA"];
        assert_eq!(pin.label, None);
        assert_eq!(pin.pins.as_ref().unwrap()[&0], "Signal");
        assert!(annotations.adc.is_none());
    }
}
