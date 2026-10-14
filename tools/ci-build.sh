#!/usr/bin/env bash
# Build and verify one format/architecture pair before the release assembly job.
set -euo pipefail
cd "$(dirname "${BASH_SOURCE[0]}")/.."
target="${1:?Usage: ci-build.sh FORMAT-ARCH}"
case "$target" in deb-amd64|deb-arm64|appimage-amd64|appimage-arm64|rpm-amd64|rpm-arm64) ;; *) exit 2 ;; esac
./tools/build-packages.sh "$target"
kind="${target%-*}"
arch="${target##*-}"
version="$(cat VERSION)"
if [[ "$kind" != rpm ]]; then
  image=localhost/om-desktop
  if [[ "$arch" == arm64 ]]; then
    if [[ "$kind" == deb ]]; then image=localhost/om-cross; else image=localhost/om-desktop-arm64; fi
  fi
  stage="out/packages/$target/root"
  inventories=()
  if [[ "$kind" == appimage ]]; then
    stage="out/packages/$target/Lichen.AppDir"
    inventories+=("$stage/usr/share/doc/lichen/bundled/bundled-packages.json")
  fi
  inventories+=("$stage/usr/share/doc/lichen/compiler-runtime/sources.json")
  python3 tools/collect_debian_sources.py "${inventories[@]}" --container "$image" \
    --output "out/corresponding-sources/$target"
fi
if [[ "$kind" == appimage ]]; then
  podman build -t localhost/om-smoke -f build-env/Containerfile.smoke build-env
  machine=x86_64
  [[ "$arch" != arm64 ]] || machine=aarch64
  actual="$(podman run --rm --network=none --userns=keep-id -v "$PWD:/src:ro" -w /src \
    -e XDG_DATA_HOME=/tmp/test-data -e XDG_CONFIG_HOME=/tmp/test-config \
    localhost/om-smoke "/src/out/packages/Lichen-$version-$machine.AppImage" \
    --appimage-extract-and-run --version)"
  [[ "$actual" == "$version" ]]
elif [[ "$kind" == rpm ]]; then
  machine="$(uname -m)"
  podman build -t localhost/om-rpm-smoke -f build-env/Containerfile.rpm-smoke build-env
  mkdir -p "out/ci/smoke-$target"
  podman run --rm -v "$PWD:/src:ro" -v "$PWD/out/ci/smoke-$target:/artifacts:z" localhost/om-rpm-smoke sh -ec '
    dnf -y --setopt=zchunk=False --setopt=install_weak_deps=False install "$1"
    test "$(/usr/bin/lichen --version)" = "$2"
    rpm -V lichen
    python3 /src/tools/smoke_rpm.py /artifacts
  ' sh "/src/out/packages/lichen-$version-1.fc44.$machine.rpm" "$version"
elif [[ "$arch" == amd64 ]]; then
  podman run --rm -v "$PWD:/src:ro" docker.io/library/debian:trixie sh -ec '
    apt-get update -qq
    DEBIAN_FRONTEND=noninteractive apt-get install -y --no-install-recommends "$1"
    test "$(/usr/bin/lichen --version)" = "$2"
    dpkg -V lichen
  ' sh "/src/out/packages/lichen_$version-1_amd64.deb" "$version"
fi
python3 tools/ci_release.py bundle "$target"
