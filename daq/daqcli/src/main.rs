//! DaqCLI entry point.

use clap::Parser as _;
use std::process;

mod parse;

#[derive(clap::Parser)]
#[command(name = "daqcli", about = "DAQ command-line tools")]
struct Cli {
    #[command(subcommand)]
    command: Command,
}

#[derive(clap::Subcommand)]
enum Command {
    /// Parse recorded DAQ logs into CSV tables
    Parse(parse::ParseArgs),
}

fn main() -> process::ExitCode {
    env_logger::Builder::from_env(env_logger::Env::default().default_filter_or("info")).init();

    match Cli::parse().command {
        Command::Parse(args) => parse::run(args),
    }
}
