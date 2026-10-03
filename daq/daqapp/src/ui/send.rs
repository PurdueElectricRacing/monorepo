use crate::app;

use super::dbc_msg_picker;

pub struct SendUi {
    pub title: String,

    msg_picker: dbc_msg_picker::DbcMsgPickerState,

    selected_msg: Option<can_dbc::Message>,
    signal_values: Vec<SignalValue>,

    sending_messages: Vec<SendingMessage>,

    send_mode: SendMode,
    period_ms: usize,
    finite_amount: usize,
    adjustable_values_enabled: bool,

    error: Option<String>,

    // Required to be stored on the struct so Drop can send cancellation messages when the UI closes
    ui_to_can_tx: std::sync::mpsc::Sender<daqcore::can_thread::CanThreadCommand>,
}

#[derive(Clone, Copy, PartialEq)]
enum SendMode {
    Infinite,
    Once,
    Finite,
}

#[derive(Clone)]
struct SignalValue {
    name: String,
    value: f64,
    min: f64,
    max: f64,
}

struct SendingMessage {
    pub amount: daqcore::can_thread::SendAmount,
    pub msg_name: String,
    pub msg_id: u32,
    pub msg_id_with_ext_flag: u32,
    pub is_msg_id_extended: bool,
    pub msg_bytes: Vec<u8>,
    pub signal_values: Vec<SignalValue>,
    pub adjustable_values_enabled: bool,
    pub last_sent: daqcore::Time,
}

enum SendUiActions {
    DeleteMessage { msg_id: u32 },
}

impl Drop for SendUi {
    fn drop(&mut self) {
        // When the Send UI is closed, we want to stop all sending messages
        log::info!(
            "Dropping SendUi, stopping all sending messages: {:?}",
            self.sending_messages
                .iter()
                .map(|msg| msg.msg_id)
                .collect::<Vec<_>>()
        );
        for msg in &self.sending_messages {
            let msg_id = msg.msg_id;
            if let Err(e) = self
                .ui_to_can_tx
                .send(daqcore::can_thread::CanThreadCommand::DeleteSendMessage { msg_id })
            {
                // Don't panic in Drop, just log the error
                log::error!(
                    "Failed to send DeleteSendMessage for msg_id {}: {}",
                    msg_id,
                    e
                );
            }
        }
    }
}

impl SendUi {
    pub fn new(
        num: usize,
        ui_to_can_tx: std::sync::mpsc::Sender<daqcore::can_thread::CanThreadCommand>,
    ) -> Self {
        Self {
            title: format!("Send UI {}", num),

            msg_picker: dbc_msg_picker::DbcMsgPickerState::default(),

            selected_msg: None,
            signal_values: Vec::new(),

            sending_messages: Vec::new(),

            send_mode: SendMode::Infinite,
            period_ms: 1000,
            finite_amount: 10,
            adjustable_values_enabled: false,

            error: None,

            ui_to_can_tx,
        }
    }

    pub fn show(
        &mut self,
        ui: &mut eframe::egui::Ui,
        parser: Option<&app::ParserInfo>,
        formatter: &Option<daqcore::formatter::Formatter>,
    ) -> egui_tiles::UiResponse {
        let Some(parser) = parser else {
            dbc_msg_picker::no_dbc_placeholder(ui);
            return egui_tiles::UiResponse::None;
        };

        eframe::egui::Frame::group(ui.style())
            .inner_margin(eframe::egui::Margin::symmetric(8, 6))
            .stroke(eframe::egui::Stroke::NONE)
            .show(ui, |ui| {
                eframe::egui::ScrollArea::vertical().show(ui, |ui| {
                    if let Some(msg) =
                        self.msg_picker
                            .show(ui, &parser.parser, self.selected_msg.is_none())
                    {
                        self.selected_msg = Some(msg.clone());
                        self.signal_values = msg
                            .signals
                            .iter()
                            .map(|sig| {
                                let (min, max) = signal_range(sig);
                                SignalValue {
                                    name: sig.name.clone(),
                                    value: 0.0,
                                    min,
                                    max,
                                }
                            })
                            .collect();
                        self.error = None;
                    }

                    if let Some(selected_msg) = &self.selected_msg {
                        ui.separator();

                        if let Some(error) = &self.error {
                            ui.label(
                                eframe::egui::RichText::new(error)
                                    .color(ui.visuals().error_fg_color),
                            );
                        }

                        ui.label(
                            eframe::egui::RichText::new(format!(
                                "Selected Message: {} (0x{:03X})",
                                selected_msg.name,
                                daqcore::can::can_dbc_to_u32_without_extid_flag(&selected_msg.id)
                            ))
                            .strong()
                            .size(16.0),
                        );

                        // Send Amount selector
                        ui.label(eframe::egui::RichText::new("Send Options").strong());

                        ui.horizontal(|ui| {
                            ui.selectable_value(&mut self.send_mode, SendMode::Once, "Once");
                            ui.selectable_value(
                                &mut self.send_mode,
                                SendMode::Infinite,
                                "Infinite",
                            );
                            ui.selectable_value(&mut self.send_mode, SendMode::Finite, "Finite");
                        });

                        match self.send_mode {
                            SendMode::Once => {}
                            SendMode::Infinite => {
                                ui.horizontal(|ui| {
                                    ui.label("Period (ms)");
                                    ui.add(
                                        eframe::egui::DragValue::new(&mut self.period_ms)
                                            .speed(1)
                                            .range(1..=10_000),
                                    );
                                });
                            }
                            SendMode::Finite => {
                                ui.horizontal(|ui| {
                                    ui.label("Amount");
                                    ui.add(
                                        eframe::egui::DragValue::new(&mut self.finite_amount)
                                            .speed(1)
                                            .range(1..=10_000),
                                    );
                                });
                                ui.horizontal(|ui| {
                                    ui.label("Period (ms)");
                                    ui.add(
                                        eframe::egui::DragValue::new(&mut self.period_ms)
                                            .speed(1)
                                            .range(1..=10_000),
                                    );
                                });
                            }
                        }
                        ui.checkbox(&mut self.adjustable_values_enabled, "Adjustable values");
                        for i in 0..self.signal_values.len() {
                            ui.horizontal(|ui| {
                                let signal = &mut self.signal_values[i];
                                ui.label(signal.name.as_str());
                                let expected_decimals = formatter
                                    .as_ref()
                                    .map(|f| f.expected_decimals(&selected_msg.name, &signal.name))
                                    .unwrap_or(2);
                                let speed = 10f64.powi(-(expected_decimals as i32));
                                if ui
                                    .add(
                                        eframe::egui::DragValue::new(&mut signal.value)
                                            .range(signal.min..=signal.max)
                                            .speed(speed),
                                    )
                                    .changed()
                                {
                                    self.signal_values[i].value = signal.value;
                                }
                            });
                        }

                        if ui.button("Send Message").clicked() {
                            let msg_id_with_ext_flag =
                                daqcore::can::can_dbc_to_u32_with_extid_flag(&selected_msg.id);
                            let encoded = encode_msg_from_signals(
                                &parser.parser,
                                msg_id_with_ext_flag,
                                &self.signal_values,
                            );

                            let Some(msg_bytes) = encoded else {
                                self.error = Some(
                                    "Failed to encode message. Check signal values.".to_string(),
                                );

                                return;
                            };

                            self.error = None;

                            let send_amount = match self.send_mode {
                                SendMode::Once => daqcore::can_thread::SendAmount::Once,

                                SendMode::Infinite => daqcore::can_thread::SendAmount::Infinite {
                                    period: self.period_ms,
                                },

                                SendMode::Finite => daqcore::can_thread::SendAmount::Finite {
                                    amount: self.finite_amount,
                                    period: self.period_ms,
                                },
                            };

                            let msg_id_u32 =
                                daqcore::can::can_dbc_to_u32_without_extid_flag(&selected_msg.id);

                            self.sending_messages.push(SendingMessage {
                                amount: send_amount,
                                msg_name: selected_msg.name.clone(),
                                msg_id: msg_id_u32,
                                msg_id_with_ext_flag,
                                is_msg_id_extended: matches!(
                                    selected_msg.id,
                                    can_dbc::MessageId::Extended(_)
                                ),
                                msg_bytes: msg_bytes.clone(),
                                signal_values: self.signal_values.clone(),
                                adjustable_values_enabled: self.adjustable_values_enabled,
                                last_sent: daqcore::Time::now(),
                            });

                            let add_send_msg = daqcore::can_thread::AddSendMessage {
                                amount: send_amount,
                                msg_id: msg_id_u32,
                                is_msg_id_extended: matches!(
                                    selected_msg.id,
                                    can_dbc::MessageId::Extended(_)
                                ),
                                msg_bytes,
                            };

                            self.selected_msg = None;
                            self.signal_values.clear();
                            self.adjustable_values_enabled = false;

                            self.ui_to_can_tx
                                .send(daqcore::can_thread::CanThreadCommand::AddSendMessage(
                                    add_send_msg,
                                ))
                                .unwrap_or_else(|error| {
                                    log::error!("Failed to submit send: {error}")
                                });
                        }
                    }

                    ui.separator();

                    let mut all_actions = Vec::new();
                    let mut updates_to_send = Vec::new();
                    for idx in (0..self.sending_messages.len()).rev() {
                        if let Some(action) =
                            self.sending_messages[idx].ui(ui, formatter, idx, &mut updates_to_send)
                        {
                            all_actions.push(action);
                        }
                        ui.add_space(8.0);
                    }

                    updates_to_send.sort_unstable();
                    updates_to_send.dedup();
                    for idx in updates_to_send {
                        if idx >= self.sending_messages.len() {
                            continue;
                        }

                        let msg = &self.sending_messages[idx];
                        if !msg.adjustable_values_enabled {
                            continue;
                        }
                        let encoded = encode_msg_from_signals(
                            &parser.parser,
                            msg.msg_id_with_ext_flag,
                            &msg.signal_values,
                        );
                        let Some(msg_bytes) = encoded else {
                            self.error = Some(format!(
                                "Failed to encode {} while applying slider update.",
                                msg.msg_name
                            ));
                            continue;
                        };

                        self.sending_messages[idx].msg_bytes = msg_bytes.clone();
                        self.error = None;

                        self.ui_to_can_tx
                            .send(daqcore::can_thread::CanThreadCommand::AddSendMessage(
                                daqcore::can_thread::AddSendMessage {
                                    amount: self.sending_messages[idx].amount,
                                    msg_id: self.sending_messages[idx].msg_id,
                                    is_msg_id_extended: self.sending_messages[idx]
                                        .is_msg_id_extended,
                                    msg_bytes,
                                },
                            ))
                            .unwrap_or_else(|error| log::error!("Failed to submit send: {error}"));
                    }

                    for action in all_actions {
                        match action {
                            SendUiActions::DeleteMessage { msg_id } => {
                                self.sending_messages.retain(|msg| msg.msg_id != msg_id);
                                self.ui_to_can_tx
                                    .send(
                                        daqcore::can_thread::CanThreadCommand::DeleteSendMessage {
                                            msg_id,
                                        },
                                    )
                                    .unwrap_or_else(|error| {
                                        log::error!("Failed to delete send: {error}")
                                    });
                            }
                        }
                    }
                });
            });

        egui_tiles::UiResponse::None
    }

    pub fn handle_can_message(&mut self, msg: &daqcore::can_thread::CanThreadEvent) {
        if let daqcore::can_thread::CanThreadEvent::SendFailed {
            msg_id,
            error,
            retrying,
        } = msg
        {
            if !retrying {
                self.sending_messages.retain(|m| m.msg_id != *msg_id);
            }
            log::error!("Send {msg_id:X} failed: {error}");
        }
        if let daqcore::can_thread::CanThreadEvent::MessageSent {
            msg_id,
            timestamp,
            amount_left,
        } = msg
        {
            if let Some(rx_amount_left) = amount_left {
                for sending_msg in &mut self.sending_messages {
                    if sending_msg.msg_id == *msg_id {
                        sending_msg.last_sent = *timestamp;
                        sending_msg.amount = *rx_amount_left;
                        break;
                    }
                }
            } else {
                // If amount_left is None, it means the message is done sending,
                // so we remove it from the list
                self.sending_messages.retain(|msg| msg.msg_id != *msg_id);
            }
        }
    }
}

impl SendingMessage {
    fn ui(
        &mut self,
        ui: &mut eframe::egui::Ui,
        formatter: &Option<daqcore::formatter::Formatter>,
        msg_idx: usize,
        updates_to_send: &mut Vec<usize>,
    ) -> Option<SendUiActions> {
        let mut delete_action = None;
        let raw_bytes_str = self
            .msg_bytes
            .iter()
            .map(|b| format!("{:02X}", b))
            .collect::<Vec<_>>()
            .join(" ");

        // Header (outside card)
        ui.horizontal(|ui| {
            ui.label(
                eframe::egui::RichText::new(format!("{}  (0x{:03X})", self.msg_name, self.msg_id))
                    .strong()
                    .size(16.0)
                    .color(ui.visuals().text_color()),
            );
            ui.label(
                eframe::egui::RichText::new(self.amount.display()).color(ui.visuals().text_color()),
            );
            ui.label(
                eframe::egui::RichText::new(format!(
                    "~{} ms ago",
                    (daqcore::Time::now().secs(self.last_sent) * 1000.0) as i64
                ))
                .italics()
                .color(ui.visuals().weak_text_color()),
            );
            ui.with_layout(
                eframe::egui::Layout::right_to_left(eframe::egui::Align::Center),
                |ui| {
                    if ui.button("🗑").on_hover_text("Delete message").clicked() {
                        delete_action = Some(SendUiActions::DeleteMessage {
                            msg_id: self.msg_id,
                        });
                    }
                    ui.label(
                        eframe::egui::RichText::new(raw_bytes_str)
                            .monospace()
                            .color(ui.visuals().text_color()),
                    );

                    ui.add_space(2.0);
                },
            );
        });

        ui.add_space(4.0);

        // Card container
        eframe::egui::Frame::group(ui.style())
            .fill(ui.visuals().faint_bg_color)
            .corner_radius(eframe::egui::CornerRadius::same(8))
            .inner_margin(eframe::egui::Margin::symmetric(8, 6))
            .show(ui, |ui| {
                ui.vertical(|ui| {
                    let total_signals = self.signal_values.len();
                    for (i, signal) in self.signal_values.iter_mut().enumerate() {
                        ui.horizontal(|ui| {
                            ui.label(
                                eframe::egui::RichText::new(&signal.name)
                                    .monospace()
                                    .color(ui.visuals().text_color()),
                            );
                            ui.with_layout(
                                eframe::egui::Layout::right_to_left(eframe::egui::Align::Center),
                                |ui| {
                                    let expected_decimals = formatter
                                        .as_ref()
                                        .map(|f| f.expected_decimals(&self.msg_name, &signal.name))
                                        .unwrap_or(2);

                                    if self.adjustable_values_enabled {
                                        let speed = 10f64.powi(-(expected_decimals as i32));
                                        if ui
                                            .add(
                                                eframe::egui::DragValue::new(&mut signal.value)
                                                    .range(signal.min..=signal.max)
                                                    .speed(speed),
                                            )
                                            .changed()
                                        {
                                            updates_to_send.push(msg_idx);
                                        }
                                    } else {
                                        ui.label(
                                            eframe::egui::RichText::new(format!(
                                                "{:.*}",
                                                expected_decimals, signal.value
                                            ))
                                            .monospace(),
                                        );
                                    }
                                },
                            );
                        });
                        if i < total_signals - 1 {
                            ui.separator();
                        }
                    }
                });
            });

        delete_action
    }
}

fn encode_msg_from_signals(
    parser: &can_decode::Parser,
    msg_id_with_ext_flag: u32,
    signals: &[SignalValue],
) -> Option<Vec<u8>> {
    let values_hashmap = signals
        .iter()
        .map(|signal| (signal.name.clone(), signal.value))
        .collect();
    parser.encode_msg(msg_id_with_ext_flag, &values_hashmap)
}

fn signal_range(sig: &can_dbc::Signal) -> (f64, f64) {
    let fallback = (-1000.0, 1000.0);

    let min = daqcore::can::can_dbc_numeric_to_f64(&sig.min);
    let max = daqcore::can::can_dbc_numeric_to_f64(&sig.max);

    if !min.is_finite() || !max.is_finite() || min >= max {
        fallback
    } else {
        (min, max)
    }
}
