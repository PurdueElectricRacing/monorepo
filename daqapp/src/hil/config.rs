// Read HIL config files (preset and tests)

use indexmap::IndexMap;
use serde::{Deserialize, Serialize};

pub const PRESETS_FILE: &str = "hil_config/presets.json";
pub const TESTS_FOLDER: &str = "hil_config/tests/";

#[derive(Debug, Clone, Serialize, Deserialize)]
#[serde(transparent)]
pub struct PresetsFile(pub IndexMap<String, Vec<String>>);

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct TestFile {
    pub name: String,
    pub description: String,

    #[serde(default)]
    pub tx: Vec<TxMessage>,

    pub expect: Vec<Expectation>,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct TxMessage {
    pub timestamp: f64,
    pub msg_name: String,

    pub signals: IndexMap<String, f64>,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct Expectation {
    pub window: [f64; 2],

    pub msg_name: String,

    #[serde(default)]
    pub signals: IndexMap<String, [f64; 2]>,
}

#[derive(Clone)]
pub struct PresetInfo {
    pub name: String,
    // base names
    pub tests: Vec<String>,
}

#[derive(Clone)]
pub struct TestInfo {
    pub basename: String,
    pub name: String,
    pub description: String,
}

pub fn list_available_tests() -> (Vec<PresetInfo>, Vec<TestInfo>, Vec<String>) {
    let mut errors = Vec::new();

    // Load individual tests
    let mut individual_tests = Vec::new();
    if let Ok(entries) = std::fs::read_dir(TESTS_FOLDER) {
        for entry in entries.flatten() {
            let path = entry.path();
            if !path.is_file() || path.extension().and_then(|e| e.to_str()) != Some("json") {
                continue;
            }

            let basename = match path.file_stem().and_then(|s| s.to_str()) {
                Some(stem) if !stem.is_empty() => stem.to_string(),
                _ => {
                    errors.push(format!(
                        "Failed to determine test basename for file: {:?}",
                        path
                    ));
                    continue;
                }
            };

            let content = match std::fs::read_to_string(&path) {
                Ok(c) => c,
                Err(_) => {
                    errors.push(format!("Failed to read test file: {:?}", path));
                    continue;
                }
            };

            match serde_json::from_str::<TestFile>(&content) {
                Ok(test_data) => individual_tests.push(TestInfo {
                    basename,
                    name: test_data.name,
                    description: test_data.description,
                }),
                Err(_) => errors.push(format!("Failed to parse test file: {:?}", path)),
            };
        }
    } else {
        errors.push(format!("Failed to read tests directory: {}", TESTS_FOLDER));
    }
    individual_tests.sort_by(|a, b| a.basename.to_lowercase().cmp(&b.basename.to_lowercase()));

    // Load presets
    let mut presets = Vec::new();
    if let Ok(presets_file) = std::fs::read_to_string(PRESETS_FILE) {
        if let Ok(presets_data) = serde_json::from_str::<PresetsFile>(&presets_file) {
            for (preset_name, subtests) in presets_data.0 {
                if subtests.is_empty() {
                    errors.push(format!("Preset '{}' has no subtests defined", preset_name));
                    continue;
                }

                if subtests.iter().any(|s| s.trim().is_empty()) {
                    errors.push(format!(
                        "Preset '{}' contains empty subtest names",
                        preset_name
                    ));
                    continue;
                }

                if subtests
                    .iter()
                    .any(|s| individual_tests.iter().all(|t| t.basename != *s))
                {
                    errors.push(format!(
                        "Preset '{}' contains subtests that do not exist: {:?}",
                        preset_name,
                        subtests
                            .iter()
                            .filter(|s| individual_tests.iter().all(|t| t.basename != **s))
                            .collect::<Vec<_>>()
                    ));
                    continue;
                }

                presets.push(PresetInfo {
                    name: preset_name,
                    tests: subtests,
                });
            }
        } else {
            errors.push(format!("Failed to parse presets file: {}", PRESETS_FILE));
        }
    } else {
        errors.push(format!("Failed to read presets file: {}", PRESETS_FILE));
    }
    presets.sort_by(|a, b| a.name.to_lowercase().cmp(&b.name.to_lowercase()));

    (presets, individual_tests, errors)
}

pub fn load_test_from_file(basename: &str) -> Result<TestFile, String> {
    let path = format!("{}{}.json", TESTS_FOLDER, basename);
    match std::fs::read_to_string(&path) {
        Ok(content) => match serde_json::from_str::<TestFile>(&content) {
            Ok(test_data) => Ok(test_data),
            Err(_) => Err(format!("Failed to parse test file: {}", path)),
        },
        Err(_) => Err(format!("Failed to read test file: {}", path)),
    }
}
