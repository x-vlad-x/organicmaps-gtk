# Install only this component when packaging the GTK app, not the upstream tools.
set(LICHEN_PRIVATE_DIR libexec/lichen)
set(LICHEN_COMPONENT Lichen)
set(LICHEN_APP_ID io.github.x_vlad_x.Lichen)

set(LICHEN_PREPARED_ICONS "" CACHE PATH "Navigation icons prepared on the build host")
if (LICHEN_PREPARED_ICONS)
  set(LICHEN_GENERATED_ICONS "${LICHEN_PREPARED_ICONS}")
  if (NOT EXISTS "${LICHEN_GENERATED_ICONS}/.stamp")
    message(FATAL_ERROR "Prepare navigation icons with tools/prepare_package_assets.py first")
  endif()
else()
  find_program(LICHEN_MAGICK magick REQUIRED)
  file(GLOB LICHEN_ANDROID_ICONS CONFIGURE_DEPENDS
    "${CMAKE_SOURCE_DIR}/android/sdk/src/main/res/drawable-xxxhdpi/ic_turn_*.webp"
    "${CMAKE_SOURCE_DIR}/android/sdk/src/main/res/drawable-xxxhdpi/ic_exit_highway_to_*.webp"
    "${CMAKE_SOURCE_DIR}/android/sdk/src/main/res/drawable/ic_roundabout_exit_*.xml"
    "${CMAKE_SOURCE_DIR}/android/sdk/src/main/res/drawable/ic_turn_round.xml")
  set(LICHEN_GENERATED_ICONS "${CMAKE_CURRENT_BINARY_DIR}/package-icons")
  add_custom_command(OUTPUT "${LICHEN_GENERATED_ICONS}/.stamp"
    COMMAND "${Python3_EXECUTABLE}" "${LICHEN_ROOT}/tools/prepare_package_assets.py"
            "${CMAKE_SOURCE_DIR}" "${LICHEN_GENERATED_ICONS}" "${LICHEN_MAGICK}"
    DEPENDS ${LICHEN_ANDROID_ICONS} "${LICHEN_ROOT}/tools/prepare_package_assets.py"
            "${LICHEN_ROOT}/packaging/${LICHEN_APP_ID}.png"
            "${LICHEN_ROOT}/packaging/${LICHEN_APP_ID}.svg"
            "${LICHEN_ROOT}/tools/vector2svg.py"
    VERBATIM)
  add_custom_target(lichen-assets DEPENDS "${LICHEN_GENERATED_ICONS}/.stamp")
  add_dependencies(lichen lichen-assets)
endif()

install(TARGETS lichen RUNTIME DESTINATION "${LICHEN_PRIVATE_DIR}/bin" COMPONENT ${LICHEN_COMPONENT})
install(PROGRAMS "${LICHEN_ROOT}/packaging/lichen" DESTINATION bin COMPONENT ${LICHEN_COMPONENT})
install(PROGRAMS "${LICHEN_ROOT}/packaging/lichen-migrate-launcher"
  DESTINATION bin COMPONENT ${LICHEN_COMPONENT})
install(FILES "${LICHEN_ROOT}/packaging/${LICHEN_APP_ID}.desktop"
  DESTINATION share/applications COMPONENT ${LICHEN_COMPONENT})
install(DIRECTORY "${LICHEN_GENERATED_ICONS}/hicolor/"
  DESTINATION share/icons/hicolor COMPONENT ${LICHEN_COMPONENT})
install(DIRECTORY "${LICHEN_ROOT}/gtk-ui/icons/" "${LICHEN_GENERATED_ICONS}/"
  DESTINATION "${LICHEN_PRIVATE_DIR}/icons" COMPONENT ${LICHEN_COMPONENT}
  PATTERN ".stamp" EXCLUDE PATTERN hicolor EXCLUDE)
install(FILES "${LICHEN_ROOT}/packaging/${LICHEN_APP_ID}.png"
  DESTINATION "${LICHEN_PRIVATE_DIR}/icons" COMPONENT ${LICHEN_COMPONENT})
install(FILES "${LICHEN_ROOT}/VERSION" DESTINATION "${LICHEN_PRIVATE_DIR}" COMPONENT ${LICHEN_COMPONENT})
install(DIRECTORY "${CMAKE_SOURCE_DIR}/data/" DESTINATION "${LICHEN_PRIVATE_DIR}/data" COMPONENT ${LICHEN_COMPONENT}
  PATTERN borders EXCLUDE PATTERN test_data EXCLUDE PATTERN styles EXCLUDE
  PATTERN symbols-svg EXCLUDE PATTERN generator EXCLUDE PATTERN vulkan_shaders EXCLUDE
  PATTERN "minsk-pass*" EXCLUDE PATTERN "drules_*.txt" EXCLUDE PATTERN drules_design.bin EXCLUDE
  PATTERN "drules_merged.*" EXCLUDE PATTERN "mapcss-*" EXCLUDE PATTERN taginfo.json EXCLUDE
  PATTERN "*.csv" EXCLUDE PATTERN "*.md" EXCLUDE PATTERN config.xsd EXCLUDE)
install(FILES "${LICHEN_ROOT}/LICENSE" "${LICHEN_ROOT}/NOTICE" "${LICHEN_ROOT}/packaging/LICENSES.md"
  DESTINATION share/doc/lichen COMPONENT ${LICHEN_COMPONENT})
# Preserve upstream notices and license texts, including statically linked libraries.
file(GLOB_RECURSE LICHEN_UPSTREAM_LICENSES RELATIVE "${CMAKE_SOURCE_DIR}"
  "${CMAKE_SOURCE_DIR}/*LICENSE*" "${CMAKE_SOURCE_DIR}/*LICENCE*"
  "${CMAKE_SOURCE_DIR}/*COPYING*" "${CMAKE_SOURCE_DIR}/*NOTICE*")
foreach(_license IN LISTS LICHEN_UPSTREAM_LICENSES)
  if (NOT _license MATCHES "(^|/)\\.git/")
    get_filename_component(_license_dir "${_license}" DIRECTORY)
    install(FILES "${CMAKE_SOURCE_DIR}/${_license}"
      DESTINATION "share/doc/lichen/upstream/${_license_dir}" COMPONENT ${LICHEN_COMPONENT})
  endif()
endforeach()
install(FILES "${CMAKE_SOURCE_DIR}/.reuse/dep5" "${CMAKE_SOURCE_DIR}/data/copyright.html"
  DESTINATION share/doc/lichen/upstream COMPONENT ${LICHEN_COMPONENT})
