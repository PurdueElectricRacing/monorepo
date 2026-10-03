#[test]
fn cli_parses_legacy_records_and_reports_errors() {
    let root = std::env::temp_dir().join(format!("superdbc-cli-{}", std::process::id()));
    let input = root.join("logs");
    let output = root.join("output");
    std::fs::create_dir_all(&input).unwrap();
    let mut record = Vec::new();
    record.extend(10u32.to_le_bytes());
    record.extend(1u32.to_le_bytes());
    record.extend([0u8; 8]);
    std::fs::write(input.join("capture.log"), record).unwrap();
    let fixture = std::path::Path::new(env!("CARGO_MANIFEST_DIR"))
        .join("../daqcore/tests/fixtures/superdbc.json");
    let result = std::process::Command::new(env!("CARGO_BIN_EXE_daqcli"))
        .arg(&fixture)
        .arg(&input)
        .arg(&output)
        .output()
        .unwrap();
    assert!(
        result.status.success(),
        "{}",
        String::from_utf8_lossy(&result.stderr)
    );
    let csv = std::fs::read_to_string(output.join("out_000.csv")).unwrap();
    assert!(csv.contains("a_box_fault_event"));
    assert!(csv.contains("Signal Unit"));
    assert!(csv.contains("OFF (0)"));
    let result = std::process::Command::new(env!("CARGO_BIN_EXE_daqcli"))
        .arg(&fixture)
        .arg(root.join("missing"))
        .arg(&output)
        .output()
        .unwrap();
    assert!(!result.status.success());
    assert!(!String::from_utf8_lossy(&result.stderr).contains("panicked"));
    std::fs::remove_dir_all(root).unwrap();
}
