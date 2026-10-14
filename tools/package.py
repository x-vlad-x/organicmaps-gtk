#!/usr/bin/env python3
"""Build local preview packages from a CMake build. Never installs on the host.

  python3 tools/package.py deb --build-dir build-aarch64 --container localhost/om-cross
  python3 tools/package.py appimage --build-dir build-appimage --container localhost/om-desktop
"""
import argparse
import hashlib
import json
from pathlib import Path
import re
import shutil
import struct
import subprocess
import urllib.request

ROOT = Path(__file__).resolve().parents[1]
APP_ID = "io.github.x_vlad_x.OrganicMapsGtk"
PRIVATE = Path("usr/libexec/organicmaps-gtk")


def run(command, **kwargs):
    return subprocess.run([str(x) for x in command], check=True, text=True, **kwargs)


def container_path(path):
    return Path("/src") / Path(path).resolve().relative_to(ROOT)


def execute(args, command, *, cwd=ROOT, capture=False):
    if args.container:
        command = ["podman", "run", "--rm", "--network=none", "--userns=keep-id",
                   "-v", f"{ROOT}:/src:z", "-w", container_path(cwd), args.container, *command]
    return run(command, cwd=cwd, capture_output=capture)


def inside(args, path):
    return container_path(path) if args.container else path


def project_state():
    result = subprocess.run(["git", "rev-parse", "--show-toplevel"], cwd=ROOT,
                            text=True, capture_output=True)
    if result.returncode == 0 and Path(result.stdout.strip()).resolve() == ROOT:
        commit = run(["git", "rev-parse", "HEAD"], cwd=ROOT, capture_output=True).stdout.strip()
        dirty = bool(run(["git", "status", "--porcelain"], cwd=ROOT, capture_output=True).stdout)
        return commit, dirty
    # Release source archives do not contain .git. Verify their file manifest
    # instead of inventing a revision or borrowing an enclosing repository's HEAD.
    metadata = json.loads((ROOT / "SOURCE-REVISION.json").read_text())
    for name, expected in metadata["files"].items():
        path = (ROOT / name).resolve()
        if not path.is_relative_to(ROOT):
            raise RuntimeError(f"Unsafe source manifest entry: {name}")
        if not path.is_file() or hashlib.sha256(path.read_bytes()).hexdigest() != expected:
            return metadata["commit"], True
    return metadata["commit"], False


def download_tools(architecture="amd64"):
    directory = ROOT / "out/tools"
    directory.mkdir(parents=True, exist_ok=True)
    tools = json.loads((ROOT / "packaging/appimage-tools.json").read_text())
    machine = {"amd64": "x86_64", "arm64": "aarch64"}[architecture]
    for name, source in tools.items():
        if machine not in name:
            continue
        path = directory / name
        if not path.exists():
            temporary = path.with_suffix(path.suffix + ".download")
            urllib.request.urlretrieve(source["url"], temporary)
            temporary.replace(path)
        if hashlib.sha256(path.read_bytes()).hexdigest() != source["sha256"]:
            raise RuntimeError(f"Checksum mismatch for {path}. Review the pinned tool source before updating it.")
        path.chmod(0o755)
    return directory


def prepare_stage(args):
    binary = args.build_dir / "organicmaps-gtk"
    machine = struct.unpack_from("<H", binary.read_bytes(), 18)[0]
    architecture = {62: "amd64", 183: "arm64"}.get(machine)
    if not architecture:
        raise RuntimeError(f"Unsupported executable architecture: {machine}")
    workspace = ROOT / "out/packages" / f"{args.format}-{architecture}"
    stage = workspace / ("OrganicMapsGtk.AppDir" if args.format == "appimage" else "root")
    if stage.exists():
        shutil.rmtree(stage)
    stage.mkdir(parents=True)
    execute(args, ["env", f"DESTDIR={inside(args, stage)}", "cmake", "--install",
                   inside(args, args.build_dir), "--component", "OrganicMapsGtk", "--prefix", "/usr", "--strip"])
    version = (ROOT / "VERSION").read_text().strip()
    commit, dirty = project_state()
    info = {
        "version": version, "architecture": architecture,
        "project_commit": commit, "project_dirty": dirty,
        "core_commit": run(["git", "rev-parse", "HEAD"], cwd=ROOT / "organicmaps", capture_output=True).stdout.strip(),
        "container": args.container, "status": "local preview; not a public release",
    }
    (stage / PRIVATE / "build-info.json").write_text(json.dumps(info, indent=2) + "\n")
    compiler_files = sorted((args.build_dir / "CMakeFiles").glob("*/CMakeCXXCompiler.cmake"))
    compilers = {match.group(1) for path in compiler_files
                 if (match := re.search(r'^set\(CMAKE_CXX_COMPILER "([^"]+)"\)', path.read_text(), re.M))}
    if len(compilers) != 1:
        raise RuntimeError(f"Cannot identify the build's C++ compiler: {compilers}")
    compiler = compilers.pop()
    execute(args, ["python3", inside(args, ROOT / "tools/prepare_static_runtime.py"),
                   inside(args, stage), compiler])
    return stage, architecture, version


def build_deb(args, stage, architecture, version):
    workspace = stage.parent
    maintainer = "Vladislav Shadiuk <6992531+x-vlad-x@users.noreply.github.com>"
    debian = workspace / "debian"
    debian.mkdir(exist_ok=True)
    (debian / "control").write_text(
        f"Source: organicmaps-gtk\nMaintainer: {maintainer}\n\n"
        "Package: organicmaps-gtk\nArchitecture: any\nDescription: Unofficial GTK offline maps\n")
    dependencies = execute(args, ["dpkg-shlibdeps", "-O", "-e" + str(inside(args, stage / PRIVATE / "bin/organicmaps-gtk"))],
                           cwd=workspace, capture=True).stdout.strip()
    dependencies = next(line.split("=", 1)[1] for line in dependencies.splitlines() if line.startswith("shlibs:Depends="))
    version = version.replace("-preview", "~preview") + "-1"
    installed_size = (sum(p.stat().st_size for p in stage.rglob("*") if p.is_file()) + 1023) // 1024
    control = stage / "DEBIAN"
    control.mkdir()
    (control / "control").write_text(
        f"Package: organicmaps-gtk\nVersion: {version}\nArchitecture: {architecture}\n"
        f"Maintainer: {maintainer}\nSection: education\nPriority: optional\n"
        f"Installed-Size: {installed_size}\nDepends: {dependencies}, espeak-ng, hicolor-icon-theme\n"
        "Recommends: geoclue-2.0, iio-sensor-proxy\n"
        "Homepage: https://github.com/x-vlad-x/organicmaps-gtk\n"
        "Description: Unofficial GTK interface for offline maps and navigation\n"
        " Offline maps for Linux phones and desktops. Based on the Organic Maps Project.\n"
        " Includes offline search, routing, bookmarks and downloadable maps.\n")
    copyright_file = stage / "usr/share/doc/organicmaps-gtk/copyright"
    copyright_file.write_text(
        "Organic Maps (GTK) — unofficial interface\n"
        "Copyright 2026 Vladislav Shadiuk. Code: Apache-2.0; see LICENSE and NOTICE.\n"
        "Organic Maps source, resources and map data have additional notices and licenses.\n"
        "See upstream/NOTICE, upstream/DATA_LICENSE.txt, upstream/dep5 and upstream/copyright.html.\n"
        "Full third-party license texts are preserved under upstream/.\n")
    # Keep the Debian ordering marker in control, but use a GitHub-safe asset filename.
    filename_version = version.replace("~", "-")
    output = ROOT / "out/packages" / f"organicmaps-gtk_{filename_version}_{architecture}.deb"
    execute(args, ["dpkg-deb", "--root-owner-group", "--build", inside(args, stage), inside(args, output)])
    execute(args, ["dpkg-deb", "--info", inside(args, output)])
    return output


def appimage_resources(args, stage):
    # Read these paths in the build container, not from the development host.
    helper = ROOT / "tools/prepare_appimage.py"
    execute(args, ["python3", inside(args, helper), inside(args, stage)])


def build_appimage(args, stage, architecture, version):
    machine = {"amd64": "x86_64", "arm64": "aarch64"}[architecture]
    tools = download_tools(architecture)
    shutil.copytree(ROOT / "packaging/licenses/appimage-runtime",
                    stage / "usr/share/doc/organicmaps-gtk/appimage-runtime", dirs_exist_ok=True)
    appimage_resources(args, stage)
    desktop = stage / f"usr/share/applications/{APP_ID}.desktop"
    execute(args, ["desktop-file-validate", inside(args, desktop)])
    execute(args, [inside(args, tools / f"linuxdeploy-{machine}.AppImage"), "--appimage-extract-and-run",
                   "--appdir", inside(args, stage), "--deploy-deps-only", inside(args, stage / "usr"),
                   "--deploy-deps-only", inside(args, stage / PRIVATE / "bin/organicmaps-gtk"),
                   "--desktop-file", inside(args, desktop), "--icon-file",
                   inside(args, stage / f"usr/share/icons/hicolor/512x512/apps/{APP_ID}.png"),
                   "--custom-apprun", inside(args, ROOT / "packaging/AppRun")])
    for library in ["libgtk-4.so.1", "libadwaita-1.so.0", "libQt6Core.so.6", "libQt6Network.so.6", "libgeoclue-2.so.0"]:
        if not (stage / "usr/lib" / library).is_file():
            raise RuntimeError(f"AppImage is missing a required application library: {library}")
    # Include distro license files and an inventory for bundled dynamic libraries.
    execute(args, ["python3", inside(args, ROOT / "tools/prepare_appimage.py"), inside(args, stage), "--licenses"])
    output = ROOT / "out/packages" / f"OrganicMapsGtk-{version}-{machine}.AppImage"
    execute(args, ["env", f"ARCH={machine}", inside(args, tools / f"appimagetool-{machine}.AppImage"),
                   "--appimage-extract-and-run", "--no-appstream", "--runtime-file", inside(args, tools / f"runtime-{machine}"),
                   inside(args, stage), inside(args, output)])
    return output


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("format", choices=["deb", "appimage"])
    parser.add_argument("--build-dir", type=Path, required=True)
    parser.add_argument("--container", help="Podman image in which the CMake build was configured")
    args = parser.parse_args()
    args.build_dir = args.build_dir.resolve()
    stage, architecture, version = prepare_stage(args)
    output = (build_deb if args.format == "deb" else build_appimage)(args, stage, architecture, version)
    checksum = hashlib.sha256(output.read_bytes()).hexdigest()
    output.with_name(output.name + ".sha256").write_text(f"{checksum}  {output.name}\n")
    shutil.copy2(ROOT / "packaging/INSTALL.md", output.parent / "README.md")
    print(f"\nCreated local preview: {output}")


if __name__ == "__main__":
    main()
