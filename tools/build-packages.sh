#!/usr/bin/env bash
# Build local previews only. Does not SSH to a phone, install, push, or publish.
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"
TARGET="${1:-all}"
case "$TARGET" in phone|desktop|all) ;; *) echo "Usage: $0 [phone|desktop|all]" >&2; exit 2 ;; esac
CORE_TAG=2026.09.29-32-android
CORE_COMMIT=b20d1417eef1e3c63a447ac8658db4307a066024
JOBS="${OMGTK_BUILD_JOBS:-8}"
for tool in podman git python3 magick; do
  command -v "$tool" >/dev/null || { echo "Required tool is missing: $tool" >&2; exit 1; }
done
if [[ ! -d organicmaps/.git ]]; then
  git clone --depth 1 --branch "$CORE_TAG" https://github.com/organicmaps/organicmaps.git organicmaps
fi
if [[ "$(git -C organicmaps rev-parse HEAD)" != "$CORE_COMMIT" ]]; then
  echo "Expected Organic Maps $CORE_TAG ($CORE_COMMIT). Existing sources were left untouched." >&2
  exit 1
fi
git -C organicmaps submodule update --init --recursive --depth 1
for patch in "$ROOT"/patches/*.patch; do
  if git -C organicmaps apply --reverse --check "$patch" 2>/dev/null; then
    continue
  fi
  git -C organicmaps apply --check "$patch"
  git -C organicmaps apply "$patch"
done
mkdir -p out
python3 tools/prepare_package_assets.py organicmaps out/package-icons "$(command -v magick)"

build_target() {
  local target="$1" image="$2" recipe="$3" build="$4"
  shift 4
  podman build -t "$image" -f "$recipe" build-env
  podman run --rm --network=none --userns=keep-id -v "$ROOT:/src:z" -w /src "$image" \
    cmake -S organicmaps -B "$build" -G Ninja -DCMAKE_BUILD_TYPE=Release \
    -DSKIP_QT_GUI=ON -DBUILD_TESTING=OFF -DOMIM_GTK_APP_DIR=/src/gtk-ui \
    -DOMGTK_PREPARED_ICONS=/src/out/package-icons -DCMAKE_INSTALL_PREFIX=/usr "$@"
  podman run --rm --network=none --userns=keep-id -v "$ROOT:/src:z" -w /src "$image" \
    cmake --build "$build" --target organicmaps-gtk -j "$JOBS"
  python3 tools/package.py "$target" --build-dir "$build" --container "$image"
}

if [[ "$TARGET" == phone || "$TARGET" == all ]]; then
  build_target deb localhost/om-cross build-env/Containerfile build-aarch64 \
    -DCMAKE_TOOLCHAIN_FILE=/src/build-env/aarch64-toolchain.cmake
fi
if [[ "$TARGET" == desktop || "$TARGET" == all ]]; then
  build_target appimage localhost/om-desktop build-env/Containerfile.desktop build-appimage \
    -DOMGTK_PHONE_GLIBC_COMPAT=OFF
fi
