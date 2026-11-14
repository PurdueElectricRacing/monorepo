use crate::{telemetry, ui, util};

const T_MIN: f64 = 15.0;
const T_MAX: f64 = 45.0;
const T_NOM: f64 = 25.0;

const T_RANGE_WARN: f64 = 5.0;
const T_RANGE_ERROR: f64 = 10.0;

const T_BOARD_WARN: f64 = 50.0;
const T_BOARD_ERROR: f64 = 60.0;

#[derive(Clone, Copy)]
pub struct ThermistorTemp {
    pub temperature: f64,
}

impl ThermistorTemp {
    pub fn color(&self) -> eframe::egui::Color32 {
        let temperature = self.temperature.clamp(T_MIN, T_MAX);

        let hue = if temperature <= T_NOM {
            let t = (temperature - T_MIN) / (T_NOM - T_MIN);
            util::lerp(
                ui::battery::common::HUE_GREEN,
                ui::battery::common::HUE_AMBER,
                t,
            )
        } else {
            let t = (temperature - T_NOM) / (T_MAX - T_NOM);
            util::lerp(
                ui::battery::common::HUE_AMBER,
                ui::battery::common::HUE_RED,
                t,
            )
        };

        util::hsv_to_color32(hue, 1.0, 1.0)
    }
}

#[derive(Clone, Copy, Default)]
struct ModuleTemps {
    cell_temps: [Option<ThermistorTemp>; ui::battery::common::CELL_THERMS_PER_MODULE],
    board_temps: [Option<ThermistorTemp>; ui::battery::common::BOARD_THERMS_PER_MODULE],
}

#[derive(Default)]
struct BmsTempSummary {
    min_cell_temp: Option<f64>,
    max_cell_temp: Option<f64>,
    avg_cell_temp: Option<f64>,
    max_board_temp: Option<f64>,
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
        let (modules, therm_time) = module_temperatures(view.setpoint_frames);
        let (bms_summary, summary_time) = bms_temp_summary(view.setpoint_frames);
        let (stale, elapsed) =
            ui::battery::common::sample_age(therm_time.max(summary_time), view.view_time());

        let theme = ui::theme::get_theme(ui.ctx());

        let temp_min = bms_summary.min_cell_temp;
        let temp_avg = bms_summary.avg_cell_temp;
        let temp_max = bms_summary.max_cell_temp;
        let temp_range = temp_min.zip(temp_max).map(|(min, max)| max - min);
        let board_max = bms_summary.max_board_temp;

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
                        if range > T_RANGE_ERROR {
                            theme.error_color()
                        } else if range > T_RANGE_WARN {
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
                        if max > T_BOARD_ERROR {
                            Some(theme.error_color())
                        } else if max > T_BOARD_WARN {
                            Some(theme.warning_color())
                        } else {
                            None
                        }
                    }),
                );
            });

            ui.add_space(12.0);

            for (module_index, module) in modules.iter().enumerate() {
                let cell_temps = module
                    .cell_temps
                    .iter()
                    .flatten()
                    .map(|therm| therm.temperature);
                let module_min = cell_temps.clone().reduce(f64::min);
                let module_max = cell_temps.clone().reduce(f64::max);
                let cell_count = cell_temps.clone().count();
                let module_avg =
                    (cell_count > 0).then(|| cell_temps.sum::<f64>() / cell_count as f64);

                let module_board_max = module
                    .board_temps
                    .iter()
                    .flatten()
                    .map(|therm| therm.temperature)
                    .reduce(f64::max);

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

                        let (chip0_therms, chip1_therms) = module
                            .cell_temps
                            .split_at(ui::battery::common::CELL_THERMS_PER_CHIP);

                        let mut sep_top = None;
                        let mut sep_bottom = None;
                        let mut sep_x = 0.0_f32;
                        let mut board_center_x = 0.0_f32;

                        ui.horizontal(|ui| {
                            for therm in chip0_therms {
                                temp_bar(ui, &theme, therm.as_ref(), stale, bar_width);
                            }
                            let (sep_rect, _) = ui.allocate_exact_size(
                                eframe::egui::Vec2::new(separator_gap, 24.0),
                                eframe::egui::Sense::hover(),
                            );
                            sep_x = sep_rect.center().x;
                            sep_top = Some(sep_rect.top());
                            let board_rect = temp_bar(
                                ui,
                                &theme,
                                module.board_temps[0].as_ref(),
                                stale,
                                bar_width,
                            );
                            board_center_x = board_rect.center().x;
                        });

                        ui.add_space(4.0);

                        ui.horizontal(|ui| {
                            for therm in chip1_therms {
                                temp_bar(ui, &theme, therm.as_ref(), stale, bar_width);
                            }
                            let (sep_rect, _) = ui.allocate_exact_size(
                                eframe::egui::Vec2::new(separator_gap, 24.0),
                                eframe::egui::Sense::hover(),
                            );
                            sep_bottom = Some(sep_rect.bottom());
                            temp_bar(
                                ui,
                                &theme,
                                module.board_temps[1].as_ref(),
                                stale,
                                bar_width,
                            );
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
    therm: Option<&ThermistorTemp>,
    stale: bool,
    bar_w: f32,
) -> eframe::egui::Rect {
    let fill_color = if stale {
        theme.text_color().linear_multiply(0.12)
    } else if let Some(therm) = therm {
        therm.color()
    } else {
        eframe::egui::Color32::GRAY
    };

    let fill_frac = if let Some(therm) = therm {
        ((therm.temperature - T_MIN) / (T_MAX - T_MIN)).clamp(0.0, 1.0) as f32
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

        let text = match therm.filter(|_| !stale) {
            Some(therm) => format!("{:.1}°C", therm.temperature),
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
            eframe::egui::FontId::proportional(11.0),
            text_color,
        );

        outer_rect
    })
    .inner
}

fn thermistor_sample(
    frame: &daqcore::frame::ParsedFrame,
) -> Option<(usize, usize, ThermistorTemp)> {
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
    let value = ThermistorTemp { temperature };

    Some((module, thermistor, value))
}

fn module_temperatures(
    frames: &[daqcore::frame::ParsedFrame],
) -> (
    [ModuleTemps; ui::battery::common::NUM_MODULES],
    Option<daqcore::time::Time>,
) {
    let mut modules = [ModuleTemps::default(); ui::battery::common::NUM_MODULES];
    let mut last_update = None;
    let mut filled = 0;
    for frame in frames.iter().rev() {
        if let Some((module, thermistor, value)) = thermistor_sample(frame)
            && module < ui::battery::common::NUM_MODULES
            && thermistor < ui::battery::common::THERMISTORS_PER_MODULE
        {
            let slot = if thermistor < ui::battery::common::CELL_THERMS_PER_MODULE {
                &mut modules[module].cell_temps[thermistor]
            } else {
                &mut modules[module].board_temps
                    [thermistor - ui::battery::common::CELL_THERMS_PER_MODULE]
            };

            if slot.is_none() {
                *slot = Some(value);
                last_update = last_update.max(Some(frame.timestamp));
                filled += 1;
                if filled
                    == ui::battery::common::NUM_MODULES
                        * ui::battery::common::THERMISTORS_PER_MODULE
                {
                    break;
                }
            }
        }
    }
    (modules, last_update)
}

fn bms_temp_summary(
    frames: &[daqcore::frame::ParsedFrame],
) -> (BmsTempSummary, Option<daqcore::time::Time>) {
    let mut values = BmsTempSummary::default();
    let mut last_update = None;
    for frame in frames.iter().rev() {
        let Some(decoded) = &frame.decoded else {
            continue;
        };

        if matches!(decoded.name(), "bms_temps" | "bms_temps_ccan") {
            for (name, signal) in decoded.iter() {
                let field = match name {
                    "min_cell_temp" => &mut values.min_cell_temp,
                    "max_cell_temp" => &mut values.max_cell_temp,
                    "avg_cell_temp" => &mut values.avg_cell_temp,
                    "max_board_temp" => &mut values.max_board_temp,
                    _ => continue,
                };

                if field.is_none() {
                    *field = Some(signal.physical());
                }
            }
            last_update = last_update.max(Some(frame.timestamp));
        }
    }
    (values, last_update)
}
