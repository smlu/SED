cmake_minimum_required(VERSION 3.21)

include("${CMAKE_CURRENT_LIST_DIR}/qt_deploy_mapping.cmake")

# Stage a clean Windows release package.  The build tree is intentionally not
# used as a source directory: only the selected executable, deployed runtime,
# approved data, and explicit notices enter the package.

function(require_value NAME)
  if(NOT DEFINED ${NAME} OR "${${NAME}}" STREQUAL "")
    message(FATAL_ERROR "${NAME} must be provided")
  endif()
endfunction()

function(require_nonempty_file FILE_PATH LABEL)
  if(NOT EXISTS "${FILE_PATH}" OR IS_DIRECTORY "${FILE_PATH}")
    message(FATAL_ERROR "${LABEL} is missing: ${FILE_PATH}")
  endif()
  file(SIZE "${FILE_PATH}" _file_size)
  if(_file_size EQUAL 0)
    message(FATAL_ERROR "${LABEL} is empty: ${FILE_PATH}")
  endif()
endfunction()

function(expand_pipe_list VALUE OUTPUT_VARIABLE)
  set(_expanded "${VALUE}")
  string(REPLACE "|" ";" _expanded "${_expanded}")
  set(${OUTPUT_VARIABLE} "${_expanded}" PARENT_SCOPE)
endfunction()

require_value(SED_PACKAGE_EXECUTABLE)
require_value(SED_PACKAGE_OUTPUT_DIR)
require_value(SED_PACKAGE_DATA_DIR)
require_value(SED_PACKAGE_RUNTIME_DATA)
require_value(SED_PACKAGE_PROJECT_LICENSE_FILES)
require_value(SED_PACKAGE_VERSION)

require_nonempty_file("${SED_PACKAGE_EXECUTABLE}" "Release executable")
if(NOT IS_DIRECTORY "${SED_PACKAGE_DATA_DIR}")
  message(FATAL_ERROR "Release data directory is missing: ${SED_PACKAGE_DATA_DIR}")
endif()

if(DEFINED SED_PACKAGE_PLATFORM AND NOT SED_PACKAGE_PLATFORM STREQUAL "")
  set(_package_platform "${SED_PACKAGE_PLATFORM}")
else()
  set(_package_platform "${CMAKE_HOST_SYSTEM_NAME}")
endif()

string(TOLOWER "${_package_platform}" _package_platform_lower)
set(_package_msvc FALSE)
if(DEFINED SED_PACKAGE_MSVC AND NOT "${SED_PACKAGE_MSVC}" STREQUAL "")
  string(TOUPPER "${SED_PACKAGE_MSVC}" _package_msvc_value)
  if(NOT _package_msvc_value MATCHES "^(0|1|ON|OFF|TRUE|FALSE|YES|NO|Y|N)$")
    message(FATAL_ERROR "SED_PACKAGE_MSVC must be a Boolean value")
  endif()
  if(_package_msvc_value MATCHES "^(1|ON|TRUE|YES|Y)$")
    set(_package_msvc TRUE)
  endif()
endif()

if(_package_platform_lower STREQUAL "windows")
  require_value(SED_PACKAGE_QT_VERSION)
  require_value(SED_PACKAGE_QT_LICENSE_FILES)
  require_value(SED_PACKAGE_QT_SBOM_FILES)
  require_value(SED_PACKAGE_QT_DEPLOY_TOOL)
  require_nonempty_file("${SED_PACKAGE_QT_DEPLOY_TOOL}" "Qt deployment tool")
  if(_package_msvc)
    require_value(SED_PACKAGE_MSVC_RUNTIME_FILES)
    require_value(SED_PACKAGE_MSVC_NOTICE_FILES)
  endif()
endif()

foreach(_metadata_value IN ITEMS SED_PACKAGE_VERSION SED_PACKAGE_PLATFORM SED_PACKAGE_QT_VERSION)
  if(DEFINED ${_metadata_value}
      AND "${${_metadata_value}}" MATCHES "[\"\\\\\r\n]")
    message(FATAL_ERROR "${_metadata_value} contains an unsupported quote, backslash, or newline")
  endif()
endforeach()

expand_pipe_list("${SED_PACKAGE_RUNTIME_DATA}" _runtime_data)
expand_pipe_list("${SED_PACKAGE_PROJECT_LICENSE_FILES}" _project_license_files)
expand_pipe_list("${SED_PACKAGE_QT_LICENSE_FILES}" _qt_license_files)
expand_pipe_list("${SED_PACKAGE_QT_SBOM_FILES}" _qt_sbom_files)
expand_pipe_list("${SED_PACKAGE_MSVC_RUNTIME_FILES}" _msvc_runtime_files)
expand_pipe_list("${SED_PACKAGE_MSVC_NOTICE_FILES}" _msvc_notice_files)

function(canonical_path INPUT_PATH OUTPUT_VARIABLE)
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

canonical_path("${SED_PACKAGE_OUTPUT_DIR}" _output_absolute)
if(_output_absolute STREQUAL "" OR _output_absolute STREQUAL "/"
    OR _output_absolute MATCHES "^[A-Za-z]:$")
  message(FATAL_ERROR
    "Refusing to remove an unsafe release output directory: ${SED_PACKAGE_OUTPUT_DIR}")
endif()

function(reject_output_overlap INPUT_PATH INPUT_LABEL)
  canonical_path("${INPUT_PATH}" _input_absolute)
  string(TOLOWER "${_input_absolute}" _input_key)
  string(TOLOWER "${_output_absolute}" _output_key)
  set(_input_prefix "${_input_key}/")
  set(_output_prefix "${_output_key}/")
  string(FIND "${_output_key}" "${_input_prefix}" _output_inside)
  string(FIND "${_input_key}" "${_output_prefix}" _input_inside)
  if(_output_key STREQUAL _input_key
      OR _output_inside EQUAL 0
      OR _input_inside EQUAL 0)
    message(FATAL_ERROR
      "Release output overlaps ${INPUT_LABEL}; choose a clean staging directory: "
      "${SED_PACKAGE_OUTPUT_DIR}")
  endif()
endfunction()

function(register_destination DESTINATION DESTINATIONS_VAR LABEL)
  # Every entry registered here is a file.  In addition to exact duplicates,
  # reject file/parent collisions (for example `foo` and `foo/bar`) and use
  # a case-insensitive comparison because the release package is consumed on
  # Windows even when this validation runs on another host.
  set(_destinations "${${DESTINATIONS_VAR}}")
  string(TOLOWER "${DESTINATION}" _destination_key)
  foreach(_existing IN LISTS _destinations)
    string(TOLOWER "${_existing}" _existing_key)
    set(_destination_prefix "${_destination_key}/")
    set(_existing_prefix "${_existing_key}/")
    string(FIND "${_destination_key}" "${_existing_prefix}" _destination_under_existing)
    string(FIND "${_existing_key}" "${_destination_prefix}" _existing_under_destination)
    if(_destination_key STREQUAL _existing_key
        OR _destination_under_existing EQUAL 0
        OR _existing_under_destination EQUAL 0)
      message(FATAL_ERROR
        "${LABEL} destinations collide: ${DESTINATION} and ${_existing}")
    endif()
  endforeach()
  list(APPEND _destinations "${DESTINATION}")
  set(${DESTINATIONS_VAR} "${_destinations}" PARENT_SCOPE)
endfunction()

function(validate_notice_set SOURCE_LIST DESTINATION_ROOT DESTINATION_LIST_VAR)
  set(_destinations)
  foreach(_notice IN LISTS ${SOURCE_LIST})
    require_nonempty_file("${_notice}" "Release notice/SBOM")
    get_filename_component(_notice_name "${_notice}" NAME)
    if(_notice_name MATCHES "[\"\r\n]")
      message(FATAL_ERROR
        "Release notice filename cannot be represented in the release manifest: ${_notice_name}")
    endif()
    register_destination(
      "${DESTINATION_ROOT}/${_notice_name}" _destinations
      "Release notice/SBOM")
  endforeach()
  set(${DESTINATION_LIST_VAR} "${_destinations}" PARENT_SCOPE)
endfunction()

get_filename_component(_executable_absolute "${SED_PACKAGE_EXECUTABLE}" ABSOLUTE)
reject_output_overlap("${_executable_absolute}" "the release executable")
reject_output_overlap("${SED_PACKAGE_DATA_DIR}" "the runtime data directory")
reject_output_overlap("${CMAKE_CURRENT_LIST_DIR}" "the source CMake directory")
foreach(_notice IN LISTS _project_license_files _qt_license_files _qt_sbom_files _msvc_notice_files)
  reject_output_overlap("${_notice}" "a notice source file")
endforeach()
foreach(_runtime IN LISTS _msvc_runtime_files)
  reject_output_overlap("${_runtime}" "an MSVC runtime source file")
endforeach()
if(DEFINED SED_PACKAGE_QT_DEPLOY_TOOL AND NOT "${SED_PACKAGE_QT_DEPLOY_TOOL}" STREQUAL "")
  reject_output_overlap("${SED_PACKAGE_QT_DEPLOY_TOOL}" "the Qt deployment tool")
endif()

# Validate every input and destination before the destructive remove. This
# keeps a typo, an empty notice, or a changed source from deleting a previous
# valid package first.
get_filename_component(_executable_name "${SED_PACKAGE_EXECUTABLE}" NAME)
if(_executable_name MATCHES "[\"\r\n]")
  message(FATAL_ERROR
    "Release executable filename cannot be represented in the release manifest: ${_executable_name}")
endif()

# These root-level destinations are all files.  Reserve the generated
# manifest and package directories before any copy occurs so a notice or a
# selected runtime DLL cannot overwrite the executable/manifest or shadow a
# directory created below.
set(_root_file_destinations)
register_destination("${_executable_name}" _root_file_destinations
  "Release root files")
register_destination("RELEASE_MANIFEST.json" _root_file_destinations
  "Release root files")
set(_reserved_root_directories data licenses)
foreach(_root_directory IN LISTS _reserved_root_directories)
  string(TOLOWER "${_root_directory}" _root_directory_key)
  string(TOLOWER "${_executable_name}" _executable_name_key)
  if(_executable_name_key STREQUAL _root_directory_key)
    message(FATAL_ERROR
      "Release executable cannot shadow the package directory: ${_executable_name}")
  endif()
endforeach()
if(_package_platform_lower STREQUAL "windows")
  string(TOLOWER "${_executable_name}" _executable_name_lower)
  if(NOT _executable_name_lower STREQUAL "sed.exe")
    message(FATAL_ERROR "Windows release executable must be named sed.exe")
  endif()
endif()

list(SORT _runtime_data)
set(_runtime_data_destinations)
foreach(_data_name IN LISTS _runtime_data)
  if(_data_name STREQUAL ""
      OR _data_name MATCHES "(^|/|\\\\)\\."
      OR _data_name MATCHES "^[A-Za-z]:[/\\\\]"
      OR _data_name MATCHES "^[/\\\\]"
      OR _data_name MATCHES "[\"\r\n]")
    message(FATAL_ERROR "Invalid runtime data name: ${_data_name}")
  endif()
  set(_data_source "${SED_PACKAGE_DATA_DIR}/${_data_name}")
  if(NOT EXISTS "${_data_source}" OR IS_DIRECTORY "${_data_source}")
    message(FATAL_ERROR "Required runtime data is missing: ${_data_source}")
  endif()
  reject_output_overlap("${_data_source}" "a runtime data file")
  file(TO_CMAKE_PATH "data/${_data_name}" _data_destination)
  register_destination("${_data_destination}" _runtime_data_destinations
    "Runtime data")
endforeach()

set(_project_notice_destinations)
foreach(_notice IN LISTS _project_license_files)
  require_nonempty_file("${_notice}" "Project license/notice")
  get_filename_component(_notice_name "${_notice}" NAME)
  if(_notice_name MATCHES "[\"\r\n]")
    message(FATAL_ERROR "Project notice filename cannot be represented in the release manifest: ${_notice_name}")
  endif()
  string(TOLOWER "${_notice_name}" _notice_name_key)
  foreach(_root_directory IN LISTS _reserved_root_directories)
    string(TOLOWER "${_root_directory}" _root_directory_key)
    if(_notice_name_key STREQUAL _root_directory_key)
      message(FATAL_ERROR
        "Project notice cannot shadow the package directory: ${_notice_name}")
    endif()
  endforeach()
  reject_output_overlap("${_notice}" "a project notice source file")
  register_destination("${_notice_name}" _root_file_destinations
    "Release root files")
  list(APPEND _project_notice_destinations "${_notice_name}")
endforeach()

validate_notice_set(_qt_license_files "licenses/qt" _qt_notice_destinations)
validate_notice_set(_qt_sbom_files "licenses/sbom" _qt_sbom_destinations)
validate_notice_set(_msvc_notice_files "licenses/msvc" _msvc_notice_destinations)

set(_msvc_runtime_destinations)
foreach(_runtime IN LISTS _msvc_runtime_files)
  require_nonempty_file("${_runtime}" "Selected MSVC runtime")
  get_filename_component(_runtime_name "${_runtime}" NAME)
  list(FIND _msvc_runtime_destinations "${_runtime_name}" _duplicate_runtime)
  if(NOT _duplicate_runtime EQUAL -1)
    message(FATAL_ERROR "Selected MSVC runtime basenames collide: ${_runtime_name}")
  endif()
  string(TOLOWER "${_runtime_name}" _runtime_name_key)
  foreach(_root_directory IN LISTS _reserved_root_directories)
    string(TOLOWER "${_root_directory}" _root_directory_key)
    if(_runtime_name_key STREQUAL _root_directory_key)
      message(FATAL_ERROR
        "Selected MSVC runtime cannot shadow the package directory: ${_runtime_name}")
    endif()
  endforeach()
  reject_output_overlap("${_runtime}" "an MSVC runtime source file")
  register_destination("${_runtime_name}" _root_file_destinations
    "Release root files")
  list(APPEND _msvc_runtime_destinations "${_runtime_name}")
endforeach()

file(REMOVE_RECURSE "${SED_PACKAGE_OUTPUT_DIR}")
file(MAKE_DIRECTORY "${SED_PACKAGE_OUTPUT_DIR}")

file(COPY_FILE "${SED_PACKAGE_EXECUTABLE}"
  "${SED_PACKAGE_OUTPUT_DIR}/${_executable_name}")

foreach(_data_name IN LISTS _runtime_data)
  set(_data_source "${SED_PACKAGE_DATA_DIR}/${_data_name}")
  get_filename_component(_data_parent "${_data_name}" DIRECTORY)
  file(MAKE_DIRECTORY "${SED_PACKAGE_OUTPUT_DIR}/data/${_data_parent}")
  file(COPY_FILE "${_data_source}"
    "${SED_PACKAGE_OUTPUT_DIR}/data/${_data_name}")
endforeach()

foreach(_notice IN LISTS _project_license_files)
  get_filename_component(_notice_name "${_notice}" NAME)
  file(COPY_FILE "${_notice}"
    "${SED_PACKAGE_OUTPUT_DIR}/${_notice_name}")
endforeach()

function(copy_notice_set SOURCE_LIST DESTINATION_ROOT)
  foreach(_notice IN LISTS ${SOURCE_LIST})
    get_filename_component(_notice_name "${_notice}" NAME)
    file(MAKE_DIRECTORY "${SED_PACKAGE_OUTPUT_DIR}/${DESTINATION_ROOT}")
    file(COPY_FILE "${_notice}"
      "${SED_PACKAGE_OUTPUT_DIR}/${DESTINATION_ROOT}/${_notice_name}")
  endforeach()
endfunction()

copy_notice_set(_qt_license_files "licenses/qt")
copy_notice_set(_qt_sbom_files "licenses/sbom")
copy_notice_set(_msvc_notice_files "licenses/msvc")

foreach(_runtime IN LISTS _msvc_runtime_files)
  get_filename_component(_runtime_name "${_runtime}" NAME)
  file(COPY_FILE "${_runtime}"
    "${SED_PACKAGE_OUTPUT_DIR}/${_runtime_name}")
endforeach()

set(_qt_system_dll_destinations)
if(DEFINED SED_PACKAGE_QT_DEPLOY_TOOL AND NOT SED_PACKAGE_QT_DEPLOY_TOOL STREQUAL "")
  if(NOT EXISTS "${SED_PACKAGE_QT_DEPLOY_TOOL}")
    message(FATAL_ERROR "Qt deployment tool is missing: ${SED_PACKAGE_QT_DEPLOY_TOOL}")
  endif()

  set(_qt_deploy_arguments
    --dir "${SED_PACKAGE_OUTPUT_DIR}"
    --release
    --no-translations
    --no-system-d3d-compiler
    --no-system-dxc-compiler
    --no-compiler-runtime
    --no-opengl-sw)

  if(_package_platform_lower STREQUAL "windows")
    if(DEFINED ENV{SystemRoot} AND NOT "$ENV{SystemRoot}" STREQUAL "")
      set(_windows_system_root "$ENV{SystemRoot}")
    elseif(DEFINED ENV{WINDIR} AND NOT "$ENV{WINDIR}" STREQUAL "")
      set(_windows_system_root "$ENV{WINDIR}")
    else()
      message(FATAL_ERROR
        "Cannot filter system DLLs because SystemRoot/WINDIR is not defined")
    endif()

    execute_process(
      COMMAND "${SED_PACKAGE_QT_DEPLOY_TOOL}"
        ${_qt_deploy_arguments}
        --dry-run
        --list mapping
        "${SED_PACKAGE_OUTPUT_DIR}/${_executable_name}"
      RESULT_VARIABLE _qt_mapping_result
      OUTPUT_VARIABLE _qt_mapping_output
      ERROR_VARIABLE _qt_mapping_error)
    if(NOT _qt_mapping_result EQUAL 0)
      message(FATAL_ERROR
        "Qt deployment dry-run mapping failed (${_qt_mapping_result}):\n"
        "${_qt_mapping_output}\n${_qt_mapping_error}")
    endif()
    set(_qt_mapping_text "${_qt_mapping_output}\n${_qt_mapping_error}")
    sed_release_collect_system_mapping(
      "${_qt_mapping_text}" "${SED_PACKAGE_OUTPUT_DIR}" "${_windows_system_root}"
      _qt_system_dll_destinations)
  endif()

  execute_process(
    COMMAND "${SED_PACKAGE_QT_DEPLOY_TOOL}"
      ${_qt_deploy_arguments}
      "${SED_PACKAGE_OUTPUT_DIR}/${_executable_name}"
    RESULT_VARIABLE _qt_deploy_result
    OUTPUT_VARIABLE _qt_deploy_output
    ERROR_VARIABLE _qt_deploy_error)
  if(NOT _qt_deploy_result EQUAL 0)
    message(FATAL_ERROR
      "Qt deployment failed (${_qt_deploy_result}):\n${_qt_deploy_output}\n${_qt_deploy_error}")
  endif()
elseif(_package_platform_lower STREQUAL "windows")
  message(FATAL_ERROR "A Qt deployment tool is required for the Windows package")
endif()

# Qt 6.10's windeployqt has no --no-system-icu switch and can copy the
# unversioned ICU DLL from Windows\System32.  Remove only the relative target
# paths whose dry-run source was proven to be under the current SystemRoot.
# Versioned ICU files sourced from the selected Qt kit are deliberately not in
# this list and remain package-local.
sed_release_remove_system_targets(
  "${SED_PACKAGE_OUTPUT_DIR}" _qt_system_dll_destinations)

# The deploy tool is allowed to add runtime DLLs/plugins, but source/build
# artifacts, settings, test binaries, and debug symbols are never allowed.
file(GLOB_RECURSE _package_entries RELATIVE "${SED_PACKAGE_OUTPUT_DIR}"
  "${SED_PACKAGE_OUTPUT_DIR}/*")
list(SORT _package_entries)
set(_manifest_entries)
foreach(_entry IN LISTS _package_entries)
  if(IS_DIRECTORY "${SED_PACKAGE_OUTPUT_DIR}/${_entry}")
    continue()
  endif()
  file(TO_CMAKE_PATH "${_entry}" _entry_normalized)
  string(TOLOWER "${_entry_normalized}" _entry_lower)
  if(_entry_normalized MATCHES "[\"\r\n]")
    message(FATAL_ERROR
      "Release package contains a path that cannot be represented in the manifest: ${_entry_normalized}")
  endif()
  if(_entry_normalized MATCHES "(^|/)(CMakeFiles|Testing|\.vs|\.git)(/|$)"
      OR _entry_normalized MATCHES "(^|/)(test|tests|sed_.*_tests|.*_test)"
      OR _entry_normalized MATCHES "\\.(pdb|ilk|obj|lib|exp|a|log|ini|sed)$")
    message(FATAL_ERROR "Release package contains a forbidden build/local artifact: ${_entry_normalized}")
  endif()
  if(_entry_normalized IN_LIST _project_notice_destinations
      OR _entry_normalized IN_LIST _runtime_data_destinations
      OR _entry_normalized IN_LIST _qt_notice_destinations
      OR _entry_normalized IN_LIST _qt_sbom_destinations
      OR _entry_normalized IN_LIST _msvc_notice_destinations
      OR _entry_normalized STREQUAL "RELEASE_MANIFEST.json"
      OR _entry_normalized STREQUAL "${_executable_name}"
      OR _entry_lower MATCHES "^(qt[56].*|icu.*|icudt.*|msvc.*|vcruntime.*|concrt.*|msvcp.*|vccorlib.*)\\.dll$"
      OR _entry_lower MATCHES "^(avcodec|avformat|avutil|swresample|swscale)-[^/]+\\.dll$"
      OR _entry_lower MATCHES "^(platforms|multimedia|styles|imageformats|iconengines|tls|networkinformation|generic)/[^/]+\\.dll$"
      OR _entry_lower STREQUAL "qt.conf")
    list(APPEND _manifest_entries "${_entry_normalized}")
    continue()
  endif()
  message(FATAL_ERROR "Release package contains an unapproved file: ${_entry_normalized}")
endforeach()

list(SORT _manifest_entries)
set(_manifest_path "${SED_PACKAGE_OUTPUT_DIR}/RELEASE_MANIFEST.json")
file(WRITE "${_manifest_path}"
  "{\n  \"format\": 1,\n"
  "  \"version\": \"${SED_PACKAGE_VERSION}\",\n"
  "  \"platform\": \"${_package_platform}\",\n"
  "  \"qt_version\": \"${SED_PACKAGE_QT_VERSION}\",\n"
  "  \"manifest_self_excluded\": true,\n"
  "  \"files\": [\n")
list(LENGTH _manifest_entries _manifest_count)
set(_manifest_index 0)
foreach(_entry IN LISTS _manifest_entries)
  math(EXPR _manifest_index "${_manifest_index} + 1")
  file(SIZE "${SED_PACKAGE_OUTPUT_DIR}/${_entry}" _entry_size)
  file(SHA256 "${SED_PACKAGE_OUTPUT_DIR}/${_entry}" _entry_hash)
  if(_manifest_index LESS _manifest_count)
    set(_comma ",")
  else()
    set(_comma "")
  endif()
  file(APPEND "${_manifest_path}"
    "    {\"path\":\"${_entry}\",\"bytes\":${_entry_size},\"sha256\":\"${_entry_hash}\"}${_comma}\n")
endforeach()
file(APPEND "${_manifest_path}" "  ]\n}\n")

message(STATUS "Staged clean SED release package at ${SED_PACKAGE_OUTPUT_DIR}")
