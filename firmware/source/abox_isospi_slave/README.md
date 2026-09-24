# abox isoSPI slave

See [the pair's bench guide](../abox_isospi_target/README.md) for hardware wiring,
build/flash commands, protocol tests, fake measurements, and debugger checks.

`protocol/emulator.c` is independent of the STM32 transport. `main.c` implements
SPI1 slave service with PB0 software chip select, interrupt-driven FIFO handling,
and a PB5 heartbeat. `g_emulator.stats` exposes protocol and peripheral errors.
