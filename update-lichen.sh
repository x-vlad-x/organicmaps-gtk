#!/usr/bin/env bash
# Builds Lichen Preview, the prototype of the Lichen GTK interface (GTK4 + libadwaita) on the CoMaps
# core, for aarch64 Linux phones (tested on the FuriLabs FLX1s with FuriOS) and installs it over SSH
# beside the regular Lichen, which it never touches.
#
#   ./update-lichen.sh                 build the pinned core, install it if the phone has an older build
#   ./update-lichen.sh --force         rebuild and reinstall even if the version is the same
#   ./update-lichen.sh --build-only    only build, don't install on the phone
#   ./update-lichen.sh --ui-only       quickly rebuild and update only the program on the phone
#                                               (without the world map data and the voice engine)
#   ./update-lichen.sh --check         only check what is installed on the phone and what's released
#   ./update-lichen.sh --uninstall     remove from the phone (maps, bookmarks and settings are kept;
#                                               --uninstall --purge removes them too)
#
#   PHONE=user@host ./update-lichen.sh  SSH destination (default: "furios", e.g. from ~/.ssh/config)
#
# How it works: the CoMaps core (C++: map rendering, search, routing, map downloads) comes from the
# CoMaps repository at the commit pinned in tools/core.env, unchanged except for a small build patch
# (patches/). The interface is our own, built on GTK4/libadwaita (gtk-ui/), and follows the Android
# app: full-screen map, bottom menu, place page sheet, route planning and turn-by-turn navigation,
# search, map downloader, bookmarks and tracks, settings. UI strings come from the CoMaps Android
# translations. The build cross-compiles for aarch64 in a Debian testing container (same base as
# FuriOS 14) with podman.
# On the phone the program lives in ~/.local/share/lichen-devel; maps in ~/.local/share/CoMaps,
# settings and bookmarks in ~/.config/CoMaps.

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck source=tools/core.env
source "$SCRIPT_DIR/tools/core.env"
SRC_DIR="$SCRIPT_DIR/core"
BUILD_DIR="$SCRIPT_DIR/build-aarch64"
OUT_DIR="$SCRIPT_DIR/out"
PHONE="${PHONE:-furios}"                       # SSH host, e.g. from ~/.ssh/config
REMOTE_DIR=".local/share/lichen-devel"         # relative to $HOME on the phone
REMOTE_MAPS=".local/share/CoMaps"
REMOTE_CONFIG=".config/CoMaps"
IMAGE="localhost/om-cross"
APP_ID="io.github.x_vlad_x.Lichen.Devel"
LAUNCHER="lichen-devel"
TTS_PACKAGES="espeak-ng libespeak-ng1 espeak-ng-data libpcaudio0 libsonic0"

FORCE=0; BUILD_ONLY=0; CHECK=0; UNINSTALL=0; PURGE=0; UI_ONLY=0
while [[ $# -gt 0 ]]; do
  case "$1" in
    --force) FORCE=1 ;;
    --build-only) BUILD_ONLY=1 ;;
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
  [[ $PURGE -eq 1 ]] && log "Removing Lichen Preview from the phone, including its maps and bookmarks" \
                     || log "Removing Lichen Preview from the phone (maps, bookmarks and settings are kept)"
  rsh "pkill -f '[l]ichen-devel/bin/lichen' ; true"
  cmd="rm -rf $REMOTE_DIR .local/share/applications/$APP_ID.desktop;"
  [[ $PURGE -eq 1 ]] && cmd+="rm -rf $REMOTE_MAPS $REMOTE_CONFIG;"
  cmd+="update-desktop-database .local/share/applications 2>/dev/null; true"
  rsh "$cmd"
  rsh "ls -d $REMOTE_MAPS $REMOTE_CONFIG 2>/dev/null | sed 's/^/  kept: ~\//'" || true
  log "Done"
  exit 0
fi

# --- which version ----------------------------------------------------------
VERSION="comaps.${CORE_COMMIT:0:8}"
FULL_VERSION="$VERSION+gtk.$(ui_rev)"
log "$CORE_NAME: ${CORE_COMMIT:0:8}, interface: $(ui_rev)"

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
log "Preparing $CORE_NAME ${CORE_COMMIT:0:8}: submodules, patches, world maps, generated data"
"$SCRIPT_DIR/tools/prepare-core.sh"

# --- build container ---------------------------------------------------------
if ! podman image exists "$IMAGE"; then
  log "Building the cross-compilation container (Debian testing + arm64)"
  podman build -t "$IMAGE" -f "$SCRIPT_DIR/build-env/Containerfile" "$SCRIPT_DIR/build-env"
fi

# --- build -------------------------------------------------------------------
python3 "$SCRIPT_DIR/tools/prepare_package_assets.py" "$SRC_DIR" "$OUT_DIR/package-icons" "$(command -v magick)"
log "Building (aarch64)"
in_container cmake -S core -B build-aarch64 -G Ninja -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_TOOLCHAIN_FILE=/src/build-env/aarch64-toolchain.cmake -DSKIP_QT_GUI=ON -DSKIP_TESTS=ON \
  -DBUILD_TESTING=OFF -DOMIM_GTK_APP_DIR=/src/gtk-ui -DLICHEN_PREPARED_ICONS=/src/out/package-icons >/dev/null
in_container ninja -C build-aarch64 -j"$(nproc)" lichen
in_container aarch64-linux-gnu-strip -o build-aarch64/lichen.stripped build-aarch64/lichen
NEED_GLIBC=$(in_container aarch64-linux-gnu-objdump -T build-aarch64/lichen | grep -o 'GLIBC_2\.[0-9]*' | sort -uV | tail -1)
echo "  requires $NEED_GLIBC"

# --- package -----------------------------------------------------------------
DIST="$OUT_DIR/lichen-devel-$VERSION"
log "Packaging: $DIST"
rm -rf "$DIST"
mkdir -p "$DIST/bin" "$DIST/icons"
cp "$BUILD_DIR/lichen.stripped" "$DIST/bin/lichen"
cp "$SCRIPT_DIR/packaging/$LAUNCHER" "$SCRIPT_DIR/packaging/$APP_ID.desktop" \
   "$SCRIPT_DIR/packaging/$APP_ID.png" "$DIST/"

# Core data (styles, fonts, symbols, World.mwm). Country maps are downloaded in the app.
rsync -a --delete \
  --exclude borders --exclude test_data --exclude styles --exclude symbols-svg --exclude generator \
  --exclude vulkan_shaders --exclude translations --exclude world_mwm --exclude 'minsk-pass*' \
  --exclude 'drules_*.txt' --exclude drules_design.bin --exclude 'drules_merged.*' --exclude mapcss-dynamic.txt \
  --exclude taginfo.json --exclude '*.diff' --exclude '*_hash' --exclude '*.prev' --exclude borders_vs_osm.csv \
  --exclude countries_synonyms.csv --exclude old_vs_new.csv --exclude '*.md' --exclude config.xsd \
  "$SRC_DIR/data/" "$DIST/data/"
# UI translations from the CoMaps Android resources.
for part in app sdk; do
  (cd "$SRC_DIR/android/$part/src/main/res" && find . -path './values*' \( -name strings.xml -o -name types_strings.xml \) \
     -exec install -D -m 644 {} "$DIST/strings/$part/{}" \;)
done

# Icons: our symbolic SVGs plus the turn and junction icons from the Android app.
cp "$SCRIPT_DIR"/gtk-ui/icons/*.svg "$DIST/icons/"
cp "$SCRIPT_DIR/packaging/$APP_ID.png" "$DIST/icons/"
rsync -a --exclude .stamp "$OUT_DIR/package-icons/" "$DIST/icons/"

echo "$FULL_VERSION" > "$DIST/VERSION"
echo "Done: $DIST ($(du -sh "$DIST" | cut -f1))"

[[ $BUILD_ONLY -eq 1 ]] && exit 0

# --- install -----------------------------------------------------------------
log "Closing Lichen Preview on the phone"
rsh "pkill -f '[l]ichen-devel/bin/lichen' ; true"

install_desktop_entry() {
  rsh "set -e; mkdir -p .local/share/applications
       sed -e \"s|^Exec=.*|Exec=\$HOME/$REMOTE_DIR/$LAUNCHER %U|\" \\
           -e \"s|^Icon=.*|Icon=\$HOME/$REMOTE_DIR/$APP_ID.png|\" \\
           $REMOTE_DIR/$APP_ID.desktop > .local/share/applications/$APP_ID.desktop
       update-desktop-database .local/share/applications 2>/dev/null || true"
}

if [[ $UI_ONLY -eq 1 ]] && rsh "test -d $REMOTE_DIR/data"; then
  log "Updating the program and icons on the phone"
  tar -C "$DIST" -cf - bin icons strings "$LAUNCHER" "$APP_ID.desktop" "$APP_ID.png" VERSION \
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
rsh "cd $REMOTE_DIR && ./bin/lichen --help >/dev/null 2>&1 && echo '  ok' || echo '  does NOT start, check: ~/$REMOTE_DIR/$LAUNCHER' >&2"

log "Installed: $FULL_VERSION, look for \"Lichen Preview\" in the app list"
