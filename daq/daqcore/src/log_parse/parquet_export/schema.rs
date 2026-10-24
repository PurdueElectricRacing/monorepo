use crate::log_parse;
use std::{collections, sync};

fn table_schema(table: &str, fields: Vec<arrow_schema::Field>) -> sync::Arc<arrow_schema::Schema> {
    sync::Arc::new(arrow_schema::Schema::new_with_metadata(
        fields,
        collections::HashMap::from([
            (
                "daq.format_version".into(),
                log_parse::parquet_export::FORMAT_VERSION.into(),
            ),
            ("daq.table".into(), table.into()),
        ]),
    ))
}

/// One decoded physical signal update per row, in message/definition order.
pub fn signals_schema() -> sync::Arc<arrow_schema::Schema> {
    table_schema(
        "signals",
        vec![
            // `message_seq` is a dataset-wide capture ordinal assigned before decoding.
            arrow_schema::Field::new("message_seq", arrow_schema::DataType::UInt64, false),
            arrow_schema::Field::new("daq_ticks_ms", arrow_schema::DataType::UInt32, false),
            arrow_schema::Field::new("timestamp_utc_ms", arrow_schema::DataType::Int64, true),
            arrow_schema::Field::new("bus_id", arrow_schema::DataType::UInt8, false),
            arrow_schema::Field::new("bus_name", arrow_schema::DataType::Utf8, false),
            arrow_schema::Field::new("can_id", arrow_schema::DataType::UInt32, false),
            arrow_schema::Field::new("is_extended", arrow_schema::DataType::Boolean, false),
            arrow_schema::Field::new("node_name", arrow_schema::DataType::Utf8, false),
            arrow_schema::Field::new("message_name", arrow_schema::DataType::Utf8, false),
            arrow_schema::Field::new("message_desc", arrow_schema::DataType::Utf8, false),
            arrow_schema::Field::new("signal_name", arrow_schema::DataType::Utf8, false),
            arrow_schema::Field::new("signal_desc", arrow_schema::DataType::Utf8, false),
            arrow_schema::Field::new("signal_unit", arrow_schema::DataType::Utf8, false),
            arrow_schema::Field::new("value", arrow_schema::DataType::Float64, false),
        ],
    )
}

/// One complete raw record per row, including records without decoded signals.
/// Payloads are exactly eight bytes.
/// `decode_status` is one of: ok, invalid_identity, unknown_bus, unknown_message, decode_error.
pub fn frames_schema() -> sync::Arc<arrow_schema::Schema> {
    table_schema(
        "frames",
        vec![
            // `message_seq` is a dataset-wide capture ordinal assigned before decoding.
            arrow_schema::Field::new("message_seq", arrow_schema::DataType::UInt64, false),
            arrow_schema::Field::new("source_file", arrow_schema::DataType::Utf8, false),
            // `source_record` is zero-based within its source file
            arrow_schema::Field::new("source_record", arrow_schema::DataType::UInt64, false),
            arrow_schema::Field::new("daq_ticks_ms", arrow_schema::DataType::UInt32, false),
            arrow_schema::Field::new("raw_identity", arrow_schema::DataType::UInt32, false),
            arrow_schema::Field::new("payload", arrow_schema::DataType::Binary, false),
            arrow_schema::Field::new("decode_status", arrow_schema::DataType::Utf8, false),
        ],
    )
}

/// Events are segment-local and may span signal chunks.
/// A null end is an instantaneous event.
/// Time basis is declared by the segment manifest, not inferred from the timestamp values.
pub fn events_schema() -> sync::Arc<arrow_schema::Schema> {
    table_schema(
        "events",
        vec![
            arrow_schema::Field::new("name", arrow_schema::DataType::Utf8, false),
            arrow_schema::Field::new("color", arrow_schema::DataType::Utf8, false),
            arrow_schema::Field::new("start_time_ms", arrow_schema::DataType::Int64, false),
            arrow_schema::Field::new("end_time_ms", arrow_schema::DataType::Int64, true),
        ],
    )
}
