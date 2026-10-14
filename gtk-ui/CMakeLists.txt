project(organicmaps_gtk)

find_package(PkgConfig REQUIRED)
pkg_check_modules(GTK_APP_DEPS REQUIRED IMPORTED_TARGET gtk4>=4.12 libadwaita-1>=1.6 libgeoclue-2.0)

file(GLOB SRC CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp ${CMAKE_CURRENT_SOURCE_DIR}/*.hpp)
# Only the original Debian cross-build needs the phone's older glibc symbol versions.
set(_phone_compat_default OFF)
if (CMAKE_CROSSCOMPILING AND CMAKE_SYSTEM_PROCESSOR MATCHES "^(aarch64|arm64)$")
  set(_phone_compat_default ON)
endif()
option(OMGTK_PHONE_GLIBC_COMPAT "Use the aarch64 phone glibc compatibility shim" ${_phone_compat_default})
if (OMGTK_PHONE_GLIBC_COMPAT)
  if (NOT CMAKE_SYSTEM_PROCESSOR MATCHES "^(aarch64|arm64)$")
    message(FATAL_ERROR "OMGTK_PHONE_GLIBC_COMPAT is only supported on aarch64")
  endif()
  list(APPEND SRC ${CMAKE_CURRENT_SOURCE_DIR}/glibc_compat.c)
endif()

get_filename_component(OMGTK_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/.." ABSOLUTE)
file(STRINGS "${OMGTK_ROOT}/VERSION" OMGTK_VERSION LIMIT_COUNT 1)
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${OMGTK_ROOT}/VERSION")

omim_add_executable(organicmaps-gtk ${SRC})
set_target_properties(organicmaps-gtk PROPERTIES UNITY_BUILD OFF)

target_compile_definitions(organicmaps-gtk PRIVATE
  QT_NO_KEYWORDS
  OMGTK_VERSION="${OMGTK_VERSION}"
  # Keep the supported API baseline usable with older distribution headers too.
  GDK_VERSION_MIN_REQUIRED=GDK_VERSION_4_12
  GDK_VERSION_MAX_ALLOWED=GDK_VERSION_4_12
  ADW_VERSION_MIN_REQUIRED=ADW_VERSION_1_6
  ADW_VERSION_MAX_ALLOWED=ADW_VERSION_1_6
)
target_compile_options(organicmaps-gtk PRIVATE -Wno-missing-field-initializers -Wno-unused-parameter)
target_link_libraries(organicmaps-gtk PRIVATE map PkgConfig::GTK_APP_DEPS)
# The core links its own static FreeType and HarfBuzz: don't export them, or they override the system
# ones Pango/cairo use (this build of FreeType has no PNG support, so color emoji disappear).
target_link_options(organicmaps-gtk PRIVATE -static-libstdc++ -static-libgcc -Wl,--exclude-libs,ALL)

include("${OMGTK_ROOT}/packaging/install.cmake")
