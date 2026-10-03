use crate::{hil, messages};

pub enum HilCommand {
    StartTest(hil::config::TestInfo),
    StartPreset(hil::config::PresetInfo),
    Stop,
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn activation_stops_hil_clears_sends_and_acknowledges_installed_binding() {
        let db = daqcore::superdbc::SuperDbc::from_str(include_str!(
            "../../../daqcore/tests/fixtures/superdbc.json"
        ))
        .unwrap();
        let first = db.bind(db.bus("VCAN").unwrap().bus_id).unwrap();
        let next = db.bind(db.bus("CCAN").unwrap().bus_id).unwrap();
        let (tx, events) = std::sync::mpsc::channel();
        let (_, rx) = std::sync::mpsc::channel();
        let mut state = crate::can::state::State::new(tx, rx, None);
        state.activate_database(first.clone());
        let test_info = hil::config::TestInfo {
            basename: "fixture".into(),
            name: "fixture".into(),
            description: String::new(),
        };
        let test = hil::run::HilRunningTest::from_parts(
            test_info,
            hil::config::TestFile {
                name: "fixture".into(),
                description: String::new(),
                tx: Vec::new(),
                expect: vec![hil::config::Expectation {
                    window: [0.0, 10.0],
                    msg_name: first.msg_defs()[0].name.clone(),
                    signals: Default::default(),
                }],
            },
        );
        state.hil_engine.begin(None, vec![test], Some(&first));
        assert!(state.hil_engine.is_running());
        state.add_send_message(messages::AddSendMessage {
            generation: db.generation(),
            bus: first.bus_id(),
            amount: messages::SendAmount::Infinite { period: 10 },
            msg_id: first.msg_defs()[0].id.to_wire_u32(),
            is_msg_id_extended: first.msg_defs()[0].id.is_extended(),
            msg_bytes: vec![0; 8],
        });
        assert_eq!(state.send_msgs.len(), 1);
        let _ = events.try_iter().collect::<Vec<_>>();
        state.activate_database(next.clone());
        assert!(!state.hil_engine.is_running());
        assert!(state.send_msgs.is_empty());
        assert_eq!(state.active_bus, next.bus_id());
        assert_eq!(state.parser.as_ref().unwrap().bus_id(), next.bus_id());
        assert!(
            matches!(events.try_recv().unwrap(), messages::MsgFromCan::DatabaseActivated { generation, bus }
            if generation == db.generation() && bus == next.bus_id())
        );
        assert!(
            matches!(events.try_recv().unwrap(), messages::MsgFromCan::Hil(snapshot)
            if snapshot.status == HilStatus::Idle)
        );
    }
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

    pub fn all_finished(&self) -> bool {
        match &self.state {
            HilState::Running { tests, .. } => tests.iter().all(|t| t.is_finished()),
            HilState::Idle { .. } => false,
        }
    }

    pub fn handle_command(
        &mut self,
        command: HilCommand,
        database: Option<&daqcore::superdbc::BusDatabase>,
    ) {
        match command {
            HilCommand::StartTest(test_info) => match hil::run::HilRunningTest::new(&test_info) {
                Ok(test) => self.begin(None, vec![test], database),
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
                self.begin(Some(preset), tests, database);
            }
            HilCommand::Stop => {
                self.state = HilState::Idle { start_error: None };
            }
        }
    }

    pub fn process_parsed(
        &mut self,
        parsed: &messages::ParsedMessage,
        database: &daqcore::superdbc::SuperDbc,
    ) {
        if parsed.frame.message(database).is_none() {
            return;
        }
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
        mut tests: Vec<hil::run::HilRunningTest>,
        database: Option<&daqcore::superdbc::BusDatabase>,
    ) {
        let Some(database) = database else {
            self.fail_start("Select a CAN database before starting HIL".into());
            return;
        };
        for test in &mut tests {
            if let Err(e) = test.resolve(database) {
                self.fail_start(e);
                return;
            }
        }
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
