//! Borrowed per-render projections of the single shared history.

use std::collections::BTreeMap;
pub struct TelemetryView<'a> {
    pub plot_frames: &'a [daqcore::ParsedFrame],
    pub frames: &'a [daqcore::ParsedFrame],
    pub timeline: &'a daqcore::timeline::Timeline,
    view_time: daqcore::Time,
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
            view_time: if session.timeline().is_live() {
                daqcore::Time::now()
            } else {
                session.timeline().setpoint()
            },
        }
    }
    pub fn view_time(&self) -> daqcore::Time {
        self.view_time
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
