//! Bounded stdout delivery, isolated from CAN control and event processing.

use std::{io, sync::mpsc, thread, time};

/// Bounds queued JSON records independently of the worker event queue.
const OUTPUT_QUEUE_CAPACITY: usize = 1_024;
/// A stalled consumer must not prevent CLI shutdown indefinitely.
const OUTPUT_FINISH_TIMEOUT: time::Duration = time::Duration::from_secs(2);

pub struct Output {
    sender: mpsc::SyncSender<Vec<u8>>,
    finished: mpsc::Receiver<io::Result<()>>,
    completion: Option<io::Result<()>>,
}

impl Output {
    pub fn spawn() -> io::Result<Self> {
        let (sender, records) = mpsc::sync_channel::<Vec<u8>>(OUTPUT_QUEUE_CAPACITY);
        let (done, finished) = mpsc::channel();
        thread::Builder::new()
            .name("daqcli-stdout".into())
            .spawn(move || {
                let result = write_records(&records, &mut io::stdout().lock());
                // The monitor may already have exited after an overflow.
                let _ = done.send(result);
            })?;
        Ok(Self {
            sender,
            finished,
            completion: None,
        })
    }

    pub fn enqueue(&mut self, record: Vec<u8>) -> io::Result<()> {
        self.check()?;
        match self.sender.try_send(record) {
            Ok(()) => Ok(()),
            Err(mpsc::TrySendError::Full(_)) => Err(io::Error::new(
                io::ErrorKind::WouldBlock,
                "stdout queue is full; output consumer cannot keep up, capture stopped",
            )),
            Err(mpsc::TrySendError::Disconnected(_)) => self
                .check()
                .and_then(|()| Err(io::Error::other("stdout writer stopped unexpectedly"))),
        }
    }

    pub fn check(&mut self) -> io::Result<()> {
        if let Some(result) = &self.completion {
            return completion_result(result);
        }
        self.completion = match self.finished.try_recv() {
            Ok(result) => Some(result),
            Err(mpsc::TryRecvError::Empty) => return Ok(()),
            Err(mpsc::TryRecvError::Disconnected) => {
                return Err(io::Error::other("stdout writer stopped unexpectedly"));
            }
        };
        match &self.completion {
            Some(result) => completion_result(result),
            None => Ok(()),
        }
    }

    pub fn finish(self) -> io::Result<()> {
        drop(self.sender);
        if let Some(result) = self.completion {
            return result;
        }
        match self.finished.recv_timeout(OUTPUT_FINISH_TIMEOUT) {
            Ok(result) => result,
            Err(mpsc::RecvTimeoutError::Timeout) => Err(io::Error::new(
                io::ErrorKind::TimedOut,
                "stdout did not finish within two seconds; output may be incomplete",
            )),
            Err(mpsc::RecvTimeoutError::Disconnected) => {
                Err(io::Error::other("stdout writer stopped unexpectedly"))
            }
        }
    }
}

fn completion_result(result: &io::Result<()>) -> io::Result<()> {
    match result {
        Ok(()) => Ok(()),
        Err(error) => Err(io::Error::new(error.kind(), error.to_string())),
    }
}

fn write_records(records: &mpsc::Receiver<Vec<u8>>, output: &mut impl io::Write) -> io::Result<()> {
    for record in records {
        output.write_all(&record)?;
        output.flush()?;
    }
    Ok(())
}
