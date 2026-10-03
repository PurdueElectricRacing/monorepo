use daqcore::superdbc::SuperDbc;
use std::{path::PathBuf, process::ExitCode};
const USAGE: &str = "Usage: daqcli <superdbc.json> <logs-directory> <output-directory> [prefix] [slot-0-bus] [slot-1-bus]\nDefaults: prefix=out, slot-0-bus=VCAN, slot-1-bus=MCAN";
fn run(args: &[String]) -> Result<(), Box<dyn std::error::Error>> {
    if !(3..=6).contains(&args.len()) {
        return Err(USAGE.into());
    }
    let db = SuperDbc::load_file(&args[0])?;
    let name0 = args.get(4).map(String::as_str).unwrap_or("VCAN");
    let name1 = args.get(5).map(String::as_str).unwrap_or("MCAN");
    let bus0 = db
        .bus(name0)
        .ok_or_else(|| format!("Unknown slot-0 bus: {name0}"))?
        .bus_id;
    let bus1 = db
        .bus(name1)
        .ok_or_else(|| format!("Unknown slot-1 bus: {name1}"))?
        .bus_id;
    if bus0 == bus1 {
        return Err("Log slots must be bound to different buses".into());
    }
    let prefix = args.get(3).map(String::as_str).unwrap_or("out");
    if prefix.is_empty() || prefix.contains(['/', '\\']) || prefix == "." || prefix == ".." {
        return Err("Prefix must be a nonempty filename component".into());
    }
    daqcore::log_parse::parse_logs_to_tables(
        &PathBuf::from(&args[1]),
        &PathBuf::from(&args[2]),
        prefix,
        &db.bind(bus0).unwrap(),
        name0,
        &db.bind(bus1).unwrap(),
        name1,
    )?;
    println!(
        "Parsed logs with SuperDBC {} into {}",
        db.version_hash(),
        args[2]
    );
    Ok(())
}
fn main() -> ExitCode {
    let args: Vec<_> = std::env::args().skip(1).collect();
    if args.len() == 1 && matches!(args[0].as_str(), "--help" | "-h") {
        println!("{USAGE}");
        return ExitCode::SUCCESS;
    }
    match run(&args) {
        Ok(()) => ExitCode::SUCCESS,
        Err(e) => {
            eprintln!("{e}");
            ExitCode::FAILURE
        }
    }
}
