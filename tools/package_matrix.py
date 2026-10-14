"""The supported package matrix and canonical GitHub-safe asset names."""
from pathlib import Path

ARCHITECTURES = {"amd64": "x86_64", "arm64": "aarch64"}
TARGETS = tuple(f"{kind}-{arch}" for kind in ("deb", "appimage", "rpm") for arch in ARCHITECTURES)


def validate_elf(path, architecture):
    with path.open("rb") as stream:
        header = stream.read(20)
    if header[:6] != b"\x7fELF\x02\x01" or int.from_bytes(header[18:20], "little") != {
            "amd64": 62, "arm64": 183}[architecture]:
        raise ValueError(f"Wrong package executable architecture: expected {architecture}")


def stage_path(target):
    return Path("out/packages") / target / ("Lichen.AppDir" if target.startswith("appimage-") else "root")


def package_name(target, version):
    kind, arch = target.split("-", 1)
    machine = ARCHITECTURES[arch]
    if kind == "deb":
        return f"lichen_{version}-1_{arch}.deb"
    if kind == "rpm":
        return f"lichen-{version}-1.fc44.{machine}.rpm"
    if kind == "appimage":
        return f"Lichen-{version}-{machine}.AppImage"
    raise ValueError(f"Unknown package format: {kind}")


def srpm_name(version):
    return f"lichen-{version}-1.fc44.src.rpm"
