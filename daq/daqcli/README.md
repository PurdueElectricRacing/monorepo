# PER DaqCLI

DaqCLI is a command-line companion to DaqApp. It runs the same `daqcore`
pipelines as the app without opening the GUI, which is handy for scripts, batch
jobs, and CI.

## Parse logs

`daqcli parse` runs the `daqcore::log_parse` pipeline used by the app's Log
Parser, so recorded `.log` files can be correlated into CSV tables. It takes
explicit paths, so it can be run from any working directory:

```bash
cargo run -p daqcli -- parse \
    --logs-dir <dir with .log files> \
    --output-dir <dir for CSV output> \
    --superdbc <path to superdbc_*.json>
```

Options:

- `--logs-dir` (required) - directory containing the raw `.log` files
- `--output-dir` (required) - directory to write CSV tables into
- `--superdbc` (required) - SuperDBC JSON artifact containing the bus definitions
- `--prefix` - output filename prefix (default: `out`)

SuperDBC artifacts are firmware build outputs found in `monorepo/superdbc`.

## Watch live CAN

`daqcli watch` connects to a live CAN source through the same `daqcore` worker
and driver stack as DaqApp. Received frames are written to stdout as JSON Lines,
and injection commands are read from stdin.

```bash
# CANable-compatible serial adapter
cargo run -p daqcli -- watch --serial /dev/tty.usbmodem101
cargo run -p daqcli -- watch --serial COM4 --speed 250k --superdbc <superdbc.json>

# UDP stream of 16-byte timestamped frames
cargo run -p daqcli -- watch --udp 8000 --superdbc <superdbc.json>

# Random frames for every message defined on the selected bus
cargo run -p daqcli -- watch --simulated --superdbc <superdbc.json>

# Echo frames transmitted by the adapter
cargo run -p daqcli -- watch --loopback
```

Options:

- `--superdbc` - SuperDBC JSON artifact used to name the bus and decode
  messages; without it, frames are emitted and injected as raw CAN
- `--bus` - SuperDBC bus ID to decode (defaults to the first bus)
- `--speed <250k|500k>` - serial adapter bit rate (default: `500k`)
- exactly one of:
  - `--serial <path>` - slcan adapter
  - `--udp <port>` - UDP receiver
  - `--simulated` - random frames for the selected bus definitions
  - `--loopback` - echo transmitted frames back as received frames

### Output

Each received frame is one JSON object followed by a newline:

```json
{"time_ms":1762345678901,"sequence":0,"bus_id":0,"bus":"VCAN","id":"0x51","remote":false,"fd":false,"brs":false,"dlc":8,"length":8,"data":"cffb1364bc42cda6","message":"motor_temps","tx_node":"MAIN_MODULE","signals":{"front_left":2561.9,"rear_right":-2283.5}}
```

- `time_ms` - host receive time, Unix milliseconds
- `sequence` - zero-based frame counter for this stream
- `bus_id`, `bus` - SuperDBC bus ID and name (omitted without `--superdbc`)
- `id` - CAN identifier in lowercase hexadecimal
- `remote`, `fd`, `brs` - CAN wire framing flags
- `dlc`, `length` - CAN DLC code and payload byte count
- `data` - payload bytes in lowercase hexadecimal
- `message`, `tx_node` - SuperDBC message and transmitting node; omitted when
  the frame has no definition (or no `--superdbc` is configured)
- `signals` - decoded values keyed by signal name, with the same omission
  rules; values are enum choice labels when available and physical values
  otherwise, and non-finite values are `null`

Logs go to stderr, so stdout carries only JSON Lines and can be piped directly
into tools like `jq`.

### Sending frames

While watching, stdin accepts one injection per line, following
`fil watch`:

```text
BUS[@TIME_MS]:ID:HEXDATA
ID:HEXDATA
```

- `BUS` - selected SuperDBC bus name or numeric bus ID; the bus prefix is
  optional with `--superdbc` and unavailable without it
- `TIME_MS` - accepted for `fil watch` compatibility; injections are immediate
- `ID` - CAN identifier, decimal or `0x`-prefixed hexadecimal
- `HEXDATA` - 0 to 8 payload bytes in hexadecimal (DAQ logs and adapters are
  classic CAN)

```text
VCAN:0x51:0102030405060708
VCAN@100:123:
456:aa
quit
```

Injections use the same `CanThreadCommand::AddSendMessage` path as DaqApp's
Message Sender. One-shot frames are queued in input order and never merged, so
repeated CAN IDs all transmit. Each line is validated against `daqcore` frame
rules and the selected SuperDBC bus before it is queued.

`quit` or `exit` stops the watch after flushing queued one-shot sends, so
messages queued just before quitting are written when the source is connected.
Stdin EOF leaves the stream running, so `printf ... | daqcli watch` keeps
monitoring after the pipe closes, and a closed stdout (for example `| head`)
stops the watch on the next received frame, like `tail -f`.

Set `RUST_LOG` to control log verbosity (for example `RUST_LOG=debug`).
Defaults to `info`.
