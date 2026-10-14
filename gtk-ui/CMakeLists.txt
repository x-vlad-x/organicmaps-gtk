project(lichen)

find_package(PkgConfig REQUIRED)
pkg_check_modules(GTK_APP_DEPS REQUIRED IMPORTED_TARGET gtk4>=4.12 libadwaita-1>=1.6 libgeoclue-2.0)

file(GLOB SRC CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp ${CMAKE_CURRENT_SOURCE_DIR}/*.hpp)
# Only the original Debian cross-build needs the phone's older glibc symbol versions.
set(_phone_compat_default OFF)
if (CMAKE_CROSSCOMPILING AND CMAKE_SYSTEM_PROCESSOR MATCHES "^(aarch64|arm64)$")
  set(_phone_compat_default ON)
endif()
option(LICHEN_PHONE_GLIBC_COMPAT "Use the aarch64 phone glibc compatibility shim" ${_phone_compat_default})
if (LICHEN_PHONE_GLIBC_COMPAT)
  if (NOT CMAKE_SYSTEM_PROCESSOR MATCHES "^(aarch64|arm64)$")
    message(FATAL_ERROR "LICHEN_PHONE_GLIBC_COMPAT is only supported on aarch64")
  endif()
  list(APPEND SRC ${CMAKE_CURRENT_SOURCE_DIR}/glibc_compat.c)
endif()

get_filename_component(LICHEN_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/.." ABSOLUTE)
file(STRINGS "${LICHEN_ROOT}/VERSION" LICHEN_VERSION LIMIT_COUNT 1)
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${LICHEN_ROOT}/VERSION")

omim_add_executable(lichen ${SRC})
set_target_properties(lichen PROPERTIES UNITY_BUILD OFF)

target_compile_definitions(lichen PRIVATE
  QT_NO_KEYWORDS
  LICHEN_VERSION="${LICHEN_VERSION}"
  # Keep the supported API baseline usable with older distribution headers too.
  GDK_VERSION_MIN_REQUIRED=GDK_VERSION_4_12
  GDK_VERSION_MAX_ALLOWED=GDK_VERSION_4_12
  ADW_VERSION_MIN_REQUIRED=ADW_VERSION_1_6
  ADW_VERSION_MAX_ALLOWED=ADW_VERSION_1_6
)
target_compile_options(lichen PRIVATE -Wno-missing-field-initializers -Wno-unused-parameter)
target_link_libraries(lichen PRIVATE map PkgConfig::GTK_APP_DEPS)
# The core links its own static FreeType and HarfBuzz: don't export them, or they override the system
# ones Pango/cairo use (this build of FreeType has no PNG support, so color emoji disappear).
option(LICHEN_STATIC_COMPILER_RUNTIME "Link the compiler runtime statically" ON)
if (LICHEN_STATIC_COMPILER_RUNTIME)
  target_link_options(lichen PRIVATE -static-libstdc++ -static-libgcc)
endif()
target_link_options(lichen PRIVATE -Wl,--exclude-libs,ALL)

# Preserve the pinned core's version when rebuilding an RPM source archive without Git.
set(LICHEN_CORE_QT_VERSION "" CACHE STRING "Core version recorded by the source archive")
set(LICHEN_CORE_INT_VERSION "" CACHE STRING "Core integer version recorded by the source archive")
if (LICHEN_CORE_QT_VERSION AND LICHEN_CORE_INT_VERSION MATCHES "^[0-9]+$")
  set(OM_VERSION "${LICHEN_CORE_QT_VERSION}")
  set(OM_INT_VERSION "${LICHEN_CORE_INT_VERSION}")
  configure_file("${CMAKE_SOURCE_DIR}/libs/platform/platform_qt_version.cpp.in"
    "${CMAKE_BINARY_DIR}/libs/platform/platform_qt_version.cpp" @ONLY)
endif()

include("${LICHEN_ROOT}/packaging/install.cmake")
