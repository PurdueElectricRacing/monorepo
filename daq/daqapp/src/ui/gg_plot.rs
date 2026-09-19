use crate::{messages, ui};
use chrono::DateTime;
use eframe::egui;
use std::collections::VecDeque;

/// Safety cap so a mis-set window / flood cannot grow without bound.
const MAX_HISTORY_POINTS: usize = 500_000;
const AXIS_LIMIT_G: f32 = 2.0;

/// Vehicle (+X forward, +Y left) to `egui_plot` data: horizontal = −Ay, vertical = Ax.
#[inline]
fn vehicle_accel_to_plot_xy(ax_g: f32, ay_g: f32) -> [f64; 2] {
    [-ay_g as f64, ax_g as f64]
}

pub struct GgPlot {
    pub title: String,
    points_g: VecDeque<(DateTime<chrono::Local>, f32, f32)>,
    ring_points: Vec<(String, Vec<[f64; 2]>)>,
    history_window_minutes: f64,
}

impl GgPlot {
    pub fn new(instance_num: usize) -> Self {
        Self {
            title: format!("G-G Plot #{}", instance_num),
            points_g: VecDeque::new(),
            ring_points: Self::build_ring_points(),
            history_window_minutes: 5.0,
        }
    }

    fn build_ring_points() -> Vec<(String, Vec<[f64; 2]>)> {
        [0.5, 1.0, 1.5]
            .iter()
            .map(|radius| {
                let mut points = Vec::with_capacity(65);
                for i in 0..=64 {
                    let theta = (i as f64 / 64.0) * std::f64::consts::TAU;
                    points.push([*radius * theta.cos(), *radius * theta.sin()]);
                }
                (format!("{radius:.1}g"), points)
            })
            .collect()
    }

    fn retention_chrono(&self) -> chrono::Duration {
        let window = std::time::Duration::from_secs_f64(self.history_window_minutes * 60.0);
        chrono::Duration::from_std(window).unwrap_or_else(|_| chrono::Duration::zero())
    }

    fn prune_points_to_window(&mut self, newest: DateTime<chrono::Local>) {
        if self.points_g.is_empty() {
            return;
        }
        let cutoff = newest - self.retention_chrono();
        while let Some((t, _, _)) = self.points_g.front() {
            if *t < cutoff {
                self.points_g.pop_front();
            } else {
                break;
            }
        }
        while self.points_g.len() > MAX_HISTORY_POINTS {
            self.points_g.pop_front();
        }
    }

    fn extract_sample(msg: &messages::MsgFromCan) -> Option<(DateTime<chrono::Local>, f32, f32)> {
        let messages::MsgFromCan::ParsedMessage(parsed) = msg else {
            return None;
        };
        if parsed.decoded.name != "IMU_acceleration" {
            return None;
        }

        let mut forward_g = None;
        let mut left_g = None;
        for (_, sig) in &parsed.decoded.signals {
            match sig.name.as_str() {
                "X_axis" => forward_g = Some(sig.value.physical as f32),
                "Y_axis" => left_g = Some(sig.value.physical as f32),
                _ => {}
            }
        }

        match (forward_g, left_g) {
            (Some(ax), Some(ay)) => Some((parsed.timestamp, ax, ay)),
            _ => None,
        }
    }

    fn add_sample(&mut self, timestamp: DateTime<chrono::Local>, ax: f32, ay: f32) {
        self.points_g.push_back((timestamp, ax, ay));
        self.prune_points_to_window(timestamp);
    }

    fn draw_background(&self, plot_ui: &mut egui_plot::PlotUi<'_>, text_color: egui::Color32) {
        for (label, points) in &self.ring_points {
            plot_ui.line(
                egui_plot::Line::new(label.clone(), egui_plot::PlotPoints::from(points.clone()))
                    .color(text_color.linear_multiply(0.18)),
            );
        }
    }

    fn draw_data(&self, plot_ui: &mut egui_plot::PlotUi<'_>, error_color: egui::Color32) {
        let trail: Vec<[f64; 2]> = self
            .points_g
            .iter()
            .map(|(_, ax_g, ay_g)| vehicle_accel_to_plot_xy(*ax_g, *ay_g))
            .collect();
        if !trail.is_empty() {
            plot_ui.points(
                egui_plot::Points::new("trail", egui_plot::PlotPoints::from(trail))
                    .radius(2.0)
                    .color(error_color.linear_multiply(0.45)),
            );
        }

        if let Some((_, ax_g, ay_g)) = self.points_g.back() {
            plot_ui.points(
                egui_plot::Points::new(
                    "current",
                    egui_plot::PlotPoints::from(vec![vehicle_accel_to_plot_xy(*ax_g, *ay_g)]),
                )
                .radius(4.5)
                .color(error_color),
            );
        }
    }

    pub fn handle_can_message(&mut self, msg: &messages::MsgFromCan) {
        if let Some((timestamp, ax, ay)) = Self::extract_sample(msg) {
            self.add_sample(timestamp, ax, ay);
        }
    }

    pub fn show(&mut self, ui: &mut egui::Ui) -> egui_tiles::UiResponse {
        let theme = ui::theme::get_theme(ui.ctx());
        if let Some((newest, _, _)) = self.points_g.back().cloned() {
            self.prune_points_to_window(newest);
        }

        ui.horizontal(|ui| {
            ui.label("Retention:");
            ui.add(egui::Slider::new(&mut self.history_window_minutes, 0.5..=20.0).suffix(" min"));
            ui.separator();
            if ui.button("🗑 Clear").clicked() {
                self.points_g.clear();
            }
        });
        ui.add_space(4.0);

        let axis_limit = AXIS_LIMIT_G as f64;
        egui_plot::Plot::new(format!("gg_plot_{}", self.title))
            .view_aspect(1.0)
            .data_aspect(1.0)
            .allow_axis_zoom_drag(false)
            .allow_scroll(false)
            .include_x(-axis_limit)
            .include_x(axis_limit)
            .include_y(-axis_limit)
            .include_y(axis_limit)
            .show_axes([true, true])
            .x_axis_label("Longitudinal acceleration")
            .y_axis_label("Lateral acceleration")
            .show(ui, |plot_ui| {
                self.draw_background(plot_ui, theme.text_color());
                self.draw_data(plot_ui, theme.error_color());
            });

        egui_tiles::UiResponse::None
    }
}
