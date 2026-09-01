use crate::{
    daq_log_parse::{consts, correlate},
    util,
    messages,
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

    // Key is (bus name, msg name, signal name), value is column index
    indexer: std::collections::HashMap<(String, String, String), usize>,
    next_col_idx: usize,
}

impl TableBuilder {
    pub fn new() -> Self {
        Self {
            header_columns: Vec::new(),
            next_col_idx: HEADER_COLUMN_COUNT,
            indexer: std::collections::HashMap::new(),
        }
    }

    fn row_width(&self) -> usize {
        HEADER_COLUMN_COUNT + self.header_columns.len()
    }

    fn push_column(&mut self, key: (String, String, String), column: TableColumn) {
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

    pub fn create_header(&mut self, parser: &can_decode::Parser, bus_name: messages::BusName) {
        let mut message_defs = parser.msg_defs();
        message_defs.sort_by_key(|m| util::can::can_dbc_to_u32_without_extid_flag(&m.id));

        for msg in message_defs {
            let bus_id = bus_name as usize;
            let node = match msg.transmitter {
                can_dbc::Transmitter::NodeName(n) => n,
                can_dbc::Transmitter::VectorXXX => "N/A".to_string(),
            };

            let msg_id_u32 = util::can::can_dbc_to_u32_with_extid_flag(&msg.id);
            let msg_desc = parser
                .msg_desc(msg_id_u32)
                .map(|d| d.to_string())
                .unwrap_or_default();

            for (i, sig) in msg.signals.iter().enumerate() {
                let key = (bus_id.to_string(), msg.name.clone(), sig.name.clone());
                if !self.indexer.contains_key(&key) {
                    let sig_desc = parser
                        .signal_desc(msg_id_u32, &sig.name)
                        .map(|d| d.to_string())
                        .unwrap_or_default();

                    self.push_column(
                        key,
                        TableColumn {
                            bus: bus_id.to_string(),
                            node: node.clone(),
                            message: msg.name.clone(),
                            message_desc: if i == 0 {
                                msg_desc.clone()
                            } else {
                                String::new()
                            },
                            signal: sig.name.clone(),
                            signal_desc: sig_desc,
                            signal_unit: sig.unit.to_string(),
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
    ) {
        std::fs::create_dir_all(out_folder).unwrap();

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
            let mut wtr = csv::Writer::from_path(out_file.clone()).unwrap();
            for row in self.build_header_rows() {
                wtr.write_record(&row).unwrap();
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
                    let decoded = &msg.decoded;
                    for (sig_name, sig_value) in &decoded.signals {
                        let key = (msg.bus_name.clone(), decoded.name.clone(), sig_name.clone());
                        if let Some(&col_idx) = self.indexer.get(&key) {
                            row[col_idx] = if let Some(enum_label) = &sig_value.value.enum_label {
                                format!("{} ({})", enum_label, sig_value.value.int_rounded())
                            } else {
                                sig_value.value.physical.to_string()
                            };
                        }
                    }
                }

                wtr.write_record(&row).unwrap();
            }
            wtr.flush().unwrap();
            log::info!("Wrote chunk {} to CSV ({})", chunk_idx, out_file.display());
        }
    }
}
