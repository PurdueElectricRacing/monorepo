use crate::telemetry;

pub struct BusLoad {
    pub title: String,
}
impl BusLoad {
    pub fn new(instance: usize) -> Self {
        Self {
            title: format!("Bus Load #{instance}"),
        }
    }
    pub fn show(
        &self,
        ui: &mut eframe::egui::Ui,
        samples: &[telemetry::BusLoadSample],
        view: &telemetry::TelemetryView<'_>,
    ) -> egui_tiles::UiResponse {
        let start = samples.partition_point(|s| s.timestamp < view.timeline.start());
        let end = samples.partition_point(|s| s.timestamp <= view.timeline.end());
        let samples = &samples[start..end];
        let points: Vec<_> = samples
            .iter()
            .map(|s| [s.timestamp.secs(view.timeline.start()), s.values[0] as f64])
            .collect();
        let points = telemetry::decimate(&points, ui.available_width().max(1.0) as usize);
        egui_plot::Plot::new(&self.title)
            .view_aspect(2.0)
            .allow_zoom([false, true])
            .allow_drag([false, true])
            .allow_scroll([false, true])
            .x_axis_formatter(|mark, _| {
                view.timeline
                    .start()
                    .offset((mark.value * 1000.0).round() as i64)
                    .label()
            })
            .x_axis_label("Time")
            .y_axis_label("Bus Load (%)")
            .show(ui, |plot| {
                plot.set_plot_bounds_x(
                    0.0..=view.timeline.end().secs(view.timeline.start()).max(0.001),
                );
                plot.line(egui_plot::Line::new("Bus Load", points));
                plot.vline(
                    egui_plot::VLine::new(
                        "Playhead",
                        view.timeline.setpoint().secs(view.timeline.start()),
                    )
                    .color(eframe::egui::Color32::YELLOW),
                );
            });
        let cursor = samples.partition_point(|s| s.timestamp <= view.timeline.setpoint());
        let samples = &samples[..cursor];
        if let Some(last) = samples.last() {
            eframe::egui::Grid::new((&self.title, "loads")).show(ui, |ui| {
                ui.label("Measurement window");
                ui.label("Load");
                ui.label("Maximum in selected interval");
                ui.end_row();
                for (i, seconds) in [1, 5, 10, 30].into_iter().enumerate() {
                    ui.label(format!("{seconds} seconds"));
                    ui.label(format!("{:.2}%", last.values[i]));
                    ui.label(format!(
                        "{:.2}%",
                        samples.iter().map(|s| s.values[i]).fold(0.0, f32::max)
                    ));
                    ui.end_row();
                }
            });
        } else {
            ui.label("No bus-load samples in the selected interval.");
        }
        egui_tiles::UiResponse::None
    }
}
