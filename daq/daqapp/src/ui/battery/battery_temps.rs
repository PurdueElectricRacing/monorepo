use crate::{telemetry, ui};

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
            return ui::colors::DARK_GRAY;
        }

        let temperature = self.temperature.clamp(T_MIN, T_MAX);

        if temperature <= T_NOM {
            let t = (temperature - T_MIN) / (T_NOM - T_MIN);
            ui::colors::lerp_color(ui::colors::GREEN, ui::colors::YELLOW, t)
        } else {
            let t = (temperature - T_NOM) / (T_MAX - T_NOM);
            ui::colors::lerp_color(ui::colors::YELLOW, ui::colors::RED, t)
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
        let (modules, last_update) = thermistor_temperatures(view.setpoint_frames);
        let (stale, elapsed) = ui::battery::common::sample_age(last_update, view.view_time());

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
            ui::colors::RED
        } else if temp_range > 5.0 {
            ui::colors::YELLOW
        } else {
            ui::colors::GREEN
        };

        eframe::egui::ScrollArea::vertical().show(ui, |ui| {
            ui.add_space(4.0);
            ui.heading(&self.title);
            ui.add_space(4.0);

            ui::battery::common::stale_banner(ui, stale, elapsed);

            ui.add_space(8.0);
            ui.label(
                eframe::egui::RichText::new("PACK SUMMARY")
                    .size(10.0)
                    .color(ui::colors::DARK_GRAY),
            );
            ui.add_space(4.0);

            ui.columns(4, |cols| {
                ui::battery::common::stat_card(
                    &mut cols[0],
                    "TEMP MIN",
                    Some(temp_min),
                    "°C",
                    stale,
                    None,
                );
                ui::battery::common::stat_card(
                    &mut cols[1],
                    "TEMP AVG",
                    Some(temp_avg),
                    "°C",
                    stale,
                    None,
                );
                ui::battery::common::stat_card(
                    &mut cols[2],
                    "TEMP MAX",
                    Some(temp_max),
                    "°C",
                    stale,
                    None,
                );
                ui::battery::common::stat_card(
                    &mut cols[3],
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
                    .fill(ui.visuals().faint_bg_color)
                    .stroke(ui.visuals().widgets.noninteractive.bg_stroke)
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
                                    ui::battery::common::reading(module_avg, 1)
                                ))
                                .size(10.0)
                                .color(ui::colors::DARK_GRAY),
                            );
                            ui.label(
                                eframe::egui::RichText::new(format!(
                                    "min {} °C",
                                    ui::battery::common::reading(module_min, 1)
                                ))
                                .size(10.0)
                                .color(ui::colors::DARK_GRAY),
                            );
                            ui.label(
                                eframe::egui::RichText::new(format!(
                                    "max {} °C",
                                    ui::battery::common::reading(module_max, 1)
                                ))
                                .size(10.0)
                                .color(ui::colors::DARK_GRAY),
                            );
                        });

                        ui.add_space(6.0);

                        let available_width = ui.available_width();
                        let cell_spacing = ui.spacing().item_spacing.x;
                        let cell_count = ui::battery::common::THERMISTORS_PER_MODULE as f32;
                        let bar_width =
                            ((available_width - cell_spacing * cell_count) / cell_count).max(8.0);

                        ui.horizontal(|ui| {
                            for cell in module.iter() {
                                temp_bar(ui, cell, stale, bar_width);
                            }
                        });
                    });

                ui.add_space(6.0);
            }
        });

        egui_tiles::UiResponse::None
    }
}

fn temp_bar(ui: &mut eframe::egui::Ui, cell: &ThermistorTemperature, stale: bool, bar_w: f32) {
    let fill_color = if stale {
        ui::colors::DARK_GRAY
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
        painter.rect_filled(outer_rect, 3.0, ui.visuals().extreme_bg_color);
        painter.rect_stroke(
            outer_rect,
            3.0,
            ui.visuals().widgets.noninteractive.bg_stroke,
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
            ui::colors::DARK_GRAY
        } else if fill_frac > 0.5 {
            ui::colors::BLACK
        } else {
            ui::colors::WHITE
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
