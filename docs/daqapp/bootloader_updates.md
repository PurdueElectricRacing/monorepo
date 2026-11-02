# DaqApp bootloader updates

This guide covers firmware updates from DaqApp. See the
[resident G4 bootloader guide](../../firmware/source/bootloader/README.md) for
architecture, protocol, flash layout, and recovery.

The updater supports the six STM32G474 VCAN nodes:
`main_module`, `dashboard`, `torque_vector`, `a_box`, `front_driveline`, and
`rear_driveline`.

## Prepare the firmware package

From the repository root, build the firmware and package:

```bash
python3 firmware/build_firmware.py
```

The command writes `firmware/output/manifest.json`,
`firmware/output/firmware_<git-ref>.tar.gz`, and resident images under
`firmware/output/bootloader_<node>/`. Flash the matching
`bootloader_<node>.bin` over SWD before the first application update. The archive
also carries a 16 KiB resident image per board for the in-application bootloader
update described below.

## Update an application

1. Connect DaqApp to VCAN and confirm target telemetry is visible.
2. Open **Bootloader** from the sidebar or command palette.
3. Select `firmware/output/manifest.json` or `firmware_*.tar.gz`.
4. Select targets with recent bootloader telemetry for normal updates, or choose
**Select available**. A nonresponding target can also be selected for single-target
armed recovery.
5. Choose **Review update** and verify the target list. Choose **Start update** for
the normal automatic reset-and-update flow. For manual recovery, choose **Arm
and wait for READY**; DaqApp sends no reset/start frame while armed.
6. Keep power and CAN connected until DaqApp reports **Update complete**; verify telemetry.

Selected boards update sequentially. Front and rear driveline are separate nodes.

## Updater architecture and state machine

![DaqApp bootloader architecture](daqapp_bootloader_architecture.drawio.png)

![DaqApp bootloader updater state machine](daqapp_bootloader_state_machine.drawio.png)

In the normal flow, DaqApp sends `START` to reset a running application into
the resident bootloader. After `READY`, it erases, transfers, validates, and
hands off the image. A target already in the resident bootloader starts directly.

Armed mode requires exactly one selected target and sends no CAN frames while
waiting. After arming, manually reset or power-cycle the target into the
resident bootloader. When its matching `READY` response arrives, DaqApp begins
the update automatically. Normal **Start update** requires recent bootloadable
telemetry for every selected target and supports multi-target sequential
updates. Cancel the armed wait from the Bootloader widget if the target does not
become ready.

See the [resident guide](../../firmware/source/bootloader/README.md) for
protocol and flash details.

## Update a resident bootloader

The same package also carries a 16 KiB resident image for each board, and DaqApp
can select **Resident bootloaders** instead of applications. The running
application handles this command set, so a target is only selectable while its
application telemetry is recent; a board already resident in the bootloader must
be reflashed over SWD instead.

In this mode DaqApp sends the dedicated bootloader update `START`, `DATA`, `CRC`,
and `JUMP` frames directly to the running application. `START` erases the live
16 KiB bootloader region at `0x08000000`, `CRC` validates the received image, and
`JUMP` resets the board so the new resident bootloader runs. There is no
reset-to-bootloader step and the target never sends `READY` in this mode.

There is no staging slot and no rollback: the erase and programming write the
live bootloader region in place. A reset, power loss, or interrupted transfer
midway leaves the resident bootloader invalid and requires reflashing over SWD.
Keep power stable and a known-good resident image available.

**Cancel** stops DaqApp's host state machine but cannot undo target writes. Retry
a cancelled or failed board before vehicle use. The protocol has no
authentication or rollback; use stable power and a trusted bus.
