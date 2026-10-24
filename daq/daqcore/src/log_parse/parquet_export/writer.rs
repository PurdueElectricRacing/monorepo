use crate::{log_parse, superdbc};
use std::{fs, io, path, sync};

/// Writes to a temporary file until the Parquet footer has been finalized
struct TableWriter {
    writer: parquet::arrow::ArrowWriter<fs::File>,
    temporary: path::PathBuf,
    destination: path::PathBuf,
}

impl TableWriter {
    fn new(
        root: &path::Path,
        relative: &path::Path,
        schema: sync::Arc<arrow_schema::Schema>,
    ) -> Result<Self, log_parse::parquet_export::Error> {
        let destination = root.join(relative);
        fs::create_dir_all(destination.parent().expect("dataset file has a parent"))?;

        let temporary = destination.with_extension("parquet.tmp");
        let file = fs::OpenOptions::new()
            .write(true)
            .create_new(true)
            .open(&temporary)?;

        let properties = parquet::file::properties::WriterProperties::builder()
            .set_compression(parquet::basic::Compression::ZSTD(Default::default()))
            .set_max_row_group_bytes(Some(log_parse::parquet_export::ROW_GROUP_TARGET_BYTES))
            .build();
        let writer = parquet::arrow::ArrowWriter::try_new(file, schema, Some(properties))?;

        Ok(Self {
            writer,
            temporary,
            destination,
        })
    }

    /// Writes a batch of rows to the current row group,
    /// flushing if the row group exceeds the target size
    fn write(
        &mut self,
        batch: &arrow_array::RecordBatch,
    ) -> Result<(), log_parse::parquet_export::Error> {
        self.writer.write(batch)?;

        if self.writer.memory_size() >= log_parse::parquet_export::ROW_GROUP_TARGET_BYTES {
            self.writer.flush()?;
        }

        Ok(())
    }

    fn size(&self) -> u64 {
        (self.writer.bytes_written() + self.writer.in_progress_size()) as u64
    }

    fn finish(self) -> Result<(), log_parse::parquet_export::Error> {
        self.writer.close()?;
        fs::rename(self.temporary, self.destination)?;

        Ok(())
    }
}

fn timestamp(
    ticks: u32,
    correction: Option<&log_parse::correlate::CorrelationFunction>,
) -> Result<i64, log_parse::parquet_export::Error> {
    match correction {
        Some(correction) => correction
            .correlate(ticks)
            .map(|t| t.timestamp_millis())
            .ok_or_else(|| {
                io::Error::new(
                    io::ErrorKind::InvalidData,
                    "corrected timestamp is outside the UTC range",
                )
                .into()
            }),
        None => Ok(i64::from(ticks)),
    }
}

pub fn write_segment(
    root: &path::Path,
    index: u32,
    chunk: &log_parse::correlate::CorrelationChunkResult,
    frames: &[log_parse::parse::CapturedFrame],
    sources: &[path::PathBuf],
    database: &superdbc::database::Database,
) -> Result<log_parse::parquet_export::manifest::Segment, log_parse::parquet_export::Error> {
    fs::create_dir_all(
        root.join(log_parse::parquet_export::segment_path(index))
            .join("signals"),
    )?;

    // Segment bounds include every raw frame, even frames that failed to decode.
    let correction = chunk.correlation_fn.as_ref();
    let mut start = i64::MAX;
    let mut end = i64::MIN;

    for frame in frames {
        let time = timestamp(frame.raw.ticks_ms, correction)?;
        start = start.min(time);
        end = end.max(time);
    }

    write_frames(root, index, frames, sources)?;
    let signal_chunks = write_signals(root, index, chunk, database)?;

    // TODO: actually put things in event.parquet
    TableWriter::new(
        root,
        &log_parse::parquet_export::events_path(index),
        log_parse::parquet_export::schema::events_schema(),
    )?
    .finish()?;

    log::info!(
        "Finished Parquet segment {}: {} frames, {} signal chunks",
        root.join(log_parse::parquet_export::segment_path(index))
            .display(),
        frames.len(),
        signal_chunks.len()
    );

    Ok(log_parse::parquet_export::manifest::Segment {
        directory: log_parse::parquet_export::segment_path(index),
        correction: correction.map(|correction| {
            let (slope, intercept_ms) = correction.coefficients();
            log_parse::parquet_export::manifest::ClockCorrection {
                slope,
                intercept_ms,
            }
        }),
        time_basis: if correction.is_some() {
            log_parse::parquet_export::manifest::TimeBasis::UtcMs
        } else {
            log_parse::parquet_export::manifest::TimeBasis::DaqTicksMs
        },
        start_time_ms: start,
        end_time_ms: end,
        signal_chunks,
    })
}

fn write_frames(
    root: &path::Path,
    index: u32,
    frames: &[log_parse::parse::CapturedFrame],
    sources: &[path::PathBuf],
) -> Result<(), log_parse::parquet_export::Error> {
    let mut writer = TableWriter::new(
        root,
        &log_parse::parquet_export::frames_path(index),
        log_parse::parquet_export::schema::frames_schema(),
    )?;

    let mut first = 0;
    while first < frames.len() {
        let mut last = first;
        let mut bytes = 0;

        // Estimate the batch size using the fixed fields and source path length.
        while last < frames.len() && bytes < log_parse::parquet_export::BATCH_TARGET_BYTES {
            bytes += 64 + sources[frames[last].source_index].as_os_str().len();
            last += 1;
        }

        let batch = frame_batch(&frames[first..last], sources)?;
        writer.write(&batch)?;

        first = last;
    }

    writer.finish()
}

fn frame_batch(
    frames: &[log_parse::parse::CapturedFrame],
    sources: &[path::PathBuf],
) -> Result<arrow_array::RecordBatch, arrow_schema::ArrowError> {
    let columns: Vec<arrow_array::ArrayRef> = vec![
        sync::Arc::new(arrow_array::UInt64Array::from_iter_values(
            frames.iter().map(|frame| frame.message_seq),
        )),
        sync::Arc::new(arrow_array::StringArray::from_iter_values(
            frames.iter().map(|frame| {
                sources[frame.source_index]
                    .to_str()
                    .expect("source paths validated before writing")
            }),
        )),
        sync::Arc::new(arrow_array::UInt64Array::from_iter_values(
            frames.iter().map(|frame| frame.source_record),
        )),
        sync::Arc::new(arrow_array::UInt32Array::from_iter_values(
            frames.iter().map(|frame| frame.raw.ticks_ms),
        )),
        sync::Arc::new(arrow_array::UInt32Array::from_iter_values(
            frames.iter().map(|frame| frame.raw.identity),
        )),
        sync::Arc::new(arrow_array::BinaryArray::from_iter_values(
            frames.iter().map(|frame| frame.raw.data),
        )),
        sync::Arc::new(arrow_array::StringArray::from_iter_values(
            frames.iter().map(|frame| frame.decode_status.as_str()),
        )),
    ];

    arrow_array::RecordBatch::try_new(log_parse::parquet_export::schema::frames_schema(), columns)
}

struct SignalRow<'a> {
    message: &'a log_parse::parse::ParsedMessage,
    definition: &'a superdbc::message::Message,
    signal: &'a superdbc::message::SignalDefinition,
    value: f64,
    utc: Option<i64>,
}

impl SignalRow<'_> {
    fn estimated_bytes(&self) -> usize {
        128 + self.message.bus_name.len()
            + self.message.decoded.tx_node().len()
            + self.definition.name().len()
            + self.definition.description().len()
            + self.signal.name().len()
            + self.signal.description().len()
            + self.signal.unit().len()
    }
}

fn signal_batch(
    rows: &[SignalRow<'_>],
) -> Result<arrow_array::RecordBatch, arrow_schema::ArrowError> {
    let columns: Vec<arrow_array::ArrayRef> = vec![
        sync::Arc::new(arrow_array::UInt64Array::from_iter_values(
            rows.iter().map(|row| row.message.message_seq),
        )),
        sync::Arc::new(arrow_array::UInt32Array::from_iter_values(
            rows.iter().map(|row| row.message.timestamp),
        )),
        sync::Arc::new(arrow_array::Int64Array::from_iter(
            rows.iter().map(|row| row.utc),
        )),
        sync::Arc::new(arrow_array::UInt8Array::from_iter_values(
            rows.iter().map(|row| row.definition.key().bus_id),
        )),
        sync::Arc::new(arrow_array::StringArray::from_iter_values(
            rows.iter().map(|row| row.message.bus_name.as_str()),
        )),
        sync::Arc::new(arrow_array::UInt32Array::from_iter_values(
            rows.iter().map(|row| row.definition.identity().raw_id()),
        )),
        sync::Arc::new(arrow_array::BooleanArray::from_iter(
            rows.iter()
                .map(|row| Some(row.definition.identity().is_extended())),
        )),
        sync::Arc::new(arrow_array::StringArray::from_iter_values(
            rows.iter().map(|row| row.message.decoded.tx_node()),
        )),
        sync::Arc::new(arrow_array::StringArray::from_iter_values(
            rows.iter().map(|row| row.definition.name()),
        )),
        sync::Arc::new(arrow_array::StringArray::from_iter_values(
            rows.iter().map(|row| row.definition.description()),
        )),
        sync::Arc::new(arrow_array::StringArray::from_iter_values(
            rows.iter().map(|row| row.signal.name()),
        )),
        sync::Arc::new(arrow_array::StringArray::from_iter_values(
            rows.iter().map(|row| row.signal.description()),
        )),
        sync::Arc::new(arrow_array::StringArray::from_iter_values(
            rows.iter().map(|row| row.signal.unit()),
        )),
        sync::Arc::new(arrow_array::Float64Array::from_iter_values(
            rows.iter().map(|row| row.value),
        )),
    ];

    arrow_array::RecordBatch::try_new(log_parse::parquet_export::schema::signals_schema(), columns)
}

struct SignalOutput {
    writer: TableWriter,
    path: path::PathBuf,
    start: i64,
    end: i64,
}

impl SignalOutput {
    fn finish(
        self,
    ) -> Result<log_parse::parquet_export::manifest::SignalChunk, log_parse::parquet_export::Error>
    {
        self.writer.finish()?;

        log::info!("Wrote Parquet signal chunk {}", self.path.display());

        Ok(log_parse::parquet_export::manifest::SignalChunk {
            path: self.path,
            start_time_ms: self.start,
            end_time_ms: self.end,
        })
    }
}

fn write_signals(
    root: &path::Path,
    index: u32,
    chunk: &log_parse::correlate::CorrelationChunkResult,
    database: &superdbc::database::Database,
) -> Result<Vec<log_parse::parquet_export::manifest::SignalChunk>, log_parse::parquet_export::Error>
{
    let mut files = Vec::new();
    let mut output: Option<SignalOutput> = None;
    let mut rows = Vec::new();
    let mut bytes = 0;

    for message in &chunk.parsed_msgs {
        // Flush only between complete messages, retaining duplicate timestamps.
        if bytes >= log_parse::parquet_export::BATCH_TARGET_BYTES {
            let current = output.as_mut().expect("buffered rows have an output file");
            current.writer.write(&signal_batch(&rows)?)?;
            rows.clear();
            bytes = 0;

            // Rotate only after writing the batch so a message stays in one file.
            if current.writer.size() >= log_parse::parquet_export::SIGNAL_CHUNK_TARGET_BYTES {
                files.push(output.take().expect("output file is open").finish()?);
            }
        }

        if message.decoded.is_empty() {
            continue;
        }

        let definition = database
            .message_by_key(message.decoded.key())
            .ok_or_else(|| {
                io::Error::new(
                    io::ErrorKind::InvalidData,
                    "decoded message has no matching definition",
                )
            })?;

        let utc = chunk
            .correlation_fn
            .as_ref()
            .map(|correction| timestamp(message.timestamp, Some(correction)))
            .transpose()?;
        let time = utc.unwrap_or(i64::from(message.timestamp));

        // Open files lazily so segments without signals have no signal chunks.
        if output.is_none() {
            let path = log_parse::parquet_export::signal_chunk_path(index, files.len().try_into()?);
            output = Some(SignalOutput {
                writer: TableWriter::new(
                    root,
                    &path,
                    log_parse::parquet_export::schema::signals_schema(),
                )?,
                path,
                start: time,
                end: time,
            });
        }

        let current = output.as_mut().expect("output file was opened above");
        current.start = current.start.min(time);
        current.end = current.end.max(time);

        for (name, value) in message.decoded.iter() {
            let signal = definition.signal(name).ok_or_else(|| {
                io::Error::new(
                    io::ErrorKind::InvalidData,
                    "decoded signal has no matching definition",
                )
            })?;

            let row = SignalRow {
                message,
                definition,
                signal,
                value: value.physical(),
                utc,
            };
            bytes += row.estimated_bytes();
            rows.push(row);
        }
    }

    // Write the final partial batch before closing the last file.
    if let Some(mut output) = output {
        if !rows.is_empty() {
            output.writer.write(&signal_batch(&rows)?)?;
        }
        files.push(output.finish()?);
    }

    Ok(files)
}
