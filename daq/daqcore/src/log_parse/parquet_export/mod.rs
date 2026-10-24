pub mod manifest;
pub mod schema;
mod writer;

use crate::{log_parse, superdbc};
use std::{error, fs, io, path};

// parquet export version
pub const FORMAT_VERSION: &str = "1.0";

pub const MANIFEST_FILE: &str = "manifest.json";

/// Approximate decoded batch size before passing data to the writer
pub const BATCH_TARGET_BYTES: usize = 4 * 1024 * 1024;
/// Approximate buffered row-group size before flushing
pub const ROW_GROUP_TARGET_BYTES: usize = 16 * 1024 * 1024;
/// Approximate signal file size before rotating chunks
pub const SIGNAL_CHUNK_TARGET_BYTES: u64 = 64 * 1024 * 1024;

/// Errors from bulk parsing, Arrow/Parquet encoding, or dataset finalization.
pub type Error = Box<dyn error::Error + Send + Sync>;

/// Dataset-relative directory for one continuous DAQ clock region.
pub fn segment_path(segment: u32) -> path::PathBuf {
    path::PathBuf::from(format!("segment-{segment:03}"))
}

/// All signals of one message stay together in one signal chunk.
pub fn signal_chunk_path(segment: u32, chunk: u32) -> path::PathBuf {
    segment_path(segment)
        .join("signals")
        .join(format!("signals-{chunk:03}.parquet"))
}

pub fn frames_path(segment: u32) -> path::PathBuf {
    segment_path(segment).join("frames.parquet")
}

pub fn events_path(segment: u32) -> path::PathBuf {
    segment_path(segment).join("events.parquet")
}

pub fn parse_logs_to_parquet(
    logs_dir: &path::Path,
    output_dir: &path::Path,
    database: &superdbc::database::Database,
) -> Result<log_parse::parquet_export::manifest::Manifest, Error> {
    if let Some(parent) = output_dir.parent().filter(|p| !p.as_os_str().is_empty()) {
        fs::create_dir_all(parent)?;
    }

    // Never overwrite an existing export or its completion marker
    fs::create_dir(output_dir)?;
    let parsed = log_parse::parse::parse_logs_with_frames(logs_dir, database)?;
    for source in &parsed.source_files {
        if source.to_str().is_none() {
            return Err(io::Error::new(
                io::ErrorKind::InvalidData,
                "source path is not valid UTF-8",
            )
            .into());
        }
    }

    let mut manifest = log_parse::parquet_export::manifest::Manifest::new(
        log_parse::parquet_export::manifest::SuperDbcReference {
            schema_version: database.schema_version().into(),
            content_hash: database.content_hash().into(),
            git_hash: database.version_hash().into(),
        },
        parsed.source_files,
    );

    let chunks = log_parse::segment::chunk_parsed(parsed.parsed_msgs);
    let mut correlated = log_parse::correlate::time_correlate_chunks(chunks);

    // Nothing decoded succesfully
    if correlated.is_empty() && !parsed.captured_frames.is_empty() {
        correlated.push(log_parse::correlate::CorrelationChunkResult::uncorrelated_new(Vec::new()));
    }

    for (index, chunk) in correlated.iter().enumerate() {
        // Raw-only records preceding a decoded boundary belong to the previous segment
        // Leading raw-only records belong to the first segment
        // Trailing ones to the last
        let first = if index == 0 {
            0
        } else {
            chunk.parsed_msgs[0].message_seq as usize
        };
        let last = match correlated.get(index + 1) {
            Some(next) => next.parsed_msgs[0].message_seq as usize,
            None => parsed.captured_frames.len(),
        };
        manifest.add_segment(log_parse::parquet_export::writer::write_segment(
            output_dir,
            index.try_into()?,
            chunk,
            &parsed.captured_frames[first..last],
            &manifest.source_files,
            database,
        )?);
    }

    let temporary = output_dir.join(log_parse::parquet_export::MANIFEST_FILE.to_owned() + ".tmp");
    {
        let mut file = fs::OpenOptions::new()
            .write(true)
            .create_new(true)
            .open(&temporary)?;
        serde_json::to_writer_pretty(&mut file, &manifest)?;
        file.sync_all()?;
    }
    // Only create manifest if the entire process was successful
    fs::rename(
        temporary,
        output_dir.join(log_parse::parquet_export::MANIFEST_FILE),
    )?;

    Ok(manifest)
}
