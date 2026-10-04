use crate::firmware::{self, FirmwarePackage, FirmwareProgress, FirmwareUpdater, TickResult};

use std::time::Instant;

#[derive(Default)]
pub struct FirmwareSession(Option<FirmwareUpdater>);

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
        let invalid_image = package.images.iter().any(|image| {
            image.bytes.is_empty() || image.bytes.len() > firmware::protocol::APPLICATION_SLOT_SIZE
        });

        let error = if self.active() {
            Some("another firmware update is running")
        } else if !connected {
            Some("CAN source is disconnected")
        } else if package.images.is_empty() {
            Some("firmware package is empty")
        } else if invalid_image {
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
        let result = match &mut self.0 {
            Some(updater) => updater.tick(now),
            None => TickResult {
                frame: None,
                progress: None,
            },
        };

        self.finish();
        result
    }

    pub fn receive(&mut self, id: u32, data: &[u8], now: Instant) -> Option<FirmwareProgress> {
        let progress = self
            .0
            .as_mut()
            .and_then(|updater| updater.on_response(id, data, now));

        self.finish();
        progress
    }

    fn finish(&mut self) {
        if self.0.as_ref().is_some_and(|u| u.is_finished()) {
            self.0 = None;
        }
    }
}
