# FIL real-time integration

DAQApp2 can use a running FIL virtual CAN network as a receive-only CAN source.
The application launches FIL itself, keeps the simulation paced to wall time, and
feeds transmitted CAN 2.0 frames into the existing DBC decoder and widgets.

## Setup

1. Build FIL, including the `watch-network` support:

   ```bash
   cmake -S /path/to/fil-combined -B /path/to/fil-combined/build -G Ninja
   cmake --build /path/to/fil-combined/build
   ```

2. Start DAQApp2 with `cargo run`.
3. In **Connection Settings**, choose **Select FIL executable** and select the
   built `fil` binary.
4. Choose **Select FIL network** and select a network JSON configuration.
5. Open **Source** and choose **FIL real-time network**.
6. Select the matching DBC file to decode the emulated traffic.

The selected paths are saved in `settings.json`. Disconnecting or switching the
source terminates the child emulator process.

## Current boundary

The live FIL CLI stream is outbound-only. DAQApp2 receives `can_tx` records from
the emulated network, but Message Sender frames cannot yet be injected into an
already-running FIL process. Attempting to send reports a receive-only driver
error instead of silently discarding the frame.
