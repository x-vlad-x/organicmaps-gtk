#!/usr/bin/env bash
# Builds Lichen, a native GNOME interface for Organic Maps (GTK4 + libadwaita) for aarch64 Linux phones
# (tested on the FuriLabs FLX1s with FuriOS) and installs it over SSH.
#
#   ./update-lichen.sh                 build the latest release, install it if the phone has an older one
#   ./update-lichen.sh --force         rebuild and reinstall even if the version is the same
#   ./update-lichen.sh --tag <tag>     build a specific tag (e.g. 2026.09.29-32-android)
#   ./update-lichen.sh --build-only    only build, don't install on the phone
#   ./update-lichen.sh --ui-only       quickly rebuild and update only the program on the phone
#                                               (without the world map data and the voice engine)
#   ./update-lichen.sh --check         only check what is installed on the phone and what's released
#   ./update-lichen.sh --uninstall     remove from the phone (maps, bookmarks and settings are kept;
#                                               --uninstall --purge removes them too)
#
#   PHONE=user@host ./update-lichen.sh  SSH destination (default: "furios", e.g. from ~/.ssh/config)
#
# How it works: the Organic Maps core (C++: map rendering, search, routing, map downloads) comes from
# the official repository unchanged, except for a small build patch (patches/). The interface is our
# own, built on GTK4/libadwaita (gtk-ui/), and follows the Android app: full-screen map, bottom menu,
# place page sheet, route planning and turn-by-turn navigation, search, map downloader, bookmarks and
# tracks, settings. UI strings come from the same translations as the Android/iOS apps.
# The build cross-compiles for aarch64 in a Debian testing container (same base as FuriOS 14) with podman.
# On the phone the program lives in ~/.local/share/lichen; maps and bookmarks in
# ~/.local/share/OrganicMaps, settings in ~/.config/OrganicMaps.

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
SRC_DIR="$SCRIPT_DIR/organicmaps"
BUILD_DIR="$SCRIPT_DIR/build-aarch64"
OUT_DIR="$SCRIPT_DIR/out"
PHONE="${PHONE:-furios}"                       # SSH host, e.g. from ~/.ssh/config
REMOTE_DIR=".local/share/lichen"      # relative to $HOME on the phone
REMOTE_MAPS=".local/share/OrganicMaps"
REMOTE_CONFIG=".config/OrganicMaps"
REPO_URL="https://github.com/organicmaps/organicmaps.git"
IMAGE="localhost/om-cross"
APP_ID="io.github.x_vlad_x.Lichen"
# Launchers and install directory of earlier builds, removed on install.
OLD_APP_IDS="app.organicmaps.gtk io.github.x_vlad_x.OrganicMapsGtk"
OLD_REMOTE_DIR=".local/share/organicmaps-gtk"
TTS_PACKAGES="espeak-ng libespeak-ng1 espeak-ng-data libpcaudio0 libsonic0"

FORCE=0; BUILD_ONLY=0; TAG=""; CHECK=0; UNINSTALL=0; PURGE=0; UI_ONLY=0
while [[ $# -gt 0 ]]; do
  case "$1" in
    --force) FORCE=1 ;;
    --build-only) BUILD_ONLY=1 ;;
    --tag) TAG="$2"; shift ;;
    --ui-only) UI_ONLY=1 ;;
    --check) CHECK=1 ;;
    --uninstall) UNINSTALL=1 ;;
    --purge) PURGE=1 ;;
    -h|--help) sed -n '2,/^$/p' "$0"; exit 0 ;;
    *) echo "Unknown option: $1" >&2; exit 1 ;;
  esac
  shift
done

log() { printf '\n\033[1;34m==> %s\033[0m\n' "$*"; }
die() { printf '\033[1;31mError: %s\033[0m\n' "$*" >&2; exit 1; }
rsh() { ssh -o BatchMode=yes -o ConnectTimeout=10 "$PHONE" "$@"; }
in_container() { podman run --rm --userns=keep-id -v "$SCRIPT_DIR":/src:z -w /src "$IMAGE" "$@"; }

ui_rev() { cat "$SCRIPT_DIR"/gtk-ui/*.cpp "$SCRIPT_DIR"/gtk-ui/*.hpp "$SCRIPT_DIR"/gtk-ui/*.c "$SCRIPT_DIR"/patches/*.patch | md5sum | cut -c1-8; }

# --- uninstall ---------------------------------------------------------------
if [[ $UNINSTALL -eq 1 ]]; then
  [[ $PURGE -eq 1 ]] && log "Removing Lichen from the phone, including maps and bookmarks" \
                     || log "Removing Lichen from the phone (maps, bookmarks and settings are kept)"
  rsh "pkill -f '[b]in/(lichen|organicmaps-gtk)' ; true"
  cmd="rm -rf $REMOTE_DIR $OLD_REMOTE_DIR .local/share/applications/$APP_ID.desktop;"
  for id in $OLD_APP_IDS; do cmd+="rm -f .local/share/applications/$id.desktop;"; done
  [[ $PURGE -eq 1 ]] && cmd+="rm -rf $REMOTE_MAPS $REMOTE_CONFIG;"
  cmd+="update-desktop-database .local/share/applications 2>/dev/null; true"
  rsh "$cmd"
  rsh "ls -d $REMOTE_MAPS $REMOTE_CONFIG 2>/dev/null | sed 's/^/  kept: ~\//'" || true
  log "Done"
  exit 0
fi

# --- which version ----------------------------------------------------------
if [[ -z "$TAG" ]]; then
  TAG=$(git ls-remote --tags --refs "$REPO_URL" '*-android' 2>/dev/null | awk -F/ '{print $3}' | sort -V | tail -1)
fi
[[ -n "$TAG" ]] || die "could not find the latest Organic Maps release"
VERSION="${TAG%-android}"
FULL_VERSION="$VERSION+gtk.$(ui_rev)"
log "Organic Maps: $VERSION, interface: $(ui_rev)"

INSTALLED=""
if [[ $BUILD_ONLY -eq 0 ]]; then
  INSTALLED=$(rsh "cat $REMOTE_DIR/VERSION 2>/dev/null" || true)
  echo "On the phone: ${INSTALLED:-not installed}"
fi
if [[ $CHECK -eq 1 ]]; then
  if [[ "$INSTALLED" == "$FULL_VERSION" ]]; then echo "Up to date."
  elif [[ "${INSTALLED%%+*}" == "$VERSION" ]]; then echo "Same core, the interface changed. Update: $0 --ui-only"
  else echo "Update available: ${INSTALLED:-none} -> $FULL_VERSION. Install: $0"; fi
  exit 0
fi
if [[ $BUILD_ONLY -eq 0 && $UI_ONLY -eq 0 && "$INSTALLED" == "$FULL_VERSION" && $FORCE -eq 0 ]]; then
  echo "Already the latest version. To reinstall: --force"
  exit 0
fi

command -v podman >/dev/null || die "podman is required"
command -v magick >/dev/null || die "ImageMagick (magick) is required for the turn icons"

# --- sources -----------------------------------------------------------------
if [[ ! -d "$SRC_DIR/.git" ]]; then
  log "Cloning Organic Maps $TAG"
  git clone --depth 1 --branch "$TAG" "$REPO_URL" "$SRC_DIR"
fi
CURRENT=$(git -C "$SRC_DIR" describe --tags --exact-match 2>/dev/null || true)
if [[ "$CURRENT" != "$TAG" ]]; then
  log "Switching the sources to $TAG"
  git -C "$SRC_DIR" checkout -- .
  git -C "$SRC_DIR" fetch --depth 1 origin tag "$TAG"
  git -C "$SRC_DIR" checkout -q "$TAG"
  rm -rf "$BUILD_DIR"
fi
log "Submodules"
git -C "$SRC_DIR" submodule update --init --recursive --depth 1 >/dev/null

log "Patches"
git -C "$SRC_DIR" checkout -- CMakeLists.txt libs/platform/CMakeLists.txt
for p in "$SCRIPT_DIR"/patches/*.patch; do
  echo "  $(basename "$p")"
  git -C "$SRC_DIR" apply "$p" || die "patch $(basename "$p") does not apply, regenerate it for the new version"
done

# --- build container ---------------------------------------------------------
if ! podman image exists "$IMAGE"; then
  log "Building the cross-compilation container (Debian testing + arm64)"
  podman build -t "$IMAGE" -f "$SCRIPT_DIR/build-env/Containerfile" "$SCRIPT_DIR/build-env"
fi

# --- build -------------------------------------------------------------------
python3 "$SCRIPT_DIR/tools/prepare_package_assets.py" "$SRC_DIR" "$OUT_DIR/package-icons" "$(command -v magick)"
log "Building (aarch64)"
in_container cmake -S organicmaps -B build-aarch64 -G Ninja -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_TOOLCHAIN_FILE=/src/build-env/aarch64-toolchain.cmake -DSKIP_QT_GUI=ON -DSKIP_TESTS=ON \
  -DBUILD_TESTING=OFF -DOMIM_GTK_APP_DIR=/src/gtk-ui -DLICHEN_PREPARED_ICONS=/src/out/package-icons >/dev/null
in_container ninja -C build-aarch64 -j"$(nproc)" lichen
in_container aarch64-linux-gnu-strip -o build-aarch64/lichen.stripped build-aarch64/lichen
NEED_GLIBC=$(in_container aarch64-linux-gnu-objdump -T build-aarch64/lichen | grep -o 'GLIBC_2\.[0-9]*' | sort -uV | tail -1)
echo "  requires $NEED_GLIBC"

# --- package -----------------------------------------------------------------
DIST="$OUT_DIR/lichen-$VERSION"
log "Packaging: $DIST"
rm -rf "$DIST"
mkdir -p "$DIST/bin" "$DIST/icons"
cp "$BUILD_DIR/lichen.stripped" "$DIST/bin/lichen"
cp "$SCRIPT_DIR/packaging/lichen" "$SCRIPT_DIR/packaging/$APP_ID.desktop" \
   "$SCRIPT_DIR/packaging/$APP_ID.png" "$DIST/"

# Core data (styles, fonts, symbols, World.mwm, translations). Country maps are downloaded in the app.
rsync -a --delete \
  --exclude borders --exclude test_data --exclude styles --exclude symbols-svg --exclude generator \
  --exclude vulkan_shaders --exclude 'minsk-pass*' --exclude 'drules_*.txt' --exclude drules_design.bin \
  --exclude 'drules_merged.*' --exclude 'mapcss-*' --exclude taginfo.json --exclude '*.csv' \
  --exclude '*.md' --exclude config.xsd \
  "$SRC_DIR/data/" "$DIST/data/"

# Icons: our symbolic SVGs plus the turn and junction icons from the Android app.
cp "$SCRIPT_DIR"/gtk-ui/icons/*.svg "$DIST/icons/"
cp "$SCRIPT_DIR/packaging/$APP_ID.png" "$DIST/icons/"
rsync -a --exclude .stamp "$OUT_DIR/package-icons/" "$DIST/icons/"

echo "$FULL_VERSION" > "$DIST/VERSION"
echo "Done: $DIST ($(du -sh "$DIST" | cut -f1))"

[[ $BUILD_ONLY -eq 1 ]] && exit 0

# --- install -----------------------------------------------------------------
log "Closing Lichen on the phone"
rsh "pkill -f '[b]in/(lichen|organicmaps-gtk)' ; true"

install_desktop_entry() {
  rsh "set -e; mkdir -p .local/share/applications
       sed -e \"s|^Exec=.*|Exec=\$HOME/$REMOTE_DIR/lichen %U|\" \\
           -e \"s|^Icon=.*|Icon=\$HOME/$REMOTE_DIR/$APP_ID.png|\" \\
           $REMOTE_DIR/$APP_ID.desktop > .local/share/applications/$APP_ID.desktop
       for id in $OLD_APP_IDS; do rm -f .local/share/applications/\$id.desktop; done
       rm -rf $OLD_REMOTE_DIR
       update-desktop-database .local/share/applications 2>/dev/null || true"
}

if [[ $UI_ONLY -eq 1 ]] && rsh "test -d $REMOTE_DIR/data"; then
  log "Updating the program and icons on the phone"
  tar -C "$DIST" -cf - bin icons lichen "$APP_ID.desktop" "$APP_ID.png" VERSION \
    | gzip -1 | rsh "mkdir -p $REMOTE_DIR && tar -C $REMOTE_DIR -xzf -"
  install_desktop_entry
  log "Done: $FULL_VERSION (restart the app)"
  exit 0
fi

log "Copying to the phone"
rsh "rm -rf $REMOTE_DIR.new && mkdir -p $REMOTE_DIR.new"
tar -C "$DIST" -cf - . | gzip -1 | rsh "tar -C $REMOTE_DIR.new -xzf -"

log "Voice guidance: espeak-ng (from the phone's apt, no root needed)"
rsh "set -e; cd $REMOTE_DIR.new; tmp=\$(mktemp -d)
     if (cd \$tmp && apt-get download $TTS_PACKAGES >/dev/null 2>&1); then
       mkdir -p tts; for d in \$tmp/*.deb; do dpkg-deb -x \$d tts; done; echo '  ok'
     else echo '  download failed: voice guidance will only work with speech-dispatcher installed'; fi
     rm -rf \$tmp"

rsh "set -e; rm -rf $REMOTE_DIR.old; [ -d $REMOTE_DIR ] && mv $REMOTE_DIR $REMOTE_DIR.old; mv $REMOTE_DIR.new $REMOTE_DIR; rm -rf $REMOTE_DIR.old"
install_desktop_entry

log "Checking that the program starts on the phone"
rsh "cd $REMOTE_DIR && ./bin/lichen --help >/dev/null 2>&1 && echo '  ok' || echo '  does NOT start, check: ~/$REMOTE_DIR/lichen' >&2"

log "Installed: $FULL_VERSION, look for \"Lichen\" in the app list"
