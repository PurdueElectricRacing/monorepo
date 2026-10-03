use serde::{Deserialize, Deserializer};
use std::collections::BTreeMap;

// JSON map keys must not silently overwrite an earlier bus or enum choice.
struct UniqueMap<T>(BTreeMap<String, T>);
impl<'de, T: Deserialize<'de>> Deserialize<'de> for UniqueMap<T> {
    fn deserialize<D: Deserializer<'de>>(d: D) -> Result<Self, D::Error> {
        struct MapVisitor<T>(std::marker::PhantomData<T>);
        impl<'de, T: Deserialize<'de>> serde::de::Visitor<'de> for MapVisitor<T> {
            type Value = UniqueMap<T>;
            fn expecting(&self, f: &mut std::fmt::Formatter) -> std::fmt::Result {
                f.write_str("an object with unique keys")
            }
            fn visit_map<M: serde::de::MapAccess<'de>>(
                self,
                mut map: M,
            ) -> Result<Self::Value, M::Error> {
                let mut values = BTreeMap::new();
                while let Some((key, value)) = map.next_entry::<String, T>()? {
                    if values.insert(key.clone(), value).is_some() {
                        return Err(serde::de::Error::custom(format!("duplicate key {key}")));
                    }
                }
                Ok(UniqueMap(values))
            }
        }
        d.deserialize_map(MapVisitor(std::marker::PhantomData))
    }
}
fn unique_map<'de, D: Deserializer<'de>, T: Deserialize<'de>>(
    d: D,
) -> Result<BTreeMap<String, T>, D::Error> {
    UniqueMap::deserialize(d).map(|map| map.0)
}
fn nullable_choices<'de, D: Deserializer<'de>>(
    d: D,
) -> Result<Option<BTreeMap<String, String>>, D::Error> {
    Option::<UniqueMap<String>>::deserialize(d).map(|map| map.map(|map| map.0))
}

// A custom deserializer makes these nullable fields required, matching the schema.
fn nullable<'de, D: Deserializer<'de>, T: Deserialize<'de>>(d: D) -> Result<Option<T>, D::Error> {
    Option::deserialize(d)
}
#[derive(Deserialize)]
#[serde(deny_unknown_fields)]
pub struct Document {
    pub content_hash: String,
    pub versions: Versions,
    #[serde(deserialize_with = "unique_map")]
    pub buses: BTreeMap<String, Bus>,
}
#[derive(Deserialize)]
#[serde(deny_unknown_fields)]
pub struct Versions {
    pub schema_version: u32,
    pub hash: String,
}
#[derive(Deserialize)]
#[serde(deny_unknown_fields)]
pub struct Bus {
    pub bus_id: u8,
    pub baud_rate: u32,
    pub nodes: Vec<Node>,
    pub messages: Vec<Message>,
}
#[derive(Deserialize)]
#[serde(deny_unknown_fields)]
pub struct Node {
    pub name: String,
    pub is_external: bool,
}
#[derive(Deserialize)]
#[serde(deny_unknown_fields)]
pub struct Message {
    pub id: u32,
    pub is_extended_id: bool,
    pub message_name: String,
    pub transmitter: String,
    pub receivers: Vec<String>,
    pub length_bytes: u8,
    #[serde(deserialize_with = "nullable")]
    pub nominal_period_ms: Option<u32>,
    pub priority: u8,
    pub description: String,
    pub signals: Vec<Signal>,
}
#[derive(Clone, Copy, Debug, PartialEq, Eq, Deserialize)]
#[serde(rename_all = "snake_case")]
pub enum RawType {
    Unsigned,
    Signed,
    Float32,
}
#[derive(Clone, Copy, Debug, PartialEq, Eq, Deserialize)]
#[serde(rename_all = "snake_case")]
pub enum ByteOrder {
    LittleEndian,
    BigEndian,
}
#[derive(Deserialize)]
#[serde(deny_unknown_fields)]
pub struct Limits {
    pub min: f64,
    pub max: f64,
}
#[derive(Deserialize)]
#[serde(deny_unknown_fields)]
pub struct Signal {
    pub signal_name: String,
    pub description: String,
    pub data_type: String,
    pub raw_type: RawType,
    pub start_bit: u8,
    pub bit_length: u8,
    pub byte_order: ByteOrder,
    pub scale: f64,
    pub offset: f64,
    #[serde(deserialize_with = "nullable")]
    pub limits: Option<Limits>,
    pub unit: String,
    #[serde(deserialize_with = "nullable_choices")]
    pub choices: Option<BTreeMap<String, String>>,
}
