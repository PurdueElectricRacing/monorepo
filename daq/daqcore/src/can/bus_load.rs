//! Monotonic bus-load measurements owned by the CAN worker.
use crate::connection::CanBusSpeed;
use std::{
    collections::VecDeque,
    time::{Duration, Instant},
};
#[derive(Default)]
pub struct BusLoadTracker {
    frames: VecDeque<(Instant, usize)>,
}
impl BusLoadTracker {
    pub fn record_frame(&mut self, data_bytes: usize, now: Instant) {
        self.frames.push_back((now, data_bytes * 8 + 66));
        self.cleanup(now);
    }
    pub fn cleanup(&mut self, now: Instant) {
        while self
            .frames
            .front()
            .is_some_and(|(t, _)| now.saturating_duration_since(*t) >= Duration::from_secs(30))
        {
            self.frames.pop_front();
        }
    }
    pub fn get_load(&self, seconds: u64, speed: CanBusSpeed, now: Instant) -> f32 {
        let bits: usize = self
            .frames
            .iter()
            .filter(|(t, _)| now.saturating_duration_since(*t) < Duration::from_secs(seconds))
            .map(|(_, n)| n)
            .sum();
        bits as f32 / (speed.to_bps() as f32 * seconds as f32) * 100.0
    }
}
#[cfg(test)]
mod tests {
    use super::*;
    #[test]
    fn expires_at_window_boundary() {
        let now = Instant::now();
        let mut load = BusLoadTracker::default();
        load.record_frame(8, now);
        assert!(load.get_load(1, CanBusSpeed::Kbps500, now) > 0.0);
        assert_eq!(
            load.get_load(1, CanBusSpeed::Kbps500, now + Duration::from_secs(1)),
            0.0
        );
        load.cleanup(now + Duration::from_secs(30));
        assert!(load.frames.is_empty());
    }
}
