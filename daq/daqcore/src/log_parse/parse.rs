// TODO: future: unified parser instead of bool flag for frames retention that is also
// streaming instead of loading all of the log files into memory at once.

use crate::{frame, log_parse, superdbc};
use std::{fs, io, mem, path};

#[derive(Debug)]
pub struct ParsedMessage {
    pub message_seq: u64,
    pub timestamp: u32,
    pub decoded: superdbc::decode::DecodedMessage,
    pub bus_name: String,
}

#[repr(C)]
#[derive(bytemuck::Pod, bytemuck::Zeroable, Copy, Clone)]
// based on definition of timestamped_frame_t in timestamped_frame.h in firmware repo
pub struct RawFrame {
    pub ticks_ms: u32,
    pub identity: u32,
    pub data: [u8; 8],
}

pub struct ParsedLogs {
    pub source_files: Vec<path::PathBuf>,
    pub parsed_msgs: Vec<ParsedMessage>,
    pub captured_frames: Vec<CapturedFrame>,
}

pub struct CapturedFrame {
    pub message_seq: u64,
    pub source_index: usize,
    pub source_record: u64,
    pub raw: RawFrame,
    pub decode_status: DecodeStatus,
}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum DecodeStatus {
    Ok,
    InvalidIdentity,
    UnknownBus,
    UnknownMessage,
    DecodeError,
}

impl DecodeStatus {
    pub fn as_str(&self) -> &'static str {
        match self {
            Self::Ok => "ok",
            Self::InvalidIdentity => "invalid_identity",
            Self::UnknownBus => "unknown_bus",
            Self::UnknownMessage => "unknown_message",
            Self::DecodeError => "decode_error",
        }
    }
}

pub fn parse_log_files_only_msgs(
    in_folder: &path::Path,
    database: &superdbc::database::Database,
) -> io::Result<Vec<ParsedMessage>> {
    Ok(parse_logs(in_folder, database, false)?.parsed_msgs)
}

pub fn parse_logs_with_frames(
    in_folder: &path::Path,
    database: &superdbc::database::Database,
) -> io::Result<ParsedLogs> {
    parse_logs(in_folder, database, true)
}

fn parse_logs(
    in_folder: &path::Path,
    database: &superdbc::database::Database,
    retain_frames: bool,
) -> io::Result<ParsedLogs> {
    let mut parsed_msgs = Vec::new();
    let mut captured_frames = Vec::new();
    let mut next_message_seq = 0;

    let entries = fs::read_dir(in_folder)?;
    let mut file_paths = entries
        .collect::<io::Result<Vec<_>>>()?
        .into_iter()
        .map(|entry| entry.path())
        .filter(|path| {
            path.is_file() && path.extension().and_then(|ext| ext.to_str()) == Some("log")
        })
        .collect::<Vec<_>>();
    file_paths.sort();
    for (source_index, path) in file_paths.iter().enumerate() {
        log::info!("Parsing log file: {}", path.display());
        let (parsed, captured, next_seq) = parse_log_file(
            path,
            database,
            source_index,
            next_message_seq,
            retain_frames,
        )?;
        parsed_msgs.extend(parsed);
        if let Some(captured) = captured {
            captured_frames.extend(captured);
        }
        next_message_seq = next_seq;
    }

    Ok(ParsedLogs {
        source_files: file_paths,
        parsed_msgs,
        captured_frames,
    })
}

/// Return: parsed messages, captured frames, next message sequence number after this file
fn parse_log_file(
    in_file: &path::Path,
    database: &superdbc::database::Database,
    source_index: usize,
    starting_next_message_seq: u64,
    retain_frames: bool,
) -> io::Result<(Vec<ParsedMessage>, Option<Vec<CapturedFrame>>, u64)> {
    let mut content = fs::read(in_file)?;
    let frame_size = mem::size_of::<RawFrame>();

    // add padding zeroes if content length is not multiple of raw frame size
    let mut added_padding = false;
    if !content.len().is_multiple_of(frame_size) {
        log::warn!(
            "Log file {} has length {} which is not a multiple of frame size {}. Possibly due to outdated log format.",
            in_file.display(),
            content.len(),
            frame_size
        );
        let padding = frame_size - content.len() % frame_size;
        content.resize(content.len() + padding, 0);
        added_padding = true;
    }

    let frames: Vec<RawFrame> = content
        .chunks_exact(frame_size)
        .map(bytemuck::pod_read_unaligned)
        .collect();

    let mut parsed_msgs = Vec::with_capacity(frames.len());
    let mut captured_frames = if retain_frames {
        Some(Vec::with_capacity(frames.len()))
    } else {
        None
    };
    let mut next_message_seq = starting_next_message_seq;

    for (i, frame) in frames.iter().enumerate() {
        if added_padding && i == frames.len() - 1 {
            log::info!(
                "Skipping last frame in {} due to padding",
                in_file.display()
            );
            break;
        }

        let message_seq = next_message_seq;
        next_message_seq += 1;
        let decode_status = match decode_frame(in_file, frame, database, message_seq) {
            Ok(message) => {
                parsed_msgs.push(message);
                DecodeStatus::Ok
            }
            Err(status) => status,
        };
        if let Some(captured) = captured_frames.as_mut() {
            captured.push(CapturedFrame {
                message_seq,
                source_index,
                source_record: i as u64,
                raw: *frame,
                decode_status,
            });
        }
    }
    Ok((parsed_msgs, captured_frames, next_message_seq))
}

fn decode_frame(
    in_file: &path::Path,
    raw: &RawFrame,
    database: &superdbc::database::Database,
    message_seq: u64,
) -> Result<ParsedMessage, DecodeStatus> {
    let extended = raw.identity & log_parse::consts::IS_EID_MASK != 0;
    let transport_flags = log_parse::consts::IS_EID_MASK | log_parse::consts::BUS_ID_MASK;
    let raw_id = raw.identity & !transport_flags;
    let identity = match frame::CanIdentity::new(raw_id, extended) {
        Ok(identity) => identity,
        Err(error) => {
            log::error!("Invalid CAN identity in {}: {error}", in_file.display());
            return Err(DecodeStatus::InvalidIdentity);
        }
    };
    let bus_id = if (raw.identity & log_parse::consts::BUS_ID_MASK) != 0 {
        1
    } else {
        0
    };
    let Some(bus) = database.bus_by_id(bus_id) else {
        log::error!("Unknown bus {bus_id} in {}", in_file.display());
        return Err(DecodeStatus::UnknownBus);
    };
    match bus.decode(identity, &raw.data) {
        Ok(decoded) => Ok(ParsedMessage {
            message_seq,
            timestamp: raw.ticks_ms,
            decoded,
            bus_name: bus.name().to_owned(),
        }),
        Err(error) => {
            log::error!(
                "Failed to decode message at {} ms with CAN ID {:X} and data {:?} on bus {}",
                raw.ticks_ms,
                identity.raw_id(),
                raw.data,
                bus_id
            );
            Err(match error {
                superdbc::error::DecodeError::UnknownMessage { .. } => DecodeStatus::UnknownMessage,
                _ => DecodeStatus::DecodeError,
            })
        }
    }
}
