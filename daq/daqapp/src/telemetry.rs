//! Borrowed per-render projections of the single shared history.

use std::collections::BTreeMap;

pub struct TelemetryView<'a> {
    // All frames in the current timeline range (start to end)
    pub all_frames: &'a [daqcore::ParsedFrame],
    // Frames in the current timeline range that are at or before the setpoint (start to setpoint)
    pub setpoint_frames: &'a [daqcore::ParsedFrame],
    // Latest frame for each message before the setpoint
    // (Latest message in the time range start to setpoint)
    pub latest_setpoint_frames: BTreeMap<daqcore::frame::CanIdentity, &'a daqcore::ParsedFrame>,
    pub timeline: &'a daqcore::timeline::Timeline,
    view_time: daqcore::Time,
}

impl<'a> TelemetryView<'a> {
    pub fn new(session: &'a daqcore::Session) -> Self {
        let cache = session.cache();
        let timeline = session.timeline();
        let all_frames = cache.frames_in_range(timeline.range());
        let setpoint_frames = cache.frames_in_range(timeline.setpoint_range());

        let mut latest_setpoint_frames = BTreeMap::new();
        for frame in setpoint_frames.iter().rev() {
            latest_setpoint_frames
                .entry(frame.identity)
                .or_insert(frame);
            if latest_setpoint_frames.len() == cache.latest_map().len() {
                break;
            }
        }

        let view_time = if timeline.is_live() {
            daqcore::Time::now()
        } else {
            timeline.setpoint()
        };

        Self {
            all_frames,
            setpoint_frames,
            timeline,
            latest_setpoint_frames,
            view_time,
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
        let minimum = chunk
            .iter()
            .enumerate()
            .min_by(|(_, left), (_, right)| left[1].total_cmp(&right[1]));
        let maximum = chunk
            .iter()
            .enumerate()
            .max_by(|(_, left), (_, right)| left[1].total_cmp(&right[1]));

        if let Some((index, _)) = minimum {
            indexes.push(index);
        }

        if let Some((index, _)) = maximum {
            indexes.push(index);
        }

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
