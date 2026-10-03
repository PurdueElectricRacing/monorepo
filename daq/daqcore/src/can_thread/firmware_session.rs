use crate::firmware;
use crate::firmware::{FirmwarePackage, FirmwareProgress, FirmwareUpdater, TickResult};
use std::time::Instant;
#[derive(Default)]
pub(super) struct FirmwareSession(Option<FirmwareUpdater>);
impl FirmwareSession {
    pub fn active(&self) -> bool {
        self.0.is_some()
    }
    pub fn start(
        &mut self,
        package: FirmwarePackage,
        armed: bool,
        connected: bool,
        now: Instant,
    ) -> FirmwareProgress {
        let error = if self.active() {
            Some("another firmware update is running")
        } else if !connected {
            Some("CAN source is disconnected")
        } else if package.images.is_empty() {
            Some("firmware package is empty")
        } else if package.images.iter().any(|image| {
            image.bytes.is_empty() || image.bytes.len() > firmware::protocol::APPLICATION_SLOT_SIZE
        }) {
            Some("firmware image must fit the 480 KiB application slot")
        } else if armed && package.images.len() != 1 {
            Some("armed updates require exactly one target")
        } else {
            None
        };
        if let Some(error) = error {
            return FirmwareProgress {
                board: String::new(),
                board_index: 0,
                board_count: package.images.len(),
                phase: "failed".into(),
                sent_bytes: 0,
                total_bytes: 0,
                error: Some(error.into()),
            };
        }
        let (updater, progress) = if armed {
            FirmwareUpdater::new_armed(package, now)
        } else {
            FirmwareUpdater::new(package, now)
        };
        self.0 = Some(updater);
        progress
    }
    pub fn cancel(&mut self) -> Option<FirmwareProgress> {
        self.0.take().map(|mut u| u.cancel())
    }
    pub fn tick(&mut self, now: Instant) -> TickResult {
        let result = self.0.as_mut().map(|u| u.tick(now)).unwrap_or(TickResult {
            frame: None,
            progress: None,
        });
        self.finish();
        result
    }
    pub fn receive(&mut self, id: u32, data: &[u8], now: Instant) -> Option<FirmwareProgress> {
        let progress = self.0.as_mut().and_then(|u| u.on_response(id, data, now));
        self.finish();
        progress
    }
    fn finish(&mut self) {
        if self.0.as_ref().is_some_and(|u| u.is_finished()) {
            self.0 = None;
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    fn package() -> FirmwarePackage {
        FirmwarePackage {
            images: vec![firmware::FirmwareImage {
                name: "test".into(),
                bytes: vec![1; 4],
                crc32: 0,
                start_id: 1,
                crc_id: 2,
                jump_id: 3,
                data_id: 4,
                response_id: 5,
            }],
        }
    }
    #[test]
    fn validates_start_and_cancels_armed_updates() {
        let now = Instant::now();
        let mut session = FirmwareSession::default();
        assert!(session.start(package(), false, false, now).error.is_some());
        assert!(!session.active());
        let mut oversized = package();
        oversized.images[0].bytes = vec![0; firmware::protocol::APPLICATION_SLOT_SIZE + 1];
        assert!(session.start(oversized, false, true, now).error.is_some());
        assert!(session.start(package(), true, true, now).error.is_none());
        assert!(session.active());
        assert!(session.start(package(), false, true, now).error.is_some());
        assert!(session.cancel().is_some());
        assert!(!session.active());
        assert!(session.cancel().is_none());
    }
}
