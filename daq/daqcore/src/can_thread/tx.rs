use crate::frame::CanFrame;
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
    pub msg_id: u32,
    pub is_msg_id_extended: bool,
    pub msg_bytes: Vec<u8>,
}
struct Scheduled {
    frame: CanFrame,
    amount: SendAmount,
    sent: Option<Instant>,
}
#[derive(Default)]
pub(super) struct SendTable(BTreeMap<u32, Scheduled>);
impl SendTable {
    pub fn add(&mut self, msg: AddSendMessage) -> Result<(), String> {
        if matches!(msg.amount, SendAmount::Finite { amount: 0, .. })
            || (!matches!(msg.amount, SendAmount::Once) && msg.amount.period() == 0)
        {
            return Err("send count and period must be positive".into());
        }
        let frame = CanFrame::data(msg.msg_id, msg.is_msg_id_extended, msg.msg_bytes)?;
        self.0.insert(
            msg.msg_id,
            Scheduled {
                frame,
                amount: msg.amount,
                sent: None,
            },
        );
        Ok(())
    }
    pub fn delete(&mut self, id: u32) {
        self.0.remove(&id);
    }
    pub fn due(&self, now: Instant) -> Vec<CanFrame> {
        self.0
            .values()
            .filter(|s| {
                s.sent.is_none_or(|t| {
                    now.saturating_duration_since(t)
                        >= Duration::from_millis(s.amount.period() as u64)
                })
            })
            .map(|s| s.frame.clone())
            .collect()
    }
    pub fn commit(&mut self, id: u32, now: Instant) -> Option<SendAmount> {
        let s = self.0.get_mut(&id)?;
        s.sent = Some(now);
        let left = s.amount.subtract_one();
        if let Some(amount) = left {
            s.amount = amount;
        } else {
            self.0.remove(&id);
        }
        left
    }
}
