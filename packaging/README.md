# Package builds

This branch builds **Lichen Preview**, the prototype on the CoMaps core: package and command
`lichen-devel`, application ID `io.github.x_vlad_x.Lichen.Devel`, installable beside `lichen`.

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

## Application icon

`io.github.x_vlad_x.Lichen.Devel.svg` is the icon source (the Lichen icon with a "DEV" band)
and is installed as the scalable icon. After changing it, re-render the 512 px PNG used for the
fixed-size icons and the AppImage:

```sh
rsvg-convert -w 512 -h 512 packaging/io.github.x_vlad_x.Lichen.Devel.svg -o packaging/io.github.x_vlad_x.Lichen.Devel.png
```

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
Set `LICHEN_BUILD_JOBS` to limit compiler processes (default: 8). Outputs are in
`out/packages/`; all build directories and downloads are ignored by Git.

The core is CoMaps, pinned in `tools/core.env` (the only place naming its repository and
commit) and checked out into `core/`. `tools/prepare-core.sh` initializes recursive
submodules, applies the GTK integration patch, downloads and verifies the world maps and
runs the core's `configure.sh` in `build-env/Containerfile.core` without network access to
generate drawing rules, symbols, strings and Boost headers. It refuses to switch an existing
checkout to another revision. Containers use distribution repositories, so the recipe does
not claim bit-for-bit reproducibility across dependency updates.

AppImage tools for both architectures are checksum-pinned in `appimage-tools.json`.
A changed upstream download fails verification; review it before updating the lock.
Libraries, Qt plugins, GIO modules, icons, schemas, translations and eSpeak data
are collected from the target architecture's Debian environment.

## RPM and source RPM

`packaging/lichen-devel.spec` is expanded with the project version and built
inside Fedora 44. It compiles the interface and pinned core, installs only the
Lichen component, and runs desktop metadata and version checks.
RPM generates ELF library dependencies automatically; eSpeak and hicolor icons
are explicit requirements. Installation uses the distribution's normal icon and
desktop database triggers and never modifies users' home directories.

The RPM uses the system compiler runtime dynamically. DEB/AppImage retain their
static GCC runtime and its source/copyright inventories. No Debian-built binary
or Debian library names are repackaged into the Fedora RPM.

The x86_64 RPM build also produces an architecture-independent source RPM containing
all interface files, the patched and prepared core (with the world maps and generated
data), recursive submodule sources and a concrete spec file. Git metadata, private local notes and build output are excluded.
To rebuild it on either architecture in Fedora 44, install the spec's build
dependencies (listed in `build-env/Containerfile.rpm`) and run:

```sh
rpmbuild --rebuild lichen-devel-0.1.0-prototype1-1.fc44.src.rpm
```

The source RPM contains project sources; it does not need to clone repositories.
Distribution build dependencies must already be installed. RPMs are unsigned for
now; checksums identify release files but are not a signing key.

## GitHub Actions

[Checks](../.github/workflows/checks.yml) validates packaging tests, scripts and
metadata on `main` pushes and pull requests. [Packages](../.github/workflows/packages.yml)
builds all six targets independently. ARM64 RPM/AppImage use native
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

Prototype versions (`-prototypeN`) are never published: the draft step refuses them.
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
  out/packages/appimage-amd64/LichenPreview.AppDir/usr/share/doc/lichen-devel/bundled/bundled-packages.json \
  out/packages/appimage-amd64/LichenPreview.AppDir/usr/share/doc/lichen-devel/compiler-runtime/sources.json \
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
