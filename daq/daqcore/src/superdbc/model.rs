//! Private, strict wire models. Required nullable fields must be present.
use std::{fmt, marker::PhantomData};

use indexmap::IndexMap;
use serde::{
    Deserialize, Deserializer,
    de::{self, MapAccess, Visitor},
};

pub const SCHEMA_VERSION: &str = "1.1";

#[derive(Deserialize)]
pub struct VersionProbe {
    pub versions: SchemaProbe,
}

#[derive(Deserialize)]
pub struct SchemaProbe {
    pub schema_version: String,
}

#[derive(Deserialize)]
#[serde(deny_unknown_fields)]
pub struct Document {
    pub content_hash: String,
    pub versions: Versions,
    #[serde(deserialize_with = "unique_map")]
    pub buses: IndexMap<String, BusModel>,
}

#[derive(Deserialize)]
#[serde(deny_unknown_fields)]
pub struct Versions {
    pub schema_version: String,
    pub hash: String,
}

#[derive(Deserialize)]
#[serde(deny_unknown_fields)]
pub struct BusModel {
    pub bus_id: u8,
    pub baud_rate: u64,
    pub nodes: Vec<NodeModel>,
    pub messages: Vec<MessageModel>,
}

#[derive(Deserialize)]
#[serde(deny_unknown_fields)]
pub struct NodeModel {
    pub name: String,
    pub is_external: bool,
}

#[derive(Deserialize)]
#[serde(deny_unknown_fields)]
pub struct MessageModel {
    pub id: u32,
    pub is_extended_id: bool,
    pub message_name: String,
    pub transmitter: String,
    pub receivers: Vec<String>,
    pub length_bytes: u8,
    #[serde(deserialize_with = "required_nullable")]
    pub nominal_period_ms: Option<u32>,
    pub priority: u8,
    pub description: String,
    pub signals: Vec<SignalModel>,
}

#[derive(Deserialize)]
#[serde(deny_unknown_fields)]
pub struct SignalModel {
    pub signal_name: String,
    pub description: String,
    pub data_type: String,
    pub raw_type: String,
    pub start_bit: u8,
    pub bit_length: u8,
    pub byte_order: String,
    pub scale: f64,
    pub offset: f64,
    #[serde(deserialize_with = "required_nullable")]
    pub limits: Option<LimitsModel>,
    pub unit: String,
    #[serde(deserialize_with = "required_nullable")]
    pub choices: Option<Choices>,
    #[serde(default)]
    pub display_format: Option<String>,
}

#[derive(Deserialize)]
#[serde(deny_unknown_fields)]
pub struct LimitsModel {
    pub min: f64,
    pub max: f64,
}

#[derive(Deserialize)]
pub struct Choices(#[serde(deserialize_with = "unique_map")] pub IndexMap<String, String>);

fn required_nullable<'de, D, T>(deserializer: D) -> Result<Option<T>, D::Error>
where
    D: Deserializer<'de>,
    T: Deserialize<'de>,
{
    Option::deserialize(deserializer)
}

// A normal map deserializer silently overwrites repeated JSON property names.
// Reject them instead, while preserving document order without serde_json::Value.
fn unique_map<'de, D, T>(deserializer: D) -> Result<IndexMap<String, T>, D::Error>
where
    D: Deserializer<'de>,
    T: Deserialize<'de>,
{
    struct UniqueMap<T>(PhantomData<T>);
    impl<'de, T: Deserialize<'de>> Visitor<'de> for UniqueMap<T> {
        type Value = IndexMap<String, T>;
        fn expecting(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
            f.write_str("an object with unique keys")
        }
        fn visit_map<A: MapAccess<'de>>(self, mut map: A) -> Result<Self::Value, A::Error> {
            let mut result = IndexMap::new();
            while let Some((key, value)) = map.next_entry::<String, T>()? {
                if result.contains_key(&key) {
                    return Err(de::Error::custom(format!("duplicate key {key:?}")));
                }

                result.insert(key, value);
            }

            Ok(result)
        }
    }

    deserializer.deserialize_map(UniqueMap(PhantomData))
}
