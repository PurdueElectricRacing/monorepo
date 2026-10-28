use crate::{app, ui};
use std::sync::mpsc;

pub struct SendUi {
    pub title: String,

    msg_picker: ui::message_picker::MessagePickerState,

    selected_msg: Option<daqcore::superdbc::message::MessageKey>,
    signal_values: Vec<SignalValue>,

    sending_messages: Vec<SendingMessage>,

    send_mode: SendMode,
    period_ms: usize,
    finite_amount: usize,
    adjustable_values_enabled: bool,

    error: Option<String>,

    // Required to be stored on the struct so Drop can send cancellation messages when the UI closes
    ui_to_can_tx: mpsc::Sender<daqcore::can_thread::CanThreadCommand>,
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
    pub amount: daqcore::can_thread::tx::SendAmount,
    pub msg_name: String,
    pub identity: daqcore::frame::CanIdentity,
    pub msg_bytes: Vec<u8>,
    pub signal_values: Vec<SignalValue>,
    pub adjustable_values_enabled: bool,
    pub last_sent: daqcore::time::Time,
}

enum SendUiActions {
    DeleteMessage {
        identity: daqcore::frame::CanIdentity,
    },
}

impl Drop for SendUi {
    fn drop(&mut self) {
        // When the Send UI is closed, we want to stop all sending messages
        log::info!(
            "Dropping SendUi, stopping all sending messages: {:?}",
            self.sending_messages
                .iter()
                .map(|msg| msg.identity)
                .collect::<Vec<_>>()
        );
        for msg in &self.sending_messages {
            let identity = msg.identity;
            if let Err(e) = self
                .ui_to_can_tx
                .send(daqcore::can_thread::CanThreadCommand::DeleteSendMessage { identity })
            {
                // Don't panic in Drop, just log the error
                log::error!(
                    "Failed to send DeleteSendMessage for identity {}: {}",
                    identity,
                    e
                );
            }
        }
    }
}

impl SendUi {
    pub fn new(
        num: usize,
        ui_to_can_tx: mpsc::Sender<daqcore::can_thread::CanThreadCommand>,
    ) -> Self {
        Self {
            title: format!("Send UI {}", num),

            msg_picker: ui::message_picker::MessagePickerState::default(),

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
        database: Option<&app::DatabaseSelection>,
        formatter: &Option<daqcore::formatter::Formatter>,
    ) -> egui_tiles::UiResponse {
        let Some(database) = database else {
            ui::message_picker::no_database_placeholder(ui);
            return egui_tiles::UiResponse::None;
        };
        let Some(bus) = database.bus() else {
            return egui_tiles::UiResponse::None;
        };

        eframe::egui::Frame::group(ui.style())
            .inner_margin(eframe::egui::Margin::symmetric(8, 6))
            .stroke(eframe::egui::Stroke::NONE)
            .show(ui, |ui| {
                eframe::egui::ScrollArea::vertical().show(ui, |ui| {
                    if let Some(picked) = self.msg_picker.show(ui, bus, self.selected_msg.is_none())
                        && let Some(msg) = bus.message(picked.identity)
                    {
                        self.selected_msg = Some(picked);
                        self.period_ms = msg.nominal_period_ms().map_or(1000, |ms| ms as usize);
                        self.signal_values = msg
                            .signals()
                            .iter()
                            .map(|sig| {
                                let (min, max) = signal_range(sig);
                                SignalValue {
                                    name: sig.name().to_owned(),
                                    value: 0.0_f64.clamp(min, max),
                                    min,
                                    max,
                                }
                            })
                            .collect();
                        self.error = None;
                    }

                    if let Some(selected_msg) =
                        self.selected_msg.and_then(|key| bus.message(key.identity))
                    {
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
                                selected_msg.name(),
                                selected_msg.identity().raw_id()
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
                        for signal in &mut self.signal_values {
                            ui.horizontal(|ui| {
                                ui.label(signal.name.as_str());
                                let expected_decimals = formatter
                                    .as_ref()
                                    .map(|f| f.expected_decimals(selected_msg.name(), &signal.name))
                                    .unwrap_or(2);
                                let speed = 10f64.powi(-(expected_decimals as i32));
                                ui.add(
                                    eframe::egui::DragValue::new(&mut signal.value)
                                        .range(signal.min..=signal.max)
                                        .speed(speed),
                                );
                            });
                        }

                        if ui.button("Send Message").clicked() {
                            let msg_bytes = match encode_msg_from_signals(
                                bus,
                                selected_msg.identity(),
                                &self.signal_values,
                            ) {
                                Ok(bytes) => bytes,
                                Err(error) => {
                                    self.error = Some(error.to_string());
                                    return;
                                }
                            };

                            self.error = None;

                            let send_amount = match self.send_mode {
                                SendMode::Once => daqcore::can_thread::tx::SendAmount::Once,

                                SendMode::Infinite => {
                                    daqcore::can_thread::tx::SendAmount::Infinite {
                                        period: self.period_ms,
                                    }
                                }

                                SendMode::Finite => daqcore::can_thread::tx::SendAmount::Finite {
                                    amount: self.finite_amount,
                                    period: self.period_ms,
                                },
                            };

                            let identity = selected_msg.identity();
                            self.sending_messages.retain(|m| m.identity != identity);

                            self.sending_messages.push(SendingMessage {
                                amount: send_amount,
                                msg_name: selected_msg.name().to_owned(),
                                identity,
                                msg_bytes: msg_bytes.clone(),
                                signal_values: self.signal_values.clone(),
                                adjustable_values_enabled: self.adjustable_values_enabled,
                                last_sent: daqcore::time::Time::now(),
                            });

                            let add_send_msg = daqcore::can_thread::tx::AddSendMessage {
                                amount: send_amount,
                                identity,
                                msg_bytes,
                                is_hil: false,
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

                        let encoded =
                            encode_msg_from_signals(bus, msg.identity, &msg.signal_values);
                        let msg_bytes = match encoded {
                            Ok(bytes) => bytes,
                            Err(error) => {
                                self.error = Some(format!("{}: {error}", msg.msg_name));
                                continue;
                            }
                        };

                        self.sending_messages[idx].msg_bytes = msg_bytes.clone();
                        self.error = None;

                        self.ui_to_can_tx
                            .send(daqcore::can_thread::CanThreadCommand::AddSendMessage(
                                daqcore::can_thread::tx::AddSendMessage {
                                    amount: self.sending_messages[idx].amount,
                                    identity: self.sending_messages[idx].identity,
                                    msg_bytes,
                                    is_hil: false,
                                },
                            ))
                            .unwrap_or_else(|error| log::error!("Failed to submit send: {error}"));
                    }

                    for action in all_actions {
                        match action {
                            SendUiActions::DeleteMessage { identity } => {
                                self.sending_messages.retain(|msg| msg.identity != identity);
                                self.ui_to_can_tx
                                    .send(
                                        daqcore::can_thread::CanThreadCommand::DeleteSendMessage {
                                            identity,
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

    pub fn reset_database(&mut self) {
        self.msg_picker = ui::message_picker::MessagePickerState::default();
        self.selected_msg = None;
        self.signal_values.clear();
        self.sending_messages.clear();
        self.adjustable_values_enabled = false;
        self.error = None;
    }

    pub fn handle_can_message(&mut self, msg: &daqcore::can_thread::CanThreadEvent) {
        if let daqcore::can_thread::CanThreadEvent::SendFailed {
            identity,
            error,
            retrying,
        } = msg
        {
            if !retrying {
                self.sending_messages.retain(|m| m.identity != *identity);
            }
            log::error!("Send {identity} failed: {error}");
        }

        if let daqcore::can_thread::CanThreadEvent::MessageSent {
            identity,
            timestamp,
            amount_left,
        } = msg
        {
            if let Some(rx_amount_left) = amount_left {
                for sending_msg in &mut self.sending_messages {
                    if sending_msg.identity == *identity {
                        sending_msg.last_sent = *timestamp;
                        sending_msg.amount = *rx_amount_left;
                        break;
                    }
                }
            } else {
                // If amount_left is None, it means the message is done sending,
                // so we remove it from the list
                self.sending_messages
                    .retain(|msg| msg.identity != *identity);
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
                eframe::egui::RichText::new(format!("{}  ({})", self.msg_name, self.identity))
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
                    (daqcore::time::Time::now().secs(self.last_sent) * 1000.0) as i64
                ))
                .italics()
                .color(ui.visuals().weak_text_color()),
            );
            ui.with_layout(
                eframe::egui::Layout::right_to_left(eframe::egui::Align::Center),
                |ui| {
                    if ui.button("🗑").on_hover_text("Delete message").clicked() {
                        delete_action = Some(SendUiActions::DeleteMessage {
                            identity: self.identity,
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
    bus: &daqcore::superdbc::database::Bus,
    identity: daqcore::frame::CanIdentity,
    signals: &[SignalValue],
) -> Result<Vec<u8>, daqcore::superdbc::error::EncodeError> {
    let values = signals
        .iter()
        .map(|signal| (signal.name.as_str(), signal.value))
        .collect::<Vec<_>>();
    bus.encode(identity, &values)
}

fn signal_range(signal: &daqcore::superdbc::message::SignalDefinition) -> (f64, f64) {
    signal
        .limits()
        .filter(|limits| limits.min < limits.max)
        .map_or((-1000.0, 1000.0), |limits| (limits.min, limits.max))
}
