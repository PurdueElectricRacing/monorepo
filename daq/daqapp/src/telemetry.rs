//! Borrowed per-render projections of the single shared history.

use std::collections::BTreeMap;
pub struct TelemetryView<'a> {
    pub plot_frames: &'a [daqcore::ParsedFrame],
    pub frames: &'a [daqcore::ParsedFrame],
    pub timeline: &'a daqcore::timeline::Timeline,
    pub latest: BTreeMap<u32, &'a daqcore::ParsedFrame>,
}
impl<'a> TelemetryView<'a> {
    pub fn new(session: &'a daqcore::Session) -> Self {
        let frames = session
            .cache()
            .frames_in_range(session.timeline().setpoint_range());
        let mut latest = BTreeMap::new();
        for frame in frames.iter().rev() {
            latest.entry(frame.identity()).or_insert(frame);
            if latest.len() == session.cache().latest_map().len() {
                break;
            }
        }
        Self {
            plot_frames: session.cache().frames_in_range(session.timeline().range()),
            frames,
            timeline: session.timeline(),
            latest,
        }
    }
    pub fn view_time(&self) -> daqcore::Time {
        if self.timeline.is_live() {
            daqcore::Time::now()
        } else {
            self.timeline.setpoint()
        }
    }
}
/// Rendering projection that preserves extrema, endpoints and ordering in each pixel bucket.
pub fn decimate(points: &[[f64; 2]], pixels: usize) -> Vec<[f64; 2]> {
    let bucket = points.len().div_ceil(pixels.max(1)).max(1);
    if bucket <= 4 {
        return points.to_vec();
    }
    let mut out = Vec::new();
    for chunk in points.chunks(bucket) {
        let mut indexes = vec![0, chunk.len() - 1];
        indexes.push(
            chunk
                .iter()
                .enumerate()
                .min_by(|a, b| a.1[1].total_cmp(&b.1[1]))
                .unwrap()
                .0,
        );
        indexes.push(
            chunk
                .iter()
                .enumerate()
                .max_by(|a, b| a.1[1].total_cmp(&b.1[1]))
                .unwrap()
                .0,
        );
        indexes.sort();
        indexes.dedup();
        out.extend(indexes.into_iter().map(|i| chunk[i]));
    }
    out
}
pub struct BusLoadSample {
    pub timestamp: daqcore::Time,
    pub values: [f32; 4],
}

#[cfg(test)]
pub fn sample(ms: i64, id: u32, name: &str, values: &[(&str, f64)]) -> daqcore::ParsedFrame {
    let mut signals = can_decode::SignalMap::default();
    for (name, value) in values {
        signals.insert(
            (*name).into(),
            can_decode::DecodedSignal {
                name: (*name).into(),
                unit: String::new(),
                value: can_decode::DecodedSignalValue {
                    physical: *value,
                    raw: Some(*value as i128),
                    enum_label: None,
                },
            },
        );
    }
    daqcore::ParsedFrame {
        timestamp: daqcore::Time::from_unix_millis(ms),
        msg_id: id,
        kind: daqcore::frame::FrameKind::Data,
        dlc: 8,
        is_msg_id_extended: false,
        raw_bytes: vec![0; 8],
        decoded: Some(can_decode::DecodedMessage {
            name: name.into(),
            msg_id: id,
            is_extended: false,
            tx_node: "test".into(),
            signals,
        }),
    }
}
#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn cursor_is_inclusive_and_unknown_replaces_decoded() {
        let mut session = daqcore::Session::live(daqcore::Time::from_unix_millis(0), 30.0);
        session.ingest_frame(sample(100, 1, "message", &[]));
        let mut unknown = sample(200, 1, "message", &[]);
        unknown.decoded = None;
        session.ingest_frame(unknown);
        session
            .timeline_mut()
            .set_setpoint(daqcore::Time::from_unix_millis(100));
        assert!(TelemetryView::new(&session).latest[&1].decoded.is_some());
        session
            .timeline_mut()
            .set_setpoint(daqcore::Time::from_unix_millis(200));
        assert!(TelemetryView::new(&session).latest[&1].decoded.is_none());
        assert_eq!(TelemetryView::new(&session).frames.len(), 2);
    }

    #[test]
    fn scope_sees_full_selection_while_values_follow_playhead() {
        let mut session = daqcore::Session::live(daqcore::Time::from_unix_millis(0), 30.0);
        for ms in [100, 200, 300] {
            session.ingest_frame(sample(ms, 1, "test", &[]));
        }
        session
            .timeline_mut()
            .set_end(daqcore::Time::from_unix_millis(200));
        session
            .timeline_mut()
            .set_setpoint(daqcore::Time::from_unix_millis(100));
        let view = TelemetryView::new(&session);
        assert_eq!(view.frames.len(), 1);
        assert_eq!(view.plot_frames.len(), 2);
        assert_eq!(
            view.latest[&1].timestamp,
            daqcore::Time::from_unix_millis(100)
        );
        assert_eq!(
            view.plot_frames.last().unwrap().timestamp,
            daqcore::Time::from_unix_millis(200)
        );
    }
    #[test]
    fn drawing_reduction_keeps_extrema_and_endpoints() {
        let mut points: Vec<_> = (0..1000).map(|i| [i as f64, 0.0]).collect();
        points[333][1] = 100.0;
        points[444][1] = -100.0;
        let reduced = decimate(&points, 50);
        assert!(reduced.len() <= 200);
        for i in [0, 333, 444, 999] {
            assert!(reduced.contains(&points[i]));
        }
        assert!(reduced.windows(2).all(|p| p[0][0] <= p[1][0]));
    }
}

#[cfg(test)]
mod profile {
    use super::*;
    #[test]
    #[ignore = "manual rendering profile"]
    fn shared_widgets_at_representative_rates() {
        use crate::ui::battery::battery_temps;
        use crate::ui::battery::battery_voltage;
        use crate::ui::scope;

        for rate in [200, 5000] {
            let mut session = daqcore::Session::live(daqcore::Time::from_unix_millis(0), 30.0);
            for i in 0..(rate * 30) {
                session.ingest_frame(sample(
                    i * 1000 / rate,
                    1,
                    "cell_telemetry",
                    &[
                        ("module_num", (i % 7) as f64),
                        ("cell_num", (i % 16) as f64),
                        ("voltage", 3.5 + (i % 10) as f64 / 100.0),
                        ("balance_status", 0.0),
                    ],
                ));
            }
            let context = eframe::egui::Context::default();
            let mut scope = scope::Scope::new(1, 1, "cell_telemetry".into(), "voltage".into());
            let mut battery = battery_voltage::BatteryVoltage::new(1);
            let mut temps = battery_temps::BatteryTemps::new(1);
            let mut timings = Vec::new();
            for tick in 0..35 {
                let start = std::time::Instant::now();
                let view = TelemetryView::new(&session);
                battery.project(&view);
                temps.project(&view);
                let output = context.run(
                    eframe::egui::RawInput {
                        screen_rect: Some(eframe::egui::Rect::from_min_size(
                            eframe::egui::Pos2::ZERO,
                            eframe::egui::vec2(1920.0, 1080.0),
                        )),
                        ..Default::default()
                    },
                    |ctx| {
                        eframe::egui::CentralPanel::default().show(ctx, |ui| {
                            ui.columns(3, |cols| {
                                let _ = scope.show(&mut cols[0], None, &view);
                                let _ = battery.show(&mut cols[1]);
                                let _ = temps.show(&mut cols[2]);
                            });
                        });
                    },
                );
                std::hint::black_box(context.tessellate(output.shapes, output.pixels_per_point));
                if tick >= 5 {
                    timings.push(start.elapsed());
                }
            }
            timings.sort();
            println!(
                "{rate} frames/s 30-second window: median={:?}, p95={:?}, max={:?} (CPU egui+projection+tessellation, no GPU)",
                timings[15], timings[28], timings[29]
            );
        }
    }
}
