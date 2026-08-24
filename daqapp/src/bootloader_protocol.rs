//! Firmware package loading and validation.
//!
//! Manifests, image paths, sizes, CRCs, and board CAN IDs are checked before the
//! CAN thread receives a package.

use crate::connection::CanBus;
use serde::Deserialize;
use std::{
    collections::HashSet,
    path::{Component, Path, PathBuf},
};

pub const PACKAGE_FORMAT: &str = "per-firmware-package-v1";

/// A validated application payload and its bootloader CAN IDs.
#[derive(Clone, Debug)]
pub struct FirmwareImage {
    pub name: String,
    pub bus: CanBus,
    pub bytes: Vec<u8>,
    pub crc32: u32,
    pub start_id: u32,
    pub crc_id: u32,
    pub jump_id: u32,
    pub data_id: u32,
    pub response_id: u32,
}

/// A complete, validated set of board images in manifest order.
#[derive(Clone, Debug)]
pub struct FirmwarePackage {
    pub images: Vec<FirmwareImage>,
}

#[derive(Debug)]
struct PackageStorage(PathBuf);

impl Drop for PackageStorage {
    fn drop(&mut self) {
        let _ = std::fs::remove_dir_all(&self.0);
    }
}

#[derive(Debug, Deserialize)]
struct Manifest {
    format: String,
    protocol_version: u32,
    crc_algorithm: String,
    boards: Vec<ManifestBoard>,
}

#[derive(Debug, Deserialize)]
struct ManifestBoard {
    name: String,
    binary: String,
    size_bytes: usize,
    crc32: String,
    application_address: String,
    can_bus: String,
    start_id: String,
    crc_id: String,
    jump_id: String,
    data_id: String,
    response_id: String,
}

impl FirmwarePackage {
    /// Load a manifest or `.tar.gz`, validating its board set, paths, sizes,
    /// CRCs, application address, and CAN IDs before returning image bytes.
    pub fn load(path: impl Into<PathBuf>) -> Result<Self, String> {
        let path = path.into();
        let (manifest_path, storage) = if path
            .file_name()
            .and_then(|name| name.to_str())
            .is_some_and(|name| name.ends_with(".tar.gz"))
        {
            let nonce = std::time::SystemTime::now()
                .duration_since(std::time::UNIX_EPOCH)
                .map_err(|error| format!("cannot create package nonce: {error}"))?
                .as_nanos();
            let root = std::env::temp_dir()
                .join(format!("per-daqapp-package-{}-{nonce}", std::process::id()));
            std::fs::create_dir(&root)
                .map_err(|error| format!("cannot create package extraction directory: {error}"))?;
            let storage = PackageStorage(root.clone());
            validate_archive(&path)?;
            let extraction = std::process::Command::new("tar")
                .args(["-xzf"])
                .arg(&path)
                .args(["-C"])
                .arg(&root)
                .output()
                .map_err(|error| {
                    format!("cannot run tar to extract {}: {error}", path.display())
                })?;
            if !extraction.status.success() {
                let _ = std::fs::remove_dir_all(&root);
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
        let manifest_text = std::fs::read_to_string(&manifest_path)
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
            .ok_or_else(|| "manifest has no parent directory".to_string())?;
        let mut images = Vec::with_capacity(manifest.boards.len());
        for board in manifest.boards {
            if images
                .iter()
                .any(|image: &FirmwareImage| image.name == board.name)
            {
                return Err(format!("duplicate board {}", board.name));
            }
            let bus = parse_bus(&board.can_bus)?;
            if board.application_address.to_ascii_uppercase() != "0X08008000" {
                return Err(format!(
                    "board {} has an invalid application address",
                    board.name
                ));
            }

            let relative_binary = Path::new(&board.binary);
            // Reject absolute/parent traversal and final-entry symlinks before reading.
            if relative_binary.is_absolute()
                || relative_binary
                    .components()
                    .any(|component| matches!(component, Component::ParentDir))
            {
                return Err(format!("unsafe binary path for board {}", board.name));
            }
            let binary_path = secure_file_path(&root.join(relative_binary), "binary")
                .map_err(|error| format!("unsafe binary path for {}: {error}", board.name))?;
            let metadata = std::fs::symlink_metadata(&binary_path)
                .map_err(|error| format!("cannot inspect {}: {error}", binary_path.display()))?;
            if metadata.len() > 160 * 1024 {
                return Err(format!("invalid image file for {}", board.name));
            }
            let bytes = std::fs::read(&binary_path)
                .map_err(|error| format!("cannot read {}: {error}", binary_path.display()))?;
            if bytes.len() != board.size_bytes {
                return Err(format!(
                    "{} has {} bytes, manifest declares {}",
                    board.name,
                    bytes.len(),
                    board.size_bytes
                ));
            }
            if bytes.is_empty() || bytes.len() % 4 != 0 || bytes.len() > 160 * 1024 {
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

            let start_id = parse_hex_u32(&board.start_id, "start_id")?;
            let crc_id = parse_hex_u32(&board.crc_id, "crc_id")?;
            let jump_id = parse_hex_u32(&board.jump_id, "jump_id")?;
            let data_id = parse_hex_u32(&board.data_id, "data_id")?;
            let response_id = parse_hex_u32(&board.response_id, "response_id")?;
            if Some((start_id, crc_id, jump_id, data_id, response_id)) != expected_ids(&board.name)
            {
                return Err(format!(
                    "board {} has unexpected bootloader IDs",
                    board.name
                ));
            }

            images.push(FirmwareImage {
                name: board.name,
                bus,
                bytes,
                crc32: expected_crc,
                start_id,
                crc_id,
                jump_id,
                data_id,
                response_id,
            });
        }

        drop(storage);
        Ok(Self { images })
    }
}

fn validate_archive(path: &Path) -> Result<(), String> {
    let listing = std::process::Command::new("tar")
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

    let names = std::process::Command::new("tar")
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
        let member = Path::new(name);
        if member.is_absolute()
            || member
                .components()
                .any(|component| matches!(component, Component::ParentDir))
        {
            return Err(format!("firmware archive contains unsafe path {name:?}"));
        }
    }
    Ok(())
}

fn secure_file_path(path: &Path, kind: &str) -> Result<PathBuf, String> {
    let mut current = PathBuf::new();
    for component in path.components() {
        match component {
            Component::Prefix(prefix) => current.push(prefix.as_os_str()),
            Component::RootDir => current.push(component.as_os_str()),
            Component::CurDir => {}
            Component::ParentDir => {
                return Err("path contains a parent traversal".to_string());
            }
            Component::Normal(name) => {
                current.push(name);
                let metadata = std::fs::symlink_metadata(&current)
                    .map_err(|error| format!("cannot inspect {}: {error}", current.display()))?;
                if metadata.file_type().is_symlink() {
                    return Err(format!("{kind} path contains a symlink component"));
                }
            }
        }
    }
    let metadata = std::fs::symlink_metadata(path)
        .map_err(|error| format!("cannot inspect {}: {error}", path.display()))?;
    if !metadata.file_type().is_file() {
        return Err(format!("{kind} is not a regular file"));
    }
    Ok(path.to_path_buf())
}

fn expected_ids(name: &str) -> Option<(u32, u32, u32, u32, u32)> {
    match name {
        "main_module" => Some((0x180, 0x192, 0x198, 0x181, 0x182)),
        "dashboard" => Some((0x183, 0x193, 0x199, 0x184, 0x185)),
        "torque_vector" => Some((0x186, 0x194, 0x19A, 0x187, 0x188)),
        "a_box" => Some((0x189, 0x195, 0x19B, 0x18A, 0x18B)),
        "front_driveline" => Some((0x18C, 0x196, 0x19C, 0x18D, 0x18E)),
        "rear_driveline" => Some((0x18F, 0x197, 0x19D, 0x190, 0x191)),
        _ => None,
    }
}

fn parse_bus(value: &str) -> Result<CanBus, String> {
    match value.to_ascii_uppercase().as_str() {
        "VCAN" => Ok(CanBus::Vcan),
        "SCAN" => Ok(CanBus::Scan),
        other => Err(format!("invalid CAN bus {other:?}; expected VCAN or SCAN")),
    }
}

impl FirmwarePackage {
    /// Return the bus shared by all images, if this package selection is single-bus.
    pub fn bus(&self) -> Option<CanBus> {
        let first = self.images.first()?.bus;
        self.images
            .iter()
            .all(|image| image.bus == first)
            .then_some(first)
    }
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
    debug_assert_eq!(data.len() % 4, 0);
    let mut crc = 0xFFFF_FFFFu32;
    for word_bytes in data.chunks_exact(4) {
        crc ^= u32::from_le_bytes(word_bytes.try_into().expect("four-byte chunk"));
        for _ in 0..8 {
            crc = (crc << 4) ^ LUT[(crc >> 28) as usize];
        }
    }
    crc
}
