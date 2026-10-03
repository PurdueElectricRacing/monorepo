use eframe::egui;

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
    pub failures: Vec<SignalFailure>,
    binding: Option<ResolvedExpectation>,
}

#[derive(Clone)]
struct ResolvedExpectation {
    generation: daqcore::superdbc::DbGeneration,
    bus: daqcore::can::BusId,
    message_index: u32,
    signals: Vec<usize>,
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

    pub fn resolve(&mut self, db: &daqcore::superdbc::BusDatabase) -> Result<(), String> {
        for expectation in &mut self.in_progress_expects {
            let index = db
                .msg_defs()
                .iter()
                .position(|m| m.name == expectation.expect.msg_name)
                .ok_or_else(|| {
                    format!(
                        "Unknown message {} on {}",
                        expectation.expect.msg_name,
                        db.bus().name
                    )
                })?;
            let message = &db.msg_defs()[index];
            let mut signals = Vec::new();
            for (name, range) in &expectation.expect.signals {
                if !range[0].is_finite() || !range[1].is_finite() || range[0] > range[1] {
                    return Err(format!("Invalid HIL range for {name}"));
                }
                signals.push(
                    message
                        .signals
                        .iter()
                        .position(|s| s.name == *name)
                        .ok_or_else(|| format!("Unknown signal {}.{name}", message.name))?,
                );
            }
            expectation.binding = Some(ResolvedExpectation {
                generation: db.database().generation(),
                bus: db.bus_id(),
                message_index: index as u32,
                signals,
            });
        }
        Ok(())
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
            if expect.result == ExpectResult::InProgress && expect.matches_message(&parsed.frame) {
                if expect.expect.signals.is_empty() {
                    expect.result = ExpectResult::Passed;
                } else {
                    let mut failures = Vec::new();
                    for (index, (sig_name, sig_range)) in expect.expect.signals.iter().enumerate() {
                        match expect
                            .binding
                            .as_ref()
                            .and_then(|binding| parsed.frame.signal(binding.signals[index]))
                        {
                            Some(sig_value) => {
                                let value = sig_value.physical;
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
            binding: None,
        }
    }

    pub fn matches_message(&self, frame: &daqcore::superdbc::DecodedFrame) -> bool {
        self.binding.as_ref().is_some_and(|b| {
            b.generation == frame.generation
                && b.bus == frame.bus
                && b.message_index == frame.msg_index
        })
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use daqcore::superdbc::*;
    fn setup() -> (BusDatabase, HilRunningTest, usize) {
        let db = SuperDbc::from_str(include_str!(
            "../../../daqcore/tests/fixtures/superdbc.json"
        ))
        .unwrap();
        let bus = db
            .buses()
            .iter()
            .find(|b| {
                b.messages
                    .iter()
                    .any(|m| m.signals.iter().any(|s| s.raw_type == RawType::Float32))
            })
            .unwrap();
        let message = bus
            .messages
            .iter()
            .find(|m| m.signals.iter().any(|s| s.raw_type == RawType::Float32))
            .unwrap();
        let index = message
            .signals
            .iter()
            .position(|s| s.raw_type == RawType::Float32)
            .unwrap();
        let mut signals = indexmap::IndexMap::new();
        signals.insert(message.signals[index].name.clone(), [-1., 1.]);
        let test = hil::config::TestFile {
            name: "probe".into(),
            description: String::new(),
            tx: Vec::new(),
            expect: vec![hil::config::Expectation {
                window: [0., 100000.],
                msg_name: message.name.clone(),
                signals,
            }],
        };
        let binding = db.bind(bus.bus_id).unwrap();
        let mut run = HilRunningTest::from_parts(
            hil::config::TestInfo {
                basename: "probe".into(),
                name: "probe".into(),
                description: String::new(),
            },
            test,
        );
        run.resolve(&binding).unwrap();
        (binding, run, index)
    }
    #[test]
    fn nan_and_missing_signal_cannot_pass_hil() {
        for missing in [false, true] {
            let (db, mut run, index) = setup();
            let binding = run.in_progress_expects[0].binding.as_ref().unwrap();
            let m = &db.msg_defs()[binding.message_index as usize];
            let mut raw: Vec<_> = m
                .signals
                .iter()
                .map(|s| match s.raw_type {
                    RawType::Unsigned => RawValue::U64(0),
                    RawType::Signed => RawValue::I64(0),
                    RawType::Float32 => RawValue::F32(0.),
                })
                .collect();
            raw[index] = RawValue::F32(f32::NAN);
            let payload = m.encode_raw(&raw).unwrap();
            let frame = db
                .decode(m.id, if missing { &[] } else { payload.data() })
                .unwrap()
                .unwrap();
            run.process_can(
                &messages::ParsedMessage {
                    timestamp: chrono::Local::now(),
                    frame,
                },
                std::time::Instant::now(),
            );
            assert!(run.in_progress_expects[0].result == ExpectResult::FailedValueOutOfRange);
            assert!(!run.in_progress_expects[0].failures.is_empty());
        }
    }
    #[test]
    fn unknown_signals_fail_resolution_and_old_generations_do_not_match() {
        let (db, mut run, _) = setup();
        let m = &db.msg_defs()[run.in_progress_expects[0]
            .binding
            .as_ref()
            .unwrap()
            .message_index as usize];
        let frame = db.decode(m.id, &[0; 8]).unwrap().unwrap();
        assert!(run.in_progress_expects[0].matches_message(&frame));
        let other = SuperDbc::from_str(include_str!(
            "../../../daqcore/tests/fixtures/superdbc.json"
        ))
        .unwrap();
        let other = other.bind(db.bus_id()).unwrap();
        let frame = other.decode(m.id, &[0; 8]).unwrap().unwrap();
        assert!(!run.in_progress_expects[0].matches_message(&frame));
        run.in_progress_expects[0]
            .expect
            .signals
            .insert("missing".into(), [0., 1.]);
        assert!(run.resolve(&db).is_err());
    }
}
