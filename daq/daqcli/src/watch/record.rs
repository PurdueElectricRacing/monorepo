//! JSON Lines records describing received CAN frames.

use daqcore::{frame, superdbc};
use std::io;

/// One JSON Lines record describing a received CAN frame.
///
/// This is an output-only type: only `Serialize` is implemented because
/// consumers read the records as JSON text.
#[derive(serde::Serialize)]
pub struct FrameRecord<'a> {
    /// Host receive time as Unix milliseconds.
    time_ms: i64,
    /// Zero-based position of the frame in this stream.
    sequence: u64,
    /// SuperDBC bus ID; omitted when no SuperDBC is selected.
    #[serde(skip_serializing_if = "Option::is_none")]
    bus_id: Option<u8>,
    /// SuperDBC bus name; omitted when no SuperDBC is selected.
    #[serde(skip_serializing_if = "Option::is_none")]
    bus: Option<&'a str>,
    /// CAN identifier as lowercase hexadecimal with a `0x` prefix.
    id: String,
    remote: bool,
    fd: bool,
    brs: bool,
    dlc: u8,
    /// Payload byte count.
    length: usize,
    /// Payload bytes as lowercase hexadecimal without separators.
    data: String,
    /// SuperDBC message name; omitted when the frame is not defined.
    #[serde(skip_serializing_if = "Option::is_none")]
    message: Option<&'a str>,
    /// SuperDBC transmitting node; omitted when the frame is not defined.
    #[serde(skip_serializing_if = "Option::is_none")]
    tx_node: Option<&'a str>,
    /// Decoded signal values; omitted when the frame is not defined.
    #[serde(skip_serializing_if = "Option::is_none")]
    signals: Option<serde_json::Map<String, serde_json::Value>>,
}

impl<'a> FrameRecord<'a> {
    pub fn from_frame(
        parsed: &'a frame::ParsedFrame,
        sequence: u64,
        bus: Option<(u8, &'a str)>,
    ) -> Self {
        let decoded = parsed.decoded.as_ref();
        let flags = FrameFlags::from_kind(parsed.kind);
        Self {
            time_ms: parsed.timestamp.unix_millis(),
            sequence,
            bus_id: bus.map(|(bus_id, _)| bus_id),
            bus: bus.map(|(_, bus_name)| bus_name),
            id: format!("0x{:x}", parsed.identity),
            remote: flags.remote,
            fd: flags.fd,
            brs: flags.brs,
            dlc: parsed.dlc,
            length: parsed.raw_bytes.len(),
            data: hex_encode(&parsed.raw_bytes),
            message: decoded.map(superdbc::decode::DecodedMessage::name),
            tx_node: decoded.map(superdbc::decode::DecodedMessage::tx_node),
            signals: decoded.map(signal_map),
        }
    }

    /// Writes this record as one compact JSON line.
    pub fn write_json_line(&self, output: &mut impl io::Write) -> io::Result<()> {
        serde_json::to_writer(&mut *output, self)?;
        output.write_all(b"\n")?;
        output.flush()
    }
}

struct FrameFlags {
    remote: bool,
    fd: bool,
    brs: bool,
}

impl FrameFlags {
    fn from_kind(kind: frame::FrameKind) -> Self {
        match kind {
            frame::FrameKind::Data => Self {
                remote: false,
                fd: false,
                brs: false,
            },
            frame::FrameKind::Remote => Self {
                remote: true,
                fd: false,
                brs: false,
            },
            frame::FrameKind::Fd { bit_rate_switched } => Self {
                remote: false,
                fd: true,
                brs: bit_rate_switched,
            },
        }
    }
}

fn signal_map(
    decoded: &superdbc::decode::DecodedMessage,
) -> serde_json::Map<String, serde_json::Value> {
    decoded
        .iter()
        .map(|(name, value)| (name.to_owned(), signal_value(value)))
        .collect()
}

fn signal_value(value: &superdbc::decode::DecodedSignalValue) -> serde_json::Value {
    if let Some(label) = value.enum_label() {
        return serde_json::Value::String(label.to_owned());
    }
    match serde_json::Number::from_f64(value.physical()) {
        Some(number) => serde_json::Value::Number(number),
        // JSON cannot represent non-finite telemetry.
        None => serde_json::Value::Null,
    }
}

fn hex_encode(bytes: &[u8]) -> String {
    const HEXADECIMAL: &[u8; 16] = b"0123456789abcdef";
    let mut encoded = String::with_capacity(bytes.len() * 2);
    for byte in bytes {
        // Nibbles are in 0..=15, so both lookups are in bounds.
        encoded.push(char::from(HEXADECIMAL[usize::from(byte >> 4)]));
        encoded.push(char::from(HEXADECIMAL[usize::from(byte & 0x0f)]));
    }
    encoded
}
