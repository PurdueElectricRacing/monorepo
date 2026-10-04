use crate::frame::{CanFrame, CanIdentity};
use std::{
    collections::BTreeMap,
    time::{Duration, Instant},
};

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
    pub identity: CanIdentity,
    pub msg_bytes: Vec<u8>,
}

struct Scheduled {
    frame: CanFrame,
    amount: SendAmount,
    sent: Option<Instant>,
}

#[derive(Default)]
pub struct SendTable(BTreeMap<CanIdentity, Scheduled>);

impl SendTable {
    pub fn add(&mut self, msg: AddSendMessage) -> Result<(), String> {
        let empty_count = matches!(msg.amount, SendAmount::Finite { amount: 0, .. });
        let periodic = !matches!(msg.amount, SendAmount::Once);
        let empty_period = periodic && msg.amount.period() == 0;

        if empty_count || empty_period {
            return Err("send count and period must be positive".into());
        }

        let frame = CanFrame::data(msg.identity, msg.msg_bytes)?;

        self.0.insert(
            msg.identity,
            Scheduled {
                frame,
                amount: msg.amount,
                sent: None,
            },
        );
        Ok(())
    }

    pub fn delete(&mut self, id: CanIdentity) {
        self.0.remove(&id);
    }

    pub fn due(&self, now: Instant) -> Vec<CanFrame> {
        self.0
            .values()
            .filter(|scheduled| {
                let Some(last_sent) = scheduled.sent else {
                    return true;
                };

                let period = Duration::from_millis(scheduled.amount.period() as u64);
                let elapsed = now.saturating_duration_since(last_sent);

                elapsed >= period
            })
            .map(|scheduled| scheduled.frame.clone())
            .collect()
    }

    pub fn commit(&mut self, id: CanIdentity, now: Instant) -> Option<SendAmount> {
        let scheduled = self.0.get_mut(&id)?;
        scheduled.sent = Some(now);
        let amount_left = scheduled.amount.subtract_one();

        if let Some(amount) = amount_left {
            scheduled.amount = amount;
        } else {
            self.0.remove(&id);
        }
        amount_left
    }
}
