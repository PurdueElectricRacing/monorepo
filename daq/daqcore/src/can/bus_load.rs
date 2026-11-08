//! Monotonic bus-load measurements owned by the CAN worker.

use std::{
    collections::{BTreeMap, VecDeque},
    time,
};

#[derive(Clone, Debug, PartialEq)]
pub struct BusLoadReading {
    pub bus_id: u8,
    pub bus_name: String,
    pub values: [f32; 4],
}

#[derive(Default)]
pub struct BusLoadTracker {
    frames: VecDeque<(time::Instant, usize)>,
}

impl BusLoadTracker {
    pub fn record_frame(&mut self, data_bytes: usize, now: time::Instant) {
        self.frames.push_back((now, data_bytes * 8 + 66));
        self.cleanup(now);
    }

    pub fn cleanup(&mut self, now: time::Instant) {
        while self.frames.front().is_some_and(|(t, _)| {
            now.saturating_duration_since(*t) >= time::Duration::from_secs(30)
        }) {
            self.frames.pop_front();
        }
    }

    pub fn get_load(&self, seconds: u64, baud_rate_bps: u64, now: time::Instant) -> f32 {
        if baud_rate_bps == 0 || seconds == 0 {
            return 0.0;
        }
        let bits: usize = self
            .frames
            .iter()
            .filter(|(t, _)| now.saturating_duration_since(*t) < time::Duration::from_secs(seconds))
            .map(|(_, n)| n)
            .sum();
        bits as f32 / (baud_rate_bps as f32 * seconds as f32) * 100.0
    }
}

#[derive(Default)]
pub struct MultiBusLoadTracker {
    trackers: BTreeMap<u8, BusLoadTracker>,
}

impl MultiBusLoadTracker {
    pub fn record_frame(&mut self, bus_id: u8, data_bytes: usize, now: time::Instant) {
        self.trackers
            .entry(bus_id)
            .or_default()
            .record_frame(data_bytes, now);
    }

    pub fn cleanup(&mut self, now: time::Instant) {
        for tracker in self.trackers.values_mut() {
            tracker.cleanup(now);
        }
    }

    pub fn get_loads(&self, bus_id: u8, baud_rate_bps: u64, now: time::Instant) -> [f32; 4] {
        let Some(tracker) = self.trackers.get(&bus_id) else {
            return [0.0; 4];
        };
        [
            tracker.get_load(1, baud_rate_bps, now),
            tracker.get_load(5, baud_rate_bps, now),
            tracker.get_load(10, baud_rate_bps, now),
            tracker.get_load(30, baud_rate_bps, now),
        ]
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn multi_bus_load_tracker_computes_per_bus_loads_independently() {
        let mut tracker = MultiBusLoadTracker::default();
        let now = time::Instant::now();

        tracker.record_frame(0, 8, now);
        let load_vcan = tracker.get_loads(0, 500_000, now);
        let load_ccan = tracker.get_loads(2, 250_000, now);

        assert!(load_vcan[0] > 0.0);
        assert_eq!(load_ccan[0], 0.0);

        // Recording the same frame on CCAN (250kbps) should produce 2x the load of VCAN (500kbps)
        tracker.record_frame(2, 8, now);
        let load_ccan_after = tracker.get_loads(2, 250_000, now);
        assert!((load_ccan_after[0] - load_vcan[0] * 2.0).abs() < 1e-4);
    }
}
