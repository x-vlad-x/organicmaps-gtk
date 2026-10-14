# Licenses and sources supplied with the prototype

The GTK interface is distributed under Apache-2.0 (`LICENSE`, `NOTICE`). This
license does not replace the licenses of the CoMaps core, map data, icons,
translations, fonts or bundled libraries.

## CoMaps and map data

The core is [CoMaps](https://www.comaps.app), pinned in `tools/core.env` to commit
`47ae541a9fc02fa2cd11c8c9f42ab32b1997947b` of https://codeberg.org/comaps/comaps.
It is licensed under Apache-2.0; its `NOTICE` names My.com B.V. (Mail.Ru Group),
the Organic Maps Contributors and the CoMaps Contributors. The packages keep that
notice, the CoMaps `LICENSE`, `data/copyright.html`, `CONTRIBUTORS` and all
third-party license texts under `upstream/`. The build applies the included GTK
integration patch and links the bundled ICU, FreeType and HarfBuzz statically.

CoMaps has no separate data license. The world maps and the country maps downloaded
in the app are built from OpenStreetMap data, available under the Open Database
License (ODbL). "Based on CoMaps" and "Map data © OpenStreetMap contributors", with
links, are shown on the map, in the main menu and in About. Lichen is not affiliated
with or endorsed by CoMaps.

The world maps, drawing rules, symbols and generated strings are not stored in the
CoMaps repository. `tools/prepare-core.sh` downloads the world maps (checking the
sizes and SHA-1 digests listed in `data/countries.txt`) and generates the rest with
the core's own `configure.sh`. The source RPM contains these prepared files.

## Software in the packages

- The DEB and RPM dynamically use distribution-provided GTK, libadwaita, Qt Core/Network,
  GeoClue and eSpeak packages. Those dependencies are not embedded in the DEB.
- The AppImage includes these libraries and a separate eSpeak NG executable.
  They retain their own licenses, including LGPL and GPL. This is not a statement
  that the whole AppImage is licensed under Apache-2.0.
- The DEB and AppImage link the GCC C++ runtime statically. Its notices, source package versions,
  Built-Using records and GCC Runtime Library Exception are supplied separately.
- The Fedora RPM uses the system GCC runtime dynamically. The source RPM contains
  the interface, the patched and prepared CoMaps core and all recursive submodule
  sources, plus a spec file that rebuilds the package without downloading project sources.
- The AppImage runtime has its own MIT license and bundled third-party notices,
  including the LGPL libfuse license. `runtime-sources.json` pins its source commit
  and the source archives for libfuse and squashfuse used by its build scripts.

Inside the installed packages, see `usr/share/doc/lichen-devel` (or
`/usr/share/doc/lichen-devel` for the DEB). The `upstream`, `compiler-runtime`,
`bundled` and `appimage-runtime` directories contain the applicable notices and
license texts. The last two directories are specific to the AppImage.

## Obtaining and modifying sources

The release must provide the GTK source changes or source archive, the pinned
upstream revision, and the corresponding-sources archive beside the binaries.
The latter contains exact Debian `.dsc`, original source archives and Debian
patches, retrieved using SHA-256 from signed APT metadata, plus AppImage runtime
sources. The JSON inventories identify the source versions of the shipped files.

For Fedora, install the spec's build dependencies and rebuild the source RPM with
`rpmbuild --rebuild lichen-devel-<version>-1.fc44.src.rpm`. The source RPM is
shared by x86_64 and aarch64; build it natively on the target architecture.

Use `dpkg-source -x package_version.dsc` to reconstruct a Debian source package.
Its `debian/rules` and Debian build dependencies describe the distribution build.
Use the included GTK build recipes to rebuild the interface and its packages.
Container repository updates can change build dependencies: byte-for-byte
reproducibility is not claimed.

To modify bundled dynamic libraries, extract the AppImage with
`--appimage-extract`, replace compatible libraries in `squashfs-root/usr/lib`
(or the appropriate plugin subdirectory), and run `squashfs-root/AppRun`.
Repacking can use the pinned AppImage tools. No package license adds a prohibition
on modifying these libraries or reverse engineering needed to debug modifications
under their LGPL terms. The application imposes no signature lock on replacement
libraries. See each component's actual license for its terms.

The upstream runtime build scripts use Alpine packages for the permissively
licensed libc and compression libraries. Their notices are included; the pinned
runtime source and its libfuse/squashfuse source archives include the build and
patch scripts. This does not claim a bit-identical rebuild of the upstream runtime.
