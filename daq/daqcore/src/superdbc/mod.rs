//! Bus-aware CAN encoding, decoding, and metadata from a SuperDBC artifact.
//!
//! Start with [`database::Database::load`] or [`database::Database::from_json`].
//! Lookups use a bus ID plus a [`crate::frame::CanIdentity`], preserving the
//! standard/extended distinction. Reuse borrowed [`message::Message`] definitions
//! for repeated codec operations; [`decode::DecodedMessage`] results own their
//! contents so they can outlive a database replacement.
//!
//! Physical encoding applies inverse scaling and offset. Raw encoding accepts
//! exact unscaled integers or Float32 values

pub mod database;
pub mod decode;
pub mod encode;
pub mod error;
pub mod message;

mod extract;
mod model;
