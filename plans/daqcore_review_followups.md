# DAQ review evaluation and follow-up plan

Status: planning only; no runtime implementation changes in this review.
Source of truth: current source, prior user decisions in `daqcore_rebuild.md`,
and the latest instruction to leave review points 3 and 4 unchanged.

## Evaluation

| Point | Finding | Disposition |
| --- | --- | --- |
| 1: frozen playhead loses earlier values | Confirmed. Queries use `[start, playhead]`; marching start eventually reaches the frozen cursor. Samples outside that interval are evicted. | Decide paused-view semantics before changing behavior. A preceding-sample query alone cannot restore evicted samples. |
| 2: selection controls retention; no hard bound | Confirmed, and intentional under the agreed start-only eviction and unlimited-retention contract. A reproduction retaining 600,001 frames demonstrates that contract, not a cap malfunction. | Keep retention behavior. Improve its explanation and expose the retained span; do not introduce budgets or a second retention policy. |
| 3: asynchronous DBC interpretation changes | Valid known limitation. | Explicitly excluded by user. No DbcLoaded event, schema epochs, acknowledgement/reset flow or parser-commit changes. |
| 4: bus load does not advance timeline | Confirmed. CAN frame timestamps drive the capture timeline; load samples are filtered through it. | Explicitly desired by user. No additional watermark/clock advancement. |
| 5: inconsistent standard/extended identity | Confirmed. Scope drops the flag; scheduling, deletion and send-result matching use numeric IDs. Cache/Jitter already distinguish identities. | Fix the full identity path, not just Scope filtering. |

The review's suggestion to bound retention contradicts the explicit removal of
60-minute/500k limits. That instruction remains in force. Frozen end also halts
marching start at its end-minus-span value, so future acquisition can accumulate
without limit even if the start track is still marked Marching. Do not add a
warning dialog, forced unpause, hidden truncation or timestamp clamping.

## 1. Choose and document paused-view semantics

This is the outstanding product decision. An optional question was presented to
the user; do not interpret elapsed time as acceptance of a new query contract.
The existing independent-track/interval semantics remain authoritative until a
choice is made.

### Recommended: stable interval through explicit paired freezing

Keep state and history derived from `[start, playhead]`. Provide an explicit
**Freeze view** / **Pause** action that freezes start at its current value and
freezes the playhead at its current value. End may continue following capture.
Individual track toggles and dragging remain independent. Freezing only playhead
continues to freeze time, not the lower interval boundary.

- Add a Session/Timeline operation for paired freezing; use ordinary fields and
  borrows, with no snapshot clone or widget-owned history.
- Perform both freezes from the same pre-action start/playhead values.
- Go live releases all tracks. Clear resets live state, preserving live span.
- Keep explicit start changes destructive under start-only eviction. A deliberate
  start edit can change a previously paused selection.
- Clarify UI labels/help: **playhead follows end** controls time selection;
  **start follows end** controls the selected/retained lower bound.
- The current timeline UI has Go live and track toggles but no dedicated Pause
  button, so paired freezing would be a new explicit control, not an edit to a
  currently present button.

This keeps paused values stable under newly arriving frames timestamped after
playhead. It does not promise an immutable snapshot when a late/out-of-order
frame is inserted at or before playhead, or when decoding configuration changes.
The review's literal frozen-playhead reproduction would still demonstrate the
intentional single-track behavior; the paired-freeze action is what guarantees a
stable selected interval during normal forward capture.

### Alternative: preserve current independent freezing entirely

Make no Timeline/query change. Explain that a frozen playhead does not freeze
start, and that freezing both tracks already provides a stable selected interval
under forward capture. Improve retained-span/status text as in increment 2.

### Alternative: state as of playhead, independent of displayed start

This explicitly supersedes the original shared-range value contract. It needs a
separate design, not simply a broader TelemetryView slice:

- Keep interval samples `[start, playhead]` and plot samples `[start, end]` separate
  from state reconstruction at playhead.
- Decide how required preceding state survives eviction. Searching the current
  retained cache is insufficient after the needed frame has been discarded.
- Multiplexed cells/thermistors require preceding samples per semantic slot;
  newest-per-CAN-ID cannot reconstruct them. Dynamics needs independent signal
  values; charging data also spans multiple messages.
- Choose a retained predecessor representation or revise the retention contract;
  define what occurs when seeking backward beyond the retained baseline.
- Specify undecoded arrivals, timestamp ties, staleness and missing/partial state.
- Assess whether this added state/indexing is worth the complexity before coding.

Do not implement this alternative without the user's decision. It conflicts with
both interval-only value queries and the goal of deriving values from one simple
frame history, and may require changes beyond the selected review scope.

## 2. Make existing retention semantics visible

This increment does not depend on the paused-view decision.

1. Use existing `RamCache::time_span()` to show the oldest and newest retained CAN
   timestamps alongside count. Keep requested start/end/playhead labels separate.
2. Explain on the start control that advancing it permanently discards older
   samples; moving it back does not reload them. Frozen boundaries allow capture
   to continue and history to grow. Use ordinary help/status text, not an alert.
3. If a requested interval extends before available history, show that the view
   is partial; if no frames are retained, show an empty-history status. Do not
   treat a sparse frame stream as proof of continuous coverage.
4. Keep bus-load sample retention tied to start and keep its timestamps from
   advancing Timeline. Label the range as CAN retained history, not all telemetry.
5. Update architecture documentation and diagram captions to distinguish selected
   range, retained range and captured-through CAN time.

No new budget settings, retention clock, truncation logic, backfill worker or
playhead-expiry policy is needed while the current retention contract remains.
Existing partial/empty views must remain explicit after deliberate eviction.

## 3. Establish a transport-neutral CAN identity contract

Define `CanIdentity` in the shared frame layer, below the worker:

- Standard/extended identity with validated ID width; no slcan type exposure.
- `Copy`, `Eq`, `Hash`, `Ord` for cache and schedule keys.
- Accessors for raw numeric ID and extended flag; explicit conversions to/from
  the existing flagged DBC ID representation.
- A documented deterministic ordering. Raw numeric IDs are for wire output and
  labels; full identity is for matching, lookup, replacement and deletion.

Retain neutral frame wire fields where useful; have CanFrame/ParsedFrame identity
accessors construct/return this type. Place DBC-specific conversions in core CAN
helpers so the frame identity does not need to own a parser or depend on UI code.
Do not persist a new settings schema for transient Scope/send configuration.

Migrate core cache keys, latest-map APIs, signal-series queries and UI latest-map
keys to the shared identity. Preserve payload moves, equal-time ordering,
compaction and the existing latest-versus-cursor distinction. Audit numeric-ID
comparisons; protocol-specific raw-ID comparisons, such as firmware standard IDs,
remain explicit rather than being indiscriminately replaced.

**Exit:** standard and extended `0x123` are distinct in all shared identity APIs;
DBC/wire conversions preserve their flags; boundaries reject invalid IDs.

## 4. Fix Scope end to end

1. Carry CanIdentity through WidgetConstructor::Scope and ScopeState::Configured.
2. Derive it from the full DBC MessageId in the Scope picker and table-to-Scope
   actions; neither path may call the flag-stripping helper for a lookup key.
3. Filter plotted frames with `frame.identity() == configured_identity`.
4. Keep full `[start, end]` plotting, real-time X labels, playhead marker and
   draw-only decimation unchanged.
5. Identify standard/extended traffic in selection labels where needed to
   distinguish otherwise identical numeric IDs. CSV exports use the same filtered
   point sequence as the plot.
6. Migrate Jitter's existing correctly flagged matching to the shared type and
   audit ViewerTable/ViewerList and formatter lookup conversions.

**Exit:** two frames with the same numeric ID and signal name, but different ID
formats, cannot enter each other's Scope series from either construction path.

## 5. Fix send identity end to end

1. Key SendTable by CanIdentity. Add/replace affects only that exact identity.
2. Update due-send commit and delete APIs to use full identity.
3. Carry identity in AddSendMessage, DeleteSendMessage, MessageSent and
   SendFailed. Construct/validate identity before submission; payload/count errors
   can still report the submitted valid identity. Reject invalid numeric IDs at
   identity construction rather than trying to encode them in a valid identity.
4. Update SendingMessage, UI action matching, replacement, acknowledgements,
   finite-count removal, failure removal, individual deletion and delete-all.
5. Preserve scheduling periods/counts and commit-only-after-success behavior.
   Keep encoding conversions explicit; raw IDs and extended flag still reach
   driver writes correctly.

**Exit:** standard and extended versions can be queued simultaneously, independently
sent/replaced/deleted/acknowledged, with no accidental cancellation of the other.

## 6. Validation and handoff

Add targeted behavioral coverage during implementation:

- Identity conversion/validation and standard/extended collisions in cache,
  Scope, Jitter and send scheduling; include deletion and result-event matching.
- If paired pause is chosen: the review's A@1 s/B@2 s fixture with a 10-second
  window remains stable through capture at 12.001 s when both start and cursor
  are frozen; independent playhead-only freezing retains its documented behavior.
- Manual start advancement still evicts and backward seeks produce partial/empty
  views; boundary samples remain inclusive; retained-span labels handle no data.
- Multiplexed cell/thermistor and charging selections retain their interval
  semantics. Do not validate only newest-per-ID values.
- No accidental retention cap: frozen history continues past 500k frames/one hour.
- Bus-load-only events do not advance the capture timeline (user decision).
- Forward-acquisition receive loops retain the recently fixed zero wait after a
  nonempty read; this plan does not revisit wall-clock regression performance.

Run workspace/all-targets checks and focused behavioral tests, then formatting
and namespace audits. No feature matrix, GUI dependency in core, synchronization
inside Session or widget-owned snapshots should be introduced. Update architecture
and handoff documentation after the final paused-view choice; record new results
rather than reusing historical test counts. Hardware validation remains separate.

Implementation order: retention explanation → shared identity → Scope → sends →
chosen paused-view behavior → focused integration validation/documentation.
Identity work and retention explanations can proceed independently of point 1.


## Point 5 implementation result

Implemented only identity increments (3–5): validated neutral CanIdentity keys
across cache/view/Jitter, both Scope construction paths, scheduler and send UI.
Add/delete commands and send success/failure events now carry full identity.
Standard and extended sends with the same number coexist; replacement, finite
count commit, deletion and UI Drop cancellation match the exact identity.
Five targeted behavioral tests pass, including the public worker's loopback
receive/send path and cache separation. Workspace/all-targets checks and formatting
pass. Hardware/GUI smoke was not performed. Other review points remain unchanged.
