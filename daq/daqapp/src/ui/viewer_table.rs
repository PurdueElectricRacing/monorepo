use crate::{action, app, telemetry, ui, widget_constructor};

const STALE_BG_ALPHA: u8 = 25;
const STALE_BORDER_WIDTH: f32 = 1.0;
const STALE_BORDER_ALPHA_FACTOR: f32 = 0.5;
const UNMATCHED_STALE_GAMMA: f32 = 0.6;

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
        database: Option<&app::DatabaseSelection>,
        view: &telemetry::TelemetryView<'_>,
    ) -> egui_tiles::UiResponse {
        ui.heading(&self.title);
        ui.horizontal(|ui| {
            ui.label("Search:");
            ui.text_edit_singleline(&mut self.search);
            let mut nodes: Vec<_> = view
                .latest_setpoint_frames
                .values()
                .filter_map(|f| f.decoded.as_ref().map(|d| d.tx_node().to_owned()))
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
        if view.setpoint_frames.is_empty() {
            ui.label("No retained CAN messages in the selected interval.");
        }

        let search = self.search.to_lowercase();
        eframe::egui::ScrollArea::vertical().show(ui, |ui| {
            for frame in view.latest_setpoint_frames.values() {
                let decoded = frame.decoded.as_ref();
                let name = decoded.map_or("Error: Unknown", |d| d.name());
                let node = decoded.map_or("Unparsed", |d| d.tx_node());
                let bus_name = database.map_or("Unknown", |db| db.bus_name(frame.bus_id));
                if self.tx_node != "Any" && self.tx_node != node {
                    continue;
                }

                let definition = decoded.and_then(|decoded| {
                    let key = decoded.key();
                    database?.database.message_by_key(key)
                });
                let stale = definition
                    .is_some_and(|message| message.is_stale(frame.timestamp, view.view_time()));

                if !search.is_empty()
                    && !name.to_lowercase().contains(&search)
                    && !node.to_lowercase().contains(&search)
                    && !bus_name.to_lowercase().contains(&search)
                    && !(stale && "stale".contains(&search))
                    && !format!("{:03X}", frame.identity)
                        .to_lowercase()
                        .contains(&search)
                    && !decoded.is_some_and(|d| {
                        d.iter()
                            .any(|(name, _)| name.to_lowercase().contains(&search))
                    })
                {
                    continue;
                }

                let signals = decoded
                    .map(|d| {
                        d.iter()
                            .map(|(name, sig)| {
                                let def = definition.and_then(|message| message.signal(name));
                                (
                                    name,
                                    daqcore::formatter::try_format(
                                        formatter,
                                        d.name(),
                                        name,
                                        def,
                                        None,
                                        sig,
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
                        bus_name,
                        key: frame.key(),
                        tx_node: node,
                        raw_bytes: &bytes,
                        timestamp: &frame.timestamp.label(),
                        stale,
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
    bus_name: &'a str,
    key: daqcore::superdbc::message::MessageKey,
    tx_node: &'a str,
    raw_bytes: &'a str,
    timestamp: &'a str,
    stale: bool,
    signals: Vec<(&'a str, String)>,
    search: &'a str,
}

impl MessageCard<'_> {
    fn ui(&self, ui: &mut eframe::egui::Ui) -> Vec<action::AppAction> {
        let mut action_queue = Vec::new();
        let theme = ui::theme::get_theme(ui.ctx());
        let error_color = theme.error_color();

        // Header (outside card)
        ui.horizontal(|ui| {
            let search = self.search.to_lowercase();
            let matches_name = search.is_empty()
                || self.msg_name.to_lowercase().contains(&search)
                || self.bus_name.to_lowercase().contains(&search);
            let name_color = match (self.stale, matches_name) {
                (true, true) => error_color,
                (true, false) => error_color.gamma_multiply(UNMATCHED_STALE_GAMMA),
                (false, true) => ui.visuals().text_color(),
                (false, false) => ui.visuals().weak_text_color(),
            };

            ui.label(
                eframe::egui::RichText::new(format!(
                    "{}: {}  [0x{:X} | 0x{:03X}]",
                    self.bus_name, self.msg_name, self.key.bus_id, self.key.identity
                ))
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
            if self.stale {
                ui.label(
                    eframe::egui::RichText::new("STALE")
                        .strong()
                        .color(error_color),
                );
            }
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

        let mut frame = eframe::egui::Frame::group(ui.style())
            .fill(ui.visuals().faint_bg_color)
            .corner_radius(eframe::egui::CornerRadius::same(8))
            .inner_margin(eframe::egui::Margin::symmetric(8, 6));
        if self.stale {
            frame = frame
                .fill(eframe::egui::Color32::from_rgba_unmultiplied(
                    error_color.r(),
                    error_color.g(),
                    error_color.b(),
                    STALE_BG_ALPHA,
                ))
                .stroke(eframe::egui::Stroke::new(
                    STALE_BORDER_WIDTH,
                    error_color.linear_multiply(STALE_BORDER_ALPHA_FACTOR),
                ));
        }

        frame.show(ui, |ui| {
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
                                            key: self.key,
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
