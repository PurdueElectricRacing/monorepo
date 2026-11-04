//! Offline DAQ log parsing into CSV tables.

use daqcore::{log_parse, superdbc};
use std::{path, process};

#[derive(clap::Args)]
pub struct ParseArgs {
    /// Directory containing recorded .log files
    #[arg(long)]
    logs_dir: path::PathBuf,
    /// Directory to write the parsed CSV tables into
    #[arg(long)]
    output_dir: path::PathBuf,
    /// Prefix for the output filenames
    #[arg(long, default_value = "out")]
    prefix: String,
    /// SuperDBC JSON artifact containing the bus definitions
    #[arg(long)]
    superdbc: path::PathBuf,
}

pub fn run(args: ParseArgs) -> process::ExitCode {
    let database = match superdbc::database::Database::load(&args.superdbc) {
        Ok(database) => database,
        Err(error) => {
            log::error!(
                "SuperDBC load failed ({}): {error}",
                args.superdbc.display()
            );
            return process::ExitCode::FAILURE;
        }
    };

    match log_parse::parse_logs_to_tables(&args.logs_dir, &args.output_dir, &args.prefix, &database)
    {
        Ok(()) => process::ExitCode::SUCCESS,
        Err(error) => {
            log::error!("log parsing failed: {error}");
            process::ExitCode::FAILURE
        }
    }
}
