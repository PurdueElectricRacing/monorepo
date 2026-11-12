# [ARCHIVED / OUT OF DATE] Purdue Electric Racing — DAQ Platform Architecture Decision Handoff

> [!WARNING]
> **THIS DOCUMENT IS ARCHIVED AND FULLY OUT OF DATE.**  
> It is preserved in `archive/` solely because it captures useful historical ideas, subteam requirements interviews, and prior tradeoffs. **Do not follow the open decision or prototype plan in this file.**  
> See the authoritative, active architecture and implementation plans at:
> - [`../PER_DAQ_Option_A_Master_Plan_v2.md`](../PER_DAQ_Option_A_Master_Plan_v2.md)
> - [`../PER_DAQ_Implementation_Roadmap.md`](../PER_DAQ_Implementation_Roadmap.md)

**Date:** November 12, 2026  
**Status:** Archived / Superseded by `PER_DAQ_Option_A_Master_Plan_v2.md`  
**Intended audience:** PER Electronics/Software, vehicle dynamics, mechanical, aero, controls/autonomy, and future DaqApp/DaqCloud developers  
**Repository:** https://github.com/PurdueElectricRacing/monorepo  
**Decision currently under consideration:**

1. **Fully custom PER system** — retain the original DaqApp/DaqCloud vision and implement the application, visualization, timeline, caching/data access, analysis, storage, and collaboration layers ourselves (using appropriate libraries and standard formats).
2. **Rerun-centered PER system** — make Rerun's **native desktop Viewer and RRD recording format** the primary visualization/data path; keep PER's acquisition, decoding, orchestration, archive management, annotations, and domain-specific analysis custom. DaqApp becomes an application **around/embedding Rerun**, rather than a new visualization engine.

> **This document is a handoff, not an advocacy piece or approved implementation specification.** Its purpose is to preserve the actual PER requirements, the reasoning behind the narrowed options, and the unknowns that must be tested. There are deliberate **open decisions**, including how best to use Rerun's Catalog, how to present one shared track-day timeline, and how to handle live-to-historical reconciliation. Specific pseudocode and schemas are illustrative, not drop-in APIs. Claims about Rerun are cross-checked against the official documentation linked at the end; performance and integration behavior on PER data are **unverified**.

---

## Quick summary for the next discussion

PER is developing **one desktop-first telemetry and engineering analysis environment**, inspired by **Nominal** (shared engineering data and analysis) and **AdvantageScope** (intuitive timeline, drag-and-drop signals, numerical/discrete plots, multimodal visualization). The app should be the primary place for **live debugging, trackside visualization, SD-card import, long-term historical analysis, and reusable shared engineering workspaces**.

**Non-negotiable operating workflow:**

1. The car logs **all telemetry to an on-car SD card** (canonical raw data) while transmitting **best-effort UDP** telemetry for live visualization.
2. Between runs, the SD card is removed and inserted into a laptop. DaqApp/DaqCLI parses/imports it **offline**, then the card returns to the car.
3. All data eventually reaches **permanent shared storage**. Multiple imports/DAQ restarts need a coherent **time axis**, searchable events, and proper source identity.
4. Another engineer can take a **USB drive with the imported data** and analyze it without internet.
5. Other subteams can add **time-based annotations**—point events and optional-duration intervals—which should synchronize quickly when connected.
6. Engineers can build **plots, formulas/derived signals, metrics, comparisons, and saved workspaces**, then export selected signals/time ranges to **MATLAB/Python-friendly formats**.
7. Future support should cover **LiDAR, 3D/poses, GPS, cameras/video**, synchronized with the ordinary CAN data.

The existing monorepo includes ideas/components called `daqcore`, `daqcli`, `daqapp`, and `daqcloud`, but **their current designs are concepts, not architectural commitments**. SQLite performance is not a reason to reject it; one prior design used server-side SQLite plus a **locally cached SQLite subset** that the UI read. The concern was the *extra replication/import/cache lifecycle*, not that SQLite is too slow.

**The central unresolved question:** Is owning the complete analysis UX worth the work and long-term maintenance, or should PER **adopt Rerun's viewer/recording abstractions** and concentrate our code on vehicle-specific semantics, collaboration, formulas, ingestion, and reliable archival infrastructure?

**Recommended next action:** Build two deliberately comparable, bounded prototypes on **the same real PER recording**, with the **same user tasks**, before choosing either path. See [Prototype plan and decision gates](#12-prototype-plan-and-decision-gates).

---

## 1. Goals, users, and engineering motivations

### 1.1 Product vision

This is **not merely a CAN graphing application** and is **not just a cloud upload service**. PER wants an integrated engineering feedback loop:

> Observe vehicle behavior → find the relevant time/lap → compare related signals → compute derived quantities → correlate with driver/team context → annotate/share an observation → make an engineering decision → repeat on later runs.

A representative vehicle dynamics workflow:

1. Locate a corner or suspected handling issue using time, lap state, or GPS.
2. Overlay steering angle, speed, lateral acceleration, shock-pot travel, brake/throttle, and other relevant channels.
3. Derive quantities such as damper velocity, roll estimates using known geometry, or pack power/energy.
4. Compare another lap, setup, or event using lap-relative time or distance rather than only absolute UTC.
5. Save a reusable workspace and note the finding so another member can reproduce it.
6. Export exact samples/time ranges for deeper MATLAB/Python analysis when needed.

### 1.2 Direct team feedback to preserve

**New team member's usability assessment:** Current DaqApp is insufficient for meaningful routine analysis: weak graphing, unintuitive time interaction, lack of visible event/context information, inconsistent widget layout, little saved analysis or collaboration support. Some criticisms refer to unfinished placeholders and not fundamental impossibilities in `egui`; treat them as *user-experience requirements*, not necessarily evidence the code architecture is wrong.

**Vehicle Dynamics:** Uses shock potentiometers, IMU/GPS, steering input, throttle/brake and other data for heave/roll/pitch, damper response, suspension validation and stiffness-related calculations. MATLAB scripts currently do much of the work; repeated script writing is slow and much recorded data goes unused. Requested easy plots, user-defined equations, and reusable derived quantities.

**Mechanical:** Wants suspension/roll plots and GPS overlays; much of the existing workflow is MATLAB time-series analysis.

**Product inspirations:**

- **AdvantageScope:** excellent signal tree, plot drag/drop, time cursor, discrete states, multiaxis plotting, derived operations and low-friction desktop use.
- **Nominal:** shared timelines, media, annotations, searchable recordings, saved dashboards, collaboration, and reusable engineering insight.
- **Rerun:** polished synchronized timeline, plots, 2D/3D/map/media viewers and native Rust/egui composition; may prevent rebuilding a general-purpose visualization platform.

Do **not** prematurely prioritize agent/AI features or sophisticated video analysis over core sensor plots, correctly timed events, formulas, and reliable workflows. Video and LiDAR are credible *future data types* to account for architecturally, even if not yet collected.

### 1.3 Desktop-first is a conscious preference

The **primary interface should be a native desktop app** that can access local recordings and hardware without a browser or a reliable internet connection. Installation can be a portable ZIP/unzip workflow; this is not considered a major UX cost. A share link/code can restore a workspace in the desktop app. The app can load remote data transparently in the background rather than requiring users to manually download whole recordings.

A separate web analysis frontend is **not currently required**. Adding one would risk a second UI codebase. A tiny web annotation/admin page might be useful later, but it is not part of the two-way architectural decision.

---

## 2. Requirements and priorities

### 2.1 Hard requirements

| ID | Requirement | Acceptance meaning |
|---|---|---|
| R1 | Canonical raw logging | SD data is authoritative; incomplete/lossy UDP never silently becomes the canonical archive. |
| R2 | Live debugging | DaqApp receives/decodes UDP, shows current vehicle state, CAN diagnostics, faults and useful plots. |
| R3 | Local post-run import | SD → parse → view/query without cloud service or internet. |
| R4 | Portable offline analysis | Copy the imported recording to a USB drive, open on another laptop, and inspect it without rebuilding an entire database installation. |
| R5 | Permanent shared archive | Raw SD logs and a useful decoded representation eventually reach DaqCloud with checksums, source/config provenance and recovery from interrupted uploads. |
| R6 | Shared timestamp model | Correlate acquisition/boot timestamps to UTC when possible, preserve raw timestamps and gaps, and display multiple segments on one day-level temporal coordinate. |
| R7 | Annotations | `name`, start, optional end, category/author/source; editable by humans and creatable during parsing; visible rapidly to connected teammates. |
| R8 | Historical navigation | Navigate calendar → track day → run/segment → time window; open signals without loading a whole season into RAM. |
| R9 | Engineering plots | Multiple signals, correct units, relevant axes, zoom/pan/seek, discrete state/event plots, saved layouts. |
| R10 | Derived analysis | Reusable formulas/integration/differentiation/statistics with explicit timestamp and unit semantics; available for live and recorded data where meaningful. |
| R11 | Export | Export specific signals and time ranges with fidelity and provenance to Parquet/CSV/MATLAB-compatible form. |
| R12 | Robustness | Detect duplicate imports, clock resets, schema changes, gaps, failed uploads; no silent data corruption. |
| R13 | One main analysis interface | Prefer same plots/formulas/workspace model for live and historical recordings, even if acquisition/query implementation differs. |
| R14 | Cross-platform desktop | Windows, Linux, macOS targeted; test actual build/dependency support. |

### 2.2 Strong but potentially staged requirements

- Automatic **live-follow**, pause, back-scrubbing and return-to-live without stopping acquisition.
- Track-day-wide timeline and named run/lap/event intervals.
- Saved workspaces/loadouts, formula definitions, links or share codes, persistent settings.
- Remote **on-demand range loading** with responsive interaction and preferably an ordinary local file cache.
- Multi-run comparison using UTC, run-relative, lap-relative, event-relative or distance-along-track coordinates.
- GPS track/map, future synchronized video and LiDAR/3D pose/frame transforms.
- Clear visibility of **data provenance**: live/provisional vs SD/canonical, schema version, software revision and clock-sync quality.

### 2.3 Non-goals for version 1

- Zero-code, fully general MATLAB replacement.
- Google Docs-style simultaneous editing of entire dashboard layouts; **asynchronous sharing plus live annotation sync** is a better first goal.
- Arbitrary user Python execution in the central service.
- Guaranteed recovery of missing UDP packets **while the car is moving** without a separate on-car readback path.
- One physically continuous data file or one single giant database table for a whole track day. A shared logical timeline does not require this.
- Building an entire web frontend in addition to the native frontend.

---

## 3. Operational workflow and time model

### 3.1 Physical workflow

```text
                 PER car
           +---------+---------+
           |                   |
     On-car SD raw        UDP telemetry
     (authoritative)      (best effort)
           |                   |
      Remove/import       DaqApp live
           |                   |
           v                   v
        DaqCLI/DaqCore  <--- session identity/time model
           |
     Local decoded data --------> DaqApp historical viewer
           |
      USB / upload
           |
           v
       DaqCloud archive -----> Other desktop DaqApps
           |
       Annotations/workspaces/formulas
```

The SD card goes back into the car between imports. Several segments and SD transfers can form one track day. A run may contain multiple DAQ clock epochs due to reboots/resets. No requirement that all physical segments share one file.

### 3.2 Suggested logical entities (architecture-independent)

- **TrackDay** — event/date/location, globally browsable timeline.
- **RecordingSegment** — a contiguous DAQ recording/clock epoch, with stable ID and original source provenance.
- **RawAsset** — original SD binary or other sensor file, with checksum and format/schema revision.
- **DecodedAsset** — SQLite data, RRD file(s), or another decoded representation, depending on architecture.
- **SignalDefinition** — stable logical identity, units, encoding, description, sampling/temporal semantics, vehicle configuration applicability.
- **ClockMapping** — relationship from raw device/DAQ clock to synchronized UTC (and accuracy/confidence/gaps).
- **Annotation** — mutable human/automated point or interval on track-day time axis, optionally linked to a segment/lap.
- **Workspace** — saved selection of signals, plots/views, formulas, time window, comparison alignment and layout.
- **DerivedDefinition/Result** — reusable formula + its inputs, version, calibrations, parameters and optional cached materialization.

**Important:** UTC is the common physical timeline *when known*; always preserve device-native timestamps. Do not invent UTC for uncorrelated segments. Distinguish “not synchronized” from “corrected by a low-confidence estimate.”

### 3.3 Temporal semantics per signal

Not all samples should be interpolated identically:

- **Continuous sampled:** RPM, temperature, shock displacement. Interpolation/hold policies must be explicit.
- **Discrete state:** ready-to-drive, gear, vehicle mode. Latest-at-before-timestamp may be correct within an epoch, with staleness/reset rules.
- **Point event:** fault triggered, lap crossing, driver comment.
- **Interval event:** skidpad test, lap, braking test.
- **Frame-based data:** LiDAR scan, image or video frame, pose/transform; use timestamps and acquisition-frame semantics.

This affects both backfill and formula correctness.

---

## 4. The two architecture candidates at a glance

| Dimension | **A — Fully custom PER** | **B — Rerun-centered PER** |
|---|---|---|
| Primary desktop app | Rust DaqApp, custom `egui` UI + selected plotting/3D/video libraries | Rust DaqApp shell embedding `re_viewer` with PER-specific panels/extensions |
| Data model fed to viewer | PER-defined signal/frame types and timeline | Rerun entities, components, archetypes and timelines |
| Primary visualizations | PER builds synchronized plots, timeline, map, 3D, media, state lanes | Rerun TimeSeries/StateTimeline/Map/Spatial2D/Spatial3D and media views |
| Historical representation | Decide independently: SQLite, Parquet/DuckDB, MCAP, or hybrid | **RRD** is the primary decoded recording format; original SD remains canonical raw |
| Server query/storage | Custom DaqCloud query APIs and storage | DaqCloud permanent storage/metadata plus Rerun Catalog for RRD indexing/querying |
| Local recordings | DaqApp custom file/database readers | Native RRD open/read; custom PER session/catalog wrapper |
| Live stream | Custom RAM store + plot updates | DaqCore decoder → Rerun SDK/Chunk Store; PER debug widgets remain custom |
| Cache and backfill | PER controls cache, query scheduling, prefetch, lookback, all UI data | Rerun handles much native viewer storage/navigation, but integration with remote retrieval/retention and cross-source merging still requires proof |
| Annotations | Custom storage, UI, and plot overlays | Custom mutable storage/editor; displayed via Rerun logging primitives or custom Viewer view |
| Derived formulas | PER implements | PER still implements; export results back to Rerun as generated entities/layers |
| LiDAR/video | PER integrates 3D renderer, codecs, frame seeking, calibration, linked time | Rerun built-ins cover many 2D/3D/video use cases, but ingestion/time calibration remains PER's job |
| Vendor/upstream coupling | Low at application design level | High at Viewer internals, RRD and Catalog APIs |
| Largest engineering risk | Scope and quality of a complete visualization/product UX | Rerun API/version coupling, OSS Catalog operation, day-wide timeline, arbitrary data access and feature gaps |

**Both options retain PER's custom CAN/UDP acquisition and binary SD parser.** Neither removes the need for a real archive, stable identifiers, time correlation, domain formulas or collaboration.

---

## 5. Candidate A — Fully custom PER application, analysis engine and backend

### 5.1 Architecture sketch

```text
                       DAQ / sensors
                         |       |
                    Live UDP   SD logs
                         |       |
                         v       v
                     DaqCore / DaqCLI
                         |       |
                         |   Decode into PER types
                         |       |
                    RAM ring   Local archive
                         |       |  (SQLite or Parquet/MCAP etc.)
                         +---+---+
                             |
                      Unified data access
                    / local query / cache
                             |
                      PER timeline/session
                             |
            +----------------+----------------+
            |                |                |
       Plots/formulas     GPS/3D/video      Debug/annotations
            +----------------+----------------+
                             |
                        DaqApp UI
                             |
                 DaqCloud central APIs
                (archive, queries, metadata)
```

### 5.2 What must be designed/built

1. **Recording and signal schema:** stable IDs, timestamps, units, versions, transformations, raw/decoded provenance.
2. **Storage format(s):** the team can still use standard formats rather than invent byte layouts: SQLite for samples, Parquet + DuckDB, MCAP for multimodal streams, and/or MP4 for cameras. “Custom” here means *PER owns the architecture and integration*, not that PER must implement database engines or video codecs.
3. **Local and remote historical readers:** chunk/time/signal queries, last-known state at a seek point, metadata/file lookup, retries, error reporting.
4. **Cache and query orchestration:** query deduplication, RAM/disk caching, prefetch, cancellation, live ring buffer, resolution-aware plotting.
5. **Visualization and interaction:** plotting (multiple signals, axes/units, numerical/discrete/event), zoom/pan/playhead, linked cursor, drag/drop signal selection, spatial views, video seeking, layout persistence.
6. **Formula engine:** typed expression definitions, unit checking, alignment/sampling semantics, integrals/derivatives, derived channels, metric summaries, comparisons.
7. **Collaboration:** annotation APIs and editing, workspaces, share codes, permissions, eventual offline sync.
8. **Archiving:** checksum-verified upload, duplicate detection, incremental parsing, backups, recovery, historical data versioning.

### 5.3 Historical storage suboptions *inside* the custom path

These are **implementation choices**, not a third top-level contender:

- **Server SQLite + local SQLite chunk replica:** the original idea. UI always reads a local database; remote queries fill it; local SD imports insert directly. This gives a uniform local SQL interface but adds record-level replication/cache management and overlap/invalidations.
- **Parquet + embedded DuckDB:** query immutable local files or remote HTTP/S3 Parquet ranges with the same SQL engine; add a manifest/catalog and a small RAM/file cache. Very good numeric query/export interoperability; multimodal assets likely need other formats/readers.
- **MCAP as decoded recording container:** useful for CAN, GPS, LiDAR, video messages and other heterogeneous timestamped streams. Native visualizations still need custom renderers; analytical signal extraction/export remains needed.
- **Hybrid:** e.g., numeric Parquet, LiDAR MCAP, video MP4, SQLite metadata. Flexible but increases format/reader orchestration.

**SQLite performance is not the deciding objection.** The real question is whether the *replication and cache state machine* improves simplicity compared with immutable file-based access.

### 5.4 How live and recorded data could share one UI

Create one **semantic** `RecordingContext` and an abstract data provider; a workspace refers to logical signal IDs, time windows, view settings and formulas, not physical database paths.

```rust
// Illustrative API contract, not existing code:
trait DataSource {
    fn list_signals(&self) -> Vec<SignalInfo>;
    async fn query_numeric(&self, channels: &[SignalId], range: TimeRange,
                           resolution: Resolution) -> Result<NumericBatch>;
    async fn query_states(&self, channels: &[SignalId], at: Timestamp)
                           -> Result<Vec<StateSample>>;
    async fn query_frames(&self, stream: StreamId, range: TimeRange)
                           -> Result<FrameBatch>;
}
```

The live data source reads a RAM buffer/local spool; local historical reads indexed assets; remote historical reads DaqCloud. The custom timeline and plot system gets one uniform interface.

**Cost:** PER is responsible for most of the viewer's sophisticated behavior, not merely the SQL queries. The abstraction is clean, but implementing it to a polished engineering-tool standard is substantial work.

### 5.5 Why choose A?

- Want maximal control over linked time-series UX, formula editor, run comparison, live/backfill logic, annotations and custom CAN diagnostics.
- Want long-term independence from a viewer's internal data model and release cadence.
- Prefer arbitrary file/storage formats and specialized analytics.
- Want to own optimization policies and resolution-aware on-demand requests precisely.

### 5.6 Why not choose A?

- Large workload to reproduce excellent **plots, state timelines, media/3D synchronization, maps, layout editor, navigation, export**, and intuitive UX.
- The current team feedback shows that basic UI interactions already need significant improvement; a good internal channel architecture alone does not solve discoverability/usability.
- More test burden and ongoing maintenance of custom graphics and data-access infrastructure.
- Risk of spending team resources building generic visualization technology instead of vehicle-specific features.

---

## 6. Candidate B — Rerun-centered DaqApp + RRD/Catalog backend

### 6.1 What Rerun is and is not

Rerun comprises several distinct pieces:

1. **Logging SDK:** Rust/C++/Python APIs that turn decoded values into Rerun entities/components/archetypes with timestamps; can stream to Viewer and/or save `.rrd` files.
2. **RRD:** Rerun's serialized recording format. It is portable and supports indexed/optimized reads; it is not the same as a generic SQL database.
3. **Native Viewer:** desktop visualization app built with Rust/egui/eframe, including in-memory **Chunk Store**, time navigation, 2D/3D/media/map/time-series views, and **blueprints** (layouts and view configuration).
4. **Catalog:** datasets → segments (logical recording IDs) → layers (physical RRD assets); a query/index service exposing Rerun data through a DataFusion/Arrow-oriented API.
5. **OSS Catalog server:** local/self-hosted, limited production features (details below).
6. **Rerun Hub:** separate managed/persistent commercial option, not assumed or required for PER.

**Critical distinction:** Rerun's SDK/Viewer works without the Catalog; local `.rrd` files can be opened directly. The Catalog helps organize/query remote archives. A Catalog dataset is **not automatically a single day-long logical Viewer recording**, and its existence does not replace PER's upload service or annotation workflows.

### 6.2 Architecture sketch

```text
                         PER car
                 +----------+----------+
                 |                     |
            UDP telemetry          SD raw logs
                 |                     |
          DaqCore CAN thread          DaqCLI
           decode/normalize           parse
                 |                     |
       PER → Rerun adapter       Rerun columns/chunks
                 |                     |
       Live Rerun recording      Optimized RRD files
                 |                     |
                 v                +----+-----------+
         DaqApp native Viewer     |                |
         (embedded re_viewer)   Local/USB       DaqCloud upload
              |                    |                |
       PER custom controls    Open RRD       Permanent RRD/raw files
       CAN debug, formulas                      + durable SQLite registry
       annotations/export                            |
                                                    v
                                             OSS Rerun Catalog
                                                    |
                                            Viewer browse/query
```

### 6.3 Concrete workflow B: live acquisition

1. DaqCore continues to receive UDP, decode CAN/SuperDBC, identify faults and update specialized PER debug state.
2. A **PER→Rerun adapter** maps signals to stable paths such as `/car/powertrain/motor_rpm` and supplies timestamps, units and scalar/discrete types.
3. The embedded Viewer receives a **live recording stream**. Same Viewer plots/map/state timelines/blueprint vocabulary can be used for saved RRD recordings.
4. Optionally spool a provisional local live recording; do not confuse that with the SD authoritative record.
5. Pausing/scrubbing the Viewer must not pause the CAN acquisition task. New samples continue arriving while user navigates old data.

The adapter needs batching and backpressure; avoid one expensive UI operation per CAN frame. Keep custom low-latency debug/control panels separate from Viewer rendering where appropriate.

### 6.4 Concrete workflow B: SD import and portable RRD

1. Parse a raw SD asset with DaqCore/DaqCLI; resolve message definitions/software revision, clock epochs and UTC mappings.
2. Create an RRD recording with stable recording ID and time axes (at least raw clock plus correlated acquisition UTC where available).
3. **Use bulk column logging** for high-volume signals; optimize/verify the RRD and create footer index where required (`rerun rrd optimize`).
4. Save imported RRD locally; open directly in desktop Viewer. USB copy transfers RRD plus PER manifest/metadata (and raw source if desired), with no Catalog necessary.
5. Upload the raw and decoded assets to DaqCloud whenever connectivity exists.

Do not promise RRD will automatically unify independent SD segments. Define an explicit mapping of **TrackDay / RecordingSegment / Rerun recording ID / Catalog segment/layer** and test day-wide viewing.

### 6.5 Concrete workflow B: DaqCloud + Catalog

**DaqCloud still owns:**

- Stable track-day and run identifiers, driver/track/test metadata and original binary logs.
- Authoritative RRD file locations/checksums, upload verification and reconciliation.
- User permissions and collaboration APIs, annotations, workspace/formula definitions.
- A persistent SQLite registry so an OSS Catalog can be recreated after restart.

**Rerun Catalog owns (as a rebuildable service):**

- Indexed catalog datasets and RRD segments/layers.
- Programmatic queries by entity, timeline, time window and segment, returning Arrow/DataFusion-like tables.
- Integration with native Rerun browsing and visualization.

**Example data organization (proposed):**

```text
DaqCloud (durable registry / storage)
└── TrackDay: 2027-05-04
    ├── Raw segments: run_001.bin, run_002.bin, ...
    ├── RRD files: run_001.rrd, run_002.rrd, ...
    ├── Time correlation + SuperDBC versions
    ├── Annotations, formulas, workspaces
    └── Rerun Catalog mapping:
         Dataset: PER.2027-05-04
         ├── Segment run_001
         │    ├── base → run_001.rrd
         │    └── derived → run_001_derived.rrd
         └── Segment run_002
              └── base → run_002.rrd
```

**Verified OSS Catalog limitations (official docs, checked 2026-11-12):**

- Catalog metadata/registrations exist **in memory** and must be reconstructed on restart; physical RRD files persist.
- Directory datasets are scanned **once** at startup, **not watched**.
- The OSS server supports registration of **server-local `file://` paths**, not arbitrary cloud object-store paths.
- **No built-in authentication**. Do not expose it publicly without an authenticated PER gateway/network boundary.
- Indexed RRD files can be read on demand; unindexed files may be loaded in full.
- File upload, raw data retention, automatic registration and durable project metadata remain PER's responsibilities.

These limitations make **DaqCloud over RRD + Catalog** materially more work than just calling `rerun server`, but far less than implementing an original time-series query engine.

### 6.6 How does “transparent live/local/cloud” work with Rerun?

The PER **app shell** can present one navigation/workspace model even though the underlying Rerun connection mechanisms differ:

| Context | Physical source | Route into Viewer | PER orchestration needed |
|---|---|---|---|
| Live | DaqCore UDP stream | Rerun logging stream / in-process receiver | Recording identity, timestamp mapping, live-follow, source label |
| Local | RRD file(s) on SSD/USB | Viewer opens local RRD | Manifest discovery, clock/session metadata, workspace restore |
| Remote | DaqCloud RRD archive registered in Catalog | Viewer/Catalog dataset/segment loading | Authentication boundary, dataset mapping, reliability, desired range/segment selection |
| Historical with missing live data repaired | Canonical SD RRD arrives after UDP provisional data | Reopen or supersede provisional session/layer | Dataset precedence, deduplication, refresh UI/cache |

**Caution:** It is not verified that a stable public API exists to implement *arbitrary* transparent local-file/remote-Catalog/live-stream switching and prefetch as one Viewer source without custom work. Rerun supports the individual modes, but the application must coordinate them. Avoid promises that remote range requests and offline caching are magically solved merely by using RRD.

### 6.7 Viewer integration and UI scope

Official Rerun `extend_viewer_ui` documentation describes embedding `re_viewer` in an `eframe` app and placing custom Rust/egui panels alongside it; custom View classes and visualizers are possible. Good PER panel candidates: CAN message inspector, remote E-stop status, logger status, sensor diagnostics, SD import, annotation editor and archive navigation.

**But the Viewer extension interfaces are explicitly unstable**: Rerun warns extensions may break with every release. Pin the Rerun/egui versions and confirm Windows/macOS/Linux builds. The simplest deployment is one large embedded Rerun viewport surrounded by PER panels; trying to break Rerun into freely dockable subwidgets inside our old dock system may be considerably harder.

### 6.8 What Rerun does *not* remove

- Custom CAN/UDP reception, fault logic, decoding, hardware debugging and SD import.
- Timestamp correlation, duplicate detection, canonical provenance and vehicle calibration.
- User-friendly formula editor, unit checking and reusable derivations; computing and versioning formulas is a PER subsystem.
- Full collaboration: editable annotations, shared workspaces, permissions, offline annotation reconciliation.
- User-facing track-day browser, cross-run comparison/alignment and source-state clarity.
- MATLAB/Python export of exactly the right signals/time windows with explicit sampling/alignment semantics (Catalog exposes queryable data, but the PER export service remains work).
- Recovery of the OSS Catalog, managed uploads, security, backup and deploy automation.

### 6.9 Why choose B?

- Reuses a polished native UI for plotting, timeline, state lanes, maps, 2D/3D, LiDAR and video rather than building generic visualization mechanics.
- Recording SDK and RRD give PER one multimodal serialized representation with direct desktop playback.
- Catalog provides indexed and queryable RRD datasets, limiting the need for a custom historical query engine.
- Rust/egui gives plausible native integration with existing PER acquisition code.
- Saved blueprints provide a useful foundation for reusable layouts (though PER must store the full workspace context).

### 6.10 Why not choose B?

- PER adopts Rerun's entity/recording model, RRD evolution and release cadence.
- Viewer internals/extension interfaces are not stable APIs.
- OSS Catalog's durability, security and registration limitations require an operational wrapper.
- Performance and lifecycle of **one global day timeline across multiple segments**, remote seeking and large continuous datasets are not guaranteed from docs alone.
- The Viewer has a memory budget and may drop old data when full; indexed Catalog reads and “unlimited scrollback in one viewer session” are not synonymous.
- Some AdvantageScope-like features (dual Y axes, exact discrete/plot UX, user formula editing, multi-run overlays) may need custom views or different workflows.
- RRD is less directly interoperable with generic MATLAB/SQL tooling than Parquet; exports require querying/conversion.
- Rerun's abstraction may not fit every specialist CAN/debug UI, requiring careful boundary design.

---

## 7. Complicated issues **shared** by both candidates

These are not arguments unique to custom or Rerun. Both paths need a considered design.

### 7.1 Live, pause, seek and backfill are three different problems

**A. Scrubbing through recently received live data:** maintain a bounded live buffer (and optionally local provisional spool); when a user pauses or seeks backward, acquisition continues. “Return to Live” resumes following the newest sample.

**B. Seeking to an arbitrary time and restoring state:** most numeric channels are sparse and not sampled at the precise cursor. Query the last valid state transition/value at or before the cursor where semantically appropriate; impose staleness and reset boundaries. This is a *lookback* query, not missing-data recovery.

**C. Replacing provisional UDP history after SD import:** incoming UDP may be incomplete. Once authoritative SD data arrives, identify matching sessions/clock epochs and **supersede**, not blindly append, provisional data. An incremental merge that never duplicates or contradicts samples is harder than simply reopening the canonical run.

**Suggested v1 product simplification:** allow explicit **Live Session** and **Imported Recording** contexts, with the same analysis tool definitions and saved dashboards. After SD import, replace/reopen the provisional session rather than attempting an invisible live-archive merge. Complete transparency can be added later if it has real value.

### 7.2 Local and remote cache behavior

**Custom:** implement RAM viewport cache, cancellation/prefetch, perhaps immutable chunk disk cache; SQL/file query design stays under PER control.

**Rerun:** use native Viewer/Chunk Store and Catalog/Indexed RRD retrieval where possible. You still need to validate what remains resident, what happens after scrubbing beyond memory retention, and whether the desktop can transparently re-request history. Do not assume the Viewer has an offline-persistent chunk cache. If persistent offline historical access is needed, explicitly copy whole files or supported chunks and keep a local manifest.

Both should: never block the GUI on network I/O; show loading/error/source status; support interrupted downloads; and distinguish cached incomplete data from complete canonical data.

### 7.3 Global day timeline vs individual segments

**Requirement:** engineer can browse one shared day time axis with multiple driving sessions, gaps, annotations and potentially overlapping sources. This is a *logical query/navigation* feature, not a requirement for a single physical recording.

**Custom:** PER owns stitching and time-range queries across segments.

**Rerun:** Catalog datasets group segments; the Viewer groups data into logical recordings by application/recording ID. A dataset with ten segments does **not** automatically become one seamless Viewer recording. Strategies to test:

1. One segment per SD recording; app-level day browser changes Viewer recording.
2. Generate a derived whole-day RRD/overview for one timeline (may duplicate data).
3. Route/merge compatible segments under a shared logical recording identity for viewing while keeping original archives separate; verify Catalog semantics, overlap behavior and memory.
4. Custom per-day timeline panel driving selected segment/Viewer state.

One of these must satisfy product UX without corrupting provenance. **This is a top decision gate for B.**

### 7.4 Mutable annotations

Annotations are independent of the archival telemetry format. Proposed data:

```json
{
  "id": "uuid",
  "track_day_id": "uuid",
  "recording_segment_id": null,
  "name": "Skidpad practice",
  "start_utc_ns": 0,
  "end_utc_ns": null,
  "category": "test_phase",
  "author_id": "user-id",
  "source": "manual",
  "revision": 1
}
```

Time values above are placeholders. Segment association is optional because notes can be created before SD import. Distinguish automated annotations from manually edited notes; don't overwrite human changes during reparse.

**Common backend:** SQLite annotation/metadata authority in DaqCloud, HTTP API, WebSocket update stream, local pending edits for offline use and conflict/revision handling. A pit-lane LAN server can sync annotations without external internet; independent laptops exchanging USB packages need eventual merge.

**Custom display:** draw point markers/intervals in PER timeline and plots; implement editing directly.

**Rerun display:** Rerun `TextLog`/`StateTimelineView` may work for some events and operating modes. Arbitrarily overlapping, mutable, collaboratively edited intervals likely need a PER annotation panel or custom view integrated with Viewer time selection. Rerun's `AnnotationContext` is *semantic class metadata*, not the shared human-comment layer. Projecting edits into RRD as new immutable log events is not by itself a deletion/update system.

### 7.5 Formula/derived-signal engine

Needs to exist in either architecture and should be independent of GUI and physical storage:

- Named formula: `pack_voltage * pack_current` with units and sign convention.
- Differential/integral/filter operations with explicit edge/gap behavior and numerical method.
- Sample alignment (different sampling rates, no silent endless forward-fill).
- User-defined parameters/calibration constants and firmware/vehicle config applicability.
- Reapply definitions to future recordings; persist definition version and inputs.
- On-demand compute vs materialized derived output, with invalidation when inputs/formula versions change.
- Live incremental evaluation for suitable operations, batch computation for lap comparisons or expensive metrics.

**Custom:** plot/query computed signals directly through PER data source.

**Rerun:** compute independently, then log the output as a derived entity or a named RRD **layer** under the corresponding recording/segment. Formula definitions and access controls remain in PER metadata. Be careful with layer identities and immutable replacement semantics.

### 7.6 MATLAB/Python exports

Export **selected signals + chosen time interval** to Parquet/CSV, with units/schema/source metadata, and optionally aligned/resampled timetables.

- **Custom SQLite:** execute proper range queries and write Arrow/Parquet/CSV.
- **Custom Parquet/DuckDB:** read and export directly, usually easiest for numeric analysis.
- **Rerun/RRD:** use Catalog DataFusion/Arrow API or RRD reader; flatten list-valued scalar components and explicitly define alignment. The official Catalog API supports segment/entity filtering and Arrow/Pandas conversion, but a good engineer-facing export UI is still custom.

### 7.7 LiDAR, GPS, video and clock transforms

Future multimodal support is a meaningful differentiator:

| Concern | Custom path | Rerun-centered path |
|---|---|---|
| LiDAR points and attributes | Define/index schema, GPU renderer, color maps and view controls | Log `Points3D` etc.; use built-in spatial view; still need sensor-frame and transform conventions |
| Vehicle/pose/frame tree | Build transform model and 3D overlay | Use Rerun transform/spatial archetypes; still compute/explain calibration |
| GPS/track map | Implement map/trajectory view | Rerun MapView; verify desired track/lap visualization |
| Camera images | Decode/render frames, synchronize | Rerun image archetypes and spatial view |
| Encoded video | Integrate media decoder/keyframe seeking and PTS↔UTC mapping | Rerun video asset/stream paths; verify codec, timestamp correspondence and seeking on targets |
| Multimodal alignment | PER time service regardless | PER time service regardless |

Do not assume that storing JPEGs/MP4s or logged 3D points automatically synchronizes them with CAN. The acquisition clocks, transformations, dropped frames and calibration must be represented explicitly.

### 7.8 Saved workspaces and links

One workspace should restore:

- Selected recording(s)/track day and source/version.
- Signal IDs, plot configuration, axes/units and linked time window/cursor.
- Formula IDs/versions, comparison alignment, annotations, filters, map/media views.
- Layout definition (PER custom or Rerun blueprint) and PER-specific panel state.

A desktop-only app can load a workspace by stable ID/share code/link with no separate web frontend. Creating a serialized workspace is moderate effort; **restoring it reproducibly across changing recording schemas and formula versions** needs care.

---

## 8. Concrete engineering boundaries and candidate code organization

**Existing conceptual areas:**

- `daqcore` — raw CAN transport/decoder, typed messages, time correlation, live/debug state.
- `daqcli` — parsing and import/export tooling.
- `daqapp` — primary native app and trackside operations.
- `daqcloud` — archive, API, metadata/annotations, user/workspace management.

**Proposed neutral PER modules, regardless of option:**

```text
common / daqcore
  vehicle decoder, signal metadata, clock correlation,
  recording identity, units, error and provenance types

daqcli
  import SD, validate, decode, build archive,
  verify, export

analysis_core
  expressions, units, resampling, metrics,
  lap/event/distance alignment, provenance

collaboration_core
  annotations, saved workspaces, formula versions,
  local pending edits, revision/conflict handling

daqcloud
  upload/asset integrity, permanent metadata SQLite,
  raw/decoded file archive, annotation/workspace APIs

daqapp
  native desktop shell, live CAN/debug panels,
  source selection, workspace UI, visualization
```

**Fork-specific differences:**

- **A:** `daqapp` includes custom timeline/widgets/plot/3D/video and `data_access` is PER-owned file/SQL/query cache logic. `daqcloud` serves range queries or immutable asset references.
- **B:** `daqapp` embeds `re_viewer`; add `rerun_adapter`, blueprint/workspace bridge, recording/source control; `daqcloud` additionally manages RRD files and Rerun OSS Catalog state/reconciliation. Avoid implementing a second large generic plot system.

**Suggested hard boundary:** Keep PER CAN decoding, time mapping, analysis definitions, annotations and workspaces decoupled from both the widget toolkit and RRD. That limits the cost of changing the frontend decision after a failed prototype.

---

## 9. Security, reliability and lifecycle

### 9.1 Canonicality and provenance

- The SD binary is authoritative for acquired telemetry.
- Live UDP is provisional; include a `source_kind` and explicit recording/session identity.
- Decoded assets are generated artifacts and should record parser/SuperDBC/software versions, time-correlation model, conversion version and raw source hash.
- Never erase or silently merge ambiguous overlapping data.
- Keep import and upload operations idempotent using hashes, IDs, versioning and atomic manifests.

### 9.2 Server durability

- Durable backup of raw binaries, decoded assets, manifests and SQLite metadata.
- Restart/rebuild of all derived query/catalog indexes from durable state.
- Concurrent read while adding new run files, with a published/committed state so partial uploads aren't visible as complete.
- Audit or revisions for user-edited annotations/workspaces/formulas.
- Proper authentication and authorization at DaqCloud. **Do not expose unauthenticated OSS Rerun Catalog directly to the public internet.**

### 9.3 Desktop packaging and offline operation

- A portable desktop build can be distributed as a ZIP; don't assume web is automatically easier or needed.
- Offline import/view/export is mandatory. Remote access is optional and should degrade cleanly when disconnected.
- Offline annotation edits need durable local storage; sync rules documented.
- Test Windows/macOS/Linux native dependency distribution and GPU drivers, especially if embedding Rerun with a pinned egui/eframe version.

### 9.4 Long-term access and migrations

- Raw sources must remain readable and reparsable independently of decoded format.
- Test ability to open and migrate old RRD (or custom) files after upgrades; Rerun documents limited forward compatibility guarantees and CLI migration/verification tooling.
- Record exact conversion/tool version and retain reproducible import code.

---

## 10. What is actually known versus hypothetical?

### Supported by stated PER workflow and teammate feedback

- SD raw logging is canonical; UDP telemetry is supplementary for live use.
- SD is physically removed after runs, parsed on a laptop and returned to the car.
- Offline and USB portability, shared annotations, permanent archive, MATLAB export are important.
- A native desktop primary app is preferred; separate web frontend is optional, not assumed.
- Current app architecture is **not fixed**; SQLite performance itself is acceptable.
- Better plots/timeline/derived signals and GPS/VD analysis are real user needs.
- LiDAR/video are future needs, not current hard acquisition sources.

### Verified Rerun capabilities and limitations (official docs; see references)

- Native Viewer uses Rust `egui`/`eframe` and can be embedded with `re_viewer`.
- Rust/Python/C++ SDK logs/streams data or saves RRD.
- RRD optimize adds indexed footer for demand reads; RRD supports file manipulation and merge/routing operations.
- OSS Catalog provides datasets/segments/layers and DataFusion-based queries.
- OSS Catalog is **in-memory registration metadata**, local-file-only registration, no auth or automatic directory watching.
- Viewer has a memory limit; old loaded data can be dropped.
- Native Viewer extension interfaces are unstable.
- Rerun has time-series, state timeline, map, 2D/3D and video representations.

### Unknown until PER prototypes it

- Real full-day recording size, required query rate and native plotting responsiveness.
- Whether one coherent **multi-segment track-day timeline** meets UX expectations through Rerun without awkward merging/duplication.
- Whether the embedded Viewer exposes sufficiently precise bidirectional time/range/selection events for PER annotation editors and debug widgets.
- Whether Rerun's own remote retrieval/backfill and RAM behavior matches desired interactive UI semantics.
- Quality/cost of MATLAB export from RRD for PER-specific schemas.
- Whether built-in Rerun plots satisfy vehicle dynamics requirements such as multiple Y axes, reusable formulas, point/interval annotation overlays, and multi-lap comparisons.
- Whether native Viewer dependency and extension maintenance is acceptable across PER's supported OSes.
- Relative person-hours and ongoing support load vs writing the needed custom visualization tool.

---

## 11. Decision matrix (qualitative, no fake precision)

| Criterion | More favorable path | Why / caveat |
|---|---|---|
| Earliest polished interactive plots/timeline | **B: Rerun** | Reuse mature Viewer mechanics; PER integration still required. |
| LiDAR/3D/video without building renderers | **B: Rerun** | Native views; ingestion and time sync still PER. |
| Total control of custom time/window/backfill UX | **A: Custom** | PER can design precise semantics, but owns implementation. |
| Generic SQL and MATLAB access | **A if Parquet/DuckDB** | B can export via Catalog/Arrow, but needs conversion. |
| Offline single-recording playback | Both | Local SQLite/Parquet/MCAP vs local RRD. |
| Portable USB dataset | Both | Need metadata/manifest and raw source handling either way. |
| Self-hosted secure, durable archive | Neither out of the box | PER builds DaqCloud regardless; B adds OSS Catalog state/recovery. |
| Annotation collaboration/workspaces | Neither out of the box | PER owns mutable app layer. |
| Cross-run, lap-relative analysis | Neither out of the box | PER analysis semantics and UX required. |
| Framework/version independence | **A: Custom** | B tied to Rerun internal viewer/version and RRD. |
| Smaller generic visualization maintenance | **B: Rerun** | Provided integration works and upgrades are manageable. |
| Ability to customize every UI interaction | **A: Custom** | B may require unstable custom view APIs. |
| Unified day timeline across segments | **Unknown** | Gate B explicitly; both require time-model design. |
| Transparent local/live/cloud operation | **Unknown** | Both need orchestration; B has built-in source paths but may not match desired abstraction. |

**Do not make this decision by totaling arbitrary scores.** Use hard gates (can it satisfy the workflows?) followed by measured implementation effort and usability.

---

## 12. Prototype plan and decision gates

### 12.1 Use the same fixture dataset for A and B

Prepare a reproducible test bundle containing:

- One **real PER SD raw recording** with a representative quantity of CAN/DAQ messages and GPS time observations.
- At least two separate recording segments/DAQ restarts in the same logical track day, ideally an actual gap or overlap.
- Mixed sample rates and discrete states/faults; injected/known UDP packet loss for live tests.
- Synthetic or public representative **LiDAR** frames and **compressed video** with known timestamps (until real PER sources exist).
- A set of reference MATLAB/Python calculations and exact expected answers/timestamp ranges.
- A small fixture of manual point and interval annotations and one shared workspace.

Record sizes/rates should be measured from actual data rather than guessed. Keep fixture, scripts and benchmark code in the repository.

### 12.2 Prototype A — custom, vertical slice

**Goal:** demonstrate the complete user interaction and a believable engineering effort to build the missing pieces, not create a finished DAQ platform.

- Live UDP → existing DaqCore → rolling buffer → responsive numeric plot.
- Timeline with live-follow, pause, click-to-seek, zoom/range select, discrete state lane and linked cursor.
- SD → parse → *one* chosen local store (SQLite or Parquet/DuckDB or MCAP) → open and navigate without internet.
- Remote `query(recording, signals, time_range, resolution)` mock endpoint; load visible range, prefetch adjacent, indicate loading/error.
- Save/reload a workspace, plot multiple signals, compute a derived quantity and export a range.
- Render one 3D point cloud and synced video clip, enough to estimate integration work.
- Sketch annotation creation/editing with one metadata store.

**Output:** working demonstration plus clear inventory of custom plot/timeline/video/3D/query/cache components not yet built.

### 12.3 Prototype B — Rerun-centered, vertical slice

**Goal:** prove Rerun can supply the polished UX *without* fighting PER's live/local/cloud semantics.

- Embed `re_viewer` in a minimal Rust `eframe` DaqApp; retain one PER CAN/debug panel and an archive browser.
- Map real DaqCore signals to stable Rerun entities, units, time axes, state changes and GPS.
- Stream representative live UDP data and pause/scrub without stopping reception.
- Convert the same SD fixture to optimized indexed `.rrd`; open locally and over USB.
- Start OSS Catalog, register at least two segments with metadata and a derived layer; connect embedded Viewer to it.
- **Required:** demonstrate a convincing unified *track-day time navigation experience*, including gaps, independent segments and annotations. Record any mismatch with the product vision.
- Integrate a mutable point/interval annotation panel with time selection and update events to a second client.
- Export selected time range and signals from RRD/Catalog to Parquet/MATLAB, with reference-value comparison.
- Load/time-seek LiDAR and video with the same workspace and cursor.
- Restart Catalog and reconstruct it from durable files/registry, without losing team metadata.

**Output:** working demonstration plus exact API/version choices, remaining custom adapters and blockers.

### 12.4 Shared benchmark scenarios

| ID | Test | What to record |
|---|---|---|
| T1 | Open one local run | Startup → first correct signal plot; memory/CPU; usability |
| T2 | Seek arbitrary local ranges | Median/p95 query→draw latency; correctness; staleness of states |
| T3 | Remote browsing over realistic network | Requests/bytes, p50/p95 latency, UI freezes, recovery after disconnect |
| T4 | Live follow for full driving session | End-to-end latency, dropped data, ring memory and backpressure |
| T5 | Pause live, scrub backward, return to live | Acquisition continuity, visible history, correct cursor/retention behavior |
| T6 | Import canonical SD after provisional UDP | Duplicate prevention, time alignment, source precedence, workspace refresh |
| T7 | One day with many segments | One navigable day axis, gaps/overlaps, annotations independent of physical segment |
| T8 | Numerical analysis | Multiple signals, units, different sampling rates, derivatives/integrals, formula reuse |
| T9 | Export exact time and signals | Bit/value/time fidelity, MATLAB interoperability and provenance |
| T10 | LiDAR and video | Frame/time sync, point-cloud performance, codec and seeking behavior |
| T11 | Annotation collaboration | Create/edit/delete, propagation, offline edits, revisions/conflicts |
| T12 | USB offline transfer | No server dependency, reopen workspace, all needed metadata available |
| T13 | Archive durability | Duplicate/partial uploads, checksum mismatch, process restart, catalog rebuild |
| T14 | Cross-platform distribution | Linux, Windows, macOS portable packaging, GPU and native dependencies |
| T15 | Upgrade compatibility | Old recording opens after app/library upgrade; conversion tools can migrate |
| T16 | Usability | Vehicle Dynamics/Mechanical engineers perform the same tasks without developer help |

**Provisional usability/performance goals** (targets to set with team, **not claims**): ordinary numeric pan/seek should feel immediate without blocking the UI; full-run live capture should not leak memory; missing data must be visibly marked; selected exports must exactly match a known reference; track-day navigation should not require manually loading many files; a new engineer should be able to make and share a useful plot without writing code.

### 12.5 Explicit decision gates

**Gate G1 — Rerun embedding feasibility.** Can we ship one portable DaqApp with embedded Viewer and PER debug tools on target OSes, with manageable dependency/API coupling?

**Gate G2 — Day-level navigation.** Can users see the required shared day time axis without confusing per-segment navigation, duplicated assets or untenable memory use?

**Gate G3 — Live/historical source UX.** Does live vs local RRD vs remote Catalog feel like one product? What special-case code and cache/backfill machinery remain?

**Gate G4 — Analysis parity.** Can vehicle dynamics perform actual multi-signal, formula, lap comparison, state/event and export tasks with Rerun's built-ins plus a reasonable PER extension layer?

**Gate G5 — Data/operational reliability.** Can the OSS Catalog be safely reconstructed after restart, securely served behind DaqCloud, and backed by durable file/upload state? Are indexed queries responsive on real recordings?

**Gate G6 — Long-term maintenance.** Is pinning and updating Rerun less work than owning the complete custom visualization stack?

**Decision logic:**

- If B passes all gates and materially reduces implementation/maintenance effort, choose **Rerun-centered**.
- If B fails a hard UX/data-model gate or requires pervasive unstable Viewer internals, choose **fully custom** even if Rerun's screenshots look better.
- If B works for display but its Catalog doesn't fit, investigate *Rerun Viewer + PER storage adapter* as a **documented fallback**, not automatically the final architecture; it may reintroduce exactly the bridge/caching complexity we tried to avoid.
- If both paths miss requirements, narrow v1 scope rather than pretending everything can be built at once.

---

## 13. Open questions to resolve in the next discussion

### Product / UX

1. Is one **shared calendar/day timeline** mandatory inside the Viewer itself, or can an integrated PER day navigator select segment recordings without hurting UX?
2. Which AdvantageScope plot features are truly hard requirements: dual axes, discrete lanes, linked crosshairs, equation editing, overlaid laps, distance axis, FFT, etc.?
3. Does “the same tools for live and historical” mean identical **workspaces/formulas**, or an entirely seamless single logical data source that automatically replaces live samples on SD import?
4. Is a small browser-only annotation form acceptable as an auxiliary tool, or absolutely everything must be in DaqApp?
5. Is immediate team-wide annotation visibility required on a disconnected track day, and if so will there be a shared pit-lane LAN server?

### Data / time

6. What are actual daily data sizes, channel counts/rates, clock resets and GPS synchronization availability?
7. Which data has a reliable UTC acquisition timestamp and which must remain device-relative?
8. How should raw CAN packet access relate to decoded signals? Are per-frame bytes, bitfields and fault debugging first-class historical queries?
9. When do we materialize derived signals vs evaluate them lazily? How do formulas respond to calibration and schema version changes?
10. How much full-day data should be visible at once? Are min/max envelope and per-run overview caches acceptable?

### Rerun-specific

11. Which Rerun Viewer, SDK and Catalog release can be pinned alongside DaqApp's chosen Rust/egui version?
12. Can the Viewer show independently archived segments and day annotations on one intuitively scrubbable time axis without creating huge duplicated whole-day recordings?
13. Are Viewer time cursor/visible-range/selection callbacks sufficiently accessible for PER annotation editing and debug panels?
14. How does a Viewer recover history after memory eviction when the source is a remote Catalog rather than a local RRD?
15. Can one app switch between live Rerun stream, local RRD and remote Catalog and restore one blueprint/workspace reliably?
16. Will RRD export to MATLAB/Python remain easy for users who want exact sparse samples or synchronized tables? Can we expose a straightforward CLI/API without requiring teammates to learn Rerun internals?
17. Is running an OSS Catalog behind DaqCloud's authenticated API acceptable, and who maintains its registration/recovery service?

### Delivery / team organization

18. Who owns UI/UX design, 3D/media, backfill/data query, analysis engine, DaqCloud operations and cross-platform testing?
19. Do we want to maintain a full viewer stack as a long-term PER research/engineering project, or prioritize vehicle engineering functionality?
20. What minimal release will be useful at the next track day, independent of the long-term decision?

---

## 14. Suggested handoff prompt for a future discussion

> We are Purdue Electric Racing and are deciding between **two desktop-first DAQ architectures**. Option A is a fully custom Rust DaqApp/DaqCloud platform that owns the timeline/visualizations/cache/storage/query/backend UX. Option B uses **Rerun's native embedded Viewer + RRD format + an RRD-backed DaqCloud wrapper (with Rerun OSS Catalog where appropriate)**, while retaining custom Rust CAN/UDP reception, SD parsing, formula engine, annotations, storage integrity, and hardware debug tools. Both must support live UDP, canonical raw SD-card imports, offline USB playback, remote browsing, a shared day timeline across recording segments, collaborative mutable annotations, saved workspaces, MATLAB export, and future LiDAR/video. The current repo is conceptual and not binding. SQLite is fast enough; complexity of the proposed local SQLite replication layer, not speed, was the concern. We want a **balanced evaluation, rigorous proof-of-concept plan, and explicit engineering tradeoffs**, especially on Rerun's global day timeline, native embedding, cache/backfill, OSS Catalog durability, and RRD export. Do not assume Rerun eliminates all backend logic or that we must create a separate web frontend. Please use the rest of this document as the source of requirements and identify tested facts versus open hypotheses.

---

## 15. References and provenance

**PER material / prior discussion:**

- [PER monorepo](https://github.com/PurdueElectricRacing/monorepo). Current structure used as **background**, not as a required design constraint; not comprehensively re-audited for this handoff.
- Previous architecture proposal in the chat, `PER_Telemetry_Architecture_Proposal.md` (November 9, 2026), evaluated SQLite/Parquet/DuckDB and earlier alternatives. Its **Parquet-first recommendation is superseded by this open two-candidate decision**, not silently carried forward.
- Teammate's `fsae_telemetry_discussion.md` (Grafana vs Rerun) and subsequent member interview/feedback with example screenshots (AdvantageScope/Nominal); some of the teammate's Catalog assumptions were incomplete/outdated.

**Official Rerun documentation, consulted November 12, 2026:**

- [How Rerun works — SDK, Viewer, Catalog](https://rerun.io/docs/concepts/how-does-rerun-work)
- [Native Viewer embedding and custom visualization extension points](https://rerun.io/docs/howto/visualization/extend-ui) — **unstable APIs** warning.
- [Rerun recordings / logical vs physical recordings](https://rerun.io/docs/concepts/logging-and-ingestion/recordings)
- [Rerun Catalog object model](https://rerun.io/docs/concepts/query-and-transform/catalog-object-model)
- [Open-source Catalog server limitations](https://rerun.io/docs/concepts/query-and-transform/oss-server)
- [Query data out of Rerun](https://rerun.io/docs/howto/query-and-transform/get-data-out)
- [Query/Catalog overview](https://rerun.io/docs/howto/query-and-transform/overview)
- [RRD inspection, migration, optimization, merging](https://rerun.io/docs/howto/logging-and-ingestion/manipulate-rrd-files)
- [Rerun layers for derived recordings](https://rerun.io/docs/howto/logging-and-ingestion/layers)
- [Viewer RAM limits](https://rerun.io/docs/howto/visualization/limit-ram)
- [State timeline views](https://rerun.io/docs/howto/visualization/state-timeline)
- [Built-in visualization types](https://rerun.io/docs/reference/types/views)
- [Catalog tables for query results and annotations](https://rerun.io/docs/howto/query-and-transform/catalog-tables)
- [Video representation and synchronization](https://rerun.io/docs/reference/types/archetypes/asset_video)
- [Rust native Viewer extension example](https://github.com/rerun-io/rerun/tree/main/examples/rust/extend_viewer_ui)

**Interpretation caution:** Official docs establish *available APIs/features*, not performance, ease of deployment, UI fit, or correctness on PER data. All such claims remain prototype tasks.

---

**Bottom line:** The decision is **own a visualization platform** versus **integrate with one and accept its data model/constraints**. The acquisition, raw archival integrity, PER-specific engineering semantics, formulas, annotations, and long-term collaboration service remain PER-owned in either case. The next useful step is a **same-data, same-workflow comparison**, not further speculative optimization of either architecture.
