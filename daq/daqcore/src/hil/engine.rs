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

    pub fn all_finished(&self) -> bool {
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
                start_time, tests, ..
            } => tests
                .iter()
                .all(|test| test.capture_complete(now.saturating_duration_since(*start_time))),
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

            let mode = match preset.as_ref() {
                Some(preset_info) => &preset_info.mode,
                None => &ExecutionMode::Parallel,
            };

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

            let mode = match preset.as_ref() {
                Some(preset_info) => &preset_info.mode,
                None => &ExecutionMode::Parallel,
            };

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

#[cfg(test)]
mod tests {
    use super::*;
    use hil::{
        config::AcceptPolicy,
        run::tests::{message, running},
    };

    #[test]
    fn snapshots_keep_collecting_after_results_and_stop_at_last_window() {
        let start = time::Instant::now();
        let mut engine = HilEngine::new(path::PathBuf::new());
        let first = running(AcceptPolicy::Any);
        let mut second = running(AcceptPolicy::First);
        second.in_progress_expects[0].expect.window[1] = 30.0;
        engine.begin(None, vec![first, second], start);
        let decoded = message("telemetry", &[("temperature", 35.0)]);
        let mut frame = frame::ParsedFrame {
            timestamp: crate::time::Time::now(),
            identity: decoded.key().identity,
            kind: frame::FrameKind::Data,
            dlc: 8,
            raw_bytes: vec![],
            decoded: Some(decoded),
        };
        engine.process_parsed(&frame, start + time::Duration::from_millis(10));
        assert!(engine.all_finished());
        assert!(
            !engine
                .snapshot(start + time::Duration::from_millis(10))
                .capture_complete
        );
        frame.decoded = Some(message("telemetry", &[("temperature", 25.0)]));
        engine.process_parsed(&frame, start + time::Duration::from_millis(25));
        let boundary = engine.snapshot(start + time::Duration::from_millis(30));
        assert!(!boundary.capture_complete);
        assert_eq!(boundary.tests[0].in_progress_expects[0].messages.len(), 1);
        assert_eq!(boundary.tests[1].in_progress_expects[0].messages.len(), 2);
        let end = start + time::Duration::from_millis(31);
        engine.tick(end);
        assert!(engine.snapshot(end).capture_complete);
        engine.process_parsed(&frame, end);
        assert_eq!(
            engine.snapshot(end).tests[1].in_progress_expects[0]
                .messages
                .len(),
            2
        );
        // Previously delivered snapshots remain immutable as collection proceeds.
        assert_eq!(boundary.tests[1].in_progress_expects[0].messages.len(), 2);
        engine.handle_command(HilCommand::Stop, end);
        assert!(engine.snapshot(end).tests.is_empty());
        engine.begin(None, vec![running(AcceptPolicy::Any)], end);
        let reset = engine.snapshot(end);
        assert!(reset.tests[0].in_progress_expects[0].messages.is_empty());
        assert!(
            reset.tests[0].in_progress_expects[0]
                .selected_message()
                .is_none()
        );
    }

    #[test]
    fn tick_expires_without_frames_and_undecoded_frames_are_ignored() {
        let start = time::Instant::now();
        let mut engine = HilEngine::new(path::PathBuf::new());
        engine.begin(None, vec![running(AcceptPolicy::Any)], start);
        let frame = frame::ParsedFrame {
            timestamp: crate::time::Time::now(),
            identity: 1,
            kind: frame::FrameKind::Data,
            dlc: 0,
            raw_bytes: vec![],
            decoded: None,
        };
        engine.process_parsed(&frame, start + time::Duration::from_millis(15));
        let end = start + time::Duration::from_millis(21);
        engine.tick(end);
        let snapshot = engine.snapshot(end);
        assert!(snapshot.capture_complete);
        assert!(
            snapshot.tests[0].in_progress_expects[0].result
                == hil::run::ExpectResult::FailedNoMessage
        );
        assert!(snapshot.tests[0].in_progress_expects[0].messages.is_empty());
    }
}
