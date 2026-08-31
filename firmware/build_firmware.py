#!/usr/bin/env python3
"""Build and package all PER firmware targets."""

from pathlib import Path
import subprocess
import tarfile
import zlib


ROOT = Path(__file__).resolve().parent
BUILD_DIR = ROOT / "build"
OUTPUT_DIR = ROOT / "output"
CAN_GENERATED_DIR = ROOT / "can_library" / "generated"


def firmware_images() -> list[Path]:
    """Return generated production images in a stable order."""
    return sorted(
        image
        for image in OUTPUT_DIR.glob("*/*.hex")
        if not image.stem.endswith("_testing")
    )


def build() -> None:
    """Clean, build, and package every firmware target."""
    subprocess.run(
        [
            "cmake",
            "-E",
            "rm",
            "-rf",
            str(BUILD_DIR),
            str(OUTPUT_DIR),
            str(CAN_GENERATED_DIR),
        ],
        cwd=ROOT,
        check=True,
    )
    subprocess.run(
        [
            "cmake",
            "-S",
            str(ROOT),
            "-B",
            str(BUILD_DIR),
            "-G",
            "Ninja",
            "-DBOOTLOADER_BUILD=OFF",
        ],
        cwd=ROOT,
        check=True,
    )
    subprocess.run(
        ["ninja", "-C", str(BUILD_DIR), "all"],
        cwd=ROOT,
        check=True,
    )
    images = firmware_images()
    for hex_path in images:
        checksum = f"{zlib.crc32(hex_path.read_bytes()) & 0xFFFFFFFF:08X}"
        hex_path.with_suffix(".crc").write_text(f"{checksum}\n", encoding="utf-8")

    try:
        git_ref = subprocess.run(
            ["git", "describe", "--tags", "--exact-match"],
            cwd=ROOT,
            check=True,
            capture_output=True,
            text=True,
        ).stdout.strip()
    except subprocess.CalledProcessError:
        git_ref = subprocess.run(
            ["git", "rev-parse", "--short", "HEAD"],
            cwd=ROOT,
            check=True,
            capture_output=True,
            text=True,
        ).stdout.strip()

    git_ref = git_ref.replace("/", "-")
    tarball = OUTPUT_DIR / f"firmware_{git_ref}.tar.gz"
    with tarfile.open(tarball, "w:gz") as archive:
        for hex_path in images:
            for artifact in (hex_path, hex_path.with_suffix(".crc")):
                archive.add(artifact, arcname=artifact.name)


build()
