use crate::can_thread;
use std::sync::mpsc;

/// The worker never waits for a frontend to consume events.
pub enum Events {
    Unbounded(mpsc::Sender<can_thread::CanThreadEvent>),
    Bounded {
        sender: mpsc::SyncSender<can_thread::CanThreadEvent>,
        overflow: mpsc::SyncSender<()>,
    },
}

impl Events {
    /// A missing receiver or full bounded queue stops the worker.
    pub fn emit(&self, event: can_thread::CanThreadEvent) -> bool {
        match self {
            Self::Unbounded(sender) => sender.send(event).is_ok(),
            Self::Bounded { sender, overflow } => emit_bounded(sender, overflow, event),
        }
    }
}

fn emit_bounded(
    sender: &mpsc::SyncSender<can_thread::CanThreadEvent>,
    overflow: &mpsc::SyncSender<()>,
    event: can_thread::CanThreadEvent,
) -> bool {
    match sender.try_send(event) {
        Ok(()) => true,
        Err(mpsc::TrySendError::Disconnected(_)) => false,
        Err(mpsc::TrySendError::Full(_)) => {
            // Separate notification survives a full event queue. Repeated
            // notifications and a closed frontend need no further action.
            let _ = overflow.try_send(());
            false
        }
    }
}
