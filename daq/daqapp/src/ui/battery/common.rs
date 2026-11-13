use crate::ui;

pub const NUM_MODULES: usize = 5;
pub const CHIPS_PER_MODULE: usize = 2;

pub const CELLS_CHIP_0: usize = 16;
pub const CELLS_CHIP_1: usize = 13;
pub const CELLS_PER_MODULE: usize = CELLS_CHIP_0 + CELLS_CHIP_1;

pub const CELL_THERMS_PER_CHIP: usize = 9;
pub const BOARD_THERMS_PER_CHIP: usize = 1;
pub const CELL_THERMS_PER_MODULE: usize = CHIPS_PER_MODULE * CELL_THERMS_PER_CHIP;
pub const BOARD_THERMS_PER_MODULE: usize = CHIPS_PER_MODULE * BOARD_THERMS_PER_CHIP;
pub const THERMISTORS_PER_MODULE: usize = CELL_THERMS_PER_MODULE + BOARD_THERMS_PER_MODULE;

pub const STALE_TIMEOUT_SECONDS: u64 = 1;

pub const HUE_RED: f64 = 0.0;
pub const HUE_AMBER: f64 = 45.0;
pub const HUE_GREEN: f64 = 120.0;

pub fn sample_age(
    last_update: Option<daqcore::time::Time>,
    view_time: daqcore::time::Time,
) -> (bool, f64) {
    let elapsed = last_update
        .map(|t| view_time.secs(t).max(0.0))
        .unwrap_or(f64::INFINITY);
    (elapsed > STALE_TIMEOUT_SECONDS as f64, elapsed)
}

pub fn stale_banner(
    ui: &mut eframe::egui::Ui,
    theme: &ui::theme::ThemeColors,
    stale: bool,
    elapsed: f64,
) {
    let (bg, dot, text) = if stale {
        let c = theme.warning_color();
        (
            eframe::egui::Color32::from_rgba_unmultiplied(c.r(), c.g(), c.b(), 30),
            c,
            format!("No data — last message {:.1} s ago", elapsed),
        )
    } else {
        let c = theme.success_color();
        (
            eframe::egui::Color32::from_rgba_unmultiplied(c.r(), c.g(), c.b(), 30),
            c,
            format!("Live — last message {:.1} s ago", elapsed),
        )
    };

    eframe::egui::Frame::NONE
        .fill(bg)
        .stroke(eframe::egui::Stroke::new(1.0_f32, dot.linear_multiply(0.5)))
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
    theme: &ui::theme::ThemeColors,
    label: &str,
    value: Option<f64>,
    unit: &str,
    stale: bool,
    override_color: Option<eframe::egui::Color32>,
) {
    eframe::egui::Frame::NONE
        .fill(theme.panel_color())
        .stroke(eframe::egui::Stroke::new(1.0_f32, theme.accent_color()))
        .inner_margin(eframe::egui::Margin::same(10))
        .corner_radius(eframe::egui::CornerRadius::same(4))
        .show(ui, |ui| {
            ui.set_min_width(ui.available_width());
            ui.vertical(|ui| {
                ui.label(
                    eframe::egui::RichText::new(label)
                        .size(10.0)
                        .color(theme.text_color().linear_multiply(0.5)),
                );
                ui.add_space(2.0);
                let val_color = override_color.unwrap_or(theme.info_color());
                if stale {
                    ui.label(
                        eframe::egui::RichText::new("—")
                            .size(20.0)
                            .color(theme.text_color().linear_multiply(0.25)),
                    );
                } else {
                    let text = if let Some(value) = value {
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
                            .color(theme.text_color().linear_multiply(0.4)),
                    );
                }
            });
        });
}

/// Keep unavailable projection values distinct from a measured zero.
pub fn reading(value: Option<f64>, precision: usize) -> String {
    if let Some(value) = value {
        format!("{value:.precision$}")
    } else {
        "—".into()
    }
}
