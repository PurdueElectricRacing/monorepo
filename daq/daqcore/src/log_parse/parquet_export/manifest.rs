use crate::log_parse;
use std::path;

#[derive(Debug, Clone, PartialEq, serde::Serialize, serde::Deserialize)]
pub struct Manifest {
    pub format_version: String,
    pub superdbc: SuperDbcReference,
    pub source_files: Vec<path::PathBuf>,
    pub segments: Vec<Segment>,
}

impl Manifest {
    pub fn new(superdbc: SuperDbcReference, source_files: Vec<path::PathBuf>) -> Self {
        Self {
            format_version: log_parse::parquet_export::FORMAT_VERSION.into(),
            superdbc,
            source_files,
            segments: Vec::new(),
        }
    }

    pub fn add_segment(&mut self, segment: Segment) {
        self.segments.push(segment);
    }
}

#[derive(Debug, Clone, PartialEq, Eq, serde::Serialize, serde::Deserialize)]
pub struct SuperDbcReference {
    pub schema_version: String,
    pub content_hash: String,
    pub git_hash: String,
}

#[derive(Debug, Clone, PartialEq, serde::Serialize, serde::Deserialize)]
pub struct Segment {
    pub directory: path::PathBuf,
    pub correction: Option<ClockCorrection>,
    pub time_basis: TimeBasis,
    pub start_time_ms: i64, // depends on time_basis
    pub end_time_ms: i64,
    pub signal_chunks: Vec<SignalChunk>,
}

#[derive(Debug, Clone, PartialEq, serde::Serialize, serde::Deserialize)]
pub struct SignalChunk {
    pub path: path::PathBuf,
    pub start_time_ms: i64, // depends on time_basis
    pub end_time_ms: i64,
}

#[derive(Debug, Clone, PartialEq, serde::Serialize, serde::Deserialize)]
/// `unix_ms = slope * daq_ticks_ms + intercept_ms`, rounded to milliseconds.
pub struct ClockCorrection {
    pub slope: f64,
    pub intercept_ms: f64,
}

#[derive(Debug, Clone, Copy, PartialEq, Eq, serde::Serialize, serde::Deserialize)]
#[serde(rename_all = "snake_case")]
pub enum TimeBasis {
    UtcMs,
    DaqTicksMs,
}
