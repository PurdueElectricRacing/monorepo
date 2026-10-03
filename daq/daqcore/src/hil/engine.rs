#[cfg(test)]
use crate::Time;
#[cfg(test)]
use crate::frame;
use crate::{ParsedFrame, hil};

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
    pub preset: Option<hil::config::PresetInfo>,
    pub tests: Vec<hil::run::HilRunningTest>,
    pub start_error: Option<String>,
}

impl HilSnapshot {
    pub fn idle() -> Self {
        Self {
            status: HilStatus::Idle,
            elapsed_ms: 0,
            preset: None,
            tests: Vec::new(),
            start_error: None,
        }
    }
}

enum HilState {
    Idle {
        start_error: Option<String>,
    },
    Running {
        start_time: std::time::Instant,
        preset: Option<hil::config::PresetInfo>,
        tests: Vec<hil::run::HilRunningTest>,
    },
}

pub struct HilEngine {
    state: HilState,
    base: std::path::PathBuf,
}

impl HilEngine {
    pub fn new(base: std::path::PathBuf) -> Self {
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
            HilState::Running { tests, .. } => tests.iter().all(|t| t.is_finished()),
            HilState::Idle { .. } => false,
        }
    }

    pub fn handle_command(&mut self, command: HilCommand, now: std::time::Instant) {
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

    pub fn process_parsed(&mut self, parsed: &ParsedFrame, now: std::time::Instant) {
        if let HilState::Running {
            start_time, tests, ..
        } = &mut self.state
        {
            for test in tests.iter_mut() {
                if let Some(decoded) = &parsed.decoded {
                    test.process_can(decoded, now.saturating_duration_since(*start_time));
                }
            }
        }
    }

    pub fn tick(&mut self, now: std::time::Instant) {
        if let HilState::Running {
            start_time, tests, ..
        } = &mut self.state
        {
            for test in tests.iter_mut() {
                test.update_expect_statuses(now.saturating_duration_since(*start_time));
            }
        }
    }

    pub fn snapshot(&self, now: std::time::Instant) -> HilSnapshot {
        match &self.state {
            HilState::Idle { start_error } => HilSnapshot {
                status: HilStatus::Idle,
                elapsed_ms: 0,
                preset: None,
                tests: Vec::new(),
                start_error: start_error.clone(),
            },
            HilState::Running {
                start_time,
                preset,
                tests,
            } => HilSnapshot {
                status: HilStatus::Running,
                elapsed_ms: now.saturating_duration_since(*start_time).as_millis(),
                preset: preset.clone(),
                tests: tests.clone(),
                start_error: None,
            },
        }
    }
    fn begin(
        &mut self,
        preset: Option<hil::config::PresetInfo>,
        tests: Vec<hil::run::HilRunningTest>,
        now: std::time::Instant,
    ) {
        self.state = HilState::Running {
            start_time: now,
            preset,
            tests,
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
    #[test]
    fn explicit_resources_and_monotonic_observation_complete_headlessly() {
        let base = std::path::Path::new(env!("CARGO_MANIFEST_DIR")).join("tests/fixtures");
        let parser = can_decode::Parser::from_dbc_file(&base.join("test.dbc")).unwrap();
        let now = std::time::Instant::now();
        let mut engine = HilEngine::new(base.join("hil"));
        engine.handle_command(
            HilCommand::StartTest(hil::config::TestInfo {
                basename: "observation".into(),
                name: "observation".into(),
                description: String::new(),
            }),
            now,
        );
        let frame = ParsedFrame {
            timestamp: Time::from_unix_millis(-1000),
            msg_id: 3,
            is_msg_id_extended: false,
            kind: frame::FrameKind::Data,
            dlc: 2,
            raw_bytes: vec![100, 0],
            decoded: parser.decode_msg(3, &[100, 0]),
        };
        engine.process_parsed(&frame, now + std::time::Duration::from_millis(50));
        let snapshot = engine.snapshot(now + std::time::Duration::from_millis(50));
        assert_eq!(snapshot.elapsed_ms, 50);
        assert!(snapshot.start_error.is_none());
        assert!(matches!(
            snapshot.tests[0].in_progress_expects[0].result,
            hil::run::ExpectResult::Passed
        ));
        assert!(engine.all_finished());
        engine.handle_command(HilCommand::Stop, now + std::time::Duration::from_millis(60));
        assert!(matches!(engine.snapshot(now).status, HilStatus::Idle));
    }
}
