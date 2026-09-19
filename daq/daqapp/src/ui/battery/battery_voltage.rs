use super::common::{self, BatteryUiState};
use crate::{messages, ui, util};
use eframe::egui::{self, Color32, Frame, RichText, Stroke};

const V_MIN: f64 = 2.7;
const V_MAX: f64 = 4.2;
const V_NOM: f64 = 3.7;

#[derive(Default, Clone)]
pub struct CellVoltage {
    pub voltage: f64,
    pub balancing: bool,
}

impl CellVoltage {
    pub fn color(&self) -> Color32 {
        if self.balancing {
            return Color32::from_rgb(33, 150, 243);
        }

        let voltage = self.voltage.clamp(V_MIN, V_MAX);

        let hue = if voltage <= V_NOM {
            let t = (voltage - V_MIN) / (V_NOM - V_MIN);
            util::lerp(0.0, 45.0, t)
        } else {
            let t = (voltage - V_NOM) / (V_MAX - V_NOM);
            util::lerp(45.0, 120.0, t)
        };

        util::hsv_to_color32(hue, 1.0, 1.0)
    }
}

#[derive(Default)]
struct ChargingVoltageTelemetry {
    pack_voltage: f64,
    pack_current: f64,
    min_cell_voltage: f64,
    max_cell_voltage: f64,
}

pub struct BatteryVoltage {
    pub title: String,
    modules: Vec<Vec<CellVoltage>>,
    charging_telemetry: Option<ChargingVoltageTelemetry>,
    ui_state: BatteryUiState,
}

impl BatteryVoltage {
    pub fn new(instance_num: usize) -> Self {
        Self {
            title: format!("Battery Voltage #{}", instance_num),
            modules: vec![
                vec![CellVoltage::default(); common::CELLS_PER_MODULE];
                common::NUM_MODULES
            ],
            charging_telemetry: None,
            ui_state: BatteryUiState::new(),
        }
    }

    pub fn handle_can_message(&mut self, msg: &messages::MsgFromCan) {
        if let messages::MsgFromCan::ParsedMessage(parsed) = msg {
            match parsed.decoded.name.as_str() {
                "cell_telemetry" | "cell_telemetry_ccan" => {
                    let mut module_num: Option<usize> = None;
                    let mut cell_num: Option<usize> = None;
                    let mut voltage: Option<f64> = None;
                    let mut balancing: Option<bool> = None;

                    for (_, sig) in parsed.decoded.signals.iter() {
                        match sig.name.as_str() {
                            "module_num" => module_num = Some(sig.value.physical.round() as usize),
                            "cell_num" => cell_num = Some(sig.value.physical.round() as usize),
                            "voltage" => voltage = Some(sig.value.physical),
                            "balance_status" => {
                                balancing = Some(
                                    sig.value
                                        .raw
                                        .map(|v| v != 0)
                                        .unwrap_or(sig.value.physical > 0.5),
                                )
                            }
                            _ => {}
                        }
                    }

                    if let (Some(module_num), Some(cell_num), Some(voltage), Some(balancing)) =
                        (module_num, cell_num, voltage, balancing)
                        && module_num < self.modules.len()
                        && cell_num < self.modules[module_num].len()
                    {
                        self.modules[module_num][cell_num].voltage = voltage;
                        self.modules[module_num][cell_num].balancing = balancing;
                        self.ui_state.mark_updated();
                    }
                }
                "pack_bms" | "pack_bms_ccan" => {
                    for (_, sig) in parsed.decoded.signals.iter() {
                        match sig.name.as_str() {
                            "pack_voltage" => {
                                self.charging_telemetry.get_or_insert_default().pack_voltage =
                                    sig.value.physical;
                            }
                            "min_cell_voltage" => {
                                self.charging_telemetry
                                    .get_or_insert_default()
                                    .min_cell_voltage = sig.value.physical;
                            }
                            "max_cell_voltage" => {
                                self.charging_telemetry
                                    .get_or_insert_default()
                                    .max_cell_voltage = sig.value.physical;
                            }
                            _ => {}
                        }
                    }

                    self.ui_state.mark_updated();
                }
                "pack_analog" | "pack_analog_ccan" => {
                    for (_, sig) in parsed.decoded.signals.iter() {
                        if sig.name.as_str() == "pack_current" {
                            self.charging_telemetry.get_or_insert_default().pack_current =
                                sig.value.physical;
                        }
                    }

                    self.ui_state.mark_updated();
                }
                _ => {}
            }
        }
    }

    pub fn show(&mut self, ui: &mut egui::Ui) -> egui_tiles::UiResponse {
        let theme = ui::theme::get_theme(ui.ctx());
        let (stale, elapsed) = self.ui_state.refresh();

        let pack_sum = self.charging_telemetry.as_ref().map(|t| t.pack_voltage);
        let current = self.charging_telemetry.as_ref().map(|t| t.pack_current);
        let pack_min = self.charging_telemetry.as_ref().map(|t| t.min_cell_voltage);
        let pack_max = self.charging_telemetry.as_ref().map(|t| t.max_cell_voltage);
        let pack_delta = if let (Some(min), Some(max)) = (pack_min, pack_max) {
            Some(max - min)
        } else {
            None
        };

        egui::ScrollArea::vertical().show(ui, |ui| {
            ui.add_space(4.0);
            ui.heading(&self.title);
            ui.add_space(4.0);

            common::stale_banner(ui, &theme, stale, elapsed);

            ui.add_space(8.0);
            ui.label(
                RichText::new("PACK SUMMARY")
                    .size(10.0)
                    .color(theme.text_color().linear_multiply(0.5)),
            );
            ui.add_space(4.0);

            ui.columns(5, |cols| {
                common::stat_card(&mut cols[0], &theme, "PACK SUM", pack_sum, "V", stale, None);
                common::stat_card(&mut cols[1], &theme, "CURRENT", current, "A", stale, None);
                common::stat_card(&mut cols[2], &theme, "CELL MIN", pack_min, "V", stale, None);
                common::stat_card(&mut cols[3], &theme, "CELL MAX", pack_max, "V", stale, None);
                common::stat_card(
                    &mut cols[4],
                    &theme,
                    "DELTA",
                    pack_delta,
                    "V",
                    stale,
                    pack_delta.map(|delta| {
                        if delta > 0.050 {
                            theme.error_color()
                        } else if delta > 0.020 {
                            theme.warning_color()
                        } else {
                            theme.success_color()
                        }
                    }),
                );
            });

            ui.add_space(12.0);

            for (module_index, module) in self.modules.iter().enumerate() {
                let module_sum: f64 = module.iter().map(|cell| cell.voltage).sum();
                let module_min = module
                    .iter()
                    .map(|cell| cell.voltage)
                    .fold(f64::MAX, f64::min);
                let module_max = module
                    .iter()
                    .map(|cell| cell.voltage)
                    .fold(f64::MIN, f64::max);
                let module_delta = if module_min < f64::MAX {
                    module_max - module_min
                } else {
                    0.0
                };

                Frame::NONE
                    .fill(theme.panel_color())
                    .stroke(Stroke::new(1.0, theme.accent_color()))
                    .inner_margin(egui::Margin::same(10))
                    .corner_radius(egui::CornerRadius::same(4))
                    .show(ui, |ui| {
                        ui.horizontal(|ui| {
                            ui.label(
                                RichText::new(format!("MODULE {module_index}"))
                                    .size(11.0)
                                    .strong(),
                            );
                            ui.add_space(8.0);
                            ui.label(
                                RichText::new(format!("sum {:.2} V", module_sum))
                                    .size(10.0)
                                    .color(theme.text_color().linear_multiply(0.55)),
                            );
                            ui.label(
                                RichText::new(format!("min {:.2} V", module_min))
                                    .size(10.0)
                                    .color(theme.text_color().linear_multiply(0.55)),
                            );
                            ui.label(
                                RichText::new(format!("max {:.2} V", module_max))
                                    .size(10.0)
                                    .color(theme.text_color().linear_multiply(0.55)),
                            );
                            ui.label(
                                RichText::new(format!("Δ {:.2} V", module_delta))
                                    .size(10.0)
                                    .color(if module_delta > 0.050 {
                                        theme.error_color()
                                    } else if module_delta > 0.020 {
                                        theme.warning_color()
                                    } else {
                                        theme.text_color().linear_multiply(0.55)
                                    }),
                            );
                        });

                        ui.add_space(6.0);

                        let available_width = ui.available_width();
                        let cell_spacing = ui.spacing().item_spacing.x;
                        let cell_count = common::CELLS_PER_MODULE as f32;
                        let bar_width =
                            ((available_width - cell_spacing * cell_count) / cell_count).max(8.0);

                        ui.horizontal(|ui| {
                            for cell in module.iter() {
                                Self::cell_bar(ui, &theme, cell, stale, bar_width);
                            }
                        });
                    });

                ui.add_space(6.0);
            }
        });

        egui_tiles::UiResponse::None
    }

    fn cell_bar(
        ui: &mut egui::Ui,
        theme: &ui::theme::ThemeColors,
        cell: &CellVoltage,
        stale: bool,
        bar_w: f32,
    ) {
        use egui::{Align2, FontId};

        let fill_color = if stale {
            theme.text_color().linear_multiply(0.12)
        } else {
            cell.color()
        };

        let fill_frac = ((cell.voltage - V_MIN) / (V_MAX - V_MIN)).clamp(0.0, 1.0) as f32;

        ui.vertical(|ui| {
            ui.set_max_width(bar_w + 4.0);

            let (outer_rect, _) =
                ui.allocate_exact_size(egui::Vec2::new(bar_w, 20.0), egui::Sense::hover());

            let painter = ui.painter();
            painter.rect_filled(outer_rect, 3.0, theme.text_color().linear_multiply(0.06));
            painter.rect_stroke(
                outer_rect,
                3.0,
                Stroke::new(0.5, theme.accent_color()),
                egui::StrokeKind::Inside,
            );

            let fill_height = outer_rect.height() * fill_frac;
            let fill_rect = egui::Rect::from_min_max(
                egui::pos2(outer_rect.min.x, outer_rect.max.y - fill_height),
                outer_rect.max,
            );
            painter.rect_filled(fill_rect, 2.0, fill_color);

            let text = if stale {
                "—".to_string()
            } else {
                format!("{:.2}", cell.voltage)
            };

            let text_color = if stale {
                theme.text_color().linear_multiply(0.25)
            } else if fill_frac > 0.5 {
                egui::Color32::BLACK
            } else {
                egui::Color32::WHITE
            };

            painter.text(
                outer_rect.center(),
                Align2::CENTER_CENTER,
                text,
                FontId::proportional(10.0),
                text_color,
            );
        });
    }
}
