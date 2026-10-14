# Install only this component when packaging the GTK app, not the upstream tools.
# The CoMaps core prototype uses its own command, directories and application ID, so its package
# installs beside the regular Lichen.
set(LICHEN_PACKAGE lichen-devel)
set(LICHEN_PRIVATE_DIR libexec/${LICHEN_PACKAGE})
set(LICHEN_DOC_DIR share/doc/${LICHEN_PACKAGE})
set(LICHEN_COMPONENT Lichen)
set(LICHEN_APP_ID io.github.x_vlad_x.Lichen.Devel)

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
    "${CMAKE_SOURCE_DIR}/android/sdk/src/main/res/drawable-xxxhdpi/ic_exit_highway_to_*.webp")
  set(LICHEN_GENERATED_ICONS "${CMAKE_CURRENT_BINARY_DIR}/package-icons")
  add_custom_command(OUTPUT "${LICHEN_GENERATED_ICONS}/.stamp"
    COMMAND "${Python3_EXECUTABLE}" "${LICHEN_ROOT}/tools/prepare_package_assets.py"
            "${CMAKE_SOURCE_DIR}" "${LICHEN_GENERATED_ICONS}" "${LICHEN_MAGICK}"
    DEPENDS ${LICHEN_ANDROID_ICONS} "${LICHEN_ROOT}/tools/prepare_package_assets.py"
            "${LICHEN_ROOT}/packaging/${LICHEN_APP_ID}.png"
            "${LICHEN_ROOT}/packaging/${LICHEN_APP_ID}.svg"
    VERBATIM)
  add_custom_target(lichen-assets DEPENDS "${LICHEN_GENERATED_ICONS}/.stamp")
  add_dependencies(lichen lichen-assets)
endif()

install(TARGETS lichen RUNTIME DESTINATION "${LICHEN_PRIVATE_DIR}/bin" COMPONENT ${LICHEN_COMPONENT})
install(PROGRAMS "${LICHEN_ROOT}/packaging/${LICHEN_PACKAGE}" DESTINATION bin COMPONENT ${LICHEN_COMPONENT})
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
# CoMaps does not keep the world maps and generated styles in Git: tools/prepare-core.sh adds them.
foreach(_prepared World.mwm WorldCoasts.mwm drules_proto_default_light.bin symbols/xhdpi/light/symbols.png)
  if (NOT EXISTS "${CMAKE_SOURCE_DIR}/data/${_prepared}")
    message(FATAL_ERROR "data/${_prepared} is missing: prepare the core with tools/prepare-core.sh")
  endif()
endforeach()
install(DIRECTORY "${CMAKE_SOURCE_DIR}/data/" DESTINATION "${LICHEN_PRIVATE_DIR}/data" COMPONENT ${LICHEN_COMPONENT}
  PATTERN borders EXCLUDE PATTERN test_data EXCLUDE PATTERN styles EXCLUDE
  PATTERN symbols-svg EXCLUDE PATTERN generator EXCLUDE PATTERN vulkan_shaders EXCLUDE
  PATTERN translations EXCLUDE PATTERN world_mwm EXCLUDE
  PATTERN "minsk-pass*" EXCLUDE PATTERN "drules_*.txt" EXCLUDE PATTERN drules_design.bin EXCLUDE
  PATTERN "drules_merged.*" EXCLUDE PATTERN mapcss-dynamic.txt EXCLUDE PATTERN taginfo.json EXCLUDE
  PATTERN "*.diff" EXCLUDE PATTERN "*_hash" EXCLUDE PATTERN "*.prev" EXCLUDE
  # Map generator inputs; CoMaps reads subtypes.csv and mapcss-mapping.csv at run time.
  PATTERN borders_vs_osm.csv EXCLUDE PATTERN countries_synonyms.csv EXCLUDE PATTERN old_vs_new.csv EXCLUDE
  PATTERN "*.md" EXCLUDE PATTERN config.xsd EXCLUDE)
# UI translations: CoMaps keeps them in its Android resources (app strings, core strings and
# feature type names), one values-<locale> folder per language.
foreach(_part app sdk)
  file(GLOB _translations RELATIVE "${CMAKE_SOURCE_DIR}/android/${_part}/src/main/res"
    "${CMAKE_SOURCE_DIR}/android/${_part}/src/main/res/values*/strings.xml"
    "${CMAKE_SOURCE_DIR}/android/${_part}/src/main/res/values*/types_strings.xml")
  foreach(_file IN LISTS _translations)
    get_filename_component(_folder "${_file}" DIRECTORY)
    install(FILES "${CMAKE_SOURCE_DIR}/android/${_part}/src/main/res/${_file}"
      DESTINATION "${LICHEN_PRIVATE_DIR}/strings/${_part}/${_folder}" COMPONENT ${LICHEN_COMPONENT})
  endforeach()
endforeach()
install(FILES "${LICHEN_ROOT}/LICENSE" "${LICHEN_ROOT}/NOTICE" "${LICHEN_ROOT}/packaging/LICENSES.md"
  DESTINATION "${LICHEN_DOC_DIR}" COMPONENT ${LICHEN_COMPONENT})
# Preserve upstream notices and license texts, including statically linked libraries.
file(GLOB_RECURSE LICHEN_UPSTREAM_LICENSES RELATIVE "${CMAKE_SOURCE_DIR}"
  "${CMAKE_SOURCE_DIR}/*LICENSE*" "${CMAKE_SOURCE_DIR}/*LICENCE*"
  "${CMAKE_SOURCE_DIR}/*COPYING*" "${CMAKE_SOURCE_DIR}/*NOTICE*")
foreach(_license IN LISTS LICHEN_UPSTREAM_LICENSES)
  if (NOT _license MATCHES "(^|/)\\.git/")
    get_filename_component(_license_dir "${_license}" DIRECTORY)
    install(FILES "${CMAKE_SOURCE_DIR}/${_license}"
      DESTINATION "${LICHEN_DOC_DIR}/upstream/${_license_dir}" COMPONENT ${LICHEN_COMPONENT})
  endif()
endforeach()
install(FILES "${CMAKE_SOURCE_DIR}/CONTRIBUTORS" "${CMAKE_SOURCE_DIR}/data/copyright.html"
  DESTINATION "${LICHEN_DOC_DIR}/upstream" COMPONENT ${LICHEN_COMPONENT})
