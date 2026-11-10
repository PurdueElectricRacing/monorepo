use crate::util;

pub const BLACK: eframe::egui::Color32 = eframe::egui::Color32::from_rgb(0x00, 0x00, 0x00);
pub const WHITE: eframe::egui::Color32 = eframe::egui::Color32::from_rgb(0xFF, 0xFF, 0xFF);
pub const DARK_GRAY: eframe::egui::Color32 = eframe::egui::Color32::from_rgb(0x83, 0x83, 0x83);
pub const LIGHT_GRAY: eframe::egui::Color32 = eframe::egui::Color32::from_rgb(0xD9, 0xD9, 0xD9);
pub const BLUE: eframe::egui::Color32 = eframe::egui::Color32::from_rgb(0x00, 0x7A, 0xFF);
pub const MUTED_BLUE: eframe::egui::Color32 = eframe::egui::Color32::from_rgb(0x0B, 0x1D, 0x31);
pub const GREEN: eframe::egui::Color32 = eframe::egui::Color32::from_rgb(0x00, 0xCC, 0x00);
pub const MUTED_GREEN: eframe::egui::Color32 = eframe::egui::Color32::from_rgb(0x0B, 0x2A, 0x0B);
pub const RED: eframe::egui::Color32 = eframe::egui::Color32::from_rgb(0xFF, 0x33, 0x36);
pub const MUTED_RED: eframe::egui::Color32 = eframe::egui::Color32::from_rgb(0x31, 0x13, 0x13);
pub const YELLOW: eframe::egui::Color32 = eframe::egui::Color32::from_rgb(0xFF, 0xDE, 0x21);
pub const MUTED_YELLOW: eframe::egui::Color32 = eframe::egui::Color32::from_rgb(0x31, 0x2C, 0x10);

pub fn lerp_color(
    a: eframe::egui::Color32,
    b: eframe::egui::Color32,
    t: f64,
) -> eframe::egui::Color32 {
    let t = t.clamp(0.0, 1.0);
    let r = util::lerp(a.r() as f64, b.r() as f64, t).round() as u8;
    let g = util::lerp(a.g() as f64, b.g() as f64, t).round() as u8;
    let b_ch = util::lerp(a.b() as f64, b.b() as f64, t).round() as u8;
    eframe::egui::Color32::from_rgb(r, g, b_ch)
}

pub fn style() -> eframe::egui::Style {
    let mut style = eframe::egui::Style::default();

    style.visuals.hyperlink_color = BLUE;
    style.visuals.warn_fg_color = YELLOW;
    style.visuals.error_fg_color = RED;
    style.visuals.selection.bg_fill = MUTED_BLUE;
    style.visuals.selection.stroke = eframe::egui::Stroke::new(1.0_f32, BLUE);

    style
}
