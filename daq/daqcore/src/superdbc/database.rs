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

/// One immutable document containing every configured bus.
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
    pub fn load(path: &Path) -> Result<Self, LoadError> {
        let json = std::fs::read_to_string(path)?;
        let mut database = Self::from_json(&json)?;
        database.path = Some(path.to_owned());
        Ok(database)
    }

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

    pub fn path(&self) -> Option<&Path> {
        self.path.as_deref()
    }

    pub fn content_hash(&self) -> &str {
        &self.content_hash
    }

    pub fn version_hash(&self) -> &str {
        &self.version_hash
    }

    pub fn schema_version(&self) -> &str {
        &self.schema_version
    }

    pub fn buses(&self) -> &[Bus] {
        &self.buses
    }

    pub fn bus(&self, name: &str) -> Option<&Bus> {
        self.bus_names.get(name).map(|&index| &self.buses[index])
    }

    pub fn bus_by_id(&self, bus_id: u8) -> Option<&Bus> {
        self.bus_ids.get(&bus_id).map(|&index| &self.buses[index])
    }

    pub fn messages(&self) -> impl Iterator<Item = &Message> {
        self.buses.iter().flat_map(|bus| &bus.messages)
    }

    pub fn message(&self, bus_id: u8, identity: CanIdentity) -> Option<&Message> {
        self.message_index
            .get(&MessageKey { bus_id, identity })
            .map(|&(bus, message)| &self.buses[bus].messages[message])
    }

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

#[derive(Debug)]
pub struct Bus {
    name: String,
    bus_id: u8,
    baud_rate: u64,
    nodes: Vec<Node>,
    messages: Vec<Message>,
    message_index: HashMap<CanIdentity, usize>,
}

#[derive(Debug, Clone)]
pub struct Node {
    pub name: String,
    pub is_external: bool,
}

impl Bus {
    pub fn name(&self) -> &str {
        &self.name
    }

    pub fn bus_id(&self) -> u8 {
        self.bus_id
    }

    pub fn baud_rate(&self) -> u64 {
        self.baud_rate
    }

    pub fn nodes(&self) -> &[Node] {
        &self.nodes
    }

    pub fn messages(&self) -> &[Message] {
        &self.messages
    }

    pub fn message(&self, identity: CanIdentity) -> Option<&Message> {
        self.message_index
            .get(&identity)
            .map(|&index| &self.messages[index])
    }

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

    pub fn encode(
        &self,
        identity: CanIdentity,
        values: &[(&str, f64)],
    ) -> Result<Vec<u8>, EncodeError> {
        self.resolve_encoder(identity)?.encode(values)
    }

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
