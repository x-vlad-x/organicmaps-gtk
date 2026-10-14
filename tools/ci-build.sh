#!/usr/bin/env bash
# Build and verify one CI target; transfer only release inputs to the next job.
set -euo pipefail
cd "$(dirname "${BASH_SOURCE[0]}")/.."
target="${1:?Usage: ci-build.sh phone|desktop}"
case "$target" in phone|desktop) ;; *) exit 2 ;; esac
./tools/build-packages.sh "$target"
if [[ "$target" == phone ]]; then
  python3 tools/collect_debian_sources.py \
    out/packages/deb-arm64/root/usr/share/doc/organicmaps-gtk/compiler-runtime/sources.json \
    --container localhost/om-cross --output out/corresponding-sources/compiler-arm64
else
  python3 tools/collect_debian_sources.py \
    out/packages/appimage-amd64/OrganicMapsGtk.AppDir/usr/share/doc/organicmaps-gtk/bundled/bundled-packages.json \
    out/packages/appimage-amd64/OrganicMapsGtk.AppDir/usr/share/doc/organicmaps-gtk/compiler-runtime/sources.json \
    --container localhost/om-desktop
  # Test on Debian 13 with system graphics/font libraries, without GTK/Qt development packages.
  podman build -t localhost/om-smoke -f build-env/Containerfile.smoke build-env
  version="$(cat VERSION)"
  actual="$(podman run --rm --network=none --userns=keep-id -v "$PWD:/src:ro" -w /src \
    localhost/om-smoke "/src/out/packages/OrganicMapsGtk-$version-x86_64.AppImage" \
    --appimage-extract-and-run --version)"
  [[ "$actual" == "$version" ]]
fi
python3 tools/ci_release.py bundle "$target"
