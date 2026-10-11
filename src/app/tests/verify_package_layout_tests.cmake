cmake_minimum_required(VERSION 3.21)

if(NOT DEFINED SED_PACKAGE_LAYOUT_VERIFIER
    OR SED_PACKAGE_LAYOUT_VERIFIER STREQUAL "")
  message(FATAL_ERROR "SED_PACKAGE_LAYOUT_VERIFIER was not provided")
endif()

if(NOT DEFINED SED_PACKAGE_LAYOUT_TEST_ROOT
    OR SED_PACKAGE_LAYOUT_TEST_ROOT STREQUAL "")
  message(FATAL_ERROR "SED_PACKAGE_LAYOUT_TEST_ROOT was not provided")
endif()

set(REQUIRED_RUNTIME_DATA
  "runtime.dat|master.tpl|mots.tpl|ijim.tpl|shapes.tpl")

function(run_package_layout_case CASE_NAME EXPECT_SUCCESS EXECUTABLE_NAME
    EXECUTABLE_CONTENT RUNTIME_KIND TEMPLATE_KIND TARGET_TEMPLATE RUNTIME_DATA)
  set(CASE_ROOT "${SED_PACKAGE_LAYOUT_TEST_ROOT}/${CASE_NAME}")
  set(EXECUTABLE_PATH "${CASE_ROOT}/${EXECUTABLE_NAME}")

  file(REMOVE_RECURSE "${CASE_ROOT}")
  file(MAKE_DIRECTORY "${CASE_ROOT}/data")

  foreach(TEMPLATE_DATABASE IN ITEMS master.tpl mots.tpl ijim.tpl shapes.tpl)
    if(TEMPLATE_DATABASE STREQUAL "${TARGET_TEMPLATE}"
        AND TEMPLATE_KIND STREQUAL "missing")
      continue()
    endif()
    if(TEMPLATE_DATABASE STREQUAL "${TARGET_TEMPLATE}"
        AND TEMPLATE_KIND STREQUAL "directory")
      file(MAKE_DIRECTORY "${CASE_ROOT}/data/${TEMPLATE_DATABASE}")
    elseif(TEMPLATE_DATABASE STREQUAL "${TARGET_TEMPLATE}"
        AND TEMPLATE_KIND STREQUAL "empty")
      file(WRITE "${CASE_ROOT}/data/${TEMPLATE_DATABASE}" "")
    else()
      file(WRITE "${CASE_ROOT}/data/${TEMPLATE_DATABASE}" "template")
    endif()
  endforeach()

  if(NOT EXECUTABLE_CONTENT STREQUAL "missing")
    file(WRITE "${EXECUTABLE_PATH}" "${EXECUTABLE_CONTENT}")
  endif()

  if(RUNTIME_KIND STREQUAL "file")
    file(WRITE "${CASE_ROOT}/data/runtime.dat" "runtime")
  elseif(RUNTIME_KIND STREQUAL "empty")
    file(WRITE "${CASE_ROOT}/data/runtime.dat" "")
  elseif(RUNTIME_KIND STREQUAL "directory")
    file(MAKE_DIRECTORY "${CASE_ROOT}/data/runtime.dat")
  elseif(NOT RUNTIME_KIND STREQUAL "missing")
    message(FATAL_ERROR "Unknown runtime fixture kind: ${RUNTIME_KIND}")
  endif()

  execute_process(
    COMMAND "${CMAKE_COMMAND}"
      "-DSED_PACKAGE_EXECUTABLE=${EXECUTABLE_PATH}"
      "-DSED_PACKAGE_PLATFORM=Linux"
      "-DSED_PACKAGE_QT_MAJOR=6"
      "-DSED_PACKAGE_DEBUG=FALSE"
      "-DSED_PACKAGE_RUNTIME_DATA=${RUNTIME_DATA}"
      -P "${SED_PACKAGE_LAYOUT_VERIFIER}"
    RESULT_VARIABLE RESULT
    OUTPUT_VARIABLE OUTPUT
    ERROR_VARIABLE ERROR
  )

  if(EXPECT_SUCCESS AND NOT RESULT EQUAL 0)
    message(FATAL_ERROR
      "${CASE_NAME} unexpectedly failed (${RESULT}):\n${OUTPUT}\n${ERROR}")
  endif()
  if(NOT EXPECT_SUCCESS AND RESULT EQUAL 0)
    message(FATAL_ERROR "${CASE_NAME} unexpectedly passed")
  endif()
endfunction()

file(REMOVE_RECURSE "${SED_PACKAGE_LAYOUT_TEST_ROOT}")

run_package_layout_case(
  valid TRUE sed-test "executable" file file "" "${REQUIRED_RUNTIME_DATA}")
run_package_layout_case(
  missing_executable FALSE sed-test missing file file ""
  "${REQUIRED_RUNTIME_DATA}")
run_package_layout_case(
  empty_executable FALSE sed-test "" file file "" "${REQUIRED_RUNTIME_DATA}")
run_package_layout_case(
  missing_runtime FALSE sed-test "executable" missing file ""
  "${REQUIRED_RUNTIME_DATA}")
run_package_layout_case(
  directory_runtime FALSE sed-test "executable" directory file ""
  "${REQUIRED_RUNTIME_DATA}")
run_package_layout_case(
  empty_runtime TRUE sed-test "executable" empty file ""
  "${REQUIRED_RUNTIME_DATA}")

foreach(TEMPLATE_DATABASE IN ITEMS master.tpl mots.tpl ijim.tpl shapes.tpl)
  foreach(BROKEN_KIND IN ITEMS missing directory empty)
    string(REPLACE "." "_" TEMPLATE_CASE_NAME "${TEMPLATE_DATABASE}")
    run_package_layout_case(
      "${BROKEN_KIND}_${TEMPLATE_CASE_NAME}"
      FALSE
      sed-test
      "executable"
      file
      "${BROKEN_KIND}"
      "${TEMPLATE_DATABASE}"
      "runtime.dat")
  endforeach()
endforeach()

set(MISSING_ARGUMENT_ROOT
  "${SED_PACKAGE_LAYOUT_TEST_ROOT}/missing_argument")
file(MAKE_DIRECTORY "${MISSING_ARGUMENT_ROOT}/data")
file(WRITE "${MISSING_ARGUMENT_ROOT}/sed-test" "executable")
file(WRITE "${MISSING_ARGUMENT_ROOT}/data/runtime.dat" "runtime")
foreach(TEMPLATE_DATABASE IN ITEMS master.tpl mots.tpl ijim.tpl shapes.tpl)
  file(WRITE "${MISSING_ARGUMENT_ROOT}/data/${TEMPLATE_DATABASE}" "template")
endforeach()

function(run_missing_argument_case OMITTED_ARGUMENT)
  set(COMMAND_ARGS "${CMAKE_COMMAND}")
  if(NOT OMITTED_ARGUMENT STREQUAL "executable")
    list(APPEND COMMAND_ARGS
      "-DSED_PACKAGE_EXECUTABLE=${MISSING_ARGUMENT_ROOT}/sed-test")
  endif()
  if(NOT OMITTED_ARGUMENT STREQUAL "platform")
    list(APPEND COMMAND_ARGS "-DSED_PACKAGE_PLATFORM=Linux")
  endif()
  if(NOT OMITTED_ARGUMENT STREQUAL "qt_major")
    list(APPEND COMMAND_ARGS "-DSED_PACKAGE_QT_MAJOR=6")
  endif()
  if(NOT OMITTED_ARGUMENT STREQUAL "debug")
    list(APPEND COMMAND_ARGS "-DSED_PACKAGE_DEBUG=FALSE")
  endif()
  if(NOT OMITTED_ARGUMENT STREQUAL "runtime_data")
    list(APPEND COMMAND_ARGS
      "-DSED_PACKAGE_RUNTIME_DATA=${REQUIRED_RUNTIME_DATA}")
  endif()
  list(APPEND COMMAND_ARGS -P "${SED_PACKAGE_LAYOUT_VERIFIER}")

  execute_process(
    COMMAND ${COMMAND_ARGS}
    RESULT_VARIABLE RESULT
    OUTPUT_VARIABLE OUTPUT
    ERROR_VARIABLE ERROR
  )
  if(RESULT EQUAL 0)
    message(FATAL_ERROR
      "missing_${OMITTED_ARGUMENT}_argument unexpectedly passed")
  endif()
endfunction()

foreach(REQUIRED_ARGUMENT IN ITEMS
    executable
    platform
    qt_major
    debug
    runtime_data)
  run_missing_argument_case(${REQUIRED_ARGUMENT})
endforeach()

execute_process(
  COMMAND "${CMAKE_COMMAND}"
    "-DSED_PACKAGE_EXECUTABLE=${MISSING_ARGUMENT_ROOT}/sed-test"
    "-DSED_PACKAGE_PLATFORM=Linux"
    "-DSED_PACKAGE_QT_MAJOR=7"
    "-DSED_PACKAGE_DEBUG=FALSE"
    "-DSED_PACKAGE_RUNTIME_DATA=${REQUIRED_RUNTIME_DATA}"
    -P "${SED_PACKAGE_LAYOUT_VERIFIER}"
  RESULT_VARIABLE UNSUPPORTED_QT_RESULT
  OUTPUT_VARIABLE UNSUPPORTED_QT_OUTPUT
  ERROR_VARIABLE UNSUPPORTED_QT_ERROR
)
if(UNSUPPORTED_QT_RESULT EQUAL 0)
  message(FATAL_ERROR "unsupported_qt_major unexpectedly passed")
endif()

execute_process(
  COMMAND "${CMAKE_COMMAND}"
    "-DSED_PACKAGE_EXECUTABLE=${MISSING_ARGUMENT_ROOT}/sed-test"
    "-DSED_PACKAGE_PLATFORM=Linux"
    "-DSED_PACKAGE_QT_MAJOR=6"
    "-DSED_PACKAGE_DEBUG=not-a-boolean"
    "-DSED_PACKAGE_RUNTIME_DATA=${REQUIRED_RUNTIME_DATA}"
    -P "${SED_PACKAGE_LAYOUT_VERIFIER}"
  RESULT_VARIABLE INVALID_DEBUG_RESULT
  OUTPUT_VARIABLE INVALID_DEBUG_OUTPUT
  ERROR_VARIABLE INVALID_DEBUG_ERROR
)
if(INVALID_DEBUG_RESULT EQUAL 0)
  message(FATAL_ERROR "invalid_debug_argument unexpectedly passed")
endif()

set(WINDOWS_ROOT "${SED_PACKAGE_LAYOUT_TEST_ROOT}/wrong_windows_name")
file(MAKE_DIRECTORY "${WINDOWS_ROOT}/data")
file(WRITE "${WINDOWS_ROOT}/sed-test.exe" "executable")
file(WRITE "${WINDOWS_ROOT}/data/runtime.dat" "runtime")
foreach(TEMPLATE_DATABASE IN ITEMS master.tpl mots.tpl ijim.tpl shapes.tpl)
  file(WRITE "${WINDOWS_ROOT}/data/${TEMPLATE_DATABASE}" "template")
endforeach()
execute_process(
  COMMAND "${CMAKE_COMMAND}"
    "-DSED_PACKAGE_EXECUTABLE=${WINDOWS_ROOT}/sed-test.exe"
    "-DSED_PACKAGE_PLATFORM=Windows"
    "-DSED_PACKAGE_QT_MAJOR=6"
    "-DSED_PACKAGE_DEBUG=FALSE"
    "-DSED_PACKAGE_RUNTIME_DATA=${REQUIRED_RUNTIME_DATA}"
    -P "${SED_PACKAGE_LAYOUT_VERIFIER}"
  RESULT_VARIABLE WRONG_NAME_RESULT
  OUTPUT_VARIABLE WRONG_NAME_OUTPUT
  ERROR_VARIABLE WRONG_NAME_ERROR
)
if(WRONG_NAME_RESULT EQUAL 0)
  message(FATAL_ERROR "wrong_windows_name unexpectedly passed")
endif()

set(QT_RUNTIME_BASES
  Qt6Core
  Qt6Gui
  Qt6Widgets
  Qt6OpenGL
  Qt6OpenGLWidgets
  Qt6Multimedia
  Qt6Network)

# Release binaries built with the selected MSVC kit import the app-local
# Universal CRT-compatible C++ runtime. Keep these fixtures explicit so the
# package verifier cannot silently regress to a Qt/data-only check.
set(MSVC_RUNTIME_DLLS
  MSVCP140.dll
  MSVCP140_2.dll
  VCRUNTIME140.dll
  VCRUNTIME140_1.dll)

function(prepare_windows_package CASE_NAME DEBUG_BUILD MULTIMEDIA_KIND
    BROKEN_KIND)
  set(CASE_ROOT "${SED_PACKAGE_LAYOUT_TEST_ROOT}/${CASE_NAME}")
  set(QT_SUFFIX "")
  if(DEBUG_BUILD)
    set(QT_SUFFIX "d")
  endif()

  file(REMOVE_RECURSE "${CASE_ROOT}")
  file(MAKE_DIRECTORY "${CASE_ROOT}/data")
  file(MAKE_DIRECTORY "${CASE_ROOT}/platforms")
  file(MAKE_DIRECTORY "${CASE_ROOT}/multimedia")
  file(WRITE "${CASE_ROOT}/sed.exe" "executable")
  file(WRITE "${CASE_ROOT}/data/runtime.dat" "runtime")
  foreach(TEMPLATE_DATABASE IN ITEMS master.tpl mots.tpl ijim.tpl shapes.tpl)
    file(WRITE "${CASE_ROOT}/data/${TEMPLATE_DATABASE}" "template")
  endforeach()
  if(NOT DEBUG_BUILD)
    foreach(MSVC_RUNTIME_DLL IN LISTS MSVC_RUNTIME_DLLS)
      file(WRITE "${CASE_ROOT}/${MSVC_RUNTIME_DLL}" "runtime")
    endforeach()
  endif()
  foreach(QT_RUNTIME_BASE IN LISTS QT_RUNTIME_BASES)
    file(WRITE "${CASE_ROOT}/${QT_RUNTIME_BASE}${QT_SUFFIX}.dll" "runtime")
  endforeach()
  file(WRITE "${CASE_ROOT}/platforms/qwindows${QT_SUFFIX}.dll" "plugin")

  if(MULTIMEDIA_KIND STREQUAL "ffmpeg")
    file(WRITE
      "${CASE_ROOT}/multimedia/ffmpegmediaplugin${QT_SUFFIX}.dll"
      "plugin")
  elseif(MULTIMEDIA_KIND STREQUAL "windows")
    file(WRITE
      "${CASE_ROOT}/multimedia/windowsmediaplugin${QT_SUFFIX}.dll"
      "plugin")
  elseif(MULTIMEDIA_KIND STREQUAL "empty")
    file(WRITE
      "${CASE_ROOT}/multimedia/ffmpegmediaplugin${QT_SUFFIX}.dll"
      "")
    file(WRITE
      "${CASE_ROOT}/multimedia/windowsmediaplugin${QT_SUFFIX}.dll"
      "")
  elseif(MULTIMEDIA_KIND STREQUAL "directory")
    file(MAKE_DIRECTORY
      "${CASE_ROOT}/multimedia/ffmpegmediaplugin${QT_SUFFIX}.dll")
  elseif(NOT MULTIMEDIA_KIND STREQUAL "missing")
    message(FATAL_ERROR "Unknown multimedia fixture kind: ${MULTIMEDIA_KIND}")
  endif()

  if(BROKEN_KIND STREQUAL "missing_runtime")
    file(REMOVE "${CASE_ROOT}/Qt6Gui${QT_SUFFIX}.dll")
  elseif(BROKEN_KIND STREQUAL "directory_runtime")
    file(REMOVE "${CASE_ROOT}/Qt6Gui${QT_SUFFIX}.dll")
    file(MAKE_DIRECTORY "${CASE_ROOT}/Qt6Gui${QT_SUFFIX}.dll")
  elseif(BROKEN_KIND STREQUAL "empty_runtime")
    file(WRITE "${CASE_ROOT}/Qt6Gui${QT_SUFFIX}.dll" "")
  elseif(BROKEN_KIND STREQUAL "missing_platform")
    file(REMOVE "${CASE_ROOT}/platforms/qwindows${QT_SUFFIX}.dll")
  elseif(BROKEN_KIND STREQUAL "directory_platform")
    file(REMOVE "${CASE_ROOT}/platforms/qwindows${QT_SUFFIX}.dll")
    file(MAKE_DIRECTORY "${CASE_ROOT}/platforms/qwindows${QT_SUFFIX}.dll")
  elseif(BROKEN_KIND STREQUAL "empty_platform")
    file(WRITE "${CASE_ROOT}/platforms/qwindows${QT_SUFFIX}.dll" "")
  elseif(BROKEN_KIND STREQUAL "missing_msvc_runtime")
    list(GET MSVC_RUNTIME_DLLS 0 PRIMARY_MSVC_RUNTIME_DLL)
    file(REMOVE "${CASE_ROOT}/${PRIMARY_MSVC_RUNTIME_DLL}")
  elseif(BROKEN_KIND STREQUAL "directory_msvc_runtime")
    list(GET MSVC_RUNTIME_DLLS 0 PRIMARY_MSVC_RUNTIME_DLL)
    file(REMOVE "${CASE_ROOT}/${PRIMARY_MSVC_RUNTIME_DLL}")
    file(MAKE_DIRECTORY "${CASE_ROOT}/${PRIMARY_MSVC_RUNTIME_DLL}")
  elseif(BROKEN_KIND STREQUAL "empty_msvc_runtime")
    list(GET MSVC_RUNTIME_DLLS 0 PRIMARY_MSVC_RUNTIME_DLL)
    file(WRITE "${CASE_ROOT}/${PRIMARY_MSVC_RUNTIME_DLL}" "")
  elseif(NOT BROKEN_KIND STREQUAL "none")
    message(FATAL_ERROR "Unknown broken fixture kind: ${BROKEN_KIND}")
  endif()
endfunction()

function(run_windows_package_case CASE_NAME EXPECT_SUCCESS DEBUG_BUILD
    EXPECTED_ERROR)
  execute_process(
    COMMAND "${CMAKE_COMMAND}"
      "-DSED_PACKAGE_EXECUTABLE=${SED_PACKAGE_LAYOUT_TEST_ROOT}/${CASE_NAME}/sed.exe"
      "-DSED_PACKAGE_PLATFORM=Windows"
      "-DSED_PACKAGE_MSVC=TRUE"
      "-DSED_PACKAGE_QT_MAJOR=6"
      "-DSED_PACKAGE_DEBUG=${DEBUG_BUILD}"
      "-DSED_PACKAGE_RUNTIME_DATA=${REQUIRED_RUNTIME_DATA}"
      -P "${SED_PACKAGE_LAYOUT_VERIFIER}"
    RESULT_VARIABLE RESULT
    OUTPUT_VARIABLE OUTPUT
    ERROR_VARIABLE ERROR
  )

  if(EXPECT_SUCCESS AND NOT RESULT EQUAL 0)
    message(FATAL_ERROR
      "${CASE_NAME} unexpectedly failed (${RESULT}):\n${OUTPUT}\n${ERROR}")
  endif()
  if(NOT EXPECT_SUCCESS AND RESULT EQUAL 0)
    message(FATAL_ERROR "${CASE_NAME} unexpectedly passed")
  endif()
  if(NOT EXPECTED_ERROR STREQUAL "")
    string(REGEX REPLACE "[ \t\r\n]+" " " NORMALIZED_ERROR "${ERROR}")
    string(REGEX REPLACE "[ \t\r\n]+" " " NORMALIZED_EXPECTED_ERROR
      "${EXPECTED_ERROR}")
    string(FIND "${NORMALIZED_ERROR}" "${NORMALIZED_EXPECTED_ERROR}"
      EXPECTED_ERROR_POSITION)
    if(EXPECTED_ERROR_POSITION EQUAL -1)
      message(FATAL_ERROR
        "${CASE_NAME} emitted an unexpected diagnostic. Expected exact substring:\n"
        "${EXPECTED_ERROR}\nActual stderr:\n${ERROR}")
    endif()
  endif()
endfunction()

set(QT5_WINDOWS_ROOT "${SED_PACKAGE_LAYOUT_TEST_ROOT}/valid_windows_qt5")
file(MAKE_DIRECTORY "${QT5_WINDOWS_ROOT}/data")
file(WRITE "${QT5_WINDOWS_ROOT}/sed.exe" "executable")
file(WRITE "${QT5_WINDOWS_ROOT}/data/runtime.dat" "runtime")
foreach(TEMPLATE_DATABASE IN ITEMS master.tpl mots.tpl ijim.tpl shapes.tpl)
  file(WRITE "${QT5_WINDOWS_ROOT}/data/${TEMPLATE_DATABASE}" "template")
endforeach()
foreach(MSVC_RUNTIME_DLL IN LISTS MSVC_RUNTIME_DLLS)
  file(WRITE "${QT5_WINDOWS_ROOT}/${MSVC_RUNTIME_DLL}" "runtime")
endforeach()
execute_process(
  COMMAND "${CMAKE_COMMAND}"
    "-DSED_PACKAGE_EXECUTABLE=${QT5_WINDOWS_ROOT}/sed.exe"
    "-DSED_PACKAGE_PLATFORM=Windows"
    "-DSED_PACKAGE_MSVC=TRUE"
    "-DSED_PACKAGE_QT_MAJOR=5"
    "-DSED_PACKAGE_DEBUG=FALSE"
    "-DSED_PACKAGE_RUNTIME_DATA=${REQUIRED_RUNTIME_DATA}"
    -P "${SED_PACKAGE_LAYOUT_VERIFIER}"
  RESULT_VARIABLE QT5_WINDOWS_RESULT
  OUTPUT_VARIABLE QT5_WINDOWS_OUTPUT
  ERROR_VARIABLE QT5_WINDOWS_ERROR
)
if(NOT QT5_WINDOWS_RESULT EQUAL 0)
  message(FATAL_ERROR
    "valid_windows_qt5 unexpectedly failed (${QT5_WINDOWS_RESULT}):\n"
    "${QT5_WINDOWS_OUTPUT}\n${QT5_WINDOWS_ERROR}")
endif()

prepare_windows_package(valid_windows FALSE ffmpeg none)
run_windows_package_case(valid_windows TRUE FALSE "")
prepare_windows_package(non_msvc_windows FALSE ffmpeg none)
foreach(MSVC_RUNTIME_DLL IN LISTS MSVC_RUNTIME_DLLS)
  file(REMOVE
    "${SED_PACKAGE_LAYOUT_TEST_ROOT}/non_msvc_windows/${MSVC_RUNTIME_DLL}")
endforeach()
execute_process(
  COMMAND "${CMAKE_COMMAND}"
    "-DSED_PACKAGE_EXECUTABLE=${SED_PACKAGE_LAYOUT_TEST_ROOT}/non_msvc_windows/sed.exe"
    "-DSED_PACKAGE_PLATFORM=Windows"
    "-DSED_PACKAGE_MSVC=FALSE"
    "-DSED_PACKAGE_QT_MAJOR=6"
    "-DSED_PACKAGE_DEBUG=FALSE"
    "-DSED_PACKAGE_RUNTIME_DATA=${REQUIRED_RUNTIME_DATA}"
    -P "${SED_PACKAGE_LAYOUT_VERIFIER}"
  RESULT_VARIABLE NON_MSVC_RESULT
  OUTPUT_VARIABLE NON_MSVC_OUTPUT
  ERROR_VARIABLE NON_MSVC_ERROR
)
if(NOT NON_MSVC_RESULT EQUAL 0)
  message(FATAL_ERROR
    "non_msvc_windows unexpectedly failed (${NON_MSVC_RESULT}):\n"
    "${NON_MSVC_OUTPUT}\n${NON_MSVC_ERROR}")
endif()
execute_process(
  COMMAND "${CMAKE_COMMAND}"
    "-DSED_PACKAGE_EXECUTABLE=${SED_PACKAGE_LAYOUT_TEST_ROOT}/non_msvc_windows/sed.exe"
    "-DSED_PACKAGE_PLATFORM=Windows"
    "-DSED_PACKAGE_MSVC=not-a-boolean"
    "-DSED_PACKAGE_QT_MAJOR=6"
    "-DSED_PACKAGE_DEBUG=FALSE"
    "-DSED_PACKAGE_RUNTIME_DATA=${REQUIRED_RUNTIME_DATA}"
    -P "${SED_PACKAGE_LAYOUT_VERIFIER}"
  RESULT_VARIABLE INVALID_MSVC_RESULT
  OUTPUT_VARIABLE INVALID_MSVC_OUTPUT
  ERROR_VARIABLE INVALID_MSVC_ERROR
)
if(INVALID_MSVC_RESULT EQUAL 0)
  message(FATAL_ERROR "invalid_msvc_argument unexpectedly passed")
endif()
string(REGEX REPLACE "[ \t\r\n]+" " " INVALID_MSVC_DIAGNOSTIC
  "${INVALID_MSVC_ERROR}")
string(FIND "${INVALID_MSVC_DIAGNOSTIC}"
  "SED_PACKAGE_MSVC must be a Boolean value"
  INVALID_MSVC_DIAGNOSTIC_POSITION)
if(INVALID_MSVC_DIAGNOSTIC_POSITION EQUAL -1)
  message(FATAL_ERROR
    "invalid_msvc_argument emitted an unexpected diagnostic:\n"
    "${INVALID_MSVC_ERROR}")
endif()
prepare_windows_package(valid_windows_debug TRUE ffmpeg none)
run_windows_package_case(valid_windows_debug TRUE TRUE "")
prepare_windows_package(multimedia_fallback FALSE windows none)
run_windows_package_case(multimedia_fallback TRUE FALSE "")
foreach(QT_RUNTIME_BASE IN LISTS QT_RUNTIME_BASES)
  set(CASE_NAME "missing_${QT_RUNTIME_BASE}")
  prepare_windows_package(${CASE_NAME} FALSE ffmpeg none)
  file(REMOVE
    "${SED_PACKAGE_LAYOUT_TEST_ROOT}/${CASE_NAME}/${QT_RUNTIME_BASE}.dll")
  run_windows_package_case(${CASE_NAME} FALSE FALSE "")
endforeach()
foreach(MSVC_RUNTIME_DLL IN LISTS MSVC_RUNTIME_DLLS)
  string(REPLACE "." "_" MSVC_RUNTIME_CASE_SUFFIX "${MSVC_RUNTIME_DLL}")
  set(CASE_NAME "missing_${MSVC_RUNTIME_CASE_SUFFIX}")
  prepare_windows_package(${CASE_NAME} FALSE ffmpeg none)
  set(MSVC_RUNTIME_PATH
    "${SED_PACKAGE_LAYOUT_TEST_ROOT}/${CASE_NAME}/${MSVC_RUNTIME_DLL}")
  file(REMOVE "${MSVC_RUNTIME_PATH}")
  run_windows_package_case(
    ${CASE_NAME}
    FALSE
    FALSE
    "Required packaged MSVC runtime is missing: ${MSVC_RUNTIME_PATH}")
endforeach()
list(GET MSVC_RUNTIME_DLLS 0 PRIMARY_MSVC_RUNTIME_DLL)
set(PRIMARY_MSVC_RUNTIME_PATH
  "${SED_PACKAGE_LAYOUT_TEST_ROOT}/directory_msvc_runtime/${PRIMARY_MSVC_RUNTIME_DLL}")
prepare_windows_package(directory_msvc_runtime FALSE ffmpeg directory_msvc_runtime)
run_windows_package_case(
  directory_msvc_runtime
  FALSE
  FALSE
  "Required packaged MSVC runtime is a directory: ${PRIMARY_MSVC_RUNTIME_PATH}")
set(PRIMARY_MSVC_RUNTIME_PATH
  "${SED_PACKAGE_LAYOUT_TEST_ROOT}/empty_msvc_runtime/${PRIMARY_MSVC_RUNTIME_DLL}")
prepare_windows_package(empty_msvc_runtime FALSE ffmpeg empty_msvc_runtime)
run_windows_package_case(
  empty_msvc_runtime
  FALSE
  FALSE
  "Required packaged MSVC runtime is empty: ${PRIMARY_MSVC_RUNTIME_PATH}")
prepare_windows_package(directory_qt_runtime FALSE ffmpeg directory_runtime)
run_windows_package_case(directory_qt_runtime FALSE FALSE "")
prepare_windows_package(empty_qt_runtime FALSE ffmpeg empty_runtime)
run_windows_package_case(empty_qt_runtime FALSE FALSE "")
prepare_windows_package(missing_platform FALSE ffmpeg missing_platform)
run_windows_package_case(missing_platform FALSE FALSE "")
prepare_windows_package(directory_platform FALSE ffmpeg directory_platform)
run_windows_package_case(directory_platform FALSE FALSE "")
prepare_windows_package(empty_platform FALSE ffmpeg empty_platform)
run_windows_package_case(empty_platform FALSE FALSE "")
prepare_windows_package(missing_multimedia FALSE missing none)
run_windows_package_case(missing_multimedia FALSE FALSE "")
prepare_windows_package(empty_multimedia FALSE empty none)
run_windows_package_case(empty_multimedia FALSE FALSE "")
prepare_windows_package(directory_multimedia FALSE directory none)
run_windows_package_case(directory_multimedia FALSE FALSE "")

message(STATUS "Verified package-layout success and rejection branches")
