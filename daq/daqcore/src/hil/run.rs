use crate::{hil, superdbc};
use std::{path, time};

#[derive(PartialEq, Eq, Clone, Copy)]
pub enum ExpectResult {
    NotInWindow,
    InProgress,
    Passed,
    FailedNoMessage,
    FailedValueOutOfRange,
}

/// Single signal failure, captured at evaluation used for displaying in the UI
#[derive(Clone)]
pub enum SignalFailure {
    /// Signal present but value is out of range
    OutOfRange {
        name: String,
        value: f64,
        range: [f64; 2],
    },
    /// Signal missing from the message
    MissingSignal { name: String, range: [f64; 2] },
}

impl SignalFailure {
    pub fn name(&self) -> &str {
        match self {
            SignalFailure::OutOfRange { name, .. } => name,
            SignalFailure::MissingSignal { name, .. } => name,
        }
    }
}

/// One reception evaluated against the signals configured for an expectation.
#[derive(Clone)]
pub struct CapturedMessage {
    pub elapsed_ms: u128,
    pub signals: indexmap::IndexMap<String, Option<f64>>,
    pub failures: Vec<SignalFailure>,
}

impl CapturedMessage {
    pub fn is_valid(&self) -> bool {
        self.failures.is_empty()
    }
}

#[derive(Clone)]
pub struct InProgressExpect {
    pub expect: hil::config::Expectation,
    pub result: ExpectResult,

    /// Failures of the policy-selected message; None before any matching message.
    pub failures: Option<Vec<SignalFailure>>,
    pub messages: Vec<CapturedMessage>,
    pub selected_message_index: Option<usize>,
}

#[derive(Clone)]
pub struct HilRunningTest {
    pub test_info: hil::config::TestInfo,
    pub tx_remaining: Vec<hil::config::TxMessage>,
    pub in_progress_expects: Vec<InProgressExpect>,
}

impl ExpectResult {
    pub fn as_str(&self) -> &'static str {
        match self {
            ExpectResult::NotInWindow => "Not in window",
            ExpectResult::InProgress => "In progress",
            ExpectResult::Passed => "Passed",
            ExpectResult::FailedNoMessage => "Failed (no message)",
            ExpectResult::FailedValueOutOfRange => "Failed (value out of range)",
        }
    }

    pub fn is_finished(&self) -> bool {
        matches!(
            self,
            ExpectResult::Passed
                | ExpectResult::FailedNoMessage
                | ExpectResult::FailedValueOutOfRange
        )
    }
}

impl HilRunningTest {
    pub fn new(base: &path::Path, test_info: &hil::config::TestInfo) -> Result<Self, String> {
        let test = hil::config::load_test_from_file(base, &test_info.basename)?;
        Ok(Self::from_parts(test_info.clone(), test))
    }

    pub fn from_basename(base: &path::Path, basename: &str) -> Result<Self, String> {
        let test = hil::config::load_test_from_file(base, basename)?;
        let test_info = hil::config::TestInfo {
            basename: basename.to_string(),
            name: test.name.clone(),
            description: test.description.clone(),
        };
        Ok(Self::from_parts(test_info, test))
    }

    pub fn from_parts(test_info: hil::config::TestInfo, test: hil::config::TestFile) -> Self {
        let mut in_progress_expects = test
            .expect
            .into_iter()
            .map(InProgressExpect::new)
            .collect::<Vec<_>>();
        in_progress_expects.sort_by(|a, b| a.expect.window[0].total_cmp(&b.expect.window[0]));
        Self {
            test_info,
            tx_remaining: test.tx,
            in_progress_expects,
        }
    }

    pub fn update_expect_statuses(&mut self, elapsed: time::Duration) {
        let ts = elapsed.as_millis();

        for expect in &mut self.in_progress_expects {
            if expect.result.is_finished() {
                continue;
            };

            // Check if window for HIL test expired, resolve test state accordingly.
            if ts > expect.expect.window[1] as u128 {
                expect.result = match &expect.failures {
                    None => ExpectResult::FailedNoMessage,
                    Some(failures) if failures.is_empty() => ExpectResult::Passed,
                    Some(_) => ExpectResult::FailedValueOutOfRange,
                };

            // If the test window is not expired, and the current time is after
            // the window start, we should start checking for matching messages.
            } else if ts >= expect.expect.window[0] as u128 {
                expect.result = ExpectResult::InProgress;
            }
        }
    }

    pub fn process_can(
        &mut self,
        decoded: &superdbc::decode::DecodedMessage,
        elapsed: time::Duration,
    ) {
        self.update_expect_statuses(elapsed);

        let ts = elapsed.as_millis();
        for expect in &mut self.in_progress_expects {
            if ts >= expect.expect.window[0] as u128
                && ts <= expect.expect.window[1] as u128
                && expect.matches_message(decoded)
            {
                let mut failures = Vec::new();
                let mut signals = indexmap::IndexMap::new();

                for (sig_name, sig_range) in &expect.expect.signals {
                    signals.insert(
                        sig_name.clone(),
                        decoded.signal(sig_name).map(|s| s.physical()),
                    );
                    match decoded.signal(sig_name) {
                        Some(sig_value) => {
                            let value = sig_value.physical();

                            // If signal value out of expected bounds, record as
                            // a "failure".
                            if !value.is_finite() || value < sig_range[0] || value > sig_range[1] {
                                failures.push(SignalFailure::OutOfRange {
                                    name: sig_name.clone(),
                                    value,
                                    range: *sig_range,
                                });
                            }
                        }
                        None => {
                            failures.push(SignalFailure::MissingSignal {
                                name: sig_name.clone(),
                                range: *sig_range,
                            });
                        }
                    }
                }

                let message = CapturedMessage {
                    elapsed_ms: ts,
                    signals,
                    failures,
                };
                // Whether the current message and failures should be the new
                // items displayed in the UI. This keeps the UI representative
                // of the selected AcceptPolicy.
                let select = match expect.expect.accept {
                    // The very first matching message is the only one that counts.
                    hil::config::AcceptPolicy::First => expect.selected_message_index.is_none(),
                    // Displayed message should always be updated to the latest message received.
                    hil::config::AcceptPolicy::Last => true,
                    // Will keep updating to the latest message until a valid one
                    // is received.
                    hil::config::AcceptPolicy::Any => expect
                        .selected_message()
                        .is_none_or(|selected| !selected.is_valid()),
                };
                if select {
                    expect.selected_message_index = Some(expect.messages.len());
                    expect.failures = Some(message.failures.clone());
                }
                if !expect.result.is_finished() {
                    expect.result = match expect.expect.accept {
                        // Immediately validates the first message that arrives.
                        hil::config::AcceptPolicy::First => {
                            if message.is_valid() {
                                ExpectResult::Passed
                            } else {
                                ExpectResult::FailedValueOutOfRange
                            }
                        }
                        hil::config::AcceptPolicy::Any if message.is_valid() => {
                            ExpectResult::Passed
                        }
                        // Expectations with AcceptPolicy::Last are evaluated after
                        // the window closes.
                        _ => ExpectResult::InProgress,
                    };
                }
                expect.messages.push(message);
            }
        }
    }

    pub fn expect_counts(&self) -> (usize, usize, usize) {
        let mut not_in_window = 0;
        let mut in_progress = 0;
        let mut completed = 0;

        for expect in &self.in_progress_expects {
            match expect.result {
                ExpectResult::NotInWindow => not_in_window += 1,
                ExpectResult::InProgress => in_progress += 1,
                _ => completed += 1,
            }
        }

        (not_in_window, in_progress, completed)
    }

    /// Returns true once all expectations' time windows have closed.
    pub fn capture_complete(&self, elapsed: time::Duration) -> bool {
        self.in_progress_expects
            .iter()
            .all(|e| elapsed.as_millis() > e.expect.window[1] as u128)
    }

    pub fn is_finished(&self) -> bool {
        self.in_progress_expects
            .iter()
            .all(|e| e.result.is_finished())
    }

    pub fn has_failed(&self) -> bool {
        self.in_progress_expects.iter().any(|e| {
            matches!(
                e.result,
                ExpectResult::FailedNoMessage | ExpectResult::FailedValueOutOfRange
            )
        })
    }
}

impl InProgressExpect {
    pub fn new(expect: hil::config::Expectation) -> Self {
        Self {
            expect,
            result: ExpectResult::NotInWindow,
            failures: None,
            messages: Vec::new(),
            /// The index of the message to display in the HIL testing UI,
            /// based on the active AcceptPolicy for the expectation.
            selected_message_index: None,
        }
    }

    /// Returns the current message displayed in the UI, or None.
    pub fn selected_message(&self) -> Option<&CapturedMessage> {
        self.selected_message_index
            .and_then(|i| self.messages.get(i))
    }

    pub fn matches_message(&self, decoded: &superdbc::decode::DecodedMessage) -> bool {
        self.expect.msg_name == decoded.name()
    }
}

#[cfg(test)]
pub(crate) mod tests {
    use super::*;
    use hil::config::AcceptPolicy;

    pub(crate) fn database_document(name: &str, values: &[(&str, f32)]) -> serde_json::Value {
        let signals: Vec<_> = values
            .iter()
            .enumerate()
            .map(|(i, (name, _))| {
                serde_json::json!({
                    "signal_name": name, "description": "", "data_type": "float",
                    "raw_type": "float32", "start_bit": i * 32, "bit_length": 32,
                    "byte_order": "little_endian", "scale": 1.0, "offset": 0.0,
                    "limits": null, "unit": "", "choices": null, "display_format": null
                })
            })
            .collect();
        let document = serde_json::json!({
            "content_hash": "0".repeat(64), "versions": {"schema_version": "1.1", "hash": "0".repeat(64)},
            "buses": {"test": {"bus_id": 0, "baud_rate": 500000,
                "nodes": [{"name": "test", "is_external": false}],
                "messages": [{"id": 1, "is_extended_id": false,
                    "message_name": name, "transmitter": "test", "receivers": [],
                    "length_bytes": 8, "nominal_period_ms": null, "priority": 0,
                    "description": "", "signals": signals}]}}
        });
        document
    }

    pub(crate) fn message(name: &str, values: &[(&str, f32)]) -> superdbc::decode::DecodedMessage {
        let database =
            superdbc::database::Database::from_json(&database_document(name, values).to_string())
                .unwrap();
        let mut payload = [0_u8; 8];
        for (i, (_, value)) in values.iter().enumerate() {
            payload[i * 4..i * 4 + 4].copy_from_slice(&value.to_le_bytes());
        }
        database
            .messages()
            .next()
            .unwrap()
            .decode(&payload)
            .unwrap()
    }

    pub(crate) fn running(policy: AcceptPolicy) -> HilRunningTest {
        HilRunningTest::from_parts(
            hil::config::TestInfo {
                name: "test".into(),
                basename: "test".into(),
                description: String::new(),
            },
            hil::config::TestFile {
                name: "test".into(),
                description: String::new(),
                tx: vec![],
                expect: vec![hil::config::Expectation {
                    window: [10.0, 20.0],
                    msg_name: "telemetry".into(),
                    signals: [("temperature".into(), [30.0, 40.0])].into(),
                    accept: policy,
                }],
            },
        )
    }

    #[test]
    fn policies_capture_after_completion_and_select_the_deciding_message() {
        for (policy, selected, passed) in [
            (AcceptPolicy::First, 0, false),
            (AcceptPolicy::Last, 2, false),
            (AcceptPolicy::Any, 1, true),
        ] {
            let mut test = running(policy);
            for (ts, value) in [(10, 25.0), (15, 35.0), (20, 45.0)] {
                test.process_can(
                    &message("telemetry", &[("temperature", value)]),
                    time::Duration::from_millis(ts),
                );
            }
            test.update_expect_statuses(time::Duration::from_millis(21));
            let e = &test.in_progress_expects[0];
            assert_eq!(e.messages.len(), 3);
            assert_eq!(e.selected_message_index, Some(selected));
            assert!(
                e.result
                    == if passed {
                        ExpectResult::Passed
                    } else {
                        ExpectResult::FailedValueOutOfRange
                    }
            );
            assert_eq!(e.failures.as_ref().unwrap().is_empty(), passed);
            assert_eq!(e.messages[1].signals["temperature"], Some(35.0));
            assert_eq!(e.messages[2].elapsed_ms, 20);
            assert!(test.capture_complete(time::Duration::from_millis(21)));
        }
    }

    #[test]
    fn capture_obeys_window_name_and_keeps_duplicates() {
        let mut test = running(AcceptPolicy::Any);
        let valid = message("telemetry", &[("temperature", 35.0)]);
        for ts in [9, 10, 10, 20, 21] {
            test.process_can(&valid, time::Duration::from_millis(ts));
        }
        test.process_can(
            &message("other", &[("temperature", 35.0)]),
            time::Duration::from_millis(15),
        );
        let e = &test.in_progress_expects[0];
        assert_eq!(e.messages.len(), 3);
        assert_eq!(e.selected_message_index, Some(0));
        assert!(e.result == ExpectResult::Passed);
        assert!(!test.capture_complete(time::Duration::from_millis(20)));
    }

    #[test]
    fn invalid_any_waits_and_no_message_is_distinct() {
        let mut test = running(AcceptPolicy::Any);
        for (ts, value) in [(10, f32::NAN), (11, f32::INFINITY), (12, 25.0)] {
            test.process_can(
                &message("telemetry", &[("temperature", value)]),
                time::Duration::from_millis(ts),
            );
            assert!(test.in_progress_expects[0].result == ExpectResult::InProgress);
        }
        test.process_can(&message("telemetry", &[]), time::Duration::from_millis(13));
        let e = &test.in_progress_expects[0];
        assert_eq!(e.selected_message_index, Some(3));
        assert!(e.messages.iter().all(|m| !m.is_valid()));
        assert_eq!(e.selected_message().unwrap().signals["temperature"], None);
        assert!(matches!(
            e.failures.as_ref().unwrap()[0],
            SignalFailure::MissingSignal { .. }
        ));
        test.update_expect_statuses(time::Duration::from_millis(21));
        assert!(test.in_progress_expects[0].result == ExpectResult::FailedValueOutOfRange);
        for policy in [AcceptPolicy::First, AcceptPolicy::Any, AcceptPolicy::Last] {
            let mut empty = running(policy);
            empty.update_expect_statuses(time::Duration::from_millis(21));
            assert!(empty.in_progress_expects[0].result == ExpectResult::FailedNoMessage);
            assert!(empty.in_progress_expects[0].selected_message().is_none());
        }
    }

    #[test]
    fn first_pass_is_stable_and_last_can_recover() {
        for policy in [AcceptPolicy::First, AcceptPolicy::Last] {
            let mut test = running(policy);
            for (ts, value) in [(10, 35.0), (11, 25.0), (12, 40.0)] {
                test.process_can(
                    &message("telemetry", &[("temperature", value)]),
                    time::Duration::from_millis(ts),
                );
            }
            test.update_expect_statuses(time::Duration::from_millis(21));
            let e = &test.in_progress_expects[0];
            assert!(e.result == ExpectResult::Passed);
            assert_eq!(
                e.selected_message().unwrap().signals["temperature"],
                Some(if matches!(policy, AcceptPolicy::First) {
                    35.0
                } else {
                    40.0
                })
            );
            assert_eq!(e.messages.len(), 3);
        }
    }

    #[test]
    fn overlapping_expects_require_all_signals_in_one_message() {
        let mut test = running(AcceptPolicy::Any);
        let mut second = test.in_progress_expects[0].expect.clone();
        second.signals.insert("voltage".into(), [30.0, 40.0]);
        test.in_progress_expects.push(InProgressExpect::new(second));
        for values in [
            [("temperature", 35.0), ("voltage", 25.0)],
            [("temperature", 25.0), ("voltage", 35.0)],
        ] {
            test.process_can(
                &message("telemetry", &values),
                time::Duration::from_millis(15),
            );
        }
        assert!(test.in_progress_expects[0].result == ExpectResult::Passed);
        assert!(test.in_progress_expects[1].result == ExpectResult::InProgress);
        assert!(
            test.in_progress_expects
                .iter()
                .all(|e| e.messages.len() == 2)
        );
    }

    #[test]
    fn message_presence_expectations_follow_policy_timing() {
        for policy in [AcceptPolicy::First, AcceptPolicy::Any, AcceptPolicy::Last] {
            let mut test = running(policy);
            test.in_progress_expects[0].expect.signals.clear();
            test.process_can(&message("telemetry", &[]), time::Duration::from_millis(10));
            let e = &test.in_progress_expects[0];
            assert!(e.messages[0].is_valid());
            assert_eq!(
                e.result == ExpectResult::Passed,
                !matches!(policy, AcceptPolicy::Last)
            );
            test.update_expect_statuses(time::Duration::from_millis(21));
            assert!(test.in_progress_expects[0].result == ExpectResult::Passed);
        }
    }
}
