//! FIL network/board configuration inspection and override materialization.
//!
//! A FIL network JSON references board JSON files, which in turn reference
//! firmware ELF images. Those ELF paths are baked into the checked-in configs
//! and frequently point at stale locations, so the FIL widget lets users
//! override the ELF per board and choose which boards participate. `fil`'s
//! `watch-network` CLI accepts a single network file and has no ELF-override
//! flags, so overrides are applied by writing patched board/network JSON files
//! into a deterministic temp directory and launching FIL against those.

use std::collections::{HashMap, HashSet};
use std::path::{Component, Path, PathBuf};

/// One board referenced by a FIL network configuration.
#[derive(Clone, Debug)]
pub struct FilBoardInfo {
    /// Board name from the board JSON (`name` field, or file stem fallback).
    pub name: String,
    /// Absolute path of the original board JSON file.
    pub board_path: PathBuf,
    /// Absolute resolved path of the ELF referenced by the board JSON, if any.
    pub default_elf: Option<PathBuf>,
    /// Whether the referenced ELF currently exists on disk.
    pub default_elf_exists: bool,
}

/// Summary of a FIL network configuration.
#[derive(Clone, Debug)]
pub struct FilNetworkInfo {
    /// Network name from the network JSON.
    pub name: String,
    /// Declared bus names (`buses` object keys).
    pub buses: Vec<String>,
    /// Boards in deterministic scheduling order.
    pub boards: Vec<FilBoardInfo>,
}

/// Join `relative` onto `base_dir` and normalize `.`/`..` lexically without
/// touching the filesystem. Absolute inputs are normalized as-is.
fn absolutize(base_dir: &Path, relative: &Path) -> PathBuf {
    let joined = if relative.is_absolute() {
        relative.to_path_buf()
    } else {
        base_dir.join(relative)
    };
    let mut out = PathBuf::new();
    for component in joined.components() {
        match component {
            Component::CurDir => {}
            Component::ParentDir => {
                out.pop();
            }
            other => out.push(other.as_os_str()),
        }
    }
    out
}

fn read_json_file(path: &Path) -> Result<serde_json::Value, String> {
    let content = std::fs::read_to_string(path)
        .map_err(|error| format!("Failed to read {}: {error}", path.display()))?;
    serde_json::from_str(&content)
        .map_err(|error| format!("Failed to parse {}: {error}", path.display()))
}

fn board_name(board_json: &serde_json::Value, board_path: &Path) -> String {
    board_json
        .get("name")
        .and_then(serde_json::Value::as_str)
        .map(str::to_owned)
        .unwrap_or_else(|| {
            board_path
                .file_stem()
                .map(|stem| stem.to_string_lossy().into_owned())
                .unwrap_or_else(|| "board".into())
        })
}

/// One board entry in a user-built network, synthesized from an ELF image
/// and a few fields. No board JSON file is required.
#[derive(serde::Serialize, serde::Deserialize, Clone, Debug, PartialEq)]
pub struct BuiltBoard {
    /// Board name used for scheduling, GPIO/ADC targeting, and file names.
    #[serde(default)]
    pub name: String,
    /// Firmware ELF image.
    #[serde(default)]
    pub elf: PathBuf,
    /// MCU config JSON. Empty means auto-located next to the FIL executable.
    #[serde(default)]
    pub mcu: PathBuf,
    /// FDCAN instances attached to the network bus. Empty means FDCAN1.
    #[serde(default)]
    pub can_instances: Vec<String>,
    /// Optional vector-table base (e.g. bootloader offset). Empty means omit.
    #[serde(default)]
    pub vector_base: String,
    /// Whether the board participates in the emulated network.
    #[serde(default = "default_board_enabled")]
    pub enabled: bool,
    /// Legacy: board JSON config file, migrated to direct fields on load.
    #[serde(default)]
    pub board: PathBuf,
    /// Legacy: firmware ELF override for `board`.
    #[serde(default)]
    pub elf_override: Option<PathBuf>,
}

fn default_board_enabled() -> bool {
    true
}

/// A user-built network composed in the FIL widget without hand-writing JSON.
#[derive(serde::Serialize, serde::Deserialize, Clone, Debug, PartialEq)]
pub struct BuiltNetwork {
    pub name: String,
    pub bus: String,
    pub bitrate: u32,
    pub boards: Vec<BuiltBoard>,
}

impl Default for BuiltNetwork {
    fn default() -> Self {
        Self {
            name: "custom".into(),
            bus: "vehicle".into(),
            bitrate: 500_000,
            boards: Vec::new(),
        }
    }
}

/// Inspect an arbitrary board config file, resolving its default ELF path.
pub fn load_board_info(board_path: &Path) -> Result<FilBoardInfo, String> {
    let board_json = read_json_file(board_path)?;
    let board_dir = board_path
        .parent()
        .unwrap_or_else(|| Path::new("."))
        .to_path_buf();
    let default_elf = board_json
        .get("elf")
        .and_then(serde_json::Value::as_str)
        .map(|elf| absolutize(&board_dir, Path::new(elf)));
    let default_elf_exists = default_elf.as_ref().is_some_and(|path| path.is_file());
    Ok(FilBoardInfo {
        name: board_name(&board_json, board_path),
        board_path: board_path.to_path_buf(),
        default_elf,
        default_elf_exists,
    })
}

/// Inspect a FIL network JSON file, resolving board and ELF paths.
pub fn load_network_info(network_path: &Path) -> Result<FilNetworkInfo, String> {
    if !network_path.is_file() {
        return Err(format!(
            "Network config does not exist: {}",
            network_path.display()
        ));
    }
    let network_json = read_json_file(network_path)?;
    let network_dir = network_path
        .parent()
        .unwrap_or_else(|| Path::new("."))
        .to_path_buf();
    let name = network_json
        .get("name")
        .and_then(serde_json::Value::as_str)
        .unwrap_or("network")
        .to_owned();
    let buses = network_json
        .get("buses")
        .and_then(serde_json::Value::as_object)
        .map(|buses| buses.keys().cloned().collect())
        .unwrap_or_default();
    let board_refs = network_json
        .get("boards")
        .and_then(serde_json::Value::as_array)
        .ok_or_else(|| {
            format!(
                "Network config {} has no `boards` array",
                network_path.display()
            )
        })?;
    let mut boards = Vec::with_capacity(board_refs.len());
    for board_ref in board_refs {
        let board_rel = board_ref.as_str().ok_or_else(|| {
            format!(
                "Network config {} has a non-string board entry",
                network_path.display()
            )
        })?;
        let board_path = absolutize(&network_dir, Path::new(board_rel));
        if !board_path.is_file() {
            return Err(format!(
                "Board config does not exist: {} (referenced by {})",
                board_path.display(),
                network_path.display()
            ));
        }
        boards.push(load_board_info(&board_path)?);
    }
    Ok(FilNetworkInfo {
        name,
        buses,
        boards,
    })
}

/// Effective ELF for a board: user override when set, otherwise the default.
pub fn effective_elf(
    board: &FilBoardInfo,
    overrides: &HashMap<String, PathBuf>,
) -> Option<PathBuf> {
    overrides
        .get(&board.name)
        .cloned()
        .or_else(|| board.default_elf.clone())
}

/// Deterministic scratch directory for one network file, so reconnects and
/// restarts reuse the same materialized paths.
fn scratch_dir(network_path: &Path) -> PathBuf {
    use std::collections::hash_map::DefaultHasher;
    use std::hash::{Hash, Hasher};
    let mut hasher = DefaultHasher::new();
    network_path.to_string_lossy().hash(&mut hasher);
    let stem = network_path
        .file_stem()
        .map(|stem| stem.to_string_lossy().into_owned())
        .unwrap_or_else(|| "network".into());
    std::env::temp_dir()
        .join("daqapp-fil")
        .join(format!("{stem}-{:016x}", hasher.finish()))
}

/// Resolve the network file FIL should actually be launched with.
///
/// Returns the original path unchanged when no override applies to an enabled
/// board and no board is disabled. Otherwise writes patched board JSON files
/// (absolute ELF override substituted, everything else preserved) plus a
/// patched network JSON referencing the enabled boards into a deterministic
/// temp directory and returns that network path.
pub fn materialize_network(
    network_path: &Path,
    elf_overrides: &HashMap<String, PathBuf>,
    disabled_boards: &HashSet<String>,
) -> Result<PathBuf, String> {
    let info = load_network_info(network_path)?;
    let enabled: Vec<&FilBoardInfo> = info
        .boards
        .iter()
        .filter(|board| !disabled_boards.contains(&board.name))
        .collect();
    if enabled.is_empty() {
        return Err("All FIL boards are disabled; enable at least one board".into());
    }
    let mut needed: HashMap<&str, &Path> = HashMap::new();
    for board in &enabled {
        if let Some(override_elf) = elf_overrides.get(&board.name) {
            if !override_elf.is_file() {
                return Err(format!(
                    "ELF override for board '{}' does not exist: {}",
                    board.name,
                    override_elf.display()
                ));
            }
            needed.insert(&board.name, override_elf.as_path());
        }
    }
    if needed.is_empty() && enabled.len() == info.boards.len() {
        return Ok(network_path.to_path_buf());
    }

    let dir = scratch_dir(network_path);
    std::fs::create_dir_all(&dir)
        .map_err(|error| format!("Failed to create {}: {error}", dir.display()))?;

    let mut board_paths = Vec::with_capacity(enabled.len());
    for board in &enabled {
        if let Some(override_elf) = needed.get(board.name.as_str()) {
            let mut board_json = read_json_file(&board.board_path)?;
            board_json["elf"] = serde_json::Value::String(override_elf.display().to_string());
            let out_path = dir.join(format!("board-{}.json", board.name));
            let content = serde_json::to_string_pretty(&board_json)
                .map_err(|error| format!("Failed to serialize board '{}': {error}", board.name))?;
            std::fs::write(&out_path, content)
                .map_err(|error| format!("Failed to write {}: {error}", out_path.display()))?;
            board_paths.push(out_path);
        } else {
            board_paths.push(board.board_path.clone());
        }
    }

    let mut network_json = read_json_file(network_path)?;
    network_json["boards"] = serde_json::Value::Array(
        board_paths
            .iter()
            .map(|path| serde_json::Value::String(path.display().to_string()))
            .collect(),
    );
    let out_network = dir.join("network.json");
    let content = serde_json::to_string_pretty(&network_json)
        .map_err(|error| format!("Failed to serialize network: {error}"))?;
    std::fs::write(&out_network, content)
        .map_err(|error| format!("Failed to write {}: {error}", out_network.display()))?;
    Ok(out_network)
}

/// Buses referenced by a board config's `can` attachments.
fn board_buses(board_json: &serde_json::Value) -> Vec<String> {
    board_json
        .get("can")
        .and_then(serde_json::Value::as_object)
        .map(|can| {
            can.values()
                .filter_map(|attachment| attachment.get("bus").and_then(serde_json::Value::as_str))
                .map(str::to_owned)
                .collect()
        })
        .unwrap_or_default()
}

/// Deterministic scratch directory for an arbitrary key (built-network spec).
fn scratch_dir_for(label: &str, key: &str) -> PathBuf {
    use std::collections::hash_map::DefaultHasher;
    use std::hash::{Hash, Hasher};
    let mut hasher = DefaultHasher::new();
    key.hash(&mut hasher);
    let safe: String = label
        .chars()
        .map(|c| if c.is_alphanumeric() { c } else { '_' })
        .take(32)
        .collect();
    std::env::temp_dir()
        .join("daqapp-fil")
        .join(format!("{safe}-{:016x}", hasher.finish()))
}

/// FDCAN instances FIL models, the valid set for synthesized attachments.
pub const FIL_CAN_INSTANCES: [&str; 3] = ["FDCAN1", "FDCAN2", "FDCAN3"];
/// ADC instances FIL models.
pub const FIL_ADC_INSTANCES: [&str; 4] = ["ADC1", "ADC2", "ADC3", "ADC4"];
/// GPIO ports FIL models (pins PA0 through PG15).
pub const FIL_GPIO_PORTS: [&str; 7] = [
    "GPIOA", "GPIOB", "GPIOC", "GPIOD", "GPIOE", "GPIOF", "GPIOG",
];

/// Locate the bundled STM32G474 MCU config next to a FIL executable
/// (`<exe>/../configs` for `fil/build/fil`, or a sibling `configs/` dir).
pub fn default_mcu_for_executable(executable: &Path) -> Option<PathBuf> {
    let dir = executable.parent()?;
    [
        dir.join("../configs/mcus/stm32g474retx.json"),
        dir.join("configs/mcus/stm32g474retx.json"),
    ]
    .into_iter()
    .map(|candidate| absolutize(Path::new("."), &candidate))
    .find(|candidate| candidate.is_file())
}

/// Effective MCU config: explicit pick when set, otherwise auto-located.
pub fn effective_mcu(board: &BuiltBoard, executable: Option<&Path>) -> Option<PathBuf> {
    if !board.mcu.as_os_str().is_empty() {
        return Some(board.mcu.clone());
    }
    executable.and_then(default_mcu_for_executable)
}

/// CAN instance names from a board JSON `can` section.
fn board_can_instances(board_json: &serde_json::Value) -> Vec<String> {
    board_json
        .get("can")
        .and_then(serde_json::Value::as_object)
        .map(|can| can.keys().cloned().collect())
        .unwrap_or_default()
}

/// Absolute MCU path referenced by a board JSON file, if any.
fn board_mcu_path(board_path: &Path, board_json: &serde_json::Value) -> Option<PathBuf> {
    let mcu = board_json.get("mcu").and_then(serde_json::Value::as_str)?;
    let board_dir = board_path.parent().unwrap_or_else(|| Path::new("."));
    Some(absolutize(board_dir, Path::new(mcu)))
}

/// Convert a legacy board-file entry to direct fields. Unresolvable entries
/// are returned unchanged for validation to report.
pub fn migrate_built_board(board: &BuiltBoard) -> BuiltBoard {
    if board.board.as_os_str().is_empty() {
        return board.clone();
    }
    let Ok(info) = load_board_info(&board.board) else {
        return board.clone();
    };
    let board_json = read_json_file(&board.board).unwrap_or(serde_json::Value::Null);
    let mut can_instances = board_can_instances(&board_json);
    if can_instances.is_empty() {
        can_instances.push(FIL_CAN_INSTANCES[0].into());
    }
    BuiltBoard {
        name: info.name,
        elf: board
            .elf_override
            .clone()
            .or_else(|| info.default_elf.clone())
            .unwrap_or_default(),
        mcu: board_mcu_path(&board.board, &board_json).unwrap_or_default(),
        can_instances,
        vector_base: board_json
            .get("vector_base")
            .and_then(serde_json::Value::as_str)
            .unwrap_or("")
            .into(),
        enabled: board.enabled,
        board: PathBuf::new(),
        elf_override: None,
    }
}

/// A board ready to be written: synthesized JSON plus its display name.
struct ResolvedBoard {
    name: String,
    board_json: serde_json::Value,
}

/// Resolve one enabled builder entry to board JSON. Direct entries are
/// synthesized from ELF plus a few fields (no board file needed).
fn resolve_built_board(
    board: &BuiltBoard,
    spec_bus: &str,
    executable: Option<&Path>,
) -> Result<ResolvedBoard, String> {
    debug_assert!(board.board.as_os_str().is_empty());
    if board.name.trim().is_empty() {
        return Err("A built board needs a name".into());
    }
    if !board.elf.is_file() {
        return Err(format!(
            "Board '{}' ELF does not exist: {}",
            board.name,
            board.elf.display()
        ));
    }
    let mcu = effective_mcu(board, executable).ok_or_else(|| {
        format!(
            "Board '{}' has no MCU config; pick one or point the FIL executable at a fil build",
            board.name
        )
    })?;
    if !mcu.is_file() {
        return Err(format!(
            "Board '{}' MCU config does not exist: {}",
            board.name,
            mcu.display()
        ));
    }
    let instances = if board.can_instances.is_empty() {
        vec![FIL_CAN_INSTANCES[0].to_owned()]
    } else {
        board.can_instances.clone()
    };
    for instance in &instances {
        if !FIL_CAN_INSTANCES.contains(&instance.as_str()) {
            return Err(format!(
                "Board '{}' has unknown CAN instance '{instance}'; expected one of {}",
                board.name,
                FIL_CAN_INSTANCES.join(", ")
            ));
        }
    }
    let mut can = serde_json::Map::new();
    for instance in instances {
        can.insert(instance, serde_json::json!({"bus": spec_bus}));
    }
    let mut board_json = serde_json::json!({
        "schema_version": 1,
        "name": board.name,
        "mcu": mcu.display().to_string(),
        "elf": board.elf.display().to_string(),
        "can": can,
    });
    if !board.vector_base.trim().is_empty() {
        board_json["vector_base"] = serde_json::Value::String(board.vector_base.clone());
    }
    Ok(ResolvedBoard {
        name: board.name.clone(),
        board_json,
    })
}

/// Resolve a legacy board-file entry: validate ELF, warn on bus mismatch,
/// and fold an ELF override into a patched copy of the board JSON.
fn resolve_legacy_board_entry(
    board: &BuiltBoard,
    spec_bus: &str,
    warnings: &mut Vec<String>,
) -> Result<ResolvedBoard, String> {
    if !board.board.is_file() {
        return Err(format!(
            "Board config does not exist: {}",
            board.board.display()
        ));
    }
    let info = load_board_info(&board.board)?;
    let board_json = read_json_file(&board.board)?;
    let buses = board_buses(&board_json);
    if !buses.is_empty() && !buses.contains(&spec_bus.to_owned()) {
        warnings.push(format!(
            "Board '{}' attaches to {} but the network bus is '{spec_bus}'",
            info.name,
            buses.join(", ")
        ));
    }
    let elf = match &board.elf_override {
        Some(elf) if !elf.is_file() => {
            return Err(format!(
                "ELF override for board '{}' does not exist: {}",
                info.name,
                elf.display()
            ));
        }
        elf @ Some(_) => elf.clone(),
        None => info.default_elf.clone(),
    };
    if elf.is_none() {
        return Err(format!(
            "Board '{}' has no firmware ELF; select one",
            info.name
        ));
    }
    let elf = elf.expect("checked");
    if !elf.is_file() {
        return Err(format!(
            "Board '{}' ELF does not exist: {}",
            info.name,
            elf.display()
        ));
    }
    let mut board_json = read_json_file(&board.board)?;
    board_json["elf"] = serde_json::Value::String(elf.display().to_string());
    Ok(ResolvedBoard {
        name: info.name,
        board_json,
    })
}

fn write_network_json(
    dest: &Path,
    spec: &BuiltNetwork,
    board_refs: &[String],
) -> Result<(), String> {
    let network_json = serde_json::json!({
        "schema_version": 1,
        "name": spec.name,
        "buses": {&spec.bus: {"type": "can", "bitrate": spec.bitrate}},
        "boards": board_refs,
    });
    let content = serde_json::to_string_pretty(&network_json)
        .map_err(|error| format!("Failed to serialize network: {error}"))?;
    std::fs::write(dest, content)
        .map_err(|error| format!("Failed to write {}: {error}", dest.display()))?;
    Ok(())
}

/// Build a runnable network from a widget-composed spec without hand-written
/// JSON. Boards are synthesized from ELF images plus a few fields; legacy
/// board-file entries keep working. Returns the generated network path plus
/// non-fatal warnings.
pub fn build_network(
    spec: &BuiltNetwork,
    executable: &Path,
) -> Result<(PathBuf, Vec<String>), String> {
    if spec.name.trim().is_empty() {
        return Err("Built network needs a name".into());
    }
    if spec.bus.trim().is_empty() || spec.bus.contains([':', '\n', '\r']) {
        return Err("Built network bus name is empty or invalid".into());
    }
    if spec.bitrate == 0 {
        return Err("Built network bitrate must be nonzero".into());
    }
    let enabled: Vec<&BuiltBoard> = spec.boards.iter().filter(|b| b.enabled).collect();
    if enabled.is_empty() {
        return Err("Add and enable at least one board".into());
    }
    let executable = executable.is_file().then_some(executable);
    let mut warnings = Vec::new();
    let mut resolved = Vec::with_capacity(enabled.len());
    let mut names = HashSet::new();
    for board in &enabled {
        let entry = if board.board.as_os_str().is_empty() {
            resolve_built_board(board, &spec.bus, executable)?
        } else {
            resolve_legacy_board_entry(board, &spec.bus, &mut warnings)?
        };
        if !names.insert(entry.name.clone()) {
            return Err(format!("Duplicate board name '{}'", entry.name));
        }
        resolved.push(entry);
    }

    let fingerprint = serde_json::to_string(spec).unwrap_or_default();
    let dir = scratch_dir_for(&spec.name, &fingerprint);
    std::fs::create_dir_all(&dir)
        .map_err(|error| format!("Failed to create {}: {error}", dir.display()))?;
    let mut board_refs = Vec::with_capacity(resolved.len());
    for entry in &resolved {
        let out_path = dir.join(format!("board-{}.json", entry.name));
        let content = serde_json::to_string_pretty(&entry.board_json)
            .map_err(|error| format!("Failed to serialize board '{}': {error}", entry.name))?;
        std::fs::write(&out_path, content)
            .map_err(|error| format!("Failed to write {}: {error}", out_path.display()))?;
        board_refs.push(out_path.display().to_string());
    }
    let out_network = dir.join("network.json");
    write_network_json(&out_network, spec, &board_refs)?;
    Ok((out_network, warnings))
}

/// Export a built network as reusable JSON files: the network file at
/// `dest` plus self-contained `board-<name>.json` siblings.
pub fn export_network(dest: &Path, spec: &BuiltNetwork, executable: &Path) -> Result<(), String> {
    let enabled: Vec<&BuiltBoard> = spec.boards.iter().filter(|b| b.enabled).collect();
    if enabled.is_empty() {
        return Err("Add and enable at least one board".into());
    }
    let parent = dest.parent().filter(|p| !p.as_os_str().is_empty());
    if let Some(parent) = parent {
        std::fs::create_dir_all(parent)
            .map_err(|error| format!("Failed to create {}: {error}", parent.display()))?;
    }
    let executable = executable.is_file().then_some(executable);
    let mut warnings = Vec::new();
    let mut board_refs = Vec::with_capacity(enabled.len());
    let mut names = HashSet::new();
    for board in &enabled {
        let entry = if board.board.as_os_str().is_empty() {
            resolve_built_board(board, &spec.bus, executable)?
        } else {
            resolve_legacy_board_entry(board, &spec.bus, &mut warnings)?
        };
        if !names.insert(entry.name.clone()) {
            return Err(format!("Duplicate board name '{}'", entry.name));
        }
        let safe_name: String = entry
            .name
            .chars()
            .map(|c| {
                if c.is_ascii_alphanumeric() || c == '-' || c == '_' {
                    c
                } else {
                    '_'
                }
            })
            .collect();
        let sibling = format!("board-{safe_name}.json");
        let out_path = dest
            .parent()
            .unwrap_or_else(|| Path::new("."))
            .join(&sibling);
        let content = serde_json::to_string_pretty(&entry.board_json)
            .map_err(|error| format!("Failed to serialize board '{}': {error}", entry.name))?;
        std::fs::write(&out_path, content)
            .map_err(|error| format!("Failed to write {}: {error}", out_path.display()))?;
        board_refs.push(sibling);
    }
    for warning in warnings {
        log::warn!("FIL export: {warning}");
    }
    write_network_json(dest, spec, &board_refs)
}
