#!/usr/bin/env python3
"""Create the contiguous binary consumed by the G4 bootloader.

Intel HEX stores absolute addresses. A bootloader-layout application begins at
0x08008000, so using ``objcopy -O binary`` would preserve a large empty prefix
before the first application byte. This utility reconstructs the loadable
address range, fills holes with erased-flash bytes, and pads the result to the
32-bit word size used by the CAN protocol.
"""

from pathlib import Path
import sys


def convert(source: Path, expected_start: int | None = None) -> bytes:
    """Return loadable HEX bytes with address gaps represented as ``0xFF``.

    The parser accepts extended-linear-address records (type 04) and data
    records (type 00). Record checksums and lengths are validated before any
    bytes are returned. ``expected_start`` is used by the build to catch an
    application accidentally linked at the standalone address.
    """
    memory: dict[int, int] = {}
    extended_address = 0
    for line_number, line in enumerate(source.read_text(encoding="ascii").splitlines(), 1):
        if not line:
            continue
        if not line.startswith(":"):
            raise ValueError(f"{source}:{line_number}: missing ':'")
        record = bytes.fromhex(line[1:])
        if len(record) < 5 or (sum(record) & 0xFF) != 0:
            raise ValueError(f"{source}:{line_number}: invalid record")
        length = record[0]
        if len(record) != length + 5:
            raise ValueError(f"{source}:{line_number}: invalid record length")
        address = (record[1] << 8) | record[2]
        record_type = record[3]
        payload = record[4:4 + length]
        if record_type == 0x04:
            if length != 2:
                raise ValueError(f"{source}:{line_number}: invalid extended address")
            extended_address = int.from_bytes(payload, "big") << 16
        elif record_type == 0x00:
            absolute_address = extended_address + address
            for index, value in enumerate(payload):
                memory[absolute_address + index] = value

    if not memory:
        raise ValueError(f"{source}: no data records")
    first = min(memory)
    if expected_start is not None and first != expected_start:
        raise ValueError(
            f"{source}: starts at 0x{first:08X}, expected 0x{expected_start:08X}"
        )
    last = max(memory)
    return bytes(memory.get(address, 0xFF) for address in range(first, last + 1))


def main() -> int:
    """Run the command-line conversion and report malformed input cleanly."""
    if len(sys.argv) not in (3, 4):
        print(f"usage: {sys.argv[0]} INPUT.hex OUTPUT.bin [EXPECTED_START]", file=sys.stderr)
        return 2
    expected = int(sys.argv[3], 0) if len(sys.argv) == 4 else None
    try:
        data = convert(Path(sys.argv[1]), expected)
        # The target receives uint32 words, so erased-state padding is part of
        # the image and must be included in the CRC manifest.
        Path(sys.argv[2]).write_bytes(data + b"\xFF" * ((4 - len(data) % 4) % 4))
    except (OSError, ValueError) as error:
        print(error, file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
