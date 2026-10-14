#!/usr/bin/env python3
"""Collect dynamically loaded resources from the Debian desktop build environment."""
from pathlib import Path
import json
import shutil
import subprocess
import sys


def multiarch():
    return subprocess.check_output(["dpkg-architecture", "-qDEB_HOST_MULTIARCH"], text=True).strip()


def copy_tree(source, destination):
    if source.exists():
        shutil.copytree(source, destination, dirs_exist_ok=True)


def prepare(stage):
    lib = Path("/usr/lib") / multiarch()
    modules = stage / "usr/lib/gio/modules"
    modules.mkdir(parents=True, exist_ok=True)
    for name in ["libdconfsettings.so", "libgiognutls.so", "libgiolibproxy.so"]:
        source = lib / "gio/modules" / name
        if source.exists():
            shutil.copy2(source, modules / name)
    subprocess.run(["gio-querymodules", modules], check=True)
    for folder in ["tls", "networkinformation"]:
        copy_tree(lib / "qt6/plugins" / folder, stage / "usr/lib/qt6/plugins" / folder)
    copy_tree(lib / "gdk-pixbuf-2.0/2.10.0/loaders", stage / "usr/lib/gdk-pixbuf-2.0/2.10.0/loaders")
    query = lib / "gdk-pixbuf-2.0/gdk-pixbuf-query-loaders"
    shutil.copy2(query, stage / "usr/bin/gdk-pixbuf-query-loaders")
    shutil.copy2("/usr/bin/espeak-ng", stage / "usr/bin/espeak-ng")
    data = lib / "espeak-ng-data"
    if not data.exists():
        data = Path("/usr/share/espeak-ng-data")
    if not data.exists():
        raise RuntimeError("espeak-ng-data is missing")
    copy_tree(data, stage / "usr/share/espeak-ng-data")
    for name in ["Adwaita", "hicolor"]:
        copy_tree(Path("/usr/share/icons") / name, stage / "usr/share/icons" / name)
    copy_tree(Path("/usr/share/glib-2.0/schemas"), stage / "usr/share/glib-2.0/schemas")
    subprocess.run(["glib-compile-schemas", stage / "usr/share/glib-2.0/schemas"], check=True)
    for locale in Path("/usr/share/locale").glob("*/LC_MESSAGES/*.mo"):
        if locale.stem in {"gtk40", "libadwaita", "gdk-pixbuf", "glib20"}:
            target = stage / locale.relative_to("/")
            target.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(locale, target)
    # Qt's TLS backend loads OpenSSL dynamically, outside the ELF dependency graph.
    for name in ["libssl.so.3", "libcrypto.so.3"]:
        shutil.copy2((lib / name).resolve(), stage / "usr/lib" / name)


def licenses(stage):
    # Include plugins and resources as well as top-level libraries. A basename-only
    # search misses modules such as dconf and can attribute files to unrelated -dev packages.
    paths, names = {}, {}
    triplet = multiarch()
    for listing in Path("/var/lib/dpkg/info").glob("*.list"):
        package = listing.name.removesuffix(".list")
        for name in listing.read_text().splitlines():
            paths.setdefault(name, set()).add(package)
            names.setdefault(Path(name).name, set()).add(package)
    owners = {}
    for path in sorted((stage / "usr").rglob("*")):
        relative = path.relative_to(stage).as_posix()
        if not path.is_file() or relative.startswith(("usr/libexec/", "usr/share/doc/")):
            continue
        with path.open("rb") as stream:
            elf = stream.read(4) == b"\x7fELF"
        original = "/" + relative
        candidates = paths.get(original) or paths.get(original.replace("/usr/lib/", f"/usr/lib/{triplet}/", 1))
        if not candidates and elf:
            candidates = names.get(path.name)
        if not candidates and elf:
            raise RuntimeError(f"Bundled ELF has no Debian source provenance: {relative}")
        for package in candidates or ():
            owners.setdefault(package, []).append(relative)
    target = stage / "usr/share/doc/lichen/bundled"
    target.mkdir(parents=True, exist_ok=True)
    inventory = []
    metadata = []
    for package, files in sorted(owners.items()):
        name = package.split(":", 1)[0]
        source = Path("/usr/share/doc") / name / "copyright"
        if not source.exists():
            raise RuntimeError(f"Bundled package is missing its copyright file: {package}")
        shutil.copy2(source, target / (package + ".copyright"))
        fields = "${binary:Package}\t${Version}\t${source:Package}\t${source:Version}"
        result = subprocess.run(["dpkg-query", "-W", "-f=" + fields, package], check=True,
                                capture_output=True, text=True)
        binary, version, source_name, source_version = result.stdout.split("\t")
        inventory.append(f"{binary}\t{version}")
        metadata.append(dict(binary_package=binary, binary_version=version,
                             source_package=source_name, source_version=source_version, files=files))
    copy_tree(Path("/usr/share/common-licenses"), target / "common-licenses")
    (target / "packages.txt").write_text("\n".join(inventory) + "\n")
    (target / "bundled-packages.json").write_text(json.dumps(metadata, indent=2) + "\n")


if __name__ == "__main__":
    (licenses if "--licenses" in sys.argv else prepare)(Path(sys.argv[1]))
