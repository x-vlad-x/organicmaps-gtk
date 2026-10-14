%global app_version @APP_VERSION@
%global debug_package %{nil}
# The pinned core combines generated C/assembly archives that fail Fedora's default LTO link.
# Keep the remaining distribution hardening flags.
%global _lto_cflags %{nil}

Name:           organicmaps-gtk
Version:        @RPM_VERSION@
Release:        1%{?dist}
Summary:        Unofficial GTK interface for offline maps and navigation
License:        Apache-2.0 AND BSD-2-Clause AND BSD-3-Clause AND BSD-Protection AND BSL-1.0 AND FTL AND MIT AND SGI-B-2.0 AND Zlib AND OFL-1.1 AND Unicode-3.0 AND ODbL-1.0
URL:            https://github.com/x-vlad-x/organicmaps-gtk
Source0:        OrganicMapsGtk-%{app_version}-full-sources.tar.gz
BuildRequires:  gcc-c++ cmake ninja-build pkgconf-pkg-config python3 git
BuildRequires:  gtk4-devel libadwaita-devel geoclue2-devel qt6-qtbase-devel
BuildRequires:  libglvnd-devel libepoxy-devel ImageMagick desktop-file-utils
BuildRequires:  libXrandr-devel libXinerama-devel libXcursor-devel libXi-devel
Requires:       espeak-ng hicolor-icon-theme
# libepoxy loads the GLES/EGL entry points dynamically, outside automatic ELF dependencies.
Requires:       libglvnd-gles%{?_isa} libglvnd-egl%{?_isa}
Recommends:     geoclue2 iio-sensor-proxy

%description
An independent GTK interface for the Organic Maps core, with offline maps,
search, routing, bookmarks and map downloads for Linux phones and desktops.
This package does not claim affiliation with or endorsement by Organic Maps.

%prep
%autosetup -n OrganicMapsGtk-%{app_version}

%build
python3 tools/prepare_package_assets.py organicmaps build-icons /usr/bin/magick
core_version="$(python3 -c 'import json; print(json.load(open("SOURCE-REVISION.json"))["core_qt_version"])')"
core_int_version="$(python3 -c 'import json; print(json.load(open("SOURCE-REVISION.json"))["core_int_version"])')"
cmake -S organicmaps -B build -G Ninja -DCMAKE_BUILD_TYPE=Release \
    -DSKIP_QT_GUI=ON -DBUILD_TESTING=OFF -DOMIM_GTK_APP_DIR="$PWD/gtk-ui" \
    -DOMGTK_PREPARED_ICONS="$PWD/build-icons" -DCMAKE_INSTALL_PREFIX=/usr \
    -DOMGTK_STATIC_COMPILER_RUNTIME=OFF -DOMGTK_PHONE_GLIBC_COMPAT=OFF \
    -DOMGTK_CORE_QT_VERSION="$core_version" -DOMGTK_CORE_INT_VERSION="$core_int_version"
cmake --build build --target organicmaps-gtk -- %{?_smp_mflags}

%install
DESTDIR=%{buildroot} cmake --install build --component OrganicMapsGtk --prefix /usr --strip
python3 tools/prepare_rpm_metadata.py %{buildroot}

%check
desktop-file-validate %{buildroot}%{_datadir}/applications/io.github.x_vlad_x.OrganicMapsGtk.desktop
test "$(%{buildroot}%{_libexecdir}/organicmaps-gtk/bin/organicmaps-gtk --version)" = "%{app_version}"

%files
%defattr(-,root,root,-)
%{_bindir}/organicmaps-gtk
%{_bindir}/organicmaps-gtk-migrate-launcher
%{_libexecdir}/organicmaps-gtk/
%{_datadir}/applications/io.github.x_vlad_x.OrganicMapsGtk.desktop
%{_datadir}/icons/hicolor/*/apps/io.github.x_vlad_x.OrganicMapsGtk.png
%license %{_docdir}/organicmaps-gtk/LICENSE
%license %{_docdir}/organicmaps-gtk/NOTICE
%doc %{_docdir}/organicmaps-gtk/LICENSES.md
%doc %{_docdir}/organicmaps-gtk/upstream/

%changelog
* Wed Oct 14 2026 Vladislav Shadiuk <6992531+x-vlad-x@users.noreply.github.com> - 0.1.0~preview5-1
- Add native Fedora packages for x86_64 and aarch64.
