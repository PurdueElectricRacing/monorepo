# daqapp RAM Cache + Timeline — Design Comparison (codex vs v2)

Status: comparison & synthesis for the orchestrator. Source docs (read-only): `ram_cache_design_outdated.md` (referred to as **Doc A**, "codex") and `ram_cache_design_v2.md` (referred to as **Doc B**, "v2").

**VERDICT: Adopt Doc B (`ram_cache_design_v2.md`) as the base plan, with 4 targeted additions from Doc A and explicit rejection of Doc A's timeline model, cache structure, and session-relative time. Doc A is rejected as a base because it violates all three non-negotiable constraints; Doc B satisfies all three but has 4 concrete flaws (P1–P4) that must be patched before implementation.**

## 1. Summary of each doc

**Doc A — `ram_cache_design_outdated.md` (codex):**
- Architecture proposal for shared RAM cache + shared start/end/setpoint timeline; first slice = CAN table, CAN list, scope; SQLite/cloud/Parquet later, integration boundary only.
- Timeline = 6 concepts (requested range / resolved start-end / setpoint / resident coverage / available range / live head) plus a **`RangePolicy` enum** (`Fixed{start,end}` / `Rolling{width}` / `Growing{start}`) and a **`CursorPolicy` enum** (`FollowEnd` / `Fixed(Time)`) that it explicitly prefers *over* "three independently marching booleans."
- Time = **"integer session-relative time, preferably microseconds,"** with an "optional UTC anchor for labels."
- Cache = **two indexes over one record**: `by_message: HashMap<MessageKey, VecDeque<Arc<FrameRecord>>>` + `raw_order: VecDeque<Arc<FrameRecord>>`, where `MessageKey = (session, bus, numeric_can_id, standard_or_extended)`; plus `coverage` interval metadata, a monotonic `revision`, dedup by "stable_frame_id," and schema-revision tagging of decoded data.
- Threading: "keep cache mutation on the UI thread initially"; bounded live batch queue; drain with work budget; overflow → "record, surface a coverage gap."
- Backfill: "source manager translates requested ranges into missing-interval requests" with session ID, request generation, coverage outcome; budgeted off-thread merges.
- Phasing: 6 steps (independent testable modules → driver envelope identity → live ingestion + immutable frame view → shared controls + table/list/scope → other telemetry → async DB backfill). Strong testing section (11 named validation scenarios).

**Doc B — `ram_cache_design_v2.md` (prior architect subagent):**
- Explicitly scoped: `daq/daqapp` only, no firmware/`daqcore` changes except read-only reuse of `daqcore::log_parse`.
- Timeline = `Time(i64)` **wall-clock Unix milliseconds (UTC)** newtype + `Timeline { start, end, setpoint, start_track, end_track, setpoint_track, window_secs, follow_offset_secs }` with **three fully independent `Track::{Marching, Frozen}` booleans**, pure `observe(latest) -> bool`, `set_*`/`release_*` API, clamped invariants (`start <= end`, `setpoint <= end`, `start <= setpoint`).
- Cache = **deliberately naive two structures**: `frames: VecDeque<CachedFrame>` (time-ordered, all frames) + `latest: HashMap<(CanBus, u32), CachedFrame>` (O(1) current-value index); hard `CAPACITY = 500_000` frame ceiling; `evict(start)`; `frames_in_range`/`frames_before`/`signal_series` (linear now, binary search later — upgrade path claimed to be a method-body-only change).
- Threading: UI thread owns cache + timeline (option A); CAN thread unchanged except timestamp type; ingest/observe/evict all inside `DAQApp::update()` during mpsc drain; backfill via separate worker mpsc; "No Arc, no Mutex, anywhere."
- Backfill: `DataSource::fill(range, &mut dyn FrameSink)` trait + `LogFileSource` (Phase 4) reusing `daqcore::log_parse`, `SqliteSource` (Phase 5); backfill **only while `Disconnected`** to keep "cache holds exactly one era" trivial.
- Phasing: 5 phases with concrete exit criteria; includes a full widget-migration table for all 14 widgets (incl. "BusLoad stays local" and "LogParser/SendUi/Bootloader/Hil/CommandPalette unchanged"), an integration file list, and 4 open questions with recommendations.

## 2. Side-by-side comparison

| Axis | Doc A (timeline_design) | Doc B (design_v2) |
|---|---|---|
| **Timeline model** | 6-concept model (requested/resolved/setpoint/coverage/available/live head); `RangePolicy` + `CursorPolicy` enums | `start`/`end`/`setpoint` × 3 independent `Track` states; `window_secs` + `follow_offset` parameters |
| **Marching semantics** | `Rolling` (end follows live head, start = end − width), `Growing` (start fixed, end follows), `Fixed`; cursor `FollowEnd` or `Fixed` | Each field: Marching rule = `end = max(end, latest+offset)`, `start = end − window`, `setpoint = end`; Frozen = pinned |
| **Invariants** | `start <= setpoint <= end`; "freeze the range at its last valid position rather than silently moving the cursor" | Clamped in setters/observe: never silently move a *frozen* field; instead clamps the *other* side (`end`→`start`, `setpoint`→`min(setpoint,end)`) |
| **Timestamp representation** | Integer **session-relative** time (µs), "optional UTC anchor"; source ticks + receive time + provenance preserved | **Wall-clock UTC i64 millis** newtype; chrono only at display/edge; stamped `Time::now()` at parse in CAN thread |
| **Cache structure** | Double index: per-message `HashMap<MessageKey, VecDeque<Arc<FrameRecord>>>` + global `raw_order` deque; `MessageKey` 4-tuple incl. session + std/ext flag; coverage intervals; revision counter; stable-frame-ID dedup | Single `VecDeque<CachedFrame>` + `HashMap<(CanBus, u32), CachedFrame>` latest-index; 500k hard cap; nothing else |
| **Latest-value / index strategy** | Per-key deque ⇒ "latest within [start, setpoint]" is native; handles "unparsed newer frame must not hide older decoded frame" explicitly; multiplexed-signal latest-present lookup | `latest()` returns the overall-newest frame per (bus, id) — **not** setpoint-bounded; scrubbed table shows live newest (gap, see §3-B1) |
| **Memory bounding/eviction** | Dual budget: retention duration **and** total memory bytes (incl. indexes, decoded allocations, queues, snapshots); eviction removes from every index; "pausing does not grant unlimited retention"; report memory pressure | Frame-count cap only (500k ≈ 120 MB worst case); `evict(timeline.start())` per `update()`; no retention floor for scrub-back (gap, §3-B2) |
| **CAN-thread fast path** | Bounded batch queue, work-budgeted drain, overflow → coverage-gap record; CAN thread must never block | Zero new sync: existing unbounded mpsc; `update()` drain does `push` + `observe` + `evict`; "no allocation beyond what ParsedMessage already carries" |
| **Backfill / data source** | Source manager, missing-interval requests, session/request-generation/schema-revision/coverage-outcome metadata, dedup by stable frame IDs, empty-response = loaded coverage | `DataSource`/`FrameSink` traits, worker thread ships `Vec<CachedFrame>` chunks over mpsc, `push_batch`, cancel via `AtomicBool`; backfill only while disconnected |
| **Threading & ownership** | "UI thread initially"; explicitly defers shared-lock cache "unless measurements show insufficient" | UI thread owns everything; explicit A-vs-B option table; rejects `Arc<Mutex>` with reasoning |
| **Widget migration/phasing** | 3-widget first slice (table/list/scope), then "other telemetry views," then persistence; per-widget behaviors in a table; render buffers "keyed by cache revision, timeline, and configuration" | Full 14-widget migration table with concrete current fields → new read path per widget; 5 phases with testable exit criteria (`rg Frozen` → 0 hits) |
| **Frozen\<T\> treatment** | "Existing per-widget freeze controls should become shared timeline controls" | Explicit: pause = freeze `timeline.setpoint`; `frozen.rs` **deleted** in Phase 3 |
| **Testability** | Dedicated: modules "Test them without GUI/driver dependencies"; 11 named validation scenarios (equal timestamps, out-of-order inserts, backward seeks, queue overflow, capture-while-paused…) | Implied by clean module split (`data::{time,timeline,cache,source,backfill}`) and phase exit criteria, but no explicit test section |

## 3. Violations & shortcomings (against the 3 non-negotiable constraints)

### Doc A violations

**A1 — Violates Constraint 1 (wall-clock timestamps).** §"Timeline semantics": *"`Time` is an integer session-relative time, preferably microseconds. Keep an optional UTC anchor for labels and cross-source correlation."* The timeline itself would be relative/since-session time with wall-clock only as an optional display anchor — the exact opposite of the constraint. Making the UTC anchor primary instead would gut most of that section's rationale (its provenance/tick-mapping discussion is built around *not* trusting host clocks).

**A2 — Violates Constraint 2 (three independent marching tracks).** §"Timeline semantics": *"Prefer explicit policies over three independently marching booleans"* with `RangePolicy::{Fixed, Rolling, Growing}` + `CursorPolicy::{FollowEnd, Fixed}`. This model can express at most: (rolling-range × follow-or-pinned) × (fixed-range × follow-or-pinned) × (growing-range × follow-or-pinned) — **it cannot express "end Marching, start Frozen at a user-chosen past anchor, setpoint Frozen"** (its `Growing` only allows fixed-*start* with end-follow, and its `Fixed` allows no marching at all), nor "start Marching, end Frozen" (a shrinking window toward a fixed right edge). The constraint requires all 3×3 combinations of {marching, frozen} per field.

**A3 — Violates Constraint 3 (simple/naive initial cache).** §"Cache structure": `HashMap<MessageKey, VecDeque<Arc<FrameRecord>>>` + parallel `raw_order: VecDeque<Arc<FrameRecord>>` = a **dual multi-index** design, plus per-frame `Arc`s, a 4-tuple key including session and std/ext flag, interval `coverage` metadata, a `revision` counter, stable-frame-ID dedup, and schema-revision tagging. This is precisely the "overly complex multi-index or segmented design" the constraint says to reject for the initial draft. (Its own mitigation — "Hide storage behind queries so sorted chunks can replace it" — concedes the storage is not naive.)

**A4 — Design flaws (not constraint violations):**
- **F1:** No concrete file/line-level integration plan. The "incremental implementation" steps have no mapping to `app.rs`/`widgets.rs`/specific widget files, unlike Doc B §8. Risk of implementation drift.
- **F2:** "Bounded live batch queue" + "work budget" + "coverage gap" machinery for the initial live path is speculative — Doc B's observation that the existing unbounded mpsc + unconditional `ctx.request_repaint()` (verified at `daq/daqapp/src/app.rs:262`) means the drain can never stall rendering indefinitely is simpler and grounded.
- **F3:** `latest_message(key, start, setpoint)` semantics (a message last seen before `start` is "unknown in that view") are *better* for scrub correctness, but they depend on the per-key deques it's forced to build (A3).
- **F4:** Coverage/empty-interval metadata and "known empty intervals" are meaningful only once persistent sources exist; in v1 (live only) they add bookkeeping with no user-visible value.

### Doc B violations / shortcomings

**B1 — Scrub correctness gap in `latest()` (biggest flaw).** §3.2: `latest(bus, msg_id) -> Option<&CachedFrame>` returns the **overall newest** frame per key. §6 migrates ViewerTable and both battery widgets to `cache.latest(bus, id)`. But the product goal is that *every* widget renders "the world as of `setpoint`" when scrubbed. With `setpoint` frozen in the past, the table and battery widgets would still show the *live newest* value — inconsistent with Scope/GpsPlot/Jitter, which do honor `setpoint` via `frames_before`/`frames_in_range`/`signal_series`. The doc's §2.2 claim *"setpoint frozen… 'pause at a moment in the past'; live data keeps filling the cache, plots stay put"* is only half true: **plots stay put, tables don't.** This needs a v1 decision (see recommendation: document + ship with a "live" badge, or add `latest_at(ts)` in a small Phase 3b).

**B2 — Eviction destroys scrub targets.** §4.2: `self.cache.evict(self.timeline.start())` every `update()`. When `start` is Marching (`start = end − 10s`), frames older than 10 s are *permanently dropped* — so a user scrubbing backward into "history" finds nothing older than the rolling window. The doc never states a retention floor. (Doc A's "retain a bounded live tail… report memory pressure instead of silently presenting a complete view" is the right instinct.) At minimum: cap how far back `evict` may advance (e.g., `evict(max(start, oldest_retained))`) or document "the naive cache only supports scrubbing within the retained tail; older data requires backfill."

**B3 — Factual error on the "two-line" upgrade (§3.4).** *"partition_point is a method on VecDeque itself (stable since Rust 1.52)."* **False.** `partition_point` exists on slices/`Vec`; `VecDeque`'s version is nightly-only (`#![feature(vec_deque_partition_point)]`). A stable `VecDeque` doesn't even expose arbitrary indexing, so a slice-based binary search can't be bolted on without a change. The *strategy* (ship linear, upgrade later) is sound and costs nothing; the cited mechanism does not exist and the upgrade is *not* method-body-only — it needs `VecDeque → Vec` (or `Vec` + offset, or a nightly feature). Since `frames` is append-only + front-pop + occasional bulk merge, a `Vec` with a `first_index: usize` cursor is a clean stable alternative that *does* support O(log n) bounds via slice `partition_point`.

**B4 — `push()` "keeps deque time-ordered" is an unhandled contract.** Live SLCAN traffic is in-order, but Phase 4 backfill (`push_batch` from a `.log`) can interleave arbitrarily with the "only while disconnected" rule being the only thing that keeps a single era. If that rule is ever relaxed, or if a backfill chunk's timestamps span live-era times, the deque order invariant silently breaks and *every* query is wrong. Needs an explicit decision at `push` (assert / sort-merge / reject) — Doc A's "backfill batches are sorted and merged, not appended" is the correct rule and should be adopted.

**B5 — Minor:**
- `Time::now()` "at parse time in the CAN thread" is verified correct as the current stamping point (`daq/daqapp/src/can/thread.rs:38` uses `chrono::Local::now()`), but note local clock = wall clock; the switch to `chrono::Utc::now()` millis is a one-line change the doc already specifies.
- §3.3 memory estimate (~250 B/frame) is plausible but the `DecodedMessage` "~100–300 B" figure is unverified; it should be measured in Phase 1 (Doc A's "measure before tuning" applies).
- §7 `DataSource::fill` takes `&self` and streams over mpsc — fine, but the `downsample if denser than ~1 frame / 1 ms per msg_id` rule is lossy and undocumented in UI; flag as product decision, not a flaw per se.

**Verified-accurate codebase claims (both docs' shared context, spot-checked):** `messages.rs:80` `ParsedMessage`/`UnparsedMessage` carry `chrono::DateTime<chrono::Local>`; `connection.rs:49` `CanBus { Vcan, Scan }` indeed lacks `Hash` (Doc B's one-line claim correct, and its self-correction of the task's "is_bus_1" note is accurate); `app.rs:218` drains the mpsc into `can_messages` per frame and calls `ctx.request_repaint()` unconditionally; `widgets.rs:75` delivers `can_messages` to each rendered widget (Doc A's "hidden panes can miss traffic" is **true** — `handle_can_message` runs in `show()`; both docs are right that ingestion is entangled with the widget loop, and Doc B's move of ingestion into `update()` fixes it for all widgets uniformly); `ui/scope.rs:33` has `window: VecDeque<(f64,f64)>`, `reference_time`, `is_paused`, `decimation_counter` exactly as Doc B's migration table says; `ui/viewer_list.rs:16` `Frozen<MsgList>` + `make_space_for_new_message` and `ui/viewer_table.rs:16` `Frozen<DecodedMsgMap>`/`Frozen<UndecodedMsgMap>` confirmed.

## 4. Recommended final plan (synthesis)

**Base: Doc B, wholesale** (time model, timeline, cache, threading, migration table, phasing, open questions), **plus** these per-idea decisions:

**Take from Doc B (keep as written):**
1. `Time(i64)` wall-clock Unix millis UTC (§2.1) — satisfies Constraint 1 exactly; chrono only at `label()`/migration shim.
2. `Timeline` with three independent `Track::{Marching, Frozen}` per `start`/`end`/`setpoint` + `window_secs`/`follow_offset_secs` (§2.2) — satisfies Constraint 2 exactly; its 4 named combinations map to today's UX (live-follow, pause-at-past, fixed-edge window, growing window).
3. Naive `RamCache { frames: <time-ascending buffer>, latest: HashMap<(CanBus,u32), CachedFrame> }` + `CAPACITY=500_000` (§3) — satisfies Constraint 3; exactly the "single time-ascending structure + simple latest-value lookup" the constraint asks for. Keep the naive `CachedFrame`/`FramePayload` shapes.
4. UI-thread ownership, zero new sync primitives, ingest/observe/evict in `update()` (§4.1–4.2) — grounded in the actual `app.rs:216` loop.
5. `DataSource`/`FrameSink` backfill trait + worker-mpsc + `push_batch` (§7); backfill only while `Disconnected` (their §10-Q3 recommendation is right for v1).
6. 5-phase plan with exit criteria (§9) and the 14-widget migration table (§6), including "BusLoad stays local" (verified: `MsgFromCan::BusLoad` is a computed telemetry value, not a frame).
7. `frozen.rs` deleted in Phase 3; pause = freeze `setpoint` (§6, §8).

**Take from Doc A (adopt these 4 ideas, patched to fit B's simpler shapes):**
1. **Testable modules + scenario list.** Add a test section to the plan: `timeline.rs`/`cache.rs` unit-testable with no GUI; must-cover scenarios: equal-timestamp ordering, backward seek after eviction, frozen-combination matrix (all 9 track states), `observe` idempotence, `push_batch` merge ordering, capacity eviction, `latest()` refresh on unparsed-after-parsed.
2. **"Backfill batches are sorted and merged, not appended."** Adopt as the explicit contract of `push_batch` — fixes B4. For v1 (single era, disconnected-only backfill) a sort-merge of the batch into the deque suffices; document it.
3. **Overflow/loss visibility, in spirit.** Keep Doc A's principle that *dropped data must be visible, not silent*, implemented cheaply: since the mpsc is unbounded, the only real loss is `CAPACITY` eviction — surface a small "N frames evicted" counter in the status area when eviction drops frames that were within the previously visible window. Reject the bounded-queue/work-budget machinery (F2 above) as premature.
4. **Unparsed-newest semantics.** In Doc B's shape this already falls out for free: `push()` overwrites `latest[(bus,id)]` with whichever frame arrived, decoded or not, and `FramePayload` tells the table to render "undecoded" — but make it an **explicit test** (Doc A's insight, Doc B's mechanism). Also adopt Doc A's "decimation is a rendering projection, not acquisition-time data loss" for Scope (replaces its `decimation_counter`).

**Reject from Doc A (with reasons):**
- **Reject `RangePolicy`/`CursorPolicy`** (violates Constraint 2 — cannot express all 9 track combinations; the enum's own "Fixed" variant even disallows marching entirely).
- **Reject session-relative `Time`** (violates Constraint 1; the provenance/tick-mapping discussion is for a future multi-source clock-mapping problem, not v1 — note it as a *future* concern under Phase 5+).
- **Reject the dual-index cache, `MessageKey` 4-tuple, `Arc<FrameRecord>`, `revision` counter, interval `coverage` metadata, stable-frame-ID dedup, schema-revision tagging** (violates Constraint 3; every one of these only earns its keep with persistent multi-source backfill — listed as *future upgrades* at Phase 5, not initial).
- **Reject bounded batch queue + work-budget drain** (premature; the existing unbounded mpsc + per-frame drain is verified adequate today).

**Patch Doc B's four flaws (required before implementation):**
- **P1 (B1):** Decide `latest()` vs `setpoint`. Recommended v1: keep naive `latest()`; when `setpoint` is Frozen, the table/battery header shows a "LIVE" badge (values are newest-known, plots are at setpoint). Add `latest_at(bus, id, ts)` as an optional Phase 3b if scrubbing the table proves essential — implementable then via the per-key approach Doc A described, *without* re-architecting the buffer.
- **P2 (B2):** Retention floor. `evict(max(timeline.start(), now - max_retention))` where the floor is a config (default e.g. 600 s, or until `CAPACITY` binds, whichever is tighter). Document: "scrubbing back beyond the retained tail shows an explicit empty state until backfilled" (Doc A's "render an explicit empty state" + "distinguish retained live history from evicted/unrecoverable history").
- **P3 (B3):** Correct the §3.4 record: `partition_point` is **not** a stable `VecDeque` method. Two honest options, choose in Phase 1 profiling: (a) keep `VecDeque`, accept O(n) scans, upgrade later by switching `frames` to `Vec<CachedFrame>` + `first_index: usize` (append/front-evict still O(1) amortized, slice `partition_point` becomes O(log n)); (b) start with the `Vec`+offset directly (≈ same code, strictly more capable). Recommend (b) — it's still "one time-ascending structure" and satisfies Constraint 3 while removing the false claim.
- **P4 (B4):** `push_batch` = sorted merge, not append (from Doc A); `push` = append + assert `timestamp >= last` (live path); on assertion failure (should be unreachable in v1) log + fall back to merge.

**Chosen concrete data structures (final, from the synthesis):**
```rust
// data/time.rs
pub struct Time(i64);            // wall-clock Unix millis, UTC. now() at parse in CAN thread.
                                 // label() -> local "HH:MM:SS.mmm" via chrono (display only).

// data/timeline.rs
pub enum Track { Marching, Frozen }
pub struct Timeline {
    start: Time, end: Time, setpoint: Time,
    start_track: Track, end_track: Track, setpoint_track: Track,
    window_secs: f64,            // Marching start: start = end - window
    follow_offset_secs: f64,     // Marching end:  end   = max(end, latest + offset)
    max_retention: Duration,     // P2: eviction floor (config)
}
// API: live(now, window) / observe(latest) -> bool / getters / is_live()
//      set_start|set_end|set_setpoint(t)  (freeze + clamp invariants)
//      release_start|release_end|release_setpoint()  ("Go live")
//      set_window_secs / set_follow_offset_secs
//      range() -> Range<Time>; setpoint_range() -> Range<Time>
// Invariants (clamp in setters/observe, never move a frozen field):
//   start <= end; setpoint <= end; start <= setpoint

// data/cache.rs
pub struct CachedFrame { timestamp: Time, bus: CanBus, msg_id: u32, payload: FramePayload }
pub enum FramePayload { Decoded { raw_bytes: Vec<u8>, decoded: DecodedMessage },
                        Undecoded { raw_bytes: Vec<u8> } }
const CAPACITY: usize = 500_000;
pub struct RamCache {
    frames: Vec<CachedFrame>,   // P3: Vec + offset (time-ascending, oldest at frames[first_index])
    first_index: usize,
    latest: HashMap<(CanBus, u32), CachedFrame>,
}
// API: new/clear/len/is_empty
//      push(f)                       // live path; assert f.timestamp >= last
//      push_batch(iter)              // P4: sort-merge into time order; single latest refresh
//      evict(start)                  // floor: max(start, now - max_retention); then CAPACITY
//      time_span() -> Option<Range<Time>>
//      latest(bus, id) -> Option<&CachedFrame>
//      frames_in_range(Range<Time>) -> Vec<&CachedFrame>   // O(log n + k) via slice partition_point
//      frames_before(ts, count) -> Vec<&CachedFrame>
//      signal_series(bus, id, signal_name, Range<Time>) -> Vec<(Time, f64)>
```
(Add `Hash` to `CanBus` derives — one line, verified needed.)

**CAN-thread → cache fast path (final):** unchanged mpsc; one-line change at `can/thread.rs:38` `chrono::Local::now()` → `Time::now()`; `ParsedMessage`/`UnparsedMessage` field type `DateTime<Local>` → `Time` in `messages.rs`. In `DAQApp::update()`: drain → for each message build `CachedFrame` (tag `self.can_bus`) → `cache.push` → `timeline.observe(ts)`; once per drain `cache.evict(timeline.start())`; render widgets from `WidgetContext { cache, timeline, … }` (cache is `&` borrowed, no per-frame clone — Doc A's "borrow it during rendering" adopted); HIL/Bootloader/BusLoad/MessageSent paths untouched.

**Phased plan (final):**
- **Phase 1 — Foundation + Scope:** new `data/{time,timeline,cache}.rs` + unit tests (Doc A scenario list); CAN-thread timestamp swap; `update()` ingest/observe/evict; timeline bar UI (3 × slider/drag + Marching/Frozen toggle + Go live); migrate Scope (`signal_series` over `setpoint_range()` ∩ local `window_duration_seconds`; pause = freeze setpoint; drop `add_point`/`reference_time`/`decimation_counter`, decimate at render). Exit: live-follow identical to today; frozen setpoint freezes Scope; no new per-frame allocations.
- **Phase 2 — Table, List, Battery:** `latest()` for table + both battery widgets (with P1 "LIVE" badge when setpoint frozen); `frames_before(setpoint, 200)` for list; delete `Frozen<…>` usage in these three + the undecoded-reconciliation pass; explicit test: unparsed-newest shows undecoded, not stale decoded.
- **Phase 3 — Remaining widgets, delete `Frozen`:** GgPlot (`signal_series`, 5-min lookback at setpoint), GpsPlot (`frames_in_range` on GPS id), Dynamics + Jitter (`frames_in_range` recomputed per render); retire `frozen.rs` (`rg Frozen` → 0); BusLoad declared non-data (stays local). Optional Phase 3b: `latest_at(ts)` if the table-LIVE-badge proves unacceptable.
- **Phase 4 — `.log` backfill:** `LogFileSource` over `daqcore::log_parse` (read-only), worker + `push_batch` (sorted merge), progress + `AtomicBool` cancel, backfill only while Disconnected, retention floor + empty-state rendering when scrubbing past the retained tail.
- **Phase 5 — SQLite + data sources:** `SqliteSource` same trait; "Data Sources" sidebar; export `timeline.range()` to `.log`; this is where Doc A's session identity, schema-revision, coverage/dedup metadata get re-introduced *behind* the query API (future upgrades only).
- **Future (noted, not planned):** per-key time-indexed latest (Doc A's `latest_message(key, start, setpoint)`), session/UTC clock mapping for UDP multi-source, memory-byte budgeting beyond the frame cap.

## 5. Open questions (post-synthesis)

1. **P1 decision:** Is "table shows LIVE newest while plots sit at a frozen setpoint" acceptable for v1 (recommended, with badge), or must the plan include `latest_at` from the start? Affects Phase 2 scope by ~1 day.
2. **`max_retention` default** (P2): propose 10 min or "until CAPACITY binds, whichever is tighter" — needs a number chosen from measured traffic (per Doc A: config, not architecture).
3. **`Vec`+offset vs `VecDeque`** (P3): recommend `Vec`+offset from day one; confirm no preference.
4. **Backfill timestamp provenance:** `.log` files were written with host wall-clock at capture time; if the host clock jumped during the recorded session, backfill frames may not line up with live-era expectations. Accept as-is for v1 (same clock domain), note for Phase 5.
5. **`signal_series` signal matching by name** scans `DecodedMessage.signals` per frame — fine at v1 rates; if a signal is wanted at the key level later, it folds into the same future per-key index (no separate work).

## Appendix: verified codebase citations

- `can/thread.rs:38` — `let timestamp = chrono::Local::now();` (parse-time stamping point; Doc B's one-line change target).
- `messages.rs:14-90` — `MsgFromCan` variants; `ParsedMessage.timestamp: DateTime<Local>` (line 80), `UnparsedMessage.timestamp` (line 87); no bus/std-ext identity on UI messages (Doc A's §Current implementation claim, confirmed).
- `connection.rs:49-55` — `CanBus { Vcan, Scan }` derives `Copy, Clone, PartialEq, Eq, Debug` but **not** `Hash` (Doc B's claimed one-line change, confirmed necessary).
- `app.rs:216-263` — `DAQApp::update` drains `can_to_ui_rx` into `can_messages`, unconditionally `ctx.request_repaint()` (Doc B §2.5 repaint claim, confirmed).
- `app.rs:55` — `can_messages: Vec<MsgFromCan>` is the frame-local vector Doc A describes ("drains that channel into a frame-local vector").
- `widgets.rs:21-27, 68-78` — `WidgetContext { can_messages: &[MsgFromCan], … }`; `Widget::show` calls `handle_can_message` per message before rendering (Doc A's "ingestion depends on rendering" claim, confirmed; Doc B's move to `update()` fixes it uniformly).
- `ui/scope.rs:32-38, 77-91` — `window: VecDeque<(f64,f64)>`, `reference_time: Option<DateTime<Local>>`, `is_paused`, `decimation_counter`, relative-seconds storage (Doc B §6 Scope row, confirmed).
- `ui/viewer_list.rs:15-17, 155-173` — `Frozen<MsgList>`, 200-item self-eviction via `make_space_for_new_message` (Doc B §6, confirmed).
- `ui/viewer_table.rs:15-18` — `Frozen<DecodedMsgMap>` + `Frozen<UndecodedMsgMap>` + `paused` (Doc B §6, confirmed).
- Factual error found in Doc B §3.4: `VecDeque::partition_point` is **not** a stable-Rust method (slice/`Vec` only; `VecDeque`'s is nightly `vec_deque_partition_point`). Upgrade-path strategy is sound; the stated mechanism and "no new field" claim are wrong (see §3-B3 / P3).
