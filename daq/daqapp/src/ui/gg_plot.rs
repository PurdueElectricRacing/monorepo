use crate::{telemetry, ui};

const AXIS_LIMIT_G: f32 = 2.0;
fn vehicle_accel_to_plot_xy(ax_g: f32, ay_g: f32) -> [f64; 2] {
    [-ay_g as f64, ax_g as f64]
}

pub struct GgPlot {
    pub title: String,
    ring_points: Vec<(String, Vec<[f64; 2]>)>,
}

impl GgPlot {
    pub fn new(instance: usize) -> Self {
        Self {
            title: format!("G-G Plot #{instance}"),
            ring_points: Self::build_ring_points(),
        }
    }

    fn draw_background(
        &self,
        plot_ui: &mut egui_plot::PlotUi<'_>,
        text_color: eframe::egui::Color32,
    ) {
        for (label, points) in &self.ring_points {
            plot_ui.line(
                egui_plot::Line::new(label.clone(), egui_plot::PlotPoints::from(points.clone()))
                    .color(text_color.linear_multiply(0.18)),
            );
        }
    }

    pub fn show(
        &self,
        ui: &mut eframe::egui::Ui,
        view: &telemetry::TelemetryView<'_>,
    ) -> egui_tiles::UiResponse {
        let points: Vec<_> = view
            .setpoint_frames
            .iter()
            .filter_map(|f| {
                let d = f.decoded.as_ref()?;
                if d.name != "IMU_acceleration" {
                    return None;
                }
                Some(vehicle_accel_to_plot_xy(
                    d.signals.get("X_axis")?.value.physical as f32,
                    d.signals.get("Y_axis")?.value.physical as f32,
                ))
            })
            .collect();
        let theme = ui::theme::get_theme(ui.ctx());
        if points.is_empty() {
            ui.label("No retained acceleration samples in the selected interval.");
        }
        egui_plot::Plot::new(format!("gg_plot_{}", self.title))
            .view_aspect(1.0)
            .data_aspect(1.0)
            .allow_axis_zoom_drag(false)
            .allow_scroll(false)
            .include_x(-AXIS_LIMIT_G as f64)
            .include_x(AXIS_LIMIT_G as f64)
            .include_y(-AXIS_LIMIT_G as f64)
            .include_y(AXIS_LIMIT_G as f64)
            .x_axis_label("Longitudinal acceleration")
            .y_axis_label("Lateral acceleration")
            .show(ui, |plot| {
                self.draw_background(plot, theme.text_color());
                let stride = points.len().div_ceil(4096).max(1);
                plot.points(
                    egui_plot::Points::new(
                        "trail",
                        egui_plot::PlotPoints::from(
                            points.iter().step_by(stride).copied().collect::<Vec<_>>(),
                        ),
                    )
                    .radius(2.0_f32)
                    .color(theme.error_color().linear_multiply(0.45)),
                );
                if let Some(point) = points.last() {
                    plot.points(
                        egui_plot::Points::new("current", vec![*point])
                            .radius(4.5_f32)
                            .color(theme.error_color()),
                    );
                }
            });
        egui_tiles::UiResponse::None
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
}
