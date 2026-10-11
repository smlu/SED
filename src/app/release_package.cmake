cmake_minimum_required(VERSION 3.21)

# This entry point performs release-kit discovery only when the explicit
# sed_release_package target is invoked.  Ordinary configure/build work must
# not require installed Qt or Visual Studio notice files to be complete.

function(require_value NAME)
  if(NOT DEFINED ${NAME} OR "${${NAME}}" STREQUAL "")
    message(FATAL_ERROR "${NAME} must be provided")
  endif()
endfunction()

function(split_pipe_list VALUE OUTPUT_VARIABLE)
  set(_items "${VALUE}")
  string(REPLACE "|" ";" _items "${_items}")
  set(${OUTPUT_VARIABLE} "${_items}" PARENT_SCOPE)
endfunction()

function(join_pipe_list INPUT_LIST OUTPUT_VARIABLE)
  set(_items "${${INPUT_LIST}}")
  string(REPLACE ";" "|" _items "${_items}")
  set(${OUTPUT_VARIABLE} "${_items}" PARENT_SCOPE)
endfunction()

require_value(SED_RELEASE_STAGE_SCRIPT)
require_value(SED_PACKAGE_EXECUTABLE)
require_value(SED_PACKAGE_OUTPUT_DIR)
require_value(SED_PACKAGE_DATA_DIR)
require_value(SED_PACKAGE_RUNTIME_DATA)
require_value(SED_PACKAGE_PROJECT_LICENSE_FILES)
require_value(SED_PACKAGE_VERSION)
require_value(SED_PACKAGE_PLATFORM)
require_value(SED_PACKAGE_CONFIGURATION)
require_value(SED_RELEASE_QT_VERSION)
require_value(SED_RELEASE_QT_DEPLOY_TOOL)

if(NOT "${SED_PACKAGE_CONFIGURATION}" STREQUAL "Release")
  message(FATAL_ERROR
    "sed_release_package must be invoked with the Release configuration; got: "
    "${SED_PACKAGE_CONFIGURATION}")
endif()

string(TOLOWER "${SED_PACKAGE_PLATFORM}" _package_platform_lower)
if(NOT _package_platform_lower STREQUAL "windows")
  message(FATAL_ERROR
    "sed_release_package requires a Windows configuration; got: ${SED_PACKAGE_PLATFORM}")
endif()

set(_package_msvc FALSE)
if(DEFINED SED_PACKAGE_MSVC
    AND "${SED_PACKAGE_MSVC}" MATCHES "^(1|ON|TRUE|YES|Y)$")
  set(_package_msvc TRUE)
endif()

if(DEFINED SED_RELEASE_QT_REQUIRED_VERSION
    AND NOT "${SED_RELEASE_QT_REQUIRED_VERSION}" STREQUAL ""
    AND NOT "${SED_RELEASE_QT_VERSION}" VERSION_EQUAL "${SED_RELEASE_QT_REQUIRED_VERSION}")
  message(FATAL_ERROR
    "sed_release_package requires Qt ${SED_RELEASE_QT_REQUIRED_VERSION}; selected Qt is "
    "${SED_RELEASE_QT_VERSION}")
endif()

require_value(SED_RELEASE_QT_VERSION_ROOT)
require_value(SED_RELEASE_QT_KIT_ROOT)

get_filename_component(_qt_parent_root
  "${SED_RELEASE_QT_VERSION_ROOT}" DIRECTORY)

if(DEFINED SED_RELEASE_QT_LICENSE_FILES
    AND NOT "${SED_RELEASE_QT_LICENSE_FILES}" STREQUAL "")
  split_pipe_list("${SED_RELEASE_QT_LICENSE_FILES}" _qt_license_files)
else()
  set(_qt_license_files
    "${_qt_parent_root}/Licenses/LICENSE")
  set(_qt_docs_root
    "${_qt_parent_root}/Docs/Qt-${SED_RELEASE_QT_VERSION}")
  foreach(_qt_module IN ITEMS qtcore qtgui qtnetwork qtmultimedia qtsvg)
    file(GLOB _qt_module_attributions LIST_DIRECTORIES false
      "${_qt_docs_root}/${_qt_module}/*-attribution-*.html")
    list(SORT _qt_module_attributions)
    if(NOT _qt_module_attributions)
      message(FATAL_ERROR
        "Qt ${SED_RELEASE_QT_VERSION} attribution HTML is missing for ${_qt_module}: "
        "${_qt_docs_root}/${_qt_module}/*-attribution-*.html")
    endif()
    list(APPEND _qt_license_files ${_qt_module_attributions})
  endforeach()
endif()

if(DEFINED SED_RELEASE_QT_SBOM_FILES
    AND NOT "${SED_RELEASE_QT_SBOM_FILES}" STREQUAL "")
  split_pipe_list("${SED_RELEASE_QT_SBOM_FILES}" _qt_sbom_files)
else()
  set(_qt_sbom_files)
  foreach(_qt_module IN ITEMS qtbase qtmultimedia qtsvg)
    set(_qt_sbom_file
      "${SED_RELEASE_QT_KIT_ROOT}/sbom/${_qt_module}-${SED_RELEASE_QT_VERSION}.spdx.json")
    list(APPEND _qt_sbom_files "${_qt_sbom_file}")
  endforeach()
endif()

set(_msvc_notice_files)
if(_package_msvc)
  if(DEFINED SED_RELEASE_MSVC_NOTICE_FILES
      AND NOT "${SED_RELEASE_MSVC_NOTICE_FILES}" STREQUAL "")
    split_pipe_list("${SED_RELEASE_MSVC_NOTICE_FILES}" _msvc_notice_files)
  else()
    require_value(SED_RELEASE_MSVC_VS_ROOT)
    set(_msvc_notice_files
      "${SED_RELEASE_MSVC_VS_ROOT}/Licenses/1033/ThirdPartyNotices.txt"
      "${SED_RELEASE_MSVC_VS_ROOT}/Licenses/1033/Redist.txt")
  endif()
endif()

if(DEFINED SED_RELEASE_MSVC_RUNTIME_FILES
    AND NOT "${SED_RELEASE_MSVC_RUNTIME_FILES}" STREQUAL "")
  set(_msvc_runtime_files "${SED_RELEASE_MSVC_RUNTIME_FILES}")
else()
  set(_msvc_runtime_files "")
endif()

join_pipe_list(_qt_license_files _qt_license_files_serialized)
join_pipe_list(_qt_sbom_files _qt_sbom_files_serialized)
join_pipe_list(_msvc_notice_files _msvc_notice_files_serialized)

set(SED_PACKAGE_QT_VERSION "${SED_RELEASE_QT_VERSION}")
set(SED_PACKAGE_QT_LICENSE_FILES "${_qt_license_files_serialized}")
set(SED_PACKAGE_QT_SBOM_FILES "${_qt_sbom_files_serialized}")
set(SED_PACKAGE_QT_DEPLOY_TOOL "${SED_RELEASE_QT_DEPLOY_TOOL}")
set(SED_PACKAGE_MSVC_NOTICE_FILES "${_msvc_notice_files_serialized}")
set(SED_PACKAGE_MSVC_RUNTIME_FILES "${_msvc_runtime_files}")

include("${SED_RELEASE_STAGE_SCRIPT}")
