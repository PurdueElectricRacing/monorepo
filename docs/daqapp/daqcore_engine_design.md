# daqcore as the Engine — Architecture Re-scope

Status: **Design proposal** (planning only — no code changes yet).
Branch: `millan/ram_cache`.
Supersedes the ownership model in [`ram_cache_design_v2.md`](ram_cache_design_v2.md) §4.1
("UI thread owns the cache and timeline") while **retaining everything else** from the
synthesis in [`ram_cache_design_comparison.md`](ram_cache_design_comparison.md) §4
(data structures, patches P1–P4, 14-widget migration table, backfill interface).

---

## 0. Binding constraints (user-mandated, verbatim — do not re-litigate)

1. **Wall-clock timestamps.** Timeline timestamps are **wall-clock** (Unix millis UTC,
   `Time(i64)` newtype). The CAN thread applies the corrected timestamp **at parse time**.
   `chrono` is used **only for display labels**, never for ordering, arithmetic, or cache
   keys.
2. **Timeline tracks.** `Timeline` has `start` / `end` / `setpoint`, each with an
   independent `Track { Marching, Frozen }` — **all 9 combinations must be expressible**.
   Marching rules: `end = max(end, latest + follow_offset_secs)`,
   `start = end − window_secs`, `setpoint = end`; invariant
   `start <= setpoint <= end` (enforced by clamping inside setters/observe);
   **never silently move a Frozen field**.
3. **Naive RAM cache.**
   `Vec<CachedFrame>` (time-ascending) + `first_index: usize`
   + `latest: HashMap<(CanBus, u32), CachedFrame>`;
   `push` = append + ordering assert; `push_batch` = sorted merge;
   `evict` floor `max(timeline.start(), now − max_retention)`;
   `CAPACITY = 500_000`.
   Patches P1–P4 plus the Track/Timeline API from the comparison doc are already decided.
   (Note: `Vec` + offset, **not** `VecDeque` — `VecDeque::partition_point` is nightly-only,
   which is exactly why P3 exists.)

---

## 1. Goals & Non-Goals

### Goals

- **`daqcore` becomes the engine.** It owns: the CAN thread, the CAN drivers, the RAM
  cache, the Timeline, the HIL engine, the firmware-update (bootloader) protocol, live log
  writing, and bus-load tracking.
- **Strict, minimal, UI-free API.** No `eframe`/`egui` type anywhere in `daqcore`'s public
  surface. Every type is usable from a single non-UI thread (a CLI, a test, an agent).
- **`daqapp` becomes a thin eframe wrapper.** All non-UI logic is gutted; widgets read
  through the daqcore read API and issue commands through the command channel.
- **`daqcli` reuses daqcore for the full feature set**, pollable and scriptable, with no UI
  assumptions.
- Data flow is strictly:
  `CAN thread (daqcore) → RAM cache + Timeline (daqcore) → strict API → daqapp / daqcli`.

### Non-Goals

- No networking transport for daqcli (remote-over-UDP is a Phase-5+ open question, §9).
- No multi-client correctness guarantees beyond a single client process per engine
  (daqapp or daqcli; not both at once against one engine — see §9 Q5).
- No change to the SLCAN wire protocol, the bootloader wire protocol, the `.log` on-disk
  format, or the `can_decode`/`can-dbc` stack.
- No re-implementation of `log_parse` (offline pipeline stays as-is in
  [`daqcore/src/log_parse/mod.rs`](../../daq/daqcore/src/log_parse/mod.rs:6)).
- No per-widget timelines; one global `Timeline` per engine (settled in
  [`ram_cache_design_v2.md`](ram_cache_design_v2.md:448) §10).
- No SQLite (Phase 5 of the cache plan; out of scope for the engine re-scope itself).

---

## 2. Architecture & Threading Model

```
+----------------------------------------------------------------------------------+
| daqcore (lib)                                                                    |
|                                                                                  |
|  Engine (handle, Clone-able)                                                      |
|   |  cmd_tx:  mpsc::Sender<EngineCommand>            (unbounded)                  |
|   |  rx:      mpsc::Receiver<EngineEvent>            (unbounded)                  |
|   |  cache:   Arc<RwLock<RamCache>>                  (shared read)                |
|   |  timeline:Arc<RwLock<Timeline>>                  (shared read)                |
|   v                                                                                |
|  +-- engine thread (1 std::thread, owned by EngineInner, joined on drop) --------+|
|  |  loop:                                                                         ||
|  |   1. drain cmd_rx          -> connect / disconnect / dbc / send / hil /        ||
|  |                                  firmware / set_timeline / evict-policy        ||
|  |   2. hil_engine.tick() @50ms (was thread.rs HIL_UPDATE_MS)                     ||
|  |   3. send_this_tick()      -> driver.write_frame (SendMsgInfo schedule)        ||
|  |   4. create_driver()       -> on Connect cmd (can/driver.rs create_driver)     ||
|  |   5. firmware_tick() x8 @4ms delay (can/firmware/bootloader.rs)                ||
|  |   6. driver.read_frames()  -> per frame:                                       ||
|  |        timestamp = Time::now()   (CONSTRAINT 1: wall-clock at parse)           ||
|  |        decode with can_decode::Parser                                          ||
|  |        cache.push(CachedFrame)          (write-lock, microsecond hold)         ||
|  |        timeline.observe(Time)           (marching rules, write-lock)           ||
|  |        hil_engine.process_parsed(...)                                     ||
|  |        bus_load_tracker.record_frame(bytes)                                 ||
|  |        daq_logger.log_can2_frame(frame)   (log only bus CAN2 today,           ||
|  |                                              thread.rs:355)                    ||
|  |   7. evict(max(timeline.start(), now - max_retention))  (CONSTRAINT 3)        ||
|  |   8. broadcast events @200ms: BusLoad, FirmwareProgress, Hil, ConnectionState ||
|  +--------------------------------------------------------------------------------+
+----------------------------------------------------------------------------------+
        ^ mpsc (EngineCommand)          ^ mpsc (EngineEvent)          ^ Arc<RwLock> read
        |                               |                             |
   daqapp (eframe UI, 60 fps)      UI command palette /            widgets @60fps +
   - widgets read cache/timeline   sidebar (connect, DBC,           daqcli (any thread,
     behind read-lock              speed, HIL, firmware)            poll at its own rate)
   - issues EngineCommand
```

### Ownership decisions

| Thing | Owner | Shared as |
|---|---|---|
| `Box<dyn Driver>` | engine thread, exclusively | not shared (closed on disconnect) |
| `can_decode::Parser` | engine thread, exclusively | not shared |
| `RamCache` | **memory shared; mutation exclusive to engine thread** | `Arc<RwLock<RamCache>>` |
| `Timeline` | **memory shared; mutation exclusive to engine thread** | `Arc<RwLock<Timeline>>` |
| `HilEngine`, `FirmwareUpdater`, `SendMsgInfo` table, `BusLoadTracker`, `DaqLogger` | engine thread, exclusively | **not shared** — clients see results only via `EngineEvent` |
| `EngineEvent` receiver | client | one `Receiver` per `Engine` handle (handle not `Clone`-able for the event rx; a second handle gets its own subscription — see §9 Q6) |

- Decode stays on the CAN thread, and the timestamp is applied there at parse time
  (replacing `chrono::Local::now()` at [`can/thread.rs`](../../daq/daqapp/src/can/thread.rs:39)
  with `Time::now()`). No frame ever crosses a channel carrying a timestamp: the cache is
  the transport for data, not the mpsc event channel.
- Disconnect semantics (unchanged from today, [`can/thread.rs`](../../daq/daqapp/src/can/thread.rs:121)):
  on `Disconnect`/re-`Connect` the driver is closed, firmware state cancelled, and
  (per the cache handoff) the cache is cleared and a fresh `Timeline` seeded — the engine
  does this inside the thread; clients observe via `EngineEvent::Disconnected` /
  `Reconnected`.
- Engine thread lifecycle: `Engine::spawn(config)` creates everything and returns the
  handle; `Engine::stop()` (or drop) signals a final `EngineCommand::Stop`, joins the
  thread, and flushes the logger.

---

## 3. daqcore Module Layout

Target layout (new files in **bold**; "source" column cites the code being moved):

| New location in `daq/daqcore/src/` | Source (old) | Notes |
|---|---|---|
| `lib.rs` | [`daqcore/src/lib.rs`](../../daq/daqcore/src/lib.rs:1) | re-exports only: `engine::{Engine, EngineConfig, EngineCommand, EngineEvent}`, `timeline::{Time, Timeline, Track}`, `cache::{RamCache, CachedFrame, FramePayload, CAPACITY}`, `connection::{ConnectionSource, CanBusSpeed, CanBus}` |
| **`engine/mod.rs`** | — | `Engine`, `EngineInner` (the moved thread body), `EngineConfig`, handle methods |
| **`engine/engine.rs`** | [`can/thread.rs`](../../daq/daqapp/src/can/thread.rs:107) `start_can_thread` + [`can/state.rs`](../../daq/daqapp/src/can/state.rs:3) `State` | The 421-line loop and the 214-line state struct fold into one `EngineInner` struct + one `run()` loop. All `MsgFromUi`/`MsgFromCan` mpsc traffic becomes `EngineCommand`/`EngineEvent` + the shared cache/timeline (§4). |
| `engine/connection.rs` | [`connection.rs`](../../daq/daqapp/src/connection.rs:2) | `ConnectionSource`, `CanBusSpeed`, and the **new** `CanBus { Vcan, Scan }` enum (derives `Hash, Eq, Clone, Copy, Debug` — it is the first element of the `latest` map key, CONSTRAINT 3). Today the docs cite a `CanBus` at `connection.rs:49`, but the file actually has `CanBusSpeed` at [line 51](../../daq/daqapp/src/connection.rs:51); no `CanBus` type exists in source yet, so this is created here. `CanBusSpeed::to_slcan_bitrate` ([line 83](../../daq/daqapp/src/connection.rs:83)) is the reason `slcan` moves into daqcore deps. |
| `engine/config.rs` | [`settings.rs`](../../daq/daqapp/src/settings.rs:14) (partial) | `EngineConfig` only. The JSON `Settings` struct, `load`/`save`, theme, and `pixels_per_point` **stay in daqapp** (they are UI prefs, not engine state). |
| `engine/formatter.rs` | [`formatter.rs`](../../daq/daqapp/src/formatter.rs:1) | whole file; see §6 decision. |
| `can/mod.rs` | [`daqcore/src/can.rs`](../../daq/daqcore/src/can.rs:1) | existing: `EXTENDED_ID_FLAG`, id-conversion helpers — kept. |
| `can/driver.rs` | [`can/driver.rs`](../../daq/daqapp/src/can/driver.rs:33) | `Driver` trait (lines 33–43), `SerialDriver`, `UdpDriver`, `SimulatedDriver`, `LoopbackDriver`, `create_driver` (line 424), `DriverError`. Feature-gated impls (§5). |
| `can/bus_load.rs` | [`can/bus_load.rs`](../../daq/daqapp/src/can/bus_load.rs:20) | `BusLoadTracker`; its `chrono::DateTime<Local>` deque is the one remaining internal chrono use — acceptable per CONSTRAINT 1 (display/tracking labels, not ordering), but flagged for a `Time` cleanup in Phase 1. |
| `can/logger.rs` | [`can/daq_parser.rs`](../../daq/daqapp/src/can/daq_parser.rs:23) | `DaqLogger` (already depends on `daqcore::log_parse::parse::RawFrame` and `consts`, lines 1–3, 95–99 — the move removes the last daqapp→daqcore upward coupling). On-disk format unchanged. |
| **`can/firmware/bootloader.rs`** | [`can/bootloader.rs`](../../daq/daqapp/src/can/bootloader.rs:33) | `FirmwareUpdater`, `OutboundFrame` (line 17), `TickResult` (line 43) — the state machine driven by the engine loop. |
| **`can/firmware/protocol.rs`** | [`bootloader_protocol.rs`](../../daq/daqapp/src/bootloader_protocol.rs:26) | `FirmwarePackage`, image validation, CRC. Pure data/protocol, no UI. |
| **`cache.rs`** (new) | design in [`ram_cache_design_comparison.md`](ram_cache_design_comparison.md:77) §4 | `RamCache`, `CachedFrame`, `FramePayload`, `push`/`push_batch`/`evict`/`frames_in_range`/`frames_before`/`latest`/`signal_series`. |
| **`timeline.rs`** (new) | design in [`ram_cache_design_v2.md`](ram_cache_design_v2.md:55) §2 | `Time(i64)`, `Track`, `Timeline`, marching `observe`, clamped setters, 9-track matrix. |
| **`hil/mod.rs`, `hil/config.rs`, `hil/engine.rs`, `hil/run.rs`** | [`hil/engine.rs`](../../daq/daqapp/src/hil/engine.rs:3), [`hil/config.rs`](../../daq/daqapp/src/hil/config.rs:43), [`hil/run.rs`](../../daq/daqapp/src/hil/run.rs:44) | whole `hil` module. `HilCommand`, `HilSnapshot`, `HilEngine`, `TestInfo`, `PresetInfo`, `HilRunningTest`. `hil/engine.rs`'s `process_parsed(&ParsedMessage)` (line 101) is re-pointed at daqcore's `CachedFrame`/decoded view instead of `messages::ParsedMessage`. |
| existing `can.rs`, `log_parse/*` | — | unchanged. `log_parse` is the offline pipeline; `log/can/logger.rs` writes exactly what it reads. |

### What is deleted from daqapp

| File | Fate |
|---|---|
| [`messages.rs`](../../daq/daqapp/src/messages.rs:3) | deleted — `MsgFromUi` (9 variants) and `MsgFromCan` (8 variants) become `EngineCommand`/`EngineEvent` in daqcore; `ParsedMessage`/`UnparsedMessage` are replaced by reading `RamCache` directly. |
| [`frozen.rs`](../../daq/daqapp/src/frozen.rs) | deleted — pause == `setpoint` track `Frozen` (CONSTRAINT 2). |
| [`can/thread.rs`](../../daq/daqapp/src/can/thread.rs), [`can/state.rs`](../../daq/daqapp/src/can/state.rs), [`can/driver.rs`](../../daq/daqapp/src/can/driver.rs), [`can/bootloader.rs`](../../daq/daqapp/src/can/bootloader.rs), [`can/bus_load.rs`](../../daq/daqapp/src/can/bus_load.rs), [`can/daq_parser.rs`](../../daq/daqapp/src/can/daq_parser.rs), [`bootloader_protocol.rs`](../../daq/daqapp/src/bootloader_protocol.rs), [`connection.rs`](../../daq/daqapp/src/connection.rs), [`hil/*`](../../daq/daqapp/src/hil/engine.rs), [`formatter.rs`](../../daq/daqapp/src/formatter.rs) | moved to daqcore; daqapp keeps a thin `pub use daqcore::...` shim only if widgets import them by name (prefer direct `daqcore::` imports). |
| [`main.rs`](../../daq/daqapp/src/main.rs:21) | shrinks to: build `Settings` → build `EngineConfig` → `Engine::spawn` → `DAQApp::new(engine, settings, cc)` → eframe run. The two mpsc channels and `start_can_thread` call (lines 21–71) disappear. |

---

## 4. The Public API (exact signatures)

This is the contract daqapp and daqcli are written against. Everything below is
`pub` in `daqcore`; nothing else is.

### 4.1 Types

```rust
// daqcore::timeline  (CONSTRAINT 1 + 2)
#[derive(Clone, Copy, PartialEq, Eq, PartialOrd, Ord, Hash)]
pub struct Time(i64); // Unix millis, UTC
impl Time {
    pub fn now() -> Self;
    pub fn from_unix_millis(ms: i64) -> Self;
    pub fn unix_millis(&self) -> i64;
    pub fn secs(&self) -> f64;               // for axis labels; chrono only at the UI edge
}

#[derive(Clone, Copy, PartialEq, Eq)]
pub enum Track { Marching, Frozen }

#[derive(Clone, Debug)]
pub struct Timeline {
    pub start: Time, pub end: Time, pub setpoint: Time,
    pub start_track: Track, pub end_track: Track, pub setpoint_track: Track,
    pub window_secs: f64,
    pub follow_offset_secs: f64,
    pub max_retention_secs: f64,
}
impl Timeline {
    pub fn new(start: Time, window_secs: f64, follow_offset_secs: f64, max_retention_secs: f64) -> Self;
    // marching rules + invariants, clamped; Frozen fields are never moved (CONSTRAINT 2)
    pub fn observe(&mut self, latest: Time);              // called by engine per batch
    pub fn set_start(&mut self, t: Time);
    pub fn set_end(&mut self, t: Time);
    pub fn set_setpoint(&mut self, t: Time);
    pub fn release_start(&mut self);  pub fn release_end(&mut self);  pub fn release_setpoint(&mut self);
    pub fn set_track(&mut self, field: TimelineField, track: Track); // TimelineField { Start, End, Setpoint }
    pub fn go_live(&mut self);                 // all three Marching, setpoint = latest
    pub fn scrub(&mut self, setpoint: Time);   // set_setpoint + clamp invariants
    pub fn time_span(&self) -> (Time, Time);   // (start, end)
}

// daqcore::cache  (CONSTRAINT 3)
pub const CAPACITY: usize = 500_000;

#[derive(Clone, Copy, PartialEq, Eq, Hash, Debug)]
pub enum CanBus { Vcan, Scan }

#[derive(Clone, Debug)]
pub enum FramePayload {
    Decoded { raw_bytes: Vec<u8>, decoded: can_decode::DecodedMessage },
    Undecoded { raw_bytes: Vec<u8> },
}

#[derive(Clone, Debug)]
pub struct CachedFrame {
    pub timestamp: Time,
    pub bus: CanBus,
    pub msg_id: u32,              // raw id, no ext flag (matches today's ParsedMessage.msg_id)
    pub is_msg_id_extended: bool,
    pub payload: FramePayload,
}

pub struct RamCache {
    frames: Vec<CachedFrame>,
    first_index: usize,
    latest: HashMap<(CanBus, u32), CachedFrame>,
}
impl RamCache {
    pub fn new() -> Self;
    pub fn push(&mut self, frame: CachedFrame);                 // append + ordering assert (P4)
    pub fn push_batch(&mut self, batch: &[CachedFrame]);        // sorted merge (P4, backfill)
    pub fn evict(&mut self, floor: Time);                       // P2: floor = max(start, now - max_retention)
    pub fn len(&self) -> usize;  pub fn is_empty(&self) -> bool;
    pub fn time_span(&self) -> Option<(Time, Time)>;
    pub fn latest(&self) -> &HashMap<(CanBus, u32), CachedFrame>;
    pub fn frames_in_range(&self, range: (Time, Time)) -> &[CachedFrame]; // partition_point, O(log n)
    pub fn frames_before(&self, ts: Time, count: usize) -> &[CachedFrame];
    pub fn signal_series(&self, bus: CanBus, msg_id: u32, signal: &str, range: (Time, Time)) -> Vec<(Time, f64)>;
}
```

> Note vs. the comparison doc: `CachedFrame` adds `is_msg_id_extended` because today's
> `ParsedMessage`/`UnparsedMessage` both carry it ([`messages.rs`](../../daq/daqapp/src/messages.rs:80))
> and the viewer widgets use it to format IDs.

### 4.2 Commands in / events out

```rust
// daqcore::engine
#[derive(Clone, Debug)]
pub struct EngineConfig {
    pub dbc_path: Option<std::path::PathBuf>,      // loaded on connect, like DbcSelected today
    pub source: Option<ConnectionSource>,          // pre-connect like main.rs:40-58
    pub log_folder: std::path::PathBuf,            // DaqLogger folder (thread.rs:111)
    pub window_secs: f64,                          // Timeline defaults
    pub follow_offset_secs: f64,
    pub max_retention_secs: f64,                   // eviction floor term (P2)
    pub formatter_config: Option<String>,          // None = embedded fallback (formatter.rs:138)
    #[cfg(feature = "hil")]
    pub hil_dir: Option<std::path::PathBuf>,       // hil_config/ dir
}

pub struct Engine { /* cmd_tx, rx, cache: Arc<RwLock<RamCache>>, timeline: Arc<RwLock<Timeline>> */ }

impl Engine {
    pub fn spawn(config: EngineConfig) -> Result<Self, EngineError>;
    pub fn stop(&self) -> Result<(), EngineError>;          // signal + join + logger flush
    pub fn command(&self, cmd: EngineCommand) -> Result<(), EngineError>; // mpsc send
    pub fn events(&mut self) -> std::vec::IntoIter<EngineEvent> { /* drain rx non-blocking */ }
    // read access — see §4.4
    pub fn timeline(&self) -> std::sync::RwLockReadGuard<'_, Timeline>;
    pub fn cache(&self) -> std::sync::RwLockReadGuard<'_, RamCache>;
    pub fn frames_in_range(&self, range: (Time, Time)) -> std::sync::RwLockReadGuard<'_, [CachedFrame]>;
    pub fn latest_frame(&self, bus: CanBus, msg_id: u32) -> Option<CachedFrame>; // cloned
    pub fn is_connected(&self) -> bool;  // cached atomic, updated on connect/disconnect
}

#[derive(Clone, Debug)]
pub enum EngineCommand {
    Connect(Option<ConnectionSource>),        // None = current source; replaces Connect in MsgFromUi
    Disconnect,
    DbcSelected(std::path::PathBuf),
    AddSendMessage { msg_id: u32, is_msg_id_extended: bool, data: Vec<u8>, amount: SendAmount },
    DeleteSendMessage { msg_id: u32 },
    UpdateLogFolder(std::path::PathBuf),
    #[cfg(feature = "hil")]
    Hil(HilCommand),
    #[cfg(feature = "firmware")]
    StartFirmwareUpdate(FirmwarePackage),
    #[cfg(feature = "firmware")]
    ArmFirmwareUpdate(FirmwarePackage),
    #[cfg(feature = "firmware")]
    CancelFirmwareUpdate,
    SetTimeline(TimelineCommand),
}

#[derive(Clone, Copy, Debug, PartialEq)]
pub enum TimelineCommand {
    SetStart(Time), SetEnd(Time), SetSetpoint(Time), Scrub(Time),
    ReleaseStart, ReleaseEnd, ReleaseSetpoint,
    SetTrack { field: TimelineField, track: Track },
    GoLive,
}

#[derive(Debug)]
pub enum EngineEvent {
    ConnectionState(ConnectionState),   // enum { Disconnected, Connecting, Connected(ConnectionSource) }
    MessageSent { msg_id: u32, timestamp: Time, amount_left: Option<SendAmount> },
    BusLoad { load_1s: f32, load_5s: f32, load_10s: f32, load_30s: f32 },
    #[cfg(feature = "hil")]
    Hil(HilSnapshot),
    #[cfg(feature = "firmware")]
    FirmwareProgress(FirmwareProgress),
}
// SendAmount, FirmwareProgress: moved verbatim from messages.rs:37 and messages.rs:97
// (FirmwareProgress.timestamp becomes Time).
```

Design notes:

- **No frame data crosses the event channel.** `MsgFromCan::ParsedMessage`/`UnparsedMessage`
  (today ~40 KB/s of channel traffic, [`thread.rs`](../../daq/daqapp/src/can/thread.rs:57))
  are deleted; widgets read `engine.frames_in_range(...)` at render time. Events carry only
  low-rate status (≤ 50 Hz, mostly 5 Hz).
- The command set is exactly today's `MsgFromUi` ([`messages.rs`](../../daq/daqapp/src/messages.rs:3))
  with `Connect` widened to carry an optional source and a `SetTimeline` added. Nothing UI-shaped.
- `Engine::spawn` takes everything the thread needs up front; there is no separate
  "start" step because daqcore owns the thread's lifecycle end to end.

### 4.3 Concurrency decision (evaluated ≥ 2 options)

| | A. Per-tick immutable snapshot | **B. `Arc<RwLock>` shared cache (chosen)** | C. Seqlock |
|---|---|---|---|
| Mechanism | Engine clones a `Snapshot { timeline, latest, frame_window }` each tick; client reads `Arc<Snapshot>` behind a plain lock/slot | Engine thread writes through a write-lock (microsecond holds); clients read through a read-lock | Writer bumps a generation counter around unsynchronized writes; readers retry on mismatch |
| Per-tick cost | Clones the published frame window (e.g. 1 s of frames at 500 kbit/s ≈ 200–400 frames ≈ 20–60 KB) + the `latest` map every tick | Two microsecond write-locks per batch; **zero copies** | Zero copies, but unsafe / `AtomicPtr` bookkeeping |
| Read cost (60 fps × 14 widgets + daqcli) | One `Arc` load per widget; data is a fixed-size snapshot | Read-lock held for the duration of `frames_in_range`/`signal_series` — typically < 100 µs per widget per frame | Retry loop; can livelock-ish spin if readers are slow |
| Staleness | Up to 1 tick (200 ms today) — bad for the "LIVE" feel of the setpoint-following widgets | None — readers see the latest pushed frame | None |
| Complexity / safety | Easy, all `Sync` | Easy, all `Sync`, std-only | Hard: raw pointers, ABA, UB risk |
| Fit for 500 k-frame cache | Requires deciding a "window" policy (what subset to clone) — a new knob with real product consequences | The whole cache is trivially addressable; eviction is the only size control | Same as B but for no benefit at our lock-hold times |

**Decision: Option B.** `RamCache` and `Timeline` live in `Arc<RwLock<…>>` created by
`Engine::spawn`. The **engine thread is the only writer** (write-locks are held only across
`push`/`push_batch`/`evict`/`observe` — microseconds, no I/O while locked, no allocation of
the frame data itself since `CachedFrame` was already built before the lock). Clients
hold read-locks for bounded, allocation-light queries: `frames_in_range` returns a
`&[CachedFrame]` slice (no copy), `latest` is a `&HashMap` borrow, `signal_series`
allocates only its output `Vec<(Time, f64)>`.

Why B beats A here: the v2 doc's fast path ([`ram_cache_design_v2.md`](ram_cache_design_v2.md:287)
§5) is a 60 fps render-time read of *the latest* frame data; a 200 ms-old snapshot would
force Scope and the viewers to either accept jitter or publish snapshots every UI frame
(from the engine — a cross-thread wake that reintroduces the mpsc data path we are
deleting). B keeps the write side at microsecond holds and gives daqcli exactly the same
pollable semantics for free.

Why not C: at microsecond write holds and read holds bounded by a single `partition_point`
scan, std `RwLock` contention is not measurable; the unsafe surface buys nothing today.
(Revisit only if profiling in Phase 3 shows reader tail latency.)

Guardrails baked into the engine:

1. Engine never holds a write-lock across `driver.read_frames()`, `write_frame`,
   `daq_logger`, or `hil.tick` — locks are scoped to the four data methods.
2. `Engine::stop()` joins the thread before dropping the `Arc`s, so no client can hold a
   stale lock after shutdown.
3. Backfill (Phase 4) also goes through the same write-lock via `push_batch`, so the
   invariants of CONSTRAINT 3 hold identically for offline and live ingestion.

### 4.4 Read views used by the 14 widgets (60 fps) and by daqcli

| Widget (from v2 §6 table) | daqcore calls per frame |
|---|---|
| Scope, GgPlot, GpsPlot, Dynamics, Jitter | `engine.frames_in_range(timeline.time_span())` → iterate, project |
| ViewerTable | `engine.cache().latest()` + `timeline()` (P1: show LIVE badge when `setpoint_track == Frozen`) |
| ViewerList | `engine.cache().frames_before(timeline.setpoint(), N)` |
| Battery (voltage/temps) | `engine.latest_frame(bus, id)` (cloned) |
| BusLoad | `EngineEvent::BusLoad` (stays local-state, v2 §6) |
| Send / Bootloader / Hil / CommandPalette / LogParser / Sidebar | command/event only |
| daqcli `watch`/`status` | `engine.events()` + `engine.cache()`/`timeline()` polled at CLI-chosen rate |

---

## 5. Dependencies & Feature Flags

`daq/daqcore/Cargo.toml` after the move (workspace deps hoisted where sensible):

| Dep | Today | New home | Feature gate |
|---|---|---|---|
| `can_decode`, `can-dbc`, `log`, `bytemuck`, `csv` | daqcore ([`Cargo.toml`](../../daq/daqcore/Cargo.toml:6)) | daqcore | default (core) |
| `chrono` | daqcore + daqapp | daqcore (still used by `log_parse` display and `BusLoadTracker` labels) | default; **not** on the engine hot path (CONSTRAINT 1) |
| `slcan` (git, `features=["sync"]`) | daqapp ([`Cargo.toml`](../../daq/daqapp/Cargo.toml)) | daqcore | `serial` (drivers need `slcan::CanFrame` as the wire type; `CanBusSpeed::to_slcan_bitrate` needs `slcan::NominalBitRate`) |
| `serialport` | daqapp | daqcore | `serial` |
| `rand` | daqapp | daqcore | `simulated` (SimulatedDriver) |
| `globset`, `indexmap` | daqapp | daqcore | `formatting` |
| `serde`, `serde_json` | daqapp | daqcore | `formatting` (formatter config), `hil` (test/preset JSON), plus serde on `ConnectionSource` for daqapp's settings file |

Proposed feature set:

```toml
[features]
default = ["serial", "udp", "simulated", "loopback", "hil", "firmware", "formatting"]
serial = ["dep:serialport", "dep:slcan"]
udp = []            # std::net only
simulated = ["dep:rand"]
loopback = []
hil = ["dep:serde", "dep:serde_json"]
firmware = []       # pure protocol + state machine, no extra deps
formatting = ["dep:globset", "dep:indexmap", "dep:serde", "dep:serde_json"]
```

- **daqcli builds slim:** `daqcore = { features = [] }` gives cache/timeline/engine/
  loopback-free core; add `serial` to actually attach hardware.
- **daqapp uses defaults.**
- `udp` is std-only but gated anyway so a future `remote` transport can be added without
  forcing UDP parsing into every build (§9 Q2).
- The `Driver` trait and `create_driver` are compiled unconditionally; each impl is
  feature-gated and `create_driver` gets a `#[cfg]` match arm (simulator stays available in
  tests via `simulated` in dev-deps).

---

## 6. Borderline Module Decisions

| Module | Decision | Rationale |
|---|---|---|
| **HIL** (`hil/`) | **daqcore** (`feature = "hil"`) | The engine ticks it at 50 ms inside the CAN loop ([`thread.rs`](../../daq/daqapp/src/can/thread.rs) `HIL_UPDATE_MS`) and feeds it every parsed frame; it is core loop logic, not UI. daqcli `hil run` is a stated goal, which only works if it lives in the engine. The 1029-line `ui/hil.rs` panel stays in daqapp and renders `EngineEvent::Hil(HilSnapshot)`. |
| **Bootloader** | Split: **protocol + state machine → daqcore** (`can/firmware/`, `feature = "firmware"`); **readiness grid UI stays daqapp** | `FirmwareUpdater` (`tick`/`on_response`, 8 frames/tick with 4 ms delay) is driven from the engine loop; `FirmwarePackage::load` validation is pure. `ui/bootloader.rs` (1029 lines) consumes only `EngineEvent::FirmwareProgress` + `EngineCommand::Start/Arm/CancelFirmwareUpdate`. |
| **Formatter** | **daqcore** (`engine/formatter.rs`, `feature = "formatting"`) | It is a pure `&str → String` library over glob patterns (no UI, no eframe); both daqapp widgets and daqcli `watch` output need it, so it must live in the shared crate. `Formatter::try_load`'s file-vs-embedded fallback ([`formatter.rs`](../../daq/daqapp/src/formatter.rs:138)) moves as-is; daqapp passes its config path through `EngineConfig.formatter_config` and keeps a direct handle for widget calls. |
| **Settings / EngineConfig** | Split: **`Settings` JSON stays daqapp**; **`EngineConfig` new in daqcore** | `theme`, `pixels_per_point`, `udp_port`, and the `settings.json` load/save loop ([`settings.rs`](../../daq/daqapp/src/settings.rs:44)) are UI preferences and app persistence, not engine state. `main.rs` translates `Settings → EngineConfig` once at startup; `Connect`/`DbcSelected`/`UpdateLogFolder` cover runtime changes. daqcli has no `Settings` — it builds `EngineConfig` from CLI args/env. |
| **Connection types** | **daqcore** (`engine/connection.rs`) | `ConnectionSource`/`CanBusSpeed` parameterize the engine's driver selection ([`can/driver.rs`](../../daq/daqapp/src/can/driver.rs:424)); daqcli needs them to connect. The new `CanBus` enum (CONSTRAINT 3 key) lives here. |
| **`frozen.rs`** | **Deleted** | Pause is exactly `setpoint_track = Frozen` (CONSTRAINT 2). Retiring it is an exit criterion of the widget-migration phase. |
| **`messages.rs`** | **Deleted** (replaced by `EngineCommand`/`EngineEvent`) | See §3. |
| **`util::can`** | **daqcore** (`can/mod.rs`) | `slcan_to_u32_with_extid_flag` etc. are used by the moved thread and logger; daqapp keeps a re-export if any widget uses them. |

---

## 7. daqcli Sketch (validates API pollability)

Target: `daq/daqcli/src/main.rs` replaces the 6-line stub
([`daqcli/src/main.rs`](../../daq/daqcli/src/main.rs:3)) with a clap subcommand tree.
Every command below uses **only** the §4 API — no eframe, no second thread model:

```rust
fn main() -> ExitCode {
    let cli = Cli::parse();
    let engine = Engine::spawn(config_from_args(&cli))?;   // --dbc, --serial/--udp/--sim
    match cli.command {
        // status: drain events + one cache/timeline summary line, then exit
        Commands::Status          => { print_status(&engine); Ok(()) },

        // watch <MSG_ID> [--bus vcan|scan] [--rate 10]
        // poll loop: every 1/rate s -> engine.events() (print status changes)
        //             + engine.latest_frame / frames_in_range(last_ts..now) -> print rows
        Commands::Watch { msg_id, bus, rate } => watch(&engine, msg_id, bus, rate),

        // send <MSG_ID> --data 0102... [--period-ms 100 | --count 5]
        Commands::Send { msg_id, data, .. } => {
            engine.command(EngineCommand::AddSendMessage { .. })?;
            // confirm via EngineEvent::MessageSent
        },

        // hil run <preset|test>   (feature "hil")
        Commands::Hil { name } => {
            engine.command(EngineCommand::Hil(HilCommand::StartPreset(load(&name)?)))?;
            // print HilSnapshot events until HilStatus::Idle
        },

        // backfill <log-dir>   (Phase 4; only valid while Disconnected)
        Commands::Backfill { log_dir } => backfill(&engine, &log_dir),
        // = parse via daqcore::log_parse, then engine.cache() write via
        //   the new EngineCommand::Backfill(batch) / RamCache::push_batch path
    }
}
```

This validates the design because: `watch` proves the read path is pollable without
events; `send` proves commands are one-shot and confirmable via events; `hil run`
proves feature-gated subcommands compile; `backfill` proves offline ingestion can share
the live cache (CONSTRAINT 3 `push_batch`); and nothing here needs `request_repaint`.

---

## 8. Phased Migration Plan (re-based onto the crate move)

Each phase ends with **daqapp building, running, and behaving as before** (plus the phase's
new capability). Relative size: S < M < L.

### Phase 0 — Pure module moves, zero behavior change (M)
Move (with `pub use` shims in daqapp so widget code doesn't change): `connection.rs` →
`engine/connection.rs`; `can/driver.rs`, `can/bus_load.rs`, `can/daq_parser.rs` → `can/`;
`bootloader_protocol.rs` + `can/bootloader.rs` → `can/firmware/`; `hil/` → `hil/`;
`formatter.rs` → `engine/formatter.rs`; `util::can` → `can/mod.rs`. Add feature gates and
the new daqcore deps (§5). `can/thread.rs` + `can/state.rs` **stay in daqapp** still, now
importing daqcore types.
**Exit:** `cargo build --workspace` green with daqcli `--no-default-features`; daqapp
identical behavior (spot-check: connect, send, HIL, firmware cancel); `rg "use crate::(can::driver|connection|formatter|hil)" daq/daqapp/src` shows only the shim files.

### Phase 1 — `Time`, `Timeline`, `RamCache` land in daqcore; engine thread pushes (M)
New `timeline.rs` + `cache.rs` (CONSTRAINTS 1–3, with the unit-test matrix from the
comparison doc: all 9 track combos, eviction floor, merge ordering). `State`
([`can/state.rs`](../../daq/daqapp/src/can/state.rs:3)) gains the two `Arc<RwLock<…>>`
fields; `process_can_frame` ([`can/thread.rs`](../../daq/daqapp/src/can/thread.rs:39)) swaps
`chrono::Local::now()` → `Time::now()`, pushes into the cache, calls `timeline.observe`,
and runs the evict floor. Widgets still receive `MsgFromCan` as today (data path not yet
removed); the cache is a write-only shadow this phase.
**Exit:** shadow cache contents match what the UI receives (verified by a test harness that
subscribes to both); 500k-frame soak stays under the §3.3 memory ceiling of v2; timeline
invariant tests green.

### Phase 2 — `Engine` handle: commands in, events out (M)
Introduce `Engine`/`EngineConfig`/`EngineCommand`/`EngineEvent`; the `start_can_thread`
body becomes `EngineInner::run`; `main.rs` swaps channels for `Engine::spawn`.
`MsgFromUi` variants are deleted one by one as their call sites send `EngineCommand`
(sidebar, send panel, HIL panel, firmware panel). `MsgFromCan` status variants
(Disconnected/Connected/MessageSent/BusLoad/Hil/FirmwareProgress) become `EngineEvent`;
`ParsedMessage`/`UnparsedMessage` are **already redundant** (data is in the cache) and go
first.
**Exit:** `rg "MsgFromUi|MsgFromCan|can::thread|can::state" daq/daqapp/src` → 0 hits;
daqapp connects/sends/HILs/firmware-cancels via `Engine::command`; daqcli `status` +
`watch` work against a SimulatedDriver.

### Phase 3 — Widget read migration, delete `frozen.rs` (L — the v2 §9 Phases 1–3 work, unchanged in scope)
Migrate all 14 widgets to `engine.cache()`/`timeline()` reads per the §4.4 table and v2 §6
order: Scope first, then ViewerTable/ViewerList/Battery, then GgPlot/GpsPlot/Dynamics/
Jitter; add the timeline bar UI (3 fields × drag + Marching/Frozen toggle + Go live);
delete `frozen.rs` and the widgets' private pause/window state.
**Exit:** `rg Frozen daq/daqapp/src` → 0 hits; live-follow identical to pre-rework;
freezing `setpoint` freezes Scope; zero per-frame allocations beyond `signal_series`
outputs (as measured in v2 Phase 1); `git diff --stat` on `ui/*` shows only read-path
changes.

### Phase 4 — `.log` backfill in daqcore (M)
`backfill` module over the existing `log_parse` pipeline
([`log_parse/mod.rs`](../../daq/daqcore/src/log_parse/mod.rs:6)): parse → `Vec<CachedFrame>`
sorted batches → `RamCache::push_batch` under the same write-lock (CONSTRAINT 3 merge
path), only while `Disconnected`, abortable worker, progress surfaced as an
`EngineEvent::BackfillProgress`. daqcli `backfill <dir>` and the daqapp LogParser widget
([`ui/log_parser.rs`](../../daq/daqapp/src/ui/log_parser.rs)) share it.
**Exit:** load a 10-min `.log`, scrub across it in Scope/table/list; worker abortable;
memory under the ceiling; CLI backfill of the same file produces the same `time_span()`.

### Phase 5 — SQLite + Data Source management (L; from v2 §9 Phase 5, now naturally daqcore-side)
`SqliteSource` + `DataSource` attach/detach + timeline-range export. daqcli gains
`open <db>` / `export <range>`.
**Exit:** a sqlite session renders identically to a `.log` session in every Phase-3
widget.

Explicitly **out of plan**: daqcli-over-UDP to a remote engine, multi-client, SQLite
replication — tracked in §9.

---

## 9. Open Questions (for the user)

1. **Default `max_retention`?** 600 s (10 min, matches the Phase-4 test log) vs.
   "bind to `CAPACITY`" (evict purely on size). Recommendation: 600 s default,
   configurable in `EngineConfig`; CAPACITY remains the hard backstop.
2. **Live frame events: yes or no?** This design deletes them entirely (widgets read the
   cache). Alternative: keep a low-rate `EngineEvent::FramesPublished { newest: Time,
   count: u32 }` so a client can *know* when to re-read without re-reading. Cost: one
   event per batch (~10/s). Recommendation: add it — it makes daqcli `watch` efficient
   and daqapp repaints precise.
3. **Feature-gate `SimulatedDriver`/`UdpDriver` out of `default`?** daqapp currently uses
   both. Recommendation: keep in default; only daqcli chooses slim builds.
4. **Remote daqcli over UDP to a running daqcore engine** (Phase 5+)? The event/command
   split is already transport-agnostic, but we need a wire codec and a security story.
   Recommendation: defer; keep this design transport-neutral.
5. **Multi-client against one engine?** `Engine`'s event `Receiver` is single-consumer.
   Today only one client exists per process. Recommendation: keep single-client; if
   daqapp+daqcli ever run against one engine, split `events()` into a broadcast channel
   then (API-compatible: `events()` stays, gains `subscribe()`).
6. **`signal_series` name-matching cost** (string compare per signal per frame in the
   decoded message). Recommendation: fine for single-signal plots; if multi-signal GgPlot
   is slow, add a per-message signal-name index built at DBC-load time — do not build
   preemptively.
7. **Should `Engine::spawn` pre-connect** (config.source set, as `main.rs:40-58` does
   today) or should connect always be an explicit `EngineCommand`? Recommendation:
   both — `source` in config *and* a `Connect` command; spawn auto-sends `Connect` when
   `source.is_some()`, preserving today's behavior.

---

## 10. Risks

| Risk | Mitigation |
|---|---|
| **slcan git dep in daqcore** — every daqcore consumer (incl. a future non-serial daqcli) inherits a git dependency. | Feature-gated behind `serial`; lockfile pins the rev; if slcan gets published, swap in Phase 5. |
| **RwLock reader tail latency** if a widget holds a read-lock across a slow `signal_series` while the engine wants to push. | §4.3 guardrail: write-locks never held across I/O; read queries bounded; profile in Phase 3 exit; fallback is seqlock (Option C) as a drop-in — the API is unchanged. |
| **Phase 0 is a big-bang move** (~10 files, 2 crates, feature wiring) and can break the build for a while. | Move in one PR but commit per-module; daqapp `pub use` shims mean zero widget edits; CI on `--no-default-features` catches accidental coupling. |
| **Engine thread as single point of failure** — a panic kills UI data. | `catch_unwind` around the run loop with a `ConnectionState::Disconnected` event + log; panic hook prints stack; Phase-2 exit criterion includes "kill -STOP the engine thread → UI shows stale-but-coherent state, no crash". |
| **Behavior drift while `MsgFromCan` data path and cache shadow coexist (Phase 1).** | Shadow phase is short and test-gated: a harness asserts cache ↔ message-stream equality for N frames before Phase 2 starts deleting the message path. |
| **1029-line `ui/bootloader.rs` and 161-line HIL panel keep their own local state** and may resist becoming pure event renderers. | Their local state is *view* state (selected tab, armed checkbox) — allowed to stay; only *data* state must come from events. Enforced by Phase-3 review checklist, not by type system. |
| **Timestamp semantics regression** (someone reintroduces `chrono` on the hot path). | CONSTRAINT 1 stated at top; `Time` is the only time type in `cache`/`timeline`/`engine` signatures; `rg "chrono::" daq/daqcore/src/engine daq/daqcore/src/cache daq/daqcore/src/timeline` → only `BusLoadTracker` labels, reviewed each PR. |
