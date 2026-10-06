use std::path::PathBuf;
use std::process::ExitCode;

use clap::Parser as ClapParser;

#[derive(ClapParser)]
#[command(name = "daqcli", about = "Parse DAQ logs into CSV tables")]
struct Cli {
    #[arg(long)]
    logs_dir: PathBuf,
    #[arg(long)]
    output_dir: PathBuf,
    #[arg(long, default_value = "out")]
    prefix: String,
    #[arg(long)]
    bus0_dbc: PathBuf,
    #[arg(long, default_value = "VCAN")]
    bus0_name: String,
    #[arg(long)]
    bus1_dbc: PathBuf,
    #[arg(long, default_value = "MCAN")]
    bus1_name: String,
}

fn main() -> ExitCode {
    env_logger::Builder::from_env(env_logger::Env::default().default_filter_or("info")).init();

    let cli = Cli::parse();

    let parser_bus_0 = match can_decode::Parser::from_dbc_file(&cli.bus0_dbc) {
        Ok(p) => p,
        Err(e) => {
            log::error!(
                "DBC load failed for bus 0 ({}): {e}",
                cli.bus0_dbc.display()
            );
            return ExitCode::FAILURE;
        }
    };
    let parser_bus_1 = match can_decode::Parser::from_dbc_file(&cli.bus1_dbc) {
        Ok(p) => p,
        Err(e) => {
            log::error!(
                "DBC load failed for bus 1 ({}): {e}",
                cli.bus1_dbc.display()
            );
            return ExitCode::FAILURE;
        }
    };

    match daqcore::log_parse::parse_logs_to_tables(
        &cli.logs_dir,
        &cli.output_dir,
        &cli.prefix,
        &parser_bus_0,
        &cli.bus0_name,
        &parser_bus_1,
        &cli.bus1_name,
    ) {
        Ok(()) => ExitCode::SUCCESS,
        Err(e) => {
            log::error!("log parsing failed: {e}");
            ExitCode::FAILURE
        }
    }
}
