use crate::app::ParserInfo;
use crate::can;
use crate::{daq_log_parse::consts, util};
use bytemuck::{Pod, Zeroable};

#[derive(Debug)]
pub struct ParsedMessage {
    pub timestamp: u32,
    pub decoded: can_decode::DecodedMessage,
    pub bus_name: String,
}

#[derive(Debug, Default, Clone)]
pub struct FileParseStats {
    pub file_name: String,
    pub total_frames: usize,
    pub parsed_frames: usize,
    pub failed_frames: usize,
    pub failed_by_bus: std::collections::BTreeMap<String, usize>,
    pub failed_can_ids: std::collections::BTreeMap<u32, usize>,
}

impl FileParseStats {
    pub fn new(file_name: impl AsRef<std::path::Path>) -> Self {
        Self {
            file_name: file_name.as_ref().to_string_lossy().to_string(),
            ..Default::default()
        }
    }

    fn record_failure(&mut self, bus_name: &str, can_id: u32) {
        self.failed_frames += 1;
        *self.failed_by_bus.entry(bus_name.to_string()).or_insert(0) += 1;
        *self.failed_can_ids.entry(can_id).or_insert(0) += 1;
    }

    pub fn summary(&self) -> String {
        let bus_summary = if self.failed_by_bus.is_empty() {
            "none".to_string()
        } else {
            self.failed_by_bus
                .iter()
                .map(|(bus, count)| format!("{bus}={count}"))
                .collect::<Vec<_>>()
                .join(", ")
        };

        let can_id_summary = if self.failed_can_ids.is_empty() {
            "none".to_string()
        } else {
            self.failed_can_ids
                .iter()
                .map(|(id, count)| format!("0x{id:08X}={count}"))
                .collect::<Vec<_>>()
                .join(", ")
        };

        if self.failed_frames == 0 {
            format!(
                "- {}: {} parsed, 0 failed / {} total",
                self.file_name, self.parsed_frames, self.total_frames,
            )
        } else {
            format!(
                "- {}: {} parsed, {} failed / {} total | buses: {} | CAN IDs: {}",
                self.file_name,
                self.parsed_frames,
                self.failed_frames,
                self.total_frames,
                bus_summary,
                can_id_summary,
            )
        }
    }
}

#[derive(Debug, Default, Clone)]
pub struct LogParseStats {
    pub files: Vec<FileParseStats>,
    pub total_parsed: usize,
    pub total_failed: usize,
}

impl LogParseStats {
    pub fn summary(&self) -> String {
        if self.files.is_empty() {
            return "No log files found.".to_string();
        }

        if self.total_failed == 0 {
            return format!(
                "Total parsed: {}\nNo decode failures across {} file(s).",
                self.total_parsed,
                self.files.len(),
            );
        }

        let file_summaries = self
            .files
            .iter()
            .filter(|file| file.failed_frames > 0)
            .map(FileParseStats::summary)
            .collect::<Vec<_>>()
            .join("\n");

        format!(
            "Total parsed: {}\nTotal failed: {}\nFailed files:\n{}",
            self.total_parsed, self.total_failed, file_summaries,
        )
    }
}

#[repr(C)]
#[derive(Pod, Zeroable, Copy, Clone)]
// based on definition of timestamped_frame_t in timestamped_frame.h in firmware repo
pub struct RawFrame {
    pub ticks_ms: u32,
    pub identity: u32,
    pub data: [u8; 8],
}

pub fn parse_log_files(
    in_folder: &std::path::Path,
    bus_parsers: &Vec<can_decode::Parser>,
) -> (Vec<ParsedMessage>, LogParseStats) {
    let mut all_parsed = Vec::new();
    let mut stats = LogParseStats::default();
    let mut file_paths = std::fs::read_dir(in_folder)
        .unwrap()
        .filter_map(|entry| entry.ok())
        .map(|entry| entry.path())
        .filter(|path| {
            path.is_file() && path.extension().and_then(|ext| ext.to_str()) == Some("log")
        })
        .collect::<Vec<_>>();
    file_paths.sort();
    for path in file_paths {
        log::info!("Parsing log file: {}", path.display());
        let (parsed, file_stats) = parse_log_file(&path, bus_parsers);
        stats.total_parsed += parsed.len();
        stats.total_failed += file_stats.failed_frames;
        stats.files.push(file_stats);
        all_parsed.extend(parsed);
    }

    (all_parsed, stats)
}

fn parse_log_file(
    in_file: &std::path::Path,
    bus_parsers: &Vec<can_decode::Parser>,
) -> (Vec<ParsedMessage>, FileParseStats) {
    let mut file_stats = FileParseStats::new(in_file);
    let mut content = std::fs::read(in_file).unwrap();

    // add padding zeroes if content length is not multiple of raw frame size
    let mut added_padding = false;
    if !content
        .len()
        .is_multiple_of(std::mem::size_of::<RawFrame>())
    {
        log::warn!(
            "Log file {} has length {} which is not a multiple of frame size {}. Possibly due to outdated log format.",
            in_file.display(),
            content.len(),
            std::mem::size_of::<RawFrame>()
        );
        content.extend(vec![
            0;
            std::mem::size_of::<RawFrame>()
                - (content.len() % std::mem::size_of::<RawFrame>())
        ]);
        added_padding = true;
    }
    let frames: Vec<RawFrame> = content
        .chunks_exact(std::mem::size_of::<RawFrame>())
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

        file_stats.total_frames += 1;

        let raw_can_id = consts::can_id_from_identity(frame.identity);
        let decode_msg_id = if (frame.identity & consts::IS_EID_MASK) != 0 {
            raw_can_id | 0x80000000
        } else {
            raw_can_id & util::can::STANDARD_ID_MASK
        };

        let bus_name = consts::bus_name_from_identity(frame.identity);
        let bus_parser = &bus_parsers[bus_name as usize];

        if let Some(decoded) = bus_parser.decode_msg(decode_msg_id, &frame.data) {
            parsed.push(ParsedMessage {
                timestamp: frame.ticks_ms,
                decoded,
                bus_name: bus_name.to_string(),
            });
            file_stats.parsed_frames += 1;
        } else {
            file_stats.record_failure(&bus_name.to_string(), raw_can_id);
            log::error!(
                "Failed to decode message at {} ms with CAN ID {:X} and data {:?} on bus {}",
                frame.ticks_ms,
                raw_can_id,
                frame.data,
                bus_name
            );
        }
    }
    (parsed, file_stats)
}

pub fn chunk_parsed(parsed: Vec<ParsedMessage>) -> Vec<Vec<ParsedMessage>> {
    let mut chunks = Vec::new();
    let mut current_chunk = Vec::new();
    let mut last_timestamp = None;

    for msg in parsed {
        if let Some(last_ts) = last_timestamp
            && (msg.timestamp < last_ts || msg.timestamp - last_ts > consts::MAX_JUMP_MS)
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
