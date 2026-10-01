# DaqApp FIL simulation

Run the firmware-in-the-loop ([FIL](https://github.com/ronakpjain/fil)) emulator as a live CAN
source. It executes real node firmware, so DBC-decoded telemetry matches hardware. DaqApp
communicates with FIL's `serve-network --transport stdio` binary protocol; see the [serve-network
protocol](https://github.com/ronakpjain/fil/blob/main/docs/serve_network.md).

## Connect

1. Build FIL (Release; `build-pgo/fil` if available — Debug builds emulate too slowly):
   ```bash
   cmake -S /path/to/fil -B /path/to/fil/build-release -G Ninja -DCMAKE_BUILD_TYPE=Release
   cmake --build /path/to/fil/build-release
   ```
2. Open **Add FIL Control** (sidebar) or **Spawn FIL Control** (command palette).
3. Pick a network source, then **Connect / Restart**:
   - **Network file**: select the `fil` binary and a network JSON. Toggle which boards run
     and override any board's ELF (useful when baked-in ELF paths are stale). The bus
     dropdown lists the network's declared buses.
   - **Build network**: no JSON needed. Set name, bus, bitrate, then **Add board…** to pick
     board configs with optional ELF overrides. **Export network JSON…** saves it for reuse.
4. Select the matching DBC to decode traffic.

**Run options** in FIL Control configure duration (0 runs until stopped), per-board instruction
budget, scheduling quantum, refresh interval, ADC decimation (1–1024), strict MMIO, wall-clock
pacing, loop batching, instruction tracing, and spin detection. Defaults use 1 ms refresh,
wall-clock pacing, loop batching, and 32× ADC decimation for responsive six-board viewing.
Set decimation to 1 when validating pedal or fault behavior: skipped ADC scans change
firmware-visible DMA updates.
Additional comma-separated live trace filters may be added; CAN transmit and GPIO input/output
filters, expectation lifecycle filters, and binary stdio control always remain enabled for DaqApp.
Add `instr` to the extra filters when enabling instruction tracing. Changes take effect on the next
**Connect / Restart**. Use a FIL executable that supports `serve-network --transport stdio`.

**Message Sender bus** selects the outgoing injection target. The separate **View/trace CAN bus**
selector chooses which network bus's CAN frames DaqApp displays and logs. Select **All buses** to
keep every transmission visible. The filter updates while connected without stopping or restarting
the FIL simulation; frames from hidden buses are discarded, not buffered for later viewing.

## Stimulus scripts

FIL stimulus scripts are attached to network configs and run by `serve-network` on the simulation
clock. In **Network file** mode, DaqApp uses the config's existing `stimuli` array. FIL resolves
relative script paths against the original network file; when board overrides or disabled boards
require DaqApp to materialize a temporary network config, it rewrites those paths to absolute paths
so they still refer to the original scripts.

In **Build network** mode, use **Add stimulus scripts…** to attach existing FIL stimulus JSON files.
The generated or exported network references those files by absolute path; exporting a network does
not copy or bundle the scripts, so keep them at those paths. DaqApp does not edit or translate the
scripts. See FIL's [stimulus-script format and timing](https://github.com/ronakpjain/fil/blob/main/docs/configuration.md#stimulus-scripts).

DaqApp owns the emulator process; disconnecting terminates it. Overrides are materialized into a
temp dir (originals untouched) and everything persists in `settings.json`. Failures are reported,
never auto-retried. Only CAN 2.0 data frames are supported; CAN FD and remote frames report send
errors.

## Expectations

Add an `expect` array to a FIL stimulus script and attach it to the network just like any
other stimulus script. For example:

```json
{
  "events": [],
  "expect": [
    { "type": "can", "bus": "vcan", "id": "0x321", "data": [1, 2],
      "at_ms": 20, "window_ms": 10 }
  ]
}
```

Use the bus name declared by your network. FIL checks firmware-generated CAN output for
an exact bus, ID, and payload match in the inclusive simulation-time interval from
`at_ms` to `at_ms + window_ms`; injected frames do not satisfy a check. See FIL's
[expectation format](https://github.com/ronakpjain/fil/blob/main/docs/configuration.md#stimulus-scripts)
for supported fields and validation rules.

The **Expectations** panel inside **FIL Control** displays FIL's authoritative lifecycle
traces: pending, passed, failed, or incomplete (the run ended before the check resolved).
It shows each check's expected frame (including standard/extended format) and time window,
with available match details. Checks appear as compact expandable cards using the script
filename and check index; full identifiers are available in tooltips. Windows and matched
times use milliseconds without losing nanosecond precision, and frame/result details wrap
onto separate lines. Results remain visible after disconnection and are cleared
on the next successful connection or with **Clear**. The panel retains up to 500 checks.
Checks are identified by FIL's `check_id`, so repeated script attachments stay distinct.
Expectation results are independent of the **View/trace CAN bus** filter. DaqApp does
not implement a separate expectation evaluator or infer success from displayed CAN frames.
Use a FIL build that emits expectation lifecycle traces; older executables cannot provide
these results. A pending check when the process stops is not proof of success or failure.

### GPIO and ADC annotations

Annotations live in the standalone `fil_annotations.json` beside DaqApp's `settings.json`
(run DaqApp from `daq/daqapp/`, as documented in its README). The checked-in file contains
labels for the PER firmware boards. Labels follow firmware pin/channel definitions and do
not verify physical harness wiring; boards without firmware-defined ADC mappings have no ADC
entries. Board keys must match the board `name` in the selected FIL network. GPIO port
labels appear in the port picker, pin labels beside each pin, ADC labels in the instance
picker, and channel labels beside the selected channel. These labels
only affect DaqApp's controls; they are not passed to FIL and do not change emulation. Edit
the file and restart DaqApp to reload it. If the file is missing, DaqApp falls back to its
embedded PER annotations. If it is unreadable or invalid, DaqApp continues without labels
and shows a warning in the FIL widget.

The file has this shape:

```json
{
  "gpio": {
    "dashboard": {
      "GPIOA": {
        "label": "Analog controls and steering inputs",
        "pins": {
          "0": "Regen 2 analog input",
          "1": "Brake pressure 1 analog input"
        }
      }
    }
  },
  "adc": {
    "dashboard": {
      "ADC1": {
        "label": "Dashboard analog controls",
        "channels": {
          "1": "Regen 2",
          "8": "Throttle 1"
        }
      }
    }
  }
}
```

Each top-level section is optional. GPIO port keys use `GPIOA`–`GPIOG`; pin keys are numbers
0–15. ADC instance keys use `ADC1`–`ADC4`; channel keys are numbers 0–19. Names are
case-sensitive; unknown board, port, or instance keys and out-of-range pins/channels are
ignored. Items absent from the file retain their existing names.

## Inject inputs

- **CAN**: use Message Sender; frames inject at the next simulation frontier.
- **ADC**: pick board/instance, set channel (0–19) and 12-bit value, **Inject**.
- **GPIO**: pick board/port; **Low**/**High**/**Release** drives one pin. Input and
  firmware-output states are shown separately across all 16 pins.

## Timing

Commands apply at simulation slice boundaries; FIL paces sim time to wall clock. DaqApp's
1 ms read wait bounds idle latency only — slow emulation or display refresh still delays
what you see.
