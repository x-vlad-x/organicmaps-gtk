# Licenses and sources supplied with the preview

The GTK interface is distributed under Apache-2.0 (`LICENSE`, `NOTICE`). This
license does not replace the licenses of the Organic Maps core, map data, icons,
translations, fonts or bundled libraries.

## Organic Maps and map data

The core is pinned to `b20d1417eef1e3c63a447ac8658db4307a066024` and retains its
Apache-2.0 license, notices and third-party texts. The build applies the included
GTK integration patch. Attribution and links to the Organic Maps Project and
OpenStreetMap are shown on the map, in the main menu and in About.

The binary maps and other compiled data have the additional terms in upstream
`DATA_LICENSE.txt`. The preview retains the Organic Maps name and upstream icon,
identifies itself as an unofficial GTK interface, and does not claim endorsement.
A new name or replacement branding requires a separate review of the upstream
data-license rebranding clause; this preview does not perform that change.

## Software in the packages

- The DEB and RPM dynamically use distribution-provided GTK, libadwaita, Qt Core/Network,
  GeoClue and eSpeak packages. Those dependencies are not embedded in the DEB.
- The AppImage includes these libraries and a separate eSpeak NG executable.
  They retain their own licenses, including LGPL and GPL. This is not a statement
  that the whole AppImage is licensed under Apache-2.0.
- The DEB and AppImage link the GCC C++ runtime statically. Its notices, source package versions,
  Built-Using records and GCC Runtime Library Exception are supplied separately.
- The Fedora RPM uses the system GCC runtime dynamically. The source RPM contains
  the interface, patched Organic Maps core and all recursive submodule sources,
  plus a spec file that rebuilds the package without downloading project sources.
- The AppImage runtime has its own MIT license and bundled third-party notices,
  including the LGPL libfuse license. `runtime-sources.json` pins its source commit
  and the source archives for libfuse and squashfuse used by its build scripts.

Inside the installed packages, see `usr/share/doc/organicmaps-gtk` (or
`/usr/share/doc/organicmaps-gtk` for the DEB). The `upstream`, `compiler-runtime`,
`bundled` and `appimage-runtime` directories contain the applicable notices and
license texts. The last two directories are specific to the AppImage.

## Obtaining and modifying sources

The release must provide the GTK source changes or source archive, the pinned
upstream revision, and the corresponding-sources archive beside the binaries.
The latter contains exact Debian `.dsc`, original source archives and Debian
patches, retrieved using SHA-256 from signed APT metadata, plus AppImage runtime
sources. The JSON inventories identify the source versions of the shipped files.

For Fedora, install the spec's build dependencies and rebuild the source RPM with
`rpmbuild --rebuild organicmaps-gtk-<version>-1.fc44.src.rpm`. The source RPM is
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
