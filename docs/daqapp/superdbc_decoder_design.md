# daqcore CAN Database API Design ("superdbc" replacement for `can_decode`)

Status: **proposal for review** — no implementation yet.
Reference checkout: `~/Documents/PER/monorepo` (branch `millan/ram_cache`, HEAD `ed0e52b`).

---

## 1. Context & findings

### 1.1 What `can_decode 0.7.2` (crates.io) provides today

A `Parser` built from DBC text (via the `can-dbc` parser):

| API | Return | Used by |
|---|---|---|
| `from_dbc_file(path)` / `add_from_dbc_file` / `add_from_str` | `Result` | app startup, CAN thread, log parser |
| `decode_msg(id: u32, data: &[u8]) -> Option<DecodedMessage>` | heap per call | CAN thread (hot path), log parser |
| `encode_msg(id, &HashMap<String,f64>) -> Option<Vec<u8>>` | | send UI |
| `encode_msg_by_name(name, ...)` | | (unused in daqapp) |
| `msg_defs() -> Vec<can_dbc::Message>` (cloned), `msg_def(id)`, `msg_entries()` | | sidebar, log_parse `TableBuilder` |
| `signal_defs(id)`, `msg_desc(id)`, `signal_desc(id, name)` | | send UI, scope, log_parse |

Key output types (all `Clone`, heap-heavy):

```rust
DecodedMessage {
    name: String,                 // cloned into every decoded frame
    msg_id: u32, is_extended: bool,
    tx_node: String,              // cloned into every decoded frame
    signals: IndexMap<String, DecodedSignal>,  // per-frame map
}
DecodedSignal { name: String, unit: String, value: DecodedSignalValue }
DecodedSignalValue { physical: f64, raw: Option<i128>, enum_label: Option<String> }
```

Pain points confirmed by reading the call sites:

1. **`DecodedMessage` is stored in `messages::ParsedMessage`, cloned into `Frozen<DecodedMsgMap>`** (`ui/viewer_table.rs`), held in widget deques, and (per the ram-cache design docs, `docs/daqapp/ram_cache_design_v2.md`) will be stored in `RamCache` at up to ~10 Hz × 30 s per bus. `name: String`, `tx_node: String`, per-signal `String` keys, `Option<String>` enum labels, `i128` raws: roughly **1–2 KiB per stored frame** in string metadata alone. This is the real reason for "cleaner and faster" — the per-frame allocation cost, not the bit-packing math.
2. **Encoding takes `&HashMap<String, f64>`** — a hash lookup per signal per encode, and the UI must build a `HashMap<String, f64>` to send one message.
3. **`Parser` is DBC-coupled**: `msg_defs()` returns `can_dbc::Message`, so daqapp/daqcore must depend on `can-dbc` directly (they do, in ~10 files) just to read `name`/`id`/`signals`/`unit`/`min`/`max`/`value_type`/`size`.
4. **Multi-bus = multiple Parser instances** (`log_parser.rs` builds one per bus file). SuperDBC is inherently multi-bus in one document; the API should expose that.
5. No `Send`/`Sync` guarantees are documented, but `Parser` is only `Clone`; the design docs already mandate the parser be owned exclusively by the engine/CAN thread.

### 1.2 SuperDBC v1 (from PR #484 / `millan/super_dbc_v1_generation`)

Python generator (`generators/canpiler/superdbc/`) emits a single JSON document:

```
SuperDbc {
  content_hash: 64-hex sha256 of the normalized buses (schema v1),
  versions: { schema_version: 1, hash: "<git short hash>" },
  buses: { "<BUS_NAME>": Bus }        // ordered in file: CCAN, MCAN, SCAN, TEST_CAN, VCAN
}
Bus {
  bus_id: 0..=7,          // numeric slot, assigned by the generator
  baud_rate: u32,
  nodes: [{ name, is_external }],
  messages: [Message]
}
Message {
  id: u32 (≤ 0x1FFFFFFF), is_extended_id: bool,
  message_name, transmitter, receivers: [NodeName],
  length_bytes: 0..=8, nominal_period_ms: Option<u32>,
  priority: 0..=5, description: &str,
  signals: [Signal]                  // sorted by (start_bit, name)
}
Signal {
  signal_name, description: &str, data_type: &str,
  raw_type: "unsigned" | "signed" | "float32",
  start_bit: 0..=63, bit_length: 1..=64,
  byte_order: "little_endian" | "big_endian",
  scale: f64, offset: f64,
  limits: Option<{ min, max }>,      // present only when both specified
  unit: &str,                          // "" when none
  choices: Option<{ "raw_int_str": label }>   // sorted; bools → {"0":"OFF","1":"ON"}
}
```

Verified facts that shape the design:

- **Bit numbering is DBC-compatible.** Compared `a_box_fault_event` (id 1, 5 bytes) in `dbc/superdbc_ed0e52b.json` and `dbc/VCAN_ed0e52b.dbc`: `idx` is `0|16@1+` in DBC and `start_bit 0, bit_length 16, little_endian` in superdbc; `state` is `32|1@1+` ↔ `start_bit 32, bit_length 1`. The sawtooth bit numbering can be reused from `can_decode`'s `extract_signal_value`/`insert_signal_value` logic as-is.
- **Scale/offset always present** (defaulted to 1.0/0.0 at generation), `float32` is the only float type (no f64 double — unlike DBC's `SIG_VALTYPE`).
- **No CAN FD** (`length_bytes ≤ 8`); daqapp already just warns-and-drops FD frames.
- **Limits are optional** (14 of ~700 signals in the sample have them). The send UI derives slider ranges from `min`/`max`; the HIL engine expects limits for range checks — need a fallback policy (see §6).
- Sample fleet: 5 buses, 145 messages, max 16 signals/message, max signal 32 bits, 192 signals with choices, 9 extended-ID messages.
- `dbc/superdbc.schema.json` is checked in; the sample `dbc/superdbc_ed0e52b.json` (316 kB) exists in the worktree. The generator also still emits per-bus `*.dbc` files, so nothing is blocked on DBC retirement.

### 1.3 Who consumes this API

| Consumer | What it needs |
|---|---|
| **daqapp CAN thread** (`can/thread.rs`) | parse-time decode of every frame (hot path); owns the parser exclusively |
| **daqapp widgets** (sidebar, dbc_msg_picker, send, scope, jitter, log_parser UI) | list messages per bus, signal metadata (unit, min/max, choices, size, signedness, description), encode for send |
| **daqapp HIL** (`hil/run.rs`) | match by message name, read a signal's physical value + limits (range checks) |
| **daqcore `log_parse`** | `decode_msg` over historical frames + full metadata for CSV headers (msg desc, signal unit/desc) |
| **daqcli** (empty today) | same surface as daqapp minus UI; validates that everything is reachable from daqcore alone |

Per the (untracked, in-progress) `docs/daqapp/daqcore_engine_design.md` / `_option_b.md`: the parser lives in daqcore, owned exclusively by the engine/CAN thread, and the decode happens exactly where the `Time` stamp is applied. The design below assumes that shape; only the *database* types are replaced.

---

## 2. Design decisions

### D1. New module in daqcore, not a crates.io package

`daq/daqcore/src/superdbc/` (name settled: `superdbc`, matching the JSON format). `can-dbc`/`can_decode` drop out of the daqapp + daqcore dependency graphs. JSON deserialization via `serde_json` (daqapp already has it; daqcore gains it — ~1 direct dep, cheap, parse happens once at startup).

### D2. Two-tier type model: owned defs → interned handles

This is the core of "cleaner and faster". The database is **loaded once, immutable, and borrowed, not shared** — each domain that needs it owns a copy of the loaded DB (owned `Clone`, exactly the way today's `can_decode::Parser` is cloned into both `can::state` on the main thread and the CAN thread); decoded output is a **small owned value that references names by index**.

```
SuperDbc (owned per domain; Clone = shallow copy, cheap)
└─ buses: Vec<BusDef>
   └─ messages: Vec<MessageDef>          // sorted by (is_extended, id)
      └─ signals: Vec<SignalDef>
```

**Why no `Arc`:** `Arc` is only needed when you want *one* allocation shared across threads *and* can't afford a `Clone`. Here we can afford the clone — the DB is immutable and small (tens of KiB of strings), and `Clone` of the loaded DB copies only a few pointers into shared buffers (see D2b), so a "clone" is ~the cost of today's `Parser` clone, which daqapp already does across threads. `Arc` would instead leak the sharing machinery into `SuperDbc::load_file`'s return type, every `ParsedMessage` (if it carried `Arc<SuperDbc>` to resolve names — it doesn't), every daqcli future feature, and the type signatures of the whole crate. Reward of `Arc` here: save a handful of pointer copies at DB *replacement* time. Cost: reference-counted indirection on the hottest path (`Decoder` borrows through it) and an ownership model the rest of daqapp (which uses plain `Clone`d state today) doesn't already speak. **Verdict: not worth it.**

**Ownership & lifetime rules (the contract that replaces Arc):**
1. A **domain** (daqapp's process, a daqcli command, a log_parse job) loads the DB once at startup and owns it.
2. Within a domain, each **thread that decodes** owns its own `Decoder` (which borrows the DB) — the CAN thread and log_parse each have one; the DB itself may be owned by the main-thread state (as today's `Parser` is) with the CAN thread holding a cheaply-cloned DB, or by the CAN thread exclusively per the engine design docs. Either is fine: both are *exclusive* ownership per thread, and borrows are validated by the compiler at thread-spawn time.
3. `DecodedFrame` is **owned** (no lifetime — see §3). Widgets, `ParsedMessage`, and the UI cache store it directly — nothing borrowed ever crosses an mpsc boundary. This is what makes cross-thread frame storage safe without `Arc`.

Every name (`message_name`, `signal_name`, `unit`, `choice label`, `description`) is stored **once** in the database. Decoded output references them by index, never by `String`.

### D2b. How `Clone` stays cheap without `Arc` — string table + index handles

`*Def` structs contain **no `String`s and no raw borrows** — only numeric fields and `u32` indices into a `StringTable` that `SuperDbc` owns:

```rust
pub struct StringTable { /* one big Box<[u8]> + offsets; interned names/units/labels/descs */ }

pub struct SignalDef {              // fixed layout, no strings
    pub name: u32,                  // index into StringTable
    pub description: u32,
    pub data_type: u32,
    pub unit: u32,
    pub raw_type: RawType,
    pub start_bit: u8, pub bit_length: u8,
    pub byte_order: ByteOrder,
    pub scale: f64, pub offset: f64,
    pub limits: Option<(f64, f64)>,
    pub choices: Option<ChoiceRange>,   // contiguous slice of (raw: i64, label: u32) in the table
}

pub struct SuperDbc {
    tables: Box<Tables>,            // StringTable + choices + per-bus id-index (owned, one allocation group)
    buses: Vec<BusDef>,             // BusDef/MessageDef are index-heavy, string-free
}
```

Consequences:

- **`clone()` is a few pointer copies + one small metadata vec copy** — still **cheaper than today's `Parser` clone** (which deep-copies `can_dbc::Message` trees). daqapp already clones the `Parser` across threads (main-thread `state.parser` + CAN-thread `driver`); the same pattern works, with a smaller payload.
- **Name access is one method, resolved by the owner:** `db.signal_name(&sig) -> &str`, `db.msg_name(&msg)`, `db.unit(&sig)`, `db.choice_label(&sig, i)`. Return lifetime is `&'db` (the owner's), not `'static`. Widgets resolve names through the DB they already hold — this is where the unit string "lives", answering the original question: **it is not stored in any parsed/decoded output.**
- **No self-referential types, no `Arc`, no `Box::leak`.** `Send`/`Sync` fall out of `#[derive]` on owned data. The only `&mut`/borrow in the system is `Decoder<'db>` (its scratch buffer), which is thread-local.
- Cost of the indirection: one index-chase per name lookup, which only happens in UI rendering / log header building (per widget frame, not per CAN frame). The hot decode path returns numeric `SignalDef` copies + values and never touches the string table.
- Fallback if the string table is overkill: keep names as `String` inside `SignalDef` and accept a ~50 KiB deep copy on `clone()`. Still fine (clone happens at startup and on DB-file change, never per frame) — the index version is preferred because it makes per-thread DB clones O(1).

### D3. Decoded output is a small fixed-size owned value; no per-frame maps, no `String`s, no borrows

```rust
// Hot-path output (replaces can_decode::DecodedMessage).
// `message` is the index of the message in its bus (owned u32, not a borrow — see below).
pub struct DecodedFrame {
    pub msg_index: u32,                   // index into BusDef.messages → resolve names via &SuperDbc
    pub raw: [u8; 8],
    pub raw_len: u8,
    pub signals: [Option<SignalData>; MAX_SIGNALS],  // positional, MAX_SIGNALS = 16 (sample max)
    pub n_signals: u8,
}

pub struct SignalData {                   // the borrow-free "value half" — stored in ParsedMessage/cache
    pub physical: f64,
    pub raw: RawValue,                    // i32 | u32 | f32 — 8 bytes, no Option<i128>
    pub choice_index: Option<u16>,        // index into SignalDef.choices; label resolved via &SuperDbc
}
```

- `MAX_SIGNALS` is a constant (16 in the sample; loader rejects messages with more, or grows it — decided in Phase 0). The fixed array means **zero per-frame heap allocation**: `Decoder` fills a fixed buffer and returns it by value. `None` slots mark "signal absent because DLC is short".
- `DecodedFrame` is **`Copy` and fully owned** (~400 B at MAX_SIGNALS=16: 16 × 24 B `SignalData` + header) — it crosses threads freely (mpsc into the UI, into the RamCache) without any `Arc` and without borrows. Name/unit/label resolution happens at display time: `db.signal_name(&bus.messages[msg_index].signals[i])`, `db.unit(...)`, `db.choice_label(...)`. This is the direct answer to the "store the unit string" question: **unit/name/label are never stored in parsed output — the DB is the only copy, owned per domain (D2b).**
- Raw value range is `bit_length ≤ 64` but the sample max is 32 bits — `RawValue::U32 | I32 | F32` covers all realistic cases; if a wider raw is ever needed the enum gains an `i64/u64` arm. **Replaces `i128` + `Option` (16 B) with an 8-byte enum.**

**`ParsedMessage` becomes:** `{ timestamp, bus: u8, frame: DecodedFrame }` — ~430 bytes, **zero heap** (the raw payload lives inside the frame), vs ~1–2 KiB + several allocations today. The UI already holds the loaded DB (it needs it for the sidebar anyway), so name resolution costs one index chase per rendered cell. **No `Arc`, no owned-name fallback, no mpsc gymnastics.**

**Risk/weighting note (why owned-index instead of borrowed `&'db MessageDef` in the output):** borrows in the output would force `ParsedMessage` to outlive the decode buffer or be converted immediately, and any cache ring (RamCache) storing frames would pin a DB borrow for 30 s per bus. Owned `u32` indices + a DB that is owned-for-the-process-life make the output trivially storable. The cost — one extra indirection when a widget renders a name — is negligible (per rendered cell, not per frame) and the DB is L1-resident.

### D4. IDs as first-class handles, u32 at the wire boundary only

```rust
#[derive(Clone, Copy, PartialEq, Eq, Hash)]
pub struct MessageId(u32);      // encodes is_extended + raw id (like today's extid-flag u32)

impl MessageId {
    pub fn from_parts(is_extended: bool, id: u32) -> Self;
    pub fn from_wire_u32(wire: u32) -> Self;    // inverse of to_wire_u32 (extid-flag form)
    pub fn to_wire_u32(self) -> u32;        // extid-flag form, for slcan conversion
    pub fn raw(self) -> u32;                // without flag, for display/logging
    pub fn is_extended(self) -> bool;
}
```

Replaces the free functions `can_dbc_to_u32_with/without_extid_flag`. `decode`/`encode` take/accept `MessageId`; `to_wire_u32` is the **only** place an ID is ever a raw `u32` (the extid-flag form used at the transport boundary). daqcore takes a `MessageId` and never learns about any specific CAN transport (see D10 — slcan is being deprecated and must stay confined to the driver).

### D5. One document, N buses, per-bus hot-path index

`SuperDbc` owns everything; at load, each bus precomputes a **flat id→message-index** (the sample has 103 messages on VCAN; `Vec` + binary search by `(is_extended, id)` or a small hash map — microbench in Phase 0). `Decoder::new(db, bus_index)` binds to one bus, so decoding never touches the other 4 buses' metadata. `msg_index` (output of decode, input of name resolution) is this same index — one number flows from decode to UI to cache without any pointer.

### D6. Encode by signal index, not by name string

```rust
pub fn encode(&self, msg: &MessageDef, values: &[f64]) -> Option<[u8; 8]>
```

`values[i]` corresponds to `msg.signals[i]`. The send UI already walks `msg.signals` to build its sliders, so it can fill a `Vec<f64>`/`[f64; n]` positionally with zero hash lookups. (A convenience `encode_by_name` can exist later; not needed now.)

### D7. `SignalDef` is string-free; the DB owns the strings

`SignalDef` (as sketched in D2b) carries only numeric fields + string-table indices; `SuperDbc` exposes the resolvers:

```rust
impl<'db> SuperDbc {
    pub fn str(&'db self, idx: u32) -> &'db str;                 // generic table lookup
    pub fn signal_name(&'db self, s: &SignalDef) -> &'db str;
    pub fn unit(&'db self, s: &SignalDef) -> &'db str;           // "" when none (matches generator)
    pub fn choice_label(&'db self, s: &SignalDef, i: u16) -> Option<&'db str>;
    pub fn msg_name(&'db self, m: &MessageDef) -> &'db str;
}
```

Everything the UI needs today (unit, min/max, choices, size, signedness, description, data_type) and that HIL needs tomorrow (limits, physical value) is on `SignalDef`/`MessageDef` numerically; text goes through the resolvers above. **There is exactly one copy of every string in the process — inside the owned `SuperDbc`.**

### D8. Error handling: typed errors, not `Box<dyn Error>`

```rust
pub enum DbError {
    Io(std::io::Error),
    InvalidJson(serde_json::Error),
    UnsupportedSchemaVersion(u32),
    MissingField(&'static str),
    MalformedSignal { message: &'static str, signal: &'static str, why: String },
}
```

Load-time validation (schema_version == 1, bit fields in range, no overlapping bit ranges per message, `start_bit + bit_length ≤ length_bytes*8`, duplicate ids per bus, choices keys parse as i64) happens **at load, once** — decode/encode then cannot fail for reasons other than "unknown id" / "DLC shorter than needed".

### D9. What we deliberately do NOT port

- `FloatFormat::F64` — superdbc has `float32` only.
- `add_from_str`/multi-DBC merging — one document is the unit of load (the loaded DB is `Clone`-cheap per D2b, so multiple loaded DBs don't cost extra sharing machinery).
- CAN FD — schema caps at 8 bytes; keep the existing warn-and-drop.
- `encode_msg_by_name` — index-based encode is strictly better for the current UI.

### D10. Transport-neutral `CanFrame`; slcan confined to the driver layer

slcan is being phased out and must remain **driver-only**. Today `slcan` types leak through five daqapp files (`can/driver.rs`, `can/thread.rs` — 21 uses, `can/daq_parser.rs`, `connection.rs`, `util.rs`); after this change the **only** file that touches `slcan` is `can/driver.rs` (plus the one-line `CanBusSpeed::to_slcan_bitrate` in `connection.rs` if we want to go fully strict, which is a driver-adjacent config conversion).

New in `daqcore::can` (next to the existing `can.rs` id helpers, or `superdbc/id.rs` — it's id-adjacent):

```rust
#[derive(Clone, Copy, Debug)]
pub struct CanFrame {
    pub id: MessageId,      // transport-neutral (D4)
    pub data: [u8; 8],
    pub len: u8,            // ≤ 8; schema caps at CAN 2.0 (D9)
}
impl CanFrame {
    pub fn data(&self) -> &[u8] { &self.data[..self.len as usize] }
    pub fn from_slcan(f: &slcan::Can2Frame) -> Self;   // OPTIONAL feature-gated adapter
}
```

- `slcan::Can2Frame → CanFrame` is the **single conversion point**, at the driver's read path. Either (a) gate it behind a `feature = "slcan"` on daqcore (recommended — daqcore's default build has zero slcan knowledge), or (b) keep it as a one-liner in `can/driver.rs` and give `CanFrame` a plain `From`-style constructor. Either way `process_can_frame(frame: &CanFrame, ...)` in `thread.rs` stops seeing any `slcan` type — the 21 uses collapse to one conversion.
- `daq_parser.rs` (log CAN frames) and `util.rs` (`slcan_to_u32_*`) drop their `slcan` imports; `daq_parser` takes `&CanFrame`, `util::can` loses the slcan helpers entirely (subsumed by `MessageId` + `CanFrame`).
- `OutboundFrame` (send path) already carries a plain `u32 id + [u8]` — the driver builds the `slcan` frame there; no change, that's the write-side mirror of the same boundary.
- If/when slcan is fully replaced, only `driver.rs` changes; `thread.rs`, the parser, `superdbc`, and all UI are untouched.

---

## 3. Proposed public API (exact shape)

Module: `daqcore::superdbc`. All types are owned, `Clone`, and `Send + Sync` (they contain no raw borrows — strings live in the `StringTable` owned by `SuperDbc`).

```rust
// ── Load ────────────────────────────────────────────────────────────────
pub struct SuperDbc { /* tables: Box<Tables>, buses: Vec<BusDef>, version info */ }

impl SuperDbc {
    pub fn load_file(path: impl AsRef<Path>) -> Result<Self, DbError>;
    pub fn from_str(json: &str) -> Result<Self, DbError>;      // tests + daqcli

    pub fn buses(&self) -> &[BusDef];
    pub fn bus(&self, name: &str) -> Option<&BusDef>;
    pub fn schema_version(&self) -> u32;
    pub fn content_hash(&self) -> &str;
    pub fn version_hash(&self) -> &str;       // generator git hash, "which DBC am I running?"

    // string resolvers (D7)
    pub fn str(&self, idx: u32) -> &str;
    pub fn signal_name(&self, s: &SignalDef) -> &str;
    pub fn unit(&self, s: &SignalDef) -> &str;
    pub fn choice_label(&self, s: &SignalDef, i: u16) -> Option<&str>;
    pub fn msg_name(&self, m: &MessageDef) -> &str;
    pub fn signal(&self, msg: &MessageDef, name: &str) -> Option<&SignalDef>; // name-based convenience (table lives here)
}
impl Clone for SuperDbc { /* cheap: copies tables ptr + small metadata vec (D2b) */ }

// ── Bus / message access (defs are string-free; text via SuperDbc resolvers) ──
pub struct BusDef {
    pub name: u32,                       // StringTable index
    pub bus_id: u8,
    pub baud_rate: u32,
    pub nodes: Vec<NodeDef>,             // NodeDef { name: u32, is_external: bool }
    pub messages: Vec<MessageDef>,       // ordered as in file
}
impl BusDef {
    pub fn message(&self, id: MessageId) -> Option<&MessageDef>;  // via per-bus id index (D5)
    pub fn message_index(&self, id: MessageId) -> Option<u32>;    // the flat index
}

pub struct MessageDef {
    pub id: MessageId,
    pub name: u32,                       // StringTable index
    pub transmitter: u32,
    pub receivers: Vec<u32>,             // StringTable indices
    pub length_bytes: u8,
    pub nominal_period_ms: Option<u32>,
    pub priority: u8,
    pub description: u32,
    pub signals: Vec<SignalDef>,         // ≤ MAX_SIGNALS
}
impl MessageDef {
    pub fn encode(&self, values: &[f64]) -> Option<[u8; 8]>;      // values[i] ↔ signals[i] (D6)
}

pub struct SignalDef { /* string-free fields + indices; see D2b */ }

// ── Decode (hot path) ───────────────────────────────────────────────────
pub struct Decoder<'db> { /* &'db SuperDbc + bus binding + scratch buffer */ }
impl<'db> Decoder<'db> {
    pub fn new(db: &'db SuperDbc, bus: u32) -> Self;      // bus = index into db.buses()
    /// `raw` is the ≤8-byte payload; returns None only for unknown MessageId.
    /// DLC < length_bytes: signals with bits beyond raw.len()*8 are marked absent.
    pub fn decode(&mut self, id: MessageId, raw: &[u8]) -> Option<DecodedFrame>;
}

// DecodedFrame / SignalData are owned + Copy (D3): cross threads / cache freely.
pub struct DecodedFrame {
    pub msg_index: u32,
    pub raw: [u8; 8],
    pub raw_len: u8,
    pub signals: [Option<SignalData>; MAX_SIGNALS],
    pub n_signals: u8,
}
pub struct SignalData {
    pub physical: f64,
    pub raw: RawValue,
    pub choice_index: Option<u16>,
}
#[derive(Clone, Copy, Debug, PartialEq)]
pub enum RawValue { U32(u32), I32(i32), F32(f32) }        // covers ≤32-bit u/s + float32
```

Notes:

- **No `Arc` anywhere.** `SuperDbc` is owned per domain and `Clone`-cheap (D2b). `Decoder<'db>` borrows the owner; it is thread-local (the CAN thread owns one, log_parse owns its own) — matching the "parser owned exclusively by the thread" constraint in the engine design docs.
- `DecodedFrame` is an **owned value**, not a borrow: it's what goes into `ParsedMessage` and the RamCache. The UI resolves names via the DB it already holds (it needs the DB for the sidebar regardless), costing one index chase per rendered cell — never on the decode path.
- `encode` is a pure method on `MessageDef` (no `&mut`, no state) — trivially callable from any thread holding `&SuperDbc`.
- `MAX_SIGNALS` (16 for the sample) is a crate constant; the loader rejects larger messages until it's bumped.

---

## 4. Module layout in daqcore

```
daq/daqcore/src/
├── can.rs                  (existing id-conversion helpers; re-expressed on MessageId; + CanFrame, D10)
├── log_parse/              (existing; swaps can_decode::Parser → SuperDbc + Decoder)
└── superdbc/               (NEW)
    ├── mod.rs              (public re-exports)
    ├── schema.rs           (serde structs mirroring JSON v1, extra="forbid")
    ├── db.rs               (SuperDbc/BusDef/MessageDef/SignalDef + interned strings + load/validate)
    ├── id.rs               (MessageId)
    ├── decode.rs           (Decoder, bit extraction — port from can_decode, sawtooth logic)
    └── encode.rs           (bit insertion — port from can_decode)
```

Dependencies added to `daqcore/Cargo.toml`: `serde` (derive), `serde_json`. Removed: `can_decode`, `can-dbc` (from daqcore **and** daqapp).

---

## 5. Performance plan (the "faster" part)

1. **Decode allocations: 2–3/frame → 0.** `DecodedFrame` is a fixed-size owned value (no heap at all); `Decoder` owns a fixed scratch buffer reused every call. Bit packing is the proven `can_decode` bit-walk (or a per-signal precomputed `(byte_off, bit_off)` table at load — the sawtooth mapping for big-endian can be precomputed once per signal; microbench both in Phase 0).
2. **Stored frames (RamCache): ~1–2 KiB + 3–4 allocs → ~420 B, zero allocs per frame** (fixed `DecodedFrame` value, D3). This is the win that matters for the 30 s ring cache: 10 Hz × 30 s = 3 k frames/bus → ~1.3 MiB of pure values (vs ~3–6 MiB of cloned metadata today), and the cache slot is a `memcpy`-able value with nothing to free.
3. **Encode: O(signals) with zero hashing** (positional values).
4. **Load: one `serde_json` pass + O(total bits) validation** — ~316 kB JSON, tens of ms, at startup.
5. **No per-frame `HashMap` construction** in `TableBuilder`/viewer-table (those build `HashMap<String, DecodedSignal>` today per decoded frame in log parsing).

Benchmark gate for Phase 0: `cargo bench` (criterion, dev-dependency only) on decode with the real `dbc/superdbc_ed0e52b.json` + captured frame samples, comparing (a) current `can_decode`, (b) new decoder, before/after the scratch-buffer vs precomputed-bit-offset variants.

---

## 6. Open decisions for review

1. **Module/crate naming**: **settled — `daqcore::superdbc`** (user preference, matches the JSON format name).
2. **`ParsedMessage` shape**: settled by D3 — `ParsedMessage = { timestamp, bus: u8, frame: DecodedFrame }` (fully owned ~430 B value, zero heap, no DB reference); the UI resolves names via the `SuperDbc` it already holds for the sidebar.
3. **Missing limits**: send-UI sliders and HIL range checks assume `min`/`max`. Superdbc has them for only 14/~700 signals. Policy options: (a) derive from bit range + scale/offset when absent, (b) slider spans full raw range, (c) require CANpiler to always emit limits (upstream change). Leaning (a)+(b).
4. **Out-of-range encode behavior**: clamp (like `can_decode`) vs `None`. Leaning clamp for slider-originated sends, `None` for programmatic.
5. **Choice/bool rendering**: `choices` are now explicit for bools (`OFF/ON`). `default_format` today renders `"{label} ({raw})"` for enums — keep as-is?
6. **Where the superdbc file is discovered**: `settings::dbc_dir()` (`../../dbc`) will contain `superdbc_<hash>.json`. Plan: auto-pick `superdbc_*.json` (highest version hash / most recent) when no per-bus file is configured, keep manual file dialog. Confirm.
7. **Alignment with the in-progress engine design docs** (`docs/daqapp/daqcore_engine_design*.md`, untracked): they reference `can_decode::Parser`/`DecodedMessage` at ~12 sites. Plan is to make this design the "database" layer under whichever engine option (A or B) lands, and update those docs' type references when this is approved. Flagging so the two efforts don't drift.

---

## 7. Phased implementation plan

**Phase 0 — Foundation (daqcore only, no daqapp changes)** — *S*
- `superdbc/` module: `schema.rs` (serde, `extra = "forbid"`, strict), `id.rs`, `db.rs` (load + full validation + interned strings + per-bus id index), `decode.rs`/`encode.rs` (bit logic ported from `can_decode`, sawtooth included).
- Unit tests: round-trip encode→decode for every message in `dbc/superdbc_ed0e52b.json` against the *DBC-generated* values; cross-check N sampled messages against `can_decode`'s DBC decode output (temp dev-dependency, then remove); sawtooth edge cases (big-endian multi-byte, bit 32 boundary, 64-bit); validation rejects (schema v2, overlapping bits, bad choices key); criterion microbench vs `can_decode`.
- Deliverable: `daqcore::superdbc` compiles + tests green; bench report answering §5.1 (scratch buffer vs precomputed offsets).

**Phase 1 — daqapp decode path cutover** — *M*
- Replace `Option<can_decode::Parser>` (state.rs / driver.rs / thread.rs / app.rs `ParserInfo`) with `SuperDbc` (owned in main-thread state; CAN thread gets a cheap clone per D2); CAN thread builds `Decoder` for the active bus(es); `ParsedMessage` per §6.2.
- Widgets reading decoded frames (viewer_table, scope, gg_plot via cache, hil/run.rs `matches_message` → compare `MessageDef.name` or id) updated.
- DBC picker: dialog filter `["json"]` on `dbc/superdbc_*.json` (§6.6); `dbc_dir()` logic updated.

**Phase 2 — daqapp encode + metadata UI cutover** — *M*
- send.rs: sliders from `SignalDef.limits` (§6.3 policy), positional `values` vec, `MessageDef::encode`; choices rendered from `SignalDef.choices`.
- dbc_msg_picker / jitter / scope signal pickers: iterate `BusDef.messages`/`MessageDef.signals` (no more `can_dbc::Message`).
- formatter.rs: `sig_def: &can_dbc::Signal` → `&SignalDef` (hex/binary formatting from `bit_length`/`raw_type`).

**Phase 3 — daqcore log_parse cutover** — *S*
- `log_parse::parse/table/correlate`: `can_decode::Parser` → `SuperDbc` + `Decoder` (loaded once per parse job); `TableBuilder` header from `BusDef`/`MessageDef`/`SignalDef` metadata via `SuperDbc` resolvers; drop per-frame `SignalMap`/`HashMap` construction.
- log_parser.rs UI: two-bus flow becomes "one superdbc, pick two buses" (or keep per-bus overrides; §6.6).

**Phase 4 — Cleanup** — *S*
- Remove `can_decode` + `can-dbc` from workspace `Cargo.toml` and both crates; delete `can_dbc_to_u32_*` + `slcan_to_u32_*` helpers (subsumed by `MessageId`/`CanFrame`); delete DBC file dialog remnants; update the two engine-design docs' references; daqcli smoke-test binary that loads the superdbc and decodes a recorded `.log`.
- Confinement check (D10): `grep -r slcan daq/` must hit only `can/driver.rs` (and the optional feature-gated adapter in daqcore).

Risks: (a) bit-packing divergence from `can_decode` on exotic big-endian layouts — mitigated by Phase 0 cross-check tests against the DBC build of the same system; (b) the untracked engine-design docs being mid-flight — mitigated by keeping this layer decoupled from threading ownership (pure data + `&self`/`&mut` API, no thread logic).

---

## 8. Worked examples: every access/encode/decode pattern, old → new

Grounded in the current daqapp/daqcore call sites. Each pattern shows the existing
`can_decode` code (abbreviated, faithful to the call site) and its `superdbc`
replacement, plus what got cleaner or cheaper. **Bold** marks the part that
changes behavior/perf, not just syntax.

### P1. Load (app startup + CAN thread)

**Today** (`can/thread.rs`):
```rust
match can_decode::Parser::from_dbc_file(&path) {
    Ok(parser) => { state.parser = Some(parser); ... }
}
// one Parser per bus file; `state.parser: Option<can_decode::Parser>`
```

**New** (`can/thread.rs`, daqcore::superdbc):
```rust
let db: SuperDbc = SuperDbc::load_file(&path).map_err(|e| /* DbError → log */)?;
// db is ONE document with all 5 buses. Clone is cheap (D2b) — the CAN thread
// takes `db.clone()`; no second file read, no second parse.
let bus = db.bus("VCAN").expect("active bus missing from superdbc");
let bus_idx = db.buses().iter().position(|b| b.name == bus.name).unwrap() as u32;
let mut decoder = Decoder::new(&db, bus_idx);
```

**Cleaner / faster:** one parse instead of N (the multi-bus `log_parser.rs`
"one Parser per file" disappears). The CAN thread holds one `Decoder` bound to
its bus, so a decode never walks the other 4 buses.

---

### P2. Decode hot path (per CAN frame)

**Today** (`can/thread.rs::process_can_frame`):
```rust
let decode_msg_id = util::can::slcan_to_u32_with_extid_flag(&frame2.id());
let raw_msg_id    = util::can::slcan_to_u32_without_extid_flag(&frame2.id());
let is_msg_id_extended = matches!(frame2.id(), slcan::Id::Extended(_));
let data = frame2.data().unwrap_or(&[]);
let decoded = state.parser.as_ref().and_then(|p| p.decode_msg(decode_msg_id, data));
// → Option<DecodedMessage>: allocs name:String, tx_node:String,
//   a per-frame IndexMap<String, DecodedSignal>, per-signal name:String+unit:String,
//   raw: Option<i128>, enum_label: Option<String>. ~2-3 heap allocs + several Strings.
```

**New** (operates on `CanFrame` — slcan is gone by the time we get here, D10):
```rust
fn process_can_frame(frame: &CanFrame, decoder: &mut Decoder, state: &mut State) -> usize {
    let decoded = decoder.decode(frame.id, frame.data());   // → Option<DecodedFrame>
    // DecodedFrame = { msg_index: u32, raw: [u8;8], raw_len,
    //                   signals: [Option<SignalData>;16], n_signals }
    // **Zero heap, no Strings, no map** — values only (D3).
    let parsed = messages::ParsedMessage {
        timestamp: chrono::Local::now(),
        bus: state.bus_id,
        frame: decoded.unwrap(),
    };
    ...
}
```

**Cleaner / faster:** **0 heap allocations** vs 2–3/frame; the frame is a
`Copy`-sized owned value that mpsc + RamCache store by value with nothing to
free. `CanFrame`/`MessageId` collapse the three lines of `slcan_to_u32_*` +
`is_msg_id_extended` flag-juggling into a single neutral `frame.id` (extended-ness
lives *inside* the id), and remove every `slcan` type from the decode path (D10).

---

### P3. `ParsedMessage` (what crosses into the UI / cache)

**Today** (`messages.rs`):
```rust
pub struct ParsedMessage {
    pub timestamp: chrono::DateTime<chrono::Local>,
    pub raw_bytes: Vec<u8>,              // alloc
    pub msg_id: u32,
    pub is_msg_id_extended: bool,
    pub decoded: can_decode::DecodedMessage,  // name:String, tx_node:String,
                                              // signals: IndexMap<String, DecodedSignal>
}
```

**New**:
```rust
pub struct ParsedMessage {
    pub timestamp: chrono::DateTime<chrono::Local>,
    pub bus: u8,
    pub frame: DecodedFrame,             // ~400 B fixed value, zero heap
}
```

**Cleaner / faster:** `msg_id`/`is_msg_id_extended`/`raw_bytes` collapse into
the frame (`frame.msg_index`, `frame.raw`, `frame.raw_len`). **~1–2 KiB +
several allocs → ~430 B, zero heap** — the number that matters for the
30 s RamCache.

---

### P4. Read a value + format it (viewer table / formatter)

**Today** (`ui/viewer_table.rs` + `formatter.rs`): the name and unit are
*already cloned into the frame*, and the widget re-looks-up the def to get
`size`/`value_type` for hex/binary:
```rust
// from the decoded frame (name/unit are Strings carried in the frame)
let (sig_name, signal) = ...;                       // signal.name:String, signal.unit:String
let msg_def = parser.as_ref().and_then(|p| p.msg_def(msg_id));   // per-card lookup
let sig_def = msg_def.and_then(|md| md.signals.iter().find(|s| s.name == *sig_name));
formatter::try_format(formatter, &msg.decoded.name, sig_name,
                      sig_def /* Option<&can_dbc::Signal> */,
                      Some(&signal.unit),
                      &signal.value);
// try_format(..., sig_def: Option<&can_dbc::Signal>, unit: Option<&str>,
//            value: &can_decode::DecodedSignalValue) -> String
//   value.physical / value.raw: Option<i128> / value.enum_label: Option<String>
```

**New**: names/units come from the DB via index; the value is already in the
frame; the def is reached **positionally** (no string `find`):
```rust
let bus = &db_buses[state.bus_id];
let msg = &bus.messages[frame.msg_index as usize];
for i in 0..frame.n_signals as usize {
    let sd  = frame.signals[i];                     // Option<SignalData>
    let sig = &msg.signals[i];                      // SignalDef — positionally aligned
    let (name, unit) = (db.signal_name(sig), db.unit(sig));   // one index-chase each
    let value = sd.as_ref().unwrap();
    format_value(formatter, name, unit, sig, value); // sig: &SignalDef (bit_length, raw_type, choices)
}
```

**Cleaner / faster:** the `md.signals.iter().find(|s| s.name == *sig_name)`
string search is gone — **`msg.signals[i]` is the def for `frame.signals[i]`**
(same order, guaranteed by construction). `can_dbc::Signal` (with
`NumericValue` min/max, `value_type` enum) is replaced by a flat `SignalDef`
with `f64` limits + `RawType`. `i128` raw + `Option<String>` enum_label
becomes an 8-byte `RawValue` enum + a `u16 choice_index`.

---

### P5. Scope / jitter (subscribe to one signal's physical value)

**Today** (`ui/scope.rs`, `ui/jitter.rs`):
```rust
if parsed_msg.decoded.msg_id != *target_msg_id { return; }
let Some(signal) = parsed_msg.decoded.signals.get(signal_name) else { return; };
self.add_point(parsed_msg.timestamp, signal.value.physical);
```

**New**:
```rust
if frame.msg_index != *target_msg_index { return; }          // u32 == u32
let Some(signal) = frame.signals.get(*target_sig_index as usize) else { return; };
self.add_point(timestamp, signal.physical);                  // .physical directly
```

**Cleaner / faster:** `msg_id` (with extid-flag) comparison → plain `msg_index`
comparison; `IndexMap::get(&String)` → **array index**; `signal.value.physical`
(2 levels of nesting) → `signal.physical`. The widget binds
`(msg_index, sig_index)` at selection time instead of a `(u32, &str)` pair.

---

### P6. Encode + send (send UI)

**Today** (`ui/send.rs`): build a `HashMap<String, f64>` per send, then hash
each signal name inside `encode_msg`:
```rust
fn encode_msg_from_signals(parser: &can_decode::Parser,
                           msg_id_with_ext_flag: u32,
                           signals: &[SignalValue]) -> Option<Vec<u8>> {
    let values_hashmap: HashMap<String, f64> =
        signals.iter().map(|s| (s.name.clone(), s.value)).collect();  // alloc + N clones
    parser.encode_msg(msg_id_with_ext_flag, &values_hashmap)          // N hash lookups
}
// slider range also needs: can_dbc_to_u32_with_extid_flag(&selected_msg.id)
//   + signal_range(&can_dbc::Signal) → NumericValue → f64 conversion
```

**New**: the UI already walks `msg.signals` to build sliders, so it fills
values **positionally** — no map, no hash:
```rust
let values: Vec<f64> = self.signal_values.iter()
    .map(|s| s.value)                 // s[i] already corresponds to msg.signals[i]
    .collect();
let bytes: Option<[u8; 8]> = msg.encode(&values);   // MessageDef::encode (D6)
// slider range straight from SignalDef:
let (lo, hi) = match sig.limits { Some((l,h)) => (l,h), None => /* derived, §6.3 */ };
// id: msg.id (MessageId) — no can_dbc_to_u32_* conversion call
```

**Cleaner / faster:** **zero hashing and zero per-send `HashMap` allocation.**
`NumericValue` (a DBC enum that needed `can_dbc_numeric_to_f64`) is just an
`f64`. Returns `[u8; 8]` (stack) instead of `Vec<u8>`.

---

### P7. Message picker (list/search messages per bus)

**Today** (`ui/dbc_msg_picker.rs`):
```rust
pub struct DbcMsgPicker { search_results: Vec<can_dbc::Message>, ... }
fn refresh_results(&mut self, parser: &can_decode::Parser) {
    self.search_results = parser.msg_defs().clone();      // clones the whole message tree
    // sort: can_dbc_to_u32_without_extid_flag(&msg.id)
}
```

**New**:
```rust
pub struct SuperDbcMsgPicker { search: u32 /* msg_index */, ... }
fn refresh(&mut self, bus: &BusDef) -> &[MessageDef] { &bus.messages }  // zero-copy borrow
// search matches on db.msg_name(&m) + m.id — no tree clone, no id conversion
```

**Cleaner / faster:** `msg_defs()` **clones every `can_dbc::Message`** (name,
transmitter, receivers, all signals) into the picker on every refresh; the new
code borrows `bus.messages` (a `Vec<MessageDef>` of string-free defs) and
filters by index. No `can_dbc::Message` in the daqapp dependency graph at all.

---

### P8. HIL (match a message, range-check a signal)

**Today** (`hil/run.rs`): matches on the **cloned name string** in the frame,
then reads the physical value:
```rust
pub fn matches_message(&self, decoded: &can_decode::DecodedMessage) -> bool {
    self.expect.msg_name == decoded.name                 // String == String, per frame
}
...
let value = parsed.decoded.signals.get(sig_name).unwrap().value.physical;
if value < lo || value > hi { /* fail */ }
```

**New**: the expectation resolves name → `(msg_index, sig_index)` **once at
plan load**, runtime is pure index compare:
```rust
// at HIL plan load (has &SuperDbc):
let (msg_index, sig_index) = resolve_expect(db, bus, &expect.msg_name, &expect.signal);
// per frame (no DB, no strings):
let matches = frame.msg_index == msg_index;
let value   = frame.signals[sig_index as usize].as_ref().unwrap().physical;
if value < lo || value > hi { /* fail */ }
```

**Cleaner / faster:** per-frame `String == String` (and the name was *cloned
into every decoded frame just for this*) → **`u32 == u32`**. The range
check reads `SignalData.physical` directly.

---

### P9. log_parse (build the CSV header from metadata)

**Today** (`daqcore/log_parse/table.rs::create_header`): per-bus `Parser`,
clone the message defs, and string-lookup descriptions:
```rust
let mut message_defs = parser.msg_defs();               // clone tree
message_defs.sort_by_key(|m| can::can_dbc_to_u32_without_extid_flag(&m.id));
for msg in message_defs {
    let msg_desc  = parser.msg_desc(msg_id_u32).map(|d| d.to_string())...;  // .to_string() alloc
    for sig in &msg.signals {
        let sig_desc = parser.signal_desc(msg_id_u32, &sig.name).map(|d| d.to_string())...;
        push_column(TableColumn { signal: sig.name.clone(),   // String clone
            signal_unit: sig.unit.to_string(), ... });
    }
}
```

**New**:
```rust
for msg in &bus.messages {                               // borrow, no clone
    let msg_desc = db.str(msg.description);               // &str, no .to_string()
    for sig in &msg.signals {
        let sig_desc = db.str(sig.description);
        push_column(TableColumn { signal: db.signal_name(sig),  // &str
            signal_unit: db.unit(sig), ... });
    }
}
// decode loop: let frame = decoder.decode(MessageId::from_wire_u32(arb_id), &frame.data)
```

**Cleaner / faster:** no `msg_defs()` tree clone, no per-cell `.to_string()`
allocations for the header (borrow `&str` until the CSV row is written), no
`can_dbc_to_u32_*` conversions. Same single-pass-over-one-bus shape as P7.

---

### Pattern summary

| Pattern | Today (`can_decode`) | `superdbc` | What improved |
|---|---|---|---|
| P1 Load | N `Parser::from_dbc_file` | 1 `SuperDbc::load_file` | one parse, all buses |
| P2 Decode | `decode_msg` → heap `DecodedMessage` | `Decoder::decode` → `DecodedFrame` | 2–3 allocs+Strings → **0** |
| P3 ParsedMessage | `raw_bytes:Vec` + `DecodedMessage` | `{ts, bus, frame}` | ~1–2 KiB → ~430 B |
| P4 Read+format | `signals.get(name)` + `find` def | `msg.signals[i]` positional | no string search, no `i128` |
| P5 Scope/jitter | `msg_id!=` + `IndexMap::get(&str)` | `msg_index==` + array index | hash lookup → index |
| P6 Encode/send | `HashMap<String,f64>` + `encode_msg` | positional `&[f64]` + `msg.encode` | **no hash, no map alloc** |
| P7 Picker | `msg_defs().clone()` tree | borrow `bus.messages` | no tree clone |
| P8 HIL | `String==String` per frame | `u32==u32` per frame | no per-frame name clone/cmp |
| P9 log_parse | `msg_defs()` clone + `.to_string()` | borrow + `&str` resolvers | no clone, no allocs |

The two structural wins that recur: **(a)** decoded output carries *values +
indices*, never `String`s (P2/P3/P8) — the DB is the single owner of every
name/unit/label; and **(b)** every per-frame lookup that was a string hash or
linear `find` today is a positional array index (P4/P5/P6/P8), because
`DecodedFrame.signals[i]` and `MessageDef.signals[i]` share one ordering.
