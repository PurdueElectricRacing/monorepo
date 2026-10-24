use crate::{log_parse, superdbc};
use std::{collections, fs, path};

const HEADER_ROW_COUNT: usize = 7;
const HEADER_COLUMN_COUNT: usize = 3; // real time, daq timestamp, then per-row header label
const HEADER_LABELS: [&str; HEADER_ROW_COUNT] = [
    "Bus",
    "Node",
    "Message",
    "Message Description",
    "Signal",
    "Signal Description",
    "Signal Unit",
];

#[derive(Clone, Default)]
struct TableColumn {
    bus: String,
    node: String,
    message: String,
    message_desc: String,
    signal: String,
    signal_desc: String,
    signal_unit: String,
}

impl TableColumn {
    fn cells(&self) -> [&str; HEADER_ROW_COUNT] {
        [
            &self.bus,
            &self.node,
            &self.message,
            &self.message_desc,
            &self.signal,
            &self.signal_desc,
            &self.signal_unit,
        ]
    }
}

pub struct TableBuilder {
    header_columns: Vec<TableColumn>,

    // Key is (bus name, msg name, signal name), value is numeric column index
    indexer: collections::HashMap<(String, String, String), usize>,
    enum_indexer: collections::HashMap<(String, String, String), usize>,
    next_col_idx: usize,
}

impl Default for TableBuilder {
    fn default() -> Self {
        Self {
            header_columns: Vec::new(),
            next_col_idx: HEADER_COLUMN_COUNT,
            indexer: collections::HashMap::new(),
            enum_indexer: collections::HashMap::new(),
        }
    }
}

impl TableBuilder {
    fn row_width(&self) -> usize {
        HEADER_COLUMN_COUNT + self.header_columns.len()
    }

    fn push_column(&mut self, key: (String, String, String), column: TableColumn) {
        self.indexer.insert(key, self.next_col_idx);
        self.header_columns.push(column);
        self.next_col_idx += 1;
    }

    fn push_enum_column(&mut self, key: (String, String, String), column: TableColumn) {
        self.enum_indexer.insert(key, self.next_col_idx);
        self.header_columns.push(column);
        self.next_col_idx += 1;
    }

    fn build_header_rows(&self) -> Vec<Vec<String>> {
        let mut rows = vec![
            vec!["".to_string(), "".to_string(), HEADER_LABELS[0].to_string()],
            vec!["".to_string(), "".to_string(), HEADER_LABELS[1].to_string()],
            vec!["".to_string(), "".to_string(), HEADER_LABELS[2].to_string()],
            vec!["".to_string(), "".to_string(), HEADER_LABELS[3].to_string()],
            vec![
                "Real Time".to_string(),
                "DAQ Timestamp".to_string(),
                HEADER_LABELS[4].to_string(),
            ],
            vec!["".to_string(), "".to_string(), HEADER_LABELS[5].to_string()],
            vec!["".to_string(), "".to_string(), HEADER_LABELS[6].to_string()],
        ];

        for column in &self.header_columns {
            for (row, cell) in rows.iter_mut().zip(column.cells()) {
                row.push(cell.to_string());
            }
        }

        rows
    }

    pub fn create_header(&mut self, bus: &superdbc::database::Bus) {
        let mut messages: Vec<_> = bus.messages().iter().collect();
        messages.sort_by_key(|message| {
            (
                message.identity().raw_id(),
                message.identity().is_extended(),
            )
        });
        for message in messages {
            for (i, signal) in message.signals().iter().enumerate() {
                let key = (
                    bus.name().to_owned(),
                    message.name().to_owned(),
                    signal.name().to_owned(),
                );
                if self.indexer.contains_key(&key) {
                    continue;
                }
                self.push_column(
                    key.clone(),
                    TableColumn {
                        bus: bus.name().to_owned(),
                        node: message.transmitter().to_owned(),
                        message: message.name().to_owned(),
                        message_desc: if i == 0 {
                            message.description().to_owned()
                        } else {
                            String::new()
                        },
                        signal: signal.name().to_owned(),
                        signal_desc: signal.description().to_owned(),
                        signal_unit: signal.unit().to_owned(),
                    },
                );
                let choices: Vec<_> = signal.choices().map(|(_, label)| label).collect();
                if !choices.is_empty() {
                    self.push_enum_column(
                        key,
                        TableColumn {
                            bus: bus.name().to_owned(),
                            node: message.transmitter().to_owned(),
                            message: message.name().to_owned(),
                            signal: format!("{}_enum", signal.name()),
                            signal_desc: format!("choices: {}", choices.join(", ")),
                            ..TableColumn::default()
                        },
                    );
                }
            }
        }
    }

    pub fn create_and_write_tables(
        &self,
        out_folder: &path::Path,
        output_prefix: &str,
        correlated_chunks: Vec<log_parse::correlate::CorrelationChunkResult>,
    ) -> Result<(), csv::Error> {
        fs::create_dir_all(out_folder)?;

        for (chunk_idx, chunk) in correlated_chunks.iter().enumerate() {
            let first_time = chunk.parsed_msgs.first().map(|m| m.timestamp).unwrap_or(0);
            let last_time = chunk.parsed_msgs.last().map(|m| m.timestamp).unwrap_or(0);

            let bin_width = u64::from(log_parse::consts::BIN_WIDTH_MS);
            let first_row_time = u64::from(first_time) / bin_width * bin_width;
            let last_row_time = u64::from(last_time).div_ceil(bin_width) * bin_width;
            let num_rows = last_row_time.saturating_sub(first_row_time) / bin_width + 1;

            let first_correlated_time: Option<String> =
                chunk.correlation_fn.as_ref().and_then(|cf| {
                    cf.correlate(first_time).map(|dt| {
                        dt.to_rfc3339_opts(chrono::SecondsFormat::Millis, true)
                            .replace(':', "-")
                    })
                });

            let out_file = match first_correlated_time {
                Some(t) => out_folder.join(format!("{}_{:03}_{}.csv", output_prefix, chunk_idx, t)),
                None => out_folder.join(format!("{}_{:03}.csv", output_prefix, chunk_idx)),
            };

            let mut writer = csv::Writer::from_path(&out_file)?;
            for row in self.build_header_rows() {
                writer.write_record(&row)?;
            }

            let mut msg_iter = chunk.parsed_msgs.iter().peekable();
            for row_idx in 0..num_rows {
                let row_time = first_row_time + row_idx * bin_width;
                let row_end = row_time + bin_width;
                let mut row = vec![String::new(); self.row_width()];

                if let Some(ct) = chunk.correlation_fn.as_ref().and_then(|cf| {
                    u32::try_from(row_time)
                        .ok()
                        .and_then(|time| cf.correlate(time))
                }) {
                    row[0] = ct.to_rfc3339_opts(chrono::SecondsFormat::Millis, true);
                }
                row[1] = format!("{:.3}", row_time as f32 / 1000.0);

                while let Some(msg) = msg_iter.next_if(|msg| u64::from(msg.timestamp) < row_end) {
                    let decoded = &msg.decoded;
                    for (sig_name, sig_value) in decoded.iter() {
                        let key = (
                            msg.bus_name.clone(),
                            decoded.name().to_owned(),
                            sig_name.to_owned(),
                        );
                        if let Some(&col_idx) = self.indexer.get(&key) {
                            row[col_idx] = if sig_value.enum_label().is_some() {
                                sig_value.int_rounded().to_string()
                            } else {
                                sig_value.physical().to_string()
                            };
                        }
                        if let Some(&enum_col_idx) = self.enum_indexer.get(&key) {
                            row[enum_col_idx] = match sig_value.enum_label() {
                                Some(label) => label.to_owned(),
                                None => "[UNKNOWN]".to_string(),
                            };
                        }
                    }
                }

                writer.write_record(&row)?;
            }

            writer.flush()?;
            log::info!("Wrote chunk {} to CSV ({})", chunk_idx, out_file.display());
        }

        Ok(())
    }
}
