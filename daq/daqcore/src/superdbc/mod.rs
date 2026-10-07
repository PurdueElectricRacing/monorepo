//! Bus-aware SuperDBC 1.1 metadata and classic CAN encoding/decoding.
//!
//! Definitions are validated and compiled when a [`Database`] is loaded. Metadata
//! borrows from that database; decoded messages own their names and values and
//! remain usable after it is replaced or dropped. Hashes are metadata only.
//!
//! ```no_run
//! use daqcore::{frame::CanIdentity, superdbc::{Database, RawValue}};
//! # fn main() -> Result<(), Box<dyn std::error::Error>> {
//! let db = Database::load(std::path::Path::new("superdbc.json"))?;
//! let identity = CanIdentity::new(0x123, false)?;
//! let message = db.message(0, identity).expect("configured message");
//! let bytes = message.encode_raw(&[("counter", RawValue::Integer(42))])?;
//! let decoded = db.decode(0, identity, &bytes)?;
//! assert_eq!(decoded.signal("counter").unwrap().int_rounded(), 42);
//! # Ok(())
//! # }
//! ```

mod database;
mod decode;
mod encode;
mod error;
mod extract;
mod message;
mod model;

pub use crate::superdbc::database::{Bus, Database, Node};
pub use crate::superdbc::decode::{DecodedMessage, DecodedSignalValue};
pub use crate::superdbc::error::{DecodeError, EncodeError, LoadError, ParseError};
pub use crate::superdbc::message::{
    ByteOrder, DisplayFormat, Message, MessageKey, RawType, RawValue, SignalDefinition,
    SignalLimits,
};
