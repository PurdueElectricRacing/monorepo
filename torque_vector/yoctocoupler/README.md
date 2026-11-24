# Yoctocoupler

Yoctocoupler is a headless 64-bit Yocto image for the Raspberry Pi Compute
Module 5 (CM5) on the `torque_vector` carrier. It runs Raspberry Pi Linux 6.12
with PREEMPT_RT, 1 kHz timers, and GPIO17 PPS. It uses systemd-networkd and
OpenSSH with public-key-only root access. The image does not include a desktop
or tools such as `rt-tests`, `pps-tools`, or `can-utils`.

| Profile | Target | KAS file |
|---|---|---|
| `production` (default) | CM5 on `torque_vector` | `kas/yoctocoupler-cm5.yml` |
| `qemu` | QEMU `raspi5b` | `kas/yoctocoupler-cm5-qemu.yml` |

Both profiles use the `raspberrypi5` BSP and share settings in
`kas/yoctocoupler-common.yml`. Production includes the MCP251XFD driver; the
QEMU profile adds emulated IGB Ethernet and `vcan0` instead. Keep the artifacts
separate: the WIC filenames follow the same pattern. Flash only the production
image to the CM5.

## Build

Run these commands from `torque_vector/yoctocoupler/`. Docker is required. The
build script downloads and verifies the pinned KAS wrapper on first use.

```sh
# Optional: install this public key for root SSH access at build time.
export YOCTOCOUPLER_SSH_PUBLIC_KEY="$(cat ~/.ssh/id_ed25519.pub)"

scripts/build-image.sh                 # Production
scripts/build-image.sh --profile qemu  # QEMU
```

Production artifacts are written to `build/tmp/deploy/images/raspberrypi5/`;
QEMU artifacts go to `build/qemu/tmp/deploy/images/raspberrypi5/`. The build
directory can be overridden with `KAS_BUILD_DIR`. Without a public key, root
SSH login is unavailable.

### Add an SSH key without rebuilding

Point `IMAGE` at the built or downloaded WIC image:

```sh
IMAGE=/path/to/yoctocoupler-image-raspberrypi5.rootfs-TIMESTAMP.wic.bz2
scripts/add-ssh-key-to-image.sh "$IMAGE" ~/.ssh/id_ed25519.pub
IMAGE="${IMAGE%.wic.bz2}-with-ssh-key.wic.bz2"
```

The helper writes a sibling image and leaves the original untouched. It also
accepts uncompressed `.wic` files. It needs Python 3, `ssh-keygen`, `dd`, and
`debugfs`/`e2fsck`, plus `bzip2` for compressed images. On macOS, install the
filesystem tools with `brew install e2fsprogs`. The modified image no longer
matches the original `.bmap`; the flash helper does not use that map.

## Flash and boot on `torque_vector`

Configure the CM5 bootloader to boot from USB. Run the flash helper on the
host, not in Docker. Set `IMAGE` to the production image you want to flash and
verify the **whole-disk** USB device, not a partition. Unmount its volumes
before flashing.

**Linux:**

```sh
lsblk -p -o NAME,TRAN,RM,SIZE,MODEL
DEVICE=/dev/disk/by-id/usb-REPLACE-ME
scripts/flash-image-to-usb.sh --dry-run "$IMAGE" "$DEVICE"
scripts/flash-image-to-usb.sh "$IMAGE" "$DEVICE"
```

**macOS** (requires Python 3):

```sh
diskutil list external physical
DEVICE=/dev/diskN # Replace with the verified external disk.
diskutil unmountDisk "$DEVICE"
scripts/flash-image-to-usb.sh --dry-run "$IMAGE" "$DEVICE"
scripts/flash-image-to-usb.sh "$IMAGE" "$DEVICE"
```

Flashing erases the target disk. The helper refuses system disks and mounted
volumes, and asks for an exact `ERASE /dev/...` confirmation before writing.
If you reconnect the drive, identify it again and repeat the dry run.

After boot, the carrier's `eth0` uses `192.168.50.2/24`, with gateway
`192.168.50.1` and DNS `1.1.1.1`. For a direct connection, set the host adapter
to `192.168.50.1/24` and run `ssh root@192.168.50.2`. The serial console is
`ttyAMA10`.

### Verify on hardware

```sh
zcat /proc/config.gz | grep -E '^CONFIG_(PREEMPT_RT|HZ_1000|PPS|PPS_CLIENT_GPIO)='
systemctl is-active systemd-networkd
systemctl is-active systemd-resolved
systemctl is-active sshd.socket
ip -4 address show dev eth0
ip route show default
resolvectl status eth0
test -e /sys/class/pps/pps0
```

The production image includes the `mcp251xfd` driver, but does not enable an
SPI CAN overlay for `torque_vector`. The overlay must describe the SPI bus,
chip select, interrupt GPIO, and oscillator before the driver can discover
the controller. Once `can0` is available, configure it for the vehicle CAN bus.

## Run in QEMU

Build the pinned `raspi5b` fork with `scripts/build-rpi5-qemu.sh`. See the
[fork README](tools/rpi5-machine/README.md) for dependencies. On macOS, use
`--install-deps` to install the Homebrew prerequisites. If you already have a
QEMU binary that supports `raspi5b`, pass `--qemu PATH` to the runner.

Build or download the QEMU artifact, then pass the WIC and the `Image` kernel
from the **same build**:

```sh
QEMU_IMAGE=/path/to/qemu-artifact/yoctocoupler-image-raspberrypi5.rootfs-TIMESTAMP.wic.bz2
scripts/run-image-in-qemu.sh --kernel "$(dirname "$QEMU_IMAGE")/Image" "$QEMU_IMAGE"
```

The runner boots the kernel directly and attaches a temporary copy of the WIC
as an SD card. It does not test the CM5 bootloader, carrier wiring, or hardware
timing. Both profiles support SocketCAN raw sockets; the QEMU profile brings
up `vcan0` at boot for application-level CAN tests. Verify it with
`ip -details link show vcan0`. Virtual CAN does not emulate the physical bus
or the MCP251XFD controller.
