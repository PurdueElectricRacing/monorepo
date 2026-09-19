use crate::messages;
use eframe::egui;

const PLOT_TIME_WINDOW_SECS: f64 = 30.0;

pub struct BusLoad {
    pub title: String,

    pub load_1s: f32,
    pub load_5s: f32,
    pub load_10s: f32,
    pub load_30s: f32,

    pub max_1s: f32,
    pub max_5s: f32,
    pub max_10s: f32,
    pub max_30s: f32,

    pub window: std::collections::VecDeque<(f64, f64)>, // (time, load)
}

impl BusLoad {
    pub fn new(instance: usize) -> Self {
        Self {
            title: format!("Bus Load #{}", instance),
            load_1s: 0.0,
            load_5s: 0.0,
            load_10s: 0.0,
            load_30s: 0.0,

            max_1s: 0.0,
            max_5s: 0.0,
            max_10s: 0.0,
            max_30s: 0.0,

            window: std::collections::VecDeque::new(),
        }
    }

    fn get_color(&self, load: f32) -> egui::Color32 {
        if load < 50.0 {
            egui::Color32::GREEN
        } else if load < 80.0 {
            egui::Color32::YELLOW
        } else {
            egui::Color32::RED
        }
    }

    pub fn show(&mut self, ui: &mut egui::Ui) -> egui_tiles::UiResponse {
        egui_plot::Plot::new(&self.title)
            .view_aspect(2.0)
            .auto_bounds(egui::Vec2b::TRUE)
            .y_axis_label("Bus Load (%)")
            .allow_axis_zoom_drag(false)
            .show(ui, |plot_ui| {
                if self.window.is_empty() {
                    return;
                }

                let points: egui_plot::PlotPoints = self
                    .window
                    .iter()
                    .map(|(time, value)| [*time, *value])
                    .collect();

                let line = egui_plot::Line::new("Bus Load (%)", points)
                    .color(egui::Color32::from_rgb(100, 200, 100))
                    .stroke(egui::Stroke::new(
                        2.0,
                        egui::Color32::from_rgb(100, 200, 100),
                    ));

                plot_ui.line(line);
            });

        egui::Grid::new(format!("bus_load_grid_{}", self.title))
            .striped(true)
            .spacing([20.0, 10.0])
            .show(ui, |ui| {
                // Header
                ui.label(
                    egui::RichText::new("Window")
                        .strong()
                        .color(ui.style().visuals.text_color()),
                );
                ui.label(
                    egui::RichText::new("Bus Load %")
                        .strong()
                        .color(ui.style().visuals.text_color()),
                );
                ui.label(
                    egui::RichText::new("Max Load %")
                        .strong()
                        .color(ui.style().visuals.text_color()),
                );
                ui.end_row();

                // 1 second
                ui.label("1 second");
                let color_1s = self.get_color(self.load_1s);
                let max_color_1s = self.get_color(self.max_1s);
                ui.colored_label(color_1s, format!("{:.2}%", self.load_1s));
                ui.colored_label(max_color_1s, format!("{:.2}%", self.max_1s));
                ui.end_row();

                // 5 seconds
                ui.label("5 seconds");
                let color_5s = self.get_color(self.load_5s);
                let max_color_5s = self.get_color(self.max_5s);
                ui.colored_label(color_5s, format!("{:.2}%", self.load_5s));
                ui.colored_label(max_color_5s, format!("{:.2}%", self.max_5s));
                ui.end_row();

                // 10 seconds
                ui.label("10 seconds");
                let color_10s = self.get_color(self.load_10s);
                let max_color_10s = self.get_color(self.max_10s);
                ui.colored_label(color_10s, format!("{:.2}%", self.load_10s));
                ui.colored_label(max_color_10s, format!("{:.2}%", self.max_10s));
                ui.end_row();

                // 30 seconds
                ui.label("30 seconds");
                let color_30s = self.get_color(self.load_30s);
                let max_color_30s = self.get_color(self.max_30s);
                ui.colored_label(color_30s, format!("{:.2}%", self.load_30s));
                ui.colored_label(max_color_30s, format!("{:.2}%", self.max_30s));
                ui.end_row();
            });

        egui_tiles::UiResponse::None
    }

    pub fn handle_can_message(&mut self, msg: &messages::MsgFromCan) {
        if let messages::MsgFromCan::BusLoad {
            load_1s,
            load_5s,
            load_10s,
            load_30s,
        } = msg
        {
            self.load_1s = *load_1s;
            self.load_5s = *load_5s;
            self.load_10s = *load_10s;
            self.load_30s = *load_30s;

            self.max_1s = self.max_1s.max(*load_1s);
            self.max_5s = self.max_5s.max(*load_5s);
            self.max_10s = self.max_10s.max(*load_10s);
            self.max_30s = self.max_30s.max(*load_30s);

            let current_time = chrono::Local::now().timestamp_millis() as f64 / 1000.0;
            self.window.push_back((current_time, *load_1s as f64));
            while let Some(&(time, _)) = self.window.front() {
                if current_time - time > PLOT_TIME_WINDOW_SECS {
                    self.window.pop_front();
                } else {
                    break;
                }
            }
        }
    }
}
