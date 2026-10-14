#!/usr/bin/env python3
"""Prepare navigation icons from the pinned core sources and the application icons."""
from pathlib import Path
import shutil
import subprocess
import sys

APP_ID = "io.github.x_vlad_x.Lichen.Devel"


def main():
    source, destination, magick = sys.argv[1:]
    resource = Path(source) / "android/sdk/src/main/res"
    destination = Path(destination)
    destination.mkdir(parents=True, exist_ok=True)
    raster = resource / "drawable-xxxhdpi"
    # CoMaps ships every turn icon as a raster image, including the single roundabout icon
    # (ic_turn_round) the interface draws the exit number over.
    for pattern, old, new in (
        ("ic_turn_*.webp", "ic_turn_", "om-turn-"),
        ("ic_exit_highway_to_*.webp", "ic_exit_highway_to_", "om-exit-highway-"),
    ):
        icons = sorted(raster.glob(pattern))
        if not icons:
            raise RuntimeError(f"No {pattern} icons in {raster}")
        for icon in icons:
            name = icon.stem.replace(old, new, 1).replace("_", "-")
            subprocess.run([magick, str(icon), str(destination / f"{name}.png")], check=True)
    if not (destination / "om-turn-round.png").is_file():
        raise RuntimeError("The roundabout icon ic_turn_round is missing")
    packaging = Path(__file__).resolve().parents[1] / "packaging"
    app_icon = packaging / f"{APP_ID}.png"
    scalable = destination / "hicolor" / "scalable" / "apps" / f"{APP_ID}.svg"
    scalable.parent.mkdir(parents=True, exist_ok=True)
    shutil.copy2(packaging / f"{APP_ID}.svg", scalable)
    for size in (48, 64, 128, 256, 512):
        target = destination / "hicolor" / f"{size}x{size}" / "apps" / f"{APP_ID}.png"
        target.parent.mkdir(parents=True, exist_ok=True)
        if size == 512:
            shutil.copy2(app_icon, target)
        else:
            subprocess.run([magick, str(app_icon), "-resize", f"{size}x{size}", str(target)], check=True)
    (destination / ".stamp").touch()


if __name__ == "__main__":
    main()
