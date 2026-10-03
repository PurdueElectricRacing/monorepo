# daqcore

Headless CAN acquisition and a single-owner telemetry library. Default features
are empty. Optional capabilities: `serial`, `udp`, `simulated`, `loopback`, `hil`,
`firmware`, `formatting`. Serial transport details remain inside its adapter.

`spawn_can_thread(config, events)` returns a handle that submits commands and
owns stop/join. Resolve DBC, HIL and log paths before spawning. Consume frame
events by value into `Session::live(now, 30.0, 0.0)`, then call `Session::evict()`
before borrowing the cache for rendering. Operational events remain separate
from frame history. Event-receiver loss and command-channel closure stop the
worker through its cleanup path.

`Time` records Unix milliseconds; protocol and scheduling components receive
monotonic instants. `RamCache` supports inclusive queries and stable sorted
merges. Timeline maintains independently frozen/marching start, end and cursor.
Only start controls eviction; there is no implicit retention limit. Session,
cache and timeline contain no threads, channels or shared-state synchronization.
CAN FD remains raw traffic; its decoding and binary logging are deferred.

Validation:

```sh
cargo test --manifest-path daq/Cargo.toml -p daqcore --no-default-features --locked
cargo test --manifest-path daq/Cargo.toml --workspace --all-features --locked
python3 daq/check_boundaries.py
cargo run --manifest-path daq/Cargo.toml -p daqcore --example profile_session --release --features simulated -- --soak
```

See [the handoff](../../plans/daqcore_rebuild.md) for decisions and hardware checks.
