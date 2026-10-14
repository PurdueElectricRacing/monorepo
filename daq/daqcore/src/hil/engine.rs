use crate::{can_thread::BusSelection, hil, frame};
use crate::hil::config::ExecutionMode;
use std::{path, time};

use crate::{
    can_thread::tx::{AddSendMessage, SendAmount::Once},
    superdbc::database::Database,
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
    pub capture_complete: bool,
    pub preset: Option<hil::config::PresetInfo>,
    pub tests: Vec<hil::run::HilRunningTest>,
    pub start_error: Option<String>,
    pub superdbc: Option<Database>,
    pub finished: bool,
    pub skipped_from: Option<usize>,
    pub has_decoder: bool,
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
            superdbc: None,
            finished: false,
            skipped_from: None,
            has_decoder: false,
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
    selection: Option<BusSelection>,
}

impl HilEngine {
    pub fn new(base: path::PathBuf) -> Self {
        Self {
            state: HilState::Idle { start_error: None },
            base,
            superdbc: None,
            selection: None,
        }
    }

    pub fn set_decoder(&mut self, superdbc: Database, selection: BusSelection) {
        self.superdbc = Some(superdbc);
        self.selection = Some(selection);
    }

    pub fn set_selection(&mut self, selection: BusSelection) {
        self.selection = Some(selection);
    }

    pub fn has_decoder(&self) -> bool {
        self.superdbc.is_some()
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

    pub fn queue_tx(
        &mut self,
        now: std::time::Instant,
    ) -> Result<(Vec<AddSendMessage>, Vec<String>), String> {
        let superdbc = self
            .superdbc
            .as_ref()
            .ok_or("SuperDBC has not been loaded")?;

        let selection = self
            .selection
            .as_ref()
            .ok_or("Bus has not been selected")?;

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
                return Err("Hil stopped early".to_owned());
            }

            let mut tx_messages: Vec<AddSendMessage> = Vec::new();
            let mut errors = Vec::new();

            let mode = match preset.as_ref() {
                Some(preset_info) => &preset_info.mode,
                None => &ExecutionMode::Parallel,
            };

            match mode {
                ExecutionMode::Parallel => {
                    for test in tests.iter_mut() {
                        if !test.tx_remaining.is_empty() {
                            let pending = std::mem::take(&mut test.tx_remaining);
                            let mut remaining = Vec::new();
                            let elapsed_ms: f64 =
                                now.saturating_duration_since(*start_time).as_secs_f64() * 1000f64;

                            for tx_message in pending {
                                if elapsed_ms < tx_message.timestamp {
                                    remaining.push(tx_message);
                                    continue;
                                }

                                match Self::hil_tx_to_send_message(&tx_message, superdbc, selection) {
                                    Ok(message) => tx_messages.push(message),
                                    Err(error) => {
                                        errors.push(format!(
                                            "Error while encoding and converting TX message {}: {}",
                                            tx_message.msg_name, error
                                        ));
                                    }
                                }
                            }

                            test.tx_remaining = remaining;
                        }
                    }
                }

                ExecutionMode::SequentialAlways | ExecutionMode::SequentialStopFail => {
                    if let Some(test) = tests.get_mut(*active_test) {
                        if !test.tx_remaining.is_empty() {
                            let pending = std::mem::take(&mut test.tx_remaining);
                            let mut remaining = Vec::new();
                            let elapsed_ms: f64 =
                                now.saturating_duration_since(*active_start_time).as_secs_f64() * 1000f64;

                            for tx_message in pending {
                                if elapsed_ms < tx_message.timestamp {
                                    remaining.push(tx_message);
                                    continue;
                                }

                                match Self::hil_tx_to_send_message(&tx_message, superdbc, selection) {
                                    Ok(message) => tx_messages.push(message),
                                    Err(error) => {
                                        errors.push(format!(
                                            "Error while encoding and converting TX message {}: {}",
                                            tx_message.msg_name, error
                                        ));
                                    }
                                }
                            }

                            test.tx_remaining = remaining;
                        }
                    }
                }
            }

            

            Ok((tx_messages, errors))
        } else {
            Err("Hil is not running".to_owned())
        }
    }

    fn hil_tx_to_send_message(
        tx_message: &hil::config::TxMessage,
        superdbc: &Database,
        selection: &BusSelection,
    ) -> Result<AddSendMessage, String> {
        let message = superdbc
            .messages()
            .find(|message| message.name() == tx_message.msg_name)
            .ok_or("Message name not found")?;

        let key = message.key();
        let contains_bus = selection.resolve_buses(superdbc)
            .is_some_and(|buses| buses.iter().any(|bus| bus.bus_id() == key.bus_id));
        if !contains_bus {
            let active_buses = selection
                .resolve_buses(superdbc)
                .map_or(
                    "None".to_string(),
                    |buses| buses.iter()
                        .map(|bus| bus.bus_id().to_string())
                        .collect::<Vec<_>>()
                        .join(", "));
            return Err(format!(
                "Message on CAN bus {} does not exist on CAN bus selection {}",
                key.bus_id, active_buses
            ));
        }

        let values: Vec<(&str, f64)> = tx_message
            .signals
            .iter()
            .map(|(name, value)| (name.as_str(), *value))
            .collect();

        let encoded_message = superdbc
            .encode(key.bus_id, key.identity, &values)
            .map_err(|error| error.to_string())?;

        Ok(AddSendMessage {
            amount: Once,
            key,
            is_extended_id: message.is_extended_id(),
            msg_bytes: encoded_message,
        })
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
                superdbc: self.superdbc.clone(),
                finished: false,
                skipped_from: None,
                has_decoder: self.has_decoder(),
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
                superdbc: self.superdbc.clone(),
                has_decoder: self.has_decoder(),

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
