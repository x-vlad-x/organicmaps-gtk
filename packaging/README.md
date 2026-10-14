# Package builds

The project builds **DEB, RPM and AppImage for both x86_64 and ARM64**. The package
architecture is verified from the ELF executable, package metadata and build manifest.
Local recipes do not install on the host, copy files to a phone or publish a release.

| Target | Build environment | Intended runtime |
| --- | --- | --- |
| `deb-arm64` | Debian testing cross-build on x86_64 | FuriOS ARM64 |
| `deb-amd64` | Native Debian 13 | Debian 13 / compatible amd64 systems |
| `appimage-amd64` | Native Debian 13 x86_64 | glibc 2.41+ and system graphics libraries |
| `appimage-arm64` | Native Debian 13 aarch64 | glibc 2.41+ and system graphics libraries |
| `rpm-amd64` | Native Fedora 44 x86_64 | Fedora 44 x86_64 |
| `rpm-arm64` | Native Fedora 44 aarch64 | Fedora 44 aarch64 |

Other RPM distributions need separate builds and tests. The ARM64 phone DEB retains
the original cross toolchain and glibc compatibility shim; RPM/AppImage do not use it.

## Build locally

Use an x86_64 or aarch64 Linux host with Podman, Git, Python 3 and ImageMagick
(`magick` or ImageMagick 6 `convert`). From the project root:

```sh
./tools/build-packages.sh all             # all three formats for the host CPU
./tools/build-packages.sh rpm             # Fedora RPM for the host CPU
./tools/build-packages.sh deb-amd64        # explicit format and architecture
./tools/build-packages.sh appimage-arm64   # run on an ARM64 build host
./tools/build-packages.sh deb-arm64        # original cross-build on an x86_64 host
```

The `phone` and `desktop` aliases still mean `deb-arm64` and `appimage-amd64`.
Set `OMGTK_BUILD_JOBS` to limit compiler processes (default: 8). Outputs are in
`out/packages/`; all build directories and downloads are ignored by Git.

The core is pinned to tag `2026.09.29-32-android`, commit
`b20d1417eef1e3c63a447ac8658db4307a066024`, with the included GTK integration patch.
The script initializes recursive submodules and refuses to switch an existing
checkout to another revision. Containers use distribution repositories, so the
recipe does not claim bit-for-bit reproducibility across dependency updates.

AppImage tools for both architectures are checksum-pinned in `appimage-tools.json`.
A changed upstream download fails verification; review it before updating the lock.
Libraries, Qt plugins, GIO modules, icons, schemas, translations and eSpeak data
are collected from the target architecture's Debian environment.

## RPM and source RPM

`packaging/organicmaps-gtk.spec` is expanded with the project version and built
inside Fedora 44. It compiles the interface and pinned core, installs only the
OrganicMapsGtk component, and runs desktop metadata and version checks.
RPM generates ELF library dependencies automatically; eSpeak and hicolor icons
are explicit requirements. Installation uses the distribution's normal icon and
desktop database triggers and never modifies users' home directories.

The RPM uses the system compiler runtime dynamically. DEB/AppImage retain their
static GCC runtime and its source/copyright inventories. No Debian-built binary
or Debian library names are repackaged into the Fedora RPM.

The x86_64 RPM build also produces an architecture-independent source RPM containing
all interface files, the patched core, recursive submodule sources and a concrete
spec file. Git metadata, private local notes and build output are excluded.
To rebuild it on either architecture in Fedora 44, install the spec's build
dependencies (listed in `build-env/Containerfile.rpm`) and run:

```sh
rpmbuild --rebuild organicmaps-gtk-0.1.0-preview5-1.fc44.src.rpm
```

The source RPM contains project sources; it does not need to clone repositories.
Distribution build dependencies must already be installed. RPMs are unsigned for
now; checksums identify release files but are not a signing key.

## Flatpak

`packaging/flatpak/io.github.x_vlad_x.OrganicMapsGtk.json` builds the application on the
GNOME 51 runtime for x86_64 and aarch64. The runtime provides GTK, libadwaita and
GeoClue; the manifest builds a minimal Qt 6 (Core and Network, used by the core),
pcaudiolib and eSpeak NG, then the pinned core with the included patches.
Navigation and application icons are converted with the SDK's libwebp tools
(`cwebp`, `dwebp`), so ImageMagick is not needed inside the SDK.

Build and install locally with `flatpak-builder` 1.4 or newer:

```sh
flatpak remote-add --user --if-not-exists flathub https://dl.flathub.org/repo/flathub.flatpakrepo
flatpak-builder --user --install-deps-from=flathub --install --force-clean \
  build-flatpak packaging/flatpak/io.github.x_vlad_x.OrganicMapsGtk.json
flatpak run io.github.x_vlad_x.OrganicMapsGtk
```

The sandbox has network access for map downloads, Wayland with X11 fallback, GPU
and PulseAudio for voice guidance. Location goes through the XDG location portal
(libgeoclue selects it automatically inside Flatpak); the compass needs read access
to iio-sensor-proxy on the system bus. Files are imported through the file chooser
portal. Maps, settings and bookmarks live in `~/.var/app/io.github.x_vlad_x.OrganicMapsGtk/`
and are not shared with DEB, RPM or AppImage installations.

The repository manifest builds the working tree. For a Flathub submission, generate
the variant that builds the release tag from Git, pinned to its commit:

```sh
python3 tools/flatpak_manifest.py flathub --output io.github.x_vlad_x.OrganicMapsGtk.json
```

`tools/flatpak_manifest.py check` (also part of `tools/ci_release.py check`) keeps the
manifest's core pin and version options equal to `tools/build-packages.sh`, and requires
the AppStream metadata to list `VERSION` as its newest release. When releasing, add the
new `<release>` entry to `packaging/io.github.x_vlad_x.OrganicMapsGtk.metainfo.xml`.
Its screenshots are JPEG copies in `docs/screenshots/appstream/`, referenced from `main`.

## GitHub Actions

[Checks](../.github/workflows/checks.yml) validates packaging tests, scripts and
metadata on `main` pushes and pull requests. [Packages](../.github/workflows/packages.yml)
builds all six targets independently. [Flatpak](../.github/workflows/flatpak.yml)
runs the Flathub linter on the manifest and builds bundles for both architectures
in the Flathub build container on tags and manual runs. ARM64 RPM/AppImage use native
`ubuntu-24.04-arm` runners; their containers pull the native aarch64 images.

The pipeline checks:

- the executable's actual architecture, package version and build commit;
- all installed application icon sizes and desktop metadata;
- exact corresponding Debian sources for AppImage and static compiler runtimes;
- installation, dependency resolution and `--version` in clean Fedora containers
  for **both RPM architectures**, plus clean Debian installation for amd64 DEB;
- unprivileged RPM GUI startup, desktop icon lookup and OpenGL ES 3 map rendering
  on an isolated software display, with screenshots and logs retained as CI artifacts;
- AppImage startup without FUSE or a display in separate Debian 13 runtime
  containers with system graphics/font libraries and no preinstalled GTK or Qt;
- source completeness, source RPM inclusion and SHA-256 for every release asset.

The RPM graphics checks use software rendering in Xvfb. They do not replace
physical GPU, GPS or navigation tests on devices.

Use **Actions → Packages → Run workflow** to build without creating a release.
The `release-files` artifact contains a tar archive of all six installers,
source RPM, source archives/parts, instructions, inventories and `SHA256SUMS`.
Intermediate artifacts are retained for 7 days; the final artifact for 14 days.

For a release, update `VERSION`, release notes and installation examples, commit,
and push a matching `v<VERSION>` tag. After every target passes, the workflow
creates a draft and checks uploaded asset hashes and sizes. Review and publish
that draft when ready. Manual runs never create releases, including runs on tags.
An existing release is never overwritten. An interrupted first upload leaves a
draft to inspect and remove before rerunning the failed draft job.

## Prepare release files locally

Rebuild all six targets from the same clean commit. For each DEB/AppImage stage,
collect its compiler inventory and (for AppImage) bundled library inventory:

```sh
python3 tools/collect_debian_sources.py \
  out/packages/appimage-amd64/OrganicMapsGtk.AppDir/usr/share/doc/organicmaps-gtk/bundled/bundled-packages.json \
  out/packages/appimage-amd64/OrganicMapsGtk.AppDir/usr/share/doc/organicmaps-gtk/compiler-runtime/sources.json \
  --container localhost/om-desktop --output out/corresponding-sources/appimage-amd64
```

Use the matching stage and container for the other three Debian-based targets.
`tools/ci-build.sh FORMAT-ARCH` performs the complete build, collection and checks
on the native CI runner and prepares a handoff for the assembly job.

`python3 tools/prepare_release.py` requires all six package manifests to match HEAD,
checks exact source coverage, deduplicates component sources shared by architectures,
and prepares `out/release/<version>/`. The component source archive is split into
64 MiB parts; `SOURCES.md` explains reassembly. Private `.local.md` files are rejected.
The tool never creates a tag, pushes code or publishes. See [LICENSES.md](LICENSES.md).
