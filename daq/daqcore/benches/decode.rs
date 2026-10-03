use daqcore::superdbc::*;
use std::{hint::black_box, time::Instant};
fn main() {
    let db = SuperDbc::from_str(include_str!("../tests/fixtures/superdbc.json")).unwrap();
    let b = db.bus("VCAN").unwrap();
    let old = can_decode::Parser::from_dbc_file(
        &std::path::Path::new(env!("CARGO_MANIFEST_DIR")).join("tests/fixtures/VCAN.dbc"),
    )
    .unwrap();
    let decoder = Decoder::new(&db, b.bus_id).unwrap();
    println!(
        "DecodedFrame: {} bytes",
        std::mem::size_of::<DecodedFrame>()
    );
    for count in [1, 4, 16] {
        let m = b
            .messages
            .iter()
            .find(|m| m.signals.len() == count)
            .unwrap();
        let data = [0u8; 8];
        let iterations = 100_000;
        let start = Instant::now();
        for _ in 0..iterations {
            black_box(
                decoder
                    .decode(black_box(m.id), black_box(&data[..m.length_bytes as usize]))
                    .unwrap(),
            );
        }
        let new = start.elapsed();
        let start = Instant::now();
        for _ in 0..iterations {
            black_box(old.decode_msg(
                black_box(m.id.to_wire_u32()),
                black_box(&data[..m.length_bytes as usize]),
            ));
        }
        let legacy = start.elapsed();
        println!(
            "{} ({count} signals): superdbc {:.0} ns/frame, can_decode {:.0} ns/frame, {:.2}x",
            m.name,
            new.as_nanos() as f64 / iterations as f64,
            legacy.as_nanos() as f64 / iterations as f64,
            legacy.as_secs_f64() / new.as_secs_f64()
        );
    }
}
