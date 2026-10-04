use crate::{
    telemetry,
    ui::{self, battery::common},
    util,
};

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
        let (modules, last_update) = thermistor_temperatures(view.setpoint_frames);
        let (stale, elapsed) = common::sample_age(last_update, view.view_time());

        let theme = ui::theme::get_theme(ui.ctx());

        let temperatures = modules
            .iter()
            .flatten()
            .map(|cell| cell.temperature)
            .filter(|v| v.is_finite());
        let temp_min = temperatures.clone().reduce(f64::min).unwrap_or(f64::NAN);
        let temp_max = temperatures.clone().reduce(f64::max).unwrap_or(f64::NAN);
        let temp_count = temperatures.clone().count().max(1) as f64;
        let temp_sum: f64 = temperatures.sum();
        let temp_avg = temp_sum / temp_count;
        let temp_range = temp_max - temp_min;
        let range_color = if temp_range > 10.0 {
            theme.error_color()
        } else if temp_range > 5.0 {
            theme.warning_color()
        } else {
            theme.success_color()
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

            ui.columns(4, |cols| {
                common::stat_card(
                    &mut cols[0],
                    &theme,
                    "TEMP MIN",
                    Some(temp_min),
                    "°C",
                    stale,
                    None,
                );
                common::stat_card(
                    &mut cols[1],
                    &theme,
                    "TEMP AVG",
                    Some(temp_avg),
                    "°C",
                    stale,
                    None,
                );
                common::stat_card(
                    &mut cols[2],
                    &theme,
                    "TEMP MAX",
                    Some(temp_max),
                    "°C",
                    stale,
                    None,
                );
                common::stat_card(
                    &mut cols[3],
                    &theme,
                    "TEMP RANGE",
                    Some(temp_range),
                    "°C",
                    stale,
                    Some(range_color),
                );
            });

            ui.add_space(12.0);

            for (module_index, module) in modules.iter().enumerate() {
                let module_sum: f64 = module
                    .iter()
                    .map(|cell| cell.temperature)
                    .filter(|v| v.is_finite())
                    .sum();
                let module_min = module
                    .iter()
                    .map(|cell| cell.temperature)
                    .filter(|v| v.is_finite())
                    .reduce(f64::min)
                    .unwrap_or(f64::NAN);
                let module_max = module
                    .iter()
                    .map(|cell| cell.temperature)
                    .filter(|v| v.is_finite())
                    .reduce(f64::max)
                    .unwrap_or(f64::NAN);
                let module_avg = module_sum
                    / module
                        .iter()
                        .filter(|cell| cell.temperature.is_finite())
                        .count() as f64;

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
                                    "avg {} °C",
                                    common::reading(module_avg, 1)
                                ))
                                .size(10.0)
                                .color(theme.text_color().linear_multiply(0.55)),
                            );
                            ui.label(
                                eframe::egui::RichText::new(format!(
                                    "min {} °C",
                                    common::reading(module_min, 1)
                                ))
                                .size(10.0)
                                .color(theme.text_color().linear_multiply(0.55)),
                            );
                            ui.label(
                                eframe::egui::RichText::new(format!(
                                    "max {} °C",
                                    common::reading(module_max, 1)
                                ))
                                .size(10.0)
                                .color(theme.text_color().linear_multiply(0.55)),
                            );
                        });

                        ui.add_space(6.0);

                        let available_width = ui.available_width();
                        let cell_spacing = ui.spacing().item_spacing.x;
                        let cell_count = common::THERMISTORS_PER_MODULE as f32;
                        let bar_width =
                            ((available_width - cell_spacing * cell_count) / cell_count).max(8.0);

                        ui.horizontal(|ui| {
                            for cell in module.iter() {
                                temp_bar(ui, &theme, cell, stale, bar_width);
                            }
                        });
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
) {
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
    });
}

fn thermistor_sample(
    frame: &daqcore::ParsedFrame,
) -> Option<(usize, usize, ThermistorTemperature)> {
    let decoded = frame.decoded.as_ref()?;
    if !matches!(
        decoded.name.as_str(),
        "thermistor_telemetry" | "thermistor_telemetry_ccan"
    ) {
        return None;
    }

    let module = decoded.signals.get("module_num")?.value.physical.round() as usize;
    let thermistor = decoded
        .signals
        .get("thermistor_num")?
        .value
        .physical
        .round() as usize;
    let temperature = decoded.signals.get("temperature")?.value.physical;
    let value = ThermistorTemperature { temperature };

    Some((module, thermistor, value))
}

fn thermistor_temperatures(
    frames: &[daqcore::ParsedFrame],
) -> (Vec<Vec<ThermistorTemperature>>, Option<daqcore::Time>) {
    let mut modules = vec![
        vec![ThermistorTemperature::default(); common::THERMISTORS_PER_MODULE];
        common::NUM_MODULES
    ];
    let mut last_update = None;
    let mut filled = 0;
    for frame in frames.iter().rev() {
        if let Some((module, thermistor, value)) = thermistor_sample(frame)
            && module < common::NUM_MODULES
            && thermistor < common::THERMISTORS_PER_MODULE
            && modules[module][thermistor].temperature.is_nan()
        {
            modules[module][thermistor] = value;
            last_update = last_update.max(Some(frame.timestamp));
            filled += 1;
            if filled == common::NUM_MODULES * common::THERMISTORS_PER_MODULE {
                break;
            }
        }
    }
    (modules, last_update)
}
