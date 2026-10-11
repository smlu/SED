cmake_minimum_required(VERSION 3.21)

# Small, platform-neutral helpers used by the release staging script.  Qt's
# windeployqt `--list mapping` output is deliberately parsed here instead of
# filtering by a DLL name: a versioned ICU DLL from the selected Qt kit must
# remain package-local, while an unversioned DLL sourced from Windows may be
# removed after deployment.

function(sed_release_mapping_canonical_path INPUT_PATH OUTPUT_VARIABLE)
  get_filename_component(_absolute "${INPUT_PATH}" ABSOLUTE)
  set(_probe "${_absolute}")
  set(_missing_parts)
  while(NOT EXISTS "${_probe}")
    get_filename_component(_parent "${_probe}" DIRECTORY)
    get_filename_component(_name "${_probe}" NAME)
    if(_name STREQUAL "" OR _parent STREQUAL "${_probe}")
      break()
    endif()
    list(PREPEND _missing_parts "${_name}")
    set(_probe "${_parent}")
  endwhile()
  if(EXISTS "${_probe}")
    get_filename_component(_canonical "${_probe}" REALPATH)
    foreach(_part IN LISTS _missing_parts)
      set(_canonical "${_canonical}/${_part}")
    endforeach()
  else()
    set(_canonical "${_absolute}")
  endif()
  file(TO_CMAKE_PATH "${_canonical}" _canonical)
  string(REGEX REPLACE "/+$" "" _canonical "${_canonical}")
  set(${OUTPUT_VARIABLE} "${_canonical}" PARENT_SCOPE)
endfunction()

function(sed_release_mapping_path_is_under CHILD_PATH PARENT_PATH OUTPUT_VARIABLE)
  sed_release_mapping_canonical_path("${CHILD_PATH}" _child)
  sed_release_mapping_canonical_path("${PARENT_PATH}" _parent)
  string(TOLOWER "${_child}" _child_key)
  string(TOLOWER "${_parent}" _parent_key)
  set(_parent_prefix "${_parent_key}/")
  string(FIND "${_child_key}" "${_parent_prefix}" _under)
  if(_child_key STREQUAL _parent_key OR _under EQUAL 0)
    set(${OUTPUT_VARIABLE} TRUE PARENT_SCOPE)
  else()
    set(${OUTPUT_VARIABLE} FALSE PARENT_SCOPE)
  endif()
endfunction()

function(sed_release_mapping_safe_target OUTPUT_DIR RAW_TARGET OUTPUT_VARIABLE)
  string(STRIP "${RAW_TARGET}" _target)
  file(TO_CMAKE_PATH "${_target}" _target)
  string(REPLACE "\\" "/" _target "${_target}")
  string(REGEX REPLACE "^\\./" "" _target "${_target}")
  if(_target STREQUAL ""
      OR _target MATCHES "^[A-Za-z]:/"
      OR _target MATCHES "^/"
      OR _target MATCHES "(^|/)\\.\\.?(/|$)"
      OR _target MATCHES "[\"\r\n\t]"
      OR _target MATCHES ";")
    message(FATAL_ERROR
      "windeployqt mapping contains an unsafe target path: ${RAW_TARGET}")
  endif()

  sed_release_mapping_canonical_path("${OUTPUT_DIR}" _output)
  sed_release_mapping_canonical_path("${OUTPUT_DIR}/${_target}" _target_absolute)
  sed_release_mapping_path_is_under("${_target_absolute}" "${_output}" _target_is_under)
  if(NOT _target_is_under)
    message(FATAL_ERROR
      "windeployqt mapping target escapes the package directory: ${RAW_TARGET}")
  endif()
  file(RELATIVE_PATH _relative_target "${_output}" "${_target_absolute}")
  file(TO_CMAKE_PATH "${_relative_target}" _relative_target)
  if(_relative_target STREQUAL "" OR _relative_target STREQUAL "."
      OR _relative_target MATCHES "(^|/)\\.\\.?(/|$)")
    message(FATAL_ERROR
      "windeployqt mapping target is not a file relative to the package: ${RAW_TARGET}")
  endif()
  set(${OUTPUT_VARIABLE} "${_relative_target}" PARENT_SCOPE)
endfunction()

function(sed_release_collect_system_mapping MAPPING_TEXT OUTPUT_DIR SYSTEM_ROOT
    OUTPUT_TARGETS_VARIABLE)
  sed_release_mapping_canonical_path("${SYSTEM_ROOT}" _system_root)
  if(NOT IS_DIRECTORY "${_system_root}")
    message(FATAL_ERROR
      "Windows system root from SystemRoot is not a directory: ${SYSTEM_ROOT}")
  endif()

  if("${MAPPING_TEXT}" MATCHES ";")
    message(FATAL_ERROR
      "windeployqt mapping output contains an unsupported semicolon")
  endif()
  string(REPLACE "\r\n" "\n" _mapping_text "${MAPPING_TEXT}")
  string(REPLACE "\r" "\n" _mapping_text "${_mapping_text}")
  string(REPLACE "\n" ";" _mapping_lines "${_mapping_text}")
  set(_system_targets)
  set(_non_system_targets)
  set(_mapping_count 0)

  foreach(_line IN LISTS _mapping_lines)
    string(STRIP "${_line}" _line)
    if(_line STREQUAL "")
      continue()
    endif()

    # Qt emits one pair per line as: "absolute source" "relative target".
    # Ignore non-mapping diagnostic lines, but require at least one mapping so
    # a changed/unsupported windeployqt output format cannot silently bypass
    # this system-file filter.
    if(NOT _line MATCHES [[^"([^"]+)"[ 	]+"([^"]+)"[ 	]*$]])
      continue()
    endif()
    set(_source "${CMAKE_MATCH_1}")
    set(_raw_target "${CMAKE_MATCH_2}")
    math(EXPR _mapping_count "${_mapping_count} + 1")

    sed_release_mapping_safe_target("${OUTPUT_DIR}" "${_raw_target}" _target)
    sed_release_mapping_canonical_path("${_source}" _source_canonical)
    sed_release_mapping_path_is_under("${_source_canonical}" "${_system_root}"
      _source_is_system)
    if(_source_is_system
        AND (NOT EXISTS "${_source_canonical}" OR IS_DIRECTORY "${_source_canonical}"))
      message(FATAL_ERROR
        "windeployqt mapping names a missing or non-file Windows system source: ${_source}")
    endif()

    string(TOLOWER "${_target}" _target_key)
    if(_source_is_system)
      foreach(_existing IN LISTS _non_system_targets)
        string(TOLOWER "${_existing}" _existing_key)
        if(_existing_key STREQUAL _target_key)
          message(FATAL_ERROR
            "windeployqt maps a package target from both Windows and Qt sources: ${_target}")
        endif()
      endforeach()
      foreach(_existing IN LISTS _system_targets)
        string(TOLOWER "${_existing}" _existing_key)
        if(_existing_key STREQUAL _target_key)
          message(FATAL_ERROR
            "windeployqt maps multiple Windows sources to one package target: ${_target}")
        endif()
      endforeach()
      list(APPEND _system_targets "${_target}")
    else()
      foreach(_existing IN LISTS _system_targets)
        string(TOLOWER "${_existing}" _existing_key)
        if(_existing_key STREQUAL _target_key)
          message(FATAL_ERROR
            "windeployqt maps a package target from both Qt and Windows sources: ${_target}")
        endif()
      endforeach()
      list(APPEND _non_system_targets "${_target}")
    endif()
  endforeach()

  if(_mapping_count EQUAL 0)
    message(FATAL_ERROR
      "windeployqt --dry-run --list mapping produced no parseable file mappings")
  endif()
  set(${OUTPUT_TARGETS_VARIABLE} "${_system_targets}" PARENT_SCOPE)
endfunction()

function(sed_release_remove_system_targets OUTPUT_DIR TARGETS)
  foreach(_target IN LISTS ${TARGETS})
    set(_target_path "${OUTPUT_DIR}/${_target}")
    if(IS_DIRECTORY "${_target_path}")
      message(FATAL_ERROR
        "windeployqt system-DLL mapping points to a package directory: ${_target}")
    endif()
    if(EXISTS "${_target_path}" OR IS_SYMLINK "${_target_path}")
      file(REMOVE "${_target_path}")
    endif()
  endforeach()
endfunction()
