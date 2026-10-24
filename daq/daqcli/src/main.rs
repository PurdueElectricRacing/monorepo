use clap::Parser as _;
use std::{path, process};

#[derive(clap::Parser)]
#[command(name = "daqcli", about = "Export DAQ logs as CSV or Parquet")]
struct Cli {
    #[arg(long)]
    logs_dir: path::PathBuf,
    #[arg(long)]
    output_dir: path::PathBuf,
    /// Output format
    #[arg(long, value_enum, default_value = "csv")]
    format: ExportFormat,
    /// CSV filename prefix (unused for Parquet export)
    #[arg(long, default_value = "out")]
    prefix: String,
    #[arg(long)]
    superdbc: path::PathBuf,
}

#[derive(Clone, Copy, clap::ValueEnum)]
enum ExportFormat {
    Csv,
    Parquet,
}

fn main() -> process::ExitCode {
    env_logger::Builder::from_env(env_logger::Env::default().default_filter_or("info")).init();

    let cli = Cli::parse();

    let database = match daqcore::superdbc::database::Database::load(&cli.superdbc) {
        Ok(database) => database,
        Err(error) => {
            log::error!("SuperDBC load failed ({}): {error}", cli.superdbc.display());
            return process::ExitCode::FAILURE;
        }
    };

    let result = match cli.format {
        ExportFormat::Csv => daqcore::log_parse::csv_export::parse_logs_to_tables(
            &cli.logs_dir,
            &cli.output_dir,
            &cli.prefix,
            &database,
        )
        .map_err(|error| error.to_string()),
        ExportFormat::Parquet => daqcore::log_parse::parquet_export::parse_logs_to_parquet(
            &cli.logs_dir,
            &cli.output_dir,
            &database,
        )
        .map(|_| ())
        .map_err(|error| error.to_string()),
    };

    match result {
        Ok(()) => process::ExitCode::SUCCESS,
        Err(e) => {
            log::error!("log parsing failed: {e}");
            process::ExitCode::FAILURE
        }
    }
}
