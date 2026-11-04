# DaqCLI

Run from `daq/`. Use `--help` on either subcommand for all options.

## Parse logs

Convert `.log` files to CSV (`--prefix` defaults to `out`):

```sh
cargo run -p daqcli -- parse \
    --logs-dir <logs> --output-dir <csv> --superdbc <superdbc.json>
```

Generated SuperDBC artifacts are in the repository's `superdbc/` directory.

## Watch CAN

Choose one source. Frames go to stdout as JSON Lines; logs go to stderr.

```sh
cargo run -p daqcli -- watch --serial /dev/tty.usbmodem101 --speed 500k
cargo run -p daqcli -- watch --udp 8000 --superdbc <superdbc.json> --bus 0
cargo run -p daqcli -- watch --simulated --superdbc <superdbc.json>
cargo run -p daqcli -- watch --loopback
```

Serial supports `250k` and `500k` (default). `--superdbc` enables decoding;
`--bus` requires it and defaults to bus 0, or the first bus if 0 is absent.

```json
{"time_ms":1762345678901,"sequence":0,"id":"0x100","dlc":2,"length":2,"data":"0102"}
```

`time_ms` is host receive time in Unix milliseconds; `sequence` starts at zero.
With a database, records add `bus_id` and `bus`; defined messages also include
`message`, `tx_node`, and `signals` (enum labels or physical numbers, `null` for
non-finite values). Buffer overflow stops capture with a nonzero exit status.

### Send frames

**Physical adapters transmit onto the CAN bus.** Stdin accepts one frame per line:

```text
0x100:0102
VCAN:0x101:ff
quit
```

Use `ID:HEXDATA` or `BUS:ID:HEXDATA`: decimal or `0x` IDs, 0–8 payload bytes,
no timestamps. Bus prefixes require `--superdbc` and must match the selected
bus name or decimal ID. Frames retain input order, including repeated IDs;
invalid commands and queue overflow are reported on stderr.

With `--superdbc`, encode a message on the selected bus by name:

```text
signals MESSAGE signal_a=12.5 signal_b=1
```

Replace the names with those in your database. Supply every signal exactly once,
using physical numeric values (not enum labels). Missing, duplicate, unknown,
non-finite, or unencodable values are rejected; omitted signals are never defaulted.
Names cannot contain whitespace. Signal commands use the same send queue as raw frames.

`quit`/`exit` allows up to two seconds to drain sends, then two more to finish
stdout; unwritten sends and output timeouts are reported. EOF keeps monitoring;
a detected broken pipe ends normally. Logging defaults to `info`; set `RUST_LOG`
to change it.
