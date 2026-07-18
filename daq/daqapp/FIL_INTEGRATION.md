# FIL real-time integration

DAQApp2 can use a running FIL virtual CAN network as a live CAN source.
The application launches FIL itself, keeps the simulation paced to wall time, and
feeds transmitted CAN 2.0 frames into the existing DBC decoder and widgets.

## Setup

1. Build FIL, including the `watch-network` support:

   ```bash
   cmake -S /path/to/fil-combined -B /path/to/fil-combined/build -G Ninja
   cmake --build /path/to/fil-combined/build
   ```

2. Start DAQApp2 with `cargo run`.
3. Choose **Add FIL Control** in the sidebar or **Spawn FIL Control** in the
   command palette.
4. In the dockable FIL pane, select the built `fil` binary and a network JSON
   configuration, then choose **Connect / Restart**.
5. Select the matching DBC file to decode the emulated traffic.

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

## Live ADC inputs

Use **ADC injection** in the FIL pane, then enter a board name, ADC instance,
channel, and raw 12-bit value. **Inject** applies the value at FIL's next safe
shared simulation frontier. Valid instances are `ADC1` through `ADC4`, channels
are 0 through 19, and values are 0 through 4095. For example, dashboard `ADC1`,
channel `3`, value `2048` sets that input to mid-scale without restarting the
simulation.

## Live GPIO

The FIL pane shows all 16 pins for a selected board and GPIO port. Firmware output
changes update the **Firmware output** column in real time. **Low**, **High**, and
**Release** drive or release the external input override for an individual pin;
the input and output states are intentionally displayed separately.

CAN frame injection remains in DAQApp2's existing Message Sender window and is
not duplicated in the FIL pane.
