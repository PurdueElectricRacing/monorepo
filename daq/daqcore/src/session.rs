use crate::{cache, frame, time, timeline};
pub struct Session {
    cache: cache::RamCache,
    timeline: timeline::Timeline,
}

impl Session {
    pub fn live(now: time::Time, window_secs: f64) -> Result<Self, String> {
        let timeline = timeline::Timeline::live(now, window_secs)?;

        Ok(Self {
            cache: cache::RamCache::new(),
            timeline,
        })
    }

    pub fn ingest_frame(&mut self, frame: frame::ParsedFrame) -> bool {
        let timestamp = frame.timestamp;
        self.cache.push(frame);

        self.timeline.observe(timestamp)
    }

    pub fn push_batch(&mut self, frames: Vec<frame::ParsedFrame>) {
        let latest = frames.iter().map(|f| f.timestamp).max();
        self.cache.push_batch(frames);

        if let Some(timestamp) = latest {
            self.timeline.observe(timestamp);
        }
    }

    pub fn evict(&mut self) -> usize {
        self.cache.evict(self.timeline.start())
    }

    pub fn reset(&mut self, now: time::Time) {
        self.cache.clear();
        self.timeline.reset(now);
    }

    pub fn cache(&self) -> &cache::RamCache {
        &self.cache
    }

    pub fn timeline(&self) -> &timeline::Timeline {
        &self.timeline
    }

    pub fn timeline_mut(&mut self) -> &mut timeline::Timeline {
        &mut self.timeline
    }
}
