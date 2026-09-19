use crate::connection;

// SOF	1
// ID	11
// RTR	1
// IDE	1
// r0	1
// DLC	4
// CRC	15
// CRC delim	1
// ACK	2
// EOF	7
// IFS	3
// Subtotal: 47
// But with bit stuffing: +~20%
const CAN_FRAME_BASE_BITS: usize = 66;

const CLEAN_UP_INTERVAL_SECS: i64 = 30;

pub struct BusLoadTracker {
    timestamp_bits: std::collections::VecDeque<(chrono::DateTime<chrono::Local>, usize)>,
}

impl BusLoadTracker {
    pub fn new() -> Self {
        Self {
            timestamp_bits: std::collections::VecDeque::new(),
        }
    }

    pub fn record_frame(&mut self, data_bytes: usize) {
        self.timestamp_bits
            .push_back((chrono::Local::now(), data_bytes * 8 + CAN_FRAME_BASE_BITS));
    }

    // Returns bus load percentage for the given window in seconds
    pub fn get_load(&self, window_secs: u64, can_bus_speed: connection::CanBusSpeed) -> f32 {
        let now = chrono::Local::now();
        let window_duration = chrono::Duration::seconds(window_secs as i64);
        let cutoff_time = now - window_duration;

        let total_bits: usize = self
            .timestamp_bits
            .iter()
            .filter(|&&(ts, _)| ts > cutoff_time)
            .map(|&(_, bits)| bits)
            .sum();

        let max_bits_in_window = can_bus_speed.to_bps() as f32 * window_secs as f32;
        (total_bits as f32 / max_bits_in_window) * 100.0
    }

    // Clean up old entries
    pub fn cleanup(&mut self) {
        let cutoff_time = chrono::Local::now() - chrono::Duration::seconds(CLEAN_UP_INTERVAL_SECS);
        while let Some(&(ts, _)) = self.timestamp_bits.front() {
            if ts <= cutoff_time {
                self.timestamp_bits.pop_front();
            } else {
                break;
            }
        }
    }
}
