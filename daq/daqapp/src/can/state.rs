use crate::{bootloader_protocol, can, connection, hil, messages};

pub struct State {
    pub can_to_ui_tx: std::sync::mpsc::Sender<messages::MsgFromCan>,
    pub ui_to_can_rx: std::sync::mpsc::Receiver<messages::MsgFromUi>,
    pub driver: Option<Box<dyn can::driver::Driver>>,
    pub current_source: Option<connection::ConnectionSource>,
    pub is_connected: bool,
    pub parser: Option<daqcore::superdbc::BusDatabase>,
    pub active_bus: daqcore::can::BusId,
    pub send_msgs: std::collections::HashMap<u32, SendMsgInfo>, // msg_id -> SendMsg
    pub bus_load_tracker: can::bus_load::BusLoadTracker,
    pub last_bus_load_update: std::time::Instant,
    pub hil_engine: hil::engine::HilEngine,
    pub last_hil_update: std::time::Instant,
    pub hil_finished_sent: bool,
    pub firmware_updater: Option<can::bootloader::FirmwareUpdater>,
}

pub struct SendMsgInfo {
    pub amount: messages::SendAmount,
    pub msg_bytes: Vec<u8>,
    pub last_sent: Option<chrono::DateTime<chrono::Local>>,
}

pub struct SendTickInfo {
    pub msg_id: u32,
    pub msg_bytes: Vec<u8>,
}

impl State {
    pub fn activate_database(&mut self, database: daqcore::superdbc::BusDatabase) {
        self.active_bus = database.bus_id();
        self.send_msgs.clear();
        self.hil_engine
            .handle_command(hil::engine::HilCommand::Stop, Some(&database));
        self.hil_finished_sent = false;
        if let Some(driver) = &mut self.driver {
            driver.set_database(Some(database.clone()));
        }
        let generation = database.database().generation();
        self.parser = Some(database);
        let _ = self
            .can_to_ui_tx
            .send(messages::MsgFromCan::DatabaseActivated {
                generation,
                bus: self.active_bus,
            });
        let _ = self
            .can_to_ui_tx
            .send(messages::MsgFromCan::Hil(self.hil_engine.snapshot()));
    }

    pub fn new(
        can_to_ui_tx: std::sync::mpsc::Sender<messages::MsgFromCan>,
        ui_to_can_rx: std::sync::mpsc::Receiver<messages::MsgFromUi>,
        current_source: Option<connection::ConnectionSource>,
    ) -> Self {
        Self {
            can_to_ui_tx,
            ui_to_can_rx,
            driver: None,
            current_source,
            is_connected: false,
            parser: None,
            active_bus: daqcore::can::BusId::new(0).unwrap(),
            send_msgs: std::collections::HashMap::new(),
            bus_load_tracker: can::bus_load::BusLoadTracker::new(),
            last_bus_load_update: std::time::Instant::now(),
            hil_engine: hil::engine::HilEngine::new(),
            last_hil_update: std::time::Instant::now(),
            hil_finished_sent: false,
            firmware_updater: None,
        }
    }

    pub fn start_firmware_update(
        &mut self,
        package: bootloader_protocol::FirmwarePackage,
        armed: bool,
    ) {
        let error = if self.firmware_updater.is_some() {
            Some("another firmware update is already running".to_string())
        } else if armed && package.images.len() != 1 {
            Some("armed bootloader updates require exactly one target".to_string())
        } else if !self.is_connected || self.driver.is_none() {
            Some("CANable is not connected".to_string())
        } else {
            None
        };
        if let Some(error) = error {
            let _ = self
                .can_to_ui_tx
                .send(messages::MsgFromCan::FirmwareProgress(
                    messages::FirmwareProgress {
                        board: "".to_string(),
                        board_index: 0,
                        board_count: package.images.len(),
                        phase: "failed".to_string(),
                        sent_bytes: 0,
                        total_bytes: 0,
                        error: Some(error),
                    },
                ));
            return;
        }
        let (updater, progress) = if armed {
            can::bootloader::FirmwareUpdater::new_armed(package)
        } else {
            can::bootloader::FirmwareUpdater::new(package)
        };
        self.firmware_updater = Some(updater);
        let _ = self
            .can_to_ui_tx
            .send(messages::MsgFromCan::FirmwareProgress(progress));
    }

    pub fn cancel_firmware_update(&mut self) {
        if let Some(mut updater) = self.firmware_updater.take() {
            let progress = updater.cancel();
            let _ = self
                .can_to_ui_tx
                .send(messages::MsgFromCan::FirmwareProgress(progress));
        }
    }

    pub fn firmware_update_active(&self) -> bool {
        self.firmware_updater.is_some()
    }

    pub fn firmware_tick(&mut self) -> Option<can::bootloader::OutboundFrame> {
        let (result, finished) = {
            let updater = self.firmware_updater.as_mut()?;
            let result = updater.tick(std::time::Instant::now());
            let finished = updater.is_finished();
            (result, finished)
        };
        if let Some(progress) = result.progress {
            let _ = self
                .can_to_ui_tx
                .send(messages::MsgFromCan::FirmwareProgress(progress));
        }
        if finished {
            self.firmware_updater = None;
        }
        result.frame
    }

    pub fn firmware_frame_received(&mut self, id: u32, data: &[u8]) {
        let (progress, finished) = {
            let Some(updater) = self.firmware_updater.as_mut() else {
                return;
            };
            let progress = updater.on_response(id, data, std::time::Instant::now());
            let finished = updater.is_finished();
            (progress, finished)
        };
        if let Some(progress) = progress {
            let _ = self
                .can_to_ui_tx
                .send(messages::MsgFromCan::FirmwareProgress(progress));
        }
        if finished {
            self.firmware_updater = None;
        }
    }

    pub fn add_send_message(&mut self, add_msg: messages::AddSendMessage) {
        if !self.parser.as_ref().is_some_and(|db| {
            db.database().generation() == add_msg.generation && db.bus_id() == add_msg.bus
        }) {
            return;
        }
        let Ok(id) = daqcore::can::MessageId::from_wire_u32(add_msg.msg_id) else {
            return;
        };
        if id.is_extended() != add_msg.is_msg_id_extended
            || add_msg.msg_bytes.len() > 8
            || self
                .parser
                .as_ref()
                .and_then(|db| db.bus().message(id))
                .is_none()
        {
            return;
        }
        let msg_id = id.to_wire_u32();
        let send_msg = SendMsgInfo::from_add_send_message(add_msg);
        self.send_msgs.insert(msg_id, send_msg);
    }

    pub fn delete_send_message(&mut self, msg_id: u32) {
        self.send_msgs.remove(&msg_id);
    }

    // Returns a list of messages that should be sent this tick, and updates
    // internal state accordingly (last sent time, amount left, remove messages
    // that are done, etc.)
    pub fn send_this_tick(&mut self) -> Vec<SendTickInfo> {
        let now = chrono::Local::now();
        let mut msgs_to_send = Vec::new();
        let mut msgs_to_remove = Vec::new();

        for (msg_id, send_msg) in self.send_msgs.iter_mut() {
            if send_msg.should_send() {
                msgs_to_send.push(SendTickInfo {
                    msg_id: *msg_id,
                    msg_bytes: send_msg.msg_bytes.clone(),
                });
                send_msg.last_sent = Some(now);
                if let Some(new_amount) = send_msg.amount.subtract_one() {
                    send_msg.amount = new_amount;
                } else {
                    msgs_to_remove.push(*msg_id);
                }
            }
        }

        for msg_id in msgs_to_remove {
            self.send_msgs.remove(&msg_id);
        }

        msgs_to_send
    }
}

impl SendMsgInfo {
    pub fn from_add_send_message(add_msg: messages::AddSendMessage) -> Self {
        Self {
            amount: add_msg.amount,
            msg_bytes: add_msg.msg_bytes,
            last_sent: None,
        }
    }

    pub fn should_send(&self) -> bool {
        match self.last_sent {
            None => true,
            Some(last_sent) => {
                let period = match &self.amount {
                    messages::SendAmount::Infinite { period } => *period,
                    messages::SendAmount::Once => 0,
                    messages::SendAmount::Finite { amount: _, period } => *period,
                };
                chrono::Local::now()
                    .signed_duration_since(last_sent)
                    .num_milliseconds()
                    >= period as i64
            }
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use daqcore::superdbc::*;
    #[test]
    fn sends_reject_stale_generations_and_keep_standard_extended_keys_distinct() {
        let mut v: serde_json::Value = serde_json::from_str(include_str!(
            "../../../daqcore/tests/fixtures/superdbc.json"
        ))
        .unwrap();
        let mut ext = v["buses"]["VCAN"]["messages"][0].clone();
        ext["is_extended_id"] = serde_json::json!(true);
        ext["message_name"] = serde_json::json!("extended_probe");
        v["buses"]["VCAN"]["messages"]
            .as_array_mut()
            .unwrap()
            .push(ext);
        let db = SuperDbc::from_str(&v.to_string()).unwrap();
        let binding = db.bind(db.bus("VCAN").unwrap().bus_id).unwrap();
        let generation = db.generation();
        let (tx, _) = std::sync::mpsc::channel();
        let (_, rx) = std::sync::mpsc::channel();
        let mut state = State::new(tx, rx, None);
        state.active_bus = binding.bus_id();
        state.parser = Some(binding.clone());
        for extended in [false, true] {
            state.add_send_message(messages::AddSendMessage {
                generation,
                bus: binding.bus_id(),
                amount: messages::SendAmount::Once,
                msg_id: if extended { 0x80000001 } else { 1 },
                is_msg_id_extended: extended,
                msg_bytes: vec![0; 5],
            });
        }
        assert_eq!(state.send_msgs.len(), 2);
        assert_eq!(state.send_this_tick().len(), 2);
        let old = SuperDbc::from_str(&v.to_string()).unwrap().generation();
        state.add_send_message(messages::AddSendMessage {
            generation: old,
            bus: binding.bus_id(),
            amount: messages::SendAmount::Once,
            msg_id: 1,
            is_msg_id_extended: false,
            msg_bytes: vec![0; 5],
        });
        assert!(state.send_msgs.is_empty());
    }
}
