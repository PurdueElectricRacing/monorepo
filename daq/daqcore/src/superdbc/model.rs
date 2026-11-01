//! Private generated wire models. Includes the generated schema/deserialization models.

include!(concat!(env!("OUT_DIR"), "/superdbc_models.rs"));

pub use MessageExport as MessageModel;
pub use SignalExport as SignalModel;
pub use SuperDbcExport as Document;

pub fn narrow<T: TryInto<U>, U>(
    value: T,
    context: &str,
    field: &str,
) -> Result<U, super::error::ParseError> {
    value.try_into().map_err(|_| {
        super::error::ParseError::definition(
            context,
            format!("{field} is outside its supported numeric range"),
        )
    })
}

#[derive(serde::Deserialize)]
pub struct VersionProbe {
    pub versions: SchemaProbe,
}

#[derive(serde::Deserialize)]
pub struct SchemaProbe {
    pub schema_version: String,
}
