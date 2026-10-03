#!/usr/bin/env python3
"""Guard ownership boundaries; run from any directory."""
from pathlib import Path
import re
import subprocess

root = Path(__file__).resolve().parent
core = root / "daqcore/src"
for path in core.rglob("*.rs"):
    text = path.read_text()
    assert not re.search(r"\b(?:egui|eframe|Arc|Mutex|RwLock|Condvar|Atomic\w*)\b", text), path
    if "slcan" in text:
        assert path == core / "can/driver.rs", path
        adapter = text.index("mod serial {")
        assert "slcan" not in text[:adapter], path
    if path.name in ("session.rs", "cache.rs", "timeline.rs"):
        assert not re.search(r"\b(?:thread|mpsc|Sender|Receiver)\s*(?:::|<)", text), path
        assert "max_retention" not in text and "CAPACITY" not in text, path

tree = subprocess.check_output([
    "cargo", "tree", "--manifest-path", str(root / "Cargo.toml"),
    "-p", "daqcore", "--no-default-features", "--locked", "--offline", "--edges", "normal"
], text=True)
assert not re.search(r"\b(?:egui|eframe|slcan|serialport) v", tree), tree
print("Core ownership and dependency boundaries pass.")
