//! DaqCLI entry point: offline log parsing and live CAN monitoring.

use clap::Parser as _;
use std::process;

mod parse;
mod watch;

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
    /// Stream live CAN as JSON Lines to stdout and inject CAN frames from stdin
    Watch(watch::WatchArgs),
}

fn main() -> process::ExitCode {
    env_logger::Builder::from_env(env_logger::Env::default().default_filter_or("info")).init();

    match Cli::parse().command {
        Command::Parse(args) => parse::run(args),
        Command::Watch(args) => watch::run(args),
    }
}
