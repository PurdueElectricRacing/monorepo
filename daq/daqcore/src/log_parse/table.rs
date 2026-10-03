use crate::{
    log_parse::{consts, correlate},
    superdbc::{BusDatabase, BusId, DbGeneration},
};

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

    // Generation-bound positional handles avoid constructing string keys per frame.
    indexer: std::collections::HashMap<(DbGeneration, BusId, u32, usize), usize>,
    databases: Vec<BusDatabase>,
    next_col_idx: usize,
}

impl TableBuilder {
    pub fn new() -> Self {
        Self {
            header_columns: Vec::new(),
            databases: Vec::new(),
            next_col_idx: HEADER_COLUMN_COUNT,
            indexer: std::collections::HashMap::new(),
        }
    }

    fn row_width(&self) -> usize {
        HEADER_COLUMN_COUNT + self.header_columns.len()
    }

    fn push_column(&mut self, key: (DbGeneration, BusId, u32, usize), column: TableColumn) {
        self.indexer.insert(key, self.next_col_idx);
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
        debug_assert!(rows.len() == HEADER_ROW_COUNT);
        debug_assert!(rows.iter().all(|r| r.len() == HEADER_COLUMN_COUNT));

        for column in &self.header_columns {
            for (row, cell) in rows.iter_mut().zip(column.cells()) {
                row.push(cell.to_string());
            }
        }

        rows
    }

    pub fn create_header(&mut self, parser: &BusDatabase, bus_name: &str) {
        self.databases.push(parser.clone());
        // Retain the CSV's historical ordering by raw arbitration ID.
        let mut messages: Vec<_> = parser.msg_defs().iter().enumerate().collect();
        messages.sort_by_key(|(_, m)| (m.id.raw(), m.id.is_extended()));
        for (mi, msg) in messages {
            for (si, sig) in msg.signals.iter().enumerate() {
                let key = (
                    parser.database().generation(),
                    parser.bus_id(),
                    mi as u32,
                    si,
                );
                if !self.indexer.contains_key(&key) {
                    self.push_column(
                        key,
                        TableColumn {
                            bus: bus_name.to_string(),
                            node: msg.transmitter.clone(),
                            message: msg.name.clone(),
                            message_desc: if si == 0 {
                                msg.description.clone()
                            } else {
                                String::new()
                            },
                            signal: sig.name.clone(),
                            signal_desc: sig.description.clone(),
                            signal_unit: sig.unit.clone(),
                        },
                    );
                }
            }
        }
    }

    pub fn create_and_write_tables(
        &self,
        out_folder: &std::path::Path,
        output_prefix: &str,
        correlated_chunks: Vec<correlate::CorrelationChunkResult>,
    ) -> Result<(), super::LogError> {
        std::fs::create_dir_all(out_folder)?;

        for (chunk_idx, chunk) in correlated_chunks.iter().enumerate() {
            let first_time = chunk.parsed_msgs.first().map(|m| m.timestamp).unwrap_or(0);
            let last_time = chunk.parsed_msgs.last().map(|m| m.timestamp).unwrap_or(0);

            let first_row_time = (first_time / consts::BIN_WIDTH_MS) * consts::BIN_WIDTH_MS;
            let last_row_time = last_time.div_ceil(consts::BIN_WIDTH_MS) * consts::BIN_WIDTH_MS;
            let num_rows = ((last_row_time - first_row_time) / consts::BIN_WIDTH_MS) + 1;

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
            let mut wtr = csv::Writer::from_path(out_file.clone())?;
            for row in self.build_header_rows() {
                wtr.write_record(&row)?;
            }

            let mut msg_iter = chunk.parsed_msgs.iter().peekable();
            for row_idx in 0..num_rows {
                let row_time = first_row_time + row_idx * consts::BIN_WIDTH_MS;
                let row_end = row_time + consts::BIN_WIDTH_MS;
                let mut row = vec![String::new(); self.row_width()];

                if let Some(ct) = chunk
                    .correlation_fn
                    .as_ref()
                    .and_then(|cf| cf.correlate(row_time))
                {
                    row[0] = ct.to_rfc3339_opts(chrono::SecondsFormat::Millis, true);
                }
                row[1] = format!("{:.3}", row_time as f32 / 1000.0);

                while let Some(msg) = msg_iter.peek() {
                    if msg.timestamp >= row_end {
                        break;
                    }

                    let msg = msg_iter.next().unwrap();
                    let Some(decoded) = self.databases.iter().find_map(|db| db.view(&msg.frame))
                    else {
                        continue;
                    };
                    for si in 0..msg.frame.n_signals as usize {
                        let Some(sig) = decoded.signals.at(si) else {
                            continue;
                        };
                        let key = (msg.frame.generation, msg.frame.bus, msg.frame.msg_index, si);
                        if let Some(&col_idx) = self.indexer.get(&key) {
                            row[col_idx] = if let Some(label) = sig.value.enum_label {
                                format!("{} ({})", label, sig.value.int_rounded())
                            } else {
                                sig.value.physical.to_string()
                            };
                        }
                    }
                }

                wtr.write_record(&row)?;
            }
            wtr.flush()?;
            log::info!("Wrote chunk {} to CSV ({})", chunk_idx, out_file.display());
        }
        Ok(())
    }
}
