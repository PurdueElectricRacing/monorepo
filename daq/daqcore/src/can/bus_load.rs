//! Monotonic bus-load measurements owned by the CAN worker.

use crate::{connection, superdbc};
use std::{
    collections::{BTreeMap, VecDeque},
    time,
};

pub const WINDOWS_SECS: [u64; 4] = [1, 5, 10, 30];
const MAX_WINDOW: time::Duration = time::Duration::from_secs(WINDOWS_SECS[WINDOWS_SECS.len() - 1]);
const UPDATE_INTERVAL: time::Duration = time::Duration::from_millis(200);
const BITS_PER_BYTE: usize = 8;
/// Estimated classic CAN framing, arbitration, CRC, and stuff-bit overhead per frame.
const CLASSIC_CAN_OVERHEAD_BITS: usize = 66;

#[derive(Clone, Debug, PartialEq)]
pub struct BusLoadReading {
    pub bus_id: u8,
    pub bus_name: String,
    pub values: [f32; 4],
}

pub struct BusLoadTracker {
    frames: BTreeMap<u8, VecDeque<(time::Instant, usize)>>,
    last_update: time::Instant,
}

impl BusLoadTracker {
    pub fn new(now: time::Instant) -> Self {
        Self {
            frames: BTreeMap::new(),
            last_update: now,
        }
    }

    pub fn reset(&mut self, now: time::Instant) {
        self.frames.clear();
        self.last_update = now;
    }

    pub fn record_frame(&mut self, bus_id: u8, data_bytes: usize, now: time::Instant) {
        let frame_bits = data_bytes * BITS_PER_BYTE + CLASSIC_CAN_OVERHEAD_BITS;
        self.frames
            .entry(bus_id)
            .or_default()
            .push_back((now, frame_bits));
        self.cleanup(now);
    }

    pub fn poll(
        &mut self,
        active_buses: &[&superdbc::database::Bus],
        driver_speed: Option<connection::CanBusSpeed>,
        now: time::Instant,
    ) -> Option<Vec<BusLoadReading>> {
        if now.duration_since(self.last_update) < UPDATE_INTERVAL {
            return None;
        }
        self.last_update = now;
        self.cleanup(now);

        let driver_bps = driver_speed.map(|speed| speed.to_bps() as u64);
        if active_buses.is_empty() {
            if self.frames.is_empty() {
                return None;
            }
            let baud_rate_bps =
                driver_bps.unwrap_or_else(|| connection::CanBusSpeed::default().to_bps() as u64);
            return Some(
                self.frames
                    .keys()
                    .map(|&bus_id| BusLoadReading {
                        bus_id,
                        bus_name: format!("Bus {bus_id}"),
                        values: self.get_loads(bus_id, baud_rate_bps, now),
                    })
                    .collect(),
            );
        }

        Some(
            active_buses
                .iter()
                .map(|bus| {
                    let baud_rate_bps = driver_bps.unwrap_or_else(|| bus.baud_rate());
                    BusLoadReading {
                        bus_id: bus.bus_id(),
                        bus_name: bus.name().to_owned(),
                        values: self.get_loads(bus.bus_id(), baud_rate_bps, now),
                    }
                })
                .collect(),
        )
    }

    fn cleanup(&mut self, now: time::Instant) {
        for frames in self.frames.values_mut() {
            while frames
                .front()
                .is_some_and(|(t, _)| now.saturating_duration_since(*t) >= MAX_WINDOW)
            {
                frames.pop_front();
            }
        }
    }

    fn get_load(&self, bus_id: u8, seconds: u64, baud_rate_bps: u64, now: time::Instant) -> f32 {
        if baud_rate_bps == 0 || seconds == 0 {
            return 0.0;
        }
        let Some(frames) = self.frames.get(&bus_id) else {
            return 0.0;
        };
        let bits: usize = frames
            .iter()
            .filter(|(t, _)| now.saturating_duration_since(*t) < time::Duration::from_secs(seconds))
            .map(|(_, n)| n)
            .sum();
        bits as f32 / (baud_rate_bps as f32 * seconds as f32) * 100.0
    }

    fn get_loads(&self, bus_id: u8, baud_rate_bps: u64, now: time::Instant) -> [f32; 4] {
        WINDOWS_SECS.map(|seconds| self.get_load(bus_id, seconds, baud_rate_bps, now))
    }
}
