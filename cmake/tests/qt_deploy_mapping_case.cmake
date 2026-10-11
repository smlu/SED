cmake_minimum_required(VERSION 3.21)

if(NOT DEFINED SED_MAPPING_OUTPUT_DIR
    OR NOT DEFINED SED_MAPPING_SYSTEM_ROOT
    OR NOT DEFINED SED_MAPPING_TEXT_FILE)
  message(FATAL_ERROR
    "SED_MAPPING_OUTPUT_DIR, SED_MAPPING_SYSTEM_ROOT, and SED_MAPPING_TEXT_FILE are required")
endif()

include("${CMAKE_CURRENT_LIST_DIR}/../qt_deploy_mapping.cmake")
file(READ "${SED_MAPPING_TEXT_FILE}" _mapping_text)
sed_release_collect_system_mapping(
  "${_mapping_text}" "${SED_MAPPING_OUTPUT_DIR}" "${SED_MAPPING_SYSTEM_ROOT}"
  _system_targets)
if(DEFINED SED_MAPPING_REMOVE_AFTER_PARSE
    AND SED_MAPPING_REMOVE_AFTER_PARSE)
  sed_release_remove_system_targets("${SED_MAPPING_OUTPUT_DIR}" _system_targets)
endif()
if(DEFINED SED_MAPPING_RESULT_FILE)
  string(REPLACE ";" "\n" _result "${_system_targets}")
  if(NOT _result STREQUAL "")
    string(APPEND _result "\n")
  endif()
  file(WRITE "${SED_MAPPING_RESULT_FILE}" "${_result}")
endif()
