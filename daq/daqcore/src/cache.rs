use crate::{ParsedFrame, Time, frame::CanIdentity};
use std::{collections::HashMap, ops::RangeInclusive};

pub type CachedFrame = ParsedFrame;

#[derive(Default)]
pub struct RamCache {
    frames: Vec<CachedFrame>,
    first_index: usize,
    latest: HashMap<CanIdentity, CachedFrame>,
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
        let first = self.active().first()?.timestamp;
        let last = self.active().last()?.timestamp;

        Some((first, last))
    }

    pub fn latest(&self, id: CanIdentity) -> Option<&CachedFrame> {
        self.latest.get(&id)
    }

    pub fn latest_map(&self) -> &HashMap<CanIdentity, CachedFrame> {
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

        batch.sort_by_key(|frame| frame.timestamp);

        let retained_count = self.len();
        let mut merged = Vec::with_capacity(retained_count + batch.len());
        let mut old = std::mem::take(&mut self.frames)
            .into_iter()
            .skip(self.first_index)
            .peekable();
        self.first_index = 0;
        let mut batch = batch.into_iter().peekable();

        while let (Some(existing), Some(incoming)) = (old.peek(), batch.peek()) {
            // Keep existing frames first when timestamps are equal.
            let next_frame = if existing.timestamp <= incoming.timestamp {
                old.next()
            } else {
                batch.next()
            };

            if let Some(frame) = next_frame {
                merged.push(frame);
            }
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

            let target_capacity = self.frames.len().max(1024);
            if self.frames.capacity() > target_capacity.saturating_mul(4) {
                self.frames.shrink_to(target_capacity);
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
        id: CanIdentity,
        signal: &str,
        range: RangeInclusive<Time>,
    ) -> Vec<(Time, f64)> {
        self.frames_in_range(range)
            .iter()
            .filter(|f| f.identity() == id)
            .filter_map(|f| {
                let decoded = f.decoded.as_ref()?;
                let value = decoded.signals.get(signal)?.value.physical;

                Some((f.timestamp, value))
            })
            .collect()
    }
}

#[cfg(test)]
mod tests {
    use crate::{Time, cache, frame};

    fn sample(timestamp: i64, value: u8, extended: bool) -> frame::ParsedFrame {
        frame::ParsedFrame {
            timestamp: Time::from_unix_millis(timestamp),
            kind: frame::FrameKind::Data,
            dlc: 1,
            msg_id: 0x123,
            is_msg_id_extended: extended,
            raw_bytes: vec![value],
            decoded: None,
        }
    }

    #[test]
    fn merge_preserves_equal_timestamp_order_and_latest_after_eviction() {
        let mut cache = cache::RamCache::new();
        cache.push(sample(1, 1, false));
        cache.push(sample(3, 2, false));
        cache.push(sample(3, 3, false));
        cache.push(sample(5, 4, false));

        assert_eq!(cache.evict(Time::from_unix_millis(2)), 1);

        cache.push_batch(vec![
            sample(3, 5, false),
            sample(4, 6, true),
            sample(2, 7, false),
            sample(3, 8, false),
        ]);

        let retained = cache.frames_in_range(Time::from_unix_millis(2)..=Time::from_unix_millis(5));
        let values: Vec<_> = retained.iter().map(|frame| frame.raw_bytes[0]).collect();

        assert_eq!(values, [7, 2, 3, 5, 8, 6, 4]);
        assert_eq!(cache.latest_map().len(), 2);

        let standard = sample(5, 4, false).identity();
        let extended = sample(4, 6, true).identity();

        assert_eq!(
            cache.latest(standard).map(|frame| frame.raw_bytes[0]),
            Some(4)
        );
        assert_eq!(
            cache.latest(extended).map(|frame| frame.raw_bytes[0]),
            Some(6)
        );
        assert_eq!(cache.evict(Time::from_unix_millis(5)), 6);
        assert!(cache.latest(extended).is_none());
        assert!(cache.latest(standard).is_some());
        assert_eq!(cache.evict(Time::from_unix_millis(6)), 1);
        assert!(cache.is_empty());
        assert!(cache.latest_map().is_empty());
    }
}
