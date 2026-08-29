# Shared bootloader contract

This component is linked by resident bootloaders and bootloader-aware G4
applications. See the [`source/bootloader` guide](../../source/bootloader/README.md)
for architecture, protocol, flash layout, and recovery.

| File | Responsibility |
| --- | --- |
| [`bootloader_common.h`](bootloader_common.h) | Protocol values, flash addresses, and metadata. |
| [`bootloader_common.c`](bootloader_common.c) | Application START reset callback implementations. |
| [`CMakeLists.txt`](CMakeLists.txt) | Shared interface target. |

Applications paired with the resident image are linked at `0x08008000` and
may occupy up to 256 KiB, ending at `0x08047FFF`. The remaining 224 KiB from
`0x08048000` through `0x0807FFFF` is reserved. Metadata is written only after
the application-slot image is validated. A successful START erases the
complete flash pages covering the requested image, and DATA is written directly
into that slot while the metadata record remains invalid until CRC and vector
checks succeed.
An application START callback only calls `NVIC_SystemReset()`; after reset the
resident bootloader advertises READY and waits up to 500 ms for START on CAN.

## Adding a G4 node

1. Add command/data and response JSON configurations.
2. Enable the application command callback.
3. Add board transport settings to `source/bootloader/node_defs.h`.
4. Link `BOOTLOADER_COMMON` and update package/DaqApp board tables.
5. Regenerate CAN artifacts; do not edit generated files.
