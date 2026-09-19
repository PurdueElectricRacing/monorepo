use super::common::{self, BatteryUiState};
use crate::{messages, ui, util};
use eframe::egui::{self, Color32, Frame, RichText, Stroke};

const T_MIN: f64 = 15.0;
const T_MAX: f64 = 45.0;
const T_NOM: f64 = 25.0;

#[derive(Default, Clone)]
pub struct ThermistorTemperature {
    pub temperature: f64,
}

impl ThermistorTemperature {
    pub fn color(&self) -> Color32 {
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
    modules: Vec<Vec<ThermistorTemperature>>,
    ui_state: BatteryUiState,
}

impl BatteryTemps {
    pub fn new(instance_num: usize) -> Self {
        Self {
            title: format!("Battery Temps #{}", instance_num),
            modules: vec![
                vec![ThermistorTemperature::default(); common::THERMISTORS_PER_MODULE];
                common::NUM_MODULES
            ],
            ui_state: BatteryUiState::new(),
        }
    }

    pub fn handle_can_message(&mut self, msg: &messages::MsgFromCan) {
        if let messages::MsgFromCan::ParsedMessage(parsed) = msg
            && (parsed.decoded.name.as_str() == "thermistor_telemetry_ccan"
                || parsed.decoded.name.as_str() == "thermistor_telemetry")
        {
            let mut module_num: Option<usize> = None;
            let mut thermistor_num: Option<usize> = None;
            let mut temperature: Option<f64> = None;

            for (_, sig) in parsed.decoded.signals.iter() {
                match sig.name.as_str() {
                    "module_num" => module_num = Some(sig.value.physical.round() as usize),
                    "thermistor_num" => thermistor_num = Some(sig.value.physical.round() as usize),
                    "temperature" => temperature = Some(sig.value.physical),
                    _ => {}
                }
            }

            if let (Some(module_num), Some(thermistor_num), Some(temperature)) =
                (module_num, thermistor_num, temperature)
                && module_num < self.modules.len()
                && thermistor_num < self.modules[module_num].len()
            {
                self.modules[module_num][thermistor_num].temperature = temperature;
                self.ui_state.mark_updated();
            }
        }
    }

    pub fn show(&mut self, ui: &mut egui::Ui) -> egui_tiles::UiResponse {
        let theme = ui::theme::get_theme(ui.ctx());
        let (stale, elapsed) = self.ui_state.refresh();

        let temperatures = self.modules.iter().flatten().map(|cell| cell.temperature);
        let temp_min = temperatures.clone().fold(f64::MAX, f64::min);
        let temp_max = temperatures.clone().fold(f64::MIN, f64::max);
        let temp_sum: f64 = temperatures.sum();
        let temp_count = (self.modules.len() * self.modules[0].len()) as f64;
        let temp_avg = temp_sum / temp_count;
        let temp_range = temp_max - temp_min;

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
                    Some(if temp_range > 10.0 {
                        theme.error_color()
                    } else if temp_range > 5.0 {
                        theme.warning_color()
                    } else {
                        theme.success_color()
                    }),
                );
            });

            ui.add_space(12.0);

            for (module_index, module) in self.modules.iter().enumerate() {
                let module_sum: f64 = module.iter().map(|cell| cell.temperature).sum();
                let module_min = module
                    .iter()
                    .map(|cell| cell.temperature)
                    .fold(f64::MAX, f64::min);
                let module_max = module
                    .iter()
                    .map(|cell| cell.temperature)
                    .fold(f64::MIN, f64::max);
                let module_avg = module_sum / module.len() as f64;

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
                                RichText::new(format!("avg {:.1} °C", module_avg))
                                    .size(10.0)
                                    .color(theme.text_color().linear_multiply(0.55)),
                            );
                            ui.label(
                                RichText::new(format!("min {:.1} °C", module_min))
                                    .size(10.0)
                                    .color(theme.text_color().linear_multiply(0.55)),
                            );
                            ui.label(
                                RichText::new(format!("max {:.1} °C", module_max))
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
                                Self::temp_bar(ui, &theme, cell, stale, bar_width);
                            }
                        });
                    });

                ui.add_space(6.0);
            }
        });

        egui_tiles::UiResponse::None
    }

    fn temp_bar(
        ui: &mut egui::Ui,
        theme: &ui::theme::ThemeColors,
        cell: &ThermistorTemperature,
        stale: bool,
        bar_w: f32,
    ) {
        use egui::{Align2, FontId};

        let fill_color = if stale {
            theme.text_color().linear_multiply(0.12)
        } else {
            cell.color()
        };

        let fill_frac = ((cell.temperature - T_MIN) / (T_MAX - T_MIN)).clamp(0.0, 1.0) as f32;

        ui.vertical(|ui| {
            ui.set_max_width(bar_w + 4.0);

            let (outer_rect, _) =
                ui.allocate_exact_size(egui::Vec2::new(bar_w, 24.0), egui::Sense::hover());

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
                format!("{:.1}°C", cell.temperature)
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
                FontId::proportional(11.0),
                text_color,
            );
        });
    }
}
