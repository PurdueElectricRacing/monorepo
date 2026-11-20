#!/usr/bin/env python3
"""Fail-closed macOS disk inspection for flash-image-to-usb.sh.

Uses diskutil's plist output instead of parsing localized human-readable text.
The helper prints tab-separated target details only after validating topology.
"""

import hashlib
import json
import os
import plistlib
import re
import subprocess
import sys


def fail(message):
    raise RuntimeError(message)


def plist(command):
    try:
        raw = subprocess.check_output(command, stderr=subprocess.PIPE)
        return plistlib.loads(raw)
    except (OSError, subprocess.CalledProcessError, plistlib.InvalidFileException, ValueError) as exc:
        fail(f"could not inspect macOS disk topology with {' '.join(command)}: {exc}")


def walk(value):
    if isinstance(value, dict):
        yield value
        for child in value.values():
            yield from walk(child)
    elif isinstance(value, list):
        for child in value:
            yield from walk(child)


def device_id(value):
    return isinstance(value, str) and re.fullmatch(r"disk[0-9]+(?:s[0-9]+)*", value) is not None


def physical_descendants(entry):
    result = set()
    for part in entry.get("Partitions", []):
        part_id = part.get("DeviceIdentifier")
        if device_id(part_id):
            result.add(part_id)
        result.update(physical_descendants(part))
    return result


def clean(value):
    value = re.sub(r"\s+", " ", str(value or "")).strip()
    return value or "-"


def inspect_target(wanted, allow_nonremovable):
    info = plist(["diskutil", "info", "-plist", wanted])
    disk_listing = plist(["diskutil", "list", "-plist"])
    disks = disk_listing.get("AllDisksAndPartitions")
    apfs = plist(["diskutil", "apfs", "list", "-plist"]).get("Containers")
    if not isinstance(disks, list) or not isinstance(apfs, list):
        fail("diskutil returned an incomplete disk/APFS inventory")
    if info.get("DeviceIdentifier") != wanted or info.get("WholeDisk") is not True:
        fail("select a physical whole disk, not a partition or synthesized volume")
    if info.get("Internal") is not False:
        fail("diskutil did not positively identify the target as external; refusing to write")
    if info.get("VirtualOrPhysical") == "Virtual":
        fail("refusing to write to a virtual disk")
    size = info.get("TotalSize")
    if not isinstance(size, int) or size <= 0:
        fail("diskutil did not report a valid target capacity")

    physical = next(
        (
            disk
            for disk in disks
            if disk.get("DeviceIdentifier") == wanted
            and isinstance(disk.get("Partitions"), list)
            and "APFSPhysicalStores" not in disk
        ),
        None,
    )
    if physical is None:
        fail("target is not listed as a physical whole disk")
    bus = info.get("BusProtocol") or info.get("Protocol") or ""
    removable = info.get("RemovableMedia") is True or info.get("Removable") is True
    if not ("USB" in str(bus).upper() or removable) and not allow_nonremovable:
        fail("macOS does not identify the disk as USB/removable; verify it or use --allow-nonremovable")

    owners = {}
    physical_ids = set()
    for disk in disks:
        disk_name = disk.get("DeviceIdentifier")
        if (
            device_id(disk_name)
            and isinstance(disk.get("Partitions"), list)
            and "APFSPhysicalStores" not in disk
        ):
            physical_ids.add(disk_name)
            for part_name in physical_descendants(disk):
                owners[part_name] = disk_name

    def physical_owner(name):
        if name in physical_ids:
            return name
        return owners.get(name)

    containers = {
        container.get("ContainerReference"): container
        for container in apfs
        if isinstance(container, dict) and container.get("ContainerReference")
    }

    def container_stores(container_id):
        container = containers.get(container_id)
        if not container:
            fail(f"could not map APFS container {container_id} to physical storage")
        stores = [
            store.get("DeviceIdentifier")
            for store in container.get("PhysicalStores", [])
            if device_id(store.get("DeviceIdentifier"))
        ]
        if not stores:
            fail(f"APFS container {container_id} has no known physical stores")
        return stores

    # Resolve root, Data, and VM through APFS physical stores. VM is where
    # macOS swapfiles live; checking it also protects external-boot setups.
    system_disks = set()
    for volume_path in ("/", "/System/Volumes/Data", "/System/Volumes/VM"):
        if volume_path != "/" and not os.path.exists(volume_path):
            continue
        volume = plist(["diskutil", "info", "-plist", volume_path])
        container_id = volume.get("APFSContainerReference")
        if container_id:
            stores = container_stores(container_id)
        else:
            stores = [
                store.get("DeviceIdentifier")
                for store in volume.get("APFSPhysicalStores", [])
                if device_id(store.get("DeviceIdentifier"))
            ]
            if not stores:
                parent = volume.get("ParentWholeDisk")
                if not device_id(parent):
                    fail(f"could not establish the system disk backing {volume_path}")
                if parent in containers:
                    stores = container_stores(parent)
                else:
                    stores = [parent]
        for store in stores:
            owner = physical_owner(store)
            if not owner:
                fail(f"could not map system backing store {store} to a physical disk")
            system_disks.add(owner)
    if wanted in system_disks:
        fail("refusing to overwrite a disk backing macOS, its Data volume, or VM/swap")

    all_store_ids = set()
    volume_to_container = {}
    for container in apfs:
        stores = {
            store.get("DeviceIdentifier")
            for store in container.get("PhysicalStores", [])
            if device_id(store.get("DeviceIdentifier"))
        }
        all_store_ids.update(stores)
        for volume in container.get("Volumes", []):
            volume_id = volume.get("DeviceIdentifier")
            if device_id(volume_id):
                volume_to_container[volume_id] = container
    for part in physical.get("Partitions", []):
        content = str(part.get("Content", "")).upper()
        part_name = part.get("DeviceIdentifier")
        if "APFS" in content and part_name not in all_store_ids:
            fail(f"could not resolve APFS partition {part_name} to its container")

    mounted = {
        disk.get("DeviceIdentifier")
        for disk in walk(disk_listing)
        if disk.get("MountPoint") and device_id(disk.get("DeviceIdentifier"))
    }
    try:
        mount_output = subprocess.check_output(["mount"], text=True, stderr=subprocess.PIPE)
    except (OSError, subprocess.CalledProcessError) as exc:
        fail(f"could not enumerate mounted filesystems: {exc}")
    for line in mount_output.splitlines():
        match = re.match(r"^/dev/(?:r)?(disk[0-9]+(?:s[0-9]+)*) on ", line)
        if match:
            mounted.add(match.group(1))

    # Resolve every mounted disk source to its physical backing store. APFS
    # snapshot names extend their volume identifier (diskNsMsK); map by that
    # prefix even if diskutil omits optional MountedSnapshots inventory.
    for mounted_id in mounted:
        stores = None
        direct_owner = physical_owner(mounted_id)
        if direct_owner:
            stores = [mounted_id]
        else:
            for volume_id, container in sorted(
                volume_to_container.items(), key=lambda item: len(item[0]), reverse=True
            ):
                if mounted_id == volume_id or mounted_id.startswith(volume_id + "s"):
                    stores = [
                        store.get("DeviceIdentifier")
                        for store in container.get("PhysicalStores", [])
                        if device_id(store.get("DeviceIdentifier"))
                    ]
                    break
            if stores is None and mounted_id in containers:
                stores = container_stores(mounted_id)
        if stores is None:
            mounted_info = plist(["diskutil", "info", "-plist", mounted_id])
            container_id = mounted_info.get("APFSContainerReference")
            if container_id:
                stores = container_stores(container_id)
            else:
                stores = [
                    store.get("DeviceIdentifier")
                    for store in mounted_info.get("APFSPhysicalStores", [])
                    if device_id(store.get("DeviceIdentifier"))
                ]
                if not stores:
                    parent = mounted_info.get("ParentWholeDisk")
                    if parent in containers:
                        stores = container_stores(parent)
                    elif device_id(parent):
                        stores = [parent]
                    elif device_id(mounted_id):
                        stores = [mounted_id]
        if not stores:
            fail(f"could not resolve mounted device {mounted_id} to physical storage")
        for store in stores:
            owner = physical_owner(store)
            if not owner:
                fail(f"could not map mounted backing store {store} to a physical disk")
            if owner == wanted:
                fail("target backs a mounted filesystem or APFS snapshot; unmount it first")

    identity_values = {
        key: info.get(key)
        for key in (
            "DeviceIdentifier",
            "TotalSize",
            "BusProtocol",
            "Protocol",
            "MediaName",
            "DeviceTreePath",
            "DiskUUID",
            "MediaUUID",
            "SerialNumber",
            "USBSerialNumber",
            "Internal",
            "RemovableMedia",
            "VirtualOrPhysical",
        )
    }
    fingerprint = hashlib.sha256(json.dumps(identity_values, sort_keys=True).encode()).hexdigest()
    raw_node = "/dev/r" + wanted
    print(
        "\t".join(
            (wanted, str(size), fingerprint, clean(info.get("MediaName")), clean(bus), raw_node)
        )
    )


def main():
    if len(sys.argv) != 3:
        fail("usage: macos-disk-safety.py DISK_IDENTIFIER ALLOW_NONREMOVABLE")
    wanted = sys.argv[1]
    if not re.fullmatch(r"disk[0-9]+", wanted):
        fail("invalid macOS whole-disk identifier")
    inspect_target(wanted, sys.argv[2] == "1")


if __name__ == "__main__":
    try:
        main()
    except RuntimeError as exc:
        print(f"error: {exc}", file=sys.stderr)
        raise SystemExit(1)
