use crate::hil;

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
    pub failures: Vec<SignalFailure>,
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
    pub fn new(base: &std::path::Path, test_info: &hil::config::TestInfo) -> Result<Self, String> {
        let test = hil::config::load_test_from_file(base, &test_info.basename)?;
        Ok(Self::from_parts(test_info.clone(), test))
    }

    pub fn from_basename(base: &std::path::Path, basename: &str) -> Result<Self, String> {
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

    pub fn update_expect_statuses(&mut self, elapsed: std::time::Duration) {
        let ts = elapsed.as_millis();
        for expect in &mut self.in_progress_expects {
            match expect.result {
                ExpectResult::NotInWindow => {
                    if ts >= expect.expect.window[0] as u128 {
                        expect.result = if ts > expect.expect.window[1] as u128 {
                            ExpectResult::FailedNoMessage
                        } else {
                            ExpectResult::InProgress
                        };
                    }
                }
                ExpectResult::InProgress => {
                    if ts > expect.expect.window[1] as u128 {
                        expect.result = ExpectResult::FailedNoMessage;
                    }
                }
                _ => {}
            }
        }
    }

    pub fn process_can(
        &mut self,
        decoded: &can_decode::DecodedMessage,
        elapsed: std::time::Duration,
    ) {
        self.update_expect_statuses(elapsed);

        for expect in &mut self.in_progress_expects {
            if expect.result == ExpectResult::InProgress && expect.matches_message(decoded) {
                if expect.expect.signals.is_empty() {
                    expect.result = ExpectResult::Passed;
                } else {
                    let mut failures = Vec::new();
                    for (sig_name, sig_range) in &expect.expect.signals {
                        match decoded.signals.get(sig_name) {
                            Some(sig_value) => {
                                let value = sig_value.value.physical;
                                if !value.is_finite()
                                    || value < sig_range[0]
                                    || value > sig_range[1]
                                {
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

                    expect.result = if failures.is_empty() {
                        ExpectResult::Passed
                    } else {
                        ExpectResult::FailedValueOutOfRange
                    };
                    expect.failures = failures;
                }
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
            failures: Vec::new(),
        }
    }

    pub fn matches_message(&self, decoded: &can_decode::DecodedMessage) -> bool {
        self.expect.msg_name == decoded.name
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    fn test() -> HilRunningTest {
        HilRunningTest::from_parts(hil::config::TestInfo { basename: "test".into(), name: "test".into(), description: String::new() },
            serde_json::from_str(r#"{"name":"test","description":"","expect":[{"window":[100,200],"msg_name":"test","signals":{"value":[1,2]}}]}"#).unwrap())
    }
    #[test]
    fn skipped_window_expires_and_missing_signals_fail() {
        let mut run = test();
        run.update_expect_statuses(std::time::Duration::from_millis(201));
        assert!(matches!(
            run.in_progress_expects[0].result,
            ExpectResult::FailedNoMessage
        ));
        assert!(run.is_finished());
        let mut run = test();
        let msg = can_decode::DecodedMessage {
            name: "test".into(),
            msg_id: 1,
            is_extended: false,
            tx_node: "test".into(),
            signals: Default::default(),
        };
        run.process_can(&msg, std::time::Duration::from_millis(100));
        assert!(matches!(
            run.in_progress_expects[0].result,
            ExpectResult::FailedValueOutOfRange
        ));
        assert!(matches!(
            run.in_progress_expects[0].failures[0],
            SignalFailure::MissingSignal { .. }
        ));
        assert!(run.is_finished());
    }
}
