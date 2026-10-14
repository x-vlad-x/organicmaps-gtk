#!/usr/bin/env python3
"""Prepare local release assets from a clean source commit. Never pushes or publishes."""
import hashlib
import io
import json
from pathlib import Path
import shutil
import subprocess
import tarfile

from collect_debian_sources import fetch_file, verify_dsc

ROOT = Path(__file__).resolve().parents[1]


def digest(path):
    with path.open("rb") as stream:
        return hashlib.file_digest(stream, "sha256").hexdigest()


def main():
    version = (ROOT / "VERSION").read_text().strip()
    commit = subprocess.check_output(["git", "rev-parse", "HEAD"], cwd=ROOT, text=True).strip()
    if subprocess.check_output(["git", "status", "--porcelain"], cwd=ROOT):
        raise RuntimeError("Commit the reviewed source changes before preparing release assets")
    destination = ROOT / "out/release" / version
    destination.mkdir(parents=True, exist_ok=True)
    appdir = ROOT / "out/packages/appimage-amd64/OrganicMapsGtk.AppDir"
    debdir = ROOT / "out/packages/deb-arm64/root"
    for stage in (appdir, debdir):
        info = json.loads((stage / "usr/libexec/organicmaps-gtk/build-info.json").read_text())
        if info["project_commit"] != commit or info["project_dirty"] or info["version"] != version:
            raise RuntimeError(f"Rebuild packages from the clean release commit: {stage}")

    source_root = ROOT / "out/corresponding-sources"
    available = set()
    source_files = []
    for name in ("debian", "compiler-arm64"):
        directory = source_root / name
        metadata = json.loads((directory / "sources.json").read_text())
        if metadata["errors"]:
            raise RuntimeError(f"Incomplete source index: {directory}")
        files = {}
        for source in metadata["sources"]:
            available.add((source["package"], source["version"]))
            for record in source["files"]:
                path = directory / record["name"]
                if not path.is_file() or path.stat().st_size != record["size"] or digest(path) != record["sha256"]:
                    raise RuntimeError(f"Missing or invalid corresponding source: {path}")
                files[path] = source_root / name / record["name"]
            verify_dsc(source, directory)
        source_files += list(files)
        source_files += [directory / "sources.json", directory / "SHA256SUMS"]

    inventories = [appdir / "usr/share/doc/organicmaps-gtk/bundled/bundled-packages.json"]
    inventories += [p / "usr/share/doc/organicmaps-gtk/compiler-runtime/sources.json" for p in (appdir, debdir)]
    required = {(p["source_package"], p["source_version"]) for inventory in inventories
                for p in json.loads(inventory.read_text())}
    if missing := required - available:
        raise RuntimeError(f"Corresponding sources are missing: {sorted(missing)}")

    runtime = json.loads((ROOT / "packaging/runtime-sources.json").read_text())
    lock = json.loads((ROOT / "packaging/appimage-tools.json").read_text())
    if runtime["runtime_commit"] != lock["runtime-x86_64"]["source_commit"]:
        raise RuntimeError("AppImage runtime source and binary locks disagree")
    runtime_dir = source_root / "appimage-runtime"
    runtime_dir.mkdir(parents=True, exist_ok=True)
    for record in runtime["files"]:
        fetch_file(record, runtime_dir)
        source_files.append(runtime_dir / record["name"])

    source_archive = destination / f"OrganicMapsGtk-{version}-corresponding-sources.tar"
    with tarfile.open(source_archive, "w") as archive:
        for path in sorted(set(source_files)):
            archive.add(path, arcname=path.relative_to(source_root), recursive=False)
        archive.add(ROOT / "packaging/LICENSES.md", arcname="README.md")
        archive.add(ROOT / "packaging/runtime-sources.json", arcname="runtime-sources.json")

    names = subprocess.check_output(["git", "ls-files", "-z"], cwd=ROOT).decode().split("\0")
    names = sorted(filter(None, names))
    if any(".local." in name or name.startswith(("out/", "organicmaps/")) for name in names):
        raise RuntimeError("Local planning files or build output must not enter release source archives")
    manifest = {name: digest(ROOT / name) for name in names}
    project_archive = destination / f"OrganicMapsGtk-{version}-sources.tar.gz"
    with tarfile.open(project_archive, "w:gz") as archive:
        for name in names:
            archive.add(ROOT / name, arcname=name, recursive=False)
        content = (json.dumps(dict(commit=commit, files=manifest), indent=2) + "\n").encode()
        member = tarfile.TarInfo("SOURCE-REVISION.json")
        member.size = len(content)
        member.mode = 0o644
        archive.addfile(member, io.BytesIO(content))

    assets = [source_archive, project_archive]
    for name in (f"organicmaps-gtk_{version.replace('-preview', '~preview')}-1_arm64.deb",
                 f"OrganicMapsGtk-{version}-x86_64.AppImage"):
        shutil.copy2(ROOT / "out/packages" / name, destination / name)
        assets.append(destination / name)
    for source, target in (("packaging/INSTALL.md", "README.md"),
                           ("packaging/RELEASE-NOTES.md", "RELEASE-NOTES.md"),
                           ("packaging/LICENSES.md", "LICENSES.md")):
        shutil.copy2(ROOT / source, destination / target)
        assets.append(destination / target)
    for inventory in inventories:
        name = "bundled-packages.json" if "bundled" in inventory.parts else (
            "compiler-runtime-amd64.json" if inventory.is_relative_to(appdir) else "compiler-runtime-arm64.json")
        shutil.copy2(inventory, destination / name)
        assets.append(destination / name)
    files = sorted(assets)
    (destination / "SHA256SUMS").write_text("".join(f"{digest(p)}  {p.name}\n" for p in files))
    print(f"Prepared local release: {destination}")
    print(f"Commit: {commit}; corresponding source packages: {len(required)}")
    print("No tag, push or GitHub release was created.")


if __name__ == "__main__":
    main()
