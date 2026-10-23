use crate::{frame, log_parse, superdbc};
use std::{fs, io, mem, path};

#[derive(Debug)]
pub struct ParsedMessage {
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

pub fn parse_log_files(
    in_folder: &path::Path,
    database: &superdbc::database::Database,
) -> io::Result<Vec<ParsedMessage>> {
    let mut all_parsed = Vec::new();
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
    for path in file_paths {
        log::info!("Parsing log file: {}", path.display());
        let parsed = parse_log_file(&path, database)?;
        all_parsed.extend(parsed);
    }

    Ok(all_parsed)
}

fn parse_log_file(
    in_file: &path::Path,
    database: &superdbc::database::Database,
) -> io::Result<Vec<ParsedMessage>> {
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
    let mut parsed = Vec::with_capacity(frames.len());

    for (i, frame) in frames.iter().enumerate() {
        if added_padding && i == frames.len() - 1 {
            log::info!(
                "Skipping last frame in {} due to padding",
                in_file.display()
            );
            break;
        }

        let extended = frame.identity & log_parse::consts::IS_EID_MASK != 0;
        let transport_flags = log_parse::consts::IS_EID_MASK | log_parse::consts::BUS_ID_MASK;
        let raw_id = frame.identity & !transport_flags;
        let identity = match frame::CanIdentity::new(raw_id, extended) {
            Ok(identity) => identity,
            Err(error) => {
                log::error!("Invalid CAN identity in {}: {error}", in_file.display());
                continue;
            }
        };

        let bus_id = ((frame.identity & log_parse::consts::BUS_ID_MASK) >> log_parse::consts::BUS_ID_OFFSET) as u8;

        let Some(bus) = database.bus_by_id(bus_id) else {
            log::error!("Unknown bus {bus_id} in {}", in_file.display());
            continue;
        };

        if let Ok(decoded) = bus.decode(identity, &frame.data) {
            parsed.push(ParsedMessage {
                timestamp: frame.ticks_ms,
                decoded,
                bus_name: bus.name().to_owned(),
            });
        } else {
            log::error!(
                "Failed to decode message at {} ms with CAN ID {:X} and data {:?} on bus {}",
                frame.ticks_ms,
                identity.raw_id(),
                frame.data,
                bus_id
            );
        }
    }
    Ok(parsed)
}

pub fn chunk_parsed(parsed: Vec<ParsedMessage>) -> Vec<Vec<ParsedMessage>> {
    let mut chunks = Vec::new();
    let mut current_chunk = Vec::new();
    let mut last_timestamp = None;

    for msg in parsed {
        if let Some(last_ts) = last_timestamp
            && (msg.timestamp < last_ts || msg.timestamp - last_ts > log_parse::consts::MAX_JUMP_MS)
            && !current_chunk.is_empty()
        {
            chunks.push(current_chunk);
            current_chunk = Vec::new();
        }
        last_timestamp = Some(msg.timestamp);
        current_chunk.push(msg);
    }

    if !current_chunk.is_empty() {
        chunks.push(current_chunk);
    }

    // Sort messages within each chunk by timestamp
    for chunk in &mut chunks {
        chunk.sort_by_key(|m| m.timestamp);
    }

    chunks
}
