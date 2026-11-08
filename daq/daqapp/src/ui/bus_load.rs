use crate::telemetry;
use std::collections::BTreeMap;

const BUS_COLORS: [eframe::egui::Color32; 6] = [
    eframe::egui::Color32::from_rgb(66, 165, 245),  // Blue
    eframe::egui::Color32::from_rgb(102, 187, 106), // Green
    eframe::egui::Color32::from_rgb(255, 167, 38),  // Orange
    eframe::egui::Color32::from_rgb(171, 71, 188),  // Purple
    eframe::egui::Color32::from_rgb(239, 83, 80),   // Red
    eframe::egui::Color32::from_rgb(38, 198, 218),  // Cyan
];

fn color_for_bus(index: usize) -> eframe::egui::Color32 {
    BUS_COLORS[index % BUS_COLORS.len()]
}

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
        let window_samples = &samples[start_index..end_index];

        let mut bus_series: BTreeMap<u8, (String, Vec<[f64; 2]>)> = BTreeMap::new();
        for sample in window_samples {
            let t = sample.timestamp.secs(start_time);
            for reading in &sample.buses {
                let entry = bus_series
                    .entry(reading.bus_id)
                    .or_insert_with(|| (reading.bus_name.clone(), Vec::new()));
                entry.0 = reading.bus_name.clone();
                entry.1.push([t, reading.values[0] as f64]);
            }
        }

        let pixels = ui.available_width().max(1.0) as usize;
        let decimated_series: Vec<(u8, String, eframe::egui::Color32, Vec<[f64; 2]>)> = bus_series
            .into_iter()
            .enumerate()
            .map(|(idx, (bus_id, (bus_name, points)))| {
                let color = color_for_bus(idx);
                (
                    bus_id,
                    bus_name,
                    color,
                    telemetry::decimate(&points, pixels),
                )
            })
            .collect();

        if !decimated_series.is_empty() {
            ui.horizontal_wrapped(|ui| {
                ui.label(eframe::egui::RichText::new("Key:").strong());
                for (_, bus_name, color, _) in &decimated_series {
                    ui.colored_label(*color, format!("■ {bus_name}"));
                    ui.add_space(8.0);
                }
            });
        }

        egui_plot::Plot::new(&self.title)
            .view_aspect(2.0)
            .allow_zoom([false, true])
            .allow_drag([false, true])
            .allow_scroll([false, true])
            .legend(egui_plot::Legend::default())
            .x_axis_formatter(|mark, _| {
                let offset_ms = (mark.value * 1000.0).round() as i64;
                start_time.offset(offset_ms).label()
            })
            .x_axis_label("Time")
            .y_axis_label("Bus Load (%)")
            .show(ui, |plot| {
                plot.set_plot_bounds_x(0.0..=duration);
                for (_, bus_name, color, points) in &decimated_series {
                    plot.line(egui_plot::Line::new(bus_name.clone(), points.clone()).color(*color));
                }
                plot.vline(
                    egui_plot::VLine::new("Playhead", playhead)
                        .color(eframe::egui::Color32::YELLOW),
                );
            });

        let cursor =
            window_samples.partition_point(|sample| sample.timestamp <= view.timeline.setpoint());
        let upto_cursor = &window_samples[..cursor];

        if let Some(last) = upto_cursor.last() {
            eframe::egui::Grid::new((&self.title, "loads"))
                .striped(true)
                .show(ui, |ui| {
                    ui.label(eframe::egui::RichText::new("Bus").strong());
                    ui.label(eframe::egui::RichText::new("Window").strong());
                    ui.label(eframe::egui::RichText::new("Current Load").strong());
                    ui.label(eframe::egui::RichText::new("Max in Interval").strong());
                    ui.end_row();

                    for (bus_idx, reading) in last.buses.iter().enumerate() {
                        let color = color_for_bus(bus_idx);
                        for (win_idx, seconds) in [1, 5, 10, 30].into_iter().enumerate() {
                            let maximum = upto_cursor
                                .iter()
                                .filter_map(|sample| {
                                    sample
                                        .buses
                                        .iter()
                                        .find(|b| b.bus_id == reading.bus_id)
                                        .map(|b| b.values[win_idx])
                                })
                                .fold(0.0, f32::max);

                            ui.colored_label(color, &reading.bus_name);
                            ui.label(format!("{seconds}s"));
                            ui.label(format!("{:.2}%", reading.values[win_idx]));
                            ui.label(format!("{maximum:.2}%"));
                            ui.end_row();
                        }
                    }
                });
        } else {
            ui.label("No bus-load samples in the selected interval.");
        }
        egui_tiles::UiResponse::None
    }
}
