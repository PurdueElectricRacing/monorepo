use crate::hil::config::ExecutionMode;
use crate::{frame, hil};
use std::{path, time};

pub enum HilCommand {
    StartTest(hil::config::TestInfo),
    StartPreset(hil::config::PresetInfo),
    Stop,
}

#[derive(Clone, Copy, PartialEq, Eq)]
pub enum HilStatus {
    Idle,
    Running,
}

#[derive(Clone)]
pub struct HilSnapshot {
    pub status: HilStatus,
    pub elapsed_ms: u128,
    pub capture_complete: bool,
    pub preset: Option<hil::config::PresetInfo>,
    pub tests: Vec<hil::run::HilRunningTest>,
    pub start_error: Option<String>,
    pub finished: bool,
    pub skipped_from: Option<usize>,
}

impl HilSnapshot {
    pub fn idle() -> Self {
        Self {
            status: HilStatus::Idle,
            elapsed_ms: 0,
            capture_complete: true,
            preset: None,
            tests: Vec::new(),
            start_error: None,
            finished: false,
            skipped_from: None,
        }
    }
}

enum HilState {
    Idle {
        start_error: Option<String>,
    },
    Running {
        start_time: time::Instant,
        preset: Option<hil::config::PresetInfo>,
        tests: Vec<hil::run::HilRunningTest>,
        active_test: usize,
        active_start_time: std::time::Instant,
        stopped_early: bool,
    },
}

pub struct HilEngine {
    state: HilState,
    base: path::PathBuf,
}

impl HilEngine {
    pub fn new(base: path::PathBuf) -> Self {
        Self {
            state: HilState::Idle { start_error: None },
            base,
        }
    }

    pub fn is_running(&self) -> bool {
        matches!(self.state, HilState::Running { .. })
    }

    fn all_finished(&self) -> bool {
        match &self.state {
            HilState::Running {
                tests,
                stopped_early,
                ..
            } => *stopped_early || tests.iter().all(|t| t.is_finished()),
            HilState::Idle { .. } => false,
        }
    }

    pub fn capture_complete(&self, now: time::Instant) -> bool {
        match &self.state {
            HilState::Running {
                start_time,
                preset,
                tests,
                active_test,
                stopped_early,
                ..
            } => match preset.as_ref().map_or(ExecutionMode::Parallel, |p| p.mode) {
                ExecutionMode::SequentialAlways | ExecutionMode::SequentialStopFail => {
                    // Sequential tests advance as soon as their results are decided.
                    *stopped_early || *active_test >= tests.len()
                }
                ExecutionMode::Parallel => tests
                    .iter()
                    .all(|test| test.capture_complete(now.saturating_duration_since(*start_time))),
            },
            HilState::Idle { .. } => true,
        }
    }

    pub fn handle_command(&mut self, command: HilCommand, now: time::Instant) {
        match command {
            HilCommand::StartTest(test_info) => {
                match hil::run::HilRunningTest::new(&self.base, &test_info) {
                    Ok(test) => self.begin(None, vec![test], now),
                    Err(err) => self.fail_start(format!("Failed to start test: {err}")),
                }
            }

            HilCommand::StartPreset(preset) => {
                let mut tests = Vec::with_capacity(preset.tests.len());
                for basename in &preset.tests {
                    match hil::run::HilRunningTest::from_basename(&self.base, basename) {
                        Ok(test) => tests.push(test),
                        Err(err) => {
                            self.fail_start(format!("Failed to start test: {err}"));
                            return;
                        }
                    }
                }
                self.begin(Some(preset), tests, now);
            }
            HilCommand::Stop => {
                self.state = HilState::Idle { start_error: None };
            }
        }
    }

    pub fn process_parsed(&mut self, parsed: &frame::ParsedFrame, now: time::Instant) {
        if let HilState::Running {
            start_time,
            preset,
            tests,
            active_test,
            active_start_time,
            stopped_early,
        } = &mut self.state
        {
            if *stopped_early {
                return;
            }

            let Some(decoded) = &parsed.decoded else {
                return;
            };

            let mode = preset.as_ref().map_or(ExecutionMode::Parallel, |p| p.mode);

            match mode {
                ExecutionMode::Parallel => {
                    for test in tests.iter_mut() {
                        test.process_can(decoded, now.saturating_duration_since(*start_time));
                    }
                }

                ExecutionMode::SequentialAlways | ExecutionMode::SequentialStopFail => {
                    if let Some(test) = tests.get_mut(*active_test) {
                        test.process_can(
                            decoded,
                            now.saturating_duration_since(*active_start_time),
                        );
                    }
                }
            }
        }
    }

    pub fn tick(&mut self, now: time::Instant) {
        if let HilState::Running {
            start_time,
            preset,
            tests,
            active_test,
            active_start_time,
            stopped_early,
        } = &mut self.state
        {
            if *stopped_early {
                return;
            }

            let mode = preset.as_ref().map_or(ExecutionMode::Parallel, |p| p.mode);

            match mode {
                ExecutionMode::Parallel => {
                    for test in tests.iter_mut() {
                        test.update_expect_statuses(now.saturating_duration_since(*start_time));
                    }
                }

                ExecutionMode::SequentialAlways | ExecutionMode::SequentialStopFail => {
                    if let Some(test) = tests.get_mut(*active_test) {
                        test.update_expect_statuses(
                            now.saturating_duration_since(*active_start_time),
                        );

                        if test.is_finished() {
                            let stop_on_failure = matches!(mode, ExecutionMode::SequentialStopFail);

                            if stop_on_failure && test.has_failed() {
                                *stopped_early = true;
                            } else {
                                *active_test += 1;
                                *active_start_time = now;
                            }
                        }
                    }
                }
            }
        }
    }

    pub fn snapshot(&self, now: time::Instant) -> HilSnapshot {
        match &self.state {
            HilState::Idle { start_error } => HilSnapshot {
                status: HilStatus::Idle,
                elapsed_ms: 0,
                capture_complete: true,
                preset: None,
                tests: Vec::new(),
                start_error: start_error.clone(),
                finished: false,
                skipped_from: None,
            },
            HilState::Running {
                start_time,
                preset,
                tests,
                active_test,
                stopped_early,
                ..
            } => HilSnapshot {
                status: HilStatus::Running,
                elapsed_ms: now.saturating_duration_since(*start_time).as_millis(),
                capture_complete: self.capture_complete(now),
                preset: preset.clone(),
                tests: tests.clone(),
                start_error: None,

                finished: self.all_finished(),
                skipped_from: if *stopped_early {
                    Some(*active_test + 1)
                } else {
                    None
                },
            },
        }
    }

    fn begin(
        &mut self,
        preset: Option<hil::config::PresetInfo>,
        tests: Vec<hil::run::HilRunningTest>,
        now: time::Instant,
    ) {
        self.state = HilState::Running {
            start_time: now,
            preset,
            tests,
            active_test: 0,
            active_start_time: now,
            stopped_early: false,
        };
    }

    fn fail_start(&mut self, message: String) {
        self.state = HilState::Idle {
            start_error: Some(message),
        };
    }
}
