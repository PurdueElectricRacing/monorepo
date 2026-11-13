use crate::{telemetry, ui, util};

const V_MIN: f64 = 2.7;
const V_MAX: f64 = 4.2;
const V_NOM: f64 = 3.6;

#[derive(Clone)]
pub struct CellVoltage {
    pub voltage: f64,
    pub balancing: bool,
}

impl CellVoltage {
    pub fn color(&self) -> eframe::egui::Color32 {
        if self.balancing {
            return eframe::egui::Color32::from_rgb(33, 150, 243);
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
    pack_voltage: Option<f64>,
    pack_current: Option<f64>,
    min_cell_voltage: Option<f64>,
    max_cell_voltage: Option<f64>,
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
        let (stale, elapsed) =
            ui::battery::common::sample_age(cell_time.max(charging_time), view.view_time());

        let theme = ui::theme::get_theme(ui.ctx());

        let pack_sum = charging_telemetry.pack_voltage;
        let current = charging_telemetry.pack_current;
        let pack_min = charging_telemetry.min_cell_voltage;
        let pack_max = charging_telemetry.max_cell_voltage;
        let pack_delta = pack_min.zip(pack_max).map(|(min, max)| max - min);

        eframe::egui::ScrollArea::vertical().show(ui, |ui| {
            ui.add_space(4.0);
            ui.heading(&self.title);
            ui.add_space(4.0);

            ui::battery::common::stale_banner(ui, &theme, stale, elapsed);

            ui.add_space(8.0);
            ui.label(
                eframe::egui::RichText::new("PACK SUMMARY")
                    .size(10.0)
                    .color(theme.text_color().linear_multiply(0.5)),
            );
            ui.add_space(4.0);

            ui.columns(5, |cols| {
                ui::battery::common::stat_card(
                    &mut cols[0],
                    &theme,
                    "PACK SUM",
                    pack_sum,
                    "V",
                    stale,
                    None,
                );
                ui::battery::common::stat_card(
                    &mut cols[1],
                    &theme,
                    "CURRENT",
                    current,
                    "A",
                    stale,
                    None,
                );
                ui::battery::common::stat_card(
                    &mut cols[2],
                    &theme,
                    "CELL MIN",
                    pack_min,
                    "V",
                    stale,
                    None,
                );
                ui::battery::common::stat_card(
                    &mut cols[3],
                    &theme,
                    "CELL MAX",
                    pack_max,
                    "V",
                    stale,
                    None,
                );
                ui::battery::common::stat_card(
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
                let voltages = module.iter().flatten().map(|cell| cell.voltage);
                let module_min = voltages.clone().reduce(f64::min);
                let module_max = voltages.clone().reduce(f64::max);
                let module_sum = module_min.map(|_| voltages.sum::<f64>());
                let module_delta = module_min.zip(module_max).map(|(min, max)| max - min);

                let delta_color = match module_delta {
                    Some(delta) if delta > 0.050 => theme.error_color(),
                    Some(delta) if delta > 0.020 => theme.warning_color(),
                    _ => theme.text_color().linear_multiply(0.55),
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
                                    ui::battery::common::reading(module_sum, 2)
                                ))
                                .size(10.0)
                                .color(theme.text_color().linear_multiply(0.55)),
                            );
                            ui.label(
                                eframe::egui::RichText::new(format!(
                                    "min {} V",
                                    ui::battery::common::reading(module_min, 2)
                                ))
                                .size(10.0)
                                .color(theme.text_color().linear_multiply(0.55)),
                            );
                            ui.label(
                                eframe::egui::RichText::new(format!(
                                    "max {} V",
                                    ui::battery::common::reading(module_max, 2)
                                ))
                                .size(10.0)
                                .color(theme.text_color().linear_multiply(0.55)),
                            );
                            ui.label(
                                eframe::egui::RichText::new(format!(
                                    "Δ {} V",
                                    ui::battery::common::reading(module_delta, 2)
                                ))
                                .size(10.0)
                                .color(delta_color),
                            );
                        });

                        ui.add_space(6.0);

                        let available_width = ui.available_width();
                        let cell_spacing = ui.spacing().item_spacing.x;
                        let cell_count = ui::battery::common::CELLS_CHIP_0 as f32;
                        let bar_width =
                            ((available_width - cell_spacing * (cell_count - 1.0)) / cell_count)
                                .max(8.0);

                        let (chip0_cells, chip1_cells) =
                            module.split_at(ui::battery::common::CELLS_CHIP_0);

                        ui.horizontal(|ui| {
                            for cell in chip0_cells {
                                cell_bar(ui, &theme, cell.as_ref(), stale, bar_width);
                            }
                        });
                        ui.add_space(4.0);
                        ui.horizontal(|ui| {
                            for cell in chip1_cells {
                                cell_bar(ui, &theme, cell.as_ref(), stale, bar_width);
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
    cell: Option<&CellVoltage>,
    stale: bool,
    bar_w: f32,
) {
    let fill_color = if stale {
        theme.text_color().linear_multiply(0.12)
    } else if let Some(cell) = cell {
        cell.color()
    } else {
        eframe::egui::Color32::GRAY
    };

    let fill_frac = if let Some(cell) = cell {
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

        let text = match cell.filter(|_| !stale) {
            Some(cell) => format!("{:.2}", cell.voltage),
            None => "—".to_string(),
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

fn cell_sample(frame: &daqcore::frame::ParsedFrame) -> Option<(usize, usize, CellVoltage)> {
    let decoded = frame.decoded.as_ref()?;
    if !matches!(decoded.name(), "cell_telemetry" | "cell_telemetry_ccan") {
        return None;
    }

    let module = decoded.signal("module_num")?.physical().round() as usize;
    let cell = decoded.signal("cell_num")?.physical().round() as usize;
    let voltage = decoded.signal("voltage")?.physical();
    let balance = decoded.signal("balance_status")?;
    let balancing = match balance.raw() {
        daqcore::superdbc::message::RawValue::Integer(value) => value != 0,
        daqcore::superdbc::message::RawValue::Float32(_) => balance.physical() > 0.5,
    };
    let value = CellVoltage { voltage, balancing };

    Some((module, cell, value))
}

/// Reconstruct multiplexed slots independently; newest frame per CAN ID is insufficient.
fn cell_voltages(
    frames: &[daqcore::frame::ParsedFrame],
) -> (Vec<Vec<Option<CellVoltage>>>, Option<daqcore::time::Time>) {
    let mut modules = vec![
        vec![None; ui::battery::common::CELLS_PER_MODULE];
        ui::battery::common::NUM_MODULES
    ];
    let mut last_update = None;
    let mut filled = 0;
    for frame in frames.iter().rev() {
        if let Some((module, cell, value)) = cell_sample(frame)
            && module < ui::battery::common::NUM_MODULES
            && cell < ui::battery::common::CELLS_PER_MODULE
            && modules[module][cell].is_none()
        {
            modules[module][cell] = Some(value);
            last_update = last_update.max(Some(frame.timestamp));
            filled += 1;
            if filled == ui::battery::common::NUM_MODULES * ui::battery::common::CELLS_PER_MODULE {
                break;
            }
        }
    }
    (modules, last_update)
}

fn charging_voltages(
    frames: &[daqcore::frame::ParsedFrame],
) -> (ChargingVoltageTelemetry, Option<daqcore::time::Time>) {
    let mut values = ChargingVoltageTelemetry::default();
    let mut last_update = None;
    for frame in frames.iter().rev() {
        let Some(decoded) = &frame.decoded else {
            continue;
        };

        match decoded.name() {
            "bms_voltages" | "bms_voltages_ccan" => {
                for (name, signal) in decoded.iter() {
                    let field = match name {
                        "pack_voltage" => &mut values.pack_voltage,
                        "min_cell_voltage" => &mut values.min_cell_voltage,
                        "max_cell_voltage" => &mut values.max_cell_voltage,
                        _ => continue,
                    };

                    if field.is_none() {
                        *field = Some(signal.physical());
                    }
                }
                last_update = last_update.max(Some(frame.timestamp));
            }
            "pack_analog" | "pack_analog_ccan" => {
                if let Some(signal) = decoded.signal("pack_current")
                    && values.pack_current.is_none()
                {
                    values.pack_current = Some(signal.physical());
                }
                last_update = last_update.max(Some(frame.timestamp));
            }
            _ => {}
        }
    }
    (values, last_update)
}
