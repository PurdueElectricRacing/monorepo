use crate::{action, app, telemetry, widget_constructor};

pub struct ViewerTable {
    pub title: String,
    search: String,
    tx_node: String,
}

impl ViewerTable {
    pub fn new(instance: usize) -> Self {
        Self {
            title: format!("CAN Viewer Table #{instance}"),
            search: String::new(),
            tx_node: "Any".into(),
        }
    }

    pub fn show(
        &mut self,
        ui: &mut eframe::egui::Ui,
        actions: &mut Vec<action::AppAction>,
        formatter: &Option<daqcore::formatter::Formatter>,
        parser: Option<&app::ParserInfo>,
        view: &telemetry::TelemetryView<'_>,
    ) -> egui_tiles::UiResponse {
        ui.heading(&self.title);
        ui.horizontal(|ui| {
            ui.label("Search:");
            ui.text_edit_singleline(&mut self.search);
            let mut nodes: Vec<_> = view
                .latest
                .values()
                .filter_map(|f| f.decoded.as_ref().map(|d| d.tx_node.clone()))
                .collect();
            nodes.sort();
            nodes.dedup();
            eframe::egui::ComboBox::from_id_salt(("tx_node", &self.title))
                .selected_text(&self.tx_node)
                .show_ui(ui, |ui| {
                    ui.selectable_value(&mut self.tx_node, "Any".into(), "Any");
                    ui.selectable_value(&mut self.tx_node, "Unparsed".into(), "Unparsed");
                    for node in nodes {
                        ui.selectable_value(&mut self.tx_node, node.clone(), node);
                    }
                });
        });
        if view.frames.is_empty() {
            ui.label("No retained CAN messages in the selected interval.");
        }

        let search = self.search.to_lowercase();
        eframe::egui::ScrollArea::vertical().show(ui, |ui| {
            for frame in view.latest.values() {
                let decoded = frame.decoded.as_ref();
                let name = decoded.map_or("Error: Unknown", |d| d.name.as_str());
                let node = decoded.map_or("Unparsed", |d| d.tx_node.as_str());
                if self.tx_node != "Any" && self.tx_node != node {
                    continue;
                }

                if !search.is_empty()
                    && !name.to_lowercase().contains(&search)
                    && !node.to_lowercase().contains(&search)
                    && !format!("{:03X}", frame.identity.raw_id())
                        .to_lowercase()
                        .contains(&search)
                    && !decoded.is_some_and(|d| {
                        d.signals.keys().any(|s| s.to_lowercase().contains(&search))
                    })
                {
                    continue;
                }

                let id = frame.identity.dbc_id();
                let definition = parser.and_then(|p| p.parser.msg_def(id));
                let signals = decoded
                    .map(|d| {
                        d.signals
                            .iter()
                            .map(|(name, sig)| {
                                let def = definition
                                    .and_then(|m| m.signals.iter().find(|s| s.name == *name));
                                (
                                    name.as_str(),
                                    daqcore::formatter::try_format(
                                        formatter,
                                        &d.name,
                                        name,
                                        def,
                                        Some(&sig.unit),
                                        &sig.value,
                                    ),
                                )
                            })
                            .collect()
                    })
                    .unwrap_or_default();
                let bytes = frame
                    .raw_bytes
                    .iter()
                    .map(|b| format!("{b:02X}"))
                    .collect::<Vec<_>>()
                    .join(" ");
                actions.extend(
                    MessageCard {
                        msg_name: name,
                        identity: frame.identity,
                        tx_node: node,
                        raw_bytes: &bytes,
                        timestamp: &frame.timestamp.label(),
                        signals,
                        search: &self.search,
                    }
                    .ui(ui),
                );
                ui.add_space(8.0);
            }
        });
        egui_tiles::UiResponse::None
    }
}

struct MessageCard<'a> {
    msg_name: &'a str,
    identity: daqcore::frame::CanIdentity,
    tx_node: &'a str,
    raw_bytes: &'a str,
    timestamp: &'a str,
    signals: Vec<(&'a str, String)>,
    search: &'a str,
}

impl MessageCard<'_> {
    fn ui(&self, ui: &mut eframe::egui::Ui) -> Vec<action::AppAction> {
        let mut action_queue = Vec::new();

        // Header (outside card)
        ui.horizontal(|ui| {
            let search = self.search.to_lowercase();
            let matches_name = search.is_empty() || self.msg_name.to_lowercase().contains(&search);
            let name_color = if matches_name {
                ui.visuals().text_color()
            } else {
                ui.visuals().weak_text_color()
            };

            ui.label(
                eframe::egui::RichText::new(format!("{}  ({})", self.msg_name, self.identity))
                    .strong()
                    .size(16.0)
                    .color(name_color),
            );

            let matches_node = search.is_empty() || self.tx_node.to_lowercase().contains(&search);
            let node_color = if matches_node {
                ui.visuals().text_color()
            } else {
                ui.visuals().weak_text_color()
            };

            ui.label(
                eframe::egui::RichText::new(format!("from {}", self.tx_node)).color(node_color),
            );
            ui.label(
                eframe::egui::RichText::new(self.timestamp)
                    .italics()
                    .color(ui.visuals().weak_text_color()),
            );
            ui.with_layout(
                eframe::egui::Layout::right_to_left(eframe::egui::Align::Center),
                |ui| {
                    ui.label(
                        eframe::egui::RichText::new(self.raw_bytes)
                            .monospace()
                            .color(ui.visuals().text_color()),
                    );
                    ui.add_space(2.0);
                },
            );
        });

        ui.add_space(4.0);

        // Card container
        if self.signals.is_empty() {
            return action_queue;
        }

        eframe::egui::Frame::group(ui.style())
            .fill(ui.visuals().faint_bg_color)
            .corner_radius(eframe::egui::CornerRadius::same(8))
            .inner_margin(eframe::egui::Margin::symmetric(8, 6))
            .show(ui, |ui| {
                ui.vertical(|ui| {
                    for (i, (sig_name, value)) in self.signals.iter().enumerate() {
                        ui.horizontal(|ui| {
                            let search = self.search.to_lowercase();
                            let matches_signal =
                                search.is_empty() || sig_name.to_lowercase().contains(&search);
                            let signal_color = if matches_signal {
                                ui.visuals().text_color()
                            } else {
                                ui.visuals().weak_text_color()
                            };

                            ui.label(
                                eframe::egui::RichText::new(*sig_name)
                                    .monospace()
                                    .color(signal_color),
                            );
                            ui.with_layout(
                                eframe::egui::Layout::right_to_left(eframe::egui::Align::Center),
                                |ui| {
                                    if ui.small_button("📊").clicked() {
                                        action_queue.push(action::AppAction::SpawnWidget(
                                            widget_constructor::WidgetConstructor::Scope {
                                                identity: self.identity,
                                                msg_name: self.msg_name.to_string(),
                                                signal_name: sig_name.to_string(),
                                            },
                                        ));
                                    }
                                    ui.add_space(8.0);
                                    ui.label(eframe::egui::RichText::new(value).monospace());
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
