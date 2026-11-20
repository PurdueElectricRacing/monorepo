#!/usr/bin/env python3
"""Small, explicit helpers for editing this project's WIC images."""

from pathlib import Path
import os
import struct
import sys


def fail(message):
    raise RuntimeError(message)


def rootfs_partition(image_path):
    with open(image_path, "rb") as image:
        mbr = image.read(512)
    if len(mbr) != 512 or mbr[510:512] != b"\x55\xaa":
        fail("input is not a DOS/MBR disk image")
    entry = mbr[446 + 16 : 446 + 32]  # partition 2
    _, _, partition_type, _, start, sectors = struct.unpack("<B3sB3sII", entry)
    if partition_type != 0x83 or start == 0 or sectors == 0:
        fail("expected an ext4/Linux rootfs in MBR partition 2")
    if (start + sectors) * 512 > os.path.getsize(image_path):
        fail("rootfs partition extends beyond the image")
    print(start, sectors)


def merge_keys(existing_path, new_path, output_path):
    old = Path(existing_path).read_bytes()
    seen = {line.rstrip(b"\r") for line in old.splitlines()}
    new_lines = []
    for line in Path(new_path).read_bytes().splitlines():
        line = line.rstrip(b"\r")
        if not line or line.lstrip().startswith(b"#"):
            continue
        if line not in seen:
            seen.add(line)
            new_lines.append(line)
    if new_lines and old and not old.endswith(b"\n"):
        old += b"\n"
    Path(output_path).write_bytes(old + b"".join(line + b"\n" for line in new_lines))
    print(len(new_lines))


def main():
    if len(sys.argv) == 3 and sys.argv[1] == "rootfs-partition":
        rootfs_partition(sys.argv[2])
    elif len(sys.argv) == 5 and sys.argv[1] == "merge-keys":
        merge_keys(sys.argv[2], sys.argv[3], sys.argv[4])
    else:
        fail(
            "usage: wic-image-tools.py rootfs-partition IMAGE | "
            "merge-keys EXISTING_KEYS NEW_KEYS OUTPUT_KEYS"
        )


if __name__ == "__main__":
    try:
        main()
    except (OSError, RuntimeError) as exc:
        print(f"error: {exc}", file=sys.stderr)
        raise SystemExit(1)
