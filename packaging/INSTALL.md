# Install Lichen 0.1.0-preview6

Choose the format for your distribution and the architecture of your device.
Run `uname -m` if unsure: `x86_64` means amd64; `aarch64` means ARM64.
Close an already running version before upgrading. Maps, settings and bookmarks
are kept in their existing XDG locations; uninstalling the package does not delete them.

## DEB — Debian and FuriOS

For an ARM64 FuriOS phone:

```sh
sudo apt install ./lichen_0.1.0-preview6-1_arm64.deb
```

For an x86_64 computer with Debian 13 or compatible libraries:

```sh
sudo apt install ./lichen_0.1.0-preview6-1_amd64.deb
```

The package manager installs the required libraries, eSpeak and icon theme.
Do not force installation if dependencies cannot be resolved.

## RPM — Fedora 44

On an x86_64 computer:

```sh
sudo dnf install ./lichen-0.1.0-preview6-1.fc44.x86_64.rpm
```

On an ARM64 device:

```sh
sudo dnf install ./lichen-0.1.0-preview6-1.fc44.aarch64.rpm
```

These are Fedora packages. Other RPM distributions require separate builds and testing.
They are not signed yet; verify downloaded file hashes against `SHA256SUMS`.

## AppImage — portable application bundle

For x86_64:

```sh
chmod +x Lichen-0.1.0-preview6-x86_64.AppImage
./Lichen-0.1.0-preview6-x86_64.AppImage
```

For ARM64, use `Lichen-0.1.0-preview6-aarch64.AppImage` in the same commands.
The AppImage requires glibc 2.41 or newer and OpenGL ES 3. If FUSE is unavailable,
add `--appimage-extract-and-run`. It includes eSpeak and application libraries;
location services, the display server and graphics drivers are provided by the system.

## Upgrading from Organic Maps (GTK)

Up to 0.1.0-preview5 this application was published as Organic Maps (GTK), package
`organicmaps-gtk`. Installing the `lichen` DEB or RPM replaces that package automatically.
For the AppImage, delete the old `OrganicMapsGtk-*.AppImage` file. Maps, settings and
bookmarks stay where they are and are used by Lichen.

## Application menu and previous SSH installation

Launch **Lichen** from the application menu after installing DEB or RPM.
If you previously installed over SSH, run this once as your normal user, without sudo:

```sh
/usr/bin/lichen-migrate-launcher
```

It backs up only the recognized old per-user launcher so the system launcher becomes
visible. It preserves custom launchers and does not delete maps, settings or bookmarks.
Package installation itself never modifies users' home directories.

## Verify downloads

Download `SHA256SUMS` beside the packages and run:

```sh
sha256sum --check --ignore-missing SHA256SUMS
```

Corresponding-source parts and the source RPM are for developers; installers do not
need them. See `SOURCES.md` and `LICENSES.md` for rebuilding and component licenses.
