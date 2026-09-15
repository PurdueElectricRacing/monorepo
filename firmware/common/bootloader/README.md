# Shared bootloader contract

This component is linked by resident bootloaders and bootloader-aware G4
applications. See the [`source/bootloader` guide](../../source/bootloader/README.md)
for architecture, protocol, flash layout, and recovery.

| File | Responsibility |
| --- | --- |
| [`bootloader_common.h`](bootloader_common.h) | Protocol values, flash addresses, and metadata. |
| [`bootloader_common.c`](bootloader_common.c) | Application START reset callback implementations. |
| [`CMakeLists.txt`](CMakeLists.txt) | Shared interface target. |

Applications use `0x08008000`–`0x0807FFFF` (480 KiB). Metadata is written only
after image validation. START erases the image pages and DATA writes directly to
the slot; metadata stays invalid until CRC and vector checks pass.

An application START callback calls `NVIC_SystemReset()`. After reset, the
resident bootloader advertises READY and waits 500 ms for START.

## Adding a G4 node

1. Add command/data and response JSON configurations.
2. Enable the application command callback.
3. Add board transport settings to `source/bootloader/node_defs.h`.
4. Link `BOOTLOADER_COMMON` and update package/DaqApp board tables.
5. Regenerate CAN artifacts; do not edit generated files.
