cmake_minimum_required(VERSION 3.21)

if(NOT DEFINED SED_MAPPING_TEST_ROOT OR SED_MAPPING_TEST_ROOT STREQUAL "")
  message(FATAL_ERROR "SED_MAPPING_TEST_ROOT must be provided")
endif()

set(_case_script "${CMAKE_CURRENT_LIST_DIR}/qt_deploy_mapping_case.cmake")
file(REMOVE_RECURSE "${SED_MAPPING_TEST_ROOT}")
file(MAKE_DIRECTORY
  "${SED_MAPPING_TEST_ROOT}/system/System32"
  "${SED_MAPPING_TEST_ROOT}/qt/bin"
  "${SED_MAPPING_TEST_ROOT}/out")
file(WRITE "${SED_MAPPING_TEST_ROOT}/system/System32/icuuc.dll" "system ICU")
file(WRITE "${SED_MAPPING_TEST_ROOT}/system/System32/other.dll" "system DLL")
file(MAKE_DIRECTORY "${SED_MAPPING_TEST_ROOT}/systematic")
file(WRITE "${SED_MAPPING_TEST_ROOT}/systematic/not-system.dll" "sibling")
file(WRITE "${SED_MAPPING_TEST_ROOT}/qt/bin/icuuc.dll" "Qt ICU")
file(WRITE "${SED_MAPPING_TEST_ROOT}/qt/bin/other.dll" "Qt DLL")

file(WRITE "${SED_MAPPING_TEST_ROOT}/valid.mapping"
  "\"${SED_MAPPING_TEST_ROOT}/system/System32/icuuc.dll\" \"icuuc.dll\"\n"
  "\"${SED_MAPPING_TEST_ROOT}/qt/bin/icuuc.dll\" \"icuuc-74.dll\"\n"
  "\"${SED_MAPPING_TEST_ROOT}/system/System32/other.dll\" \"plugins\\\\system.dll\"\n"
  "\"${SED_MAPPING_TEST_ROOT}/qt/bin/other.dll\" \"networkinformation/qnetworklistmanager.dll\"\n")
file(WRITE "${SED_MAPPING_TEST_ROOT}/valid.expected"
  "icuuc.dll\nplugins/system.dll\n")
file(MAKE_DIRECTORY "${SED_MAPPING_TEST_ROOT}/out/plugins")
file(WRITE "${SED_MAPPING_TEST_ROOT}/out/icuuc.dll" "system copy")
file(WRITE "${SED_MAPPING_TEST_ROOT}/out/plugins/system.dll" "system copy")
file(WRITE "${SED_MAPPING_TEST_ROOT}/out/icuuc-74.dll" "selected Qt copy")

execute_process(
  COMMAND "${CMAKE_COMMAND}"
    "-DSED_MAPPING_OUTPUT_DIR=${SED_MAPPING_TEST_ROOT}/out"
    "-DSED_MAPPING_SYSTEM_ROOT=${SED_MAPPING_TEST_ROOT}/system"
    "-DSED_MAPPING_TEXT_FILE=${SED_MAPPING_TEST_ROOT}/valid.mapping"
    "-DSED_MAPPING_RESULT_FILE=${SED_MAPPING_TEST_ROOT}/valid.result"
    "-DSED_MAPPING_REMOVE_AFTER_PARSE=TRUE"
    -P "${_case_script}"
  RESULT_VARIABLE _valid_result
  OUTPUT_VARIABLE _valid_output
  ERROR_VARIABLE _valid_error)
if(NOT _valid_result EQUAL 0)
  message(FATAL_ERROR "valid mapping unexpectedly failed:\n${_valid_output}\n${_valid_error}")
endif()
file(READ "${SED_MAPPING_TEST_ROOT}/valid.result" _valid_actual)
file(READ "${SED_MAPPING_TEST_ROOT}/valid.expected" _valid_expected)
if(NOT _valid_actual STREQUAL _valid_expected)
  message(FATAL_ERROR
    "system mapping selection mismatch:\nexpected:\n${_valid_expected}\nactual:\n${_valid_actual}")
endif()
if(EXISTS "${SED_MAPPING_TEST_ROOT}/out/icuuc.dll"
    OR EXISTS "${SED_MAPPING_TEST_ROOT}/out/plugins/system.dll")
  message(FATAL_ERROR "validated Windows system targets were not removed")
endif()
if(NOT EXISTS "${SED_MAPPING_TEST_ROOT}/out/icuuc-74.dll")
  message(FATAL_ERROR "selected Qt ICU target was removed with the system target")
endif()

# A SystemRoot sibling must not be treated as a system source merely because
# its spelling starts with the same characters as the root path.
file(WRITE "${SED_MAPPING_TEST_ROOT}/sibling.mapping"
  "\"${SED_MAPPING_TEST_ROOT}/systematic/not-system.dll\" \"sibling.dll\"\n")
execute_process(
  COMMAND "${CMAKE_COMMAND}"
    "-DSED_MAPPING_OUTPUT_DIR=${SED_MAPPING_TEST_ROOT}/out"
    "-DSED_MAPPING_SYSTEM_ROOT=${SED_MAPPING_TEST_ROOT}/system"
    "-DSED_MAPPING_TEXT_FILE=${SED_MAPPING_TEST_ROOT}/sibling.mapping"
    "-DSED_MAPPING_RESULT_FILE=${SED_MAPPING_TEST_ROOT}/sibling.result"
    -P "${_case_script}"
  RESULT_VARIABLE _sibling_result
  OUTPUT_VARIABLE _sibling_output
  ERROR_VARIABLE _sibling_error)
if(NOT _sibling_result EQUAL 0)
  message(FATAL_ERROR "SystemRoot sibling mapping unexpectedly failed")
endif()
file(READ "${SED_MAPPING_TEST_ROOT}/sibling.result" _sibling_actual)
if(NOT _sibling_actual STREQUAL "")
  message(FATAL_ERROR "SystemRoot sibling was incorrectly selected for removal")
endif()

# A target outside the staging directory must never be converted into a file
# to remove, even if its source is under SystemRoot.
file(WRITE "${SED_MAPPING_TEST_ROOT}/outside.mapping"
  "\"${SED_MAPPING_TEST_ROOT}/system/System32/icuuc.dll\" \"../outside.dll\"\n")
execute_process(
  COMMAND "${CMAKE_COMMAND}"
    "-DSED_MAPPING_OUTPUT_DIR=${SED_MAPPING_TEST_ROOT}/out"
    "-DSED_MAPPING_SYSTEM_ROOT=${SED_MAPPING_TEST_ROOT}/system"
    "-DSED_MAPPING_TEXT_FILE=${SED_MAPPING_TEST_ROOT}/outside.mapping"
    -P "${_case_script}"
  RESULT_VARIABLE _outside_result
  OUTPUT_VARIABLE _outside_output
  ERROR_VARIABLE _outside_error)
if(_outside_result EQUAL 0)
  message(FATAL_ERROR "mapping target outside the package unexpectedly passed")
endif()

file(WRITE "${SED_MAPPING_TEST_ROOT}/absolute.mapping"
  "\"${SED_MAPPING_TEST_ROOT}/system/System32/icuuc.dll\" \"${SED_MAPPING_TEST_ROOT}/out/icuuc.dll\"\n")
execute_process(
  COMMAND "${CMAKE_COMMAND}"
    "-DSED_MAPPING_OUTPUT_DIR=${SED_MAPPING_TEST_ROOT}/out"
    "-DSED_MAPPING_SYSTEM_ROOT=${SED_MAPPING_TEST_ROOT}/system"
    "-DSED_MAPPING_TEXT_FILE=${SED_MAPPING_TEST_ROOT}/absolute.mapping"
    -P "${_case_script}"
  RESULT_VARIABLE _absolute_result
  OUTPUT_VARIABLE _absolute_output
  ERROR_VARIABLE _absolute_error)
if(_absolute_result EQUAL 0)
  message(FATAL_ERROR "absolute mapping target unexpectedly passed")
endif()

file(WRITE "${SED_MAPPING_TEST_ROOT}/semicolon.mapping"
  "\"${SED_MAPPING_TEST_ROOT}/system/System32/icuuc.dll\" \"bad;target.dll\"\n")
execute_process(
  COMMAND "${CMAKE_COMMAND}"
    "-DSED_MAPPING_OUTPUT_DIR=${SED_MAPPING_TEST_ROOT}/out"
    "-DSED_MAPPING_SYSTEM_ROOT=${SED_MAPPING_TEST_ROOT}/system"
    "-DSED_MAPPING_TEXT_FILE=${SED_MAPPING_TEST_ROOT}/semicolon.mapping"
    -P "${_case_script}"
  RESULT_VARIABLE _semicolon_result
  OUTPUT_VARIABLE _semicolon_output
  ERROR_VARIABLE _semicolon_error)
if(_semicolon_result EQUAL 0)
  message(FATAL_ERROR "semicolon mapping unexpectedly passed")
endif()

# A target mapped from both Windows and Qt sources is ambiguous. Removing it
# would risk deleting the selected Qt copy, so the staging step must fail.
file(WRITE "${SED_MAPPING_TEST_ROOT}/ambiguous.mapping"
  "\"${SED_MAPPING_TEST_ROOT}/system/System32/icuuc.dll\" \"icuuc.dll\"\n"
  "\"${SED_MAPPING_TEST_ROOT}/qt/bin/icuuc.dll\" \"icuuc.dll\"\n")
execute_process(
  COMMAND "${CMAKE_COMMAND}"
    "-DSED_MAPPING_OUTPUT_DIR=${SED_MAPPING_TEST_ROOT}/out"
    "-DSED_MAPPING_SYSTEM_ROOT=${SED_MAPPING_TEST_ROOT}/system"
    "-DSED_MAPPING_TEXT_FILE=${SED_MAPPING_TEST_ROOT}/ambiguous.mapping"
    -P "${_case_script}"
  RESULT_VARIABLE _ambiguous_result
  OUTPUT_VARIABLE _ambiguous_output
  ERROR_VARIABLE _ambiguous_error)
if(_ambiguous_result EQUAL 0)
  message(FATAL_ERROR "ambiguous source mapping unexpectedly passed")
endif()

# Check the reverse order as well; a parser that only checks previously seen
# Windows entries could otherwise miss this collision.
file(WRITE "${SED_MAPPING_TEST_ROOT}/ambiguous-reversed.mapping"
  "\"${SED_MAPPING_TEST_ROOT}/qt/bin/icuuc.dll\" \"icuuc.dll\"\n"
  "\"${SED_MAPPING_TEST_ROOT}/system/System32/icuuc.dll\" \"icuuc.dll\"\n")
execute_process(
  COMMAND "${CMAKE_COMMAND}"
    "-DSED_MAPPING_OUTPUT_DIR=${SED_MAPPING_TEST_ROOT}/out"
    "-DSED_MAPPING_SYSTEM_ROOT=${SED_MAPPING_TEST_ROOT}/system"
    "-DSED_MAPPING_TEXT_FILE=${SED_MAPPING_TEST_ROOT}/ambiguous-reversed.mapping"
    -P "${_case_script}"
  RESULT_VARIABLE _ambiguous_reversed_result
  OUTPUT_VARIABLE _ambiguous_reversed_output
  ERROR_VARIABLE _ambiguous_reversed_error)
if(_ambiguous_reversed_result EQUAL 0)
  message(FATAL_ERROR "reverse-order ambiguous source mapping unexpectedly passed")
endif()

file(WRITE "${SED_MAPPING_TEST_ROOT}/empty.mapping" "windeployqt diagnostic only\n")
execute_process(
  COMMAND "${CMAKE_COMMAND}"
    "-DSED_MAPPING_OUTPUT_DIR=${SED_MAPPING_TEST_ROOT}/out"
    "-DSED_MAPPING_SYSTEM_ROOT=${SED_MAPPING_TEST_ROOT}/system"
    "-DSED_MAPPING_TEXT_FILE=${SED_MAPPING_TEST_ROOT}/empty.mapping"
    -P "${_case_script}"
  RESULT_VARIABLE _empty_result
  OUTPUT_VARIABLE _empty_output
  ERROR_VARIABLE _empty_error)
if(_empty_result EQUAL 0)
  message(FATAL_ERROR "empty mapping output unexpectedly passed")
endif()

file(REMOVE_RECURSE "${SED_MAPPING_TEST_ROOT}")
message(STATUS "Verified Qt deployment mapping parsing and containment")
