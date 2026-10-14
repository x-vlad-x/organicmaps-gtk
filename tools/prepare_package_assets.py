#!/usr/bin/env python3
"""Prepare navigation icons from the pinned Organic Maps sources.

Usage: prepare_package_assets.py SOURCE DESTINATION [MAGICK]

Without MAGICK the images are converted with the libwebp tools cwebp and dwebp,
which the GNOME SDK provides for the Flatpak build instead of ImageMagick.
"""
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile

from vector2svg import convert

APP_ID = "io.github.x_vlad_x.OrganicMapsGtk"
ICON_SIZES = (48, 64, 128, 256, 512)


def magick_converter(magick):
    def to_png(source, target, size=None):
        resize = ["-resize", f"{size}x{size}"] if size else []
        subprocess.run([magick, str(source), *resize, str(target)], check=True)
    return to_png


def webp_converter():
    def to_png(source, target, size=None):
        resize = ["-resize", str(size), str(size)] if size else []
        if source.suffix == ".webp":
            subprocess.run(["dwebp", "-quiet", str(source), *resize, "-o", str(target)], check=True)
            return
        with tempfile.TemporaryDirectory() as tmp:
            lossless = Path(tmp) / "image.webp"
            subprocess.run(["cwebp", "-quiet", "-lossless", "-exact", str(source), "-o", str(lossless)], check=True)
            subprocess.run(["dwebp", "-quiet", str(lossless), *resize, "-o", str(target)], check=True)
    return to_png


def main():
    if len(sys.argv) not in (3, 4):
        sys.exit(__doc__.strip())
    source, destination = Path(sys.argv[1]), Path(sys.argv[2])
    to_png = magick_converter(sys.argv[3]) if len(sys.argv) == 4 else webp_converter()
    resource = source / "android/sdk/src/main/res"
    destination.mkdir(parents=True, exist_ok=True)
    raster = resource / "drawable-xxxhdpi"
    for pattern, old, new in (
        ("ic_turn_*.webp", "ic_turn_", "om-turn-"),
        ("ic_exit_highway_to_*.webp", "ic_exit_highway_to_", "om-exit-highway-"),
    ):
        for icon in sorted(raster.glob(pattern)):
            name = icon.stem.replace(old, new, 1).replace("_", "-")
            to_png(icon, destination / f"{name}.png")
    vector = resource / "drawable"
    for icon in sorted(vector.glob("ic_roundabout_exit_*.xml")) + [vector / "ic_turn_round.xml"]:
        name = "om-" + icon.stem.removeprefix("ic_").replace("_", "-")
        convert(str(icon), str(destination / f"{name}.svg"), True)
    app_icon = Path(__file__).resolve().parents[1] / "packaging" / f"{APP_ID}.png"
    for size in ICON_SIZES:
        target = destination / "hicolor" / f"{size}x{size}" / "apps" / f"{APP_ID}.png"
        target.parent.mkdir(parents=True, exist_ok=True)
        if size == 512:
            shutil.copy2(app_icon, target)
        else:
            to_png(app_icon, target, size)
    (destination / ".stamp").touch()


if __name__ == "__main__":
    main()
