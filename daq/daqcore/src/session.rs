//! Shared per-update orchestration, owned by the UI/main thread; contains no worker/channel.
#[cfg(test)]
use crate::frame;
use crate::{ParsedFrame, Time, cache::RamCache, timeline::Timeline};
pub struct Session {
    cache: RamCache,
    timeline: Timeline,
}
impl Session {
    pub fn live(now: Time, window_secs: f64) -> Self {
        let timeline = Timeline::live(now, window_secs);
        Self {
            cache: RamCache::new(),
            timeline,
        }
    }
    pub fn ingest_frame(&mut self, frame: ParsedFrame) -> bool {
        let timestamp = frame.timestamp;
        if self
            .cache
            .time_span()
            .is_some_and(|(_, latest)| timestamp < latest)
        {
            self.cache.push_batch(vec![frame]);
        } else {
            self.cache.push(frame);
        }
        self.timeline.observe(timestamp)
    }
    pub fn push_batch(&mut self, frames: Vec<ParsedFrame>) {
        let latest = frames.iter().map(|f| f.timestamp).max();
        self.cache.push_batch(frames);
        if let Some(t) = latest {
            self.timeline.observe(t);
        }
    }
    pub fn evict(&mut self) -> usize {
        self.cache.evict(self.timeline.start())
    }
    pub fn reset(&mut self, now: Time) {
        *self = Self::live(now, self.timeline.window_secs());
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
#[cfg(test)]
mod tests {
    use super::*;
    fn f(ms: i64) -> ParsedFrame {
        ParsedFrame {
            kind: frame::FrameKind::Data,
            dlc: 1,
            timestamp: Time::from_unix_millis(ms),
            msg_id: 1,
            is_msg_id_extended: false,
            raw_bytes: vec![],
            decoded: None,
        }
    }
    #[test]
    fn frozen_start_has_no_frame_or_time_limit() {
        let mut s = Session::live(Time::from_unix_millis(0), 30.0);
        s.timeline_mut().set_start(Time::from_unix_millis(0));
        for i in 0..500_001 {
            s.ingest_frame(f(i * 10));
        }
        assert_eq!(s.evict(), 0);
        assert_eq!(s.cache().len(), 500_001);
        assert!(s.timeline().end().unix_millis() > 3_600_000);
        s.timeline_mut()
            .set_start(Time::from_unix_millis(4_000_000));
        assert_eq!(s.evict(), 400_000);
        assert_eq!(s.cache().len(), 100_001);
        s.timeline_mut().set_start(Time::from_unix_millis(0));
        assert!(
            s.cache()
                .frames_in_range(Time::from_unix_millis(0)..=Time::from_unix_millis(10))
                .is_empty()
        );
    }
    #[test]
    fn regressing_clock_is_preserved_and_sorted() {
        let mut s = Session::live(Time::from_unix_millis(0), 30.0);
        s.ingest_frame(f(20));
        s.ingest_frame(f(10));
        assert_eq!(
            s.cache().time_span(),
            Some((Time::from_unix_millis(10), Time::from_unix_millis(20)))
        );
        assert_eq!(
            s.cache().latest(1).unwrap().timestamp,
            Time::from_unix_millis(20)
        );
    }
    #[test]
    fn frozen_end_limits_viewing_but_does_not_stop_capture() {
        let mut session = Session::live(Time::from_unix_millis(0), 30.0);
        session.ingest_frame(f(100));
        session.timeline_mut().set_end(Time::from_unix_millis(100));
        let selected = session.timeline().range();
        session.ingest_frame(f(200));
        session.evict();
        assert_eq!(session.timeline().range(), selected);
        assert_eq!(session.cache().len(), 2);
        assert_eq!(
            session.cache().time_span().unwrap().1,
            Time::from_unix_millis(200)
        );
        assert_eq!(session.cache().frames_in_range(selected).len(), 1);
    }
}
