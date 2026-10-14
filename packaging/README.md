# Local preview packages

These recipes create **local previews**, not a public release. The current name
and upstream icon are retained while branding and map-data redistribution are
reviewed. No package is uploaded, installed on the host, or copied to a phone
by these scripts.

## Build

On an x86_64 Linux build machine, install Podman, Git, Python 3 and ImageMagick
(`magick`), then run from the project root:

```sh
./tools/build-packages.sh          # ARM64 .deb and x86_64 AppImage
./tools/build-packages.sh phone    # only the phone package
./tools/build-packages.sh desktop  # only the desktop AppImage
```

Set `OMGTK_BUILD_JOBS` to limit concurrent compiler processes (default: 8).
The first run downloads build dependencies and compiles the core. Output files
and their SHA-256 checksums are in `out/packages/`.

The Organic Maps core is pinned to tag `2026.09.29-32-android`, commit
`b20d1417eef1e3c63a447ac8658db4307a066024`. The script refuses to switch an existing
checkout to another revision. Container images use Debian repositories, so this
is a repeatable build recipe, not a claim of bit-for-bit reproducibility.

The phone uses the existing Debian testing cross-build. The desktop build uses
Debian 13 (trixie) and bundles application libraries, GTK resources, Qt networking
plugins and eSpeak. The host must still provide compatible glibc, display and
graphics drivers; AppImage does not mean compatibility with every distribution.
The desktop preview is intended for x86_64 systems with glibc 2.41 or newer and
OpenGL ES 3 support. Additional distributions need real testing before they are
listed as supported.

AppImage tooling and its runtime are checksum-pinned in `appimage-tools.json`.
Downloads go to `out/tools/`. Their upstream `continuous` URLs can change: a
checksum mismatch fails the build. Review a new upstream tool before changing
the lock file; do not bypass the check.

## Try the packages

### FuriOS phone

Copy `organicmaps-gtk_0.1.0-preview4-1_arm64.deb` to the phone. In its directory:

```sh
sudo apt install ./organicmaps-gtk_0.1.0-preview4-1_arm64.deb
```

Close an already running version of the app. If the original SSH installation is
still present, its per-user launcher takes precedence over the system launcher.
After installing the package, migrate that launcher once, **as the desktop user
without sudo**:

```sh
/usr/bin/organicmaps-gtk-migrate-launcher
```

This backs up only the recognized SSH desktop entry under
`$XDG_STATE_HOME/organicmaps-gtk/launcher-backups` (default: `~/.local/state`).
Custom, hidden and symlinked desktop entries are preserved. Package installation
never edits users' home directories. Repeating the migration is harmless.

You can also explicitly launch the packaged version from a terminal:

```sh
/usr/bin/organicmaps-gtk
```

Do not uninstall or purge the old version just to try this package. Existing
maps, settings and bookmarks use the same XDG locations as before. Make a backup
of important bookmarks before testing a preview. Uninstalling the system package
does not remove those user directories.

The .deb declares ELF-derived library dependencies and depends on the system
`espeak-ng` package. GeoClue and the compass are host services. A dependency error
must be investigated; do not force installation with missing dependencies.

### Desktop

Close an already running version of the app, make the AppImage executable if
needed, and run it:

```sh
chmod +x OrganicMapsGtk-0.1.0-preview4-x86_64.AppImage
./OrganicMapsGtk-0.1.0-preview4-x86_64.AppImage
```

If FUSE mounting is unavailable, use the runtime's extraction fallback:

```sh
./OrganicMapsGtk-0.1.0-preview4-x86_64.AppImage --appimage-extract-and-run
```

The AppImage includes eSpeak and voice data. GeoClue, compass hardware, the display
server, portal services and graphics drivers remain system integrations.

Application icons are installed in hicolor at 48, 64, 128, 256 and 512 pixels.
The .deb depends on `hicolor-icon-theme`, whose dpkg trigger updates its cache.
A single application ID is retained across installation methods, so the package
replaces the old menu entry rather than adding a duplicate icon.

For the first manual check, report only: did the app open, was the map visible,
and could you open Search and the map downloader? Include the exact error if it
did not start. GPS and voice need separate device checks.

## Developer checks and installation layout

```sh
python3 -m unittest discover -s tests -v
desktop-file-validate packaging/io.github.x_vlad_x.OrganicMapsGtk.desktop
bash -n tools/build-packages.sh update-organicmaps-gtk.sh
```

CMake component `OrganicMapsGtk` installs a launcher in `PREFIX/bin`, the real
binary and resources in `PREFIX/libexec/organicmaps-gtk`, desktop metadata and
icons under `PREFIX/share`, and license files under `PREFIX/share/doc`.
`tools/package.py` stages that component without installing it on the host.
The legacy SSH launcher layout remains supported and tested, including paths
and file arguments containing spaces.

Each staged package includes `build-info.json` with the project/core revisions,
architecture and dirty-state marker. The phone .deb dependencies are generated
using `dpkg-shlibdeps` inside its build container. The AppImage includes a Debian
package inventory and copyright files for bundled libraries. Before any public
release, finish the license/source-distribution review for the bundled software,
especially eSpeak and other copyleft components, and resolve branding/data terms.

The `--version` option returns the GTK interface version from the root `VERSION`
file without opening a window. About also shows the core and map-data versions.

## Prepare release assets locally

First commit the reviewed source changes, then rebuild the packages from that clean
commit. Release preparation refuses dirty or mismatched sources. Collect sources
from the manifests generated by those exact builds:

```sh
python3 tools/collect_debian_sources.py \
  out/packages/appimage-amd64/OrganicMapsGtk.AppDir/usr/share/doc/organicmaps-gtk/bundled/bundled-packages.json \
  out/packages/appimage-amd64/OrganicMapsGtk.AppDir/usr/share/doc/organicmaps-gtk/compiler-runtime/sources.json
python3 tools/collect_debian_sources.py \
  out/packages/deb-arm64/root/usr/share/doc/organicmaps-gtk/compiler-runtime/sources.json \
  --container localhost/om-cross --output out/corresponding-sources/compiler-arm64
python3 tools/prepare_release.py
```

The source collector enables deb-src only in a disposable build container. It
downloads exact Debian versions, verifies SHA-256 against signed APT metadata,
and cross-checks every `.dsc` file list. Old versions disappearing from the
repository cause an error; do not silently substitute newer sources. Retain
the completed source archive for as long as the binaries are distributed.

`prepare_release.py` checks source coverage for all bundled-file and compiler
manifests, collects the checksum-pinned AppImage runtime sources, and creates
source archives, binaries, instructions, inventories and SHA256SUMS in
`out/release/<version>/`. It does not create tags, push code or publish a release.
See [LICENSES.md](LICENSES.md) for component licensing and modification instructions.

The component source archive is supplied as numbered 64 MiB parts, with checksums
and reassembly instructions in `SOURCES.md`. Release uploads can transfer the
parts in parallel. Installers do not need these source files.

The GTK source archive includes `SOURCE-REVISION.json`, which lets packaging
verify source provenance when `.git` is absent. Building the archive still needs
network access to fetch the pinned core and the documented build dependencies.
