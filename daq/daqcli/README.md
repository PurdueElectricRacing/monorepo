# DaqCLI

Run commands from `daq/` with `cargo run -p daqcli -- ...`.

## Parse logs

Convert recorded `.log` files into CSV tables:

```sh
cargo run -p daqcli -- parse \
    --logs-dir <logs> --output-dir <csv> --superdbc <superdbc.json>
```

`--prefix` sets the output filename prefix (default: `out`). Generated SuperDBC
artifacts are in the repository's `superdbc/` directory.

## Watch CAN

Receive frames as JSON Lines on stdout. Logs go to stderr.

```sh
cargo run -p daqcli -- watch --serial /dev/tty.usbmodem101 --speed 500k
cargo run -p daqcli -- watch --udp 8000 --superdbc <superdbc.json> --bus 0
cargo run -p daqcli -- watch --simulated --superdbc <superdbc.json>
cargo run -p daqcli -- watch --loopback
```

Choose exactly one source: `--serial <path>`, `--udp <port>`, `--simulated`, or
`--loopback`. Serial speed is `250k` or `500k` (default). UDP receives 16-byte
timestamped frames. Loopback echoes transmitted frames.

`--superdbc` enables decoding; `--bus` requires it. The default is bus 0, or the
first bus in the artifact if bus 0 is absent. Without a database, watch uses raw
CAN IDs and payloads.

### Output

```json
{"time_ms":1762345678901,"sequence":0,"id":"0x100","dlc":2,"length":2,"data":"0102"}
```

`time_ms` is host receive time in Unix milliseconds. `sequence` starts at zero.
`id` and `data` are lowercase hexadecimal; `dlc` is the CAN DLC code and `length`
is the payload byte count.

With a database, records include `bus_id` and `bus`. Defined messages also have
`message`, `tx_node`, and `signals`. Signals are enum labels or physical numbers;
non-finite numbers are `null`.

### Send frames

Stdin accepts one classic CAN frame per line:

```text
0x100:0102
VCAN:0x101:ff
quit
```

Format: `ID:HEXDATA` or `BUS:ID:HEXDATA`. IDs are decimal or `0x`-prefixed hex;
payloads contain 0–8 bytes. A bus prefix requires `--superdbc` and must match the
selected bus name or decimal bus ID. Timestamps are not supported.

Frames are queued in input order, including repeated IDs. Stdin waits for the
worker to process each submission; the pending injection queue holds at most
65,536 frames. Invalid commands and queue overflow are reported on stderr.
**Sending to a physical adapter transmits onto the CAN bus.**

`quit`/`exit` drains accepted one-shot sends for up to two seconds while connected,
then stops. Unsent frames are reported. EOF leaves monitoring running. A broken
stdout pipe stops watch on the next frame. Set `RUST_LOG` to change logging
verbosity (default: `info`).
