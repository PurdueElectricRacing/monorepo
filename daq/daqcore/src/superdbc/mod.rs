//! Immutable SuperDBC v1 metadata and allocation-free classic CAN decoding.
mod schema;
pub use crate::can::{BusId, CanFrame, MessageId};
pub use schema::{ByteOrder, RawType};
use std::{
    collections::HashSet,
    fmt,
    path::Path,
    sync::{
        Arc,
        atomic::{AtomicU64, Ordering},
    },
};

pub const MAX_SIGNALS: usize = 64;
static NEXT_GENERATION: AtomicU64 = AtomicU64::new(1);
#[derive(Clone, Copy, Debug, PartialEq, Eq, Hash)]
pub struct DbGeneration(u64);

#[derive(Debug)]
pub enum DbError {
    Io(std::io::Error),
    InvalidJson(serde_json::Error),
    UnsupportedSchemaVersion(u32),
    InvalidDefinition { path: String, reason: String },
}
impl fmt::Display for DbError {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        match self {
            Self::Io(e) => write!(f, "database I/O: {e}"),
            Self::InvalidJson(e) => write!(f, "database JSON: {e}"),
            Self::UnsupportedSchemaVersion(v) => write!(f, "unsupported SuperDBC schema {v}"),
            Self::InvalidDefinition { path, reason } => write!(f, "{path}: {reason}"),
        }
    }
}
impl std::error::Error for DbError {
    fn source(&self) -> Option<&(dyn std::error::Error + 'static)> {
        match self {
            Self::Io(e) => Some(e),
            Self::InvalidJson(e) => Some(e),
            _ => None,
        }
    }
}
fn invalid(path: &str, reason: impl ToString) -> DbError {
    DbError::InvalidDefinition {
        path: path.into(),
        reason: reason.to_string(),
    }
}

#[derive(Clone, Debug)]
pub struct SuperDbc {
    inner: Arc<DbInner>,
}
#[derive(Debug)]
struct DbInner {
    buses: Vec<BusDef>,
    bus_indices: [Option<usize>; 8],
    content_hash: String,
    version_hash: String,
    generation: DbGeneration,
}
#[derive(Clone, Debug)]
pub struct NodeDef {
    pub name: String,
    pub is_external: bool,
}
#[derive(Clone, Debug)]
pub struct BusDef {
    pub name: String,
    pub bus_id: BusId,
    pub baud_rate: u32,
    pub nodes: Vec<NodeDef>,
    pub messages: Vec<MessageDef>,
}
#[derive(Clone, Debug)]
pub struct MessageDef {
    pub id: MessageId,
    pub name: String,
    pub transmitter: String,
    pub receivers: Vec<String>,
    pub length_bytes: u8,
    pub nominal_period_ms: Option<u32>,
    pub priority: u8,
    pub description: String,
    pub signals: Vec<SignalDef>,
}
#[derive(Clone, Debug)]
pub struct SignalDef {
    pub name: String,
    pub description: String,
    pub data_type: String,
    pub unit: String,
    pub raw_type: RawType,
    pub start_bit: u8,
    pub bit_length: u8,
    pub byte_order: ByteOrder,
    pub scale: f64,
    pub offset: f64,
    pub limits: Option<(f64, f64)>,
    pub choices: Vec<(i128, String)>,
    positions: Box<[u8]>,
    required_bytes: usize,
}
impl BusDef {
    pub fn message_index(&self, id: MessageId) -> Option<u32> {
        self.messages
            .binary_search_by_key(&id, |m| m.id)
            .ok()
            .map(|i| i as u32)
    }
    pub fn message(&self, id: MessageId) -> Option<&MessageDef> {
        self.messages.get(self.message_index(id)? as usize)
    }
}
impl SuperDbc {
    pub fn load_file(path: impl AsRef<Path>) -> Result<Self, DbError> {
        Self::from_str(&std::fs::read_to_string(path).map_err(DbError::Io)?)
    }
    #[allow(clippy::should_implement_trait)]
    pub fn from_str(json: &str) -> Result<Self, DbError> {
        let doc: schema::Document = serde_json::from_str(json).map_err(DbError::InvalidJson)?;
        if doc.versions.schema_version != 1 {
            return Err(DbError::UnsupportedSchemaVersion(
                doc.versions.schema_version,
            ));
        }
        if doc.content_hash.len() != 64
            || !doc
                .content_hash
                .bytes()
                .all(|b| b.is_ascii_digit() || (b'a'..=b'f').contains(&b))
        {
            return Err(invalid(
                "content_hash",
                "expected 64 lowercase hexadecimal digits",
            ));
        }
        if doc.versions.hash.is_empty() {
            return Err(invalid("versions.hash", "empty name"));
        }
        let mut buses = Vec::new();
        let mut bus_indices = [None; 8];
        for (name, bus) in doc.buses {
            let bp = format!("buses.{name}");
            let id = BusId::new(bus.bus_id).map_err(|e| invalid(&bp, e))?;
            if name.is_empty() || bus.baud_rate == 0 {
                return Err(invalid(&bp, "empty bus name or zero baud rate"));
            }
            if bus_indices[id.raw() as usize].is_some() {
                return Err(invalid(&bp, "duplicate bus ID"));
            }
            let mut nodes = Vec::new();
            let mut node_names = HashSet::new();
            for n in bus.nodes {
                if n.name.is_empty() || !node_names.insert(n.name.clone()) {
                    return Err(invalid(&bp, "empty or duplicate node name"));
                }
                nodes.push(NodeDef {
                    name: n.name,
                    is_external: n.is_external,
                });
            }
            let mut messages = Vec::new();
            let mut names = HashSet::new();
            let mut ids = HashSet::new();
            for m in bus.messages {
                let mp = format!("{bp}.{}", m.message_name);
                let mid =
                    MessageId::from_parts(m.is_extended_id, m.id).map_err(|e| invalid(&mp, e))?;
                if m.message_name.is_empty()
                    || !names.insert(m.message_name.clone())
                    || !ids.insert(mid)
                {
                    return Err(invalid(&mp, "empty message name or duplicate name/ID"));
                }
                if m.length_bytes > 8
                    || m.priority > 5
                    || m.nominal_period_ms == Some(0)
                    || m.signals.len() > MAX_SIGNALS
                {
                    return Err(invalid(
                        &mp,
                        "invalid length, priority, period, or signal count",
                    ));
                }
                if !node_names.contains(&m.transmitter)
                    || m.receivers.iter().any(|n| !node_names.contains(n))
                {
                    return Err(invalid(&mp, "unknown transmitter/receiver node"));
                }
                let mut occupied = 0u64;
                let mut signals = Vec::new();
                let mut signal_names = HashSet::new();
                for s in m.signals {
                    let sp = format!("{mp}.{}", s.signal_name);
                    if s.signal_name.is_empty()
                        || s.data_type.is_empty()
                        || !signal_names.insert(s.signal_name.clone())
                    {
                        return Err(invalid(&sp, "empty type/name or duplicate signal name"));
                    }
                    if s.bit_length == 0 || s.bit_length > 64 || s.start_bit > 63 {
                        return Err(invalid(&sp, "invalid bit range"));
                    }
                    if !s.scale.is_finite() || s.scale == 0.0 || !s.offset.is_finite() {
                        return Err(invalid(
                            &sp,
                            "scale must be finite and nonzero; offset must be finite",
                        ));
                    }
                    if s.raw_type == RawType::Float32
                        && (s.bit_length != 32 || s.choices.as_ref().is_some_and(|c| !c.is_empty()))
                    {
                        return Err(invalid(&sp, "float32 requires 32 bits and no choices"));
                    }
                    let limits = s.limits.map(|l| (l.min, l.max));
                    if limits.is_some_and(|(a, b)| !a.is_finite() || !b.is_finite() || a > b) {
                        return Err(invalid(&sp, "limits must be finite and ordered"));
                    }
                    let mut positions = Vec::new();
                    let mut bit = s.start_bit as usize;
                    let mut footprint = 0u64;
                    for _ in 0..s.bit_length {
                        if bit >= m.length_bytes as usize * 8 {
                            return Err(invalid(&sp, "signal exceeds payload"));
                        }
                        positions.push(bit as u8);
                        footprint |= 1u64 << bit;
                        bit = match s.byte_order {
                            ByteOrder::LittleEndian => bit + 1,
                            ByteOrder::BigEndian => {
                                if bit % 8 == 0 {
                                    bit + 15
                                } else {
                                    bit - 1
                                }
                            }
                        };
                    }
                    if occupied & footprint != 0 {
                        return Err(invalid(&sp, "signals overlap"));
                    }
                    occupied |= footprint;
                    let required_bytes = positions.iter().max().copied().unwrap() as usize / 8 + 1;
                    let (lo, hi) = integer_bounds(s.raw_type, s.bit_length);
                    let mut choices = Vec::new();
                    for (key, label) in s.choices.unwrap_or_default() {
                        let raw = key
                            .parse::<i128>()
                            .map_err(|_| invalid(&sp, "invalid choice key"))?;
                        if raw.to_string() != key || raw < lo || raw > hi {
                            return Err(invalid(
                                &sp,
                                "choice key is noncanonical or outside raw range",
                            ));
                        }
                        choices.push((raw, label));
                    }
                    choices.sort_by_key(|c| c.0);
                    signals.push(SignalDef {
                        name: s.signal_name,
                        description: s.description,
                        data_type: s.data_type,
                        unit: s.unit,
                        raw_type: s.raw_type,
                        start_bit: s.start_bit,
                        bit_length: s.bit_length,
                        byte_order: s.byte_order,
                        scale: s.scale,
                        offset: s.offset,
                        limits,
                        choices,
                        positions: positions.into_boxed_slice(),
                        required_bytes,
                    });
                }
                messages.push(MessageDef {
                    id: mid,
                    name: m.message_name,
                    transmitter: m.transmitter,
                    receivers: m.receivers,
                    length_bytes: m.length_bytes,
                    nominal_period_ms: m.nominal_period_ms,
                    priority: m.priority,
                    description: m.description,
                    signals,
                });
            }
            messages.sort_by_key(|m| m.id);
            bus_indices[id.raw() as usize] = Some(buses.len());
            buses.push(BusDef {
                name,
                bus_id: id,
                baud_rate: bus.baud_rate,
                nodes,
                messages,
            });
        }
        Ok(Self {
            inner: Arc::new(DbInner {
                buses,
                bus_indices,
                content_hash: doc.content_hash,
                version_hash: doc.versions.hash,
                generation: DbGeneration(NEXT_GENERATION.fetch_add(1, Ordering::Relaxed)),
            }),
        })
    }
    pub fn buses(&self) -> &[BusDef] {
        &self.inner.buses
    }
    pub fn bus(&self, name: &str) -> Option<&BusDef> {
        self.buses().iter().find(|b| b.name == name)
    }
    pub fn bus_by_id(&self, id: BusId) -> Option<&BusDef> {
        self.inner.bus_indices[id.raw() as usize].map(|i| &self.inner.buses[i])
    }
    pub fn schema_version(&self) -> u32 {
        1
    }
    pub fn content_hash(&self) -> &str {
        &self.inner.content_hash
    }
    pub fn version_hash(&self) -> &str {
        &self.inner.version_hash
    }
    pub fn generation(&self) -> DbGeneration {
        self.inner.generation
    }
    pub fn bind(&self, bus: BusId) -> Option<BusDatabase> {
        self.bus_by_id(bus)?;
        Some(BusDatabase {
            db: self.clone(),
            bus,
        })
    }
}

/// A cheap owned bus binding used by metadata consumers and log jobs.
#[derive(Clone, Debug)]
pub struct BusDatabase {
    db: SuperDbc,
    bus: BusId,
}
impl BusDatabase {
    pub fn database(&self) -> &SuperDbc {
        &self.db
    }
    pub fn bus_id(&self) -> BusId {
        self.bus
    }
    pub fn bus(&self) -> &BusDef {
        self.db
            .bus_by_id(self.bus)
            .expect("validated immutable bus binding")
    }
    pub fn msg_defs(&self) -> &[MessageDef] {
        &self.bus().messages
    }
    pub fn msg_def(&self, wire: u32) -> Option<&MessageDef> {
        self.bus().message(MessageId::from_wire_u32(wire).ok()?)
    }
    pub fn msg_desc(&self, wire: u32) -> Option<&str> {
        Some(&self.msg_def(wire)?.description)
    }
    pub fn signal_desc(&self, wire: u32, name: &str) -> Option<&str> {
        Some(
            &self
                .msg_def(wire)?
                .signals
                .iter()
                .find(|s| s.name == name)?
                .description,
        )
    }
    pub fn decode(
        &self,
        id: MessageId,
        data: &[u8],
    ) -> Result<Option<DecodedFrame>, crate::can::FrameError> {
        Decoder::new(&self.db, self.bus).unwrap().decode(id, data)
    }
    pub fn view<'a>(&'a self, frame: &'a DecodedFrame) -> Option<DecodedMessage<'a>> {
        if frame.bus != self.bus {
            return None;
        }
        frame.view(&self.db)
    }
}

#[derive(Clone, Copy, Debug, PartialEq)]
pub enum RawValue {
    U64(u64),
    I64(i64),
    F32(f32),
}
fn integer_bounds(ty: RawType, bits: u8) -> (i128, i128) {
    match ty {
        RawType::Signed => (-(1i128 << (bits - 1)), (1i128 << (bits - 1)) - 1),
        _ => (0, (1i128 << bits) - 1),
    }
}
fn mask(bits: u8) -> u64 {
    if bits == 64 {
        u64::MAX
    } else {
        (1u64 << bits) - 1
    }
}
impl SignalDef {
    pub fn raw_value(&self, raw_bits: u64) -> RawValue {
        match self.raw_type {
            RawType::Unsigned => RawValue::U64(raw_bits),
            RawType::Signed => RawValue::I64(
                ((raw_bits << (64 - self.bit_length)) as i64) >> (64 - self.bit_length),
            ),
            RawType::Float32 => RawValue::F32(f32::from_bits(raw_bits as u32)),
        }
    }
    pub fn physical_range(&self) -> (f64, f64) {
        if let Some(l) = self.limits {
            return l;
        }
        if self.raw_type == RawType::Float32 {
            return (-1000.0, 1000.0);
        }
        let (lo, hi) = integer_bounds(self.raw_type, self.bit_length);
        let a = lo as f64 * self.scale + self.offset;
        let b = hi as f64 * self.scale + self.offset;
        if a.is_finite() && b.is_finite() {
            (a.min(b), a.max(b))
        } else {
            (-1000.0, 1000.0)
        }
    }
    pub fn choice_label(&self, raw: i128) -> Option<&str> {
        self.choices
            .binary_search_by_key(&raw, |c| c.0)
            .ok()
            .map(|i| self.choices[i].1.as_str())
    }
}
#[derive(Clone, Copy, Debug, Default)]
pub struct SignalData {
    pub physical: f64,
    pub raw_bits: u64,
}
#[derive(Clone, Copy, Debug)]
pub struct DecodedFrame {
    pub generation: DbGeneration,
    pub bus: BusId,
    pub id: MessageId,
    pub msg_index: u32,
    raw: [u8; 8],
    pub raw_len: u8,
    signals: [SignalData; MAX_SIGNALS],
    present: u64,
    pub n_signals: u8,
}
impl DecodedFrame {
    pub fn data(&self) -> &[u8] {
        &self.raw[..usize::from(self.raw_len).min(8)]
    }
    pub fn signal(&self, index: usize) -> Option<&SignalData> {
        if index < MAX_SIGNALS
            && index < self.n_signals as usize
            && self.present & (1u64 << index) != 0
        {
            Some(&self.signals[index])
        } else {
            None
        }
    }
    pub fn message<'a>(&self, db: &'a SuperDbc) -> Option<&'a MessageDef> {
        if self.generation != db.generation() {
            return None;
        }
        let m = db
            .bus_by_id(self.bus)?
            .messages
            .get(self.msg_index as usize)?;
        (m.id == self.id).then_some(m)
    }
    pub fn view<'a>(&'a self, db: &'a SuperDbc) -> Option<DecodedMessage<'a>> {
        let msg = self.message(db)?;
        Some(DecodedMessage {
            name: &msg.name,
            msg_id: self.id.to_wire_u32(),
            tx_node: &msg.transmitter,
            signals: DecodedSignals { msg, frame: self },
        })
    }
}
pub struct Decoder<'db> {
    db: &'db SuperDbc,
    bus: &'db BusDef,
}
impl<'db> Decoder<'db> {
    pub fn new(db: &'db SuperDbc, bus: BusId) -> Option<Self> {
        Some(Self {
            db,
            bus: db.bus_by_id(bus)?,
        })
    }
    pub fn decode(
        &self,
        id: MessageId,
        raw: &[u8],
    ) -> Result<Option<DecodedFrame>, crate::can::FrameError> {
        let checked = CanFrame::new(id, raw)?;
        let Some(index) = self.bus.message_index(id) else {
            return Ok(None);
        };
        let m = &self.bus.messages[index as usize];
        let mut frame = DecodedFrame {
            generation: self.db.generation(),
            bus: self.bus.bus_id,
            id,
            msg_index: index,
            raw: [0; 8],
            raw_len: checked.len(),
            signals: [SignalData::default(); MAX_SIGNALS],
            present: 0,
            n_signals: m.signals.len() as u8,
        };
        frame.raw[..raw.len()].copy_from_slice(raw);
        for (i, s) in m.signals.iter().enumerate() {
            if s.required_bytes > raw.len() {
                continue;
            }
            let mut bits = 0u64;
            for (j, bit) in s.positions.iter().enumerate() {
                let value = ((raw[*bit as usize / 8] >> (*bit % 8)) & 1) as u64;
                match s.byte_order {
                    ByteOrder::LittleEndian => bits |= value << j,
                    ByteOrder::BigEndian => bits = (bits << 1) | value,
                }
            }
            let numeric = match s.raw_value(bits) {
                RawValue::U64(v) => v as f64,
                RawValue::I64(v) => v as f64,
                RawValue::F32(v) => v as f64,
            };
            frame.signals[i] = SignalData {
                physical: numeric * s.scale + s.offset,
                raw_bits: bits,
            };
            frame.present |= 1u64 << i;
        }
        Ok(Some(frame))
    }
    pub fn decode_frame(
        &self,
        frame: &CanFrame,
    ) -> Result<Option<DecodedFrame>, crate::can::FrameError> {
        if frame.is_remote() || frame.bus.is_some_and(|b| b != self.bus.bus_id) {
            return Ok(None);
        }
        self.decode(frame.id, frame.data())
    }
}

pub struct DecodedMessage<'a> {
    pub name: &'a str,
    pub msg_id: u32,
    pub tx_node: &'a str,
    pub signals: DecodedSignals<'a>,
}
pub struct DecodedSignals<'a> {
    msg: &'a MessageDef,
    frame: &'a DecodedFrame,
}
pub struct DecodedSignal<'a> {
    pub name: &'a str,
    pub unit: &'a str,
    pub definition: &'a SignalDef,
    pub value: DecodedSignalValue<'a>,
}
#[derive(Clone, Copy, Debug)]
pub struct DecodedSignalValue<'a> {
    pub physical: f64,
    pub raw: Option<i128>,
    pub raw_bits: u64,
    pub enum_label: Option<&'a str>,
}
impl DecodedSignalValue<'_> {
    pub fn int_rounded(&self) -> i128 {
        self.raw.unwrap_or_else(|| self.physical.round() as i128)
    }
}
impl<'a> DecodedSignals<'a> {
    pub fn at(&self, i: usize) -> Option<DecodedSignal<'a>> {
        let s = self.msg.signals.get(i)?;
        let value = self.frame.signal(i)?;
        let raw = match s.raw_value(value.raw_bits) {
            RawValue::U64(v) => Some(v as i128),
            RawValue::I64(v) => Some(v as i128),
            RawValue::F32(_) => None,
        };
        Some(DecodedSignal {
            name: &s.name,
            unit: &s.unit,
            definition: s,
            value: DecodedSignalValue {
                physical: value.physical,
                raw,
                raw_bits: value.raw_bits,
                enum_label: raw.and_then(|v| s.choice_label(v)),
            },
        })
    }
    pub fn get(&self, name: &str) -> Option<DecodedSignal<'a>> {
        self.at(self.msg.signals.iter().position(|s| s.name == name)?)
    }
    pub fn iter(&self) -> SignalIter<'a> {
        SignalIter {
            signals: DecodedSignals {
                msg: self.msg,
                frame: self.frame,
            },
            index: 0,
        }
    }
    pub fn values(&self) -> impl Iterator<Item = DecodedSignal<'a>> {
        self.iter().map(|(_, s)| s)
    }
}
pub struct SignalIter<'a> {
    signals: DecodedSignals<'a>,
    index: usize,
}
impl<'a> Iterator for SignalIter<'a> {
    type Item = (&'a str, DecodedSignal<'a>);
    fn next(&mut self) -> Option<Self::Item> {
        while self.index < self.signals.msg.signals.len() {
            let i = self.index;
            self.index += 1;
            if let Some(s) = self.signals.at(i) {
                return Some((s.name, s));
            }
        }
        None
    }
}
impl<'a> IntoIterator for &DecodedSignals<'a> {
    type Item = (&'a str, DecodedSignal<'a>);
    type IntoIter = SignalIter<'a>;
    fn into_iter(self) -> Self::IntoIter {
        self.iter()
    }
}

#[derive(Clone, Copy, Debug)]
pub enum EncodePolicy {
    Reject,
    Clamp,
}
#[derive(Clone, Debug, PartialEq, Eq)]
pub enum EncodeError {
    InvalidDefinition,
    ValueCount { expected: usize, actual: usize },
    NonFinite { signal: usize },
    OutOfRange { signal: usize },
    RawType { signal: usize },
}
impl fmt::Display for EncodeError {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        write!(f, "encode error: {self:?}")
    }
}
impl std::error::Error for EncodeError {}
impl MessageDef {
    // UI selections own clones of metadata. Public descriptive fields may be
    // edited on those clones, so check their layout before using cached positions.
    fn validate_encoding(&self) -> Result<(), EncodeError> {
        if self.length_bytes > 8 || self.signals.len() > MAX_SIGNALS {
            return Err(EncodeError::InvalidDefinition);
        }
        let mut occupied = 0u64;
        for s in &self.signals {
            if !(1..=64).contains(&s.bit_length)
                || s.positions.len() != s.bit_length as usize
                || (s.raw_type == RawType::Float32 && s.bit_length != 32)
                || !s.scale.is_finite()
                || s.scale == 0.0
                || !s.offset.is_finite()
            {
                return Err(EncodeError::InvalidDefinition);
            }
            let mut expected = s.start_bit as usize;
            for &bit in &s.positions {
                if bit as usize != expected
                    || expected >= self.length_bytes as usize * 8
                    || occupied & (1u64 << bit) != 0
                {
                    return Err(EncodeError::InvalidDefinition);
                }
                occupied |= 1u64 << bit;
                expected = match s.byte_order {
                    ByteOrder::LittleEndian => expected + 1,
                    ByteOrder::BigEndian if expected % 8 == 0 => expected + 15,
                    ByteOrder::BigEndian => expected - 1,
                };
            }
        }
        Ok(())
    }

    pub fn encode(&self, values: &[f64], policy: EncodePolicy) -> Result<CanFrame, EncodeError> {
        self.validate_encoding()?;
        if values.len() != self.signals.len() {
            return Err(EncodeError::ValueCount {
                expected: self.signals.len(),
                actual: values.len(),
            });
        }
        let mut bits = [0u64; MAX_SIGNALS];
        for (i, (s, value)) in self.signals.iter().zip(values).enumerate() {
            if !value.is_finite() {
                return Err(EncodeError::NonFinite { signal: i });
            }
            let scaled = (*value - s.offset) / s.scale;
            if !scaled.is_finite() {
                return Err(EncodeError::OutOfRange { signal: i });
            }
            bits[i] = if s.raw_type == RawType::Float32 {
                let v = scaled as f32;
                if !v.is_finite() {
                    return Err(EncodeError::OutOfRange { signal: i });
                }
                v.to_bits() as u64
            } else {
                let (lo, hi) = integer_bounds(s.raw_type, s.bit_length);
                let rounded = scaled.round();
                // Compare integers after conversion: f64 represents u64::MAX as 2^64.
                let raw = rounded as i128;
                if matches!(policy, EncodePolicy::Reject) && (raw < lo || raw > hi) {
                    return Err(EncodeError::OutOfRange { signal: i });
                }
                raw.clamp(lo, hi) as u64 & mask(s.bit_length)
            };
        }
        Ok(self.pack(&bits))
    }
    pub fn encode_raw(&self, values: &[RawValue]) -> Result<CanFrame, EncodeError> {
        self.validate_encoding()?;
        if values.len() != self.signals.len() {
            return Err(EncodeError::ValueCount {
                expected: self.signals.len(),
                actual: values.len(),
            });
        }
        let mut bits = [0u64; MAX_SIGNALS];
        for (i, (s, v)) in self.signals.iter().zip(values).enumerate() {
            bits[i] = match (s.raw_type, *v) {
                (RawType::Unsigned, RawValue::U64(v)) if v <= mask(s.bit_length) => v,
                (RawType::Signed, RawValue::I64(v)) => {
                    let (lo, hi) = integer_bounds(s.raw_type, s.bit_length);
                    if (v as i128) < lo || (v as i128) > hi {
                        return Err(EncodeError::OutOfRange { signal: i });
                    }
                    v as u64 & mask(s.bit_length)
                }
                (RawType::Float32, RawValue::F32(v)) => v.to_bits() as u64,
                (RawType::Unsigned, RawValue::U64(_)) => {
                    return Err(EncodeError::OutOfRange { signal: i });
                }
                _ => return Err(EncodeError::RawType { signal: i }),
            };
        }
        Ok(self.pack(&bits))
    }
    fn pack(&self, values: &[u64; MAX_SIGNALS]) -> CanFrame {
        let mut data = [0u8; 8];
        for (s, v) in self.signals.iter().zip(values) {
            for (j, bit) in s.positions.iter().enumerate() {
                let shift = match s.byte_order {
                    ByteOrder::LittleEndian => j,
                    ByteOrder::BigEndian => s.bit_length as usize - 1 - j,
                };
                data[*bit as usize / 8] |= (((v >> shift) & 1) as u8) << (*bit % 8);
            }
        }
        CanFrame::new(self.id, &data[..self.length_bytes as usize])
            .expect("validated message length")
    }
}
