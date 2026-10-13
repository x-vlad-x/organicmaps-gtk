# Organic Maps (GTK)

An unofficial native GNOME interface (GTK4 + libadwaita) for [Organic Maps](https://organicmaps.app),
made for Linux phones. Developed and tested on the FuriLabs FLX1s with FuriOS (Phosh), but it should
work on any aarch64 phone or tablet running GTK 4.12+ and libadwaita 1.6+.

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
