//! Existing CSV table export using the shared log processing modules.

use crate::{log_parse, superdbc};
use std::path;

pub mod table;

pub fn parse_logs_to_tables(
    logs_dir: &path::Path,
    output_dir: &path::Path,
    output_prefix: &str,
    database: &superdbc::database::Database,
) -> Result<(), csv::Error> {
    let parsed = log_parse::parse::parse_log_files_only_msgs(logs_dir, database)?;
    let chunked = log_parse::segment::chunk_parsed(parsed);
    let correlated = log_parse::correlate::time_correlate_chunks(chunked);

    let mut table_builder = log_parse::csv_export::table::TableBuilder::default();
    for bus in [0, 1].into_iter().filter_map(|id| database.bus_by_id(id)) {
        table_builder.create_header(bus);
    }
    table_builder.create_and_write_tables(output_dir, output_prefix, correlated)
}
