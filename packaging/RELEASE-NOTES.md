# Lichen 0.1.0-prototype1 — CoMaps core prototype (not a release)

**This is a test build of the `comaps-core-prototype` branch, not a release.** It checks
whether Lichen can move from the Organic Maps core to the
[CoMaps](https://www.comaps.app) core. The decision to switch has not been made. Prototype
versions are never published as GitHub releases; the packages exist only as workflow
artifacts.

## Installs beside Lichen

The prototype has its own identity, so it can be installed together with the regular,
Organic Maps based Lichen without changing it:

- application ID `io.github.x_vlad_x.Lichen.Devel`, launcher name **Lichen Preview**,
  an icon with a red "DEV" band;
- command, DEB/RPM package and private directory `lichen-devel`;
- no Conflicts, Replaces, Obsoletes or Provides against `lichen` or `organicmaps-gtk`;
- maps in `~/.local/share/CoMaps`, settings and bookmarks in `~/.config/CoMaps` (the
  locations of the CoMaps desktop app). `~/.local/share/OrganicMaps` and
  `~/.config/OrganicMaps` are never read or written.

## Changes from the Organic Maps build

- The core is CoMaps at commit `47ae541a9fc02fa2cd11c8c9f42ab32b1997947b`. Map
  downloads come from the CoMaps servers; maps of the Organic Maps build are not shared.
- Interface strings come from the CoMaps Android translations. A few labels that CoMaps
  does not have are shown in English.
- Layers: hiking and cycling routes, driving and public transport details are CoMaps
  "map modes" (one at a time; walking is the default). Outdoors and contour lines stay
  layers. The large-font switch is a map label size of 100–400 %.
- Shared links use `https://comaps.at/`; the app opens `geo:`, `cm://` and `comaps://` links.
- The route optimization switch is gone: CoMaps always reorders a new intermediate stop.
- Attribution reads "Based on CoMaps" and "Map data © OpenStreetMap contributors".

## Validation and limitations

Container checks cover package installation and startup. They do not establish GPS,
hardware rendering, voice guidance or navigation on the phone.

The interface is Apache-2.0; the CoMaps core, libraries, fonts, voices and map data
retain their own licenses. See `LICENSES.md` and the package notices. This is an
independent unofficial interface and does not claim affiliation with or endorsement by
CoMaps.
