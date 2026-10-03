# daqcore rebuild: CAN worker, shared cache, and timeline

Status: implementation in progress. This is the handoff source of truth. User decisions here supersede the older design proposals.

## Goal and decisions

Build fresh composable libraries in daqcore. The CAN worker owns drivers, decoding, sending, live logging, HIL, and firmware. The UI/main thread owns Session (cache and timeline), ingests frames once, and renders through ordinary borrows. Reuse sound protocol code and tests, rebuild ownership and orchestration, and delete legacy paths after adoption. Temporary adapters are migration scaffolding only.

- Order: CAN worker, then cache/timeline, then all daqapp widgets; include a small CLI.
- Global default window: 30 seconds. All widgets derive their display interval from start/end/setpoint.
- No retention duration or frame-count cap. Evict strictly before timeline.start(). Frozen start retains history indefinitely while end advances.
- Table and battery follow the selected cursor (overrides the former P1 LIVE exception).
- Keep slcan private to the serial adapter; preserve supported workflows and fix identified defects.
- Defer backfill, SQLite, Data Sources, new HIL transmission features, and decoded/logged CAN FD. Preserve existing log export.

## Contracts

Transport-neutral frames preserve ID, extended flag, kind, DLC, and payload. ParsedFrame has optional decoded content and a Time(i64) Unix-millisecond timestamp applied after reading, at decoding. Monotonic clocks are injected into scheduling, HIL, firmware, and bus-load components. Only the event emitter owns the outbound sender. Spawn creates a command channel and returns a sender/join-owning handle; Stop and disconnected channels clean up logging/driver/protocol state and terminate. Optional serial/udp/simulated/loopback/hil/firmware/formatting features permit headless builds without GUI or serial dependencies.

Timeline has three independent Marching/Frozen tracks (eight binary combinations). Marching end follows max(end, latest + offset); start follows end - window; cursor follows end. Clamp marching fields around frozen anchors to maintain start <= setpoint <= end without moving frozen fields. Setters clamp the requested field; release immediately recomputes against last observation.

Cache is sorted Vec<CachedFrame> + first_index + HashMap<u32, CachedFrame>. Push asserts append ordering; batches stable-sort/merge with existing frames before incoming equals. Session consumes frames by value, falls back to merge for clock regression, observes time, and evicts at start only. Prefix compaction releases discarded storage. Latest must match retained history, including equal timestamps. Queries include both boundaries. Session has no thread, channel, lock, or atomic.

Frame history queries use [start, setpoint]; end is the plot boundary. Table projects newest per ID in that interval. Battery reconstructs newest per module/cell/thermistor, not just latest per message ID. Projections borrow retained frames and add no persistent histories/indexes. Advancing start destroys old history; backward seeking cannot recover it. Keep presentation configuration local.

## Increments and exit gates

1. **Build baseline/types:** enable bytemuck derives, introduce neutral frames/Time/errors/connection, preserve legacy settings, pin slcan and optional dependencies, preserve offline parsing. Exit: independent core tests, settings round trips, no GUI dependency.
2. **Drivers/decoder:** bounded serial/UDP reads, direct UDP identity flags, unsupported-write errors, 200 Hz simulation, explicit DBC reload retaining working parser on failure, raw unknown traffic and FD recognition/counting. Exit: deterministic conversion/UDP/ID/decoding/timestamp tests.
3. **Lifecycle/sending:** single driver owner, separate send scheduler, commit count/period only after write success, validate sends, report failures, retry same source, stop on explicit disconnect, cancel firmware on source change. Exit: due-time/count/write-failure/queue/reconnect/event-order tests.
4. **Peripherals:** preserve firmware handshake/retries/armed mode/24-bit indexes/480 KiB boundary; sequential <=8 writes/burst with 4 ms pacing and abort on uncertainty. Remove HIL GUI/messages/path discovery; explicit base dir, observation-only, 50 ms snapshots. Monotonic bus load (1/5/10/30 s, 200 ms publication). Preserve logger format/rotation/flush/folder changes. Exit: existing firmware plus headless HIL/logger/load/cancellation tests.
5. **Worker composition:** thin command/lifecycle/tick/RX/log/event loop, no grab-bag State or embedded protocol knowledge, suppress ordinary sends and HIL decoding during firmware, interruptible idle waits, cleanup and join. Exit: full-worker scripted tests and prompt idle shutdown/channel-loss handling.
6. **Worker adoption/CLI:** app resolves paths/settings, temporary adapter, update delivers to hidden panes, delete legacy worker/drivers/HIL/firmware, CLI connect/watch with explicit source/DBC/filter and Ctrl-C (default serial+loopback). Exit: workflows through shared worker and headless CLI.
7. **Data modules:** pure timeline/cache/session, 30 s default/zero offset, start-only eviction/compaction, retained span/reset/stable batches without backfill worker. Exit: all track/boundary/ordering/clock/merge/eviction/latest tests.
8. **App session/controls:** ingest and evict before rendering; global field/track/window/pause/live/clear controls; Pause freezes cursor, Go live releases all, Clear resets live preserving window/offset. Preserve disconnect/same-source history, reset on explicit source change. Persist window/offset with backward-compatible defaults, fresh positions on launch. Exit: capture while paused, consistent controls, isolated sources.
9. **Widgets:** Scope/list/table, then battery/dynamics, then G-G/GPS/jitter. Delete replaced histories immediately, local durations/pauses/reference clocks/acquisition decimation. Keep table search/formatting/list 200-frame display limit; drawing-only decimation; multiplexed battery/charging/missing/stale indicators; transforms/maps/nominal jitter period. Shared temporal actions. Operational HIL/firmware/send state updates outside show. Bus-load buffer uses global start eviction/cursor display. Exit: shared pause/scrub, hidden-pane correctness, all widgets retained.
10. **Cleanup:** delete Frozen/messages/adapters/shims/dummy arithmetic. Formatter parsing/matching in core, file/default selection in app. WidgetContext supplies borrowed views/state/actions, no frame batches. CLI uses Session. Update docs and feature/dependency/synchronization checks. Exit: one worker, one ingestion path, one frame history, no compatibility layer or GUI dependency in core.

## Validation

- Build/test no-default core, individual optional capabilities, complete feature set, app and CLI.
- Worker: read/write failures, source transitions, reload, HIL completion, firmware recovery/cancellation, logger flush, channel closure, stop/join.
- Data: eight tracks, frozen conflicts, inclusive cursor, seeks after eviction, equal timestamps, stable merges, regression, prefix reclamation.
- Unlimited retention: >500,000 frames spanning >1 hour with frozen start, no truncation; then advance start and verify eviction.
- Widgets: hidden panes, shared pause/scrub, undecoded newest, multiplexed cells, charging state, GPS/G-G transforms, jitter intervals.
- Integration: settings, connections, sends, bootloader, HIL presets, formatting, existing log export.
- Performance: 10-minute simulation and 200/5,000 fps profiling; practical 60 fps with default marching window.
- Hardware: representative serial/firmware smoke tests, reported separately from automation.

Future log replay must reconstruct wall-clock time from relative ticks.

## Implementation record

- Initial inspection: standalone daqcore failed because bytemuck derive support was implicit; existing user documentation changes left intact.
- Saved this handoff before implementation.

### Implemented increments

1. **Baseline/shared types complete:** explicit bytemuck derive support; locked
   slcan revision; transport-neutral kind/DLC/identity/payload and wall timestamps;
   legacy serial settings and unit-variant loopback settings round-trip.
2. **Drivers/decoder complete:** bounded 10 ms serial/UDP reads, neutral adapters,
   direct UDP extended flag, read-only UDP failures, 5 ms simulation schedule,
   loopback, explicit reload preserving the last working DBC. FD remains visible
   as raw frames and contributes to load; decoded FD and FD binary logging defer.
3. **Lifecycle/sending complete:** one driver owner and monotonic reconnect
   deadline; separate validation/scheduling with commit after successful write.
   Failures distinguish retrying queues from terminal rejections. Invalid replacement
   removes the previous send consistently with the UI. Explicit disconnect disables
   retries. Ordered SourceSelected events establish app history boundaries.
4. **Peripherals complete:** protocol/package logic and firmware tests preserved;
   injected updater time, sequential eight-frame maximum burst, 4 ms spacing,
   cancellation on uncertain writes/source selection; explicit HIL directory and
   injected elapsed time, no GUI dependencies; monotonic bus-load windows and
   periodic publication; original binary logger and offline export remain.
5. **Worker complete:** thin composition loop, one event emitter, interruptible
   idle waits, explicit stop/join and cleanup on channel closure. Firmware suppresses
   ordinary sends and HIL observations; received raw/decoded telemetry remains
   available to the shared acquisition path.
6. **Worker adoption/CLI complete:** app uses the core handle, resolves worker
   resources before spawn and delivers operational events to all panes in update.
   Legacy orchestration/drivers/messages and duplicate protocols removed. CLI
   connect/watch uses the same worker and Session, feature-aware sources, optional
   ID filtering, Unix/Windows shutdown handlers; Unix Ctrl-C exercised.
7. **Data modules complete:** three independent timeline tracks with protected
   frozen anchors; sorted Vec/first_index/latest cache, inclusive queries and
   stable merge; recorded clock regression preserved at ingestion. Batch rebuilding
   clones only the latest frame per identity. Standard/extended identities remain
   distinct through the latest map. Start-only eviction compacts proportional
   discarded prefixes; no retention-duration/frame-count setting or limit.
8. **App session/controls complete:** one shared session, ingestion/eviction before
   borrowing; independent global tracks, window/offset, scrub slider, Pause, Go live
   and Clear. Preferences persist with defaults; launch positions initialize fresh.
   Disconnect/reconnect preserves history; an ordered source change resets it.
9. **Widgets complete:** scope/list/table/battery/dynamics/G-G/GPS/jitter use the
   selected shared interval. Removed private frame histories, local duration and
   pause snapshots. Multiplexed projections reconstruct cells/thermistors and
   charging values, with missing values distinct from zero. Latest projections
   walk backward; filled battery slots skip older payload work. Decimation affects
   drawing only. Operational HIL/firmware/send state stays separate; global bus-load
   samples merge clock regressions and follow start/setpoint.
10. **Boundary cleanup complete:** borrowed WidgetContext views/actions, core
    formatter parsing with app defaults/file selection, no legacy adapters or
    Frozen implementation, no dummy arithmetic. Added READMEs, design precedence
    notice, independent feature CI and ownership/dependency checks.

### Automated results

- Workspace all-feature/all-target tests: 33 core tests and 10 app tests passed;
  the manual rendering benchmark is ignored by the ordinary test suite and was
  explicitly run in release mode. CLI has no unit tests; subprocess smoke tests
  below exercise it.
- Minimal core check and individual feature builds/tests passed (serial, UDP,
  simulated, loopback, HIL, firmware, formatting). The final matrix was rerun
  after the batch-merge improvement.
- Formatting, whitespace and `python3 daq/check_boundaries.py` passed. The guard
  checks minimal dependency tree (no egui/eframe/serial/slcan), private serial
  transport usage and absence of explicit shared-state synchronization in core.
- Unlimited-retention test: 500,001 frames spanning 5,000 seconds retained with
  frozen start; advancing start evicts 400,000. Backward seeks cannot restore them.
- Worker tests cover successful send before loopback receive, unsupported versus
  transient write failures, cleanup, finite scheduling, reconnect deadline,
  explicit disconnect, command closure, receiver loss, and idle UDP shutdown.
  HIL completes from explicit fixture resources using injected monotonic time;
  existing firmware word-index/480 KiB/READY/streaming tests and armed cancellation
  pass. Logger folder change/drop preserves and flushes 16-byte records.
- Ten-minute real worker simulation: **120,001 frames**, **6,001 retained**,
  **1.147 ms shutdown**. The earlier interrupted soak was not counted.
- Release ingestion/latest-query profile, 600 simulated seconds each:
  200 frames/s: 6,001 retained, 127.6 ms total, 0.211 ms maximum tick;
  5,000 frames/s: 150,005 retained, 518.9 ms total, 3.464 ms maximum tick.
  Fixed the benchmark's own 32-bit counter overflow by using 64-bit counters.
- Release CPU rendering profile (scope + both battery panels, 30-second window,
  projection/egui/tessellation): 200 frames/s median 4.17 ms, p95 4.85 ms;
  5,000 frames/s median 14.59 ms, p95 15.71 ms, maximum 15.77 ms. Before reverse
  projections the 5,000 frames/s median was 23.31 ms. These are CPU measurements
  on this machine, not a guarantee for every pane layout or GPU.
- CLI smoke: loopback connect and simulated watch both exit successfully on
  SIGINT; approximately 5.05 ms / 3.06 ms exit, watch printed 101 frames.
- UDP socket tests required execution outside the network sandbox; all passed.
  After the session interruption, installed Nix pkg-config paths supplied the
  development libraries for serial/app builds; no dependencies were installed.

### Remaining external validation and deferred scope

Representative serial-adapter and firmware hardware smoke tests were **not run**:
no hardware target was provided. Interactive GUI/GPU rendering, platform-specific
Windows shutdown, and real maps/network resource behavior still need manual smoke
checks. Automated tests do not certify every firmware fault/recovery trace or every
GUI presentation combination. Validate those against the supported hardware before
release; retain protocol fixtures as the automated baseline.

Backfill, SQLite/data-source management, new HIL transmission, decoded/logged CAN
FD, and wall-time reconstruction for future log replay remain deferred as agreed.
The rebuild is implemented; these explicit external/deferred items are not hidden
behind compatibility adapters.

- Final cursor/GPS/jitter coverage: frozen end with marching cursor is treated as
  a historical view for staleness; GPS rejects invalid fixes and respects cursor;
  jitter counts all matching CAN arrivals (including undecoded frames), excludes
  extended/standard identity collisions, and uses only the selected interval.
- UI DBC selection retains its working parser when a candidate fails to load.
