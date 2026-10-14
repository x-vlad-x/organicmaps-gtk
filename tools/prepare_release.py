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
from package_matrix import TARGETS, ARCHITECTURES, stage_path, package_name, srpm_name

ROOT = Path(__file__).resolve().parents[1]


def digest(path):
    with path.open("rb") as stream:
        return hashlib.file_digest(stream, "sha256").hexdigest()


def split_source_archive(path, part_size=64 * 1024 * 1024):
    if part_size <= 0:
        raise ValueError("Source part size must be positive")
    parts = []
    with path.open("rb") as stream:
        while content := stream.read(part_size):
            part = path.with_name(path.name + f".part{len(parts) + 1:02}")
            part.write_bytes(content)
            parts.append(part)
    # A smaller regenerated archive must not leave extra numbered parts behind.
    for stale in path.parent.glob(path.name + ".part*"):
        if stale.name.removeprefix(path.name + ".part").isdigit() and stale not in parts:
            stale.unlink()
    return parts


def main():
    version = (ROOT / "VERSION").read_text().strip()
    commit = subprocess.check_output(["git", "rev-parse", "HEAD"], cwd=ROOT, text=True).strip()
    if subprocess.check_output(["git", "status", "--porcelain"], cwd=ROOT):
        raise RuntimeError("Commit the reviewed source changes before preparing release assets")
    destination = ROOT / "out/release" / version
    destination.mkdir(parents=True, exist_ok=True)
    stages = {target: ROOT / stage_path(target) for target in TARGETS}
    for target, stage in stages.items():
        info = json.loads((stage / "usr/libexec/organicmaps-gtk/build-info.json").read_text())
        if (info["project_commit"] != commit or info["project_dirty"] or info["version"] != version
                or info["architecture"] != target.split("-", 1)[1]):
            raise RuntimeError(f"Rebuild packages from the clean release commit: {stage}")

    source_root = ROOT / "out/corresponding-sources"
    available, source_files, source_hashes = set(), {}, {}
    inventories = {}
    for target, stage in stages.items():
        if target.startswith("rpm-"):
            continue  # RPM uses system libraries and supplies a complete rebuildable source RPM.
        inventories[f"compiler-runtime-{target}.json"] = stage / "usr/share/doc/organicmaps-gtk/compiler-runtime/sources.json"
        if target.startswith("appimage-"):
            inventories[f"bundled-packages-{target}.json"] = stage / "usr/share/doc/organicmaps-gtk/bundled/bundled-packages.json"
        directory = source_root / target
        metadata = json.loads((directory / "sources.json").read_text())
        if metadata["errors"]:
            raise RuntimeError(f"Incomplete source index: {directory}")
        for source in metadata["sources"]:
            available.add((source["package"], source["version"]))
            for record in source["files"]:
                path = directory / record["name"]
                if not path.is_file() or path.stat().st_size != record["size"] or digest(path) != record["sha256"]:
                    raise RuntimeError(f"Missing or invalid corresponding source: {path}")
                name = "debian/" + record["name"]
                if name in source_hashes and source_hashes[name] != record["sha256"]:
                    raise RuntimeError(f"Conflicting source files across architectures: {name}")
                source_hashes[name] = record["sha256"]
                source_files[name] = path
            verify_dsc(source, directory)
        for name in ("sources.json", "SHA256SUMS"):
            source_files[f"indexes/{target}/{name}"] = directory / name
    required = {(p["source_package"], p["source_version"]) for inventory in inventories.values()
                for p in json.loads(inventory.read_text())}
    if missing := required - available:
        raise RuntimeError(f"Corresponding sources are missing: {sorted(missing)}")

    runtime = json.loads((ROOT / "packaging/runtime-sources.json").read_text())
    lock = json.loads((ROOT / "packaging/appimage-tools.json").read_text())
    if any(runtime["runtime_commit"] != lock[f"runtime-{machine}"]["source_commit"]
           for machine in ARCHITECTURES.values()):
        raise RuntimeError("AppImage runtime source and binary locks disagree")
    runtime_dir = source_root / "appimage-runtime"
    runtime_dir.mkdir(parents=True, exist_ok=True)
    for record in runtime["files"]:
        fetch_file(record, runtime_dir)
        source_files["appimage-runtime/" + record["name"]] = runtime_dir / record["name"]

    source_archive = destination / f"OrganicMapsGtk-{version}-corresponding-sources.tar"
    with tarfile.open(source_archive, "w") as archive:
        for name, path in sorted(source_files.items()):
            archive.add(path, arcname=name, recursive=False)
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

    source_parts = split_source_archive(source_archive)
    source_guide = destination / "SOURCES.md"
    source_guide.write_text(
        "# Corresponding component sources\n\n"
        f"Download all {len(source_parts)} files named `{source_archive.name}.part*` and "
        "`SHA256SUMS` into one directory. These sources are not needed to install the application.\n\n"
        "Verify, join and extract the files:\n\n```sh\n"
        "sha256sum --check --ignore-missing SHA256SUMS\n"
        f"cat {source_archive.name}.part* > {source_archive.name}\n"
        f"printf '%s  %s\\n' '{digest(source_archive)}' '{source_archive.name}' | sha256sum --check\n"
        f"tar -xf {source_archive.name}\n```\n\n"
        "Keep the numbered suffixes unchanged. The archive includes exact Debian source packages, "
        "patches, `.dsc` descriptors and AppImage runtime sources. Reconstruct a Debian package "
        "with `dpkg-source -x package_version.dsc`. See `LICENSES.md` for build and license details.\n")
    assets = [*source_parts, source_guide, project_archive]
    for name in [*[package_name(target, version) for target in TARGETS], srpm_name(version)]:
        shutil.copy2(ROOT / "out/packages" / name, destination / name)
        assets.append(destination / name)
    for source, target in (("packaging/INSTALL.md", "README.md"),
                           ("packaging/RELEASE-NOTES.md", "RELEASE-NOTES.md"),
                           ("packaging/LICENSES.md", "LICENSES.md")):
        shutil.copy2(ROOT / source, destination / target)
        assets.append(destination / target)
    for name, inventory in inventories.items():
        shutil.copy2(inventory, destination / name)
        assets.append(destination / name)
    files = sorted(assets)
    (destination / "SHA256SUMS").write_text("".join(f"{digest(p)}  {p.name}\n" for p in files))
    print(f"Prepared local release: {destination}")
    print(f"Commit: {commit}; corresponding source packages: {len(required)}")
    print("No tag, push or GitHub release was created.")


if __name__ == "__main__":
    main()
