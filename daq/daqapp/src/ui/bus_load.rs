use crate::telemetry::{BusLoadSample, TelemetryView};
use eframe::egui;
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
        &mut self,
        ui: &mut egui::Ui,
        samples: &[BusLoadSample],
        view: &TelemetryView<'_>,
    ) -> egui_tiles::UiResponse {
        let end = samples.partition_point(|s| s.timestamp <= view.timeline.setpoint());
        let samples = &samples[..end];
        let points: Vec<_> = samples
            .iter()
            .map(|s| [s.timestamp.secs(view.timeline.start()), s.values[0] as f64])
            .collect();
        egui_plot::Plot::new(&self.title)
            .view_aspect(2.0)
            .include_x(0.0)
            .include_x(view.timeline.end().secs(view.timeline.start()))
            .y_axis_label("Bus Load (%)")
            .show(ui, |plot| {
                plot.line(egui_plot::Line::new(
                    "Bus Load",
                    crate::telemetry::decimate(&points, ui_width_hint()),
                ));
            });
        if let Some(last) = samples.last() {
            egui::Grid::new((&self.title, "loads")).show(ui, |ui| {
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
fn ui_width_hint() -> usize {
    1024
}
