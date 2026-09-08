use crate::{hil, messages};

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
}

impl Default for HilEngine {
    fn default() -> Self {
        Self::new()
    }
}

impl HilEngine {
    pub fn new() -> Self {
        Self {
            state: HilState::Idle { start_error: None },
        }
    }

    pub fn is_running(&self) -> bool {
        matches!(self.state, HilState::Running { .. })
    }

    pub fn handle_command(&mut self, command: HilCommand) {
        match command {
            HilCommand::StartTest(test_info) => match hil::run::HilRunningTest::new(&test_info) {
                Ok(test) => self.begin(None, vec![test]),
                Err(err) => self.fail_start(format!("Failed to start test: {err}")),
            },

            HilCommand::StartPreset(preset) => {
                let mut tests = Vec::with_capacity(preset.tests.len());
                for basename in &preset.tests {
                    match hil::run::HilRunningTest::from_basename(basename) {
                        Ok(test) => tests.push(test),
                        Err(err) => {
                            self.fail_start(format!("Failed to start test: {err}"));
                            return;
                        }
                    }
                }
                self.begin(Some(preset), tests);
            }
            HilCommand::Stop => {
                self.state = HilState::Idle { start_error: None };
            }
        }
    }

    pub fn process_parsed(&mut self, parsed: &messages::ParsedMessage) {
        if let HilState::Running {
            start_time, tests, ..
        } = &mut self.state
        {
            for test in tests.iter_mut() {
                test.process_can(parsed, *start_time);
            }
        }
    }

    pub fn tick(&mut self) {
        if let HilState::Running {
            start_time, tests, ..
        } = &mut self.state
        {
            for test in tests.iter_mut() {
                test.update_expect_statuses(*start_time);
            }
        }
    }

    pub fn snapshot(&self) -> HilSnapshot {
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
                elapsed_ms: start_time.elapsed().as_millis(),
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
    ) {
        self.state = HilState::Running {
            start_time: std::time::Instant::now(),
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
