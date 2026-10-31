use crate::{app, telemetry, ui};

pub struct Jitter {
    pub title: String,
    msg_picker: ui::message_picker::MessagePickerState,
    selected_msg: Option<daqcore::superdbc::message::MessageKey>,
    period_ms: usize,
}

impl Jitter {
    pub fn new(instance: usize) -> Self {
        Self {
            title: format!("Jitter #{instance}"),
            msg_picker: ui::message_picker::MessagePickerState::default(),
            selected_msg: None,
            period_ms: 100,
        }
    }

    pub fn reset_database(&mut self) {
        self.selected_msg = None;
        self.msg_picker = ui::message_picker::MessagePickerState::default();
    }

    pub fn show(
        &mut self,
        ui: &mut eframe::egui::Ui,
        database: Option<&app::DatabaseSelection>,
        view: &telemetry::TelemetryView<'_>,
    ) -> egui_tiles::UiResponse {
        let Some(database) = database else {
            ui::message_picker::no_database_placeholder(ui);
            return egui_tiles::UiResponse::None;
        };
        let Some(bus) = database.bus() else {
            return egui_tiles::UiResponse::None;
        };

        if let Some(msg) = self.msg_picker.show(ui, bus, self.selected_msg.is_none()) {
            self.selected_msg = Some(msg);
            self.period_ms = bus
                .message(msg.identity)
                .and_then(|message| message.nominal_period_ms())
                .map_or(100, |ms| ms as usize);
        }
        ui.horizontal(|ui| {
            ui.label("Nominal period:");
            ui.add(
                eframe::egui::DragValue::new(&mut self.period_ms)
                    .range(1..=1_000_000)
                    .suffix(" ms"),
            );
        });
        if let Some(msg) = self.selected_msg.and_then(|key| bus.message(key.identity)) {
            let identity = msg.identity();
            ui.label(format!("{} ({identity})", msg.name()));

            let deviations = interval_deviations(view.setpoint_frames, identity, self.period_ms);
            ui.label("Absolute deviation from nominal period over the shared interval");
            ui.label(format!("Intervals recorded: {}", deviations.len()));


            if !deviations.is_empty() {


                let maximum = deviations.iter().copied().fold(0.0, f64::max);
                let average = deviations.iter().sum::<f64>() / deviations.len() as f64;

                // 1. Calculate Standard Deviation (% and raw ms)
                let variance = deviations
                    .iter()
                    .map(|&dev| (dev - average).powi(2))
                    .sum::<f64>() / deviations.len() as f64;
                let std_dev_pct = variance.sqrt();
                let std_dev_ms = (std_dev_pct / 100.0) * (self.period_ms as f64);

                // 2. Calculate 99th Percentile Error
                let mut sorted_devs = deviations.clone();
                sorted_devs.sort_by(|a, b| a.total_cmp(b)); // Sort smallest to largest
                let idx_99 = ((sorted_devs.len() as f64 * 0.99) as usize).min(sorted_devs.len().saturating_sub(1));
                let p99_error = sorted_devs[idx_99];

                // Calculate the raw millisecond equivalents
                let period = self.period_ms as f64;
                let max_ms = (maximum / 100.0) * period;
                let p99_ms = (p99_error / 100.0) * period;
                let avg_ms = (average / 100.0) * period;

                // 3. Print the new labels to the UI
                ui.label(format!("Worst Error (Max): {maximum:.2}% ({max_ms:.2} ms)"));
                ui.label(format!("99th Percentile Error: {p99_error:.2}% ({p99_ms:.2} ms)"));
                ui.label(format!("Average Error: {average:.2}% ({avg_ms:.2} ms)"));
                ui.label(format!("Std Dev: {std_dev_pct:.2}% ({std_dev_ms:.2} ms)"));

                //ui.label(format!("DEBUG: max_ms = {max_ms:.2}"));

                // 4. Generate the Bell Curve / Histogram
                let num_buckets = 50;
                let bucket_size = (maximum / num_buckets as f64).max(0.0001); // Prevent divide-by-zero
                let mut buckets = vec![0; num_buckets + 1];

                // Sort the deviations into the buckets
                for &dev in &deviations {
                    let bucket_idx = (dev / bucket_size) as usize;
                    buckets[bucket_idx.min(num_buckets)] += 1;
                }

                // Convert the buckets into egui_plot Bars
                    let bars: Vec<egui_plot::Bar> = buckets
                        .iter() // Use iter() here so we don't consume the buckets list yet
                        .enumerate()
                        .map(|(i, &count)| {
                            let x_val = (i as f64) * bucket_size + (bucket_size / 2.0);
                            egui_plot::Bar::new(x_val, count as f64).width(bucket_size * 0.9)
                        })
                        .collect();

                    // Find the highest count in any bucket to calculate headroom
                    let max_bucket_count = buckets.into_iter().max().unwrap_or(0) as f64;

                    let chart = egui_plot::BarChart::new("Histogram", bars).color(eframe::egui::Color32::LIGHT_BLUE);

                    // Draw the actual plot graph on the screen
                    egui_plot::Plot::new("jitter_histogram")
                        .height(ui.available_height()) // Expands to fill all remaining vertical space
                        .allow_zoom(false)
                        .allow_drag(false)
                        .allow_scroll(false) // Prevents the mouse wheel from moving the graph
                        .auto_bounds(true) // Forces the graph to auto-fit the data
                        .include_y(0.0)
                        .include_y(max_bucket_count * 1.1) // Adds 10% blank space above the tallest bar
                        .include_x(0.0)
                        .show(ui, |plot_ui| plot_ui.bar_chart(chart));
            }



        }
        egui_tiles::UiResponse::None
    }
}

fn interval_deviations(
    frames: &[daqcore::frame::ParsedFrame],
    identity: daqcore::frame::CanIdentity,
    period_ms: usize,
) -> Vec<f64> {
    let timestamps: Vec<_> = frames
        .iter()
        .filter(|f| f.identity == identity)
        .map(|f| f.timestamp)
        .collect();
    timestamps
        .windows(2)
        .map(|interval| {
            let elapsed_ms = interval[1].secs(interval[0]) * 1000.0;
            let deviation_ms = (elapsed_ms - period_ms as f64).abs();

            100.0 * deviation_ms / period_ms.max(1) as f64
        })
        .collect()
}
