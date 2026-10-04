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

    /// Append ordered traffic; preserve recorded timestamps by merging regressions.
    pub fn push(&mut self, frame: CachedFrame) {
        let out_of_order = self
            .active()
            .last()
            .is_some_and(|latest| frame.timestamp < latest.timestamp);

        if out_of_order {
            self.push_batch(vec![frame]);
            return;
        }

        self.latest.insert(frame.identity, frame.clone());
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
                .entry(frame.identity)
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
            .filter(|f| f.identity == id)
            .filter_map(|f| {
                let decoded = f.decoded.as_ref()?;
                let value = decoded.signals.get(signal)?.value.physical;

                Some((f.timestamp, value))
            })
            .collect()
    }
}
