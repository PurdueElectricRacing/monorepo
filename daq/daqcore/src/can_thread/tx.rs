use crate::frame;
use std::{
    collections::{BTreeMap, VecDeque},
    time,
};

/// Bounds one-shot frames that are waiting for a connection.
const MAX_PENDING_ONE_SHOT_SENDS: usize = 65_536;

#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum SendAmount {
    Infinite { period: usize },
    Once,
    Finite { amount: usize, period: usize },
}

impl SendAmount {
    pub fn subtract_one(&self) -> Option<Self> {
        match *self {
            Self::Once => None,
            Self::Infinite { .. } => Some(*self),
            Self::Finite { amount, period } if amount > 1 => Some(Self::Finite {
                amount: amount - 1,
                period,
            }),
            _ => None,
        }
    }

    pub fn display(&self) -> String {
        match self {
            Self::Once => "Once".into(),
            Self::Infinite { period } => format!("∞ ({period} ms period)"),
            Self::Finite { amount, period } => format!("{amount} times ({period} ms period)"),
        }
    }

    fn period(self) -> usize {
        match self {
            Self::Once => 0,
            Self::Infinite { period } | Self::Finite { period, .. } => period,
        }
    }
}

pub struct AddSendMessage {
    pub amount: SendAmount,
    pub identity: frame::CanIdentity,
    pub is_extended_id: bool,
    pub msg_bytes: Vec<u8>,
}

struct Scheduled {
    frame: frame::CanFrameTx,
    amount: SendAmount,
    sent: Option<time::Instant>,
}

#[derive(Default)]
pub struct SendTable {
    scheduled: BTreeMap<frame::CanIdentity, Scheduled>,
    one_shot: VecDeque<frame::CanFrameTx>,
}

impl SendTable {
    pub fn add(&mut self, msg: AddSendMessage) -> Result<(), String> {
        let empty_count = matches!(msg.amount, SendAmount::Finite { amount: 0, .. });
        let periodic = !matches!(msg.amount, SendAmount::Once);
        let empty_period = periodic && msg.amount.period() == 0;

        if empty_count || empty_period {
            return Err("send count and period must be positive".into());
        }

        let frame = frame::CanFrameTx::data(msg.identity, msg.is_extended_id, msg.msg_bytes)?;

        self.scheduled.insert(
            msg.identity,
            Scheduled {
                frame,
                amount: msg.amount,
                sent: None,
            },
        );
        Ok(())
    }

    pub fn delete(&mut self, id: frame::CanIdentity) {
        self.scheduled.remove(&id);
    }

    /// Queues a frame that bypasses the identity-keyed periodic table.
    ///
    /// One-shot frames keep insertion order and are not deduplicated.
    pub fn add_one_shot(&mut self, frame: frame::CanFrameTx) -> Result<(), String> {
        if self.one_shot.len() >= MAX_PENDING_ONE_SHOT_SENDS {
            return Err("CAN injection queue is full".into());
        }
        self.one_shot.push_back(frame);
        Ok(())
    }

    /// Borrows the oldest queued one-shot frame.
    pub fn next_one_shot(&self) -> Option<&frame::CanFrameTx> {
        self.one_shot.front()
    }

    /// Removes the oldest queued one-shot frame once it is written.
    pub fn commit_one_shot(&mut self) {
        self.one_shot.pop_front();
    }

    /// Drains the remaining one-shot frames, for example to report them on shutdown.
    pub fn drain_one_shot(&mut self) -> impl Iterator<Item = frame::CanFrameTx> + '_ {
        self.one_shot.drain(..)
    }

    pub fn due(&self, now: time::Instant) -> Vec<frame::CanFrameTx> {
        self.scheduled
            .values()
            .filter(|scheduled| {
                let Some(last_sent) = scheduled.sent else {
                    return true;
                };

                let period = time::Duration::from_millis(scheduled.amount.period() as u64);
                let elapsed = now.saturating_duration_since(last_sent);

                elapsed >= period
            })
            .map(|scheduled| scheduled.frame.clone())
            .collect()
    }

    pub fn commit(&mut self, id: frame::CanIdentity, now: time::Instant) -> Option<SendAmount> {
        let scheduled = self.scheduled.get_mut(&id)?;
        scheduled.sent = Some(now);
        let amount_left = scheduled.amount.subtract_one();

        if let Some(amount) = amount_left {
            scheduled.amount = amount;
        } else {
            self.scheduled.remove(&id);
        }
        amount_left
    }
}
