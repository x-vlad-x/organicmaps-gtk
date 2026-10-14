#!/usr/bin/env python3
"""Download World.mwm and WorldCoasts.mwm for the pinned core and verify them.

CoMaps does not keep the world maps in Git; its configure.sh downloads them from the map
generator. This does the same and checks the sizes and SHA-1 digests listed in countries.txt.

  python3 tools/fetch_world_maps.py core/data
"""
import base64
import hashlib
import json
from pathlib import Path
import sys
import urllib.request

BASE_URL = "https://mapgen-fi-1.comaps.app/maps"


def verified(path, size, sha1):
    if not path.is_file() or path.is_symlink() or path.stat().st_size != size:
        return False
    digest = hashlib.sha1()
    with path.open("rb") as stream:
        while chunk := stream.read(1 << 20):
            digest.update(chunk)
    return base64.b64encode(digest.digest()).decode() == sha1


def main():
    data = Path(sys.argv[1])
    countries = json.loads((data / "countries.txt").read_text())
    version, series = countries["v"], countries["map_series"]
    for entry in countries["g"]:
        if entry["id"] not in ("World", "WorldCoasts"):
            continue
        target = data / f"{entry['id']}.mwm"
        if verified(target, entry["s"], entry["sha1_base64"]):
            continue
        url = f"{BASE_URL}/{series}/{version}/{target.name}"
        print(f"Downloading {url}", flush=True)
        temporary = target.with_suffix(".download")
        urllib.request.urlretrieve(url, temporary)
        if target.is_symlink():
            target.unlink()
        temporary.replace(target)
        if not verified(target, entry["s"], entry["sha1_base64"]):
            target.unlink()
            raise RuntimeError(f"Size or checksum mismatch for {url}")


if __name__ == "__main__":
    main()
