# Lichen 0.1.0-preview6 — new name

Lichen is the new name of Organic Maps (GTK), an unofficial GTK interface for
Organic Maps on Linux phones and desktops.

## Packages

DEB, RPM and AppImage are built for both **x86_64** and **ARM64 (aarch64)**.
Select the package matching your distribution and CPU; the phone's ARM64 package
cannot run on an x86_64 computer. See `README.md` for installation commands.

## Changes

- The application is now called Lichen, with its own icon. The application ID is
  `io.github.x_vlad_x.Lichen`, the command and package are `lichen`.
- The `lichen` DEB and RPM replace an installed `organicmaps-gtk` package.
  Maps, settings and bookmarks stay in `~/.local/share/OrganicMaps` and
  `~/.config/OrganicMaps` and are used without changes.
- `lichen-migrate-launcher` also hides the launcher of an SSH installation made
  before the rename.

## Validation and limitations

Container checks cover package installation and startup. They do not establish GPS,
hardware rendering or navigation support on every device or distribution.

This remains a prerelease. The interface is Apache-2.0; libraries, fonts, voices and
map data retain their own licenses. See `LICENSES.md`, the package notices,
`SOURCES.md` and `SHA256SUMS`. This is an independent unofficial interface and does not
claim affiliation with or endorsement by the Organic Maps Project.
