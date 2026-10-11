cmake_minimum_required(VERSION 3.21)

if(NOT DEFINED SED_STAGE_TEST_ROOT OR SED_STAGE_TEST_ROOT STREQUAL "")
  message(FATAL_ERROR "SED_STAGE_TEST_ROOT must be provided")
endif()

set(STAGE_SCRIPT
  "${CMAKE_CURRENT_LIST_DIR}/../stage_release_package.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/../runtime_data.cmake")
file(REMOVE_RECURSE "${SED_STAGE_TEST_ROOT}")
file(MAKE_DIRECTORY
  "${SED_STAGE_TEST_ROOT}/bin"
  "${SED_STAGE_TEST_ROOT}/data"
  "${SED_STAGE_TEST_ROOT}/notices"
  "${SED_STAGE_TEST_ROOT}/stage")
file(WRITE "${SED_STAGE_TEST_ROOT}/bin/sed.exe" "synthetic executable")
file(WRITE "${SED_STAGE_TEST_ROOT}/data/needed.dat" "required runtime data")
file(WRITE "${SED_STAGE_TEST_ROOT}/data/stale.dat" "must not be discovered")
file(WRITE "${SED_STAGE_TEST_ROOT}/notices/LICENSE" "GPL notice")
file(WRITE "${SED_STAGE_TEST_ROOT}/notices/THIRD_PARTY_NOTICES.md" "third-party notice")
file(WRITE "${SED_STAGE_TEST_ROOT}/notices/empty.txt" "")
file(WRITE "${SED_STAGE_TEST_ROOT}/notices/qt-license.txt" "Qt license")
file(WRITE "${SED_STAGE_TEST_ROOT}/notices/qt-sbom.json" "{\"components\":[]}")
file(WRITE "${SED_STAGE_TEST_ROOT}/notices/empty-deploy-tool.exe" "")

function(run_stage CASE_NAME EXPECT_SUCCESS)
  set(_command "${CMAKE_COMMAND}")
  foreach(_definition IN LISTS ARGN)
    list(APPEND _command "-D${_definition}")
  endforeach()
  list(APPEND _command -P "${STAGE_SCRIPT}")
  execute_process(
    COMMAND ${_command}
    RESULT_VARIABLE _result
    OUTPUT_VARIABLE _output
    ERROR_VARIABLE _error)
  if(EXPECT_SUCCESS AND NOT _result EQUAL 0)
    message(FATAL_ERROR
      "${CASE_NAME} unexpectedly failed (${_result}):\n${_output}\n${_error}")
  endif()
  if(NOT EXPECT_SUCCESS AND _result EQUAL 0)
    message(FATAL_ERROR "${CASE_NAME} unexpectedly passed")
  endif()
endfunction()

set(COMMON_DEFINITIONS
  "SED_PACKAGE_EXECUTABLE=${SED_STAGE_TEST_ROOT}/bin/sed.exe"
  "SED_PACKAGE_DATA_DIR=${SED_STAGE_TEST_ROOT}/data"
  "SED_PACKAGE_RUNTIME_DATA=needed.dat"
  "SED_PACKAGE_PROJECT_LICENSE_FILES=${SED_STAGE_TEST_ROOT}/notices/LICENSE|${SED_STAGE_TEST_ROOT}/notices/THIRD_PARTY_NOTICES.md"
  "SED_PACKAGE_VERSION=0.4.0-test"
  "SED_PACKAGE_PLATFORM=Linux"
  "SED_PACKAGE_MSVC=FALSE")

run_stage(valid TRUE
  "${COMMON_DEFINITIONS}"
  "SED_PACKAGE_OUTPUT_DIR=${SED_STAGE_TEST_ROOT}/stage")

if(NOT EXISTS "${SED_STAGE_TEST_ROOT}/stage/sed.exe")
  message(FATAL_ERROR "valid staging did not copy the executable")
endif()
if(NOT EXISTS "${SED_STAGE_TEST_ROOT}/stage/data/needed.dat")
  message(FATAL_ERROR "valid staging did not copy explicitly selected data")
endif()
if(EXISTS "${SED_STAGE_TEST_ROOT}/stage/data/stale.dat")
  message(FATAL_ERROR "valid staging copied an unselected data file")
endif()
if(NOT EXISTS "${SED_STAGE_TEST_ROOT}/stage/RELEASE_MANIFEST.json")
  message(FATAL_ERROR "valid staging did not write the release manifest")
endif()
file(READ "${SED_STAGE_TEST_ROOT}/stage/RELEASE_MANIFEST.json" VALID_MANIFEST)
foreach(_required_manifest_text IN ITEMS
    "\"version\": \"0.4.0-test\""
    "\"manifest_self_excluded\": true"
    "\"path\":\"data/needed.dat\"")
  string(FIND "${VALID_MANIFEST}" "${_required_manifest_text}" _manifest_position)
  if(_manifest_position EQUAL -1)
    message(FATAL_ERROR
      "valid staging manifest lacks fixed field: ${_required_manifest_text}")
  endif()
endforeach()
string(FIND "${VALID_MANIFEST}" "${SED_STAGE_TEST_ROOT}" _absolute_path_position)
if(NOT _absolute_path_position EQUAL -1)
  message(FATAL_ERROR "release manifest contains a machine path")
endif()

# Exercise the production runtime-data manifest with only the shipped baseline
# files present. Optional IJIM category files must not be prerequisites for a
# valid stage, while all baseline data must be copied from this explicit set.
set(_production_data_root "${SED_STAGE_TEST_ROOT}/production-data")
set(_production_stage_root "${SED_STAGE_TEST_ROOT}/production-stage")
set(_production_baseline_files
  df2jk.lst
  ijim.tpl
  master.tpl
  mats.jlh
  mats.jll
  mots.tpl
  mtpls.jlh
  mtpls.jll
  shapes.tpl
  tpls.jlh
  tpls.jll
)
file(MAKE_DIRECTORY "${_production_data_root}")
foreach(_data_name IN LISTS _production_baseline_files)
  file(WRITE "${_production_data_root}/${_data_name}"
    "synthetic runtime data: ${_data_name}")
endforeach()
string(JOIN "|" _production_runtime_data ${SED_RUNTIME_DATA_FILES})

run_stage(production_manifest_baseline_files TRUE
  "SED_PACKAGE_EXECUTABLE=${SED_STAGE_TEST_ROOT}/bin/sed.exe"
  "SED_PACKAGE_OUTPUT_DIR=${_production_stage_root}"
  "SED_PACKAGE_DATA_DIR=${_production_data_root}"
  "SED_PACKAGE_RUNTIME_DATA=${_production_runtime_data}"
  "SED_PACKAGE_PROJECT_LICENSE_FILES=${SED_STAGE_TEST_ROOT}/notices/LICENSE|${SED_STAGE_TEST_ROOT}/notices/THIRD_PARTY_NOTICES.md"
  "SED_PACKAGE_VERSION=0.4.0-test"
  "SED_PACKAGE_PLATFORM=Linux"
  "SED_PACKAGE_MSVC=FALSE")

foreach(_data_name IN LISTS _production_baseline_files)
  set(_staged_data "${_production_stage_root}/data/${_data_name}")
  if(NOT EXISTS "${_staged_data}")
    message(FATAL_ERROR
      "production manifest staging omitted baseline runtime data: ${_data_name}")
  endif()
  file(READ "${_staged_data}" _staged_data_contents)
  if(NOT _staged_data_contents STREQUAL "synthetic runtime data: ${_data_name}")
    message(FATAL_ERROR
      "production manifest staging changed baseline runtime data: ${_data_name}")
  endif()
endforeach()

set(_package_layout_verifier
  "${CMAKE_CURRENT_LIST_DIR}/../../src/app/tests/verify_package_layout.cmake")
execute_process(
  COMMAND "${CMAKE_COMMAND}"
    "-DSED_PACKAGE_EXECUTABLE=${_production_stage_root}/sed.exe"
    "-DSED_PACKAGE_PLATFORM=Linux"
    "-DSED_PACKAGE_QT_MAJOR=6"
    "-DSED_PACKAGE_DEBUG=FALSE"
    "-DSED_PACKAGE_RUNTIME_DATA=${_production_runtime_data}"
    -P "${_package_layout_verifier}"
  RESULT_VARIABLE _package_layout_result
  OUTPUT_VARIABLE _package_layout_output
  ERROR_VARIABLE _package_layout_error)
if(NOT _package_layout_result EQUAL 0)
  message(FATAL_ERROR
    "production manifest staged layout unexpectedly failed (${_package_layout_result}):\n"
    "${_package_layout_output}\n${_package_layout_error}")
endif()

# The verifier independently requires each supported game's template database,
# even when a package has otherwise valid runtime data.
file(REMOVE "${_production_stage_root}/data/ijim.tpl")
execute_process(
  COMMAND "${CMAKE_COMMAND}"
    "-DSED_PACKAGE_EXECUTABLE=${_production_stage_root}/sed.exe"
    "-DSED_PACKAGE_PLATFORM=Linux"
    "-DSED_PACKAGE_QT_MAJOR=6"
    "-DSED_PACKAGE_DEBUG=FALSE"
    "-DSED_PACKAGE_RUNTIME_DATA=${_production_runtime_data}"
    -P "${_package_layout_verifier}"
  RESULT_VARIABLE _missing_template_result
  OUTPUT_VARIABLE _missing_template_output
  ERROR_VARIABLE _missing_template_error)
if(_missing_template_result EQUAL 0)
  message(FATAL_ERROR
    "package layout verifier accepted a missing required IJIM template database")
endif()
string(FIND
  "${_missing_template_output}\n${_missing_template_error}"
  "Required packaged template database is missing"
  _missing_template_reason)
if(_missing_template_reason EQUAL -1)
  message(FATAL_ERROR
    "missing IJIM template database was rejected for an unexpected reason:\n"
    "${_missing_template_output}\n${_missing_template_error}")
endif()

# The output directory is destructive by design, so a source-root overlap must
# fail before REMOVE_RECURSE can touch the input tree.
run_stage(output_overlaps_source FALSE
  "${COMMON_DEFINITIONS}"
  "SED_PACKAGE_OUTPUT_DIR=${SED_STAGE_TEST_ROOT}")
if(NOT EXISTS "${SED_STAGE_TEST_ROOT}/bin/sed.exe")
  message(FATAL_ERROR "overlap rejection removed the source executable")
endif()

file(MAKE_DIRECTORY "${SED_STAGE_TEST_ROOT}/stage-empty")
file(WRITE "${SED_STAGE_TEST_ROOT}/stage-empty/previous-package.txt" "keep this package")
run_stage(empty_notice_preserves_output FALSE
  "${COMMON_DEFINITIONS}"
  "SED_PACKAGE_OUTPUT_DIR=${SED_STAGE_TEST_ROOT}/stage-empty"
  "SED_PACKAGE_PROJECT_LICENSE_FILES=${SED_STAGE_TEST_ROOT}/notices/empty.txt")
if(NOT EXISTS "${SED_STAGE_TEST_ROOT}/stage-empty/previous-package.txt")
  message(FATAL_ERROR "empty notice rejection removed a previous package")
endif()

file(WRITE "${SED_STAGE_TEST_ROOT}/notices/sed.exe" "must not replace the executable")
run_stage(root_destination_collision FALSE
  "${COMMON_DEFINITIONS}"
  "SED_PACKAGE_OUTPUT_DIR=${SED_STAGE_TEST_ROOT}/stage-root-collision"
  "SED_PACKAGE_PROJECT_LICENSE_FILES=${SED_STAGE_TEST_ROOT}/notices/sed.exe")
if(EXISTS "${SED_STAGE_TEST_ROOT}/stage-root-collision")
  message(FATAL_ERROR "root destination collision created an output")
endif()

file(WRITE "${SED_STAGE_TEST_ROOT}/data/Foo.dat" "file")
file(WRITE "${SED_STAGE_TEST_ROOT}/data/foo.dat" "file")
run_stage(data_destination_collision FALSE
  "${COMMON_DEFINITIONS}"
  "SED_PACKAGE_OUTPUT_DIR=${SED_STAGE_TEST_ROOT}/stage-data-collision"
  "SED_PACKAGE_RUNTIME_DATA=Foo.dat|foo.dat")
if(EXISTS "${SED_STAGE_TEST_ROOT}/stage-data-collision")
  message(FATAL_ERROR "data destination collision created an output")
endif()

run_stage(missing_runtime_list FALSE
  "SED_PACKAGE_EXECUTABLE=${SED_STAGE_TEST_ROOT}/bin/sed.exe"
  "SED_PACKAGE_OUTPUT_DIR=${SED_STAGE_TEST_ROOT}/stage-missing-runtime"
  "SED_PACKAGE_DATA_DIR=${SED_STAGE_TEST_ROOT}/data"
  "SED_PACKAGE_PROJECT_LICENSE_FILES=${SED_STAGE_TEST_ROOT}/notices/LICENSE"
  "SED_PACKAGE_VERSION=0.4.0-test"
  "SED_PACKAGE_PLATFORM=Linux")

run_stage(windows_missing_kit_metadata FALSE
  "SED_PACKAGE_EXECUTABLE=${SED_STAGE_TEST_ROOT}/bin/sed.exe"
  "SED_PACKAGE_OUTPUT_DIR=${SED_STAGE_TEST_ROOT}/stage-windows"
  "SED_PACKAGE_DATA_DIR=${SED_STAGE_TEST_ROOT}/data"
  "SED_PACKAGE_RUNTIME_DATA=needed.dat"
  "SED_PACKAGE_PROJECT_LICENSE_FILES=${SED_STAGE_TEST_ROOT}/notices/LICENSE"
  "SED_PACKAGE_VERSION=0.4.0-test"
  "SED_PACKAGE_PLATFORM=Windows"
  "SED_PACKAGE_MSVC=FALSE")

run_stage(windows_empty_deploy_tool FALSE
  "SED_PACKAGE_EXECUTABLE=${SED_STAGE_TEST_ROOT}/bin/sed.exe"
  "SED_PACKAGE_OUTPUT_DIR=${SED_STAGE_TEST_ROOT}/stage-windows-empty-tool"
  "SED_PACKAGE_DATA_DIR=${SED_STAGE_TEST_ROOT}/data"
  "SED_PACKAGE_RUNTIME_DATA=needed.dat"
  "SED_PACKAGE_PROJECT_LICENSE_FILES=${SED_STAGE_TEST_ROOT}/notices/LICENSE"
  "SED_PACKAGE_VERSION=0.4.0-test"
  "SED_PACKAGE_PLATFORM=Windows"
  "SED_PACKAGE_MSVC=FALSE"
  "SED_PACKAGE_QT_VERSION=6.9.0"
  "SED_PACKAGE_QT_LICENSE_FILES=${SED_STAGE_TEST_ROOT}/notices/qt-license.txt"
  "SED_PACKAGE_QT_SBOM_FILES=${SED_STAGE_TEST_ROOT}/notices/qt-sbom.json"
  "SED_PACKAGE_QT_DEPLOY_TOOL=${SED_STAGE_TEST_ROOT}/notices/empty-deploy-tool.exe")
if(EXISTS "${SED_STAGE_TEST_ROOT}/stage-windows-empty-tool")
  message(FATAL_ERROR "empty Qt deployment-tool rejection created an output")
endif()

execute_process(
  COMMAND "${CMAKE_COMMAND}"
    "-DSED_MAPPING_TEST_ROOT=${SED_STAGE_TEST_ROOT}/mapping-tests"
    -P "${CMAKE_CURRENT_LIST_DIR}/qt_deploy_mapping_tests.cmake"
  RESULT_VARIABLE _mapping_test_result
  OUTPUT_VARIABLE _mapping_test_output
  ERROR_VARIABLE _mapping_test_error)
if(NOT _mapping_test_result EQUAL 0)
  message(FATAL_ERROR
    "Qt deployment mapping tests failed (${_mapping_test_result}):\n"
    "${_mapping_test_output}\n${_mapping_test_error}")
endif()

file(REMOVE_RECURSE "${SED_STAGE_TEST_ROOT}")
message(STATUS "Verified release-package staging success and rejection branches")
