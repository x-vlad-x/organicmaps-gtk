#!/usr/bin/env python3
"""Prepare navigation icons from the pinned Organic Maps sources."""
from pathlib import Path
import shutil
import subprocess
import sys

from vector2svg import convert


def main():
    source, destination, magick = sys.argv[1:]
    resource = Path(source) / "android/sdk/src/main/res"
    destination = Path(destination)
    destination.mkdir(parents=True, exist_ok=True)
    raster = resource / "drawable-xxxhdpi"
    for pattern, old, new in (
        ("ic_turn_*.webp", "ic_turn_", "om-turn-"),
        ("ic_exit_highway_to_*.webp", "ic_exit_highway_to_", "om-exit-highway-"),
    ):
        for icon in sorted(raster.glob(pattern)):
            name = icon.stem.replace(old, new, 1).replace("_", "-")
            subprocess.run([magick, str(icon), str(destination / f"{name}.png")], check=True)
    vector = resource / "drawable"
    for icon in sorted(vector.glob("ic_roundabout_exit_*.xml")) + [vector / "ic_turn_round.xml"]:
        name = "om-" + icon.stem.removeprefix("ic_").replace("_", "-")
        convert(str(icon), str(destination / f"{name}.svg"), True)
    app_id = "io.github.x_vlad_x.OrganicMapsGtk"
    app_icon = Path(__file__).resolve().parents[1] / "packaging" / f"{app_id}.png"
    for size in (48, 64, 128, 256, 512):
        target = destination / "hicolor" / f"{size}x{size}" / "apps" / f"{app_id}.png"
        target.parent.mkdir(parents=True, exist_ok=True)
        if size == 512:
            shutil.copy2(app_icon, target)
        else:
            subprocess.run([magick, str(app_icon), "-resize", f"{size}x{size}", str(target)], check=True)
    (destination / ".stamp").touch()


if __name__ == "__main__":
    main()
