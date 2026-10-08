//! Loading, validating, and looking up definitions across all buses in one object.

use std::{
    collections::{HashMap, HashSet},
    path::{Path, PathBuf},
};

use crate::frame::CanIdentity;

use crate::superdbc::{
    decode::DecodedMessage,
    error::{DecodeError, EncodeError, LoadError, ParseError},
    message::{Message, MessageKey, RawValue, nonempty},
    model::{self, Document, VersionProbe},
};

/// An owned, validated SuperDBC database containing all of its buses.
///
/// Load a replacement database when switching artifacts at runtime. Only schema
/// version 1.1 is supported; decoded results own their data and remain usable
/// after the database that produced them is dropped.
#[derive(Debug)]
pub struct Database {
    path: Option<PathBuf>,
    content_hash: String,
    version_hash: String,
    schema_version: String,
    buses: Vec<Bus>,
    bus_names: HashMap<String, usize>,
    bus_ids: HashMap<u8, usize>,
    message_index: HashMap<MessageKey, (usize, usize)>,
}

impl Database {
    /// Read a UTF-8 SuperDBC JSON artifact and compile its definitions.
    ///
    /// Use this for a database stored on disk. The supplied path is retained by
    /// [`Self::path`] without canonicalization; no content hash is verified.
    ///
    /// # Errors
    ///
    /// Returns [`LoadError::Io`] if reading fails, or [`LoadError::Parse`] for
    /// invalid JSON, an unsupported schema version, or invalid definitions.
    ///
    /// # Example
    ///
    /// ```no_run
    /// use std::path::Path;
    /// use daqcore::superdbc::database::Database;
    ///
    /// # fn main() -> Result<(), Box<dyn std::error::Error>> {
    /// let database = Database::load(Path::new("superdbc.json"))?;
    /// for bus in database.buses() {
    ///     println!("{}: {} messages", bus.name(), bus.messages().len());
    /// }
    /// # Ok(())
    /// # }
    /// ```
    pub fn load(path: &Path) -> Result<Self, LoadError> {
        let json = std::fs::read_to_string(path)?;
        let mut database = Self::from_json(&json)?;
        database.path = Some(path.to_owned());
        Ok(database)
    }

    /// Parse and compile a complete SuperDBC JSON document in memory.
    ///
    /// Use this for downloaded or embedded artifacts. The database owns all
    /// metadata and retains no reference to `json`; [`Self::path`] returns `None`.
    /// Only schema version 1.1 is accepted. Unknown fields, missing required
    /// nullable fields, duplicate definitions, and invalid signal layouts are
    /// rejected during loading rather than during encoding or decoding.
    ///
    /// # Errors
    ///
    /// Returns [`ParseError::InvalidJson`] for deserialization failures,
    /// [`ParseError::UnsupportedSchemaVersion`] for a different schema version,
    /// or [`ParseError::InvalidDefinition`] for invalid metadata or wire layouts.
    pub fn from_json(json: &str) -> Result<Self, ParseError> {
        // Probe the version before interpreting fields under the 1.1 schema.
        // Deserialize from the original text to retain order and duplicate keys.
        let probe: VersionProbe = serde_json::from_str(json)?;

        if probe.versions.schema_version != model::SCHEMA_VERSION {
            return Err(ParseError::UnsupportedSchemaVersion {
                found: probe.versions.schema_version,
            });
        }

        let document: Document = serde_json::from_str(json)?;
        // Keep definitions in document order; indexes point into the owned vectors
        // so fast lookups do not require duplicate definitions or shared ownership.
        let mut buses = Vec::with_capacity(document.buses.len());
        let mut bus_names = HashMap::with_capacity(document.buses.len());
        let mut bus_ids = HashMap::with_capacity(document.buses.len());
        let mut message_index = HashMap::new();

        for (name, model) in document.buses {
            let context = format!("bus {name:?}");
            nonempty(&name, &context, "bus name")?;

            if model.bus_id > 7 {
                return Err(ParseError::definition(&context, "bus_id must be 0..=7"));
            }

            if model.baud_rate == 0 {
                return Err(ParseError::definition(
                    &context,
                    "baud_rate must be positive",
                ));
            }

            let bus_index = buses.len();

            if bus_ids.insert(model.bus_id, bus_index).is_some() {
                return Err(ParseError::definition(&context, "duplicate bus_id"));
            }

            bus_names.insert(name.clone(), bus_index);
            let mut nodes = Vec::with_capacity(model.nodes.len());

            for node in model.nodes {
                nonempty(&node.name, &context, "node name")?;
                nodes.push(Node {
                    name: node.name,
                    is_external: node.is_external,
                });
            }

            let mut messages = Vec::with_capacity(model.messages.len());
            let mut identities = HashMap::with_capacity(model.messages.len());
            let mut names = HashSet::with_capacity(model.messages.len());

            for message in model.messages {
                let message_context = format!("{context} / message {:?}", message.message_name);

                if !names.insert(message.message_name.clone()) {
                    return Err(ParseError::definition(
                        message_context,
                        "duplicate message name",
                    ));
                }

                let message = Message::compile(model.bus_id, message, &message_context)?;
                let message_position = messages.len();

                if identities
                    .insert(message.identity(), message_position)
                    .is_some()
                {
                    return Err(ParseError::definition(
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
                bus_id: model.bus_id,
                baud_rate: model.baud_rate,
                nodes,
                messages,
                message_index: identities,
            });
        }

        // Hashes are metadata supplied by the generator, not load-time checks.
        Ok(Self {
            path: None,
            content_hash: document.content_hash,
            version_hash: document.versions.hash,
            schema_version: document.versions.schema_version,
            buses,
            bus_names,
            bus_ids,
            message_index,
        })
    }

    /// Return the supplied file path for [`Self::load`], or `None` for [`Self::from_json`].
    pub fn path(&self) -> Option<&Path> {
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
    pub fn messages(&self) -> impl Iterator<Item = &Message> {
        self.buses.iter().flat_map(|bus| &bus.messages)
    }

    /// Look up a message using both its bus ID and CAN identity.
    pub fn message(&self, bus_id: u8, identity: CanIdentity) -> Option<&Message> {
        self.message_index
            .get(&MessageKey { bus_id, identity })
            .map(|&(bus, message)| &self.buses[bus].messages[message])
    }

    /// Look up and decode a frame on the specified bus into an owned result.
    ///
    /// `identity` includes the standard/extended distinction. `data` must contain
    /// at least the message's declared length and at most eight bytes; trailing
    /// padding is ignored. Scaling and offset are applied to physical values,
    /// while exact raw integers are retained. Non-finite telemetry is accepted.
    /// See [`Message::decode`] for the numeric and ownership semantics.
    ///
    /// # Errors
    ///
    /// Returns [`DecodeError::UnknownBus`], [`DecodeError::UnknownMessage`], or
    /// [`DecodeError::InvalidPayloadLength`]. JSON errors occur only at load time.
    ///
    /// # Example
    ///
    /// ```no_run
    /// use std::path::Path;
    /// use daqcore::frame::CanIdentity;
    /// use daqcore::superdbc::database::Database;
    ///
    /// # fn main() -> Result<(), Box<dyn std::error::Error>> {
    /// let database = Database::load(Path::new("superdbc.json"))?;
    /// let identity = CanIdentity::new(0x123, false)?;
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
        identity: CanIdentity,
        data: &[u8],
    ) -> Result<DecodedMessage, DecodeError> {
        self.bus_by_id(bus_id)
            .ok_or(DecodeError::UnknownBus { bus_id })?
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
    /// See [`Message::encode`] for the conversion details.
    pub fn encode(
        &self,
        bus_id: u8,
        identity: CanIdentity,
        values: &[(&str, f64)],
    ) -> Result<Vec<u8>, EncodeError> {
        self.bus_by_id(bus_id)
            .ok_or(EncodeError::UnknownBus { bus_id })?
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
    /// values. See [`Message::encode_raw`] for details.
    pub fn encode_raw(
        &self,
        bus_id: u8,
        identity: CanIdentity,
        values: &[(&str, RawValue)],
    ) -> Result<Vec<u8>, EncodeError> {
        self.bus_by_id(bus_id)
            .ok_or(EncodeError::UnknownBus { bus_id })?
            .encode_raw(identity, values)
    }
}

/// Definitions and metadata for one bus, borrowed from a [`Database`].
///
/// Use its lookup and codec methods when the bus is already known, so callers
/// only need to supply the CAN identity rather than repeating the bus ID.
#[derive(Debug)]
pub struct Bus {
    name: String,
    bus_id: u8,
    baud_rate: u64,
    nodes: Vec<Node>,
    messages: Vec<Message>,
    message_index: HashMap<CanIdentity, usize>,
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
    pub fn messages(&self) -> &[Message] {
        &self.messages
    }

    /// Look up a message on this bus by its standard or extended identity.
    ///
    /// Returns `None` if absent. The extended flag is part of the identity, so a
    /// standard and extended message can have the same numeric ID.
    pub fn message(&self, identity: CanIdentity) -> Option<&Message> {
        self.message_index
            .get(&identity)
            .map(|&index| &self.messages[index])
    }

    /// Decode a frame on this bus, returning owned names and signal values.
    ///
    /// Accepts the declared payload length through eight bytes and ignores
    /// padding. See [`Message::decode`] for scaling and raw-value semantics.
    ///
    /// # Errors
    ///
    /// Returns [`DecodeError::UnknownMessage`] if this bus lacks the identity,
    /// or [`DecodeError::InvalidPayloadLength`] for a short or oversized payload.
    pub fn decode(
        &self,
        identity: CanIdentity,
        data: &[u8],
    ) -> Result<DecodedMessage, DecodeError> {
        self.message(identity)
            .ok_or(DecodeError::UnknownMessage {
                key: MessageKey {
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
    /// length. See [`Message::encode`] for precision and range details.
    ///
    /// # Errors
    ///
    /// Returns [`EncodeError::UnknownMessage`] if absent, or an encoding error
    /// for invalid signal names, completeness, or numeric values.
    pub fn encode(
        &self,
        identity: CanIdentity,
        values: &[(&str, f64)],
    ) -> Result<Vec<u8>, EncodeError> {
        self.resolve_encoder(identity)?.encode(values)
    }

    /// Encode exact raw integers or finite Float32 values for a message on this bus.
    ///
    /// Bypasses scaling and offset. Supply every signal exactly once, in any
    /// order; the result has the declared payload length. See
    /// [`Message::encode_raw`] for type and range requirements.
    ///
    /// # Errors
    ///
    /// Returns [`EncodeError::UnknownMessage`] if absent, or an encoding error
    /// for invalid signal names, completeness, raw types, or values.
    pub fn encode_raw(
        &self,
        identity: CanIdentity,
        values: &[(&str, RawValue)],
    ) -> Result<Vec<u8>, EncodeError> {
        self.resolve_encoder(identity)?.encode_raw(values)
    }

    fn resolve_encoder(&self, identity: CanIdentity) -> Result<&Message, EncodeError> {
        self.message(identity).ok_or(EncodeError::UnknownMessage {
            key: MessageKey {
                bus_id: self.bus_id,
                identity,
            },
        })
    }
}
