
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
#[cfg(test)]
mod expectation_tests {
    use std::collections::HashMap;
    use crate::ui::fil::events::update_expectations;
    use super::super::{
        FIL_EXPECTATION_HISTORY_LIMIT, clear_expectations, network_result_is_current,
    };

    fn event(
        status: daqcore::can::driver::FilExpectationStatus,
    ) -> daqcore::can::driver::FilExpectationEvent {
        daqcore::can::driver::FilExpectationEvent {
            check_id: "stimulus/0/script/expect/0".into(),
            script: "script".into(),
            status,
            expected_bus: "vehicle".into(),
            expected_id: 0x321,
            expected_extended: false,
            expected_data: vec![1, 2],
            window_start_ns: 10,
            window_end_ns: 20,
            matched_bus: None,
            matched_id: None,
            matched_data: None,
            matched_origin: None,
            matched_time_ns: None,
            reason: None,
        }
    }

    #[test]
    fn network_results_are_accepted_only_for_the_selected_path() {
        let selected = std::path::Path::new("new-network.json");
        assert!(network_result_is_current(Some(selected), selected));
        assert!(!network_result_is_current(
            Some(selected),
            std::path::Path::new("old-network.json")
        ));
        assert!(!network_result_is_current(None, selected));
    }

    #[test]
    fn history_is_bounded() {
        let mut events = HashMap::new();
        let mut order = Vec::new();
        for index in 0..=FIL_EXPECTATION_HISTORY_LIMIT {
            let mut check = event(daqcore::can::driver::FilExpectationStatus::Pending);
            check.check_id = index.to_string();
            update_expectations(&mut events, &mut order, check);
        }
        assert_eq!(events.len(), FIL_EXPECTATION_HISTORY_LIMIT);
        assert_eq!(order.len(), FIL_EXPECTATION_HISTORY_LIMIT);
        assert!(!events.contains_key("0"));
    }

    #[test]
    fn completed_run_history_is_retained_until_explicit_reset() {
        let mut events = HashMap::new();
        let mut order = Vec::new();
        update_expectations(
            &mut events,
            &mut order,
            event(daqcore::can::driver::FilExpectationStatus::Fail),
        );
        assert_eq!(events.len(), 1);
        clear_expectations(&mut events, &mut order);
        assert!(events.is_empty());
        assert!(order.is_empty());
    }

    #[test]
    fn lifecycle_replaces_pending_check_without_duplicate_history() {
        let mut events = HashMap::new();
        let mut order = Vec::new();
        update_expectations(
            &mut events,
            &mut order,
            event(daqcore::can::driver::FilExpectationStatus::Pending),
        );
        let mut passed = event(daqcore::can::driver::FilExpectationStatus::Pass);
        passed.matched_bus = Some("vehicle".into());
        passed.matched_id = Some(0x321);
        passed.matched_data = Some(vec![1, 2]);
        passed.matched_time_ns = Some(15);
        update_expectations(&mut events, &mut order, passed);
        assert_eq!(order.len(), 1);
        assert_eq!(events.len(), 1);
        let stored = events
            .values()
            .next()
            .expect("updated check should remain in history");
        assert_eq!(
            stored.status,
            daqcore::can::driver::FilExpectationStatus::Pass
        );
        assert_eq!(stored.matched_time_ns, Some(15));
        clear_expectations(&mut events, &mut order);
        assert!(events.is_empty());
        assert!(order.is_empty());
    }
}

