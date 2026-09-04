"""
artifacts.py

Author: Irving Wang (irvingw@purdue.edu)
"""

from dataclasses import dataclass
from pathlib import Path
from typing import Mapping


@dataclass
class Artifact:
    collection: str
    relative_path: str
    content: str


def write_artifacts(roots: Mapping[str, Path], artifacts: list[Artifact]) -> None:
    for root in roots.values():
        root.mkdir(parents=True, exist_ok=True)

    for artifact in artifacts:
        path = roots[artifact.collection] / artifact.relative_path
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(artifact.content, encoding="utf-8", newline="\n")


def clear_artifacts(roots: Mapping[str, Path], patterns: Mapping[str, str]) -> None:
    for collection, root in roots.items():
        if not root.exists():
            continue
        for path in root.glob(patterns[collection]):
            if path.is_file():
                path.unlink()
