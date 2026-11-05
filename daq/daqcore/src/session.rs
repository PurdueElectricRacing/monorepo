use crate::{cache, frame, task_names, time, timeline};
pub struct Session {
    cache: cache::RamCache,
    timeline: timeline::Timeline,
    // Name fragments from evicted frames; retained frames are replayed per view.
    task_name_baseline: task_names::TaskNames,
}

impl Session {
    pub fn live(now: time::Time, window_secs: f64) -> Result<Self, String> {
        let timeline = timeline::Timeline::live(now, window_secs)?;

        Ok(Self {
            cache: cache::RamCache::new(),
            timeline,
            task_name_baseline: task_names::TaskNames::default(),
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
        let floor = self.timeline.start();
        for frame in self
            .cache
            .frames_before(floor, self.cache.len())
            .iter()
            .take_while(|frame| frame.timestamp < floor)
        {
            self.task_name_baseline.observe(frame);
        }
        self.cache.evict(floor)
    }

    pub fn reset(&mut self, now: time::Time) {
        self.cache.clear();
        self.timeline.reset(now);
        self.task_name_baseline = task_names::TaskNames::default();
    }

    /// Reconstruct names at the playhead, including fragments older than the window.
    pub fn task_names_at(&self, at: time::Time) -> task_names::TaskNames {
        let mut names = self.task_name_baseline.clone();
        for frame in self.cache.frames_before(at, self.cache.len()) {
            names.observe(frame);
        }
        names
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
