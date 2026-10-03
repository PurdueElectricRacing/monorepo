# daqapp RAM Cache + Timeline — Implementation Handoff (Option B)

**Status:** This is the fresh implementation handoff for the **locked-in Option B
daqcore-centric architecture** ([`daqcore_engine_design_option_b.md`](daqcore_engine_design_option_b.md)).
It replaces the old pre-re-scope handoff. Everything in that old doc that contradicts this
one (notably the `(CanBus, u32)` cache key and the `bus` field on frame structs) is
**superseded by Patch P5** — do not re-create those shapes.

**Source of truth:** [`daqcore_engine_design_option_b.md`](daqcore_engine_design_option_b.md)
(the "Option B doc"). When this handoff and the Option B doc ever appear to disagree, the
Option B doc wins. Secondary references: [`daqcore_engine_design.md`](daqcore_engine_design.md)
(Option A, rejected), [`ram_cache_design_v2.md`](ram_cache_design_v2.md) (widget migration
table + Timeline API), [`ram_cache_design_comparison.md`](ram_cache_design_comparison.md)
(P1–P4 patches + final data structures).

**Scope of this task: pure documentation. No source code, no `Cargo.toml`, no other doc is
modified by writing this file.**

---

## 1. Decision record

**Option B is chosen. Option A (monolithic `Engine`) is rejected. The pre-re-scope
daqapp-only plan is retired.**

- **Option A** (`daqcore_engine_design.md`): a single `Engine` thread in daqcore owns
  driver, parser, HIL, firmware **and** the cache + timeline, sharing the latter out via
  `Arc<RwLock<RamCache>>` / `Arc<RwLock<Timeline>>`; daqapp and daqcli are thin clients.
- **Option B** (this handoff): daqcore is a library of composable modules with **zero
  synchronization primitives in any public type**. The CAN thread is a daqcore module
  (`can_thread`) that the caller spawns and drives with commands in / events out over plain
  `std::sync::mpsc`. The RAM cache and Timeline are **plain fields on the UI/main thread**,
  owned via a thin `Session` helper; widgets read them through ordinary `&` borrows at
  60 fps — no lock, no channel hop, no snapshot staleness.

**Rationale (one paragraph, per §9 of the Option B doc).** Option B is the lower-regression
path: today's app already runs "frames over mpsc, UI thread owns everything", and Option B
splits the thread work into a **test-gated, behavior-preserving Phase 2a** refactor (same
channel pattern, same cadences, proven by headless unit tests plus an old-vs-new
event-stream diff) followed by a **mechanical crate move in Phase 2b**. Option A, by
contrast, restructures the thread into an engine and re-plumbs the UI from a message stream
to shared locked state in a single step with no intermediate proof point. The 60 fps read
path is the app's bottleneck; Option B removes the only contention the RAM-cache design
introduces, while Option A pays an `RwLock` read-guard tax on every widget, every frame,
forever. **Escape hatch (why A loses):** Option A would only be chosen *iff* daqcore were
roadmap'd as a standalone embeddable engine for a **third** consumer (headless recorder
service, second GUI framework, remote client) where caller-written orchestration is
unacceptable. That third consumer is **not on the roadmap** — daqapp and daqcli are the only
two consumers. Option A is therefore shelved, revisited only if that changes.

---

## 2. Architecture summary

### 2.1 daqcore module layout (old file → new module)

From Option B doc §2. (Line numbers verified on `millan/ram_cache`.)

| Old location (lines) | New location | Notes |
|---|---|---|
| [`daq/daqapp/src/can/driver.rs`](../../daq/daqapp/src/can/driver.rs) (435) | `daqcore/src/can/driver.rs` | Pure move. `Driver` trait ([`driver.rs:33-43`](../../daq/daqapp/src/can/driver.rs:33)), 4 impls, `create_driver` ([`driver.rs:424-434`](../../daq/daqapp/src/can/driver.rs:424)). Feature-gated: `serial`, `udp`, `simulated`, `loopback`. |
| [`daq/daqapp/src/can/thread.rs`](../../daq/daqapp/src/can/thread.rs) (421) | `daqcore/src/can_thread/{run,rx,decode,events}.rs` | **Decomposed, not pasted**: the ~310-line `start_can_thread` loop ([`thread.rs:107-420`](../../daq/daqapp/src/can/thread.rs:107)) splits into a thin orchestration `run()` (~150 lines) + per-concern components (§2.4). Timestamp becomes `Time::now()` stamped inside `FrameDecoder` (replacing `chrono::Local::now()` at [`thread.rs:39`](../../daq/daqapp/src/can/thread.rs:39)). Constants become a module `consts` block ([`thread.rs:3-12`](../../daq/daqapp/src/can/thread.rs:3)). |
| [`daq/daqapp/src/can/state.rs`](../../daq/daqapp/src/can/state.rs) (215) | `daqcore/src/can_thread/{tx,connection,firmware}.rs` | The 13-field `State` grab bag ([`state.rs:3-17`](../../daq/daqapp/src/can/state.rs:3)) is **abolished** and split into `SendTable` (send scheduling, [`state.rs:159-213`](../../daq/daqapp/src/can/state.rs:159), `DateTime<Local>` → `Time`), `ConnectionManager` (driver lifecycle, [`thread.rs:263-294`](../../daq/daqapp/src/can/thread.rs:263)), and `FirmwareSession` ([`state.rs:55-144`](../../daq/daqapp/src/can/state.rs:55), events *returned*, not sent). |
| [`daq/daqapp/src/messages.rs`](../../daq/daqapp/src/messages.rs) (106) | `daqcore/src/can_thread/types.rs` | `MsgFromUi` → `CanThreadCommand`; `MsgFromCan` → `CanThreadEvent`; `ParsedMessage`/`UnparsedMessage` merge into one `ParsedFrame`. The one genuinely 1:1 row (rename + `Time`). **Deleted from daqapp.** |
| [`daq/daqapp/src/can/bootloader.rs`](../../daq/daqapp/src/can/bootloader.rs) (500) | `daqcore/src/can/firmware.rs` | `FirmwareUpdater`, `OutboundFrame`; feature `firmware`. |
| [`daq/daqapp/src/bootloader_protocol.rs`](../../daq/daqapp/src/bootloader_protocol.rs) (425) | `daqcore/src/can/firmware_protocol.rs` | Pure protocol parsing; feature `firmware`. |
| [`daq/daqapp/src/can/daq_parser.rs`](../../daq/daqapp/src/can/daq_parser.rs) (205) | `daqcore/src/can/logger.rs` | `DaqLogger`; CAN2-only binary flat log. |
| [`daq/daqapp/src/can/bus_load.rs`](../../daq/daqapp/src/can/bus_load.rs) (64) | `daqcore/src/can/bus_load.rs` | `BusLoadTracker`, 1/5/10/30 s windows. |
| [`daq/daqapp/src/hil/`](../../daq/daqapp/src/hil) (~545) | `daqcore/src/hil/` | `HilEngine` moves; `process_parsed` changes signature from `&messages::ParsedMessage` to `&ParsedFrame`. `config::list_available_tests` takes an explicit base-dir arg. Feature `hil`. |
| [`daq/daqapp/src/connection.rs`](../../daq/daqapp/src/connection.rs) (107) | `daqcore/src/connection.rs` | `ConnectionSource`, `CanBusSpeed`; serde impls move too. |
| [`daq/daqapp/src/formatter.rs`](../../daq/daqapp/src/formatter.rs) (311) | `daqcore/src/formatter.rs` | `&str → String` glob formatter; feature `formatting`. |
| [`daq/daqcore/src/can.rs`](../../daq/daqcore/src/can.rs) (26) | stays `daqcore/src/can.rs` | `EXTENDED_ID_FLAG`, `can_dbc_to_u32_*` — unchanged. |
| `daqcore/src/log_parse/` | stays | Offline CSV correlation; unchanged. |
| — (new) | `daqcore/src/timeline.rs` | `Time`, `Track`, `Timeline`. |
| — (new) | `daqcore/src/cache.rs` | `RamCache`, `CachedFrame`, `FramePayload`, `CAPACITY = 500_000`. |
| — (new) | `daqcore/src/session.rs` | `Session` helper. |
| — (new) | `daqcore/src/backfill.rs` | `.log` parse worker spawner. |

**Why no `CanBus` enum:** `CanBus` was intentionally removed in "DaqApp Bootloader updates,
reset detection, metadata erasing (#489)" — cosmetic in the sidebar, never wired to
anything; today's frame messages carry no bus field. Future dual-bus extension path:
add `bus` field to `ParsedFrame`/`CachedFrame` and widen the `latest` key to
`(CanBus, u32)` — a contained change to three types plus the map key.

**daqapp keeps:** `app.rs`, `widgets.rs`, `ui/`, `settings.rs`, `shortcuts.rs`,
`workspace.rs`, `action.rs`, `assets.rs`, `util.rs` (minus `util::can` ID helpers), themes.
**Deleted from daqapp (across phases):** `messages.rs`, `frozen.rs` (Phase 3), and the moved
`can/*` / `hil/*` / `bootloader_protocol.rs` / `formatter.rs` / `connection.rs` files (as
`pub use` shims during Phases 0–2, then removed).

### 2.2 The `can_thread` public API (VERBATIM from Option B doc §3.1)

```rust
// daqcore::can_thread

/// Everything the can thread needs before it can run. Built by the caller
/// (daqapp main, or daqcli main) — daqcore never reads config files itself.
#[derive(Debug, Clone)]
pub struct CanThreadConfig {
    pub dbc_path: Option<std::path::PathBuf>,   // re-loadable via DbcSelected
    pub log_folder: Option<std::path::PathBuf>, // live binary log (DaqLogger), CAN2 only
    pub hil_dir: std::path::PathBuf,            // presets/tests folder for the HIL engine
}

/// Commands UI → can thread. Mirrors today's MsgFromUi 1:1 (9 variants) + Stop.
pub enum CanThreadCommand {
    Connect(Option<connection::ConnectionSource>), // None = disconnect
    DbcSelected(std::path::PathBuf),
    AddSendMessage(can_thread::AddSendMessage),
    DeleteSendMessage { msg_id: u32 },
    UpdateLogFolder(std::path::PathBuf),
    Hil(hil::HilCommand),
    StartFirmwareUpdate(can::firmware::FirmwarePackage),
    ArmFirmwareUpdate(can::firmware::FirmwarePackage),
    CancelFirmwareUpdate,
    Stop,
}

/// Events can thread → UI. Mirrors today's MsgFromCan (9 variants) 1:1,
/// with timestamps already `Time` (Constraint 1: applied at parse).
pub enum CanThreadEvent {
    Frame(ParsedFrame),                 // replaces ParsedMessage + UnparsedMessage
    Disconnection,
    ConnectionSuccessful,
    ConnectionFailed(String),
    MessageSent { msg_id: u32, timestamp: Time, amount_left: Option<SendAmount> },
    BusLoad { load_1s: f32, load_5s: f32, load_10s: f32, load_30s: f32 },
    Hil(hil::HilSnapshot),
    FirmwareProgress(can::firmware::FirmwareProgress),
}

/// One received CAN frame, decoded if the DBC knows it. The UI does
/// `session.ingest_frame(&frame)` for every one — same code path for both
/// decoded and undecoded frames (today: two variants, messages.rs:15-34).
#[derive(Debug, Clone)]
pub struct ParsedFrame {
    pub timestamp: timeline::Time,        // Time::now() at parse, in the can thread
    pub msg_id: u32,                      // raw id, no extended flag (matches today)
    pub is_msg_id_extended: bool,
    pub raw_bytes: Vec<u8>,
    pub decoded: Option<can_decode::DecodedMessage>,
}

/// Spawn the can thread. Caller keeps `out` (the event receiver) and the handle.
pub fn spawn_can_thread(
    config: CanThreadConfig,
    out: std::sync::mpsc::Sender<CanThreadEvent>,
    cmd: std::sync::mpsc::Receiver<CanThreadCommand>,
) -> CanThreadHandle;

/// Owns the command side; joining is explicit so the caller controls shutdown.
pub struct CanThreadHandle { /* cmd_tx, stop flag, join: Option<JoinHandle<()>> */ }
impl CanThreadHandle {
    pub fn command(&self, cmd: CanThreadCommand);           // fire-and-forget, today's pattern
    pub fn stop(&mut self) -> std::thread::Result<()>;      // sends Stop, joins
}
```

`SendAmount`, `AddSendMessage`, `FirmwareProgress` move verbatim from
[`messages.rs:37-70`](../../daq/daqapp/src/messages.rs:37),
[`messages.rs:72-77`](../../daq/daqapp/src/messages.rs:72),
[`messages.rs:97-105`](../../daq/daqapp/src/messages.rs:97) (timestamp fields become
`Time`). All channel types are `Send` plain data — no locks, no `Arc`.
**Note the counts: `CanThreadCommand` has 10 variants; `CanThreadEvent` has 8 variants.**

### 2.3 daqapp-side ownership model

The **UI thread owns the data; the CAN thread owns the state machines.** Nothing is shared
between the two except by value over the channel (Option B doc §3.2–3.3, §4.1):

```rust
// daqapp::app  (after Option B migration)
pub struct DAQApp {
    // NEW — Option B's defining fields: plain, single-owner, no locks
    session: daqcore::Session,                 // owns RamCache + Timeline
    can_thread: daqcore::can_thread::CanThreadHandle,
    can_to_ui: std::sync::mpsc::Receiver<daqcore::can_thread::CanThreadEvent>,
    backfill_rx: Option<std::sync::mpsc::Receiver<daqcore::backfill::BackfillBatch>>,

    // unchanged UI-only state
    widgets: /* egui_tiles layout, widget registry, ... */,
    connection_status: /* from ConnectionSuccessful/Failed events */,
    hil_snapshot: daqcore::hil::HilSnapshot,   // last received, rendered as-is
    firmware_progress: Option<daqcore::can::firmware::FirmwareProgress>,
    bus_load: (f32, f32, f32, f32),
    settings: settings::Settings,              // stays daqapp
}
```

- **CAN thread owns (single owner, `&mut` only, never shared):** `driver:
  Option<Box<dyn Driver>>`, `parser: Option<can_decode::Parser>`, `send_msgs` table,
  `bus_load_tracker`, `daq_logger`, `hil_engine`, `firmware_updater`.
- **CAN thread must NOT own:** `RamCache`, `Timeline`, `Settings`/UI prefs, widget state.
  It only *emits* `Frame` events; it never mutates cache or timeline.
- **Sub-decision (a):** HIL + firmware stay **inside** the CAN thread. **Sub-decision
  (b):** parsed frames ship **out** over the mpsc channel (exactly today's
  [`messages.rs:15-34`](../../daq/daqapp/src/messages.rs:15) pattern).

### 2.4 The can thread is a composition of submodules (refactor, not paste — Option B doc §3.7)

The current `start_can_thread` ([`thread.rs:107-420`](../../daq/daqapp/src/can/thread.rs:107))
interleaves eight concerns in ~310 lines, and `State`
([`state.rs:3-17`](../../daq/daqapp/src/can/state.rs:3)) is a 13-field grab bag. Phase 2a
decomposes it (still in daqapp) into:

| Component | New file | Contract |
|---|---|---|
| `Events` | `can_thread/events.rs` | The **single** send choke point; `fn emit(&self, ev: CanThreadEvent)`; the only type in the module holding the `Sender`. |
| `ConnectionManager` | `can_thread/connection.rs` | `connect(source)`, `write_frame()`, `read_frames()`, `close()`; returns connection *events* to the caller instead of sending them. |
| `FrameDecoder` | `can_thread/decode.rs` | `decode(&mut self, frame: &slcan::CanFrame, now: Time) -> ParsedFrame`; owns `Option<can_decode::Parser>`; the **only** place a `Time` stamp is applied (Constraint 1). |
| `SendTable` | `can_thread/tx.rs` | `add`/`delete`/`due(now: Time) -> Vec<SendTick>`; pure due-time math with an injected clock — directly unit-testable. |
| `FirmwareSession` | `can_thread/firmware.rs` | `start(package, armed)`, `tick(now: Instant) -> Vec<OutboundFrame>` (≤ 8 per tick; caller paces the 4 ms delay), `frame_received(id, data) -> Option<FirmwareProgress>` — progress is **returned**, never sent inline. |
| `RxLoop` | `can_thread/rx.rs` | Per-read error classification (Timeout → 2 ms retry flag; other → `ConnectionFailed` event), firmware-response filtering; returns a `ReadOutcome` enum, sends nothing. |
| (existing modules, used as-is) | — | `bus_load::BusLoadTracker`, `can::logger::DaqLogger`, `hil::HilEngine`, `can::firmware::FirmwareUpdater` — already single-concern; the run loop ticks them at existing cadences. |
| `run` | `can_thread/run.rs` | ~120–150 lines, orchestration only: drain commands → HIL tick → sends (`SendTable` → `ConnectionManager`) → firmware burst → read (`RxLoop`) → decode → bus-load → log → `events.emit(...)` per returned event. **No protocol knowledge.** |

**The rules that make the split real:**
1. Components are dumb `&mut self` state machines with no `Sender` inside (except
   `Events`). Clocks are **injected per call** (`now: Time` / `now: Instant`) — one `now`
   per loop iteration replaces the four independent `chrono::Local::now()` call sites
   ([`thread.rs:39`](../../daq/daqapp/src/can/thread.rs:39),
   [`thread.rs:220`](../../daq/daqapp/src/can/thread.rs:220),
   [`state.rs:160`](../../daq/daqapp/src/can/state.rs:160),
   [`state.rs:207`](../../daq/daqapp/src/can/state.rs:207)).
2. Return events, don't send them. Components return `Option<T>` / `Vec<T>`; the run loop
   is the only place `events.emit(...)` is called.
3. **Behavior is preserved exactly**: constants
   ([`thread.rs:3-12`](../../daq/daqapp/src/can/thread.rs:3)), event *order*, channel
   types, and all cadences (50 ms HIL, 200 ms bus-load, 8×4 ms firmware) are unchanged.
4. Proven before it moves: headless unit tests + a 10-min loopback/serial scenario that
   diffs the *old* vs *new* event stream (a debug-only `Events` mirror — same technique as
   the Phase 1 shadow harness).

### 2.5 The per-tick `update()` pipeline (Option B doc §4.2, verbatim)

```rust
fn update(&mut self, ctx: &egui::Context, _frame: &mut eframe::Frame) {
    // 1. DRAIN + INGEST (was: build can_messages; now: mutate the session directly)
    while let Ok(event) = self.can_to_ui.try_recv() {
        match event {
            CanThreadEvent::Frame(frame) => {
                // cache.push (append + ordering assert) + timeline.observe(latest)
                let changed = self.session.ingest_frame(&frame);
                if changed { /* optional: mark LIVE badge etc. */ }
            }
            CanThreadEvent::BusLoad { .. } => { self.bus_load = ..; }
            CanThreadEvent::Hil(s) => { self.hil_snapshot = s; }
            CanThreadEvent::FirmwareProgress(p) => { self.firmware_progress = Some(p); }
            CanThreadEvent::ConnectionSuccessful | Disconnection | ConnectionFailed(_) => { .. }
            CanThreadEvent::MessageSent { .. } => { /* send-queue widget state */ }
        }
    }

    // 2. BACKFILL (only while Disconnected): one batch per update
    if let Some(rx) = &self.backfill_rx {
        while let Ok(batch) = rx.try_recv() {
            self.session.push_batch(std::mem::take(&mut batch.frames)); // sorted merge
        }
    }

    // 3. PER-TICK MAINTENANCE (marching + eviction floor, Constraint 3)
    self.session.evict_now();   // floor = max(timeline.start(), now - max_retention)

    // 4. RENDER — widgets take &RamCache / &Timeline via WidgetContext
    egui_tiles::CentralPanel::default().show(ctx, |ui| {
        let mut wc = WidgetContext {
            cache: self.session.cache(),       // &RamCache  — plain borrow
            timeline: self.session.timeline(), // &Timeline  — plain borrow
            hil: &self.hil_snapshot, ..        // unchanged
        };
        for widget in self.widgets.iter_mut() { widget.show(ui, &mut wc); }
    });

    // Timeline bar (Slider/drag per field): writes set_*/release_* directly on
    // self.session.timeline_mut() — UI thread is the sole owner, so Constraint 2
    // compliance is by construction, no messaging round-trip needed.

    ctx.request_repaint(); // unchanged: unconditional (app.rs:263)
}
```

**Pipeline in one line:** drain `can_to_ui` → `session.ingest_frame` per frame → backfill
`session.push_batch` (Disconnected only) → `session.evict_now()` → render via plain
`&RamCache`/`&Timeline` borrows into `WidgetContext` → `ctx.request_repaint()` (unconditional).

**The selling point, made concrete:** the 60 fps read path is `widget.show()` calling
`cache.frames_in_range(...)` / `timeline.range()` on **same-thread data through a plain
`&` borrow**. No `RwLockReadGuard`, no channel, no snapshot staleness. Ingestion also moves
out of `show()`: `can_messages` disappears entirely; the session is mutated during drain
(step 1) before any widget renders (step 4), so a widget's view of the cache is exactly
"everything received before this frame" — deterministic per frame.

### 2.6 The daqcore data-module APIs (VERBATIM, Option B doc §5.1/§5.2/§5.6)

```rust
// daqcore::timeline
#[derive(Debug, Clone, Copy, PartialEq, Eq, PartialOrd, Ord, Hash)]
pub struct Time(i64); // wall-clock Unix millis UTC (Constraint 1)

impl Time {
    pub fn now() -> Self;
    pub fn from_unix_millis(ms: i64) -> Self;
    pub fn unix_millis(self) -> i64;
    pub fn secs(self, other: Time) -> f64;
    pub fn label(self) -> String; // chrono, display ONLY
}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum Track { Marching, Frozen }

pub struct Timeline { /* start, end, setpoint + 3×Track + window_secs +
                      follow_offset_secs + max_retention (Constraint 2) */ }

impl Timeline {
    pub fn live(now: Time, window_secs: f64) -> Self;
    pub fn observe(&mut self, latest: Time) -> bool; // marching re-derive; false = no move
    pub fn start(&self) -> Time;
    pub fn end(&self) -> Time;
    pub fn setpoint(&self) -> Time;
    pub fn is_live(&self) -> bool;
    pub fn set_start(&mut self, t: Time);            // freezes track + clamps
    pub fn set_end(&mut self, t: Time);
    pub fn set_setpoint(&mut self, t: Time);
    pub fn release_start(&mut self);
    pub fn release_end(&mut self);
    pub fn release_setpoint(&mut self);              // "go live"
    pub fn set_window_secs(&mut self, secs: f64);
    pub fn set_follow_offset_secs(&mut self, secs: f64);
    pub fn set_max_retention(&mut self, d: std::time::Duration);
    pub fn max_retention(&self) -> std::time::Duration;
    pub fn range(&self) -> std::ops::Range<Time>;
    pub fn setpoint_range(&self) -> std::ops::Range<Time>;
}
```

```rust
// daqcore::cache  — NOTE: `u32`-only keys per Patch P5; there is NO `bus` field
pub struct CachedFrame {
    pub timestamp: Time,
    pub msg_id: u32,                 // no extended flag; key of `latest` (Patch P5, §0)
    pub is_msg_id_extended: bool,
    pub payload: FramePayload,
}
pub enum FramePayload {
    Decoded { raw_bytes: Vec<u8>, decoded: can_decode::DecodedMessage },
    Undecoded { raw_bytes: Vec<u8> },
}

pub const CAPACITY: usize = 500_000;

pub struct RamCache { /* frames: Vec<CachedFrame>, first_index: usize,
                      latest: HashMap<u32, CachedFrame> (Constraint 3, Patch P5) */ }

impl RamCache {
    pub fn new() -> Self;
    pub fn push(&mut self, frame: CachedFrame) -> bool;          // append + ordering assert
    pub fn push_batch(&mut self, batch: Vec<CachedFrame>);       // sorted merge
    pub fn evict(&mut self, floor: Time) -> usize;               // P1+P2
    pub fn len(&self) -> usize;
    pub fn is_empty(&self) -> bool;
    pub fn time_span(&self) -> Option<(Time, Time)>;
    pub fn latest(&self, msg_id: u32) -> Option<&CachedFrame>;
    pub fn latest_map(&self) -> &HashMap<u32, CachedFrame>;
    pub fn frames_in_range(&self, range: (Time, Time)) -> &[CachedFrame];
    pub fn frames_before(&self, ts: Time, count: usize) -> &[CachedFrame];
    pub fn signal_series(&self, msg_id: u32, signal: &str,
                         range: (Time, Time)) -> Vec<(Time, f64)>;
}
```

`CachedFrame` carries `is_msg_id_extended` because today's `ParsedMessage` does
([`messages.rs:79-86`](../../daq/daqapp/src/messages.rs:79)). If you are tempted to add a
`bus` field or a `(CanBus, u32)` key: **stop — that is the pre-P5 design and it is
superseded.**

```rust
// daqcore::session — the orchestration helper (decision: ADOPT it)
/// Pairs the cache and timeline and does the per-tick bookkeeping that both
/// daqapp and daqcli must perform. SINGLE OWNERSHIP: constructed on the UI/main
/// thread, never shared. Contains no thread, no channel, no atomic — it is data
/// plus methods. (This is the guardrail that keeps Option B from drifting into
/// Option A's Engine: if Session ever needs a Sender or a lock, stop and re-scope.)
pub struct Session { cache: RamCache, timeline: Timeline }

impl Session {
    pub fn live(now: Time, window_secs: f64, follow_offset_secs: f64,
                max_retention: std::time::Duration) -> Self;
    /// Per received frame: cache.push + timeline.observe. Returns true if the
    /// timeline moved (repaint hint / LIVE badge).
    pub fn ingest_frame(&mut self, frame: &ParsedFrame) -> bool;
    /// Per tick: evict with floor = max(timeline.start(), now - max_retention).
    pub fn evict_now(&mut self);
    /// Backfill path (UI thread only): sorted-merge a batch.
    pub fn push_batch(&mut self, frames: Vec<CachedFrame>);
    pub fn cache(&self) -> &RamCache;
    pub fn timeline(&self) -> &Timeline;
    pub fn timeline_mut(&mut self) -> &mut Timeline;
}
```

**Why `Session` is still a library (and not a drift back to Option A):** (1) it owns no
thread; (2) it owns no synchronization primitive — its fields are private plain data
accessible only through `&`/`&mut` on the owning thread; (3) the type system enforces
single ownership (`&mut self` only). It is the same category of object as
`std::collections::BTreeMap`. It exists because without it daqapp and daqcli each
re-implement the drain → `push` → `observe` → `evict` loop (~40 lines, including the
Constraint-3 evict floor and the "merged undecoded frame refreshes `latest`" rule).
**Drift guardrail:** any PR adding a `Sender`/`Receiver`/atomic field to `Session` is a
re-scope to Option A and must be approved as such.

### 2.7 Backfill path (`.log` → cache, Option B doc §4.4)

```rust
// daqcore::backfill
pub struct BackfillBatch { pub frames: Vec<cache::CachedFrame> } // e.g. 2_000 frames

pub fn spawn_log_worker(
    log_path: std::path::PathBuf,
    out: std::sync::mpsc::Sender<BackfillBatch>,
    cancel: std::sync::mpsc::Sender<()>,
) -> std::thread::JoinHandle<()>;
// worker: daqcore::log_parse per record → build CachedFrame with the log's own
// timestamps (already wall-clock at the source) → ship in time-ascending chunks.
```

- Only allowed while `Disconnected`; the UI refuses the file-open otherwise.
- The **UI thread** runs `session.push_batch(chunk)` — a sorted merge (P4). Lock-free by
  construction: the cache is single-threaded-owned, so there is no synchronization
  question at all.
- (Option A ran `push_batch` on the engine thread under an `RwLock`; Option B's choice is
  the one that needs no locking.)

### 2.8 Dependencies & feature flags (Option B doc §7)

| Feature | Default? | Enables |
|---|---|---|
| (default) | yes | `timeline`, `cache`, `session`, `connection`, `can` ID utils, `log_parse`, `backfill` (parse side), serde |
| `serial` | yes (daqapp) | `slcan`, `serialport`, `SerialDriver` |
| `udp` | yes (daqapp) | `slcan`, `UdpDriver` |
| `simulated` | yes (daqapp) | `slcan`, `rand`, `SimulatedDriver` |
| `loopback` | yes (daqapp) | `LoopbackDriver` |
| `hil` | yes (daqapp) | `indexmap`, `serde` presets, `hil/` |
| `firmware` | yes (daqapp) | `can/firmware.rs`, `firmware_protocol.rs` |
| `formatting` | yes (daqapp) | `globset`, `formatter.rs` |

- daqapp: `daqcore = { workspace = true, features = ["serial","udp","simulated","loopback","hil","firmware","formatting"] }`.
- daqcli (slim): `daqcore = { workspace = true, default-features = false, features = ["serial"] }`
  + `hil`/`firmware` per subcommand — **a daqcli build without `serial` has no
  `slcan`/git dep at all.**
- Hardware deps that move to daqcore: `slcan` (git, `features=["sync"]` — the git dep now
  sits in a library crate; mitigate by pinning the rev / vendoring / keeping it behind the
  three driver features), `serialport 4.8.1`, `rand 0.10.1`, `globset 0.4.18`,
  `indexmap 2.14.0`. Already in daqcore: `can_decode 0.7.2`, `can-dbc 9.0.0`,
  `chrono 0.4.44` (display only), `log`, `bytemuck 1.25.0`, `csv 1.4.0`.
- **No** `hashbrown` in daqcore (std `HashMap` per Constraint 3). UI-only deps (eframe
  0.33.3, egui_tiles 0.14.1, egui_plot 0.34.1, walkers, rfd, env_logger) stay in daqapp.

---

## 3. Binding constraints (user-mandated — do NOT re-litigate)

All five are quoted verbatim from the Option B doc. Implementers: if a design detail you
want conflicts with any of these, the constraint wins; if you believe the constraint
itself is wrong, stop and escalate — do not silently deviate.

**Constraint 1 — Wall-clock timestamps (Option B doc §0, verbatim).**
Timeline timestamps are **wall-clock** (Unix millis UTC, `Time(i64)` newtype). The CAN
thread applies the corrected timestamp **at parse time**. `chrono` is used **only for
display labels**, never for ordering, arithmetic, or cache keys.

**Constraint 2 — Timeline tracks (Option B doc §0, verbatim).**
`Timeline` has `start` / `end` / `setpoint`, each with an independent
`Track { Marching, Frozen }` — **all 9 combinations must be expressible**. Marching rules:
`end = max(end, latest + follow_offset_secs)`, `start = end − window_secs`,
`setpoint = end`; invariant `start <= setpoint <= end` (enforced by clamping inside
setters/observe); **never silently move a Frozen field**.

**Constraint 3 — Naive RAM cache (Option B doc §0, verbatim).**
`Vec<CachedFrame>` (time-ascending) + `first_index: usize`
+ `latest: HashMap<(CanBus, u32), CachedFrame>`;
`push` = append + ordering assert; `push_batch` = sorted merge;
`evict` floor `max(timeline.start(), now − max_retention)`;
`CAPACITY = 500_000`.
Patches P1–P4 plus the Track/Timeline API from the comparison doc are already decided.
(Note: `Vec` + offset, **not** `VecDeque` — `VecDeque::partition_point` is
nightly-only, which is exactly why P3 exists.)

**Constraint 4 — Patch P5 (user re-evaluation, supersedes the `latest` key in Constraint 3 — Option B doc §0, verbatim).**
The `(CanBus, u32)` key is **not** created: `CanBus` was intentionally removed in "DaqApp
Bootloader updates, reset detection, metadata erasing (#489)" — it was cosmetic in the
sidebar and not wired to anything, and today's `ParsedMessage`/`UnparsedMessage`
([`messages.rs:79-94`](../../daq/daqapp/src/messages.rs:79)) carry no bus field. The key
is **`u32`-only** (msg_id without the extended flag) until a real dual-bus consumer
exists; the `(bus, u32)` shape stays documented above only for that future
re-scope. All Option B APIs use the `u32`-only key.

**Constraint 5 — No sync primitives in public types (Option B doc §5, verbatim).**
No `Arc`, `RwLock`, `Mutex`, `Condvar`, or `Atomic*` appears in any public type,
signature, or field. Every stateful public type documents its **single owning thread** in
its doc comment. The only cross-thread types are `mpsc::Sender<T>`/`Receiver<T>` (plain
`Send` handles) passed by the caller.
*This is Option B's defining invariant, and it is testable*:
`rg "Mutex|RwLock|Arc" daq/daqcore/src` must return **0 hits**, and it is an exit
criterion for **every** phase, not just Phase 4.

---

## 4. Phased implementation plan (Option B doc §8)

Each phase lists: goal, concrete file-level steps, the exact **test gate**, and a
**definition-of-done** checklist. A phase is not done until its test gate passes; the next
phase must not start before that. Constraint 5's grep (`rg "Mutex|RwLock|Arc"
daq/daqcore/src` → 0 hits) is re-run at the end of **every** phase.

### Phase 0 — Pure leaf-module moves, zero behavior change

**Goal.** Move the leaf modules into daqcore behind the §2.8 feature flags, with daqapp
behaving byte-for-byte identically. `thread.rs`, `state.rs`, `messages.rs` **stay in
daqapp** (they still name `crate::messages` types).

**Steps.**
1. Move `can/driver.rs` → `daqcore/src/can/driver.rs` (feature-gated
   `serial`/`udp`/`simulated`/`loopback`).
2. Move `can/bus_load.rs` → `daqcore/src/can/bus_load.rs`.
3. Move `can/daq_parser.rs` → `daqcore/src/can/logger.rs` (`DaqLogger`).
4. Move `can/bootloader.rs` → `daqcore/src/can/firmware.rs` and
   `bootloader_protocol.rs` → `daqcore/src/can/firmware_protocol.rs` (feature `firmware`).
5. Move `hil/` (`engine.rs`, `run.rs`, `config.rs`) → `daqcore/src/hil/` (feature `hil`);
   `config::list_available_tests` gains an explicit base-dir argument.
6. Move `connection.rs` → `daqcore/src/connection.rs`; `formatter.rs` →
   `daqcore/src/formatter.rs` (feature `formatting`).
7. Fold `util::can` ID helpers into `daqcore::can` (already there: `can.rs` 26 lines).
8. Add the §2.8 dependencies to `daqcore/Cargo.toml` behind features; daqapp depends on
   daqcore with the full feature list; leave `pub use` shims at every old daqapp path.

**Test gate.** `cargo test` green (workspace, resolver 3, edition 2024);
`rg "pub mod (driver|bus_load|hil|formatter)" daq/daqapp/src` → 0 hits (only shims remain);
daqapp builds against daqcore features; **no behavior diff** — diff `main.rs` boot logs
old vs new.

**Definition of done.**
- [ ] All §2 rows for leaf modules moved; daqapp has `pub use` shims at old paths.
- [ ] `thread.rs` / `state.rs` / `messages.rs` still in daqapp, untouched.
- [ ] Boot-log diff empty; `cargo test` green; Constraint-5 grep 0 hits.

### Phase 1 — `Time`, `Timeline`, `RamCache`, `Session` land in daqcore; shadow ingest

**Goal.** The pure data modules exist in daqcore with full unit test coverage, and the app
shadows its legacy state (`frozen.rs`) with a `Session`, proving equivalence while widgets
keep using the old path.

**Steps.**
1. Create `daqcore/src/timeline.rs` (`Time`/`Track`/`Timeline`, API of §2.6).
2. Create `daqcore/src/cache.rs` (`RamCache`, `CachedFrame`, `FramePayload`,
   `CAPACITY = 500_000` — the **P5 `u32`-only** shapes; no `bus` field).
3. Create `daqcore/src/session.rs` (`Session` of §2.6).
4. Write the unit-test scenario list: all **9 track combos** (Constraint 2),
   equal-timestamp ordering, backward seek after eviction, `observe` idempotence,
   `push_batch` sorted-merge correctness, capacity eviction at `CAPACITY`, and
   "undecoded-newest refreshes `latest`" (P1).
5. daqapp: `DAQApp` gains `session: Session`; `update()` drains and **also** ingests into
   the shadow session while widgets keep using `frozen.rs`; a **debug-mode equality
   harness** compares shadow vs legacy per frame.
6. One-line swap: frame timestamp at parse in
   [`thread.rs:39`](../../daq/daqapp/src/can/thread.rs:39) becomes `Time::now()`
   (Constraint 1); `chrono` display labels unchanged.

**Test gate.** daqcore unit tests green **headless**; a 10-minute simulated-bus run with
**zero harness mismatches** (shadow `Session` vs `frozen.rs`); chrono display labels
unchanged in the UI.

**Definition of done.**
- [ ] `timeline.rs` / `cache.rs` / `session.rs` in daqcore, APIs exactly as §2.6.
- [ ] Full unit-test list green headless (no hardware, no eframe).
- [ ] 10-min sim run: zero shadow-vs-legacy mismatches.
- [ ] `Time` stamped at parse; Constraint-5 grep 0 hits.

### Phase 2a — In-place can-thread refactor (still in daqapp) — **no observable behavior change**

**Goal.** Decompose the 421-line can thread into the §2.4 components **inside daqapp**
(`daqapp/src/can/`), abolish `State`, collapse the four `chrono::Local::now()` sites into
the single injected per-iteration clock, and route every send through `Events` —
**with zero observable behavior change**. The crate boundary is not touched yet.

**Steps.**
1. Create `daqapp/src/can/{run,rx,decode,tx,connection,firmware,events}.rs` per the §2.4
   component table; split the ~310-line `start_can_thread` loop
   ([`thread.rs:107-420`](../../daq/daqapp/src/can/thread.rs:107)) into the thin `run()`
   (~120–150 lines) + components.
2. Abolish the 13-field `State`
   ([`state.rs:3-17`](../../daq/daqapp/src/can/state.rs:3)); its send-scheduling code
   becomes `SendTable`, its firmware code `FirmwareSession`, its driver lifecycle
   `ConnectionManager`.
3. Inject one `now` per loop iteration; `FrameDecoder` is the only `Time`-stamp site.
4. All ~12 scattered `.send(...).expect()` call sites collapse into `events.emit(...)`.
5. Channel types (`MsgFromUi` / `MsgFromCan`) are **unchanged** in this phase.
6. Add a debug-only `Events` mirror for the event-stream diff.

**Test gate (this is the proof that behavior did not change).**
- Headless unit tests green on the new components: send-table due-time matrix with fake
  clocks, firmware-session script, decode on hand-built frames, `RxLoop` error
  classification against `LoopbackDriver`.
- **10-minute loopback + serial scenario: old-vs-new event-stream diff (via the debug
  mirror) shows zero mismatches.**
- Manual/scripted verification of all 4 connection sources, DBC reload, send queue, HIL,
  and a firmware update.
- `rg "chrono::Local" daq/daqapp/src/can` → 0 hits.

**Definition of done.**
- [ ] §2.4 component files exist under `daqapp/src/can/`; `State` struct deleted.
- [ ] Event-stream diff: zero mismatches over the 10-min loopback+serial run.
- [ ] Cadences unchanged (50 ms HIL, 200 ms bus-load, 8×4 ms firmware).
- [ ] `rg "chrono::Local" daq/daqapp/src/can` → 0 hits; Constraint-5 grep 0 hits.

### Phase 2b — `can_thread` module: the refactored thread moves to daqcore — **no observable behavior change**

**Goal.** Move the **already-verified** Phase 2a module into `daqcore::can_thread` as a
**mechanical crate move with no further restructuring**, and rename to the §2.2 public
API.

**Steps.**
1. Move `daqapp/src/can/{run,rx,decode,tx,connection,firmware,events,types}.rs` →
   `daqcore/src/can_thread/`.
2. Rename `MsgFromUi` → `CanThreadCommand`, `MsgFromCan` → `CanThreadEvent`; merge
   `ParsedMessage` / `UnparsedMessage` into `ParsedFrame`; swap any remaining
   `DateTime<Local>` for `Time` (the §2.2 API, verbatim).
3. Delete `messages.rs` from daqapp (its `SendAmount` / `AddSendMessage` /
   `FirmwareProgress` types now live in `can_thread::types`).
4. daqapp `main.rs` spawns via `spawn_can_thread(config, out_tx, cmd_rx)`; `update()`
   drains `CanThreadEvent` and calls `session.ingest_frame` / status updates.
5. Widgets **still read `frozen.rs`** (fed from the same drain) for one more phase.
6. HIL and firmware tick in-thread per sub-decision (a).

**Test gate.**
- `rg "start_can_thread|MsgFromUi|MsgFromCan|can::state" daq/daqapp/src` → 0 hits.
- The **entire Phase 2a scenario suite re-run green against the daqcore module** (unit
  tests + 10-min loopback/serial event-stream diff).
- `CanThreadCommand::Stop` joins cleanly (no hang, no leak).

**Definition of done.**
- [ ] `daqcore::can_thread` is the §2.2 API verbatim; its internal components are
  module-private.
- [ ] daqapp spawns via `spawn_can_thread`; zero daqapp-side can-thread code remains.
- [ ] Phase 2a suite green against daqcore; behavior unchanged (stream diff zero
    mismatches); Constraint-5 grep 0 hits.

### Phase 3 — Widget read migration; delete `frozen.rs`

**Goal.** Migrate all 14 widgets from `Frozen<T>` snapshots to plain `&RamCache` /
`&Timeline` borrows via `WidgetContext`, and delete `frozen.rs` (and `messages.rs` if
still present).

**Steps.**
1. Follow the 14-widget migration table in
   [`ram_cache_design_v2.md:346-387`](ram_cache_design_v2.md:346): each widget switches
   from `Frozen<T>` to `wc.cache()` / `wc.timeline()` borrows.
2. `WidgetContext` drops `can_messages`
   ([`widgets.rs:21-27`](../../daq/daqapp/src/widgets.rs:21)); ingestion is already in
   `update()` drain (Phase 2b), so `show()` is render-only.
3. Timeline bar writes `set_*` / `release_*` directly on
   `self.session.timeline_mut()` (UI thread is the sole owner — no messaging round-trip).
4. LIVE badge becomes the naive `latest()` read (P1).
5. Delete `daq/daqapp/src/frozen.rs` and `daq/daqapp/src/messages.rs`.

**Test gate.**
- `rg Frozen daq/daqapp/src` → 0 hits.
- `messages.rs` deleted; `frozen.rs` deleted.
- 60 fps scope/table/list render reading straight from `&RamCache` (manual perf check:
  no visible frame drops vs pre-migration at the same bus load).

**Definition of done.**
- [ ] All 14 widgets read via `WidgetContext` borrows; no snapshot types remain.
- [ ] `frozen.rs` and `messages.rs` removed from daqapp.
- [ ] Timeline bar drives `Timeline` directly; all 9 track combos reachable from the UI.
- [ ] Constraint-5 grep 0 hits.

### Phase 4 — `.log` backfill in daqcore

**Goal.** Offline `.log` files load into the cache via the §2.7 backfill path,
disconnected-only, lock-free.

**Steps.**
1. Create `daqcore/src/backfill.rs`: `spawn_log_worker(log_path, out, cancel)` —
   `log_parse` per record → `CachedFrame` (the log's own wall-clock timestamps) →
   time-ascending `BackfillBatch` chunks (~2 000 frames).
2. daqapp: file-open dialog gated on `Disconnected`; on open, spawn the worker and store
   the `Receiver<BackfillBatch>` in `DAQApp.backfill_rx`; the drain in `update()` step 2
   runs `session.push_batch(chunk)` (one batch per `try_recv`).
3. Cancel path sends on the `cancel` channel and aborts the worker.

**Test gate.**
- 10-minute `.log` file: loads, renders, scrubs correctly.
- Cancel mid-load aborts the worker cleanly.
- **`rg "Mutex|RwLock|Arc" daq/daqcore/src` → 0 hits** — Option B's defining invariant,
  and the exit test for every phase (re-asserted here for emphasis).

**Definition of done.**
- [ ] `backfill.rs` in daqcore; worker spawner matches §2.7.
- [ ] Backfill possible only while Disconnected; UI refuses otherwise.
- [ ] Zero locks in the path; Constraint-5 grep 0 hits.

### Phase 5 — SQLite + Data Source management

**Goal.** Persistent storage as another frame source for the same single-owner cache,
plus the Data Sources management panel.

**Steps.**
1. SQLite as **another `Vec<CachedFrame>` producer** feeding
   `session.push_batch` on the UI thread — no engine-thread coordination needed
   (this is more natural in Option B than in Option A, where it required the shared lock).
2. Data Sources panel: list/add sources, and **source switch = one `Session` reset**
   (rebuild `Session::live` + re-ingest from the chosen source).

**Test gate.**
- SQLite-backed view renders identically to the RAM view (side-by-side comparison on the
  same dataset).
- Source switch is one `Session` reset with no stale frames from the previous source.

**Definition of done.**
- [ ] SQLite source produces `Vec<CachedFrame>` batches into the UI-thread `Session`.
- [ ] Data Sources panel works; switch = one `Session` reset.
- [ ] Constraint-5 grep 0 hits.

**Why this order (vs Option A).** (1) No "engine handle" phase — Phase 2 is
*refactor-then-move*: 2a decomposes the thread in place (behavior-preserving,
stream-verified) and 2b moves the verified module with the existing channel pattern.
(2) The shadow-cache phase (1) precedes the thread move, so the riskiest change (thread
relocation) lands on top of already-proven cache code. (3) Constraint 5's invariant test
runs in **every** phase.

---

## 5. daqcli

`daqcli` today is a 6-line stub
([`daq/daqcli/src/main.rs:1-6`](../../daq/daqcli/src/main.rs:1)). Option B's CLI is
**structurally identical to daqapp's main loop**: spawn the can thread, own a `Session`,
drain, ingest, evict, print.

```rust
// daqcli main (sketch, Option B doc §6)
fn main() {
    let cli = clap::Parser::parse(); // connect | watch <msg> | status | send | hil run <preset> | backfill <file>

    let (out_tx, out_rx) = mpsc::channel();
    let (cmd_tx, cmd_rx) = mpsc::channel();
    let mut can_thread = daqcore::can_thread::spawn_can_thread(config_from_cli(&cli), out_tx, cmd_rx);
    let mut session = daqcore::Session::live(Time::now(), cli.window, cli.follow, cli.retention);

    loop {
        // command pump for interactive subcommands (send / hil run / arm ...)
        pump_stdin(&mut cmd_tx, &cli);

        while let Ok(event) = out_rx.try_recv() {
            match event {
                CanThreadEvent::Frame(f) => {
                    session.ingest_frame(&f);
                    if let Some(watch) = &cli.watch {
                        if f.msg_id == watch.id { print_frame(&f); } // prints live
                    }
                }
                CanThreadEvent::BusLoad { .. } => if cli.status { print_bus_load(..); }
                CanThreadEvent::Hil(s) => if cli.hil { print_hil(&s); }   // hil run <preset>
                CanThreadEvent::FirmwareProgress(p) => { print_progress(&p); }
                _ => { /* status line updates */ }
            }
        }
        session.evict_now();
        std::thread::sleep(std::time::Duration::from_millis(10)); // CLI cadence, not 60fps
    }
}
```

**Subcommand mapping.**
- `connect` / `status` → `Connect(source)` command + event readout.
- `watch <msg>` → filter `Frame` events live (or, for history,
  `session.cache().frames_before` + `signal_series` after a scrub).
- `send` → `AddSendMessage` / `DeleteSendMessage`.
- `hil run <preset>` → `Hil(HilCommand::StartTest)` + render `Hil` snapshots.
- `backfill <file>` → `spawn_log_worker` + `session.push_batch` (no GUI, prints counts).

**Why Option B supports daqcli (peer, not client).** In Option A, daqcli would be a
*client* of the `Engine` — sending `EngineCommand`s and receiving `EngineEvent`s — and
`watch` would have to read frame data from shared locked state because Option A
deliberately keeps frame data *off* the event channel. In Option B, daqcli is a **peer**:
it owns its own `Session` and spawns its own `spawn_can_thread` in a plain runtime loop
(no eframe, no UI thread). No duplication exists because `Session` carries the shared
~40 lines of per-tick orchestration. And `watch` needs every frame — Option B's channel
already delivers every `Frame` event to the owner by design, so daqcli gets the full
stream **for free**. Feature-wise, daqcli builds slim
(`default-features = false, features = ["serial"]`), so a no-serial daqcli has no
`slcan`/git dep at all — something Option A's monolithic `Engine` cannot offer.

---

## 6. Open questions (carried from Option B doc §10)

These are open, not blocking. Each has a **recommended answer — recommended, not yet
ratified**; ratify when you reach the relevant phase, and record the decision here.

1. **`max_retention` default.** 600 s (10 min) at 200 frames/s ≈ 120k frames ≈ well under
   `CAPACITY`; and should the `Session::live` args in daqapp read it from
   `settings.json`? — *Recommended: 600 s default, read from `settings.json` (it is user
   tuning, same category as window size).*
2. **One `Frame` event vs two (`Parsed`/`Undecoded`).** Option B merges today's two
   `MsgFromCan` variants into one `Frame { decoded: Option<DecodedMessage> }` (§2.2).
   Keep the split instead for 1:1 diff-ability with
   [`messages.rs:15-34`](../../daq/daqapp/src/messages.rs:15)? — *Recommended: keep the
   merged single `Frame` variant (cleaner; the Phase 2a event-stream diff compares
   against today's stream anyway, where the mapping is trivial).*
3. **Phase 2 shadow period for widgets.** Option B keeps `frozen.rs` fed from the new
   `CanThreadEvent` drain for one phase (widgets migrate in Phase 3). Acceptable, or
   migrate widgets in the same phase as the thread move? — *Recommended: keep the
   separate Phase 3 as planned (smaller, independently verifiable PRs).*
4. **slcan git dep in a library crate.** Accept it behind the `serial`/`udp`/`simulated`
   features (any daqcore default build pulls it), or pin/vendor it in the workspace
   `[patch]` section as part of Phase 0? — *Recommended: pin the rev in the workspace
   `[patch]` section in Phase 0 (keeps the git dep reproducible without vendoring
   weight).*
5. **`Session` naming and surface.** Is `ingest_frame` / `evict_now` / `push_batch` the
   right vocabulary, and should `Session` own the `max_retention`/window defaults, or
   stay a dumb cache+timeline pair with the caller computing the evict floor? —
   *Recommended: keep the §2.6 vocabulary; `Session::live` takes all four args explicitly
   (no hidden defaults) so daqapp/daqcli stay symmetric.*
6. **daqcli default features.** Slim-by-default (`default-features = false` + `serial`)
   vs same feature set as daqapp for developer convenience? — *Recommended:
   slim-by-default per §2.8 (it is the differentiator; add features per subcommand as
   needed).*
7. **HIL event cadence.** Keep today's 50 ms `HilSnapshot` throttle
   ([`thread.rs:175-186`](../../daq/daqapp/src/can/thread.rs:175)) unchanged, or emit on
   status *change* (cheaper for daqcli, different semantics)? — *Recommended: keep the
   50 ms throttle unchanged in this migration (behavior-preservation mandate; revisit in
   a follow-up).*
8. **Codifying the no-sync rule.** Should Constraint 5 be codified as a compile-fail
   doc-test in daqcore (e.g., asserting `Session: !Sync`), or is the grep-style exit
   criterion (`rg "Mutex|RwLock|Arc" daq/daqcore/src` → 0) enough? — *Recommended: the
   grep in every phase's exit criteria is enough for now; add the compile-fail test in
   Phase 1 while the modules are fresh (cheap then, costly later).*
9. **Refactor sequencing and granularity (Phase 2a).** Is *refactor in daqapp first,
   move second* the right call (two reviewable PRs, one extra phase) vs. refactor and
   move in a single phase (fewer phases, one giant PR)? And is the §2.4 component
   granularity right — e.g., merge `RxLoop` + `ConnectionManager` into one `io`
   component, or keep the HIL `process_parsed` call in the run loop rather than inside
   `FrameDecoder`? — *Recommended: keep refactor-then-move (it is the crux of Option B's
   low-regression argument); keep the §2.4 granularity as-is (the split exists so Phase
   2b is a mechanical move — re-merging components erodes that).*

---

## 7. Known ground facts (verified against source on `millan/ram_cache` — trust these)

- Current drain + repaint: [`app.rs:216-263`](../../daq/daqapp/src/app.rs:216) (drains
  `can_to_ui_rx` into a frame-local `can_messages: Vec<MsgFromCan>`, repaints
  unconditionally at line 263); ingestion is entangled with rendering because widgets
  receive `can_messages` during `show()` ([`widgets.rs:68-78`](../../daq/daqapp/src/widgets.rs:68)).
- [`messages.rs`](../../daq/daqapp/src/messages.rs) (106 lines): `MsgFromUi` (3-13),
  `MsgFromCan` (15-34), `SendAmount` (37-70), `AddSendMessage` (72-77),
  `ParsedMessage`/`UnparsedMessage` (79-94, carry `is_msg_id_extended`, **no bus field**),
  `FirmwareProgress` (97-105).
- [`can/thread.rs`](../../daq/daqapp/src/can/thread.rs) (421 lines): constants (3-12),
  `process_can_frame` (31-105, chrono stamp at :39, HIL callback at :56, inline sends at
  57-60 and 85-88), `start_can_thread` (107-420); `chrono::Local::now()` at
  :39 and :220.
- [`can/state.rs`](../../daq/daqapp/src/can/state.rs) (215 lines): 13-field `State`
  (3-17), firmware protocol (55-144, inline event sends), send scheduling (159-213,
  `DateTime<Local>`).
- [`can/driver.rs`](../../daq/daqapp/src/can/driver.rs) (435 lines): `Driver` trait
  (33-43), 4 impls, `create_driver` (424-434).
- The only `CanBus*` type in source is `CanBusSpeed` at
  [`connection.rs:51`](../../daq/daqapp/src/connection.rs:51) — there is no `CanBus`
  enum (that is why Patch P5 is safe).
- daqcore today: [`daqcore/src/lib.rs`](../../daq/daqcore/src/lib.rs) (22 lines:
  `pub mod can; pub mod log_parse;` + dummy `add`/`subtract`); deps
  [`daqcore/Cargo.toml`](../../daq/daqcore/Cargo.toml) (`can_decode 0.7.2`,
  `can-dbc 9.0.0`, `chrono 0.4.44`, `log`, `bytemuck 1.25.0`, `csv 1.4.0`).
- Workspace: `daq/` with members `daqapp` / `daqcore` / `daqcli`, resolver 3,
  edition 2024, branch `millan/ram_cache`.
- HIL engine: `HilEngine` at [`hil/engine.rs:47`](../../daq/daqapp/src/hil/engine.rs:47);
  `process_parsed` at [:101](../../daq/daqapp/src/hil/engine.rs:101) (signature
  `&messages::ParsedMessage` today → `&ParsedFrame` after the move); 50 ms snapshot
  throttle at [`thread.rs:175-186`](../../daq/daqapp/src/can/thread.rs:175).
