use crate::{telemetry, ui};

use std::f32::consts::PI;

const STALE_TIMEOUT_SECONDS: u64 = 1;
const WHEELBASE_M: f32 = 1.530; // Standard Formula Student wheelbase
const CHASSIS_WIDTH_M: f32 = 1.4;
const CHASSIS_LENGTH_M: f32 = 2.5;
const ACCEL_VECTOR_SCALE: f32 = 20.0;
const SPEED_VECTOR_SCALE: f32 = 2.0;
const YAW_RATE_MAX_FOR_DRAW: f32 = 2.0; // rad/s clamp for visualization
const YAW_ARC_MIN_SWEEP_RAD: f32 = PI / 8.0;
const YAW_ARC_MAX_SWEEP_RAD: f32 = PI * 1.4;
const YAW_ARC_SEGMENTS: usize = 28;

pub struct Dynamics {
    pub title: String,
}

impl Dynamics {
    pub fn new(instance_num: usize) -> Self {
        Self {
            title: format!("Dynamics #{}", instance_num),
        }
    }

    pub fn show(
        &self,
        ui: &mut eframe::egui::Ui,
        view: &telemetry::TelemetryView<'_>,
    ) -> egui_tiles::UiResponse {
        let values = dynamics_values(view.setpoint_frames);
        let elapsed = values
            .last_update
            .map(|t| view.view_time().secs(t).max(0.0))
            .unwrap_or(f64::INFINITY);
        let stale = elapsed > STALE_TIMEOUT_SECONDS as f64;

        let theme = ui::theme::get_theme(ui.ctx());

        eframe::egui::ScrollArea::vertical().show(ui, |ui| {
            ui.add_space(4.0);
            ui.heading(&self.title);
            ui.add_space(4.0);

            draw_status_banner(ui, &theme, stale, elapsed);

            ui.add_space(8.0);

            // Allocation for custom visualization
            let available_w = ui.available_width();
            let size = eframe::egui::Vec2::splat(available_w.min(400.0));
            let (rect, _response) = ui.allocate_exact_size(size, eframe::egui::Sense::hover());

            let painter = ui.painter();
            painter.rect_filled(rect, 4.0, theme.panel_color());
            painter.rect_stroke(
                rect,
                4.0,
                eframe::egui::Stroke::new(1.0_f32, theme.accent_color()),
                eframe::egui::StrokeKind::Inside,
            );

            // --- 2D Drawing Logic ---
            let center = rect.center();
            let pixels_per_meter = rect.width() / 6.0; // 6 meters total width

            draw_chassis(painter, center, pixels_per_meter, &theme);

            if !stale {
                draw_dynamics(painter, center, pixels_per_meter, &theme, &values);
            }
        });

        egui_tiles::UiResponse::None
    }
}

/// Values derived from the selected frames; no widget state or rendering behavior.
struct DynamicsValues {
    // Motor/Vehicle Speed
    velocity_mps: f32,

    // IMU Data
    accel_x: f32, // Longitudinal (X+ = Forward)
    accel_y: f32, // Lateral (Y+ = Left)

    // Steering
    steer_angle_rad: f32,
    yaw_rate_rads: f32,

    last_update: Option<daqcore::Time>,
}

fn dynamics_values(frames: &[daqcore::ParsedFrame]) -> DynamicsValues {
    let mut values = DynamicsValues {
        velocity_mps: 0.0,
        accel_x: 0.0,
        accel_y: 0.0,
        steer_angle_rad: 0.0,
        yaw_rate_rads: 0.0,
        last_update: None,
    };

    for frame in frames {
        if let Some(parsed) = frame.decoded_view() {
            match parsed.decoded.name.as_str() {
                "IMU_acceleration" => {
                    for (_, sig) in parsed.decoded.signals.iter() {
                        match sig.name.as_str() {
                            "X_axis" => values.accel_x = sig.value.physical as f32,
                            "Y_axis" => values.accel_y = sig.value.physical as f32,
                            _ => {}
                        }
                    }
                    values.last_update = Some(parsed.timestamp);
                }
                "IMU_angular_rate" => {
                    for (_, sig) in parsed.decoded.signals.iter() {
                        if sig.name.as_str() == "Z_axis" {
                            values.yaw_rate_rads = sig.value.physical.to_radians() as f32
                        }
                    }
                    values.last_update = Some(parsed.timestamp);
                }
                "steering_angle" => {
                    for (_, sig) in parsed.decoded.signals.iter() {
                        if sig.name == "angle" {
                            values.steer_angle_rad = sig.value.physical.to_radians() as f32;
                        }
                    }
                    values.last_update = Some(parsed.timestamp);
                }
                // TODO: Implement velocity tracking (GPS velocity or Wheel speed)
                _ => {}
            }
        }
    }
    values
}

fn draw_status_banner(
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

fn draw_chassis(
    painter: &eframe::egui::Painter,
    center: eframe::egui::Pos2,
    pixels_per_meter: f32,
    theme: &ui::theme::ThemeColors,
) {
    // 1. Draw Chassis (Top-down)
    let chassis_w = CHASSIS_WIDTH_M * pixels_per_meter;
    let chassis_h = CHASSIS_LENGTH_M * pixels_per_meter;
    let chassis_rect =
        eframe::egui::Rect::from_center_size(center, eframe::egui::Vec2::new(chassis_w, chassis_h));

    painter.rect_stroke(
        chassis_rect,
        2.0,
        eframe::egui::Stroke::new(2.0_f32, theme.text_color().linear_multiply(0.3)),
        eframe::egui::StrokeKind::Outside,
    );

    // 2. Draw Wheelbase / Axles
    let axle_dist_px = WHEELBASE_M * pixels_per_meter;
    let front_axle_y = center.y - axle_dist_px / 2.0;
    let rear_axle_y = center.y + axle_dist_px / 2.0;

    painter.line_segment(
        [
            eframe::egui::Pos2::new(center.x - chassis_w / 2.0, front_axle_y),
            eframe::egui::Pos2::new(center.x + chassis_w / 2.0, front_axle_y),
        ],
        eframe::egui::Stroke::new(1.0_f32, theme.text_color().linear_multiply(0.2)),
    );
    painter.line_segment(
        [
            eframe::egui::Pos2::new(center.x - chassis_w / 2.0, rear_axle_y),
            eframe::egui::Pos2::new(center.x + chassis_w / 2.0, rear_axle_y),
        ],
        eframe::egui::Stroke::new(1.0_f32, theme.text_color().linear_multiply(0.2)),
    );
}

fn draw_dynamics(
    painter: &eframe::egui::Painter,
    center: eframe::egui::Pos2,
    pixels_per_meter: f32,
    theme: &ui::theme::ThemeColors,
    values: &DynamicsValues,
) {
    let axle_dist_px = WHEELBASE_M * pixels_per_meter;
    let front_axle_y = center.y - axle_dist_px / 2.0;
    let rear_axle_y = center.y + axle_dist_px / 2.0;

    // 3. Acceleration Vectors (separate X and Y)
    let accel_x_vec = eframe::egui::Vec2::new(0.0, -values.accel_x * ACCEL_VECTOR_SCALE);
    let accel_y_vec = eframe::egui::Vec2::new(-values.accel_y * ACCEL_VECTOR_SCALE, 0.0);
    painter.line_segment(
        [center, center + accel_x_vec],
        eframe::egui::Stroke::new(3.0_f32, theme.error_color()),
    );
    painter.circle_filled(center + accel_x_vec, 4.0, theme.error_color());
    painter.line_segment(
        [center, center + accel_y_vec],
        eframe::egui::Stroke::new(3.0_f32, theme.error_color()),
    );
    painter.circle_filled(center + accel_y_vec, 4.0, theme.error_color());

    // 4. Velocity Vector (Green)
    let vel_vec = eframe::egui::Vec2::new(0.0, -values.velocity_mps * SPEED_VECTOR_SCALE);
    painter.line_segment(
        [
            eframe::egui::Pos2::new(center.x, front_axle_y),
            eframe::egui::Pos2::new(center.x, front_axle_y) + vel_vec,
        ],
        eframe::egui::Stroke::new(3.0_f32, theme.success_color()),
    );

    // 5. Yaw rotation arc + arrowhead
    draw_yaw_rotation_arrow(painter, center, pixels_per_meter, theme, values);

    // 6. Labels
    let label_top_left = eframe::egui::Pos2::new(
        center.x - 110.0,
        center.y - (CHASSIS_LENGTH_M * pixels_per_meter / 2.0) - 48.0,
    );
    painter.text(
        label_top_left,
        eframe::egui::Align2::LEFT_TOP,
        format!("Yaw: {:+.2} rad/s", values.yaw_rate_rads),
        eframe::egui::FontId::monospace(12.0),
        theme.text_color(),
    );
    painter.text(
        label_top_left + eframe::egui::Vec2::new(0.0, 16.0),
        eframe::egui::Align2::LEFT_TOP,
        format!("Accel X: {:+.2} m/s²", values.accel_x),
        eframe::egui::FontId::monospace(12.0),
        theme.error_color(),
    );
    painter.text(
        label_top_left + eframe::egui::Vec2::new(0.0, 32.0),
        eframe::egui::Align2::LEFT_TOP,
        format!("Accel Y: {:+.2} m/s²", values.accel_y),
        eframe::egui::FontId::monospace(12.0),
        theme.error_color(),
    );

    // 7. Turn Radius Projection
    if values.steer_angle_rad.abs() > 0.01 {
        let r_m = WHEELBASE_M / values.steer_angle_rad.tan();
        let r_px = r_m * pixels_per_meter;
        let turn_center = eframe::egui::Pos2::new(center.x - r_px, rear_axle_y);

        painter.circle_stroke(
            turn_center,
            r_px.abs(),
            eframe::egui::Stroke::new(1.0_f32, theme.info_color().linear_multiply(0.3)),
        );
    }
}

fn draw_yaw_rotation_arrow(
    painter: &eframe::egui::Painter,
    center: eframe::egui::Pos2,
    pixels_per_meter: f32,
    theme: &ui::theme::ThemeColors,
    values: &DynamicsValues,
) {
    let yaw_abs = values.yaw_rate_rads.abs();
    if yaw_abs < 0.01 {
        return;
    }

    let yaw_norm = (yaw_abs / YAW_RATE_MAX_FOR_DRAW).clamp(0.0, 1.0);
    let sweep = YAW_ARC_MIN_SWEEP_RAD + yaw_norm * (YAW_ARC_MAX_SWEEP_RAD - YAW_ARC_MIN_SWEEP_RAD);
    // egui screen-space has +Y downward, so positive yaw (CCW) needs negative angle sweep.
    let direction = if values.yaw_rate_rads >= 0.0 {
        -1.0
    } else {
        1.0
    };

    let start_angle = -PI / 2.0;
    let end_angle = start_angle + direction * sweep;
    let radius = pixels_per_meter * (CHASSIS_WIDTH_M * 0.75);
    let stroke = eframe::egui::Stroke::new(1.5 + 1.5 * yaw_norm, theme.success_color());

    let mut prev = point_on_circle(center, radius, start_angle);
    for i in 1..=YAW_ARC_SEGMENTS {
        let t = i as f32 / YAW_ARC_SEGMENTS as f32;
        let angle = start_angle + (end_angle - start_angle) * t;
        let next = point_on_circle(center, radius, angle);
        painter.line_segment([prev, next], stroke);
        prev = next;
    }

    // Dot marker at arc end
    painter.circle_filled(prev, 4.0, theme.success_color());
}

fn point_on_circle(center: eframe::egui::Pos2, radius: f32, angle: f32) -> eframe::egui::Pos2 {
    eframe::egui::Pos2::new(
        center.x + radius * angle.cos(),
        center.y + radius * angle.sin(),
    )
}
