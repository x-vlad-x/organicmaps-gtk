# Organic Maps (GTK) 0.1.0-preview4 — first preview

An unofficial GTK interface for Organic Maps on Linux phones and desktops.

## Downloads

- **FuriOS phone / ARM64:** `organicmaps-gtk_0.1.0-preview4-1_arm64.deb`.
- **Linux desktop / x86_64:** `OrganicMapsGtk-0.1.0-preview4-x86_64.AppImage`.
- Installation and upgrade instructions: `README.md`.

## Included in this preview

- Offline maps, search, routing, bookmarks and map downloads based on Organic Maps.
- A standard draggable window header on desktop.
- Application icons at standard sizes and migration from the previous SSH launcher.
- Persistent map-data attribution links on the map, in the menu and in About.
- An independent interface version, license notices and corresponding source archives.

## Validation and limitations

Earlier previews were tested on FuriOS: startup, street-level maps without internet,
correct location and audible test speech. On CachyOS, startup, map rendering and
main screens were checked in Wayland and X11; window dragging was also confirmed.
These checks cover successive previews; the preview4 DEB has not yet been installed
on the physical phone.

The voice sounds robotic; tone improvements are deferred. Navigation while moving,
other distributions and RPM packages have not been separately tested.
The AppImage requires glibc 2.41 or newer and OpenGL ES 3. Please report bugs through
the project's GitHub Issues.

## Sources and licenses

The interface uses Apache-2.0. Libraries, voices, fonts and maps retain their own
licenses; see `LICENSES.md` and the notices inside the packages.
`OrganicMapsGtk-0.1.0-preview4-sources.tar.gz` contains the interface sources.
`OrganicMapsGtk-0.1.0-preview4-corresponding-sources.tar.part*` supplies the component
source archive in numbered parts. See `SOURCES.md` for download and reassembly
instructions. It includes exact Debian source packages and AppImage runtime sources.
Checksums are in `SHA256SUMS`.

Based on the [Organic Maps Project](https://organicmaps.app).
Map data © [OpenStreetMap](https://www.openstreetmap.org/copyright) and
[Organic Maps](https://organicmaps.app).
This is an independent unofficial port and does not claim affiliation or endorsement.
