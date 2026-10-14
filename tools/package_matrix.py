"""The supported package matrix, package identity and canonical GitHub-safe asset names."""
from pathlib import Path

# The CoMaps core prototype has its own identity, so it installs beside the regular Lichen
# without sharing a package, command, launcher or data directory with it.
APP_ID = "io.github.x_vlad_x.Lichen.Devel"
PACKAGE = "lichen-devel"
APPIMAGE_NAME = "LichenPreview"
PRIVATE = Path("usr/libexec") / PACKAGE
DOC = Path("usr/share/doc") / PACKAGE

ARCHITECTURES = {"amd64": "x86_64", "arm64": "aarch64"}
TARGETS = tuple(f"{kind}-{arch}" for kind in ("deb", "appimage", "rpm") for arch in ARCHITECTURES)


def validate_elf(path, architecture):
    with path.open("rb") as stream:
        header = stream.read(20)
    if header[:6] != b"\x7fELF\x02\x01" or int.from_bytes(header[18:20], "little") != {
            "amd64": 62, "arm64": 183}[architecture]:
        raise ValueError(f"Wrong package executable architecture: expected {architecture}")


def debian_version(version):
    """Package version that sorts prereleases first: 0.1.0-prototype1 -> 0.1.0~prototype1."""
    return version.replace("-", "~", 1)


def stage_path(target):
    return Path("out/packages") / target / (f"{APPIMAGE_NAME}.AppDir" if target.startswith("appimage-") else "root")


def package_name(target, version):
    kind, arch = target.split("-", 1)
    machine = ARCHITECTURES[arch]
    if kind == "deb":
        return f"{PACKAGE}_{version}-1_{arch}.deb"
    if kind == "rpm":
        return f"{PACKAGE}-{version}-1.fc44.{machine}.rpm"
    if kind == "appimage":
        return f"{APPIMAGE_NAME}-{version}-{machine}.AppImage"
    raise ValueError(f"Unknown package format: {kind}")


def srpm_name(version):
    return f"{PACKAGE}-{version}-1.fc44.src.rpm"
