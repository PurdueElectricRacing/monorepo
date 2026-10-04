//! Borrowed per-render projections of the single shared history.

use std::collections::BTreeMap;

pub struct TelemetryView<'a> {
    pub plot_frames: &'a [daqcore::ParsedFrame],
    pub frames: &'a [daqcore::ParsedFrame],
    pub timeline: &'a daqcore::timeline::Timeline,
    view_time: daqcore::Time,
    pub latest: BTreeMap<daqcore::frame::CanIdentity, &'a daqcore::ParsedFrame>,
}

impl<'a> TelemetryView<'a> {
    pub fn new(session: &'a daqcore::Session) -> Self {
        let cache = session.cache();
        let timeline = session.timeline();
        let frames = cache.frames_in_range(timeline.setpoint_range());
        let mut latest = BTreeMap::new();

        for frame in frames.iter().rev() {
            latest.entry(frame.identity()).or_insert(frame);
            if latest.len() == cache.latest_map().len() {
                break;
            }
        }

        let view_time = if timeline.is_live() {
            daqcore::Time::now()
        } else {
            timeline.setpoint()
        };

        Self {
            plot_frames: cache.frames_in_range(timeline.range()),
            frames,
            timeline,
            latest,
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

#[cfg(test)]
mod tests {
    use crate::telemetry;

    #[test]
    fn decimation_preserves_endpoints_extrema_and_order() {
        let points: Vec<_> = (0..100)
            .map(|index| {
                let value = match index {
                    12 => -100.0,
                    63 => 100.0,
                    _ => 0.0,
                };

                [index as f64, value]
            })
            .collect();
        let reduced = telemetry::decimate(&points, 4);

        assert_eq!(reduced.first(), points.first());
        assert_eq!(reduced.last(), points.last());
        assert!(reduced.contains(&[12.0, -100.0]));
        assert!(reduced.contains(&[63.0, 100.0]));
        assert!(reduced.windows(2).all(|pair| pair[0][0] < pair[1][0]));
        assert!(reduced.len() < points.len());
        assert!(telemetry::decimate(&[], 0).is_empty());
    }
}
