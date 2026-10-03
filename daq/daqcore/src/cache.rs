//! Naive sorted frame history, owned by the UI/main thread. Eviction has no implicit cap.
#[cfg(test)]
use crate::can;
#[cfg(test)]
use crate::frame;
use crate::{ParsedFrame, Time};
use std::{collections::HashMap, ops::RangeInclusive};
pub type CachedFrame = ParsedFrame;
#[derive(Default)]
pub struct RamCache {
    frames: Vec<CachedFrame>,
    first_index: usize,
    latest: HashMap<u32, CachedFrame>,
}
impl RamCache {
    pub fn new() -> Self {
        Self::default()
    }
    pub fn clear(&mut self) {
        self.frames.clear();
        self.first_index = 0;
        self.latest.clear();
    }
    pub fn len(&self) -> usize {
        self.frames.len() - self.first_index
    }
    pub fn is_empty(&self) -> bool {
        self.len() == 0
    }
    pub fn time_span(&self) -> Option<(Time, Time)> {
        Some((
            self.active().first()?.timestamp,
            self.active().last()?.timestamp,
        ))
    }
    pub fn latest(&self, id: u32) -> Option<&CachedFrame> {
        self.latest.get(&id)
    }
    pub fn latest_map(&self) -> &HashMap<u32, CachedFrame> {
        &self.latest
    }
    fn active(&self) -> &[CachedFrame] {
        &self.frames[self.first_index..]
    }
    pub fn push(&mut self, frame: CachedFrame) {
        self.latest.insert(frame.identity(), frame.clone());
        self.frames.push(frame);
    }
    pub fn push_batch(&mut self, mut batch: Vec<CachedFrame>) {
        if batch.is_empty() {
            return;
        }
        batch.sort_by_key(|f| f.timestamp);
        let mut old = std::mem::take(&mut self.frames)
            .into_iter()
            .skip(self.first_index)
            .peekable();
        self.first_index = 0;
        let mut batch = batch.into_iter().peekable();
        let mut merged = Vec::with_capacity(old.size_hint().0 + batch.size_hint().0);
        while let (Some(a), Some(b)) = (old.peek(), batch.peek()) {
            merged.push(if a.timestamp <= b.timestamp {
                old.next().unwrap()
            } else {
                batch.next().unwrap()
            });
        }
        merged.extend(old);
        merged.extend(batch);
        self.frames = merged;
        self.latest.clear();
        for frame in self.frames.iter().rev() {
            self.latest
                .entry(frame.identity())
                .or_insert_with(|| frame.clone());
        }
    }
    pub fn evict(&mut self, floor: Time) -> usize {
        let removed = self.active().partition_point(|f| f.timestamp < floor);
        self.first_index += removed;
        if removed != 0 {
            self.latest.retain(|_, f| f.timestamp >= floor);
        }
        if self.first_index > 0 && self.first_index >= self.len() {
            self.frames.drain(..self.first_index);
            self.first_index = 0;
            if self.frames.capacity() > self.frames.len().max(1024).saturating_mul(4) {
                self.frames.shrink_to(self.frames.len().max(1024));
            }
        }
        removed
    }
    pub fn frames_in_range(&self, range: RangeInclusive<Time>) -> &[CachedFrame] {
        let frames = self.active();
        if range.is_empty() {
            return &frames[..0];
        }
        let start = frames.partition_point(|f| f.timestamp < *range.start());
        let end = frames.partition_point(|f| f.timestamp <= *range.end());
        &frames[start..end]
    }
    pub fn frames_before(&self, time: Time, count: usize) -> &[CachedFrame] {
        let frames = self.active();
        let end = frames.partition_point(|f| f.timestamp <= time);
        &frames[end.saturating_sub(count)..end]
    }
    pub fn signal_series(
        &self,
        id: u32,
        signal: &str,
        range: RangeInclusive<Time>,
    ) -> Vec<(Time, f64)> {
        self.frames_in_range(range)
            .iter()
            .filter(|f| f.identity() == id)
            .filter_map(|f| {
                Some((
                    f.timestamp,
                    f.decoded.as_ref()?.signals.get(signal)?.value.physical,
                ))
            })
            .collect()
    }
}
#[cfg(test)]
mod tests {
    use super::*;
    fn f(ms: i64, id: u32, byte: u8) -> CachedFrame {
        ParsedFrame {
            kind: frame::FrameKind::Data,
            dlc: 1,
            timestamp: Time::from_unix_millis(ms),
            msg_id: id,
            is_msg_id_extended: false,
            raw_bytes: vec![byte],
            decoded: None,
        }
    }
    #[test]
    fn merge_is_stable_and_queries_include_cursor() {
        let mut c = RamCache::new();
        c.push(f(10, 1, 1));
        c.push(f(10, 1, 2));
        c.push_batch(vec![f(5, 2, 3), f(10, 1, 4)]);
        assert_eq!(
            c.frames_in_range(Time::from_unix_millis(10)..=Time::from_unix_millis(10))
                .iter()
                .map(|f| f.raw_bytes[0])
                .collect::<Vec<_>>(),
            [1, 2, 4]
        );
        assert_eq!(c.latest(1).unwrap().raw_bytes, [4]);
        assert_eq!(c.evict(Time::from_unix_millis(10)), 1);
        assert!(c.latest(2).is_none());
        c.evict(Time::from_unix_millis(11));
        assert!(c.is_empty());
        assert_eq!(c.first_index, 0);
        assert!(c.latest_map().is_empty());
    }
    #[test]
    fn prefixes_compact_proportionally_and_last_equal_timestamp_is_retained() {
        let mut c = RamCache::new();
        for i in 0..100 {
            c.push(f(i, 1, i as u8));
        }
        c.push(f(99, 1, 200));
        c.evict(Time::from_unix_millis(20));
        assert_eq!(c.first_index, 20);
        assert_eq!(c.latest(1).unwrap().raw_bytes, [200]);
        c.evict(Time::from_unix_millis(60));
        assert_eq!(c.first_index, 0);
        assert_eq!(c.frames.len(), 41);
        c.evict(Time::from_unix_millis(99));
        assert_eq!(c.len(), 2);
        c.evict(Time::from_unix_millis(100));
        assert!(c.latest(1).is_none());
    }
    #[test]
    fn standard_and_extended_ids_have_distinct_latest_values() {
        let mut cache = RamCache::new();
        cache.push(f(1, 3, 1));
        let mut extended = f(2, 3, 2);
        extended.is_msg_id_extended = true;
        cache.push(extended);
        assert_eq!(cache.latest(3).unwrap().raw_bytes, [1]);
        assert_eq!(
            cache.latest(3 | can::EXTENDED_ID_FLAG).unwrap().raw_bytes,
            [2]
        );
    }
}
