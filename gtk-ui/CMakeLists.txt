project(organicmaps_gtk)

find_package(PkgConfig REQUIRED)
pkg_check_modules(GTK_APP_DEPS REQUIRED IMPORTED_TARGET gtk4 libadwaita-1 libgeoclue-2.0)

file(GLOB SRC CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp ${CMAKE_CURRENT_SOURCE_DIR}/*.hpp ${CMAKE_CURRENT_SOURCE_DIR}/*.c)

omim_add_executable(organicmaps-gtk ${SRC})
set_target_properties(organicmaps-gtk PROPERTIES UNITY_BUILD OFF)

target_compile_definitions(organicmaps-gtk PRIVATE
  QT_NO_KEYWORDS
  # The phone runs GTK 4.22 and libadwaita 1.9: warn about anything newer.
  GDK_VERSION_MIN_REQUIRED=GDK_VERSION_4_12
  GDK_VERSION_MAX_ALLOWED=GDK_VERSION_4_22
  ADW_VERSION_MIN_REQUIRED=ADW_VERSION_1_6
  ADW_VERSION_MAX_ALLOWED=ADW_VERSION_1_9
)
target_compile_options(organicmaps-gtk PRIVATE -Wno-missing-field-initializers -Wno-unused-parameter)
target_link_libraries(organicmaps-gtk PRIVATE map PkgConfig::GTK_APP_DEPS)
# The core links its own static FreeType and HarfBuzz: don't export them, or they override the system
# ones Pango/cairo use (this build of FreeType has no PNG support, so color emoji disappear).
target_link_options(organicmaps-gtk PRIVATE -static-libstdc++ -static-libgcc -Wl,--exclude-libs,ALL)

install(TARGETS organicmaps-gtk DESTINATION bin)
