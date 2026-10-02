# daqapp RAM Cache + Timeline — Implementation Handoff

Status: ready to implement. This handoff tells a fresh task where the planning stands and exactly how to start coding. Read order before writing any code:
1. This file.
2. `docs/daqapp/ram_cache_design_comparison.md` — the **authoritative plan** (synthesis + patches P1–P4 + final data structures + phased plan).
3. `docs/daqapp/ram_cache_design_v2.md` — Doc B: the detailed base design (API shapes, migration table for all 14 widgets, phasing, file list). Note its §3.4 `partition_point` claim is wrong (fixed by patch P3).
4. `docs/daqapp/ram_cache_design_outdated.md` — Doc A (codex): **rejected as a base** (violates all 3 constraints) but 4 ideas were adopted from it (test scenario list, sorted-merge backfill, visible eviction, decimation-as-render-projection).
5. `DAQ_Master_Plan.drawio` — the long-range system goal this work is building toward.

## Non-negotiable constraints (user-mandated)

1. **Wall-clock timestamps everywhere in the timeline.** The CAN thread applies the (corrected) timestamp at parse time. `Time(i64)` = Unix millis UTC; chrono only for display labels.
2. **`start` / `end` / `setpoint` each march independently.** Each is `Track::{Marching, Frozen}`; all 9 combinations must be expressible. Never silently move a Frozen field; clamp the other side. Invariants: `start <= setpoint <= end`.
3. **Simple, deliberately naive cache.** One time-ascending buffer + one latest-value `HashMap`. No multi-index, no Arcs, no revision counters, no coverage metadata in v1.

## The plan in one paragraph

Build `data/{time,timeline,cache}.rs` in `daq/daqapp/src/` with `Time`, `Timeline` (3 independent tracks), and `RamCache` (`Vec<CachedFrame>` + `first_index` offset + `latest: HashMap<(CanBus,u32), CachedFrame>`, `CAPACITY = 500_000`). The UI thread owns everything — no Arc/Mutex; the CAN thread keeps sending `ParsedMessage`/`UnparsedMessage` over the existing unbounded mpsc, only the timestamp field changes from `DateTime<Local>` to `Time` (one-line swap at `can/thread.rs:38`). In `DAQApp::update()`: drain mpsc → build `CachedFrame` → `cache.push` → `timeline.observe(ts)` → once per drain `cache.evict(...)` with a retention floor → widgets render from `&RamCache` + `&Timeline` via `WidgetContext`. Widgets migrate off their private stores and `Frozen<T>` is deleted in Phase 3.

## Required patches to Doc B (already decided, do not re-litigate)

- **P1 (scrub gap):** `latest()` is newest, not at-setpoint. Ship v1 with naive `latest()`; when `setpoint` is Frozen, table/battery widgets show a "LIVE" badge. Optional Phase 3b: `latest_at(bus, id, ts)`.
- **P2 (eviction floor):** `evict(max(timeline.start(), now − max_retention))`; `max_retention` is config (default 600 s or until CAPACITY binds, whichever tighter). Scrubbing past the retained tail renders an explicit empty state.
- **P3 (storage):** Use `Vec<CachedFrame>` + `first_index: usize`, NOT `VecDeque` (Doc B's `partition_point` claim was a factual error — it is nightly-only on VecDeque). This gives O(log n) `frames_in_range` via slice `partition_point` for free and is still "one naive structure."
- **P4 (ordering):** live `push` = append + `debug_assert(ts >= last)`; `push_batch` (backfill) = **sorted merge**, never append.

## Adopted from Doc A (keep these in scope)

- Unit-test scenario list for `timeline.rs`/`cache.rs` (no GUI): all 9 track-state combos, equal-timestamp ordering, backward seek after eviction, `observe` idempotence, `push_batch` merge order, capacity eviction, unparsed-newest refreshes `latest()` (table renders "undecoded", not stale decoded).
- Eviction visibility: a small "N frames evicted" counter in the status area when eviction drops frames that were within the previously visible window.
- Scope decimation is a render-time projection, not acquisition-time data loss (replaces `decimation_counter`).

## Final data structures (from the comparison doc §4 — authoritative)

```rust
pub struct Time(i64);            // wall-clock Unix millis UTC

pub enum Track { Marching, Frozen }
pub struct Timeline {
    start: Time, end: Time, setpoint: Time,
    start_track: Track, end_track: Track, setpoint_track: Track,
    window_secs: f64,            // Marching start: start = end - window
    follow_offset_secs: f64,     // Marching end:  end   = max(end, latest + offset)
    max_retention: Duration,     // P2 eviction floor
}
// observe(latest) -> bool; set_*/release_* freeze+clamp; range(); setpoint_range()

pub struct CachedFrame { timestamp: Time, bus: CanBus, msg_id: u32, payload: FramePayload }
pub enum FramePayload { Decoded { raw_bytes: Vec<u8>, decoded: DecodedMessage },
                        Undecoded { raw_bytes: Vec<u8> } }
pub struct RamCache {
    frames: Vec<CachedFrame>,    // time-ascending; oldest live frame at frames[first_index]
    first_index: usize,
    latest: HashMap<(CanBus, u32), CachedFrame>,
}
// push / push_batch(merge) / evict / latest / frames_in_range / frames_before / signal_series
```
Plus: add `Hash` to the `CanBus` derives in [`connection.rs`](daq/daqapp/src/connection.rs:49) (one line, verified missing).

## Phased plan (each phase must hit its exit criteria before the next starts)

- **Phase 1 — Foundation + Scope.** `data/{time,timeline,cache}.rs` + unit tests; CAN-thread timestamp swap; `update()` ingest/observe/evict; timeline bar UI (3 sliders + Marching/Frozen toggles + "Go live"); Scope migrated to `signal_series` over `setpoint_range()` ∩ local window (pause = freeze setpoint; drop `add_point`/`reference_time`/`decimation_counter`). **Exit:** live-follow looks identical to today; frozen setpoint freezes Scope; no new per-frame allocations.
- **Phase 2 — Table, List, Battery.** `latest()` for ViewerTable + both battery widgets (with P1 "LIVE" badge); `frames_before(setpoint, 200)` for ViewerList; remove their `Frozen<…>` usage; explicit unparsed-newest test.
- **Phase 3 — Remaining widgets, delete `Frozen`.** GgPlot, GpsPlot, Dynamics, Jitter migrated; `frozen.rs` deleted (`rg Frozen` → 0 hits). BusLoad stays local (computed telemetry, not a frame). LogParser/SendUi/Bootloader/Hil/CommandPalette unchanged. Optional 3b: `latest_at(ts)`.
- **Phase 4 — `.log` backfill.** `DataSource`/`FrameSink` trait + `LogFileSource` over `daqcore::log_parse` (read-only reuse, no daqcore changes); worker thread ships chunks over mpsc; `push_batch` sorted merge; progress + `AtomicBool` cancel; backfill only while Disconnected; retention floor + empty-state rendering.
- **Phase 5 — SQLite + data sources.** `SqliteSource` behind the same trait; "Data Sources" sidebar; export `timeline.range()` to `.log`. Doc A's session-id/schema-revision/coverage ideas re-enter here *behind* the query API only.

## How to start (first concrete steps of Phase 1)

1. `daq/daqapp/src/data/mod.rs`, `data/time.rs` (`Time` newtype: `now()` = `chrono::Utc::now().timestamp_millis()`, `label()` local "HH:MM:SS.mmm", `from_millis`, Ord/Copy/Clone/Debug derives).
2. `data/timeline.rs` per the API above + the full unit-test matrix (9 track combos, clamps, `observe` idempotence).
3. `data/cache.rs` per the structures above + tests (merge order, eviction floor, capacity, `frames_in_range` via slice `partition_point`, `signal_series`).
4. One-line swap: `can/thread.rs` `chrono::Local::now()` → `Time::now()`; `messages.rs` `ParsedMessage`/`UnparsedMessage` `.timestamp` type → `Time` (keep the `DateTime` only where the old per-widget paths still need it during migration — a small `Time::to_datetime_local()` shim is fine until Phase 3 removes `frozen.rs`).
5. `app.rs`: add `cache: RamCache`, `timeline: Timeline` fields to `DAQApp`; in `update()`, before widget render, drain → `push`/`observe` → `evict`; extend `WidgetContext` with `cache: &RamCache`, `timeline: &Timeline` (keep `can_messages` until Phase 2/3 so unmigrated widgets keep working).
6. `connection.rs`: add `Hash` to `CanBus` derives.
7. Timeline bar UI + Scope migration (last, so the foundation can be tested headless first).

## Open questions (do NOT block Phase 1; decide when you reach the relevant phase)

1. `max_retention` default: 600 s vs until CAPACITY binds — tune from measured traffic (Phase 1 profiling).
2. Table "LIVE badge" vs `latest_at` from day one (P1) — defer to end of Phase 2.
3. `.log` backfill timestamp provenance (host wall-clock at capture) — accept for v1, revisit Phase 5.
4. `signal_series` matches signals by name per frame — fine at v1 rates (~50–200 msgs/s; ceiling ~5k frames/s).

## Known ground facts (verified against source during planning — trust these)

- Two threads: CAN (spawned in `main.rs`) + eframe UI; single unbounded mpsc each way (`messages.rs`).
- `app.rs:216-263`: `update()` drains into frame-local `can_messages: Vec<MsgFromCan>`, unconditional `request_repaint()`.
- `widgets.rs:68-78`: `Widget::show` calls `handle_can_message` per message inside render — ingestion is currently entangled with rendering; moving ingest into `update()` fixes hidden-pane misses.
- `ui/scope.rs`: `window: VecDeque<(f64,f64)>` + `reference_time` + `is_paused` + `decimation_counter` (relative-seconds storage — this is what we're replacing).
- `ui/viewer_list.rs`: `Frozen<MsgList>`, 200-item self-eviction. `ui/viewer_table.rs`: `Frozen<DecodedMsgMap>` + `Frozen<UndecodedMsgMap>`.
- `frozen.rs`: `Frozen<T> { rt_data, frozen_data: Option<T> }` — deleted in Phase 3.
- Bus identity: `connection::CanBus { Vcan, Scan }` (no `Hash` yet). ID packing: `EXTENDED_ID_FLAG = 0x80000000` in `daqcore::can`.
- Persistence today: binary flat log via DaqLogger (CAN2 only), offline CSV via `daqcore::log_parse`; no SQLite yet.
