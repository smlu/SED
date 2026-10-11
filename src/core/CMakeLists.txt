add_library(sed_core
  src/math_utils.cpp
  src/geometry.cpp
  src/level/build.cpp
  src/level/cleave.cpp
  src/level/clipboard.cpp
  src/level/connected_surface_uv.cpp
  src/level/edit.cpp
  src/level/extrude_contained_ring.cpp
  src/level/geometry.cpp
  src/level/arch_lighting.cpp
  src/level/arch_lighting_clipboard.cpp
  src/level/lighting.cpp
  src/level/merge.cpp
  src/level/thing.cpp
  src/level/transform.cpp
  src/level/uv.cpp
  src/editor/uv_promotion.cpp
  src/assets/3do.cpp
  src/assets/cmp.cpp
  src/assets/asc.cpp
  src/assets/df2jk.cpp
  src/assets/episode.cpp
  src/assets/cog_script.cpp
  src/assets/gob.cpp
  src/assets/indy_wv.cpp
  src/assets/jed.cpp
  src/assets/jkl.cpp
  src/assets/key.cpp
  src/assets/items_dat.cpp
  src/assets/jlh.cpp
  src/assets/lev.cpp
    src/assets/mat.cpp
    src/assets/material_dimensions.cpp
  src/assets/models_dat.cpp
  src/assets/par.cpp
  src/assets/pup.cpp
  src/assets/resource_locator.cpp
  src/assets/sed.cpp
  src/assets/static_resources.cpp
  src/assets/snd.cpp
  src/assets/spr.cpp
  src/assets/string_table.cpp
  src/assets/tpl.cpp
  src/assets/wav.cpp
  src/level.cpp
  src/values.cpp
)

target_include_directories(sed_core
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)

target_compile_features(sed_core PUBLIC cxx_std_20)

if(MSVC)
  # Fix parallel compilation PDB contention (C1041) in this environment.
  target_compile_options(sed_core PRIVATE /FS)
endif()

add_executable(sed_core_tests
  tests/test_main.cpp
  tests/geometry/geometry_math_tests.cpp
  tests/level/level_tests.cpp
  tests/level/level_geometry_tests.cpp
  tests/level/level_lighting_tests.cpp
  tests/level/level_picking_tests.cpp
  tests/level/level_adjoin_tests.cpp
  tests/level/level_build_tests.cpp
  tests/level/level_extrude_tests.cpp
  tests/level/extrude_collinear_support_tests.cpp
  tests/level/extrude_contained_ring_tests.cpp
  tests/level/extrude_arrangement_registry_tests.cpp
  tests/level/level_closure_contract_tests.cpp
  tests/level/level_cleave_tests.cpp
  tests/level/cleave_subdivided_portal_tests.cpp
  tests/level/level_connect_contract_tests.cpp
  tests/level/level_cog_reference_tests.cpp
  tests/level/level_clipboard_tests.cpp
  tests/level/level_edit_tests.cpp
  tests/level/level_merge_tests.cpp
  tests/level/level_surface_merge_uv_tests.cpp
  tests/level/level_thing_tests.cpp
  tests/level/level_transform_tests.cpp
  tests/level/connected_surface_uv_tests.cpp
  tests/level/surface_extrude_uv_regression_tests.cpp
  tests/level/level_uv_tests.cpp
  tests/level/level_layers_tests.cpp
  tests/editor/editor_tests.cpp
  tests/editor/pending_cog_import_tests.cpp
  tests/text/text_tests.cpp
  tests/binary_reader_tests.cpp
  tests/assets/jkl_level_tests.cpp
  tests/assets/arch_lighting_tests.cpp
  tests/level/arch_lighting_clipboard_tests.cpp
  tests/assets/jkl_save_tests.cpp
  tests/assets/ndy_save_tests.cpp
  tests/assets/jed_level_tests.cpp
  tests/assets/sed_level_tests.cpp
  tests/assets/model3do_tests.cpp
  tests/assets/key_tests.cpp
  tests/assets/tpl_tests.cpp
  tests/assets/items_dat_tests.cpp
  tests/assets/mat_cmp_tests.cpp
  tests/assets/models_dat_tests.cpp
  tests/assets/par_tests.cpp
  tests/assets/pup_tests.cpp
  tests/assets/resource_locator_disk_tests.cpp
  tests/assets/resource_locator_bounded_read_tests.cpp
  tests/assets/resource_stream_failure_tests.cpp
  tests/assets/resource_locator_sharing_tests.cpp
  tests/assets/resource_locator_overlay_tests.cpp
  tests/assets/cog_script_tests.cpp
  tests/assets/snd_tests.cpp
  tests/assets/string_table_tests.cpp
  tests/assets/spr_tests.cpp
  tests/assets/gob_tests.cpp
  tests/assets/indy_wv_tests.cpp
  tests/assets/jlh_tests.cpp
  tests/assets/df2jk_tests.cpp
  tests/assets/episode_tests.cpp
  tests/assets/asc_tests.cpp
  tests/assets/lev_tests.cpp
  tests/assets/wav_tests.cpp
)
target_link_libraries(sed_core_tests PRIVATE sed_core)
target_include_directories(sed_core_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/tests)

if(MSVC)
  target_compile_options(sed_core_tests PRIVATE /FS)
endif()

add_test(NAME sed_core_tests COMMAND sed_core_tests)
add_test(NAME sed_pending_cog_import_tests
  COMMAND sed_core_tests --pending-cog-import)
set(SED_REPO_ROOT "${CMAKE_SOURCE_DIR}")
set(SED_CORE_TEST_TEMP_ROOT "${CMAKE_BINARY_DIR}/test-temp/core")
file(MAKE_DIRECTORY "${SED_CORE_TEST_TEMP_ROOT}")
set_tests_properties(sed_core_tests PROPERTIES
  ENVIRONMENT "TMPDIR=${SED_CORE_TEST_TEMP_ROOT};TEMP=${SED_CORE_TEST_TEMP_ROOT};TMP=${SED_CORE_TEST_TEMP_ROOT}"
  LABELS "unit;module;domain;integration"
  TIMEOUT 300
  WORKING_DIRECTORY "${SED_REPO_ROOT}"
)
set_tests_properties(sed_pending_cog_import_tests PROPERTIES
  ENVIRONMENT "TMPDIR=${SED_CORE_TEST_TEMP_ROOT};TEMP=${SED_CORE_TEST_TEMP_ROOT};TMP=${SED_CORE_TEST_TEMP_ROOT}"
  LABELS "unit;domain;integration"
  TIMEOUT 30
  WORKING_DIRECTORY "${SED_REPO_ROOT}"
)

add_executable(sed_document_allocation_tests tests/editor/document_allocation_tests.cpp)
target_link_libraries(sed_document_allocation_tests PRIVATE sed_core)
target_include_directories(sed_document_allocation_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/tests)
if(MSVC)
  target_compile_options(sed_document_allocation_tests PRIVATE /FS)
endif()
add_test(NAME sed_document_allocation_tests COMMAND sed_document_allocation_tests)
set_tests_properties(sed_document_allocation_tests PROPERTIES
  LABELS "unit;domain;allocation"
  TIMEOUT 300
  WORKING_DIRECTORY "${SED_REPO_ROOT}"
)

# Isolated processes retain coverage of allocation failures that previously
# invoked std::terminate through false noexcept contracts.
foreach(contract_mode IN ITEMS template-find template-field frame-parse frame-get frame-default-get surface-ref pup-parse)
  string(REPLACE "-" "_" contract_name "${contract_mode}")
  add_test(NAME "sed_allocation_contract_${contract_name}"
    COMMAND sed_document_allocation_tests "--${contract_mode}-fault")
  set_tests_properties("sed_allocation_contract_${contract_name}" PROPERTIES
    LABELS "unit;module;allocation"
    TIMEOUT 30
    WORKING_DIRECTORY "${SED_REPO_ROOT}"
  )
endforeach()

add_executable(sed_frame_transform_allocation_tests tests/level/frame_transform_allocation_tests.cpp)
target_link_libraries(sed_frame_transform_allocation_tests PRIVATE sed_core)
target_include_directories(sed_frame_transform_allocation_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/tests)
if(MSVC)
  target_compile_options(sed_frame_transform_allocation_tests PRIVATE /FS)
endif()
foreach(frame_mode IN ITEMS translate-things translate-frames rotate-frames scale-frames flip-frames)
  string(REPLACE "-" "_" frame_test_name "${frame_mode}")
  add_test(NAME "sed_frame_allocation_${frame_test_name}"
    COMMAND sed_frame_transform_allocation_tests "${frame_mode}")
  set_tests_properties("sed_frame_allocation_${frame_test_name}" PROPERTIES
    LABELS "unit;module;allocation"
    TIMEOUT 30
    WORKING_DIRECTORY "${SED_REPO_ROOT}"
  )
endforeach()

add_executable(sed_geometry_transform_allocation_tests tests/level/geometry_transform_allocation_tests.cpp)
target_link_libraries(sed_geometry_transform_allocation_tests PRIVATE sed_core)
target_include_directories(sed_geometry_transform_allocation_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/tests)
if(MSVC)
  target_compile_options(sed_geometry_transform_allocation_tests PRIVATE /FS)
endif()
foreach(geometry_transform_mode IN ITEMS recalc translate-sectors translate-surfaces translate-edges translate-vertices
    rotate-sectors rotate-surfaces rotate-edges scale-sectors-textures scale-surfaces-textures flip-sectors-axis
    flip-sectors-plane duplicate-sector)
  add_test(NAME "sed_geometry_transform_allocation_${geometry_transform_mode}"
    COMMAND sed_geometry_transform_allocation_tests "${geometry_transform_mode}")
  set_tests_properties("sed_geometry_transform_allocation_${geometry_transform_mode}" PROPERTIES
    LABELS "unit;module;allocation"
    TIMEOUT 60
    WORKING_DIRECTORY "${SED_REPO_ROOT}"
  )
endforeach()

add_executable(sed_cleave_allocation_tests tests/level/cleave_allocation_tests.cpp)
target_link_libraries(sed_cleave_allocation_tests PRIVATE sed_core)
target_include_directories(sed_cleave_allocation_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/tests)
if(MSVC)
  target_compile_options(sed_cleave_allocation_tests PRIVATE /FS)
endif()
foreach(cleave_mode IN ITEMS surface edge sector batch bounded-batch join knife-keep knife-cut copy-adjoin-params merge-mirrored auto-adjoin join-batch diagnostic-boundary)
  add_test(NAME "sed_cleave_allocation_${cleave_mode}"
    COMMAND sed_cleave_allocation_tests "${cleave_mode}")
  set_tests_properties("sed_cleave_allocation_${cleave_mode}" PROPERTIES
    LABELS "unit;module;allocation"
    TIMEOUT 60
    WORKING_DIRECTORY "${SED_REPO_ROOT}"
  )
endforeach()

add_executable(sed_mat_allocation_tests tests/assets/mat_allocation_tests.cpp)
target_link_libraries(sed_mat_allocation_tests PRIVATE sed_core)
target_include_directories(sed_mat_allocation_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/tests)
if(MSVC)
  target_compile_options(sed_mat_allocation_tests PRIVATE /FS)
endif()
foreach(mat_mode IN ITEMS cel-count mipmap-count)
  string(REPLACE "-" "_" mat_test_name "${mat_mode}")
  add_test(NAME "sed_mat_allocation_${mat_test_name}"
    COMMAND sed_mat_allocation_tests "${mat_mode}")
  set_tests_properties("sed_mat_allocation_${mat_test_name}" PROPERTIES
    LABELS "unit;module;allocation"
    TIMEOUT 30
    WORKING_DIRECTORY "${SED_REPO_ROOT}"
  )
endforeach()

add_executable(sed_extrude_allocation_tests tests/level/extrude_allocation_tests.cpp)
target_link_libraries(sed_extrude_allocation_tests PRIVATE sed_core)
target_include_directories(sed_extrude_allocation_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/tests)
if(MSVC)
  target_compile_options(sed_extrude_allocation_tests PRIVATE /FS)
endif()
foreach(extrude_mode IN ITEMS wire solid bounds source-cut commit expand inset rounded-expand)
  add_test(NAME "sed_extrude_allocation_${extrude_mode}"
    COMMAND sed_extrude_allocation_tests "${extrude_mode}")
  set_tests_properties("sed_extrude_allocation_${extrude_mode}" PROPERTIES
    LABELS "unit;module;allocation"
    TIMEOUT 60
    WORKING_DIRECTORY "${SED_REPO_ROOT}"
  )
endforeach()

if(SED_BUILD_FUZZERS)
  if(NOT CMAKE_CXX_COMPILER_ID MATCHES "Clang" OR MSVC)
    message(FATAL_ERROR
      "SED_BUILD_FUZZERS currently requires the Clang GNU-style driver "
      "so libFuzzer, AddressSanitizer, and UndefinedBehaviorSanitizer use "
      "one verified flag set.")
  endif()

  # Build a focused, fully instrumented codec rather than linking the ordinary
  # sed_core archive: otherwise most of the parser would remain unsanitized
  # and unrelated editor geometry would make each fuzz iteration/build larger.
  add_executable(sed_file_fuzzer
    fuzz/sed_file_fuzzer.cpp
    src/assets/sed.cpp
    src/level.cpp
    src/math_utils.cpp
  )
  target_include_directories(sed_file_fuzzer PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
  )
  target_compile_features(sed_file_fuzzer PRIVATE cxx_std_20)
  target_compile_options(sed_file_fuzzer PRIVATE
    -O1
    -fsanitize=fuzzer,address,undefined
    -fno-sanitize-recover=undefined
    -fno-omit-frame-pointer
  )
  target_link_options(sed_file_fuzzer PRIVATE
    -fsanitize=fuzzer,address,undefined
    -fno-sanitize-recover=undefined
    -fno-omit-frame-pointer
  )
endif()
