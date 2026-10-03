pub mod consts;
pub mod correlate;
pub mod parse;
pub mod table;

pub fn parse_logs_to_tables(
    logs_dir: &std::path::Path,
    output_dir: &std::path::Path,
    output_prefix: &str,
    parser_bus_0: &crate::superdbc::BusDatabase,
    bus_0_name: &str,
    parser_bus_1: &crate::superdbc::BusDatabase,
    bus_1_name: &str,
) -> Result<(), LogError> {
    if parser_bus_0.database().generation() == parser_bus_1.database().generation()
        && parser_bus_0.bus_id() == parser_bus_1.bus_id()
    {
        return Err(LogError::InvalidBinding);
    }
    let parsed = parse::parse_log_files(logs_dir, parser_bus_0, parser_bus_1)?;
    let chunked = parse::chunk_parsed(parsed);
    let correlated = correlate::time_correlate_chunks(chunked, &[parser_bus_0, parser_bus_1]);

    let mut table_builder = table::TableBuilder::new();
    table_builder.create_header(parser_bus_0, bus_0_name);
    table_builder.create_header(parser_bus_1, bus_1_name);
    table_builder.create_and_write_tables(output_dir, output_prefix, correlated)
}

#[derive(Debug)]
pub enum LogError {
    InvalidBinding,
    Io(std::io::Error),
    Csv(csv::Error),
}
impl std::fmt::Display for LogError {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        match self {
            Self::InvalidBinding => f.write_str("Log slots must be bound to different buses"),
            Self::Io(e) => write!(f, "Log I/O: {e}"),
            Self::Csv(e) => write!(f, "CSV output: {e}"),
        }
    }
}
impl std::error::Error for LogError {
    fn source(&self) -> Option<&(dyn std::error::Error + 'static)> {
        match self {
            Self::InvalidBinding => None,
            Self::Io(e) => Some(e),
            Self::Csv(e) => Some(e),
        }
    }
}
impl From<std::io::Error> for LogError {
    fn from(e: std::io::Error) -> Self {
        Self::Io(e)
    }
}
impl From<csv::Error> for LogError {
    fn from(e: csv::Error) -> Self {
        Self::Csv(e)
    }
}
