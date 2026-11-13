use crate::{can, driver, frame, superdbc};
use std::time;

use rand::Rng as _;
use rand::RngExt as _;
use rand::prelude::IndexedRandom as _;
use rand::seq::IteratorRandom as _;

const FRAME_INTERVAL: time::Duration = time::Duration::from_millis(1);

pub struct SimulatedDriver {
    buses: Vec<Vec<superdbc::message::Message>>,
    next: time::Instant,
}

impl SimulatedDriver {
    pub fn new(buses: &[&superdbc::database::Bus]) -> Self {
        let buses = buses.iter().map(|bus| bus.messages().to_vec()).collect();
        Self {
            buses,
            next: time::Instant::now(),
        }
    }

    fn message_frame(
        message: &superdbc::message::Message,
        rng: &mut impl rand::Rng,
    ) -> driver::DriverResult<frame::CanFrame> {
        let identity = message.identity();
        let bus_id = message.key().bus_id;
        let size = usize::from(message.length_bytes());

        if size > can::CLASSIC_CAN_MAX_BYTES {
            return Err(driver::DriverError::Timeout);
        }

        let mut data = vec![0u8; size];
        if message.signals.is_empty() {
            rng.fill_bytes(&mut data);
        } else {
            for signal in &message.signals {
                let bits = Self::signal_bits(signal, rng);
                signal.codec.insert(&mut data, bits);
            }
        }

        frame::CanFrame::data(Some(bus_id), identity, data).map_err(driver::DriverError::Read)
    }

    fn signal_bits(signal: &superdbc::message::SignalDefinition, rng: &mut impl rand::Rng) -> u64 {
        if let Some((raw, _)) = signal.choices().choose(rng) {
            return raw as u64;
        }
        match signal.raw_type() {
            superdbc::message::RawType::Float32 => Self::float_bits(signal, rng),
            _ => Self::integer_bits(signal, rng),
        }
    }

    // Sample a representable raw integer within physical limits when possible
    fn integer_bits(signal: &superdbc::message::SignalDefinition, rng: &mut impl rand::Rng) -> u64 {
        let (mut lower, mut upper) = (signal.codec.min, signal.codec.max);
        if let Some(limits) = signal.limits() {
            let r1 = (limits.min - signal.offset()) / signal.scale();
            let r2 = (limits.max - signal.offset()) / signal.scale();
            let low = r1.min(r2).ceil();
            let high = r1.max(r2).floor();
            // Limits are presentation metadata and may contain no wire value.
            // In that case retain the full representable range.
            if low <= high && low <= upper as f64 && high >= lower as f64 {
                let candidate_lower = lower.max(low as i128);
                let candidate_upper = upper.min(high as i128);
                if candidate_lower <= candidate_upper {
                    lower = candidate_lower;
                    upper = candidate_upper;
                }
            }
        }
        rng.random_range(lower..=upper) as u64
    }

    // Sample a finite raw float within physical limits when possible and return its wire bits
    fn float_bits(signal: &superdbc::message::SignalDefinition, rng: &mut impl rand::Rng) -> u64 {
        let (r1, r2) = match signal.limits() {
            Some(limits) => (
                (limits.min - signal.offset()) / signal.scale(),
                (limits.max - signal.offset()) / signal.scale(),
            ),
            None => (-f64::from(f32::MAX), f64::from(f32::MAX)),
        };
        let low = r1.min(r2).max(-f64::from(f32::MAX));
        let high = r1.max(r2).min(f64::from(f32::MAX));
        let mut lower = low as f32;
        let mut upper = high as f32;
        if f64::from(lower) < low {
            lower = lower.next_up();
        }
        if f64::from(upper) > high {
            upper = upper.next_down();
        }
        if !lower.is_finite() || !upper.is_finite() || lower > upper {
            lower = -f32::MAX;
            upper = f32::MAX;
        }
        let raw = rng.random_range(f64::from(lower)..=f64::from(upper)) as f32;
        u64::from(raw.to_bits())
    }
}

impl driver::Driver for SimulatedDriver {
    fn read_frames(&mut self) -> driver::DriverResult<Vec<frame::CanFrame>> {
        let now = time::Instant::now();
        if now < self.next {
            return Err(driver::DriverError::Timeout);
        }
        self.next += FRAME_INTERVAL;
        let mut rng = rand::rng();

        let frames: Vec<_> = self
            .buses
            .iter()
            .filter_map(|messages| {
                let message = messages.choose(&mut rng)?;
                Some(Self::message_frame(message, &mut rng))
            })
            .collect::<driver::DriverResult<_>>()?;

        if !frames.is_empty() {
            return Ok(frames);
        }

        let identity = rng.random_range(0..=can::STANDARD_ID_MASK);
        let mut data = vec![0u8; can::CLASSIC_CAN_MAX_BYTES];
        rng.fill_bytes(&mut data);
        let frame =
            frame::CanFrame::data(None, identity, data).map_err(driver::DriverError::Read)?;
        Ok(vec![frame])
    }

    fn write_frame(&mut self, _: frame::CanFrameTx) -> driver::DriverResult<()> {
        Ok(())
    }
}
