use crate::hil::config::ExecutionMode;
use crate::{frame, hil};
use std::{path, time};
use std::{path::{self, Path, PathBuf}, time};

use crate::{
    can_thread::tx::{AddSendMessage, SendAmount::Once}, frame, hil, superdbc::database::Database
};

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
    pub finished: bool,
    pub skipped_from: Option<usize>,
}

impl HilSnapshot {
    pub fn idle() -> Self {
        Self {
            status: HilStatus::Idle,
            elapsed_ms: 0,
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
    superdbc: Option<Database>,
}

impl HilEngine {
    pub fn new(base: path::PathBuf) -> Self {
        Self {
            state: HilState::Idle { start_error: None },
            base,
            superdbc: None,
        }
    }

    pub fn set_superdbc(&mut self, superdbc: Database) {
        self.superdbc = Some(superdbc);
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

    pub fn queue_tx(&mut self, now: std::time::Instant) -> Result<Vec<AddSendMessage>, String> {
        // Take all tx_messages from running Hil tests
        // and then encode them to be able to send them
        // back to the CAN thread as a CAN message

        // Encoding can be done by loading superdbc
        // then unwrapping the signals to the desired format
        // of the encoder

        // Iterate through all the tx_messages and only
        // send them when the timestamp has been reached

        let superdbc = self.superdbc.as_ref()
            .ok_or("SuperDBC has not been loaded")?;

        if let HilState::Running {
            start_time, preset: _, tests,
            active_test,
            active_start_time,
            stopped_early,
        } = &mut self.state
        {
            let mut tx_messages: Vec<AddSendMessage> = Vec::new();


            for test in tests.iter_mut() {
                if !test.tx_remaining.is_empty() {
                    let pending = std::mem::take(&mut test.tx_remaining);
                    let mut remaining = Vec::new();

                    for tx_message in pending {
                        let elapsed_ms: f64 = now.saturating_duration_since(*start_time).as_secs_f64() * 1000f64;

                        if elapsed_ms < tx_message.timestamp {
                            remaining.push(tx_message);
                            continue;
                        }

                        let message = superdbc
                            .messages()
                            .find(|message| message.name() == tx_message.msg_name)
                            .ok_or("Message name not found")?;


                        let key = message.key();
                        let values: Vec<(&str, f64)> = tx_message
                            .signals
                            .iter()
                            .map(|(name, value)| (name.as_str(), *value))
                            .collect();


                        let encoded_message = superdbc.encode(
                            key.bus_id,
                            key.identity,
                            &values
                        ).map_err(|error| error.to_string())?;
                        
                        tx_messages.push(
                            AddSendMessage {
                                amount: Once,
                                identity: key.identity,
                                msg_bytes: encoded_message
                            }
                        );

                        // Encode the due instruction and add it to tx_messages.
                    }

                    test.tx_remaining = remaining;
                }
            }


            Ok(tx_messages)
        } else {
            Err(format!("Hil is not running"))
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
