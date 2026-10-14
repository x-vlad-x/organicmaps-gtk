#!/usr/bin/env python3
"""Record RPM build provenance from the included source archive manifest."""
import json
from pathlib import Path
import platform
import sys

from package_matrix import PRIVATE
import core_source

root = Path(__file__).resolve().parents[1]
metadata = json.loads((root / "SOURCE-REVISION.json").read_text())
info = dict(version=(root / "VERSION").read_text().strip(),
            architecture={"x86_64": "amd64", "aarch64": "arm64"}[platform.machine()],
            project_commit=metadata["commit"], project_dirty=metadata["dirty"],
            core=core_source.pinned()["CORE_NAME"], core_commit=metadata["core_commit"], distribution="Fedora 44",
            compiler_runtime="system libraries; no bundled GCC runtime")
target = Path(sys.argv[1]) / PRIVATE / "build-info.json"
target.write_text(json.dumps(info, indent=2) + "\n")
