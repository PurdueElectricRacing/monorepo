use crate::{driver, frame};
use std::mem;

#[derive(Default)]
pub struct LoopbackDriver {
    queued: Vec<frame::CanFrame>,
}

impl driver::Driver for LoopbackDriver {
    fn read_frames(&mut self) -> driver::DriverResult<Vec<frame::CanFrame>> {
        if self.queued.is_empty() {
            return Err(driver::DriverError::Timeout);
        }
        Ok(mem::take(&mut self.queued))
    }

    fn write_frame(&mut self, tx_frame: frame::CanFrameTx) -> driver::DriverResult<()> {
        self.queued.push(tx_frame.frame);
        Ok(())
    }
}
