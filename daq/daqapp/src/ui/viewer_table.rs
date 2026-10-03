use crate::{action, app, formatter, frozen, messages, widget_constructor};
use eframe::egui;

type DecodedMsgMap = hashbrown::HashMap<(daqcore::can::BusId, u32), messages::ParsedMessage>;
type UndecodedMsgMap =
    hashbrown::HashMap<(Option<daqcore::can::BusId>, u32), messages::UnparsedMessage>;

#[derive(Clone, PartialEq, Eq)]
enum TxNodeSearch {
    Any,
    Unparsed,
    Node(String),
}

pub struct ViewerTable {
    pub title: String,
    decoded_msgs: frozen::Frozen<DecodedMsgMap>,
    undecoded_msgs: frozen::Frozen<UndecodedMsgMap>,
    paused: bool,
    search: String,
    tx_node: TxNodeSearch,
}

impl TxNodeSearch {
    fn matches(&self, tx_node: &str) -> bool {
        match self {
            TxNodeSearch::Any => true,
            TxNodeSearch::Unparsed => tx_node.eq_ignore_ascii_case("Unparsed"),
            TxNodeSearch::Node(node) => tx_node.eq_ignore_ascii_case(node),
        }
    }
}

impl ViewerTable {
    pub fn new(instance_num: usize) -> Self {
        Self {
            title: format!("CAN Viewer Table #{}", instance_num),
            decoded_msgs: frozen::Frozen::new(DecodedMsgMap::new()),
            undecoded_msgs: frozen::Frozen::new(UndecodedMsgMap::new()),
            paused: false,
            search: String::new(),
            tx_node: TxNodeSearch::Any,
        }
    }

    pub fn show(
        &mut self,
        ui: &mut egui::Ui,
        action_queue: &mut Vec<action::AppAction>,
        formatter: &Option<formatter::Formatter>,
        parser: Option<&app::ParserInfo>,
    ) -> egui_tiles::UiResponse {
        let database = parser.map(|p| p.parser.database());
        ui.heading(format!("🚗 {}", self.title));

        ui.horizontal(|ui| {
            if ui
                .button(if self.paused { "Resume" } else { "Pause" })
                .clicked()
            {
                self.paused = !self.paused;
                if self.paused {
                    self.decoded_msgs.freeze();
                    self.undecoded_msgs.freeze();
                } else {
                    self.decoded_msgs.unfreeze();
                    self.undecoded_msgs.unfreeze();
                }
            }

            if ui.button("Clear").clicked() {
                self.decoded_msgs.apply_both(|ms| ms.clear());
                self.undecoded_msgs.apply_both(|ms| ms.clear());
            }
        });

        self.clean_undecoded();

        ui.separator();

        ui.add_space(4.0);

        egui::Frame::group(ui.style())
            .inner_margin(egui::Margin::symmetric(8, 6))
            .stroke(egui::Stroke::NONE)
            .show(ui, |ui| {
                ui.horizontal(|ui| {
                    ui.label("Search:");
                    let search_bg = if ui.visuals().text_edit_bg_color.is_some() {
                        ui.visuals().widgets.inactive.bg_fill
                    } else {
                        ui.visuals().extreme_bg_color
                    };
                    ui.add(
                        egui::TextEdit::singleline(&mut self.search).background_color(search_bg),
                    );

                    ui.add_space(8.0);

                    let mut all_tx_nodes = self
                        .decoded_msgs
                        .get()
                        .values()
                        .filter_map(|msg| msg.decoded(database?).map(|v| v.tx_node.to_string()))
                        .collect::<Vec<_>>();
                    all_tx_nodes.sort_unstable();
                    all_tx_nodes.dedup();
                    ui.label("Tx Node:");
                    egui::ComboBox::from_id_salt(("tx_node_filter", &self.title))
                        .selected_text(match &self.tx_node {
                            TxNodeSearch::Any => "Any".to_string(),
                            TxNodeSearch::Unparsed => "Unparsed".to_string(),
                            TxNodeSearch::Node(node) => node.clone(),
                        })
                        .show_ui(ui, |ui| {
                            ui.selectable_value(&mut self.tx_node, TxNodeSearch::Any, "Any");
                            for tx_node in all_tx_nodes {
                                ui.selectable_value(
                                    &mut self.tx_node,
                                    TxNodeSearch::Node(tx_node.clone()),
                                    tx_node,
                                );
                            }
                            if !self.undecoded_msgs.get().is_empty() {
                                ui.selectable_value(
                                    &mut self.tx_node,
                                    TxNodeSearch::Unparsed,
                                    "Unparsed",
                                );
                            }
                        });
                });
                ui.add_space(8.0);

                let decoded = self.decoded_msgs.get();
                let undecoded = self.undecoded_msgs.get();

                if decoded.is_empty() && undecoded.is_empty() {
                    ui.centered_and_justified(|ui| {
                        ui.label(
                            egui::RichText::new("No CAN messages to display.")
                                .italics()
                                .weak(),
                        );
                    });
                    return;
                }

                egui::ScrollArea::vertical().show(ui, |ui| {
                    let low_search = self.search.to_lowercase();

                    if !undecoded.is_empty() {
                        let mut undecoded_msg_keys = undecoded
                            .iter()
                            .filter_map(|(&msg_id, msg)| {
                                let tx_filter = matches!(
                                    self.tx_node,
                                    TxNodeSearch::Any | TxNodeSearch::Unparsed
                                );
                                if !tx_filter {
                                    return None;
                                }

                                if self.search.is_empty()
                                    || format!("{:03X}", msg.frame.id.raw())
                                        .to_lowercase()
                                        .contains(&low_search)
                                    || "error: unknown".contains(&low_search)
                                    || "unparsed".contains(&low_search)
                                {
                                    Some(msg_id)
                                } else {
                                    None
                                }
                            })
                            .collect::<Vec<_>>();
                        undecoded_msg_keys.sort();
                        for msg_id in undecoded_msg_keys {
                            let msg = &undecoded[&msg_id];
                            let raw_bytes_str = msg
                                .frame
                                .data()
                                .iter()
                                .map(|b| format!("{:02X}", b))
                                .collect::<Vec<_>>()
                                .join(" ");
                            MessageCard {
                                bus: msg
                                    .frame
                                    .bus
                                    .unwrap_or(daqcore::can::BusId::new(0).unwrap()),
                                msg_name: "Error: Unknown",
                                msg_id: msg.frame.id.to_wire_u32(),
                                tx_node: "Unparsed",
                                raw_bytes: &raw_bytes_str,
                                timestamp: &msg.timestamp.format("%-I:%M:%S%.3f").to_string(),
                                signals: Vec::new(),
                                search: &self.search,
                            }
                            .ui(ui)
                            .into_iter()
                            .for_each(|spawn| action_queue.push(spawn));
                        }
                        ui.add_space(8.0);
                    }

                    let mut decoded_msg_keys = decoded
                        .iter()
                        .filter_map(|(&msg_id, msg)| {
                            let view = msg.decoded(database?)?;
                            let tx_filter = self.tx_node.matches(&view.tx_node);
                            if !tx_filter {
                                return None;
                            }

                            if self.search.is_empty()
                                || view.name.to_lowercase().contains(&low_search)
                                || format!("{:03X}", msg.frame.id.raw())
                                    .to_lowercase()
                                    .contains(&low_search)
                                || view.tx_node.to_lowercase().contains(&low_search)
                                || view
                                    .signals
                                    .values()
                                    .any(|sig| sig.name.to_lowercase().contains(&low_search))
                            {
                                Some(msg_id)
                            } else {
                                None
                            }
                        })
                        .collect::<Vec<_>>();
                    decoded_msg_keys.sort();
                    for msg_id in decoded_msg_keys {
                        let msg = &decoded[&msg_id];
                        let Some(view) = database.and_then(|db| msg.decoded(db)) else {
                            continue;
                        };
                        let signals: Vec<(&str, String)> = view
                            .signals
                            .iter()
                            .map(|(name, signal)| {
                                (
                                    name,
                                    formatter::try_format(
                                        formatter,
                                        view.name,
                                        name,
                                        Some(signal.definition),
                                        Some(signal.unit),
                                        &signal.value,
                                    ),
                                )
                            })
                            .collect();
                        let raw_bytes_str = msg
                            .frame
                            .data()
                            .iter()
                            .map(|b| format!("{:02X}", b))
                            .collect::<Vec<_>>()
                            .join(" ");
                        MessageCard {
                            bus: msg.frame.bus,
                            msg_name: view.name,
                            msg_id: view.msg_id,
                            tx_node: &view.tx_node,
                            raw_bytes: &raw_bytes_str,
                            timestamp: &msg.timestamp.format("%-I:%M:%S%.3f").to_string(),
                            signals,
                            search: &self.search,
                        }
                        .ui(ui)
                        .into_iter()
                        .for_each(|spawn| action_queue.push(spawn));
                        ui.add_space(8.0);
                    }
                });
            });

        egui_tiles::UiResponse::None
    }

    pub fn handle_can_message(&mut self, msg: &messages::MsgFromCan) {
        match msg {
            messages::MsgFromCan::ParsedMessage(parsed_msg) => {
                self.decoded_msgs.get_mut().insert(
                    (parsed_msg.frame.bus, parsed_msg.frame.id.to_wire_u32()),
                    *parsed_msg,
                );
            }
            messages::MsgFromCan::UnparsedMessage(unparsed_msg) => {
                self.undecoded_msgs.get_mut().insert(
                    (unparsed_msg.frame.bus, unparsed_msg.frame.id.to_wire_u32()),
                    *unparsed_msg,
                );
            }
            _ => {}
        }
    }

    fn clean_undecoded(&mut self) {
        // Remove any undecoded messages that have a decoded message with a newer timestamp
        let decoded = &self.decoded_msgs.rt_data;
        let undecoded = self.undecoded_msgs.get_mut();
        undecoded.retain(|&msg_id, unparsed_msg| {
            if let Some(parsed_msg) = msg_id.0.and_then(|bus| decoded.get(&(bus, msg_id.1))) {
                parsed_msg.timestamp <= unparsed_msg.timestamp
            } else {
                true
            }
        });
    }
}

struct MessageCard<'a> {
    bus: daqcore::can::BusId,
    msg_name: &'a str,
    msg_id: u32,
    tx_node: &'a str,
    raw_bytes: &'a str,
    timestamp: &'a str,
    signals: Vec<(&'a str, String)>,
    search: &'a str,
}

impl MessageCard<'_> {
    fn ui(&self, ui: &mut egui::Ui) -> Vec<action::AppAction> {
        let mut action_queue = Vec::new();
        // Header (outside card)
        ui.horizontal(|ui| {
            ui.label(
                egui::RichText::new(format!(
                    "{}  (0x{:03X}, bus {}{})",
                    self.msg_name,
                    self.msg_id & daqcore::can::EXTENDED_ID_MASK,
                    self.bus.raw(),
                    if self.msg_id & daqcore::can::EXTENDED_ID_FLAG != 0 {
                        ", extended"
                    } else {
                        ""
                    }
                ))
                .strong()
                .size(16.0)
                .color(
                    if self.search.is_empty()
                        || self
                            .msg_name
                            .to_lowercase()
                            .contains(&self.search.to_lowercase())
                    {
                        ui.visuals().text_color()
                    } else {
                        ui.visuals().weak_text_color()
                    },
                ),
            );
            ui.label(
                egui::RichText::new(format!("from {}", self.tx_node)).color(
                    if self.search.is_empty()
                        || self
                            .tx_node
                            .to_lowercase()
                            .contains(&self.search.to_lowercase())
                    {
                        ui.visuals().text_color()
                    } else {
                        ui.visuals().weak_text_color()
                    },
                ),
            );
            ui.label(
                egui::RichText::new(self.timestamp)
                    .italics()
                    .color(ui.visuals().weak_text_color()),
            );
            ui.with_layout(egui::Layout::right_to_left(egui::Align::Center), |ui| {
                ui.label(
                    egui::RichText::new(self.raw_bytes)
                        .monospace()
                        .color(ui.visuals().text_color()),
                );
                ui.add_space(2.0);
            });
        });

        ui.add_space(4.0);

        // Card container
        if self.signals.is_empty() {
            return action_queue;
        }

        egui::Frame::group(ui.style())
            .fill(ui.visuals().faint_bg_color)
            .corner_radius(egui::CornerRadius::same(8))
            .inner_margin(egui::Margin::symmetric(8, 6))
            .show(ui, |ui| {
                ui.vertical(|ui| {
                    for (i, (sig_name, value)) in self.signals.iter().enumerate() {
                        ui.horizontal(|ui| {
                            ui.label(
                                egui::RichText::new(*sig_name).monospace().color(
                                    if self.search.is_empty()
                                        || sig_name
                                            .to_lowercase()
                                            .contains(&self.search.to_lowercase())
                                    {
                                        ui.visuals().text_color()
                                    } else {
                                        ui.visuals().weak_text_color()
                                    },
                                ),
                            );
                            ui.with_layout(
                                egui::Layout::right_to_left(egui::Align::Center),
                                |ui| {
                                    if ui.small_button("📊").clicked() {
                                        action_queue.push(action::AppAction::SpawnWidget(
                                            widget_constructor::WidgetConstructor::Scope {
                                                bus: self.bus,
                                                msg_id: self.msg_id,
                                                msg_name: self.msg_name.to_string(),
                                                signal_name: sig_name.to_string(),
                                            },
                                        ));
                                    }
                                    ui.add_space(8.0);
                                    ui.label(egui::RichText::new(value).monospace());
                                },
                            );
                        });
                        if i < self.signals.len() - 1 {
                            ui.separator();
                        }
                    }
                });
            });

        action_queue
    }
}
