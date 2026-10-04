//! Three independent tracks, owned by the UI/main thread.
use crate::Time;
use std::ops::RangeInclusive;

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum Track {
    Marching,
    Frozen,
}

#[derive(Debug, Clone)]
pub struct Timeline {
    start: Time,
    end: Time,
    setpoint: Time,
    start_track: Track,
    end_track: Track,
    setpoint_track: Track,
    window_ms: i64,
    latest: Time,
}

fn millis(seconds: f64) -> i64 {
    let milliseconds = (seconds * 1000.0).round();

    milliseconds.min(i64::MAX as f64) as i64
}

impl Timeline {
    pub fn live(now: Time, window_secs: f64) -> Self {
        let window_ms = millis(window_secs);

        Self {
            start: now.offset(-window_ms),
            end: now,
            setpoint: now,
            start_track: Track::Marching,
            end_track: Track::Marching,
            setpoint_track: Track::Marching,
            window_ms,
            latest: now,
        }
    }

    pub fn start(&self) -> Time {
        self.start
    }

    pub fn end(&self) -> Time {
        self.end
    }

    pub fn setpoint(&self) -> Time {
        self.setpoint
    }

    pub fn start_track(&self) -> Track {
        self.start_track
    }

    pub fn end_track(&self) -> Track {
        self.end_track
    }

    pub fn setpoint_track(&self) -> Track {
        self.setpoint_track
    }

    pub fn window_secs(&self) -> f64 {
        self.window_ms as f64 / 1000.0
    }

    pub fn is_live(&self) -> bool {
        self.setpoint_track == Track::Marching && self.end_track == Track::Marching
    }

    pub fn range(&self) -> RangeInclusive<Time> {
        self.start..=self.end
    }

    pub fn setpoint_range(&self) -> RangeInclusive<Time> {
        self.start..=self.setpoint
    }

    pub fn observe(&mut self, latest: Time) -> bool {
        self.latest = self.latest.max(latest);
        self.recompute()
    }

    fn recompute(&mut self) -> bool {
        let before = (self.start, self.end, self.setpoint);

        if self.end_track == Track::Marching {
            self.end = self.end.max(self.latest);
            if self.start_track == Track::Frozen {
                self.end = self.end.max(self.start);
            }

            if self.setpoint_track == Track::Frozen {
                self.end = self.end.max(self.setpoint);
            }
        }

        if self.start_track == Track::Marching {
            self.start = self.end.offset(-self.window_ms);
            if self.setpoint_track == Track::Frozen {
                self.start = self.start.min(self.setpoint);
            }
        }

        if self.setpoint_track == Track::Marching {
            self.setpoint = self.end;
        }

        before != (self.start, self.end, self.setpoint)
    }

    pub fn set_start(&mut self, time: Time) {
        let upper_bound = if self.setpoint_track == Track::Frozen {
            self.setpoint
        } else {
            self.end
        };

        self.start = time.min(upper_bound);
        self.start_track = Track::Frozen;
        self.recompute();
    }

    pub fn set_end(&mut self, time: Time) {
        let mut end = time;

        if self.start_track == Track::Frozen {
            end = end.max(self.start);
        }

        if self.setpoint_track == Track::Frozen {
            end = end.max(self.setpoint);
        }

        self.end = end;
        self.end_track = Track::Frozen;
        self.recompute();
    }

    pub fn set_setpoint(&mut self, time: Time) {
        self.setpoint = time.clamp(self.start, self.end);
        self.setpoint_track = Track::Frozen;
        self.recompute();
    }

    pub fn release_start(&mut self) {
        self.start_track = Track::Marching;
        self.recompute();
    }

    pub fn release_end(&mut self) {
        self.end_track = Track::Marching;
        self.recompute();
    }

    pub fn release_setpoint(&mut self) {
        self.setpoint_track = Track::Marching;
        self.recompute();
    }

    pub fn go_live(&mut self) {
        self.start_track = Track::Marching;
        self.end_track = Track::Marching;
        self.setpoint_track = Track::Marching;
        self.recompute();
    }

    pub fn set_window_secs(&mut self, seconds: f64) {
        self.window_ms = millis(seconds);
        self.recompute();
    }
}
