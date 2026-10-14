# Installing the preview

These are preview packages for **0.1.0-preview4**.

The map window has a standard draggable header. The packages include application
icons at standard sizes, a migration command for the previous SSH launcher, and
persistent links to Organic Maps and OpenStreetMap. Release assets also include
licenses and the corresponding sources of bundled components.

## FuriOS phone

1. Copy `organicmaps-gtk_0.1.0-preview4-1_arm64.deb` to the phone.
2. Close any running version of Organic Maps GTK.
3. Open a terminal in the directory containing the file and run:

```sh
sudo apt install ./organicmaps-gtk_0.1.0-preview4-1_arm64.deb
```

Open **Organic Maps (GTK)** from the application list. To launch the system
package explicitly, run `/usr/bin/organicmaps-gtk`.

If the previous SSH installation is still present, its user launcher may take
precedence over the system launcher. Run this once **without sudo**:

```sh
/usr/bin/organicmaps-gtk-migrate-launcher
```

This backs up the recognized old launcher and makes the packaged launcher
available. Maps, bookmarks, settings and the old application files are preserved.
Repeating the command is harmless. Custom desktop entries are left unchanged
with an explanatory message.

If installation reports missing dependencies, include the error text in your
bug report. Do not force installation with unresolved dependencies.

## Desktop

Run `OrganicMapsGtk-0.1.0-preview4-x86_64.AppImage`. If necessary, make it executable:

```sh
chmod +x OrganicMapsGtk-0.1.0-preview4-x86_64.AppImage
./OrganicMapsGtk-0.1.0-preview4-x86_64.AppImage
```

If FUSE mounting is unavailable, try:

```sh
./OrganicMapsGtk-0.1.0-preview4-x86_64.AppImage --appimage-extract-and-run
```

The AppImage requires glibc 2.41 or newer and OpenGL ES 3. Startup, map rendering,
menus, About, the map downloader and clean shutdown were checked on CachyOS in
Wayland and X11. Other distributions need separate testing.

## Known limitations and feedback

Earlier previews were tested on FuriOS and CachyOS. The phone checks confirmed
street-level map display without internet, correct location and audible speech.
The synthesized voice sounds robotic; improving its tone is deferred. Navigation
while moving has not been separately checked. The preview4 DEB has not yet been
installed on the physical phone.

Report the action that failed, the package version and any error message in the
project's GitHub Issues. RPM packages are not part of this preview.
