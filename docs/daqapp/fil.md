# DaqApp FIL simulation

Run the firmware-in-the-loop ([FIL](https://github.com/ronakpjain/fil)) emulator as a live CAN
source. It executes real node firmware, so DBC-decoded telemetry matches hardware. See FIL's
[`watch-network` guide](https://github.com/ronakpjain/fil/blob/main/docs/watch_network.md) for
the control protocol and config format.

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
filters and stdin control always remain enabled for DaqApp. Add `instr` to the extra filters
when enabling instruction tracing. Changes take effect on the next
**Connect / Restart**. Use a FIL executable built with watch-network support for these options.

DaqApp owns the emulator process; disconnecting kills it. Overrides are materialized into a
temp dir (originals untouched) and everything persists in `settings.json`. Failures are
reported, never auto-retried. Only CAN 2.0 data frames are supported; CAN FD and remote
frames report send errors.

## Inject inputs

- **CAN**: use Message Sender; frames inject at the next simulation frontier.
- **ADC**: pick board/instance, set channel (0–19) and 12-bit value, **Inject**.
- **GPIO**: pick board/port; **Low**/**High**/**Release** drives one pin. Input and
  firmware-output states are shown separately across all 16 pins.

## Timing

Commands apply at simulation slice boundaries; FIL paces sim time to wall clock. DaqApp's
1 ms read wait bounds idle latency only — slow emulation or display refresh still delays
what you see.
