# Install Lichen Preview 0.1.0-prototype1

**A test build of Lichen on the CoMaps core, not a release.** It installs beside the
regular Lichen and does not change it: the package is `lichen-devel`, the launcher is
**Lichen Preview** (icon with a red "DEV" band), and maps, settings and bookmarks are
kept separately in `~/.local/share/CoMaps` and `~/.config/CoMaps`. The Organic Maps
directories of the regular Lichen (`~/.local/share/OrganicMaps`, `~/.config/OrganicMaps`)
are never read or written. Country maps must be downloaded again inside the preview.

Choose the format for your distribution and the architecture of your device.
Run `uname -m` if unsure: `x86_64` means amd64; `aarch64` means ARM64.

## DEB — FuriOS and Debian

For an ARM64 FuriOS phone:

```sh
sudo apt install ./lichen-devel_0.1.0-prototype1-1_arm64.deb
```

For an x86_64 computer with Debian 13 or compatible libraries:

```sh
sudo apt install ./lichen-devel_0.1.0-prototype1-1_amd64.deb
```

The package manager installs the required libraries, eSpeak and icon theme.
Do not force installation if dependencies cannot be resolved. Launch **Lichen Preview**
from the application menu, or run `lichen-devel`.

To remove it (maps, settings and bookmarks stay in `~/.local/share/CoMaps` and
`~/.config/CoMaps`; delete those directories to remove them too):

```sh
sudo apt remove lichen-devel
```

## RPM — Fedora 44

```sh
sudo dnf install ./lichen-devel-0.1.0-prototype1-1.fc44.x86_64.rpm
sudo dnf install ./lichen-devel-0.1.0-prototype1-1.fc44.aarch64.rpm   # ARM64
sudo dnf remove lichen-devel
```

These are Fedora packages. They are not signed; verify file hashes against `SHA256SUMS`.

## AppImage — portable application bundle

```sh
chmod +x LichenPreview-0.1.0-prototype1-x86_64.AppImage
./LichenPreview-0.1.0-prototype1-x86_64.AppImage
```

For ARM64, use `LichenPreview-0.1.0-prototype1-aarch64.AppImage`. The AppImage
requires glibc 2.41 or newer and OpenGL ES 3. If FUSE is unavailable, add
`--appimage-extract-and-run`.

## Verify downloads

Download `SHA256SUMS` beside the packages and run:

```sh
sha256sum --check --ignore-missing SHA256SUMS
```

See `LICENSES.md` for component licenses and sources.
