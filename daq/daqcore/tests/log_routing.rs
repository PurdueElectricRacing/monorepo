use daqcore::{can::*, log_parse, superdbc::*};

#[test]
fn legacy_bus_and_extended_flags_route_to_explicit_buses_and_csv_columns() {
    let db = SuperDbc::from_str(include_str!("fixtures/superdbc.json")).unwrap();
    let vcan = db.bind(db.bus("VCAN").unwrap().bus_id).unwrap();
    let ccan = db.bind(db.bus("CCAN").unwrap().bus_id).unwrap();
    assert!(matches!(
        log_parse::parse_logs_to_tables(
            std::path::Path::new("unused"),
            std::path::Path::new("unused"),
            "out",
            &vcan,
            "VCAN",
            &vcan,
            "VCAN"
        ),
        Err(log_parse::LogError::InvalidBinding)
    ));
    let root = std::env::temp_dir().join(format!("superdbc-log-routing-{}", std::process::id()));
    let input = root.join("logs");
    let output = root.join("csv");
    std::fs::create_dir_all(&input).unwrap();
    let mut records = Vec::new();
    for (identity, data) in [(1u32, [0u8; 8]), (0xc0000001, [0xff; 8])] {
        records.extend(10u32.to_le_bytes());
        records.extend(identity.to_le_bytes());
        records.extend(data);
    }
    assert_eq!(records.len(), 32);
    std::fs::write(input.join("capture.log"), records).unwrap();
    let parsed = log_parse::parse::parse_log_files(&input, &vcan, &ccan).unwrap();
    assert_eq!(parsed.len(), 2);
    assert_eq!(parsed[0].frame.bus, vcan.bus_id());
    assert_eq!(parsed[0].frame.id, MessageId::from_parts(false, 1).unwrap());
    assert_eq!(parsed[1].frame.bus, ccan.bus_id());
    assert_eq!(parsed[1].frame.id, MessageId::from_parts(true, 1).unwrap());
    log_parse::parse_logs_to_tables(&input, &output, "golden", &vcan, "VCAN", &ccan, "CCAN")
        .unwrap();
    let mut reader = csv::ReaderBuilder::new()
        .has_headers(false)
        .from_path(output.join("golden_000.csv"))
        .unwrap();
    let rows: Vec<_> = reader.records().map(Result::unwrap).collect();
    assert_eq!(rows.len(), 8);
    let column = |bus: &str, message: &str, signal: &str| {
        (3..rows[0].len())
            .find(|&i| {
                rows[0].get(i) == Some(bus)
                    && rows[2].get(i) == Some(message)
                    && rows[4].get(i) == Some(signal)
            })
            .unwrap()
    };
    assert_eq!(&rows[7][1], "0.010");
    assert_eq!(
        &rows[7][column("VCAN", "a_box_fault_event", "idx")],
        "PACK_OVERTEMP (0)"
    );
    assert_eq!(
        &rows[7][column("VCAN", "a_box_fault_event", "state")],
        "OFF (0)"
    );
    assert_eq!(
        &rows[7][column("CCAN", "bms_pecs_ccan", "RDCV_0_PEC")],
        "ON (1)"
    );
    std::fs::remove_dir_all(root).unwrap();
}
