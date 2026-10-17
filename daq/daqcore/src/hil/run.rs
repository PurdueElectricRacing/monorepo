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

#[derive(Clone)]
pub struct InProgressExpect {
    pub expect: hil::config::Expectation,
    pub result: ExpectResult,

    /// None until a matching message arrives; an empty vector means the
    /// last message received was valid.
    pub failures: Option<Vec<SignalFailure>>,
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

        for expect in &mut self.in_progress_expects {
            if expect.result == ExpectResult::InProgress && expect.matches_message(decoded) {
                let mut failures = Vec::new();

                for (sig_name, sig_range) in &expect.expect.signals {
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

                expect.result = match expect.expect.accept {
                    // Only concerned with the very first matching message,
                    // immediately render a decision on whether the test passes.
                    hil::config::AcceptPolicy::First => {
                        if failures.is_empty() {
                            ExpectResult::Passed
                        } else {
                            ExpectResult::FailedValueOutOfRange
                        }
                    }
                    // Will accept any message, but does not automatically fail
                    // if signals are incorrect.
                    hil::config::AcceptPolicy::Any if failures.is_empty() => ExpectResult::Passed,
                    // In either case, will not confirm test result until the window
                    // has fully expired.
                    hil::config::AcceptPolicy::Any | hil::config::AcceptPolicy::Last => {
                        ExpectResult::InProgress
                    }
                };
                expect.failures = Some(failures);
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
            failures: None,
        }
    }

    pub fn matches_message(&self, decoded: &superdbc::decode::DecodedMessage) -> bool {
        self.expect.msg_name == decoded.name()
    }
}
