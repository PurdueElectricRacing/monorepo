//! Receive-only hardware check; no DBC, flashing, or CAN transmission.
//! cargo run --manifest-path daq/daqcore/Cargo.toml --example cannector_probe -- vcan 5

use daqcore::{can::driver, connection};
use std::{collections::BTreeMap, error, time};

fn main() -> Result<(), Box<dyn error::Error>> {
    let args: Vec<_> = std::env::args().skip(1).collect();
    if args.len() > 2 {
        return Err("usage: cannector_probe [vcan|mcan|scan] [seconds]".into());
    }
    let channel = match args.first().map(String::as_str).unwrap_or("vcan") {
        "vcan" => connection::CannectorChannel::Vcan,
        "mcan" => connection::CannectorChannel::Mcan,
        "scan" => connection::CannectorChannel::Scan,
        _ => return Err("channel must be vcan, mcan or scan".into()),
    };
    let seconds = args
        .get(1)
        .map(|s| s.parse::<u64>())
        .transpose()?
        .unwrap_or(5);
    if !(1..=3600).contains(&seconds) {
        return Err("duration must be 1..3600 seconds".into());
    }

    let source = connection::ConnectionSource::Cannector(channel);
    let mut device = driver::create_driver(&source, None)?;
    println!(
        "Connected: {}. Reading for {seconds}s; no CAN transmission.",
        source.display_name()
    );
    let end = time::Instant::now() + time::Duration::from_secs(seconds);
    let mut counts = BTreeMap::new();
    let mut total = 0;
    while time::Instant::now() < end {
        match device.read_frames() {
            Ok(frames) => {
                for frame in frames {
                    if total < 10 {
                        println!("{} {:02X?}", frame.identity, frame.data);
                    }
                    *counts.entry(frame.identity).or_insert(0_u64) += 1;
                    total += 1;
                }
            }
            Err(driver::DriverError::Timeout) => {}
            Err(error) => return Err(error.into()),
        }
    }
    device.close()?;
    println!("Received {total} frames on {}.", channel.display_name());
    for (identity, count) in counts {
        println!("  {identity}: {count}");
    }
    if total == 0 {
        println!(
            "USB opened, but no CAN frames arrived on this input. Check input selection, transmitter, wiring, power and termination."
        );
    }
    Ok(())
}
