use crate::frame;
use std::{collections::BTreeMap, time};

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
    pub msg_bytes: Vec<u8>,
    pub is_hil: bool,
}

struct Scheduled {
    frame: frame::CanFrame,
    amount: SendAmount,
    sent: Option<time::Instant>,
}

#[derive(Default)]
pub struct SendTable(BTreeMap<(frame::CanIdentity, u8), Scheduled>);

impl SendTable {
    pub fn add(&mut self, msg: AddSendMessage) -> Result<(), String> {
        let empty_count = matches!(msg.amount, SendAmount::Finite { amount: 0, .. });
        let periodic = !matches!(msg.amount, SendAmount::Once);
        let empty_period = periodic && msg.amount.period() == 0;

        if empty_count || empty_period {
            return Err("send count and period must be positive".into());
        }

        let frame = frame::CanFrame::data(msg.identity, msg.msg_bytes)?;

        // Allow HIL messages to populate slots past slot 0
        // which is reserved for traditional messages sent
        // from Send UI
        if msg.is_hil {
            for i in 1..=u8::MAX {
                if let std::collections::btree_map::Entry::Vacant(e) =
                    self.0.entry((msg.identity, i))
                {
                    e.insert(Scheduled {
                        frame,
                        amount: msg.amount,
                        sent: None,
                    });
                    break;
                }

                if i == u8::MAX {
                    return Err("No more space in send table".to_owned());
                }
            }
        } else {
            self.0.insert(
                (msg.identity, 0),
                Scheduled {
                    frame,
                    amount: msg.amount,
                    sent: None,
                },
            );
        }
        Ok(())
    }

    pub fn delete(&mut self, id: frame::CanIdentity) {
        self.0.remove(&(id, 0));
    }

    pub fn delete_all(&mut self, id: frame::CanIdentity) {
        self.0.retain(|(identity, _), _| *identity != id);
    }

    pub fn delete_hil(&mut self) {
        self.0.retain(|(_, index), _| *index == 0);
    }

    pub fn due(&self, now: time::Instant) -> Vec<((frame::CanIdentity, u8), frame::CanFrame)> {
        self.0
            .iter()
            .filter(|((_, _), scheduled)| {
                let Some(last_sent) = scheduled.sent else {
                    return true;
                };

                let period = time::Duration::from_millis(scheduled.amount.period() as u64);
                let elapsed = now.saturating_duration_since(last_sent);

                elapsed >= period
            })
            .map(|(key, scheduled)| (*key, scheduled.frame.clone()))
            .collect()
    }

    pub fn commit(
        &mut self,
        id: frame::CanIdentity,
        index: u8,
        now: time::Instant,
    ) -> Option<SendAmount> {
        let scheduled = self.0.get_mut(&(id, index))?;
        scheduled.sent = Some(now);
        let amount_left = scheduled.amount.subtract_one();

        if let Some(amount) = amount_left {
            scheduled.amount = amount;
        } else {
            self.0.remove(&(id, index));
        }
        amount_left
    }
}
