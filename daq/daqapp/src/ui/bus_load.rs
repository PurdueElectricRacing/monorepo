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
        let start_time = view.timeline.start();
        let end_time = view.timeline.end();
        let duration = end_time.secs(start_time).max(0.001);
        let playhead = view.timeline.setpoint().secs(start_time);

        let start_index = samples.partition_point(|sample| sample.timestamp < start_time);
        let end_index = samples.partition_point(|sample| sample.timestamp <= end_time);
        let samples = &samples[start_index..end_index];

        let points: Vec<_> = samples
            .iter()
            .map(|sample| [sample.timestamp.secs(start_time), sample.values[0] as f64])
            .collect();
        let pixels = ui.available_width().max(1.0) as usize;
        let points = telemetry::decimate(&points, pixels);

        egui_plot::Plot::new(&self.title)
            .view_aspect(2.0)
            .allow_zoom([false, true])
            .allow_drag([false, true])
            .allow_scroll([false, true])
            .x_axis_formatter(|mark, _| {
                let offset_ms = (mark.value * 1000.0).round() as i64;
                start_time.offset(offset_ms).label()
            })
            .x_axis_label("Time")
            .y_axis_label("Bus Load (%)")
            .show(ui, |plot| {
                plot.set_plot_bounds_x(0.0..=duration);
                plot.line(egui_plot::Line::new("Bus Load", points));
                plot.vline(
                    egui_plot::VLine::new("Playhead", playhead)
                        .color(eframe::egui::Color32::YELLOW),
                );
            });
        let cursor = samples.partition_point(|sample| sample.timestamp <= view.timeline.setpoint());
        let samples = &samples[..cursor];

        if let Some(last) = samples.last() {
            eframe::egui::Grid::new((&self.title, "loads")).show(ui, |ui| {
                ui.label("Measurement window");
                ui.label("Load");
                ui.label("Maximum in selected interval");
                ui.end_row();

                for (i, seconds) in [1, 5, 10, 30].into_iter().enumerate() {
                    let maximum = samples
                        .iter()
                        .map(|sample| sample.values[i])
                        .fold(0.0, f32::max);

                    ui.label(format!("{seconds} seconds"));
                    ui.label(format!("{:.2}%", last.values[i]));
                    ui.label(format!("{maximum:.2}%"));
                    ui.end_row();
                }
            });
        } else {
            ui.label("No bus-load samples in the selected interval.");
        }
        egui_tiles::UiResponse::None
    }
}
