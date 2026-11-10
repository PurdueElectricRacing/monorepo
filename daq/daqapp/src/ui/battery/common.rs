use crate::ui;

pub const NUM_MODULES: usize = 5;
pub const CELLS_PER_MODULE: usize = 29;
pub const THERMISTORS_PER_MODULE: usize = 20;

pub const STALE_TIMEOUT_SECONDS: u64 = 1;

pub fn sample_age(
    last_update: Option<daqcore::time::Time>,
    view_time: daqcore::time::Time,
) -> (bool, f64) {
    let elapsed = last_update
        .map(|t| view_time.secs(t).max(0.0))
        .unwrap_or(f64::INFINITY);
    (elapsed > STALE_TIMEOUT_SECONDS as f64, elapsed)
}

pub fn stale_banner(ui: &mut eframe::egui::Ui, stale: bool, elapsed: f64) {
    let (bg, dot, text) = if stale {
        (
            ui::colors::MUTED_YELLOW,
            ui::colors::YELLOW,
            format!("No data — last message {:.1} s ago", elapsed),
        )
    } else {
        (
            ui::colors::MUTED_GREEN,
            ui::colors::GREEN,
            format!("Live — last message {:.1} s ago", elapsed),
        )
    };

    eframe::egui::Frame::NONE
        .fill(bg)
        .stroke(eframe::egui::Stroke::new(1.0_f32, dot))
        .inner_margin(eframe::egui::Margin::symmetric(10, 6))
        .corner_radius(eframe::egui::CornerRadius::same(4))
        .show(ui, |ui| {
            ui.horizontal(|ui| {
                let (rect, _) = ui.allocate_exact_size(
                    eframe::egui::Vec2::splat(8.0),
                    eframe::egui::Sense::hover(),
                );
                ui.painter().circle_filled(rect.center(), 4.0, dot);
                ui.add_space(4.0);
                ui.colored_label(dot, &text);
            });
        });
}

pub fn stat_card(
    ui: &mut eframe::egui::Ui,
    label: &str,
    value: Option<f64>,
    unit: &str,
    stale: bool,
    override_color: Option<eframe::egui::Color32>,
) {
    eframe::egui::Frame::NONE
        .fill(ui.visuals().faint_bg_color)
        .stroke(ui.visuals().widgets.noninteractive.bg_stroke)
        .inner_margin(eframe::egui::Margin::same(10))
        .corner_radius(eframe::egui::CornerRadius::same(4))
        .show(ui, |ui| {
            ui.set_min_width(ui.available_width());
            ui.vertical(|ui| {
                ui.label(
                    eframe::egui::RichText::new(label)
                        .size(10.0)
                        .color(ui::colors::DARK_GRAY),
                );
                ui.add_space(2.0);
                let val_color = override_color.unwrap_or(ui::colors::BLUE);
                if stale {
                    ui.label(
                        eframe::egui::RichText::new("—")
                            .size(20.0)
                            .color(ui::colors::DARK_GRAY),
                    );
                } else {
                    let text = if let Some(value) = value.filter(|value| value.is_finite()) {
                        format!("{value:.2}")
                    } else {
                        "—".to_string()
                    };

                    ui.label(
                        eframe::egui::RichText::new(text)
                            .size(20.0)
                            .color(val_color),
                    );
                    ui.label(
                        eframe::egui::RichText::new(unit)
                            .size(10.0)
                            .color(ui::colors::DARK_GRAY),
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
