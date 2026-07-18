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

The **FIL bus** field selects the virtual bus used by Message Sender frames. Its
default is `vehicle`, matching the committed PER network configuration.

The selected paths are saved in `settings.json`. Disconnecting or switching the
source terminates the child emulator process. DAQApp2 launches the simulation
without a time limit; it continues until disconnected, the application closes, or
firmware reaches a genuine terminal failure. The instruction budget is set to the
largest supported value and is therefore not a practical runtime boundary.

Message Sender frames are written to FIL's live control channel and injected at
the next safe shared simulation frontier. CAN 2.0 standard and extended data
frames are supported in both directions. CAN FD and remote-frame injection remain
unsupported and report an explicit send error.
