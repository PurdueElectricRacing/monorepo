# DAQ rebuild: full diff review and style cleanup

Reviewed the complete tracked diff from master `4d93f8e14` to HEAD `a86b77553`, including the working-tree cleanup. The original PR changes 71 files. Existing untracked design documents and diagrams were preserved.

## Review scope

- Workspace manifests, lockfile, CI, and CLI.
- App startup, settings, commands, operational-event delivery, widget construction, and workspace rendering.
- Neutral frame types, identity, drivers, connection lifecycle, decoding, sending, worker scheduling, and shutdown.
- Firmware package validation and updater state machine, compared with their implementations on master.
- HIL resource loading, observation, monotonic deadlines, and snapshot publication.
- Logging and offline CSV export, including their existing formats.
- Timeline tracks, Session ingestion, cache ordering, stable merge, eviction, compaction, and projections.
- Every migrated telemetry widget and shared timeline controls.

## Completed cleanup

The earlier code uses blank lines between methods and logical sections, local intermediate values, and ordinary branches. The cleanup follows that style while retaining grouped local imports and fully qualified dependency paths in daqapp.

- Removed every `.unwrap()` in the DAQ workspace, including legacy formatting and offline log-processing code.
- Reduced `.expect()` from eight sites to three CAN-identity validation sites. Their panic contracts are now explicit; strengthening those input boundaries remains a substantive follow-up below.
- Expanded the compressed timeline UI, including tuples, assignments, match arms, and multi-statement branches.
- Added spacing between declarations, methods, guards, computations, rendering, and shutdown sections.
- Named bounds, plot duration, playhead position, colors, scheduling conditions, selected frames, and projected values instead of nesting conditionals in arguments and expressions.
- Made worker phases readable, restored named pacing/publication constants, and removed redundant scope blocks without changing scheduling or event ordering.
- Replaced panic-prone UDP byte conversion, cache merge iteration, extrema selection, and formatter definition access with explicit operations.
- Startup failures now return from `main`. Settings-save failures are logged. Log-folder command failures reach the app diagnostic.
- Offline log reads and CSV writes return errors through `parse_logs_to_tables`; the UI reports failure instead of panicking or announcing successful completion.
- Added three regression tests for stable merge/eviction, decimation, and invalid CSV output directories.

Successful capture, query, plotting, retention, and protocol behavior is preserved. Error handling is deliberately improved. The offline export functions now return `Result`; their in-repository caller was migrated.

## Remaining findings

These are separate from the completed style cleanup. Five temporary reproduction checks confirmed the behaviors below; the checks were removed after running so they do not become tests that require defects to persist.

### 1. P2: Existing settings are discarded on upgrade

`daq/daqapp/src/settings.rs`: `window_secs` is required during deserialization, but files written on master do not contain it. Deserialization fails and `Settings::load` returns the entire default configuration, losing the selected source, DBC, speed, theme, scale, and log folder. A subsequent save overwrites the old configuration.

Reproduction: serialize a Settings value, remove `window_secs`, then deserialize. It fails with a missing-field error.

Follow-up: give the new field an explicit 30-second serde default and verify that a complete master-format settings document preserves all existing values. Log malformed-settings errors instead of silently treating all failures as an empty configuration.

### 2. P2: Public firmware commands accept misaligned images

`daq/daqcore/src/can_thread/firmware_session.rs` checks for empty/oversized images but omits the four-byte alignment required by `firmware/updater.rs`. `FirmwarePackage` and its image bytes are public, so library callers can bypass the filesystem loader's alignment check and submit an accepted package that later panics while slicing a data word.

Reproduction: construct a three-byte image, consume the initial START frame, provide an ACK containing size three, then tick the updater. The data-word slice panics. The app's manifest loader rejects this image; the gap concerns the public core API.

Follow-up: reuse package/image validation at the command boundary, including alignment and protocol IDs. Give direct public updater construction an enforced validation contract as well.

### 3. P2: Public Timeline inputs can violate its ordering invariant

`daq/daqcore/src/timeline.rs` converts spans without validating finite/nonnegative input. `Timeline::live(time, -1.0)` puts start after end; a subsequent `set_setpoint` panics in `clamp`. Very large negative inputs can also overflow negation. The current app guards its initial span and limits the UI control, but reusable core callers have no such protection.

Follow-up: validate the span in both public entry points, explicitly define invalid-input behavior, and test negative, nonfinite, zero, and very large positive spans. This must not introduce a retention-duration limit.

### 4. P2: Identity validation assumptions are not enforced at every public boundary

`daq/daqcore/src/can.rs` and `frame.rs` retain three identity-validation expects. Raw frame ID fields are public; `can_dbc::MessageId` also permits constructing an out-of-range numeric ID. These objects can reach identity conversion without a guaranteed validation boundary.

Reproduction: `can_dbc_identity(&can_dbc::MessageId::Standard(0x800))` panics. The same issue applies to manually constructed out-of-range neutral frames.

Follow-up: validate DBC message definitions on loading and reject invalid input diagnostically. Choose an enforced frame identity contract, such as storing validated identity directly or making frame construction fallible with private identity fields. Avoid silent fallback IDs or masking malformed input into another identity.

### 5. P2: Direct cache append has no ordering guard

`daq/daqcore/src/cache.rs`: `push` appends without the ordering assertion called for in the rebuild plan. Range queries and eviction use binary search, so an unsorted append silently invalidates their results. Session handles regression correctly; this concerns callers of the public RamCache append API.

Reproduction: push timestamps two then one, then query the inclusive range one through one. The query includes timestamp two.

Follow-up: make the sorted-append precondition explicit and enforce it, or use a checked append API. Keep Session's sorted-merge handling for recorded wall-clock regressions.

## Coverage and handoff observations

The firmware protocol/updater tests present on master were removed during migration. The reviewed workspace contained no test functions before this cleanup. The three new tests cover the changed operations only; they do not establish firmware, driver, worker lifecycle, identity-collision, or widget integration coverage. Restore the relevant existing tests and add the required worker/data tests before claiming those milestones are validated.

`daqcli` remains a Hello-world stub. The connect/watch CLI described in the rebuild plan is still outstanding.

The review respects the user's existing decisions: unlimited start-based retention, accepted clock-regression merge cost, current bus-load watermark behavior, and the known DBC reload limitation. It does not reopen those decisions. Frozen-playhead snapshot semantics remain the separately recorded follow-up in `daqcore_review_followups.md`.

## Validation

- Workspace tests: three permanent regression tests pass.
- Five temporary defect reproductions: all confirmed the findings above, then were removed.
- Workspace compilation is exercised by the tests.
- `cargo fmt --all --check` and `git diff --check`: pass.
- Workspace Clippy with `-A clippy::all -D clippy::unwrap_used`: passes; this specifically checks the no-unwrap rule, not every Clippy lint.
- No hardware, interactive GUI, or sustained throughput profiling was performed in this review.
