use eframe::egui;

use crate::{hil, messages};

#[derive(PartialEq, Eq)]
pub enum ExpectResult {
    NotInWindow,
    InProgress,
    Passed,
    FailedNoMessage,
    FailedValueOutOfRange,
}

/// Single signal failure, captured at evaluation used for displaying in the UI
pub enum SignalFailure {
    /// Signal present but value is out of range
    OutOfRange {
        name: String,
        value: f64,
        range: [f64; 2],
    },
    /// Signal missing from the message
    MissingSignal {
        name: String,
        range: [f64; 2],
    },
}

impl SignalFailure {
    pub fn name(&self) -> &str {
        match self {
            SignalFailure::OutOfRange { name, .. } => name,
            SignalFailure::MissingSignal { name, ..} => name,
        }
    }
}

pub struct InProgressExpect {
    pub expect: hil::config::Expectation,
    pub result: ExpectResult,
    pub failures: Vec<SignalFailure>,
}

pub struct HilRunningTest {
    pub test_info: hil::config::TestInfo,
    pub tx_remaining: Vec<hil::config::TxMessage>,
    pub in_progress_expects: Vec<InProgressExpect>,
}

pub enum HilState {
    Idle,
    Running {
        start_time: std::time::Instant,
        preset_info: Option<hil::config::PresetInfo>,
        tests: Vec<HilRunningTest>,
    },
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
        let mut in_progress_expects = test
            .expect
            .into_iter()
            .map(InProgressExpect::new)
            .collect::<Vec<_>>();
        in_progress_expects.sort_by(|a, b| a.expect.window[0].total_cmp(&b.expect.window[0]));
        Ok(Self {
            test_info: test_info.clone(),
            tx_remaining: test.tx,
            in_progress_expects,
        })
    }

    pub fn update_expect_statuses(&mut self, start_time: std::time::Instant) {
        let ts = start_time.elapsed().as_millis();
        for expect in &mut self.in_progress_expects {
            match expect.result {
                ExpectResult::NotInWindow => {
                    if ts >= expect.expect.window[0] as u128 {
                        expect.result = ExpectResult::InProgress;
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
        parsed: &messages::ParsedMessage,
        start_time: std::time::Instant,
    ) {
        self.update_expect_statuses(start_time);

        for expect in &mut self.in_progress_expects {
            if expect.result == ExpectResult::InProgress && expect.matches_message(&parsed.decoded)
            {
                if expect.expect.signals.is_empty() {
                    expect.result = ExpectResult::Passed;
                } else {
                    let mut failures =  Vec::new();
                    for (sig_name, sig_range) in &expect.expect.signals {
                        match parsed.decoded.signals.get(sig_name) {
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
