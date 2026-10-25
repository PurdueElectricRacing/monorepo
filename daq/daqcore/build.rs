#[path = "build_support/superdbc.rs"]
mod superdbc;

use std::{env, error, path};

fn main() -> Result<(), Box<dyn error::Error>> {
    // Resolve paths from this crate rather than the caller's working directory
    let manifest = path::PathBuf::from(
        env::var_os("CARGO_MANIFEST_DIR").ok_or("Cargo did not set CARGO_MANIFEST_DIR")?,
    );

    // CANpiler writes its schema into the repository's ignored superdbc directory
    let schema = manifest.join("../../superdbc/superdbc.schema.json");

    // The checked-in fallback is used when the schema is missing
    // The fallback is also always refreshed when the schema is present
    let fallback = manifest.join("src/superdbc/model_generated.rs");

    // model.rs always includes this OUT_DIR file, regardless of whether it was
    // generated from the schema or copied from the checked-in fallback
    let output = path::PathBuf::from(env::var_os("OUT_DIR").ok_or("Cargo did not set OUT_DIR")?)
        .join("superdbc_models.rs");

    // Let Cargo rerun generation when its inputs or implementation change.
    println!("cargo::rerun-if-changed=build.rs");
    println!("cargo::rerun-if-changed=build_support");
    println!("cargo::rerun-if-changed={}", schema.display());
    println!("cargo::rerun-if-changed={}", fallback.display());
    // A fresh checkout may have neither the schema nor the superdbc directory
    // Watch the nearest existing directory so Cargo notices them appearing
    let mut directory = schema
        .parent()
        .ok_or("schema path has no parent directory")?;
    while !directory.exists() {
        directory = directory
            .parent()
            .ok_or("schema path has no existing ancestor directory")?;
    }
    println!("cargo::rerun-if-changed={}", directory.display());

    // Only a missing schema permits fallback
    superdbc::build(&schema, &fallback, &output)
        .map_err(|error| format!("SuperDBC model generation failed: {error}").into())
}
