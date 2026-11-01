//! Loading, validating, and looking up definitions across all buses in one object.

use crate::{frame, superdbc};
use std::{
    collections::{HashMap, HashSet},
    fs, path,
};

/// An owned, validated SuperDBC database containing all of its buses.
///
/// Load a replacement database when switching artifacts at runtime. Only schema
/// version 1.1 is supported; decoded results own their data and remain usable
/// after the database that produced them is dropped.
#[derive(Debug, Clone)]
pub struct Database {
    path: Option<path::PathBuf>,
    content_hash: String,
    version_hash: String,
    schema_version: String,
    buses: Vec<Bus>,
    bus_names: HashMap<String, usize>,
    bus_ids: HashMap<u8, usize>,
    message_index: HashMap<superdbc::message::MessageKey, (usize, usize)>,
}

impl Database {
    /// Read a UTF-8 SuperDBC JSON artifact and compile its definitions.
    ///
    /// Use this for a database stored on disk. The supplied path is retained by
    /// [`Self::path`] without canonicalization; no content hash is verified.
    ///
    /// # Errors
    ///
    /// Returns [`crate::superdbc::error::LoadError::Io`] if reading fails, or [`crate::superdbc::error::LoadError::Parse`] for
    /// invalid JSON, an unsupported schema version, or invalid definitions.
    ///
    /// # Example
    ///
    /// ```no_run
    ///
    /// use std::{error, path};
    ///
    /// # fn main() -> Result<(), Box<dyn error::Error>> {
    /// let database = daqcore::superdbc::database::Database::load(path::Path::new("superdbc.json"))?;
    /// for bus in database.buses() {
    ///     println!("{}: {} messages", bus.name(), bus.messages().len());
    /// }
    /// # Ok(())
    /// # }
    /// ```
    pub fn load(path: &path::Path) -> Result<Self, superdbc::error::LoadError> {
        let json = fs::read_to_string(path)?;
        let mut database = Self::from_json(&json)?;
        database.path = Some(path.to_owned());
        Ok(database)
    }

    /// Parse and compile a complete SuperDBC JSON document in memory.
    ///
    /// Use this for downloaded or embedded artifacts. The database owns all
    /// metadata and retains no reference to `json`; [`Self::path`] returns `None`.
    ///
    /// # Errors
    ///
    /// Returns [`crate::superdbc::error::ParseError::InvalidJson`] for deserialization failures,
    /// [`crate::superdbc::error::ParseError::UnsupportedSchemaVersion`] for a different schema version,
    /// or [`crate::superdbc::error::ParseError::InvalidDefinition`] for invalid metadata or wire layouts.
    pub fn from_json(json: &str) -> Result<Self, superdbc::error::ParseError> {
        // Parse the strict 1.1 document from the original text so bus
        // declaration order is retained.
        let document: superdbc::model::Document = serde_json::from_str(json)?;

        if document.versions.schema_version != superdbc::model::SCHEMA_VERSION {
            return Err(superdbc::error::ParseError::UnsupportedSchemaVersion {
                found: document.versions.schema_version.clone(),
            });
        }
        // Keep definitions in document order; indexes point into the owned vectors
        // so fast lookups do not require duplicate definitions or shared ownership.
        let mut buses = Vec::with_capacity(document.buses.len());
        let mut bus_names = HashMap::with_capacity(document.buses.len());
        let mut bus_ids = HashMap::with_capacity(document.buses.len());
        let mut message_index = HashMap::new();

        for (name, model) in document.buses {
            let name: String = name.into();
            let context = format!("bus {name:?}");
            superdbc::message::nonempty(&name, &context, "bus name")?;
            let bus_id: u8 = superdbc::model::narrow(model.bus_id, &context, "bus_id")?;

            if bus_id > 7 {
                return Err(superdbc::error::ParseError::definition(
                    &context,
                    "bus_id must be 0..=7",
                ));
            }

            let bus_index = buses.len();

            if bus_ids.insert(bus_id, bus_index).is_some() {
                return Err(superdbc::error::ParseError::definition(
                    &context,
                    "duplicate bus_id",
                ));
            }

            bus_names.insert(name.clone(), bus_index);
            let mut nodes = Vec::with_capacity(model.nodes.len());

            for node in model.nodes {
                superdbc::message::nonempty(&node.name, &context, "node name")?;
                nodes.push(Node {
                    name: node.name.into(),
                    is_external: node.is_external,
                });
            }

            let mut messages = Vec::with_capacity(model.messages.len());
            let mut identities = HashMap::with_capacity(model.messages.len());
            let mut names = HashSet::with_capacity(model.messages.len());

            for message in model.messages {
                let message_context = format!("{context} / message {:?}", message.message_name);

                if !names.insert(message.message_name.clone()) {
                    return Err(superdbc::error::ParseError::definition(
                        message_context,
                        "duplicate message name",
                    ));
                }

                let message =
                    superdbc::message::Message::compile(bus_id, message, &message_context)?;
                let message_position = messages.len();

                if identities
                    .insert(message.identity(), message_position)
                    .is_some()
                {
                    return Err(superdbc::error::ParseError::definition(
                        message_context,
                        "duplicate CAN identity",
                    ));
                }

                // A CAN identity is unique only within its bus. The global index
                // includes the bus ID to avoid collisions across networks.
                message_index.insert(message.key(), (bus_index, message_position));
                messages.push(message);
            }

            buses.push(Bus {
                name,
                bus_id,
                baud_rate: model.baud_rate.get(),
                nodes,
                messages,
                message_index: identities,
            });
        }

        // Hashes are metadata supplied by the generator, not load-time checks.
        Ok(Self {
            path: None,
            content_hash: document.content_hash.into(),
            version_hash: document.versions.hash.into(),
            schema_version: document.versions.schema_version,
            buses,
            bus_names,
            bus_ids,
            message_index,
        })
    }

    /// Return the supplied file path for [`Self::load`], or `None` for [`Self::from_json`].
    pub fn path(&self) -> Option<&path::Path> {
        self.path.as_deref()
    }

    /// Return the artifact's content-hash metadata, exactly as supplied.
    pub fn content_hash(&self) -> &str {
        &self.content_hash
    }

    /// Return `versions.hash`, the artifact version metadata supplied by canpiler.
    pub fn version_hash(&self) -> &str {
        &self.version_hash
    }

    /// Return the format version used to interpret this artifact (currently `"1.1"`).
    pub fn schema_version(&self) -> &str {
        &self.schema_version
    }

    /// Borrow all buses in JSON declaration order, without cloning their definitions.
    pub fn buses(&self) -> &[Bus] {
        &self.buses
    }

    /// Look up a bus by its exact, case-sensitive name; return `None` if absent.
    pub fn bus(&self, name: &str) -> Option<&Bus> {
        self.bus_names.get(name).map(|&index| &self.buses[index])
    }

    /// Look up a bus by its numeric ID; return `None` if absent.
    pub fn bus_by_id(&self, bus_id: u8) -> Option<&Bus> {
        self.bus_ids.get(&bus_id).map(|&index| &self.buses[index])
    }

    /// Iterate over borrowed messages in bus declaration order, then message declaration order.
    pub fn messages(&self) -> impl Iterator<Item = &superdbc::message::Message> {
        self.buses.iter().flat_map(|bus| &bus.messages)
    }

    /// Look up a message using both its bus ID and CAN identity.
    pub fn message(
        &self,
        bus_id: u8,
        identity: frame::CanIdentity,
    ) -> Option<&superdbc::message::Message> {
        self.message_by_key(superdbc::message::MessageKey { bus_id, identity })
    }

    /// Resolve a complete key from a decoded message or retained UI selection.
    /// Returns `None` if this database lacks the key.
    pub fn message_by_key(
        &self,
        key: superdbc::message::MessageKey,
    ) -> Option<&superdbc::message::Message> {
        self.message_index
            .get(&key)
            .map(|&(bus, message)| &self.buses[bus].messages[message])
    }

    /// Look up and decode a frame on the specified bus into an owned result.
    ///
    /// `identity` is the numeric 29-bit CAN identifier. `data` must contain
    /// at least the message's declared length and at most eight bytes; trailing
    /// padding is ignored. Scaling and offset are applied to physical values,
    /// while exact raw integers are retained. Non-finite telemetry is accepted.
    /// See [`crate::superdbc::message::Message::decode`] for the numeric and ownership semantics.
    ///
    /// # Errors
    ///
    /// Returns [`crate::superdbc::error::DecodeError::UnknownBus`], [`crate::superdbc::error::DecodeError::UnknownMessage`], or
    /// [`crate::superdbc::error::DecodeError::InvalidPayloadLength`]. JSON errors occur only at load time.
    ///
    /// # Example
    ///
    /// ```no_run
    ///
    /// use std::{error, path};
    ///
    /// # fn main() -> Result<(), Box<dyn error::Error>> {
    /// let database = daqcore::superdbc::database::Database::load(path::Path::new("superdbc.json"))?;
    /// let identity: daqcore::frame::CanIdentity = 0x123;
    /// if let Some(message) = database.message(0, identity) {
    ///     let payload = vec![0; usize::from(message.length_bytes())];
    ///     let decoded = database.decode(0, identity, &payload)?;
    ///     for (name, value) in decoded.iter() {
    ///         println!("{name}: {} ({:?})", value.physical(), value.raw());
    ///     }
    /// }
    /// # Ok(())
    /// # }
    /// ```
    pub fn decode(
        &self,
        bus_id: u8,
        identity: frame::CanIdentity,
        data: &[u8],
    ) -> Result<superdbc::decode::DecodedMessage, superdbc::error::DecodeError> {
        self.bus_by_id(bus_id)
            .ok_or(superdbc::error::DecodeError::UnknownBus { bus_id })?
            .decode(identity, data)
    }

    /// Look up a message on a bus and encode named physical values.
    ///
    /// Each signal must appear exactly once, in any order. Applies inverse
    /// scaling `(physical - offset) / scale`, rounds integer values with ties
    /// away from zero, and returns the declared payload length. Values are never
    /// clamped; physical limits are presentation metadata. For exact large
    /// integers that `f64` cannot represent, use [`Self::encode_raw`].
    ///
    /// # Errors
    ///
    /// Returns an error for an unknown bus or message, unknown/duplicate/missing
    /// signals, non-finite values, or values outside the wire representation.
    /// See [`crate::superdbc::message::Message::encode`] for the conversion details.
    pub fn encode(
        &self,
        bus_id: u8,
        identity: frame::CanIdentity,
        values: &[(&str, f64)],
    ) -> Result<Vec<u8>, superdbc::error::EncodeError> {
        self.bus_by_id(bus_id)
            .ok_or(superdbc::error::EncodeError::UnknownBus { bus_id })?
            .encode(identity, values)
    }

    /// Look up a message on a bus and encode exact, unscaled wire values.
    ///
    /// No scaling or offset is applied. Use this for exact integer counters,
    /// bitfields, or test payloads. Each signal must appear exactly once, in any
    /// order, as an integer or Float32 matching its definition. The result has
    /// the message's declared payload length; physical limits are not enforced.
    ///
    /// # Errors
    ///
    /// Returns an error for an unknown bus or message, unknown/duplicate/missing
    /// signals, mismatched raw types, out-of-range integers, or non-finite Float32
    /// values. See [`crate::superdbc::message::Message::encode_raw`] for details.
    pub fn encode_raw(
        &self,
        bus_id: u8,
        identity: frame::CanIdentity,
        values: &[(&str, superdbc::message::RawValue)],
    ) -> Result<Vec<u8>, superdbc::error::EncodeError> {
        self.bus_by_id(bus_id)
            .ok_or(superdbc::error::EncodeError::UnknownBus { bus_id })?
            .encode_raw(identity, values)
    }
}

/// Definitions and metadata for one bus, borrowed from a [`crate::superdbc::database::Database`].
///
/// Use its lookup and codec methods when the bus is already known, so callers
/// only need to supply the CAN identity rather than repeating the bus ID.
#[derive(Debug, Clone)]
pub struct Bus {
    name: String,
    bus_id: u8,
    baud_rate: u64,
    nodes: Vec<Node>,
    messages: Vec<superdbc::message::Message>,
    message_index: HashMap<frame::CanIdentity, usize>,
}

/// A node declared on a bus, including nodes supplied by external systems.
#[derive(Debug, Clone)]
pub struct Node {
    /// Node name as declared in the artifact.
    pub name: String,
    /// Whether the node is external to the generated firmware system.
    pub is_external: bool,
}

impl Bus {
    /// Return the bus name as declared in the artifact.
    pub fn name(&self) -> &str {
        &self.name
    }

    /// Return the numeric bus ID (0 through 7) used in message lookup keys.
    pub fn bus_id(&self) -> u8 {
        self.bus_id
    }

    /// Return the declared CAN bitrate in bits per second; this does not configure hardware.
    pub fn baud_rate(&self) -> u64 {
        self.baud_rate
    }

    /// Borrow the declared nodes in artifact order.
    pub fn nodes(&self) -> &[Node] {
        &self.nodes
    }

    /// Borrow all message definitions on this bus in declaration order.
    pub fn messages(&self) -> &[superdbc::message::Message] {
        &self.messages
    }

    /// Look up a message on this bus by its numeric 29-bit CAN identity.
    ///
    /// Returns `None` if absent. Each numeric ID appears at most once per bus.
    pub fn message(&self, identity: frame::CanIdentity) -> Option<&superdbc::message::Message> {
        self.message_index
            .get(&identity)
            .map(|&index| &self.messages[index])
    }

    /// Decode a frame on this bus, returning owned names and signal values.
    ///
    /// Accepts the declared payload length through eight bytes and ignores
    /// padding. See [`crate::superdbc::message::Message::decode`] for scaling and raw-value semantics.
    ///
    /// # Errors
    ///
    /// Returns [`crate::superdbc::error::DecodeError::UnknownMessage`] if this bus lacks the identity,
    /// or [`crate::superdbc::error::DecodeError::InvalidPayloadLength`] for a short or oversized payload.
    pub fn decode(
        &self,
        identity: frame::CanIdentity,
        data: &[u8],
    ) -> Result<superdbc::decode::DecodedMessage, superdbc::error::DecodeError> {
        self.message(identity)
            .ok_or(superdbc::error::DecodeError::UnknownMessage {
                key: superdbc::message::MessageKey {
                    bus_id: self.bus_id,
                    identity,
                },
            })?
            .decode(data)
    }

    /// Encode named physical values for a message on this bus.
    ///
    /// Applies inverse scaling and offset, with integer rounding. Supply every
    /// signal exactly once, in any order; the result has the declared payload
    /// length. See [`crate::superdbc::message::Message::encode`] for precision and range details.
    ///
    /// # Errors
    ///
    /// Returns [`crate::superdbc::error::EncodeError::UnknownMessage`] if absent, or an encoding error
    /// for invalid signal names, completeness, or numeric values.
    pub fn encode(
        &self,
        identity: frame::CanIdentity,
        values: &[(&str, f64)],
    ) -> Result<Vec<u8>, superdbc::error::EncodeError> {
        self.resolve_encoder(identity)?.encode(values)
    }

    /// Encode exact raw integers or finite Float32 values for a message on this bus.
    ///
    /// Bypasses scaling and offset. Supply every signal exactly once, in any
    /// order; the result has the declared payload length. See
    /// [`crate::superdbc::message::Message::encode_raw`] for type and range requirements.
    ///
    /// # Errors
    ///
    /// Returns [`crate::superdbc::error::EncodeError::UnknownMessage`] if absent, or an encoding error
    /// for invalid signal names, completeness, raw types, or values.
    pub fn encode_raw(
        &self,
        identity: frame::CanIdentity,
        values: &[(&str, superdbc::message::RawValue)],
    ) -> Result<Vec<u8>, superdbc::error::EncodeError> {
        self.resolve_encoder(identity)?.encode_raw(values)
    }

    fn resolve_encoder(
        &self,
        identity: frame::CanIdentity,
    ) -> Result<&superdbc::message::Message, superdbc::error::EncodeError> {
        self.message(identity)
            .ok_or(superdbc::error::EncodeError::UnknownMessage {
                key: superdbc::message::MessageKey {
                    bus_id: self.bus_id,
                    identity,
                },
            })
    }
}

#[cfg(test)]
mod tests {
    use super::{Database, superdbc};
    use serde_json::json;

    fn artifact(schema_version: &str, is_extended_id: bool) -> serde_json::Value {
        let mut bus = serde_json::Map::new();
        bus.insert("bus_id".into(), json!(0));
        bus.insert("baud_rate".into(), json!(500000));
        bus.insert(
            "nodes".into(),
            json!([{ "name": "ecu_a", "is_external": false }]),
        );
        bus.insert(
            "messages".into(),
            json!([{
                "id": 0x123,
                "is_extended_id": is_extended_id,
                "message_name": "Status",
                "transmitter": "ecu_a",
                "receivers": ["ecu_b"],
                "length_bytes": 1,
                "nominal_period_ms": 10,
                "priority": 1,
                "description": "status frame",
                "signals": [{
                    "signal_name": "Ready",
                    "description": "ready flag",
                    "data_type": "integer",
                    "raw_type": "unsigned",
                    "start_bit": 0,
                    "bit_length": 1,
                    "byte_order": "little_endian",
                    "scale": 1.0,
                    "offset": 0.0,
                    "limits": { "min": 0.0, "max": 1.0 },
                    "unit": "",
                    "choices": null,
                    "display_format": "integer"
                }]
            }]),
        );
        let mut document = serde_json::Map::new();
        document.insert(
            "content_hash".into(),
            json!("0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef"),
        );
        document.insert(
            "versions".into(),
            json!({ "schema_version": schema_version, "hash": "h1" }),
        );
        document.insert("buses".into(), json!({ "CAN0": bus }));
        serde_json::Value::Object(document)
    }

    #[test]
    fn message_framing_is_preserved() -> Result<(), Box<dyn std::error::Error>> {
        for extended in [false, true] {
            let database = Database::from_json(&artifact("1.1", extended).to_string())?;
            let message = database
                .bus("CAN0")
                .and_then(|bus| bus.message(0x123))
                .ok_or("message missing")?;
            assert_eq!(message.is_extended_id(), extended);
        }
        Ok(())
    }

    #[test]
    fn unsupported_schema_version_is_rejected() {
        let result = Database::from_json(&artifact("1.2", false).to_string());
        assert!(matches!(
            result,
            Err(superdbc::error::ParseError::UnsupportedSchemaVersion { .. })
        ));
    }
}
