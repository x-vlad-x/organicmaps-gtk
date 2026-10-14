# Install only this component when packaging the GTK app, not the upstream tools.
set(OMGTK_PRIVATE_DIR libexec/organicmaps-gtk)
set(OMGTK_COMPONENT OrganicMapsGtk)
set(OMGTK_APP_ID io.github.x_vlad_x.OrganicMapsGtk)

set(OMGTK_PREPARED_ICONS "" CACHE PATH "Navigation icons prepared on the build host")
if (OMGTK_PREPARED_ICONS)
  set(OMGTK_GENERATED_ICONS "${OMGTK_PREPARED_ICONS}")
  if (NOT EXISTS "${OMGTK_GENERATED_ICONS}/.stamp")
    message(FATAL_ERROR "Prepare navigation icons with tools/prepare_package_assets.py first")
  endif()
else()
  find_program(OMGTK_MAGICK magick REQUIRED)
  file(GLOB OMGTK_ANDROID_ICONS CONFIGURE_DEPENDS
    "${CMAKE_SOURCE_DIR}/android/sdk/src/main/res/drawable-xxxhdpi/ic_turn_*.webp"
    "${CMAKE_SOURCE_DIR}/android/sdk/src/main/res/drawable-xxxhdpi/ic_exit_highway_to_*.webp"
    "${CMAKE_SOURCE_DIR}/android/sdk/src/main/res/drawable/ic_roundabout_exit_*.xml"
    "${CMAKE_SOURCE_DIR}/android/sdk/src/main/res/drawable/ic_turn_round.xml")
  set(OMGTK_GENERATED_ICONS "${CMAKE_CURRENT_BINARY_DIR}/package-icons")
  add_custom_command(OUTPUT "${OMGTK_GENERATED_ICONS}/.stamp"
    COMMAND "${Python3_EXECUTABLE}" "${OMGTK_ROOT}/tools/prepare_package_assets.py"
            "${CMAKE_SOURCE_DIR}" "${OMGTK_GENERATED_ICONS}" "${OMGTK_MAGICK}"
    DEPENDS ${OMGTK_ANDROID_ICONS} "${OMGTK_ROOT}/tools/prepare_package_assets.py"
            "${OMGTK_ROOT}/packaging/${OMGTK_APP_ID}.png"
            "${OMGTK_ROOT}/tools/vector2svg.py"
    VERBATIM)
  add_custom_target(organicmaps-gtk-assets DEPENDS "${OMGTK_GENERATED_ICONS}/.stamp")
  add_dependencies(organicmaps-gtk organicmaps-gtk-assets)
endif()

install(TARGETS organicmaps-gtk RUNTIME DESTINATION "${OMGTK_PRIVATE_DIR}/bin" COMPONENT ${OMGTK_COMPONENT})
install(PROGRAMS "${OMGTK_ROOT}/packaging/organicmaps-gtk" DESTINATION bin COMPONENT ${OMGTK_COMPONENT})
install(PROGRAMS "${OMGTK_ROOT}/packaging/organicmaps-gtk-migrate-launcher"
  DESTINATION bin COMPONENT ${OMGTK_COMPONENT})
install(FILES "${OMGTK_ROOT}/packaging/${OMGTK_APP_ID}.desktop"
  DESTINATION share/applications COMPONENT ${OMGTK_COMPONENT})
install(FILES "${OMGTK_ROOT}/packaging/${OMGTK_APP_ID}.metainfo.xml"
  DESTINATION share/metainfo COMPONENT ${OMGTK_COMPONENT})
install(DIRECTORY "${OMGTK_GENERATED_ICONS}/hicolor/"
  DESTINATION share/icons/hicolor COMPONENT ${OMGTK_COMPONENT})
install(DIRECTORY "${OMGTK_ROOT}/gtk-ui/icons/" "${OMGTK_GENERATED_ICONS}/"
  DESTINATION "${OMGTK_PRIVATE_DIR}/icons" COMPONENT ${OMGTK_COMPONENT}
  PATTERN ".stamp" EXCLUDE PATTERN hicolor EXCLUDE)
install(FILES "${OMGTK_ROOT}/packaging/${OMGTK_APP_ID}.png"
  DESTINATION "${OMGTK_PRIVATE_DIR}/icons" COMPONENT ${OMGTK_COMPONENT})
install(FILES "${OMGTK_ROOT}/VERSION" DESTINATION "${OMGTK_PRIVATE_DIR}" COMPONENT ${OMGTK_COMPONENT})
install(DIRECTORY "${CMAKE_SOURCE_DIR}/data/" DESTINATION "${OMGTK_PRIVATE_DIR}/data" COMPONENT ${OMGTK_COMPONENT}
  PATTERN borders EXCLUDE PATTERN test_data EXCLUDE PATTERN styles EXCLUDE
  PATTERN symbols-svg EXCLUDE PATTERN generator EXCLUDE PATTERN vulkan_shaders EXCLUDE
  PATTERN "minsk-pass*" EXCLUDE PATTERN "drules_*.txt" EXCLUDE PATTERN drules_design.bin EXCLUDE
  PATTERN "drules_merged.*" EXCLUDE PATTERN "mapcss-*" EXCLUDE PATTERN taginfo.json EXCLUDE
  PATTERN "*.csv" EXCLUDE PATTERN "*.md" EXCLUDE PATTERN config.xsd EXCLUDE)
install(FILES "${OMGTK_ROOT}/LICENSE" "${OMGTK_ROOT}/NOTICE" "${OMGTK_ROOT}/packaging/LICENSES.md"
  DESTINATION share/doc/organicmaps-gtk COMPONENT ${OMGTK_COMPONENT})
# Preserve upstream notices and license texts, including statically linked libraries.
file(GLOB_RECURSE OMGTK_UPSTREAM_LICENSES RELATIVE "${CMAKE_SOURCE_DIR}"
  "${CMAKE_SOURCE_DIR}/*LICENSE*" "${CMAKE_SOURCE_DIR}/*LICENCE*"
  "${CMAKE_SOURCE_DIR}/*COPYING*" "${CMAKE_SOURCE_DIR}/*NOTICE*")
foreach(_license IN LISTS OMGTK_UPSTREAM_LICENSES)
  if (NOT _license MATCHES "(^|/)\\.git/")
    get_filename_component(_license_dir "${_license}" DIRECTORY)
    install(FILES "${CMAKE_SOURCE_DIR}/${_license}"
      DESTINATION "share/doc/organicmaps-gtk/upstream/${_license_dir}" COMPONENT ${OMGTK_COMPONENT})
  endif()
endforeach()
install(FILES "${CMAKE_SOURCE_DIR}/.reuse/dep5" "${CMAKE_SOURCE_DIR}/data/copyright.html"
  DESTINATION share/doc/organicmaps-gtk/upstream COMPONENT ${OMGTK_COMPONENT})
