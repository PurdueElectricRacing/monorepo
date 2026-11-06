use std::{collections::HashSet, env, fs, path, process, time};

pub const PACKAGE_FORMAT: &str = "per-firmware-package-v1";
/// Start of the single application slot in the STM32G474 flash map.
pub const APPLICATION_ADDRESS: u32 = 0x0800_8000;
/// Maximum application payload accepted by the bootloader and package loader.
pub const APPLICATION_SLOT_SIZE: usize = 480 * 1024;
pub const BOOTLOADER_ADDRESS: u32 = 0x0800_0000;
pub const BOOTLOADER_SIZE: usize = 16 * 1024;

#[derive(Clone, Debug)]
pub struct FirmwareImage {
    pub name: String,
    pub can_bus: String,
    pub git_hash: Option<u32>,
    pub bytes: Vec<u8>,
    pub address: u32,
    pub crc32: u32,
    pub start_id: u32,
    pub crc_id: u32,
    pub jump_id: u32,
    pub data_id: u32,
    pub response_id: u32,
}

#[derive(Clone, Debug)]
pub struct FirmwarePackage {
    pub images: Vec<FirmwareImage>,
    pub bootloader_images: Vec<FirmwareImage>,
}

#[derive(Debug)]
struct PackageStorage(path::PathBuf);

impl Drop for PackageStorage {
    fn drop(&mut self) {
        let _ = fs::remove_dir_all(&self.0);
    }
}

#[derive(Debug, serde::Deserialize)]
struct Manifest {
    format: String,
    protocol_version: u32,
    crc_algorithm: String,
    boards: Vec<ManifestBoard>,
    #[serde(default)]
    bootloaders: Vec<ManifestBootloader>,
}

#[derive(Debug, serde::Deserialize)]
struct ManifestBootloader {
    name: String,
    can_bus: String,
    git_hash: String,
    binary: String,
    size_bytes: usize,
    crc32: String,
    bootloader_address: String,
    start_id: String,
    crc_id: String,
    jump_id: String,
    data_id: String,
    response_id: String,
}

#[derive(Debug, serde::Deserialize)]
struct ManifestBoard {
    name: String,
    #[serde(default)]
    can_bus: Option<String>,
    #[serde(default)]
    git_hash: Option<String>,
    binary: String,
    size_bytes: usize,
    crc32: String,
    application_address: String,
    start_id: String,
    crc_id: String,
    jump_id: String,
    data_id: String,
    response_id: String,
}

impl FirmwarePackage {
    /// Load a manifest or `.tar.gz`, validating its board set, paths, sizes,
    /// CRCs, application address, and CAN IDs before returning image bytes.
    pub fn load(path: impl Into<path::PathBuf>) -> Result<Self, String> {
        let path = path.into();
        let (manifest_path, storage) = if path
            .file_name()
            .and_then(|name| name.to_str())
            .is_some_and(|name| name.ends_with(".tar.gz"))
        {
            let nonce = time::SystemTime::now()
                .duration_since(time::UNIX_EPOCH)
                .map_err(|error| format!("cannot create package nonce: {error}"))?
                .as_nanos();
            let root =
                env::temp_dir().join(format!("per-daqapp-package-{}-{nonce}", process::id()));
            fs::create_dir(&root)
                .map_err(|error| format!("cannot create package extraction directory: {error}"))?;
            let storage = PackageStorage(root.clone());
            validate_archive(&path)?;
            let extraction = process::Command::new("tar")
                .args(["-xzf"])
                .arg(&path)
                .args(["-C"])
                .arg(&root)
                .output()
                .map_err(|error| {
                    format!("cannot run tar to extract {}: {error}", path.display())
                })?;
            if !extraction.status.success() {
                let _ = fs::remove_dir_all(&root);
                return Err(format!(
                    "cannot extract {}: {}",
                    path.display(),
                    String::from_utf8_lossy(&extraction.stderr).trim()
                ));
            }
            (root.join("manifest.json"), Some(storage))
        } else {
            (path, None)
        };

        let manifest_path = secure_file_path(&manifest_path, "manifest")?;
        let manifest_text = fs::read_to_string(&manifest_path)
            .map_err(|error| format!("cannot read {}: {error}", manifest_path.display()))?;
        let manifest: Manifest = serde_json::from_str(&manifest_text)
            .map_err(|error| format!("invalid firmware manifest: {error}"))?;

        if manifest.format != PACKAGE_FORMAT {
            return Err(format!(
                "unsupported package format {:?}; expected {:?}",
                manifest.format, PACKAGE_FORMAT
            ));
        }

        if manifest.protocol_version != 1 {
            return Err(format!(
                "unsupported bootloader protocol version {}",
                manifest.protocol_version
            ));
        }

        if manifest.crc_algorithm != "STM32_CRC32_MPEG2_WORD_LE" {
            return Err(format!(
                "unsupported CRC algorithm {:?}",
                manifest.crc_algorithm
            ));
        }
        const BOARDS: [&str; 6] = [
            "main_module",
            "dashboard",
            "torque_vector",
            "a_box",
            "front_driveline",
            "rear_driveline",
        ];
        let board_names: HashSet<_> = manifest
            .boards
            .iter()
            .map(|board| board.name.as_str())
            .collect();
        if board_names.len() != BOARDS.len()
            || !BOARDS.iter().all(|name| board_names.contains(name))
        {
            return Err("firmware package must contain all six supported boards".to_string());
        }

        let root = manifest_path
            .parent()
            .ok_or_else(|| "manifest has no parent directory".to_string())?
            .to_path_buf();
        let mut images = Vec::with_capacity(manifest.boards.len());
        let mut protocol_ids = HashSet::new();
        for board in manifest.boards {
            if images
                .iter()
                .any(|image: &FirmwareImage| image.name == board.name)
            {
                return Err(format!("duplicate board {}", board.name));
            }

            if parse_hex_u32(&board.application_address, "application_address")?
                != APPLICATION_ADDRESS
            {
                return Err(format!(
                    "board {} has an invalid application address",
                    board.name
                ));
            }

            let relative_binary = path::Path::new(&board.binary);
            // Reject absolute/parent traversal and final-entry symlinks before reading.
            if relative_binary.is_absolute()
                || relative_binary
                    .components()
                    .any(|component| matches!(component, path::Component::ParentDir))
            {
                return Err(format!("unsafe binary path for board {}", board.name));
            }

            let binary_path = secure_file_path(&root.join(relative_binary), "binary")
                .map_err(|error| format!("unsafe binary path for {}: {error}", board.name))?;
            if !binary_path.starts_with(&root) {
                return Err(format!(
                    "binary path escapes package root for {}",
                    board.name
                ));
            }

            let metadata = fs::symlink_metadata(&binary_path)
                .map_err(|error| format!("cannot inspect {}: {error}", binary_path.display()))?;
            if metadata.len() > APPLICATION_SLOT_SIZE as u64 {
                return Err(format!("invalid image file for {}", board.name));
            }

            let bytes = fs::read(&binary_path)
                .map_err(|error| format!("cannot read {}: {error}", binary_path.display()))?;
            if bytes.len() != board.size_bytes {
                return Err(format!(
                    "{} has {} bytes, manifest declares {}",
                    board.name,
                    bytes.len(),
                    board.size_bytes
                ));
            }

            if !valid_application_size(bytes.len()) {
                return Err(format!("invalid image size for {}", board.name));
            }

            let expected_crc = parse_hex_u32(&board.crc32, "crc32")?;
            let actual_crc = crc32_words(&bytes);
            if actual_crc != expected_crc {
                return Err(format!(
                    "CRC mismatch for {}: manifest 0x{expected_crc:08X}, calculated 0x{actual_crc:08X}",
                    board.name
                ));
            }

            let start_id = parse_standard_can_id(&board.start_id, "start_id")?;
            let crc_id = parse_standard_can_id(&board.crc_id, "crc_id")?;
            let jump_id = parse_standard_can_id(&board.jump_id, "jump_id")?;
            let data_id = parse_standard_can_id(&board.data_id, "data_id")?;
            let response_id = parse_standard_can_id(&board.response_id, "response_id")?;
            // Older application-only manifests omitted the per-board bus.
            let can_bus = board.can_bus.unwrap_or_else(|| match board.name.as_str() {
                "front_driveline" | "rear_driveline" => "SCAN".to_owned(),
                _ => "VCAN".to_owned(),
            });
            if can_bus.trim().is_empty() {
                return Err(format!("board {} has an empty CAN bus", board.name));
            }
            if [start_id, crc_id, jump_id, data_id, response_id]
                .into_iter()
                .any(|id| !protocol_ids.insert((can_bus.clone(), id)))
            {
                return Err(format!("board {} reuses a bootloader CAN ID", board.name));
            }

            images.push(FirmwareImage {
                name: board.name,
                can_bus,
                git_hash: board
                    .git_hash
                    .as_deref()
                    .map(|hash| parse_hex_u32(hash, "git_hash"))
                    .transpose()?,
                bytes,
                address: APPLICATION_ADDRESS,
                crc32: expected_crc,
                start_id,
                crc_id,
                jump_id,
                data_id,
                response_id,
            });
        }

        let mut bootloader_images = Vec::with_capacity(manifest.bootloaders.len());
        let mut bootloader_names = HashSet::new();
        for bootloader in manifest.bootloaders {
            if !bootloader_names.insert(bootloader.name.clone()) {
                return Err(format!("duplicate bootloader target {}", bootloader.name));
            }
            if !BOARDS.contains(&bootloader.name.as_str()) {
                return Err(format!("unsupported bootloader target {}", bootloader.name));
            }
            if parse_hex_u32(&bootloader.bootloader_address, "bootloader_address")?
                != BOOTLOADER_ADDRESS
            {
                return Err(format!(
                    "bootloader {} has an invalid bootloader address",
                    bootloader.name
                ));
            }
            let relative_binary = path::Path::new(&bootloader.binary);
            if relative_binary.is_absolute()
                || relative_binary
                    .components()
                    .any(|component| matches!(component, path::Component::ParentDir))
            {
                return Err(format!(
                    "unsafe binary path for bootloader {}",
                    bootloader.name
                ));
            }
            let binary_path = secure_file_path(&root.join(relative_binary), "binary")
                .map_err(|error| format!("unsafe binary path for {}: {error}", bootloader.name))?;
            if !binary_path.starts_with(&root) {
                return Err(format!(
                    "binary path escapes package root for {}",
                    bootloader.name
                ));
            }
            let bytes = fs::read(&binary_path)
                .map_err(|error| format!("cannot read {}: {error}", binary_path.display()))?;
            if bytes.len() != bootloader.size_bytes
                || bytes.len() != BOOTLOADER_SIZE
                || !bytes.len().is_multiple_of(4)
            {
                return Err(format!(
                    "invalid bootloader image size for {} (expected {BOOTLOADER_SIZE} bytes)",
                    bootloader.name
                ));
            }
            let expected_crc = parse_hex_u32(&bootloader.crc32, "crc32")?;
            if crc32_words(&bytes) != expected_crc {
                return Err(format!("CRC mismatch for bootloader {}", bootloader.name));
            }
            let ids = [
                parse_standard_can_id(&bootloader.start_id, "start_id")?,
                parse_standard_can_id(&bootloader.crc_id, "crc_id")?,
                parse_standard_can_id(&bootloader.jump_id, "jump_id")?,
                parse_standard_can_id(&bootloader.data_id, "data_id")?,
                parse_standard_can_id(&bootloader.response_id, "response_id")?,
            ];
            if !images
                .iter()
                .any(|image| image.name == bootloader.name && image.can_bus == bootloader.can_bus)
            {
                return Err(format!(
                    "bootloader {} must use the same CAN bus as its application",
                    bootloader.name
                ));
            }
            if ids
                .into_iter()
                .any(|id| !protocol_ids.insert((bootloader.can_bus.clone(), id)))
            {
                return Err(format!("bootloader {} reuses a CAN ID", bootloader.name));
            }
            bootloader_images.push(FirmwareImage {
                name: bootloader.name,
                can_bus: bootloader.can_bus,
                git_hash: Some(parse_hex_u32(&bootloader.git_hash, "git_hash")?),
                bytes,
                address: BOOTLOADER_ADDRESS,
                crc32: expected_crc,
                start_id: ids[0],
                crc_id: ids[1],
                jump_id: ids[2],
                data_id: ids[3],
                response_id: ids[4],
            });
        }

        drop(storage);
        Ok(Self {
            images,
            bootloader_images,
        })
    }
}

fn validate_archive(path: &path::Path) -> Result<(), String> {
    let listing = process::Command::new("tar")
        .args(["-tvzf"])
        .arg(path)
        .output()
        .map_err(|error| format!("cannot inspect archive {}: {error}", path.display()))?;
    if !listing.status.success() {
        return Err(format!(
            "cannot inspect archive {}: {}",
            path.display(),
            String::from_utf8_lossy(&listing.stderr).trim()
        ));
    }

    if String::from_utf8_lossy(&listing.stdout)
        .lines()
        .any(|line| !matches!(line.as_bytes().first(), Some(b'd') | Some(b'-')))
    {
        return Err("firmware archive contains a symlink or special file".to_string());
    }

    let names = process::Command::new("tar")
        .args(["-tzf"])
        .arg(path)
        .output()
        .map_err(|error| format!("cannot inspect archive {}: {error}", path.display()))?;
    if !names.status.success() {
        return Err(format!(
            "cannot inspect archive {}: {}",
            path.display(),
            String::from_utf8_lossy(&names.stderr).trim()
        ));
    }

    for name in String::from_utf8_lossy(&names.stdout).lines() {
        let member = path::Path::new(name);
        if member.is_absolute()
            || member
                .components()
                .any(|component| matches!(component, path::Component::ParentDir))
        {
            return Err(format!("firmware archive contains unsafe path {name:?}"));
        }
    }
    Ok(())
}

fn secure_file_path(path: &path::Path, kind: &str) -> Result<path::PathBuf, String> {
    if path
        .components()
        .any(|component| matches!(component, path::Component::ParentDir))
    {
        return Err("path contains a parent traversal".to_string());
    }

    // Reject a symlink as the selected file itself, but permit canonical system
    // path aliases such as macOS `/var` -> `/private/var` in ancestor directories.
    let metadata = fs::symlink_metadata(path)
        .map_err(|error| format!("cannot inspect {}: {error}", path.display()))?;
    if metadata.file_type().is_symlink() {
        return Err(format!("{kind} is a symlink"));
    }

    if !metadata.file_type().is_file() {
        return Err(format!("{kind} is not a regular file"));
    }

    path.canonicalize()
        .map_err(|error| format!("cannot resolve {}: {error}", path.display()))
}

fn valid_application_size(size_bytes: usize) -> bool {
    size_bytes != 0 && size_bytes.is_multiple_of(4) && size_bytes <= APPLICATION_SLOT_SIZE
}

fn parse_standard_can_id(value: &str, field: &str) -> Result<u32, String> {
    let id = parse_hex_u32(value, field)?;
    if id > 0x7ff {
        return Err(format!("{field} value {value:?} is not a standard CAN ID"));
    }
    Ok(id)
}

fn parse_hex_u32(value: &str, field: &str) -> Result<u32, String> {
    let without_prefix = value
        .strip_prefix("0x")
        .or_else(|| value.strip_prefix("0X"))
        .unwrap_or(value);
    u32::from_str_radix(without_prefix, 16)
        .map_err(|error| format!("invalid {field} value {value:?}: {error}"))
}

/// Compute the target's CRC-32/MPEG-2 over little-endian words.
pub fn crc32_words(data: &[u8]) -> u32 {
    const LUT: [u32; 16] = [
        0x00000000, 0x04C11DB7, 0x09823B6E, 0x0D4326D9, 0x130476DC, 0x17C56B6B, 0x1A864DB2,
        0x1E475005, 0x2608EDB8, 0x22C9F00F, 0x2F8AD6D6, 0x2B4BCB61, 0x350C9B64, 0x31CD86D3,
        0x3C8EA00A, 0x384FBDBD,
    ];
    let (words, remainder) = data.as_chunks::<4>();
    debug_assert!(remainder.is_empty());
    let mut crc = 0xFFFF_FFFFu32;
    for word_bytes in words {
        crc ^= u32::from_le_bytes(*word_bytes);
        for _ in 0..8 {
            crc = (crc << 4) ^ LUT[(crc >> 28) as usize];
        }
    }
    crc
}
