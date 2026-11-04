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

    pub messages: Vec<CapturedMessage>,

    // The index of the message to display in the HIL testing UI,
    // based on the active AcceptPolicy for the expectation.
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
                expect.result = match expect.selected_message() {
                    None => ExpectResult::FailedNoMessage,
                    Some(message) if message.is_valid() => ExpectResult::Passed,
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
            messages: Vec::new(),
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
