use crate::{telemetry, ui, util};

const T_MIN: f64 = 15.0;
const T_MAX: f64 = 45.0;
const T_NOM: f64 = 25.0;

#[derive(Clone)]
pub struct ThermistorTemperature {
    pub temperature: f64,
}

impl Default for ThermistorTemperature {
    fn default() -> Self {
        Self {
            temperature: f64::NAN,
        }
    }
}

impl ThermistorTemperature {
    pub fn color(&self) -> eframe::egui::Color32 {
        if !self.temperature.is_finite() {
            return eframe::egui::Color32::GRAY;
        }

        let temperature = self.temperature.clamp(T_MIN, T_MAX);

        let hue = if temperature <= T_NOM {
            let t = (temperature - T_MIN) / (T_NOM - T_MIN);
            util::lerp(120.0, 45.0, t)
        } else {
            let t = (temperature - T_NOM) / (T_MAX - T_NOM);
            util::lerp(45.0, 0.0, t)
        };

        util::hsv_to_color32(hue, 1.0, 1.0)
    }
}

struct BmsTempTelemetry {
    min_cell_temp: f64,
    max_cell_temp: f64,
    avg_cell_temp: f64,
    max_board_temp: f64,
}

impl Default for BmsTempTelemetry {
    fn default() -> Self {
        Self {
            min_cell_temp: f64::NAN,
            max_cell_temp: f64::NAN,
            avg_cell_temp: f64::NAN,
            max_board_temp: f64::NAN,
        }
    }
}

pub struct BatteryTemps {
    pub title: String,
}

impl BatteryTemps {
    pub fn new(instance_num: usize) -> Self {
        Self {
            title: format!("Battery Temps #{}", instance_num),
        }
    }

    pub fn show(
        &self,
        ui: &mut eframe::egui::Ui,
        view: &telemetry::TelemetryView<'_>,
    ) -> egui_tiles::UiResponse {
        let (modules, therm_time) = thermistor_temperatures(view.setpoint_frames);
        let (bms_telemetry, bms_time) = bms_temps(view.setpoint_frames);
        let (stale, elapsed) =
            ui::battery::common::sample_age(therm_time.max(bms_time), view.view_time());

        let theme = ui::theme::get_theme(ui.ctx());

        let temp_min = bms_telemetry.as_ref().map(|t| t.min_cell_temp);
        let temp_avg = bms_telemetry.as_ref().map(|t| t.avg_cell_temp);
        let temp_max = bms_telemetry.as_ref().map(|t| t.max_cell_temp);
        let temp_range = if let (Some(min), Some(max)) = (temp_min, temp_max) {
            Some(max - min)
        } else {
            None
        };
        let board_max = bms_telemetry.as_ref().map(|t| t.max_board_temp);

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
                    "CELL MIN",
                    temp_min,
                    "°C",
                    stale,
                    None,
                );
                ui::battery::common::stat_card(
                    &mut cols[1],
                    &theme,
                    "CELL AVG",
                    temp_avg,
                    "°C",
                    stale,
                    None,
                );
                ui::battery::common::stat_card(
                    &mut cols[2],
                    &theme,
                    "CELL MAX",
                    temp_max,
                    "°C",
                    stale,
                    None,
                );
                ui::battery::common::stat_card(
                    &mut cols[3],
                    &theme,
                    "CELL RANGE",
                    temp_range,
                    "°C",
                    stale,
                    temp_range.map(|range| {
                        if range > 10.0 {
                            theme.error_color()
                        } else if range > 5.0 {
                            theme.warning_color()
                        } else {
                            theme.success_color()
                        }
                    }),
                );
                ui::battery::common::stat_card(
                    &mut cols[4],
                    &theme,
                    "BOARD MAX",
                    board_max,
                    "°C",
                    stale,
                    board_max.and_then(|max| {
                        if max > 60.0 {
                            Some(theme.error_color())
                        } else if max > 50.0 {
                            Some(theme.warning_color())
                        } else {
                            None
                        }
                    }),
                );
            });

            ui.add_space(12.0);

            for (module_index, module) in modules.iter().enumerate() {
                let (cell_therms, board_therms) =
                    module.split_at(ui::battery::common::CELL_THERMS_PER_MODULE);

                let module_sum: f64 = cell_therms
                    .iter()
                    .map(|cell| cell.temperature)
                    .filter(|v| v.is_finite())
                    .sum();
                let module_min = cell_therms
                    .iter()
                    .map(|cell| cell.temperature)
                    .filter(|v| v.is_finite())
                    .reduce(f64::min)
                    .unwrap_or(f64::NAN);
                let module_max = cell_therms
                    .iter()
                    .map(|cell| cell.temperature)
                    .filter(|v| v.is_finite())
                    .reduce(f64::max)
                    .unwrap_or(f64::NAN);
                let module_avg = module_sum
                    / cell_therms
                        .iter()
                        .filter(|cell| cell.temperature.is_finite())
                        .count() as f64;

                let module_board_max = board_therms
                    .iter()
                    .map(|cell| cell.temperature)
                    .filter(|v| v.is_finite())
                    .reduce(f64::max)
                    .unwrap_or(f64::NAN);

                eframe::egui::Frame::NONE
                    .fill(theme.panel_color())
                    .stroke(eframe::egui::Stroke::new(1.0_f32, theme.accent_color()))
                    .inner_margin(eframe::egui::Margin::same(10))
                    .corner_radius(eframe::egui::CornerRadius::same(4))
                    .show(ui, |ui| {
                        let available_width = ui.available_width();
                        let cell_spacing = ui.spacing().item_spacing.x;
                        let separator_gap = 10.0_f32;
                        let total_cols = (ui::battery::common::CELL_THERMS_PER_CHIP
                            + ui::battery::common::BOARD_THERMS_PER_CHIP)
                            as f32;
                        let bar_width = ((available_width
                            - separator_gap
                            - cell_spacing * total_cols)
                            / total_cols)
                            .max(8.0);

                        let header_resp = ui.horizontal(|ui| {
                            ui.label(
                                eframe::egui::RichText::new(format!("MODULE {module_index}"))
                                    .size(11.0)
                                    .strong(),
                            );
                            ui.add_space(8.0);
                            ui.label(
                                eframe::egui::RichText::new(format!(
                                    "avg {} °C",
                                    ui::battery::common::reading(module_avg, 1)
                                ))
                                .size(10.0)
                                .color(theme.text_color().linear_multiply(0.55)),
                            );
                            ui.label(
                                eframe::egui::RichText::new(format!(
                                    "min {} °C",
                                    ui::battery::common::reading(module_min, 1)
                                ))
                                .size(10.0)
                                .color(theme.text_color().linear_multiply(0.55)),
                            );
                            ui.label(
                                eframe::egui::RichText::new(format!(
                                    "max {} °C",
                                    ui::battery::common::reading(module_max, 1)
                                ))
                                .size(10.0)
                                .color(theme.text_color().linear_multiply(0.55)),
                            );
                            ui.label(
                                eframe::egui::RichText::new(format!(
                                    "board max {} °C",
                                    ui::battery::common::reading(module_board_max, 1)
                                ))
                                .size(10.0)
                                .color(theme.text_color().linear_multiply(0.55)),
                            );
                        });

                        ui.add_space(6.0);

                        let (chip0_cells, chip1_cells) =
                            cell_therms.split_at(ui::battery::common::CELL_THERMS_PER_CHIP);

                        let mut sep_top = None;
                        let mut sep_bottom = None;
                        let mut sep_x = 0.0_f32;
                        let mut board_center_x = 0.0_f32;

                        ui.horizontal(|ui| {
                            for cell in chip0_cells {
                                temp_bar(ui, &theme, cell, stale, bar_width);
                            }
                            let (sep_rect, _) = ui.allocate_exact_size(
                                eframe::egui::Vec2::new(separator_gap, 24.0),
                                eframe::egui::Sense::hover(),
                            );
                            sep_x = sep_rect.center().x;
                            sep_top = Some(sep_rect.top());
                            let board_rect =
                                temp_bar(ui, &theme, &board_therms[0], stale, bar_width);
                            board_center_x = board_rect.center().x;
                        });

                        ui.add_space(4.0);

                        ui.horizontal(|ui| {
                            for cell in chip1_cells {
                                temp_bar(ui, &theme, cell, stale, bar_width);
                            }
                            let (sep_rect, _) = ui.allocate_exact_size(
                                eframe::egui::Vec2::new(separator_gap, 24.0),
                                eframe::egui::Sense::hover(),
                            );
                            sep_bottom = Some(sep_rect.bottom());
                            temp_bar(ui, &theme, &board_therms[1], stale, bar_width);
                        });

                        ui.painter().text(
                            eframe::egui::pos2(board_center_x, header_resp.response.rect.center().y),
                            eframe::egui::Align2::CENTER_CENTER,
                            "BOARD",
                            eframe::egui::FontId::proportional(9.0),
                            theme.text_color().linear_multiply(0.45),
                        );

                        if let (Some(top), Some(bottom)) = (sep_top, sep_bottom) {
                            ui.painter().line_segment(
                                [
                                    eframe::egui::pos2(sep_x, top),
                                    eframe::egui::pos2(sep_x, bottom),
                                ],
                                eframe::egui::Stroke::new(1.0_f32, theme.accent_color()),
                            );
                        }
                    });

                ui.add_space(6.0);
            }
        });

        egui_tiles::UiResponse::None
    }
}

fn temp_bar(
    ui: &mut eframe::egui::Ui,
    theme: &ui::theme::ThemeColors,
    cell: &ThermistorTemperature,
    stale: bool,
    bar_w: f32,
) -> eframe::egui::Rect {
    let fill_color = if stale {
        theme.text_color().linear_multiply(0.12)
    } else {
        cell.color()
    };

    let fill_frac = if cell.temperature.is_finite() {
        ((cell.temperature - T_MIN) / (T_MAX - T_MIN)).clamp(0.0, 1.0) as f32
    } else {
        0.0
    };

    ui.vertical(|ui| {
        ui.set_max_width(bar_w + 4.0);

        let (outer_rect, _) = ui.allocate_exact_size(
            eframe::egui::Vec2::new(bar_w, 24.0),
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

        let text = if stale || !cell.temperature.is_finite() {
            "—".to_string()
        } else {
            format!("{:.1}°C", cell.temperature)
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
            eframe::egui::FontId::proportional(11.0),
            text_color,
        );

        outer_rect
    })
    .inner
}

fn thermistor_sample(
    frame: &daqcore::frame::ParsedFrame,
) -> Option<(usize, usize, ThermistorTemperature)> {
    let decoded = frame.decoded.as_ref()?;
    if !matches!(
        decoded.name(),
        "thermistor_telemetry" | "thermistor_telemetry_ccan"
    ) {
        return None;
    }

    let module = decoded.signal("module_num")?.physical().round() as usize;
    let thermistor = decoded.signal("thermistor_num")?.physical().round() as usize;
    let temperature = decoded.signal("temperature")?.physical();
    let value = ThermistorTemperature { temperature };

    Some((module, thermistor, value))
}

fn thermistor_temperatures(
    frames: &[daqcore::frame::ParsedFrame],
) -> (Vec<Vec<ThermistorTemperature>>, Option<daqcore::time::Time>) {
    let mut modules =
        vec![
            vec![ThermistorTemperature::default(); ui::battery::common::THERMISTORS_PER_MODULE];
            ui::battery::common::NUM_MODULES
        ];
    let mut last_update = None;
    let mut filled = 0;
    for frame in frames.iter().rev() {
        if let Some((module, thermistor, value)) = thermistor_sample(frame)
            && module < ui::battery::common::NUM_MODULES
            && thermistor < ui::battery::common::THERMISTORS_PER_MODULE
            && modules[module][thermistor].temperature.is_nan()
        {
            modules[module][thermistor] = value;
            last_update = last_update.max(Some(frame.timestamp));
            filled += 1;
            if filled
                == ui::battery::common::NUM_MODULES * ui::battery::common::THERMISTORS_PER_MODULE
            {
                break;
            }
        }
    }
    (modules, last_update)
}

fn bms_temps(
    frames: &[daqcore::frame::ParsedFrame],
) -> (Option<BmsTempTelemetry>, Option<daqcore::time::Time>) {
    let mut values: Option<BmsTempTelemetry> = None;
    let mut last_update = None;
    for frame in frames.iter().rev() {
        let Some(decoded) = &frame.decoded else {
            continue;
        };

        if matches!(decoded.name(), "bms_temps" | "bms_temps_ccan") {
            for (name, signal) in decoded.iter() {
                let field = match name {
                    "min_cell_temp" => &mut values.get_or_insert_default().min_cell_temp,
                    "max_cell_temp" => &mut values.get_or_insert_default().max_cell_temp,
                    "avg_cell_temp" => &mut values.get_or_insert_default().avg_cell_temp,
                    "max_board_temp" => &mut values.get_or_insert_default().max_board_temp,
                    _ => continue,
                };

                if field.is_nan() {
                    *field = signal.physical();
                }
            }
            last_update = last_update.max(Some(frame.timestamp));
        }
    }
    (values, last_update)
}
