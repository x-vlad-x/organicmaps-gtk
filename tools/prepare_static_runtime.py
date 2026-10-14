#!/usr/bin/env python3
"""Record the source provenance and notices of the statically linked GCC runtime."""
from pathlib import Path
import json
import re
import shutil
import subprocess
import sys


def prepare(stage, compiler):
    target = stage / "usr/share/doc/organicmaps-gtk/compiler-runtime"
    target.mkdir(parents=True, exist_ok=True)
    records = []
    for option in ("-print-file-name=libstdc++.a", "-print-libgcc-file-name"):
        archive = subprocess.check_output([compiler, option], text=True).strip()
        owner = subprocess.check_output(["dpkg-query", "-S", archive], text=True).split(": ", 1)[0]
        fields = "${binary:Package}\t${Version}\t${source:Package}\t${source:Version}\t${Built-Using}"
        result = subprocess.check_output(["dpkg-query", "-W", "-f=" + fields, owner], text=True)
        package, version, source, source_version, built_using = result.split("\t")
        copyright_file = Path("/usr/share/doc") / package.split(":", 1)[0] / "copyright"
        shutil.copy2(copyright_file, target / (package + ".copyright"))
        records.append(dict(binary_package=package, binary_version=version, source_package=source,
                            source_version=source_version, static_archive=archive))
        for dependency, dependency_version in re.findall(r"([a-z0-9+.-]+) \(= ([^)]+)\)", built_using):
            records.append(dict(source_package=dependency, source_version=dependency_version,
                                built_using_by=package))
    common = target / "common-licenses"
    shutil.copytree("/usr/share/common-licenses", common, dirs_exist_ok=True)
    (target / "sources.json").write_text(json.dumps(records, indent=2) + "\n")


if __name__ == "__main__":
    prepare(Path(sys.argv[1]), sys.argv[2])
