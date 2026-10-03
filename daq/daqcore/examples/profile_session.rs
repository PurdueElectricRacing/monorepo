//! Headless ingestion/query profile; --soak runs the real simulated worker for ten minutes.
use daqcore::{ParsedFrame, Session, Time};
use std::time::{Duration, Instant};
fn main() {
    if std::env::args().any(|a| a == "--soak") {
        soak();
        return;
    }
    for rate in [200_i64, 5000] {
        let mut session = Session::live(Time::from_unix_millis(0), 30.0, 0.0);
        let started = Instant::now();
        let mut max_tick = Duration::ZERO;
        let mut batches = 0;
        let mut i = 0;
        while i < rate * 600 {
            let tick = Instant::now();
            for _ in 0..(rate / 60).max(1) {
                let ms = i * 1000 / rate;
                session.ingest_frame(ParsedFrame {
                    kind: daqcore::frame::FrameKind::Data,
                    dlc: 8,
                    timestamp: Time::from_unix_millis(ms as i64),
                    msg_id: (i % 100) as u32,
                    is_msg_id_extended: false,
                    raw_bytes: vec![0; 8],
                    decoded: None,
                });
                i += 1;
            }
            session.evict();
            let frames = session
                .cache()
                .frames_in_range(session.timeline().setpoint_range());
            // Representative newest-per-ID borrowed projection (no second persistent index).
            let mut latest = std::collections::BTreeMap::new();
            for f in frames.iter().rev() {
                latest.entry(f.identity()).or_insert(f);
                if latest.len() == session.cache().latest_map().len() {
                    break;
                }
            }
            std::hint::black_box(latest);
            max_tick = max_tick.max(tick.elapsed());
            batches += 1;
        }
        println!(
            "{rate} frames/s, 600 simulated seconds: retained={}, ticks={batches}, elapsed={:?}, max_tick={max_tick:?}",
            session.cache().len(),
            started.elapsed()
        );
    }
}
#[cfg(feature = "simulated")]
fn soak() {
    use daqcore::{
        can_thread::{CanThreadCommand, CanThreadConfig, CanThreadEvent, spawn_can_thread},
        connection::ConnectionSource,
    };
    let (tx, rx) = std::sync::mpsc::channel();
    let mut worker = spawn_can_thread(CanThreadConfig::default(), tx).unwrap();
    assert!(
        worker
            .command(CanThreadCommand::Connect(Some(
                ConnectionSource::Simulated(true, None)
            )))
            .is_ok()
    );
    let mut session = Session::live(Time::now(), 30.0, 0.0);
    let start = Instant::now();
    let mut count = 0;
    while start.elapsed() < Duration::from_secs(600) {
        if let Ok(CanThreadEvent::Frame(frame)) = rx.recv_timeout(Duration::from_millis(20)) {
            session.ingest_frame(frame);
            count += 1;
        }
        session.evict();
    }
    let stopping = Instant::now();
    worker.stop().unwrap();
    println!(
        "10-minute worker soak: frames={count}, retained={}, shutdown={:?}",
        session.cache().len(),
        stopping.elapsed()
    );
    assert!(count > 60_000);
    assert!(session.cache().len() < 10_000);
}
#[cfg(not(feature = "simulated"))]
fn soak() {
    panic!("--soak requires the simulated feature");
}
