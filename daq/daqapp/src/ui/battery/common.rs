use crate::ui::theme::ThemeColors;
use eframe::egui::{self, Color32, Frame, RichText, Stroke};

pub const NUM_MODULES: usize = 7;
pub const CELLS_PER_MODULE: usize = 16;
pub const THERMISTORS_PER_MODULE: usize = 10;

pub const STALE_TIMEOUT_SECONDS: u64 = 1;

pub struct BatteryUiState {
    last_update: Option<daqcore::Time>,
    view_time: daqcore::Time,
}
impl BatteryUiState {
    pub fn new() -> Self {
        Self {
            last_update: None,
            view_time: daqcore::Time::now(),
        }
    }
    pub fn mark_updated(&mut self, timestamp: daqcore::Time) {
        self.last_update = Some(self.last_update.map_or(timestamp, |old| old.max(timestamp)));
    }
    pub fn set_view_time(&mut self, time: daqcore::Time) {
        self.view_time = time;
    }
    pub fn refresh(&mut self) -> (bool, f64) {
        let elapsed = self
            .last_update
            .map(|t| self.view_time.secs(t).max(0.0))
            .unwrap_or(f64::INFINITY);
        (elapsed > STALE_TIMEOUT_SECONDS as f64, elapsed)
    }
}

pub fn stale_banner(ui: &mut egui::Ui, theme: &ThemeColors, stale: bool, elapsed: f64) {
    let (bg, dot, text) = if stale {
        let c = theme.warning_color();
        (
            Color32::from_rgba_unmultiplied(c.r(), c.g(), c.b(), 30),
            c,
            format!("No data — last message {:.1} s ago", elapsed),
        )
    } else {
        let c = theme.success_color();
        (
            Color32::from_rgba_unmultiplied(c.r(), c.g(), c.b(), 30),
            c,
            format!("Live — last message {:.1} s ago", elapsed),
        )
    };

    Frame::NONE
        .fill(bg)
        .stroke(Stroke::new(1.0_f32, dot.linear_multiply(0.5)))
        .inner_margin(egui::Margin::symmetric(10, 6))
        .corner_radius(egui::CornerRadius::same(4))
        .show(ui, |ui| {
            ui.horizontal(|ui| {
                let (rect, _) =
                    ui.allocate_exact_size(egui::Vec2::splat(8.0), egui::Sense::hover());
                ui.painter().circle_filled(rect.center(), 4.0, dot);
                ui.add_space(4.0);
                ui.colored_label(dot, &text);
            });
        });
}

pub fn stat_card(
    ui: &mut egui::Ui,
    theme: &ThemeColors,
    label: &str,
    value: Option<f64>,
    unit: &str,
    stale: bool,
    override_color: Option<Color32>,
) {
    Frame::NONE
        .fill(theme.panel_color())
        .stroke(Stroke::new(1.0_f32, theme.accent_color()))
        .inner_margin(egui::Margin::same(10))
        .corner_radius(egui::CornerRadius::same(4))
        .show(ui, |ui| {
            ui.set_min_width(ui.available_width());
            ui.vertical(|ui| {
                ui.label(
                    RichText::new(label)
                        .size(10.0)
                        .color(theme.text_color().linear_multiply(0.5)),
                );
                ui.add_space(2.0);
                let val_color = override_color.unwrap_or(theme.info_color());
                if stale {
                    ui.label(
                        RichText::new("—")
                            .size(20.0)
                            .color(theme.text_color().linear_multiply(0.25)),
                    );
                } else {
                    ui.label(
                        RichText::new(if let Some(v) = value.filter(|v| v.is_finite()) {
                            format!("{:.2}", v)
                        } else {
                            "—".to_string()
                        })
                        .size(20.0)
                        .color(val_color),
                    );
                    ui.label(
                        RichText::new(unit)
                            .size(10.0)
                            .color(theme.text_color().linear_multiply(0.4)),
                    );
                }
            });
        });
}

/// Keep unavailable projection values distinct from a measured zero.
pub fn reading(value: f64, precision: usize) -> String {
    if value.is_finite() {
        format!("{value:.precision$}")
    } else {
        "—".into()
    }
}
