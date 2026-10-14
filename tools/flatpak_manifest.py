#!/usr/bin/env python3
"""Check the Flatpak manifest and write the variant submitted to Flathub.

  flatpak_manifest.py check
  flatpak_manifest.py flathub [--commit SHA] [--output PATH]

The repository manifest builds the working tree. The Flathub variant builds the
release tag v<VERSION> from Git, pinned to its commit.
"""
import argparse
import copy
import json
from pathlib import Path
import re
import subprocess
import sys
import xml.etree.ElementTree as ET

ROOT = Path(__file__).resolve().parents[1]
APP_ID = "io.github.x_vlad_x.OrganicMapsGtk"
MANIFEST = ROOT / "packaging/flatpak" / f"{APP_ID}.json"
METAINFO = ROOT / "packaging" / f"{APP_ID}.metainfo.xml"
REPOSITORY = "https://github.com/x-vlad-x/organicmaps-gtk.git"
CORE_REPOSITORY = "https://github.com/organicmaps/organicmaps.git"


def load(root=ROOT):
    return json.loads((root / MANIFEST.relative_to(ROOT)).read_text())


def app_module(manifest):
    module = manifest["modules"][-1]
    if module["name"] != "organicmaps-gtk":
        raise ValueError("The application must be the last Flatpak module")
    return module


def core_pin(root=ROOT):
    script = (root / "tools/build-packages.sh").read_text()
    values = dict(re.findall(r"^(CORE_TAG|CORE_COMMIT)=(\S+)$", script, re.MULTILINE))
    return values["CORE_TAG"], values["CORE_COMMIT"]


def core_versions(tag, commit):
    """Version strings tools/unix/version.sh derives from a pinned release tag."""
    match = re.fullmatch(r"((\d{4})\.(\d{2})\.(\d{2})-\d+)-android", tag)
    if not match:
        raise ValueError(f"Unexpected core tag: {tag}")
    day, year, month, date = match.groups()
    return f"{day}-{commit[:8]}-Linux", f"{year[2:]}{month}{date}"


def release_version(version):
    return version.replace("-preview", "~preview")


def check(root=ROOT):
    manifest = load(root)
    if manifest["id"] != APP_ID or manifest["command"] != "organicmaps-gtk":
        raise ValueError("Unexpected Flatpak application ID or command")
    module = app_module(manifest)
    local = [s for s in module["sources"] if s["type"] == "dir"]
    cores = [s for s in module["sources"] if s.get("url") == CORE_REPOSITORY]
    if len(local) != 1 or local[0]["path"] != "../.." or len(cores) != 1:
        raise ValueError("The application module needs the working tree and the core as sources")
    tag, commit = core_pin(root)
    core = cores[0]
    if (core["type"], core.get("tag"), core.get("commit"), core.get("dest")) != ("git", tag, commit, "organicmaps"):
        raise ValueError(f"Flatpak core source must match tools/build-packages.sh: {tag} {commit}")
    qt_version, int_version = core_versions(tag, commit)
    commands = " ".join(module["build-commands"])
    for option in (f"-DOMGTK_CORE_QT_VERSION={qt_version}", f"-DOMGTK_CORE_INT_VERSION={int_version}"):
        if option not in commands.split():
            raise ValueError(f"Flatpak build commands must pass {option}")
    version = (root / "VERSION").read_text().strip()
    releases = ET.parse(root / METAINFO.relative_to(ROOT)).getroot().findall("releases/release")
    if not releases or releases[0].get("version") != release_version(version):
        raise ValueError(f"Add release {release_version(version)} at the top of the metainfo releases")


def flathub(manifest, commit, version):
    if not re.fullmatch(r"[0-9a-f]{40}", commit):
        raise ValueError(f"Expected a full commit hash: {commit}")
    result = copy.deepcopy(manifest)
    sources = app_module(result)["sources"]
    index = next(i for i, s in enumerate(sources) if s["type"] == "dir")
    sources[index] = {"type": "git", "url": REPOSITORY, "tag": f"v{version}", "commit": commit}
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    commands = parser.add_subparsers(dest="command", required=True)
    commands.add_parser("check")
    submit = commands.add_parser("flathub")
    submit.add_argument("--commit", help="commit of the release tag (default: resolve v<VERSION>)")
    submit.add_argument("--output", type=Path, help="write here instead of standard output")
    args = parser.parse_args()
    check()
    if args.command == "flathub":
        version = (ROOT / "VERSION").read_text().strip()
        commit = args.commit or subprocess.check_output(
            ["git", "rev-parse", f"refs/tags/v{version}^{{commit}}"], cwd=ROOT, text=True).strip()
        text = json.dumps(flathub(load(), commit, version), indent=2, ensure_ascii=False) + "\n"
        if args.output:
            args.output.write_text(text)
        else:
            sys.stdout.write(text)


if __name__ == "__main__":
    main()
