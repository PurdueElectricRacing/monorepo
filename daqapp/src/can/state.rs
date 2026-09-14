use crate::{can, connection, messages};

pub struct State {
    pub can_to_ui_tx: std::sync::mpsc::Sender<messages::MsgFromCan>,
    pub ui_to_can_rx: std::sync::mpsc::Receiver<messages::MsgFromUi>,
    pub driver: Option<Box<dyn can::driver::Driver>>,
    pub current_source: Option<connection::ConnectionSource>,
    pub is_connected: bool,
    pub parsers: Vec<Option<can_decode::Parser>>, // default, VCAN, MCAN, SCAN
    pub send_msgs: std::collections::HashMap<u32, SendMsgInfo>, // msg_id -> SendMsg
    pub bus_load_tracker: can::bus_load::BusLoadTracker,
    pub last_bus_load_update: std::time::Instant,
}

pub struct SendMsgInfo {
    pub amount: messages::SendAmount,
    pub is_msg_id_extended: bool,
    pub msg_bytes: Vec<u8>,
    pub last_sent: Option<chrono::DateTime<chrono::Local>>,
}

pub struct SendTickInfo {
    pub msg_id: u32,
    pub msg_bytes: Vec<u8>,
    pub is_msg_id_extended: bool,
}

impl State {
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
            parsers: vec![None, None, None, None],
            send_msgs: std::collections::HashMap::new(),
            bus_load_tracker: can::bus_load::BusLoadTracker::new(),
            last_bus_load_update: std::time::Instant::now(),
        }
    }

    pub fn parser_for_bus(&self, bus_name: messages::BusName) -> Option<&can_decode::Parser> {
        self.parsers
            .get(bus_name as usize)
            .and_then(|parser| parser.as_ref())
    }

    pub fn add_send_message(&mut self, add_msg: messages::AddSendMessage) {
        let msg_id = add_msg.msg_id;
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
                    is_msg_id_extended: send_msg.is_msg_id_extended,
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
            is_msg_id_extended: add_msg.is_msg_id_extended,
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
