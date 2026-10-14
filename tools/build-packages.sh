#!/usr/bin/env bash
# Build local previews only. Does not SSH to a phone, install, push, or publish.
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"
TARGET="${1:-all}"
NATIVE="$(uname -m)"
case "$NATIVE" in x86_64) ARCH=amd64 ;; aarch64) ARCH=arm64 ;; *) echo "Unsupported build host: $NATIVE" >&2; exit 2 ;; esac
case "$TARGET" in
  phone) TARGET=deb-arm64 ;;
  desktop) TARGET=appimage-amd64 ;;
  deb|appimage|rpm) TARGET="$TARGET-$ARCH" ;;
  all|deb-amd64|deb-arm64|appimage-amd64|appimage-arm64|rpm-amd64|rpm-arm64) ;;
  *) echo "Usage: $0 [all|deb|rpm|appimage|FORMAT-amd64|FORMAT-arm64|phone|desktop]" >&2; exit 2 ;;
esac
CORE_TAG=2026.09.29-32-android
CORE_COMMIT=b20d1417eef1e3c63a447ac8658db4307a066024
JOBS="${LICHEN_BUILD_JOBS:-8}"
for tool in podman git python3; do
  command -v "$tool" >/dev/null || { echo "Required tool is missing: $tool" >&2; exit 1; }
done
MAGICK="$(command -v magick || command -v convert || true)"
[[ -n "$MAGICK" ]] || { echo "Required tool is missing: ImageMagick (magick or convert)" >&2; exit 1; }
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
python3 tools/prepare_package_assets.py organicmaps out/package-icons "$MAGICK"

build_target() {
  local target="$1" image="$2" recipe="$3" build="$4"
  shift 4
  podman build -t "$image" -f "$recipe" build-env
  podman run --rm --network=none --userns=keep-id -v "$ROOT:/src:z" -w /src "$image" \
    cmake -S organicmaps -B "$build" -G Ninja -DCMAKE_BUILD_TYPE=Release \
    -DSKIP_QT_GUI=ON -DBUILD_TESTING=OFF -DOMIM_GTK_APP_DIR=/src/gtk-ui \
    -DLICHEN_PREPARED_ICONS=/src/out/package-icons -DCMAKE_INSTALL_PREFIX=/usr "$@"
  podman run --rm --network=none --userns=keep-id -v "$ROOT:/src:z" -w /src "$image" \
    cmake --build "$build" --target lichen -j "$JOBS"
  python3 tools/package.py "$target" --build-dir "$build" --container "$image"
}

build_package() {
  local target="$1" format="${1%-*}" arch="${1##*-}"
  if [[ "$target" == deb-arm64 && "$ARCH" == amd64 ]]; then
    build_target deb localhost/om-cross build-env/Containerfile build-aarch64 \
      -DCMAKE_TOOLCHAIN_FILE=/src/build-env/aarch64-toolchain.cmake
  elif [[ "$arch" != "$ARCH" ]]; then
    echo "Build $target on a native $arch host (GitHub Actions supplies both architectures)." >&2
    exit 2
  elif [[ "$format" == rpm ]]; then
    podman build -t localhost/om-rpm -f build-env/Containerfile.rpm build-env
    python3 tools/package_rpm.py --container localhost/om-rpm --arch "$arch" --jobs "$JOBS"
  else
    local image=localhost/om-desktop build=build-appimage
    if [[ "$arch" == arm64 ]]; then image=localhost/om-desktop-arm64; build=build-appimage-arm64; fi
    build_target "$format" "$image" build-env/Containerfile.desktop "$build" \
      -DLICHEN_PHONE_GLIBC_COMPAT=OFF
  fi
}
if [[ "$TARGET" == all ]]; then
  for format in deb appimage rpm; do build_package "$format-$ARCH"; done
else
  build_package "$TARGET"
fi
