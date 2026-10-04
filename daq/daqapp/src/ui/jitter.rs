use crate::{app, telemetry, ui::dbc_msg_picker};

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
            let identity = match daqcore::can::can_dbc_identity(&msg.id) {
                Ok(identity) => identity,
                Err(error) => {
                    ui.colored_label(ui.visuals().error_fg_color, error.to_string());
                    return egui_tiles::UiResponse::None;
                }
            };

            let deviations = interval_deviations(view.frames, identity, self.period_ms);
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
    frames: &[daqcore::ParsedFrame],
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
