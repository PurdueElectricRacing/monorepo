use super::dbc_msg_picker;
use crate::app;
use crate::telemetry;

pub struct Jitter {
    pub title: String,
    msg_picker: dbc_msg_picker::DbcMsgPickerState,
    selected_msg: Option<can_dbc::Message>,
    period_ms: usize,
}
impl Jitter {
    pub fn new(instance: usize) -> Self {
        Self {
            title: format!("Jitter #{instance}"),
            msg_picker: dbc_msg_picker::DbcMsgPickerState::default(),
            selected_msg: None,
            period_ms: 100,
        }
    }
    pub fn show(
        &mut self,
        ui: &mut eframe::egui::Ui,
        parser: Option<&app::ParserInfo>,
        view: &telemetry::TelemetryView<'_>,
    ) -> egui_tiles::UiResponse {
        let Some(parser) = parser else {
            dbc_msg_picker::no_dbc_placeholder(ui);
            return egui_tiles::UiResponse::None;
        };
        if let Some(msg) = self
            .msg_picker
            .show(ui, &parser.parser, self.selected_msg.is_none())
        {
            self.selected_msg = Some(msg);
        }
        ui.horizontal(|ui| {
            ui.label("Nominal period:");
            ui.add(
                eframe::egui::DragValue::new(&mut self.period_ms)
                    .range(1..=1_000_000)
                    .suffix(" ms"),
            );
        });
        if let Some(msg) = &self.selected_msg {
            let id = daqcore::can::can_dbc_to_u32_without_extid_flag(&msg.id);
            ui.label(format!("{} (0x{id:X})", msg.name));
            let deviations = interval_deviations(
                view.frames,
                daqcore::can::can_dbc_to_u32_with_extid_flag(&msg.id),
                self.period_ms,
            );
            ui.label("Absolute deviation from nominal period over the shared interval");
            ui.label(format!("Intervals recorded: {}", deviations.len()));
            if !deviations.is_empty() {
                ui.label(format!(
                    "Max: {:.2}%  Average: {:.2}%",
                    deviations.iter().copied().fold(0.0, f64::max),
                    deviations.iter().sum::<f64>() / deviations.len() as f64
                ));
            }
        }
        egui_tiles::UiResponse::None
    }
}

fn interval_deviations(
    frames: &[daqcore::ParsedFrame],
    identity: u32,
    period_ms: usize,
) -> Vec<f64> {
    let timestamps: Vec<_> = frames
        .iter()
        .filter(|f| f.identity() == identity)
        .map(|f| f.timestamp)
        .collect();
    timestamps
        .windows(2)
        .map(|w| {
            100.0 * (w[1].secs(w[0]) * 1000.0 - period_ms as f64).abs() / period_ms.max(1) as f64
        })
        .collect()
}
#[cfg(test)]
mod tests {
    use super::*;
    #[test]
    fn selected_interval_counts_undecoded_arrivals_and_distinguishes_identity() {
        let mut session = daqcore::Session::live(daqcore::Time::from_unix_millis(0), 30.0);
        for ms in [0, 100, 220, 300] {
            let mut f = telemetry::sample(ms, 1, "test", &[]);
            f.decoded = None;
            session.ingest_frame(f);
        }
        let mut extended = telemetry::sample(150, 1, "test", &[]);
        extended.is_msg_id_extended = true;
        session.ingest_frame(extended);
        session
            .timeline_mut()
            .set_start(daqcore::Time::from_unix_millis(100));
        session
            .timeline_mut()
            .set_setpoint(daqcore::Time::from_unix_millis(220));
        let deviations =
            interval_deviations(telemetry::TelemetryView::new(&session).frames, 1, 100);
        assert_eq!(deviations, [20.0]);
    }
}
