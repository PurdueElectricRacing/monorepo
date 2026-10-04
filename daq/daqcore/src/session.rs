use crate::{ParsedFrame, Time, cache::RamCache, timeline::Timeline};
pub struct Session {
    cache: RamCache,
    timeline: Timeline,
}

impl Session {
    pub fn live(now: Time, window_secs: f64) -> Result<Self, String> {
        let timeline = Timeline::live(now, window_secs)?;

        Ok(Self {
            cache: RamCache::new(),
            timeline,
        })
    }

    pub fn ingest_frame(&mut self, frame: ParsedFrame) -> bool {
        let timestamp = frame.timestamp;
        self.cache.push(frame);

        self.timeline.observe(timestamp)
    }

    pub fn push_batch(&mut self, frames: Vec<ParsedFrame>) {
        let latest = frames.iter().map(|f| f.timestamp).max();
        self.cache.push_batch(frames);

        if let Some(timestamp) = latest {
            self.timeline.observe(timestamp);
        }
    }

    pub fn evict(&mut self) -> usize {
        self.cache.evict(self.timeline.start())
    }

    pub fn reset(&mut self, now: Time) {
        self.cache.clear();
        self.timeline.reset(now);
    }

    pub fn cache(&self) -> &RamCache {
        &self.cache
    }

    pub fn timeline(&self) -> &Timeline {
        &self.timeline
    }

    pub fn timeline_mut(&mut self) -> &mut Timeline {
        &mut self.timeline
    }
}
