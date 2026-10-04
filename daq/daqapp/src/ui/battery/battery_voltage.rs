use crate::{
    telemetry,
    ui::{self, battery::common},
    util,
};

const V_MIN: f64 = 2.7;
const V_MAX: f64 = 4.2;
const V_NOM: f64 = 3.6;

#[derive(Clone)]
pub struct CellVoltage {
    pub voltage: f64,
    pub balancing: bool,
}

impl Default for CellVoltage {
    fn default() -> Self {
        Self {
            voltage: f64::NAN,
            balancing: false,
        }
    }
}

impl CellVoltage {
    pub fn color(&self) -> eframe::egui::Color32 {
        if self.balancing {
            return eframe::egui::Color32::from_rgb(33, 150, 243);
        }

        if !self.voltage.is_finite() {
            return eframe::egui::Color32::GRAY;
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

struct ChargingVoltageTelemetry {
    pack_voltage: f64,
    pack_current: f64,
    min_cell_voltage: f64,
    max_cell_voltage: f64,
}

impl Default for ChargingVoltageTelemetry {
    fn default() -> Self {
        Self {
            pack_voltage: f64::NAN,
            pack_current: f64::NAN,
            min_cell_voltage: f64::NAN,
            max_cell_voltage: f64::NAN,
        }
    }
}

pub struct BatteryVoltage {
    pub title: String,
}

impl BatteryVoltage {
    pub fn new(instance_num: usize) -> Self {
        Self {
            title: format!("Battery Voltage #{}", instance_num),
        }
    }

    pub fn show(
        &self,
        ui: &mut eframe::egui::Ui,
        view: &telemetry::TelemetryView<'_>,
    ) -> egui_tiles::UiResponse {
        let (modules, cell_time) = cell_voltages(view.setpoint_frames);
        let (charging_telemetry, charging_time) = charging_voltages(view.setpoint_frames);
        let (stale, elapsed) = common::sample_age(cell_time.max(charging_time), view.view_time());

        let theme = ui::theme::get_theme(ui.ctx());

        let pack_sum = charging_telemetry.as_ref().map(|t| t.pack_voltage);
        let current = charging_telemetry.as_ref().map(|t| t.pack_current);
        let pack_min = charging_telemetry.as_ref().map(|t| t.min_cell_voltage);
        let pack_max = charging_telemetry.as_ref().map(|t| t.max_cell_voltage);
        let pack_delta = if let (Some(min), Some(max)) = (pack_min, pack_max) {
            Some(max - min)
        } else {
            None
        };

        eframe::egui::ScrollArea::vertical().show(ui, |ui| {
            ui.add_space(4.0);
            ui.heading(&self.title);
            ui.add_space(4.0);

            common::stale_banner(ui, &theme, stale, elapsed);

            ui.add_space(8.0);
            ui.label(
                eframe::egui::RichText::new("PACK SUMMARY")
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

            for (module_index, module) in modules.iter().enumerate() {
                let module_sum: f64 = module
                    .iter()
                    .map(|cell| cell.voltage)
                    .filter(|v| v.is_finite())
                    .sum();
                let module_min = module
                    .iter()
                    .map(|cell| cell.voltage)
                    .filter(|v| v.is_finite())
                    .reduce(f64::min)
                    .unwrap_or(f64::NAN);
                let module_max = module
                    .iter()
                    .map(|cell| cell.voltage)
                    .filter(|v| v.is_finite())
                    .reduce(f64::max)
                    .unwrap_or(f64::NAN);
                let module_delta = if module_min.is_finite() {
                    module_max - module_min
                } else {
                    f64::NAN
                };

                let delta_color = if module_delta > 0.050 {
                    theme.error_color()
                } else if module_delta > 0.020 {
                    theme.warning_color()
                } else {
                    theme.text_color().linear_multiply(0.55)
                };

                eframe::egui::Frame::NONE
                    .fill(theme.panel_color())
                    .stroke(eframe::egui::Stroke::new(1.0_f32, theme.accent_color()))
                    .inner_margin(eframe::egui::Margin::same(10))
                    .corner_radius(eframe::egui::CornerRadius::same(4))
                    .show(ui, |ui| {
                        ui.horizontal(|ui| {
                            ui.label(
                                eframe::egui::RichText::new(format!("MODULE {module_index}"))
                                    .size(11.0)
                                    .strong(),
                            );
                            ui.add_space(8.0);
                            ui.label(
                                eframe::egui::RichText::new(format!(
                                    "sum {} V",
                                    common::reading(module_sum, 2)
                                ))
                                .size(10.0)
                                .color(theme.text_color().linear_multiply(0.55)),
                            );
                            ui.label(
                                eframe::egui::RichText::new(format!(
                                    "min {} V",
                                    common::reading(module_min, 2)
                                ))
                                .size(10.0)
                                .color(theme.text_color().linear_multiply(0.55)),
                            );
                            ui.label(
                                eframe::egui::RichText::new(format!(
                                    "max {} V",
                                    common::reading(module_max, 2)
                                ))
                                .size(10.0)
                                .color(theme.text_color().linear_multiply(0.55)),
                            );
                            ui.label(
                                eframe::egui::RichText::new(format!(
                                    "Δ {} V",
                                    common::reading(module_delta, 2)
                                ))
                                .size(10.0)
                                .color(delta_color),
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
                                cell_bar(ui, &theme, cell, stale, bar_width);
                            }
                        });
                    });

                ui.add_space(6.0);
            }
        });

        egui_tiles::UiResponse::None
    }
}

fn cell_bar(
    ui: &mut eframe::egui::Ui,
    theme: &ui::theme::ThemeColors,
    cell: &CellVoltage,
    stale: bool,
    bar_w: f32,
) {
    let fill_color = if stale {
        theme.text_color().linear_multiply(0.12)
    } else {
        cell.color()
    };

    let fill_frac = if cell.voltage.is_finite() {
        ((cell.voltage - V_MIN) / (V_MAX - V_MIN)).clamp(0.0, 1.0) as f32
    } else {
        0.0
    };

    ui.vertical(|ui| {
        ui.set_max_width(bar_w + 4.0);

        let (outer_rect, _) = ui.allocate_exact_size(
            eframe::egui::Vec2::new(bar_w, 20.0),
            eframe::egui::Sense::hover(),
        );

        let painter = ui.painter();
        painter.rect_filled(outer_rect, 3.0, theme.text_color().linear_multiply(0.06));
        painter.rect_stroke(
            outer_rect,
            3.0,
            eframe::egui::Stroke::new(0.5_f32, theme.accent_color()),
            eframe::egui::StrokeKind::Inside,
        );

        let fill_height = outer_rect.height() * fill_frac;
        let fill_rect = eframe::egui::Rect::from_min_max(
            eframe::egui::pos2(outer_rect.min.x, outer_rect.max.y - fill_height),
            outer_rect.max,
        );
        painter.rect_filled(fill_rect, 2.0, fill_color);

        let text = if stale || !cell.voltage.is_finite() {
            "—".to_string()
        } else {
            format!("{:.2}", cell.voltage)
        };

        let text_color = if stale {
            theme.text_color().linear_multiply(0.25)
        } else if fill_frac > 0.5 {
            eframe::egui::Color32::BLACK
        } else {
            eframe::egui::Color32::WHITE
        };

        painter.text(
            outer_rect.center(),
            eframe::egui::Align2::CENTER_CENTER,
            text,
            eframe::egui::FontId::proportional(10.0),
            text_color,
        );
    });
}

fn cell_sample(frame: &daqcore::ParsedFrame) -> Option<(usize, usize, CellVoltage)> {
    let decoded = frame.decoded.as_ref()?;
    if !matches!(
        decoded.name.as_str(),
        "cell_telemetry" | "cell_telemetry_ccan"
    ) {
        return None;
    }

    let module = decoded.signals.get("module_num")?.value.physical.round() as usize;
    let cell = decoded.signals.get("cell_num")?.value.physical.round() as usize;
    let voltage = decoded.signals.get("voltage")?.value.physical;
    let balance = &decoded.signals.get("balance_status")?.value;
    let balancing = balance
        .raw
        .map_or(balance.physical > 0.5, |value| value != 0);
    let value = CellVoltage { voltage, balancing };

    Some((module, cell, value))
}

/// Reconstruct multiplexed slots independently; newest frame per CAN ID is insufficient.
fn cell_voltages(
    frames: &[daqcore::ParsedFrame],
) -> (Vec<Vec<CellVoltage>>, Option<daqcore::Time>) {
    let mut modules =
        vec![vec![CellVoltage::default(); common::CELLS_PER_MODULE]; common::NUM_MODULES];
    let mut last_update = None;
    let mut filled = 0;
    for frame in frames.iter().rev() {
        if let Some((module, cell, value)) = cell_sample(frame)
            && module < common::NUM_MODULES
            && cell < common::CELLS_PER_MODULE
            && modules[module][cell].voltage.is_nan()
        {
            modules[module][cell] = value;
            last_update = last_update.max(Some(frame.timestamp));
            filled += 1;
            if filled == common::NUM_MODULES * common::CELLS_PER_MODULE {
                break;
            }
        }
    }
    (modules, last_update)
}

fn charging_voltages(
    frames: &[daqcore::ParsedFrame],
) -> (Option<ChargingVoltageTelemetry>, Option<daqcore::Time>) {
    let mut values: Option<ChargingVoltageTelemetry> = None;
    let mut last_update = None;
    for frame in frames.iter().rev() {
        let Some(decoded) = &frame.decoded else {
            continue;
        };

        match decoded.name.as_str() {
            "pack_bms" | "pack_bms_ccan" => {
                for (_, signal) in &decoded.signals {
                    let field = match signal.name.as_str() {
                        "pack_voltage" => &mut values.get_or_insert_default().pack_voltage,
                        "min_cell_voltage" => &mut values.get_or_insert_default().min_cell_voltage,
                        "max_cell_voltage" => &mut values.get_or_insert_default().max_cell_voltage,
                        _ => continue,
                    };

                    if field.is_nan() {
                        *field = signal.value.physical;
                    }
                }
                last_update = last_update.max(Some(frame.timestamp));
            }
            "pack_analog" | "pack_analog_ccan" => {
                if let Some(signal) = decoded.signals.get("pack_current") {
                    let field = &mut values.get_or_insert_default().pack_current;
                    if field.is_nan() {
                        *field = signal.value.physical;
                    }
                }
                last_update = last_update.max(Some(frame.timestamp));
            }
            _ => {}
        }
    }
    (values, last_update)
}
