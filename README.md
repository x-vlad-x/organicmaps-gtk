# Organic Maps (GTK)

An unofficial native GNOME interface (GTK4 + libadwaita) for [Organic Maps](https://organicmaps.app),
made for Linux phones. Developed and tested on the FuriLabs FLX1s with FuriOS (Phosh).
An x86_64 AppImage is also tested on CachyOS. Other systems need separate testing;
package dependencies and supported versions are documented in [packaging/README.md](packaging/README.md).

## Download

**[First public preview — 0.1.0-preview4](https://github.com/x-vlad-x/organicmaps-gtk/releases/tag/v0.1.0-preview4)**

| Device | Download | Tested system |
|:--|:--|:--|
| Phone · ARM64 | [DEB package](https://github.com/x-vlad-x/organicmaps-gtk/releases/download/v0.1.0-preview4/organicmaps-gtk_0.1.0-preview4-1_arm64.deb) | FuriOS / FuriLabs FLX1s |
| Desktop · x86_64 | [AppImage](https://github.com/x-vlad-x/organicmaps-gtk/releases/download/v0.1.0-preview4/OrganicMapsGtk-0.1.0-preview4-x86_64.AppImage) | CachyOS · Wayland & X11 |

See the [installation instructions](packaging/INSTALL.md) and
[release notes](https://github.com/x-vlad-x/organicmaps-gtk/releases/tag/v0.1.0-preview4)
for requirements, checksums and preview limitations. The AppImage requires glibc 2.41 or newer
and OpenGL ES 3. Sources and license notices are available with the release.

## Interface

| | | | |
|:-:|:-:|:-:|:-:|
| ![Place page](docs/screenshots/place-page.webp) | ![Search](docs/screenshots/search.webp) | ![Route planning](docs/screenshots/route.webp) | ![Navigation](docs/screenshots/navigation.webp) |
| ![Night mode](docs/screenshots/night-mode.webp) | ![Map downloader](docs/screenshots/downloader.webp) | ![Map layers](docs/screenshots/layers.webp) | ![Settings](docs/screenshots/settings.webp) |

The core (map rendering, search, routing, map downloads, bookmarks) is the official Organic Maps code,
unchanged. The interface is new and follows the Android app:

- full-screen map rendered on the GPU (OpenGL ES 3 through GtkGLArea);
- Android-style touch gestures: drag, pinch, two-finger rotation, double tap, long tap (hides/shows
  the controls);
- bottom menu (Search / Route / Bookmarks and tracks / Menu), zoom and "My position" buttons;
- place page in a bottom sheet: type, address, opening hours ("Closed now • Opens at 10:00"), phone,
  website, Wikipedia, coordinates, Save / Route from / Route to / Share;
- search with categories, history, match highlighting and results on the map;
- routing: car, pedestrian, bicycle, public transport, ruler; intermediate stops, alternative routes,
  elevation profile; turn-by-turn navigation with lanes, speed limit, arrival time and voice guidance
  (espeak-ng, installed together with the app);
- map downloader: country tree, search, updates, "Download <region>" banner on the map;
- bookmarks and tracks: lists, bookmark editor (name, color, list, notes), KML/KMZ/GPX import,
  track recording;
- layers: outdoors, hiking and cycling routes, isolines, subway; night mode (the UI turns dark
  together with the map); settings as in the Android app;
- location through GeoClue, compass through iio-sensor-proxy;
- opens `geo:` and `om://` links and bookmark files.

The UI is translated into all languages Organic Maps supports: strings come from the same
translations as the Android and iOS apps (`data/strings/strings.txt`), the language follows the system
locale.

## Building and installing

Build recipes for `.deb` (ARM64 phone) and AppImage (x86_64 desktop), including source
collection and release preparation, are described in [packaging/README.md](packaging/README.md).
The original SSH installation method remains available below.

Requirements on the build machine (x86_64 Linux): git, podman, ImageMagick, rsync, python3, ssh.
The phone must be reachable over SSH with key authentication.

```bash
./update-organicmaps-gtk.sh            # build the latest Organic Maps release and install it on the phone
./update-organicmaps-gtk.sh --ui-only  # quickly update only the program (after changes in gtk-ui/)
./update-organicmaps-gtk.sh --check    # what is installed on the phone, is there an update
./update-organicmaps-gtk.sh --build-only
./update-organicmaps-gtk.sh --uninstall [--purge]
```

The SSH destination is taken from `PHONE` (default `furios`), e.g. `PHONE=furios@192.168.1.10`.

The script clones Organic Maps at the latest release tag into `organicmaps/`, applies the build patch
and cross-compiles for aarch64 in a Debian testing container (`build-env/`). The first build takes
about 15 minutes, later ones a couple of minutes. The result is in `out/`.

On the phone the program is installed to `~/.local/share/organicmaps-gtk`, maps go to
`~/.local/share/OrganicMaps`, settings and bookmarks to `~/.config/OrganicMaps` (the same locations
as the official Qt desktop app). No root access is needed.

## Project layout

- `gtk-ui/` — the interface, C++ on the plain GTK4/libadwaita C API: `map_view` (GtkGLArea and drape
  contexts on their own threads, touch input), `main_window`, `place_page`, `search`, `routing`,
  `downloader`, `bookmarks`, `settings`, `location` (GeoClue), `tts`, `l10n` (twine strings);
- `gtk-ui/icons/` — icons converted from the Android app (`tools/vector2svg.py`);
- `patches/` — the build patch for Organic Maps: hooks `gtk-ui/` into the CMake tree and takes the
  translated strings from the app instead of the stub;
- `build-env/` — container and CMake toolchain for cross-compilation;
- `packaging/` — launcher, desktop entry and icon;
- `tools/` — helper scripts.

## Debugging

With `OMGTK_DEBUG=1` the app accepts commands over D-Bus, which is handy for testing on the phone
without touching it (see `gtk-ui/debug.cpp` for the command list):

```bash
gdbus call --session --dest io.github.x_vlad_x.OrganicMapsGtk --object-path /io/github/x_vlad_x/OrganicMapsGtk \
  --method org.gtk.Actions.Activate debug "[<'tap 200 400'>]" "{}"
```

## License

Apache License 2.0, see [LICENSE](LICENSE) and [NOTICE](NOTICE).

This project is not affiliated with or endorsed by the Organic Maps project. It is built on the
[Organic Maps Project](https://organicmaps.app) code, icons and translations. Map data
© [OpenStreetMap](https://www.openstreetmap.org/copyright) contributors.
