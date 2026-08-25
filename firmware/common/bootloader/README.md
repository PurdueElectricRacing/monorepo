# Shared bootloader contract

This component is linked by resident bootloaders and bootloader-aware G4
applications. See the [`source/bootloader` guide](../../source/bootloader/README.md)
for architecture, protocol, flash layout, and recovery.

| File | Responsibility |
| --- | --- |
| [`bootloader_common.h`](bootloader_common.h) | Protocol values, flash addresses, and metadata. |
| [`bootloader_common.c`](bootloader_common.c) | Generated application START reset callbacks. |
| [`CMakeLists.txt`](CMakeLists.txt) | Shared interface target. |

Applications paired with the resident image are linked at `0x08008000`.
Metadata is written only after a staged image is validated and installed. An
application START callback only calls `NVIC_SystemReset()`; after reset the
resident bootloader advertises READY and waits briefly for START on CAN.

## Adding a G4 node

1. Add command/data and response JSON configurations.
2. Enable the application command callback.
3. Add board transport settings to `source/bootloader/node_defs.h`.
4. Link `BOOTLOADER_COMMON` and update package/DaqApp board tables.
5. Regenerate CAN artifacts; do not edit generated files.
