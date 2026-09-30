pub mod consts;
pub mod correlate;
pub mod parse;
pub mod table;

pub fn parse_logs_to_tables(
    logs_dir: &std::path::Path,
    output_dir: &std::path::Path,
    output_prefix: &str,
    parser_bus_0: &can_decode::Parser,
    bus_0_name: &str,
    parser_bus_1: &can_decode::Parser,
    bus_1_name: &str,
) {
    let parsed = parse::parse_log_files(logs_dir, parser_bus_0, parser_bus_1);
    let chunked = parse::chunk_parsed(parsed);
    let correlated = correlate::time_correlate_chunks(chunked);

    let mut table_builder = table::TableBuilder::new();
    table_builder.create_header(parser_bus_0, bus_0_name);
    table_builder.create_header(parser_bus_1, bus_1_name);
    table_builder.create_and_write_tables(output_dir, output_prefix, correlated);
}
