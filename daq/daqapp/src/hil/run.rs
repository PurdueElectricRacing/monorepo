use eframe::egui;

use crate::hil::config::AcceptPolicy;
use crate::{hil, messages};

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

#[derive(Clone)]
pub struct InProgressExpect {
    pub expect: hil::config::Expectation,
    pub result: ExpectResult,
    /// None until a matching message arrives; an empty vector means it was valid.
    pub last_failure: Option<Vec<SignalFailure>>,
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

    pub fn as_color32(&self) -> egui::Color32 {
        match self {
            ExpectResult::NotInWindow => egui::Color32::GRAY,
            ExpectResult::InProgress => egui::Color32::YELLOW,
            ExpectResult::Passed => egui::Color32::GREEN,
            ExpectResult::FailedNoMessage | ExpectResult::FailedValueOutOfRange => {
                egui::Color32::RED
            }
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
    pub fn new(test_info: &hil::config::TestInfo) -> Result<Self, String> {
        let test = hil::config::load_test_from_file(&test_info.basename)?;
        Ok(Self::from_parts(test_info.clone(), test))
    }

    pub fn from_basename(basename: &str) -> Result<Self, String> {
        let test = hil::config::load_test_from_file(basename)?;
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

    pub fn update_expect_statuses(&mut self, start_time: std::time::Instant) {
        self.update_expect_statuses_at(start_time.elapsed().as_millis());
    }

    fn update_expect_statuses_at(&mut self, ts: u128) {
        for expect in &mut self.in_progress_expects {
            if expect.result.is_finished() {
                continue;
            }
            // Check expiration first so a tick that skips the window cannot open it.
            if ts > expect.expect.window[1] as u128 {
                expect.result = match &expect.last_failure {
                    None => ExpectResult::FailedNoMessage,
                    Some(failures) if failures.is_empty() => ExpectResult::Passed,
                    Some(_) => ExpectResult::FailedValueOutOfRange,
                };
            } else if ts >= expect.expect.window[0] as u128 {
                expect.result = ExpectResult::InProgress;
            }
        }
    }

    pub fn process_can(
        &mut self,
        parsed: &messages::ParsedMessage,
        start_time: std::time::Instant,
    ) {
        self.process_can_at(&parsed.decoded, start_time.elapsed().as_millis());
    }

    fn process_can_at(&mut self, decoded: &can_decode::DecodedMessage, ts: u128) {
        self.update_expect_statuses_at(ts);

        for expect in &mut self.in_progress_expects {
            if expect.result == ExpectResult::InProgress && expect.matches_message(decoded) {
                let mut failures = Vec::new();
                for (sig_name, sig_range) in &expect.expect.signals {
                    match decoded.signals.get(sig_name) {
                        Some(sig_value) => {
                            let value = sig_value.value.physical;
                            if value < sig_range[0] || value > sig_range[1] {
                                failures.push(SignalFailure::OutOfRange {
                                    name: sig_name.clone(),
                                    value,
                                    range: *sig_range,
                                });
                            }
                        }
                        None => failures.push(SignalFailure::MissingSignal {
                            name: sig_name.clone(),
                            range: *sig_range,
                        }),
                    }
                }

                expect.result = match expect.expect.accept {
                    AcceptPolicy::First => {
                        if failures.is_empty() {
                            ExpectResult::Passed
                        } else {
                            ExpectResult::FailedValueOutOfRange
                        }
                    }
                    AcceptPolicy::Any if failures.is_empty() => ExpectResult::Passed,
                    AcceptPolicy::Any | AcceptPolicy::Last => ExpectResult::InProgress,
                };
                expect.last_failure = Some(failures);
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

    pub fn is_finished(&self) -> bool {
        self.in_progress_expects
            .iter()
            .all(|e| e.result.is_finished())
    }
}

impl InProgressExpect {
    pub fn new(expect: hil::config::Expectation) -> Self {
        Self {
            expect,
            result: ExpectResult::NotInWindow,
            last_failure: None,
        }
    }

    pub fn matches_message(&self, decoded: &can_decode::DecodedMessage) -> bool {
        self.expect.msg_name == decoded.name
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    fn running_test(policy: AcceptPolicy) -> HilRunningTest {
        let expect = hil::config::Expectation {
            window: [10.0, 20.0],
            msg_name: "telemetry".into(),
            signals: [("temperature".into(), [30.0, 40.0])].into(),
            accept: policy,
        };
        HilRunningTest::from_parts(
            hil::config::TestInfo {
                basename: "test".into(),
                name: "test".into(),
                description: String::new(),
            },
            hil::config::TestFile {
                name: "test".into(),
                description: String::new(),
                tx: Vec::new(),
                expect: vec![expect],
            },
        )
    }

    fn message(values: &[(&str, f64)]) -> can_decode::DecodedMessage {
        can_decode::DecodedMessage {
            name: "telemetry".into(),
            msg_id: 1,
            is_extended: false,
            tx_node: "test".into(),
            signals: values
                .iter()
                .map(|(name, value)| {
                    (
                        name.to_string(),
                        can_decode::DecodedSignal {
                            name: name.to_string(),
                            value: can_decode::DecodedSignalValue::new_float_backed_numeric(*value),
                            unit: String::new(),
                        },
                    )
                })
                .collect(),
        }
    }

    #[test]
    fn policies_select_the_correct_message() {
        for (values, expected) in [
            (
                [25.0, 35.0],
                [
                    ExpectResult::FailedValueOutOfRange,
                    ExpectResult::Passed,
                    ExpectResult::Passed,
                ],
            ),
            (
                [35.0, 25.0],
                [
                    ExpectResult::Passed,
                    ExpectResult::FailedValueOutOfRange,
                    ExpectResult::Passed,
                ],
            ),
        ] {
            for (policy, expected) in [AcceptPolicy::First, AcceptPolicy::Last, AcceptPolicy::Any]
                .into_iter()
                .zip(expected)
            {
                let mut test = running_test(policy);
                test.process_can_at(&message(&[("temperature", values[0])]), 10);
                test.process_can_at(&message(&[("temperature", values[1])]), 20);
                if matches!(policy, AcceptPolicy::Last) {
                    assert!(test.in_progress_expects[0].result == ExpectResult::InProgress);
                    assert!(!test.is_finished());
                }
                test.update_expect_statuses_at(21);
                assert!(
                    test.in_progress_expects[0].result == expected,
                    "policy: {policy:?}, values: {values:?}"
                );
                assert!(test.is_finished());
                assert_eq!(
                    test.in_progress_expects[0]
                        .last_failure
                        .as_ref()
                        .unwrap()
                        .is_empty(),
                    expected == ExpectResult::Passed
                );
            }
        }
    }

    #[test]
    fn any_waits_for_a_valid_message_or_timeout() {
        let mut test = running_test(AcceptPolicy::Any);
        test.process_can_at(&message(&[("temperature", 25.0)]), 10);
        test.process_can_at(&message(&[]), 20);
        assert!(!test.is_finished());
        test.update_expect_statuses_at(21);
        assert!(test.in_progress_expects[0].result == ExpectResult::FailedValueOutOfRange);
        assert!(matches!(
            test.in_progress_expects[0]
                .last_failure
                .as_ref()
                .unwrap()
                .as_slice(),
            [SignalFailure::MissingSignal { .. }]
        ));
    }

    #[test]
    fn only_matching_messages_inside_the_window_count() {
        for policy in [AcceptPolicy::First, AcceptPolicy::Last, AcceptPolicy::Any] {
            let mut test = running_test(policy);
            let valid = message(&[("temperature", 35.0)]);
            test.process_can_at(&valid, 9);
            let mut unrelated = valid.clone();
            unrelated.name = "other".into();
            test.process_can_at(&unrelated, 15);
            test.process_can_at(&valid, 21);
            assert!(test.in_progress_expects[0].result == ExpectResult::FailedNoMessage);
            assert!(test.in_progress_expects[0].last_failure.is_none());

            let mut skipped = running_test(policy);
            skipped.process_can_at(&valid, 21);
            assert!(skipped.in_progress_expects[0].result == ExpectResult::FailedNoMessage);
            assert!(skipped.in_progress_expects[0].last_failure.is_none());
        }
    }

    #[test]
    fn tick_without_messages_finishes_all_policies() {
        for policy in [AcceptPolicy::First, AcceptPolicy::Last, AcceptPolicy::Any] {
            let mut test = running_test(policy);
            test.update_expect_statuses_at(21);
            assert!(test.in_progress_expects[0].result == ExpectResult::FailedNoMessage);
        }
    }

    #[test]
    fn presence_only_last_still_waits_for_window_closure() {
        for policy in [AcceptPolicy::First, AcceptPolicy::Last, AcceptPolicy::Any] {
            let mut test = running_test(policy);
            test.in_progress_expects[0].expect.signals.clear();
            test.process_can_at(&message(&[]), 10);
            assert_eq!(test.is_finished(), !matches!(policy, AcceptPolicy::Last));
            assert!(
                test.in_progress_expects[0]
                    .last_failure
                    .as_ref()
                    .unwrap()
                    .is_empty()
            );
            test.update_expect_statuses_at(21);
            assert!(test.in_progress_expects[0].result == ExpectResult::Passed);
        }
    }

    #[test]
    fn any_requires_all_signals_to_pass_in_one_message() {
        let mut test = running_test(AcceptPolicy::Any);
        test.in_progress_expects[0]
            .expect
            .signals
            .insert("voltage".into(), [10.0, 20.0]);
        test.process_can_at(&message(&[("temperature", 35.0), ("voltage", 0.0)]), 10);
        test.process_can_at(&message(&[("temperature", 25.0), ("voltage", 15.0)]), 15);
        assert!(!test.is_finished());
        test.update_expect_statuses_at(21);
        assert!(test.in_progress_expects[0].result == ExpectResult::FailedValueOutOfRange);
    }
}
