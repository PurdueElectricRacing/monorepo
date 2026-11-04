use crate::{telemetry, ui};
use daqcore::can_thread;

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
        database: Option<&can_thread::state::DatabaseSelection>,
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
            ui.label(format!("{} (0x{identity:03X})", msg.name()));

            let deviations = interval_deviations(view.setpoint_frames, identity, self.period_ms);
            ui.label("Absolute deviation from nominal period over the shared interval");
            ui.label(format!("Intervals recorded: {}", deviations.len()));
            if !deviations.is_empty() {
                let maximum = deviations.iter().copied().fold(0.0, f64::max);
                let average = deviations.iter().sum::<f64>() / deviations.len() as f64;

                ui.label(format!("Max: {maximum:.2}%  Average: {average:.2}%"));
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
