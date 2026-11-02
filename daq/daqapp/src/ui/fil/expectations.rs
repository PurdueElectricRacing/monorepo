pub fn expectation_name(event: &daqcore::can::driver::FilExpectationEvent) -> String {
    let script = event
        .script
        .rsplit(['/', '\\'])
        .next()
        .unwrap_or(&event.script);
    let index = event.check_id.rsplit("/expect/").next().unwrap_or("?");
    let attachment = event
        .check_id
        .strip_prefix("stimulus/")
        .and_then(|rest| rest.split('/').next())
        .unwrap_or("?");
    format!("{script} · check {index} · script {attachment}")
}

pub fn expectation_bytes(data: &[u8]) -> String {
    if data.is_empty() {
        return "(empty)".into();
    }
    data.iter()
        .map(|byte| format!("{byte:02X}"))
        .collect::<Vec<_>>()
        .join(" ")
}

pub fn expectation_ms(ns: u64) -> String {
    let whole = ns / 1_000_000;
    let remainder = ns % 1_000_000;
    if remainder == 0 {
        whole.to_string()
    } else {
        format!("{whole}.{:06}", remainder)
            .trim_end_matches('0')
            .to_owned()
    }
}
