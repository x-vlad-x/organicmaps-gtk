#!/usr/bin/env python3
"""Validate and transfer release inputs between CI jobs; create drafts for tags."""
import argparse
from concurrent.futures import ThreadPoolExecutor
import json
import os
from pathlib import Path
import re
import subprocess
import tarfile

from prepare_release import digest
from package_matrix import (APP_ID, ARCHITECTURES, DOC, PACKAGE, PRIVATE, TARGETS, debian_version,
                            package_name, srpm_name, stage_path, validate_elf)

ROOT = Path(__file__).resolve().parents[1]
STAGES = {target: stage_path(target) for target in TARGETS}


def run(*args):
    return subprocess.check_output(args, cwd=ROOT, text=True).strip()


def check_version(version, ref):
    # Prototype versions identify test builds of a branch; they are never published.
    if not re.fullmatch(r"[0-9]+\.[0-9]+\.[0-9]+(?:-(?:preview|prototype)[1-9][0-9]*)?", version):
        raise ValueError(f"Unsupported release version: {version}")
    if ref.startswith("refs/tags/") and ref != f"refs/tags/v{version}":
        raise ValueError(f"Tag {ref} does not match VERSION {version}")


def check():
    version = (ROOT / "VERSION").read_text().strip()
    check_version(version, os.environ.get("GITHUB_REF", ""))
    notes = (ROOT / "packaging/RELEASE-NOTES.md").read_text()
    if not notes.splitlines()[0].startswith(f"# Lichen {version} "):
        raise ValueError("Update RELEASE-NOTES.md for VERSION before building a release")
    if run("git", "status", "--porcelain"):
        raise ValueError("Build only from a clean source commit")
    return version


def write_tar(destination, paths):
    destination.parent.mkdir(parents=True, exist_ok=True)
    with tarfile.open(destination, "w") as archive:
        for path in sorted(set(paths)):
            archive.add(ROOT / path, arcname=str(path), recursive=False)


def bundle(target):
    version = check()
    stage = STAGES[target]
    info = json.loads((ROOT / stage / PRIVATE / "build-info.json").read_text())
    kind, architecture = target.split("-", 1)
    if (info["project_commit"], info["project_dirty"], info["version"], info["architecture"]) != (
            run("git", "rev-parse", "HEAD"), False, version, architecture):
        raise ValueError("Package provenance does not match this build")
    binary = ROOT / stage / PRIVATE / "bin/lichen"
    validate_elf(binary, architecture)
    metadata = [stage / PRIVATE / "build-info.json"]
    if kind != "rpm":
        metadata.append(stage / DOC / "compiler-runtime/sources.json")
    desktop = ROOT / stage / f"usr/share/applications/{APP_ID}.desktop"
    run("desktop-file-validate", str(desktop))
    for size in (48, 64, 128, 256, 512):
        icon = ROOT / stage / f"usr/share/icons/hicolor/{size}x{size}/apps/{APP_ID}.png"
        if not icon.is_file():
            raise ValueError(f"Missing installed icon: {icon}")
    package = Path("out/packages") / package_name(target, version)
    if kind == "deb":
        fields = run("dpkg-deb", "-f", str(package), "Package", "Architecture", "Version", "Depends",
                     "Conflicts", "Replaces", "Provides")
        if f"Package: {PACKAGE}\n" not in fields + "\n" or f"Version: {debian_version(version)}-1" not in fields or (
                f"Architecture: {architecture}" not in fields or any(name not in fields for name in
                    ("espeak-ng", "hicolor-icon-theme", "libgles2", "libegl1"))):
            raise ValueError("DEB name, version, architecture or required dependencies are incorrect")
        if any(field in fields for field in ("Conflicts:", "Replaces:", "Provides:")):
            raise ValueError("The prototype DEB must install beside the regular Lichen")
    elif kind == "rpm":
        fields = run("podman", "run", "--rm", "--network=none", "--userns=keep-id", "-v", f"{ROOT}:/src:ro",
                     "localhost/om-rpm", "rpm", "-qp", "--qf", "%{ARCH}\\n%{VERSION}\\n%{REQUIRENAME}\\n",
                     "/src/" + str(package))
        if fields.splitlines()[:2] != [ARCHITECTURES[architecture], debian_version(version)]:
            raise ValueError("RPM architecture or version is incorrect")
        dependencies = run("podman", "run", "--rm", "--network=none", "--userns=keep-id", "-v", f"{ROOT}:/src:ro",
                           "localhost/om-rpm", "rpm", "-qp", "--requires", "/src/" + str(package))
        if any(name not in dependencies for name in ("espeak-ng", "hicolor-icon-theme", "libglvnd-gles", "libglvnd-egl")):
            raise ValueError("RPM is missing required speech, icon or graphics dependencies")
    else:
        metadata.append(stage / DOC / "bundled/bundled-packages.json")
    paths = [package, *metadata]
    if kind == "rpm":
        if architecture == "amd64":
            paths.append(Path("out/packages") / srpm_name(version))
    else:
        sources = ROOT / "out/corresponding-sources" / target
        paths.extend(p.relative_to(ROOT) for p in sources.iterdir() if p.is_file())
    write_tar(ROOT / "out/ci" / f"{target}.tar", paths)


def extract(path):
    with tarfile.open(path) as archive:
        members = archive.getmembers()
        # Handoffs contain regular files only, under ignored build output paths.
        for member in members:
            name = Path(member.name)
            if not member.isfile() or name.is_absolute() or ".." in name.parts or name.parts[:1] != ("out",):
                raise ValueError(f"Invalid CI archive member: {member.name}")
        archive.extractall(ROOT, filter="data")


def release_assets(directory):
    assets = []
    for line in (directory / "SHA256SUMS").read_text().splitlines():
        checksum, name = line.split("  ", 1)
        if not re.fullmatch(r"[a-f0-9]{64}", checksum) or Path(name).name != name or name in (".", ".."):
            raise ValueError(f"Invalid release checksum entry: {line}")
        path = directory / name
        if path in assets or not path.is_file() or digest(path) != checksum:
            raise ValueError(f"Invalid or duplicate release asset: {name}")
        assets.append(path)
    if not assets:
        raise ValueError("Release asset list is empty")
    return [*assets, directory / "SHA256SUMS"]


def draft(version):
    if "-prototype" in version:
        raise ValueError("Prototype builds are test artifacts and are never published as releases")
    extract(ROOT / "out/ci/release-files.tar")
    directory = ROOT / "out/release" / version
    assets = release_assets(directory)
    tag = f"v{version}"
    existing = json.loads(run("gh", "release", "list", "--limit", "1000", "--json", "tagName"))
    if any(item["tagName"] == tag for item in existing):
        raise ValueError("A release already exists for this tag; it will not be changed")
    args = ["gh", "release", "create", tag, "--draft", "--verify-tag", "--latest=false",
            "--title", f"Lichen {version}", "--notes-file", "packaging/RELEASE-NOTES.md"]
    if "-preview" in version:
        args.append("--prerelease")
    print(run(*args), flush=True)
    with ThreadPoolExecutor(max_workers=4) as pool:
        list(pool.map(lambda path: run("gh", "release", "upload", tag, str(path)), assets))
    remote = json.loads(run("gh", "release", "view", tag, "--json", "assets"))["assets"]
    expected = {path.name: (path.stat().st_size, "sha256:" + digest(path)) for path in assets}
    actual = {item["name"]: (item["size"], item["digest"]) for item in remote}
    if actual != expected:
        raise ValueError("Uploaded assets failed server size/checksum verification; keep the release as a draft")
    print("Verified draft release. Review and publish it when ready.")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("command", choices=("check", "bundle", "unpack", "verify", "draft"))
    parser.add_argument("target", nargs="?", choices=STAGES)
    args = parser.parse_args()
    version = check()
    if args.command == "bundle":
        if not args.target:
            parser.error("bundle requires a target")
        bundle(args.target)
    elif args.command == "unpack":
        for target in STAGES:
            extract(ROOT / "out/ci" / f"{target}.tar")
    elif args.command == "verify":
        files = release_assets(ROOT / "out/release" / version)
        write_tar(ROOT / "out/ci/release-files.tar", [p.relative_to(ROOT) for p in files])
        print(f"Verified {len(files)} release assets")
    elif args.command == "draft":
        draft(version)


if __name__ == "__main__":
    main()
