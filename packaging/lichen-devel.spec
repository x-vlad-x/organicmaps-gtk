%global app_version @APP_VERSION@
%global debug_package %{nil}
# The pinned core combines generated C/assembly archives that fail Fedora's default LTO link.
# Keep the remaining distribution hardening flags.
%global _lto_cflags %{nil}

Name:           lichen-devel
Version:        @RPM_VERSION@
Release:        1%{?dist}
Summary:        Unofficial GTK interface for offline maps (CoMaps core prototype)
License:        Apache-2.0 AND BSD-2-Clause AND BSD-3-Clause AND BSD-Protection AND BSL-1.0 AND FTL AND MIT AND SGI-B-2.0 AND Zlib AND OFL-1.1 AND Unicode-3.0 AND ODbL-1.0
URL:            https://github.com/x-vlad-x/lichen
Source0:        LichenPreview-%{app_version}-full-sources.tar.gz
BuildRequires:  gcc-c++ cmake ninja-build pkgconf-pkg-config python3 git
BuildRequires:  gtk4-devel libadwaita-devel geoclue2-devel qt6-qtbase-devel
BuildRequires:  libglvnd-devel libepoxy-devel ImageMagick desktop-file-utils
BuildRequires:  libXrandr-devel libXinerama-devel libXcursor-devel libXi-devel
Requires:       espeak-ng hicolor-icon-theme
# libepoxy loads the GLES/EGL entry points dynamically, outside automatic ELF dependencies.
Requires:       libglvnd-gles%{?_isa} libglvnd-egl%{?_isa}
Recommends:     geoclue2 iio-sensor-proxy
# A prototype that installs beside lichen: no Obsoletes, Conflicts or Provides.

%description
Lichen Preview, a prototype of the Lichen GTK interface on the CoMaps core, with
offline maps, search, routing, bookmarks and map downloads for Linux phones and
desktops. It keeps its data in ~/.local/share/CoMaps and ~/.config/CoMaps and
installs beside the regular Lichen. Not a release.
This package does not claim affiliation with or endorsement by CoMaps.

%prep
%autosetup -n LichenPreview-%{app_version}

%build
# The source archive contains the core as prepared by tools/prepare-core.sh.
python3 tools/prepare_package_assets.py core build-icons /usr/bin/magick
core_version="$(python3 -c 'import json; print(json.load(open("SOURCE-REVISION.json"))["core_qt_version"])')"
core_int_version="$(python3 -c 'import json; print(json.load(open("SOURCE-REVISION.json"))["core_int_version"])')"
cmake -S core -B build -G Ninja -DCMAKE_BUILD_TYPE=Release \
    -DSKIP_QT_GUI=ON -DBUILD_TESTING=OFF -DOMIM_GTK_APP_DIR="$PWD/gtk-ui" \
    -DLICHEN_PREPARED_ICONS="$PWD/build-icons" -DCMAKE_INSTALL_PREFIX=/usr \
    -DLICHEN_STATIC_COMPILER_RUNTIME=OFF -DLICHEN_PHONE_GLIBC_COMPAT=OFF \
    -DLICHEN_CORE_QT_VERSION="$core_version" -DLICHEN_CORE_INT_VERSION="$core_int_version"
cmake --build build --target lichen -- %{?_smp_mflags}

%install
DESTDIR=%{buildroot} cmake --install build --component Lichen --prefix /usr --strip
python3 tools/prepare_rpm_metadata.py %{buildroot}

%check
desktop-file-validate %{buildroot}%{_datadir}/applications/io.github.x_vlad_x.Lichen.Devel.desktop
test "$(%{buildroot}%{_libexecdir}/lichen-devel/bin/lichen --version)" = "%{app_version}"

%files
%defattr(-,root,root,-)
%{_bindir}/lichen-devel
%{_libexecdir}/lichen-devel/
%{_datadir}/applications/io.github.x_vlad_x.Lichen.Devel.desktop
%{_datadir}/icons/hicolor/*/apps/io.github.x_vlad_x.Lichen.Devel.png
%{_datadir}/icons/hicolor/scalable/apps/io.github.x_vlad_x.Lichen.Devel.svg
%license %{_docdir}/lichen-devel/LICENSE
%license %{_docdir}/lichen-devel/NOTICE
%doc %{_docdir}/lichen-devel/LICENSES.md
%doc %{_docdir}/lichen-devel/upstream/

%changelog
* Wed Oct 14 2026 Vladislav Shadiuk <6992531+x-vlad-x@users.noreply.github.com> - 0.1.0~prototype1-1
- Prototype on the CoMaps core, packaged as lichen-devel beside lichen.

* Wed Oct 14 2026 Vladislav Shadiuk <6992531+x-vlad-x@users.noreply.github.com> - 0.1.0~preview6-1
- Rename the package from organicmaps-gtk to lichen.

* Wed Oct 14 2026 Vladislav Shadiuk <6992531+x-vlad-x@users.noreply.github.com> - 0.1.0~preview5-1
- Add native Fedora packages for x86_64 and aarch64.
