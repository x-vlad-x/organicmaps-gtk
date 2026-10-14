# Organic Maps (GTK) 0.1.0-preview5 — RPM and two architectures

An unofficial GTK interface for Organic Maps on Linux phones and desktops.

## Packages

DEB, RPM and AppImage are built for both **x86_64** and **ARM64 (aarch64)**.
Select the package matching your distribution and CPU; the phone's ARM64 package
cannot run on an x86_64 computer. See `README.md` for installation commands.

- DEB: ARM64 for FuriOS; amd64 for Debian 13 and compatible systems.
- RPM: native Fedora 44 builds for x86_64 and aarch64, with a rebuildable source RPM.
- AppImage: x86_64 and aarch64, with glibc 2.41 or newer and OpenGL ES 3.

## Changes

- Native RPM packaging with automatic library dependencies and standard desktop icons.
- A complete source RPM containing the interface, core, patches and recursive submodules.
- Six package builds in GitHub Actions, with ARM64 RPM/AppImage built on native ARM runners.
- Package installation checks in clean Debian/Fedora containers and AppImage startup
  checks in separate Debian 13 runtime containers.
- Architecture-specific library/plugin collection and pinned tools for both AppImages.
- Corrected screenshot URLs in the previous preview's release description.

## Validation and limitations

The build pipeline verifies executable architecture, package version, installed icons,
source provenance and corresponding component sources before preparing release files.
Container checks cover package installation and startup. They do not establish GPS,
hardware rendering or navigation support on every device or distribution.

Earlier phone builds were checked for startup, street maps without internet,
correct location and audible speech. Desktop map rendering and main screens were
checked in Wayland and X11. The synthesized voice still sounds robotic.

This remains a prerelease. The interface is Apache-2.0; libraries, fonts, voices and
map data retain their own licenses. See `LICENSES.md`, the package notices,
`SOURCES.md` and `SHA256SUMS`. This is an independent unofficial port and does not
claim affiliation with or endorsement by the Organic Maps Project.
