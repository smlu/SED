configure_file(
  "${CMAKE_CURRENT_SOURCE_DIR}/src/app_version.hpp.in"
  "${CMAKE_CURRENT_BINARY_DIR}/app_version.hpp"
  @ONLY
)

set(SED_QT_ROOT "" CACHE PATH "Qt prefix or base directory containing a compatible Qt kit")
if(SED_QT_ROOT)
  if(EXISTS "${SED_QT_ROOT}/lib/cmake/Qt6/Qt6Config.cmake" OR EXISTS "${SED_QT_ROOT}/lib/cmake/Qt5/Qt5Config.cmake")
    list(PREPEND CMAKE_PREFIX_PATH "${SED_QT_ROOT}")
  else()
    file(GLOB _qt_configs
      "${SED_QT_ROOT}/*/msvc*/lib/cmake/Qt6/Qt6Config.cmake"
      "${SED_QT_ROOT}/*/*/msvc*/lib/cmake/Qt6/Qt6Config.cmake"
      "${SED_QT_ROOT}/*/lib/cmake/Qt6/Qt6Config.cmake"
      "${SED_QT_ROOT}/*/*/lib/cmake/Qt6/Qt6Config.cmake"
      "${SED_QT_ROOT}/*/msvc*/lib/cmake/Qt5/Qt5Config.cmake"
      "${SED_QT_ROOT}/*/*/msvc*/lib/cmake/Qt5/Qt5Config.cmake"
      "${SED_QT_ROOT}/*/lib/cmake/Qt5/Qt5Config.cmake"
      "${SED_QT_ROOT}/*/*/lib/cmake/Qt5/Qt5Config.cmake"
    )

    if(_qt_configs)
      list(SORT _qt_configs)
      list(GET _qt_configs 0 _qt_config)

      get_filename_component(_qt_prefix "${_qt_config}" DIRECTORY)  # .../lib/cmake/Qt6 or .../lib/cmake/Qt5
      get_filename_component(_qt_prefix "${_qt_prefix}" DIRECTORY)  # .../lib/cmake
      get_filename_component(_qt_prefix "${_qt_prefix}" DIRECTORY)  # .../lib
      get_filename_component(_qt_prefix "${_qt_prefix}" DIRECTORY)  # prefix
      list(PREPEND CMAKE_PREFIX_PATH "${_qt_prefix}")
    else()
      list(PREPEND CMAKE_PREFIX_PATH "${SED_QT_ROOT}")
    endif()
  endif()
elseif(DEFINED ENV{QTDIR} AND EXISTS "$ENV{QTDIR}")
  list(PREPEND CMAKE_PREFIX_PATH "$ENV{QTDIR}")
endif()

set(_qt_components Widgets OpenGL)

find_package(Qt6 COMPONENTS Widgets OpenGL OpenGLWidgets Concurrent Multimedia QUIET)
if(Qt6_FOUND)
  set(QT_VERSION_MAJOR 6)
  list(APPEND _qt_components OpenGLWidgets)
else()
  find_package(Qt5 COMPONENTS Widgets OpenGL Concurrent Multimedia QUIET)
  if(Qt5_FOUND)
    set(QT_VERSION_MAJOR 5)
  endif()
endif()

if(NOT DEFINED QT_VERSION_MAJOR)
  message(STATUS "Qt not found; skipping sed_app (set SED_BUILD_APP=OFF to silence).")
  return()
endif()

set(CMAKE_AUTOMOC OFF)
set(CMAKE_AUTOUIC OFF)
set(CMAKE_AUTORCC OFF)

add_executable(sed_app
  src/main.cpp
  src/app_icon.cpp
  src/app_icon.hpp
  src/app_logging.cpp
  src/app_logging.hpp
  src/app_version.hpp.in
  src/app_settings.cpp
  src/app_settings.hpp
  src/main_window/main_window.hpp
  src/gl_view.hpp
  src/main_window/core.cpp
  src/main_window/clipboard.cpp
  src/main_window/light_bake.cpp
  src/main_window/arch_lighting.cpp
  src/main_window/edit/state.cpp
  src/main_window/events/event_filter.cpp
  src/main_window/resources/editor.cpp
  src/main_window/resources/panel.cpp
  src/main_window/preferences/panel.cpp
  src/main_window/properties/panel.cpp
  src/main_window/properties/light_parameter_dialogs.cpp
  src/main_window/menus/menu_actions.cpp
  src/main_window/menus/edit_core_actions.cpp
  src/main_window/menus/edit_delete_action.cpp
  src/main_window/menus/edit_entity_menus.cpp
  src/main_window/menus/edit_history_actions.cpp
  src/main_window/menus/file_menu.cpp
  src/main_window/menus/map_menu.cpp
  src/main_window/menus/transform_flip_surface_action.cpp
  src/main_window/menus/transform_menu.cpp
  src/main_window/menus/transform_mirror_action.cpp
  src/main_window/menus/transform_rotate_action.cpp
  src/main_window/menus/transform_scale_action.cpp
  src/main_window/menus/transform_translate_action.cpp
  src/main_window/menus/view_menu.cpp
  src/main_window/project/startup.cpp
  src/ui/project/direct_sed_open_utils.cpp
  src/ui/project/direct_sed_open_utils.hpp
  src/ui/project/project_startup_utils.cpp
  src/ui/project/project_startup_utils.hpp
  src/ui/sector/sector_thrust_direction_pad.hpp
  src/render/gl_view_core.cpp
  src/render/gl_view_fast_update.cpp
  src/render/gl_view_picking.cpp
  src/render/gl_view_render.cpp
  src/render/gl_view_input.cpp
  src/render/gl_view_mesh.cpp
  src/render/gl_view_gizmos.cpp
  src/render/overlays/frame_render_cache.cpp
  src/render/overlays/frame_ghost_render.cpp
  src/render/overlays/frame_ghost_topology.cpp
  src/render/overlays/frame_ghost_topology.hpp
  src/render/overlays/frame_pose_cache_utils.cpp
  src/render/overlays/frame_pose_cache_utils.hpp
  src/render/detail/axis_widget.hpp
  src/render/detail/common_helpers.hpp
  src/render/detail/view_helpers.hpp
  src/render/fast_update/thing_transform_update.cpp
  src/render/fast_update/thing_transform_update.hpp
  src/render/transform/geometry_transform_preview_utils.cpp
  src/render/transform/geometry_transform_preview_utils.hpp
  src/render/grid/viewport_grid_utils.cpp
  src/render/grid/viewport_grid_utils.hpp
  src/render/cleave/cleave_measurement_utils.hpp
  src/render/cleave/cleave_wheel_session.hpp
  src/render/cleave/cleave_wheel_session.cpp
  src/render/gizmos/gizmo_icon_utils.cpp
  src/render/gizmos/gizmo_icon_utils.hpp
  src/render/gizmos/light_range_gizmo_utils.hpp
  src/render/gizmos/sector_thrust_gizmo_utils.hpp
  src/render/math/gizmo_math.cpp
  src/render/math/gizmo_math.hpp
  src/render/mesh/render_lighting_utils.cpp
  src/render/mesh/render_lighting_utils.hpp
  src/render/mesh/render_mesh_utils.cpp
  src/render/mesh/render_mesh_utils.hpp
  src/render/mesh/thing_render_geometry_utils.cpp
  src/render/mesh/thing_render_geometry_utils.hpp
  src/render/overlays/thing_shape_overlay_utils.cpp
  src/render/overlays/thing_shape_overlay_utils.hpp
  src/render/mesh/wireframe_overlay_utils.hpp
  src/render/selection/selection_geometry_utils.cpp
  src/render/selection/selection_geometry_utils.hpp
  src/render/selection/surface_selection_utils.cpp
  src/render/selection/surface_selection_utils.hpp
  src/render/snap/surface_transform_snap_utils.cpp
  src/render/snap/surface_transform_snap_utils.hpp
  src/render/texture/texture_alpha_utils.cpp
  src/render/texture/texture_alpha_utils.hpp
  src/render/translucency/translucent_order_utils.hpp
  src/render/upload/buffer_upload_utils.cpp
  src/render/upload/buffer_upload_utils.hpp
  src/render/gl_view_upload.cpp
  src/render/gl_view_resources.cpp
  src/thing/thing_resource_utils.cpp
  src/thing/thing_resource_utils.hpp
  src/thing/thing_parameter_editor_utils.hpp
  src/thing/thing_parameter_schema.cpp
  src/thing/thing_parameter_schema.hpp
  src/thing/thing_parameter_utils.hpp
  src/thing/thing_snap_utils.cpp
  src/thing/thing_snap_utils.hpp
  src/ui/components/collapsible_section.cpp
  src/ui/components/collapsible_section.hpp
  src/ui/components/about_dialog.cpp
  src/ui/components/about_dialog.hpp
  src/ui/components/flow_layout.hpp
  src/ui/components/live_color_popup.cpp
  src/ui/components/loose_document_watcher.cpp
  src/ui/components/live_color_popup.hpp
  src/ui/components/light_parameters_dialog.hpp
  src/ui/components/resource_unsaved_changes_prompt.hpp
  src/ui/components/path_display_utils.hpp
  src/ui/components/property_editor_controls.cpp
  src/ui/components/property_editor_controls.hpp
  src/ui/components/category_membership_utils.hpp
  src/ui/components/category_tree_widgets.hpp
  src/ui/components/preview_thumbnail_list.hpp
  src/ui/components/preview_worker_pool.hpp
  src/ui/components/spin_scrub_utils.cpp
  src/ui/components/spin_scrub_utils.hpp
  src/ui/components/precise_numeric_input.cpp
  src/ui/components/precise_numeric_input.hpp
  src/ui/components/ui_decimal_format.cpp
  src/ui/components/ui_decimal_format.hpp
  src/ui/components/ui_decimal_policy.hpp
  src/ui/components/scrub_float_edit.cpp
  src/ui/components/scrub_float_edit.hpp
  src/ui/components/shared_ui_styles.hpp
  src/ui/components/preview_resource_disk_watcher.hpp
  src/ui/components/preview_resource_revision_utils.hpp
  src/ui/components/template_model_preview_icon.hpp
  src/ui/audio/audio_transition_utils.cpp
  src/ui/audio/audio_transition_utils.hpp
  src/ui/audio/app_sound_player.cpp
  src/ui/audio/app_sound_player.hpp
  src/ui/audio/app_sound_player_pcm_device.hpp
  src/ui/audio/sector_sound_utils.cpp
  src/ui/audio/sector_sound_utils.hpp
  src/ui/audio/wav_preview_metadata.cpp
  src/ui/audio/wav_preview_metadata.hpp
  src/ui/episode/episode_editor.cpp
  src/ui/episode/episode_editor.hpp
  src/ui/components/viewport_camera_menu.cpp
  src/ui/components/viewport_camera_menu.hpp
  src/ui/components/viewport_icons.cpp
  src/ui/components/viewport_icons.hpp
  src/main_window/cog/scripts.cpp
  src/main_window/cog/editor.cpp
  src/ui/cog/cog_instance_picker.cpp
  src/ui/cog/cog_instance_picker.hpp
  src/ui/cog/cog_script_diagnostics_utils.cpp
  src/ui/cog/cog_script_diagnostics_utils.hpp
  src/ui/cog/cog_script_editor_widget.cpp
  src/ui/cog/cog_script_editor_widget.hpp
  src/ui/cog/cog_script_format_utils.cpp
  src/ui/cog/cog_script_format_utils.hpp
  src/ui/cog/cog_script_assignment_utils.cpp
  src/ui/cog/cog_script_assignment_utils.hpp
  src/ui/cog/cog_script_insert_utils.cpp
  src/ui/cog/cog_script_insert_utils.hpp
  src/ui/cog/cog_script_language_utils.cpp
  src/ui/cog/cog_script_language_utils.hpp
  src/ui/cog/cog_script_navigation_utils.cpp
  src/ui/cog/cog_script_navigation_utils.hpp
  src/ui/cog/cog_script_outline_utils.cpp
  src/ui/cog/cog_script_outline_utils.hpp
  src/ui/cog/cog_script_symbol_utils.cpp
  src/ui/cog/cog_script_symbol_utils.hpp
  src/ui/cog/cog_script_tab_utils.cpp
  src/ui/cog/cog_script_tab_utils.hpp
  src/ui/cog/cog_script_theme_utils.hpp
  src/ui/cog/cog_script_text_utils.cpp
  src/ui/cog/cog_script_text_utils.hpp
  src/ui/checks/level_check_utils.cpp
  src/ui/checks/level_check_utils.hpp
  src/main_window/document/state.cpp
  src/main_window/document/recovery.cpp
  src/ui/recovery/document_recovery_utils.cpp
  src/ui/recovery/document_recovery_utils.hpp
  src/ui/recovery/architectural_lighting_workspace_utils.cpp
  src/ui/recovery/architectural_lighting_workspace_utils.hpp
  src/ui/recovery/partial_level_salvage.cpp
  src/ui/recovery/partial_level_salvage.hpp
  src/ui/recovery/windows_recovery_hooks.cpp
  src/ui/recovery/windows_recovery_hooks.hpp
  src/ui/layers/layer_color_utils.cpp
  src/ui/layers/layer_color_utils.hpp
  src/ui/lighting/light_bake_utils.hpp
  src/ui/lighting/mots_arch_lighting_dialog.cpp
  src/ui/lighting/mots_arch_lighting_dialog.hpp
  src/ui/lighting/mots_arch_lighting_staging.cpp
  src/ui/navigation/outliner_types.hpp
  src/main_window/layers/layers.cpp
  src/main_window/query/level_query.cpp
  src/ui/query/level_query_utils.cpp
  src/ui/query/level_query_utils.hpp
  src/ui/resources/cog_script_picker.cpp
  src/ui/resources/cog_script_picker.hpp
  src/ui/resources/project_resource_settings.cpp
  src/ui/resources/project_resource_settings.hpp
  src/ui/resources/resource_picker_kind_utils.cpp
  src/ui/resources/resource_picker_kind_utils.hpp
  src/ui/resources/resource_picker.cpp
  src/ui/resources/resource_picker.hpp
  src/ui/resources/resource_authoring_editor.cpp
  src/ui/resources/resource_authoring_history.cpp
  src/ui/cog/cog_document_history.cpp
  src/ui/resources/resource_authoring_editor.hpp
  src/ui/resources/resource_authoring_detail.hpp
  src/ui/resources/resource_authoring_pane.hpp
  src/ui/resources/resource_authoring_sprite.cpp
  src/ui/resources/resource_authoring_tables.cpp
  src/ui/resources/template_resource_utils.cpp
  src/ui/resources/template_resource_utils.hpp
  src/main_window/sector/draw.cpp
  src/main_window/sector/join.cpp
  src/main_window/sector/shapes.cpp
  src/main_window/surface/extrude.cpp
  src/ui/sector/sector_join_utils.cpp
  src/ui/sector/sector_join_utils.hpp
  src/ui/sector/sector_import_failure_dialog.hpp
  src/ui/sector/sector_import_utils.cpp
  src/ui/sector/sector_import_utils.hpp
  src/ui/sector/sector_shape_utils.cpp
  src/ui/sector/sector_shape_utils.hpp
  src/ui/sector/sector_wire_utils.cpp
  src/ui/sector/sector_wire_utils.hpp
  src/ui/selection/selection_tool_availability_utils.cpp
  src/ui/selection/selection_tool_availability_utils.hpp
  src/ui/surface/surface_extrude_preview_utils.cpp
  src/ui/surface/surface_extrude_preview_utils.hpp
  src/ui/surface/surface_extra_light_utils.hpp
  src/ui/uv/uv_island_geometry_utils.cpp
  src/ui/uv/uv_island_geometry_utils.hpp
  src/ui/uv/smart_uv_project_dialog.cpp
  src/ui/uv/smart_uv_project_dialog.hpp
  src/ui/uv/smart_uv_project_utils.cpp
  src/ui/uv/smart_uv_project_utils.hpp
  src/ui/uv/follow_active_quads_utils.cpp
  src/ui/uv/follow_active_quads_utils.hpp
  src/ui/uv/uv_surface_geometry_utils.cpp
  src/ui/uv/uv_surface_geometry_utils.hpp
  src/ui/shortcuts/shortcut_context_utils.cpp
  src/ui/shortcuts/shortcut_context_utils.hpp
  src/ui/shortcuts/selection_modifier_utils.cpp
  src/ui/shortcuts/selection_modifier_utils.hpp
  src/ui/shortcuts/key_sequence_display.cpp
  src/ui/shortcuts/key_sequence_display.hpp
  src/main_window/shortcuts.cpp
  src/ui/transform/rotation_utils.cpp
  src/ui/transform/rotation_utils.hpp
  src/ui/templates/template_editor_utils.hpp
  src/main_window/templates/template_editor.cpp
  src/main_window/uv/editor.cpp
  src/main_window/checks.cpp
  src/main_window/common.hpp
  src/main_window/detail/helpers.hpp
  src/main_window/docks/layout.cpp
  src/main_window/history.cpp
  src/main_window/lifecycle.cpp
  src/main_window/internal.hpp
  src/main_window/messages.cpp
  src/main_window/outliner.cpp
  src/main_window/selection/context.cpp
  src/main_window/viewport/thing_placement.cpp
  src/main_window/viewport/edit_mode_overlay.cpp
  src/main_window/viewport/navigation.cpp
  src/main_window/viewport/audio.cpp
  src/main_window/viewport/preferences.cpp
  src/main_window/viewport/signals.cpp
  src/main_window/viewport/overlay.cpp
  src/ui/viewport/thing_placement_drop_utils.cpp
  src/ui/viewport/thing_placement_drop_utils.hpp
  src/ui/viewport/thing_placement_panel_resize_utils.cpp
  src/ui/viewport/thing_placement_panel_resize_utils.hpp
  src/ui/viewport/thing_placement_wheel_utils.cpp
  src/ui/viewport/thing_placement_wheel_utils.hpp
  src/ui/viewport/toast_overlay_painter.cpp
  src/ui/viewport/toast_overlay_painter.hpp
  src/render/cleave/cleave_hud_presentation.cpp
  src/render/cleave/cleave_hud_presentation.hpp
  src/ui/viewport/viewport_zoom_utils.cpp
  src/ui/viewport/viewport_zoom_utils.hpp
  src/ui/viewport/viewport_orbit_utils.cpp
  src/ui/viewport/viewport_orbit_utils.hpp
  src/main_window/viewport/tools_overlay.cpp
  src/main_window/viewport/cleave_options_overlay.cpp
  src/ui/viewport/cleave_shape_utils.cpp
  src/ui/viewport/cleave_shape_utils.hpp
  src/ui/viewport/cleave_shape_popover.cpp
  src/ui/viewport/cleave_shape_popover.hpp
  src/main_window/viewport/sector_thrust_options_overlay.cpp
  src/main_window/viewport/transform_overlay.cpp
  src/viewport_camera.cpp
  src/viewport_camera.hpp
)

# Keep the stable CMake target name used by presets and tests while shipping
# the concise user-facing executable name.
set_target_properties(sed_app PROPERTIES OUTPUT_NAME "sed")

if(QT_VERSION_MAJOR EQUAL 6)
  qt_add_resources(sed_app_icon_resource_sources resources/app_icons.qrc)
else()
  qt5_add_resources(sed_app_icon_resource_sources resources/app_icons.qrc)
endif()
target_sources(sed_app PRIVATE ${sed_app_icon_resource_sources})

if(WIN32)
  # The shell/taskbar icon keeps a black outer field for contrast, while Qt's
  # separately embedded app_window_* frames remain borderless in title bars.
  set(SED_APP_ICON_PATH "${CMAKE_CURRENT_SOURCE_DIR}/resources/app_flat_red_circle.ico")
  if(EXISTS "${SED_APP_ICON_PATH}")
    file(TO_CMAKE_PATH "${SED_APP_ICON_PATH}" SED_APP_ICON_PATH_RC)
    configure_file(
      "${CMAKE_CURRENT_SOURCE_DIR}/resources/sed_app.rc.in"
      "${CMAKE_CURRENT_BINARY_DIR}/sed_app.rc"
      @ONLY
    )
    set_source_files_properties("${CMAKE_CURRENT_BINARY_DIR}/sed_app.rc"
      PROPERTIES OBJECT_DEPENDS "${SED_APP_ICON_PATH}"
    )
    target_sources(sed_app PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/sed_app.rc")
  endif()
endif()

target_include_directories(sed_app PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_include_directories(sed_app PRIVATE ${CMAKE_CURRENT_BINARY_DIR})

if(QT_VERSION_MAJOR EQUAL 6)
  qt_wrap_cpp(sed_app_moc_srcs
    TARGET sed_app
    src/gl_view.hpp
    src/main_window/main_window.hpp
    src/ui/components/scrub_float_edit.hpp
    src/ui/components/ui_decimal_policy.hpp
  )
else()
  qt5_wrap_cpp(sed_app_moc_srcs
    src/gl_view.hpp
    src/main_window/main_window.hpp
    src/ui/components/scrub_float_edit.hpp
    src/ui/components/ui_decimal_policy.hpp
  )
endif()

target_sources(sed_app PRIVATE ${sed_app_moc_srcs})

target_link_libraries(sed_app
  PRIVATE
    sed_core
    Qt${QT_VERSION_MAJOR}::Widgets
    Qt${QT_VERSION_MAJOR}::OpenGL
    Qt${QT_VERSION_MAJOR}::Concurrent
    Qt${QT_VERSION_MAJOR}::Multimedia
)

if(WIN32)
  target_link_libraries(sed_app PRIVATE Dbghelp Ole32 Shell32)
endif()

if (MSVC)
  target_compile_options(sed_app PRIVATE /bigobj)
endif()

option(SED_APP_CONSOLE "Build sed_app with a Windows console subsystem" OFF)
if(MSVC AND NOT SED_APP_CONSOLE)
  target_link_options(sed_app PRIVATE /SUBSYSTEM:WINDOWS /ENTRY:mainCRTStartup)
endif()

# Resolve the release CRT from the compiler-selected VS installation instead
# of searching PATH, System32, or a different VS install. This is the
# app-local equivalent of CMake's InstallRequiredSystemLibraries deployment,
# while also covering current toolsets that older CMake modules do not name.
set(_sed_msvc_runtime_copy_script "")
if(WIN32 AND MSVC)
  set(_sed_msvc_architecture "${MSVC_CXX_ARCHITECTURE_ID}")
  if(_sed_msvc_architecture STREQUAL "")
    set(_sed_msvc_architecture "${MSVC_C_ARCHITECTURE_ID}")
  endif()
  if(_sed_msvc_architecture STREQUAL "")
    set(_sed_msvc_architecture "${CMAKE_GENERATOR_PLATFORM}")
  endif()
  string(TOLOWER "${_sed_msvc_architecture}" _sed_msvc_architecture)
  if(_sed_msvc_architecture STREQUAL "amd64")
    set(_sed_msvc_architecture x64)
  elseif(_sed_msvc_architecture STREQUAL "win32")
    set(_sed_msvc_architecture x86)
  elseif(NOT _sed_msvc_architecture MATCHES "^(x64|x86|arm64|arm64ec)$")
    message(FATAL_ERROR
      "Unsupported MSVC target architecture for app-local runtime deployment: "
      "${_sed_msvc_architecture}")
  endif()

  get_filename_component(_sed_msvc_compiler_dir
    "${CMAKE_CXX_COMPILER}" DIRECTORY)
  get_filename_component(_sed_msvc_host_dir
    "${_sed_msvc_compiler_dir}" DIRECTORY)
  get_filename_component(_sed_msvc_bin_dir
    "${_sed_msvc_host_dir}" DIRECTORY)
  get_filename_component(_sed_msvc_toolset_dir
    "${_sed_msvc_bin_dir}" DIRECTORY)
  get_filename_component(_sed_msvc_toolset_parent
    "${_sed_msvc_toolset_dir}" DIRECTORY)
  get_filename_component(_sed_msvc_tools_parent
    "${_sed_msvc_toolset_parent}" DIRECTORY)
  get_filename_component(_sed_msvc_vc_dir
    "${_sed_msvc_tools_parent}" DIRECTORY)

  # Redist patch directories can use the expanded toolset version while
  # CMake's compiler path is an 8.3 alias (and an installed kit can retain a
  # newer redist patch). Search only this compiler's VS installation, then
  # take the newest official release CRT for the target architecture.
  file(GLOB _sed_msvc_crt_candidates LIST_DIRECTORIES true
    "${_sed_msvc_vc_dir}/Redist/MSVC/*/${_sed_msvc_architecture}/Microsoft.*.CRT")
  set(_sed_msvc_crt_directories)
  foreach(_sed_msvc_crt_candidate IN LISTS _sed_msvc_crt_candidates)
    if(IS_DIRECTORY "${_sed_msvc_crt_candidate}")
      list(APPEND _sed_msvc_crt_directories "${_sed_msvc_crt_candidate}")
    endif()
  endforeach()
  list(SORT _sed_msvc_crt_directories COMPARE NATURAL ORDER DESCENDING)
  list(LENGTH _sed_msvc_crt_directories _sed_msvc_crt_directory_count)
  if(_sed_msvc_crt_directory_count EQUAL 0)
    message(FATAL_ERROR
      "The selected MSVC toolchain has no release CRT redist directory: "
      "${_sed_msvc_vc_dir}/Redist/MSVC/*/${_sed_msvc_architecture}")
  endif()
  list(GET _sed_msvc_crt_directories 0 _sed_msvc_crt_dir)

  file(GLOB _sed_msvc_runtime_files LIST_DIRECTORIES false
    "${_sed_msvc_crt_dir}/*.dll")
  list(SORT _sed_msvc_runtime_files)
  if(NOT _sed_msvc_runtime_files)
    message(FATAL_ERROR
      "The selected MSVC release CRT redist directory contains no DLLs: "
      "${_sed_msvc_crt_dir}")
  endif()

  foreach(_sed_required_msvc_runtime IN ITEMS
      MSVCP140.dll
      MSVCP140_1.dll
      MSVCP140_2.dll
      VCRUNTIME140.dll
      VCRUNTIME140_1.dll)
    if(NOT EXISTS "${_sed_msvc_crt_dir}/${_sed_required_msvc_runtime}")
      message(FATAL_ERROR
        "The selected MSVC release CRT redist is missing "
        "${_sed_required_msvc_runtime}: ${_sed_msvc_crt_dir}")
    endif()
  endforeach()

  string(REPLACE ";" "|" SED_MSVC_RUNTIME_FILES_SERIALIZED
    "${_sed_msvc_runtime_files}")
  set(_sed_msvc_runtime_copy_script
    "${CMAKE_CURRENT_BINARY_DIR}/copy_msvc_runtime.cmake")
  configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/copy_msvc_runtime.cmake.in"
    "${_sed_msvc_runtime_copy_script}"
    @ONLY)
endif()

if(QT_VERSION_MAJOR EQUAL 6)
  target_link_libraries(sed_app PRIVATE Qt6::OpenGLWidgets)
else()
  # Qt5 doesn't have OpenGLWidgets; QOpenGLWidget lives in QtWidgets.
endif()

# Keep the executable directory runnable outside a Qt developer shell. In
# particular, Qt Multimedia requires a backend plugin in addition to its DLL.
if(WIN32 AND QT_VERSION_MAJOR EQUAL 6 AND TARGET Qt6::windeployqt)
  add_custom_command(TARGET sed_app POST_BUILD
    COMMAND "$<TARGET_FILE:Qt6::windeployqt>"
      --no-translations
      --no-system-d3d-compiler
      --no-system-dxc-compiler
      --no-compiler-runtime
      --no-opengl-sw
      "$<TARGET_FILE:sed_app>"
    COMMENT "Deploying the Qt runtime and multimedia backends"
    VERBATIM
  )
endif()

if(WIN32 AND MSVC)
  add_custom_command(TARGET sed_app POST_BUILD
    COMMAND "${CMAKE_COMMAND}"
      "-DSED_MSVC_RUNTIME_CONFIG=$<CONFIG>"
      "-DSED_MSVC_RUNTIME_OUTPUT_DIR=$<TARGET_FILE_DIR:sed_app>"
      -P "${_sed_msvc_runtime_copy_script}"
    COMMENT "Deploying the selected MSVC Release runtime"
    VERBATIM
  )
endif()

target_compile_definitions(sed_app PRIVATE QT_NO_KEYWORDS)

add_executable(sed_app_tests
  tests/test_main.cpp
  tests/test_common.hpp
  tests/app_sound_player_pcm_device_tests.cpp
  tests/render_preview_utility_tests.cpp
  tests/ambient_audio_identity_tests.cpp
  tests/audio_resource_identity_tests.cpp
  tests/preview_resource_failure_tests.cpp
  tests/thing_light_precision_tests.cpp
  tests/template_precision_equality_tests.cpp
  tests/thing_resource_cache_tests.cpp
  tests/scrub_float_precision_tests.cpp
  tests/editing_precision_tests.cpp
  tests/consistency_geometry_contract_tests.cpp
  tests/editing_helper_contract_tests.cpp
  tests/document_recovery_tests.cpp
  tests/partial_level_salvage_tests.cpp
  tests/episode_utility_tests.cpp
  tests/layer_utility_tests.cpp
  tests/thing_parameter_schema_tests.cpp
  tests/thing_shape_overlay_tests.cpp
  tests/template_collision_bounds_guide_tests.cpp
  tests/thing_utility_tests.cpp
  tests/theme_storage_tests.cpp
  tests/viewport_aa_preference_tests.cpp
  tests/viewport_banner_settings_tests.cpp
  tests/toast_overlay_painter_tests.cpp
  tests/cleave_hud_presentation_tests.cpp
  tests/geometry_transform_preview_utility_tests.cpp
  tests/cleave_shape_utils_tests.cpp
  tests/cleave_measurement_utils_tests.cpp
  tests/undo_label_tests.cpp
  src/app_settings.cpp
  src/ui/viewport/toast_overlay_painter.cpp
  src/render/cleave/cleave_hud_presentation.cpp
  src/render/cleave/cleave_hud_presentation.hpp
  src/viewport_camera.cpp
  src/viewport_camera.hpp
  src/ui/components/viewport_icons.cpp
  src/ui/components/viewport_icons.hpp
  src/render/fast_update/thing_transform_update.cpp
  src/render/fast_update/thing_transform_update.hpp
  src/render/transform/geometry_transform_preview_utils.cpp
  src/render/transform/geometry_transform_preview_utils.hpp
  src/ui/viewport/cleave_shape_utils.cpp
  src/ui/viewport/cleave_shape_utils.hpp
  src/ui/viewport/cleave_shape_popover.cpp
  src/ui/viewport/cleave_shape_popover.hpp
  src/render/grid/viewport_grid_utils.cpp
  src/render/grid/viewport_grid_utils.hpp
  src/render/gizmos/gizmo_icon_utils.cpp
  src/render/gizmos/gizmo_icon_utils.hpp
  src/render/gizmos/light_range_gizmo_utils.hpp
  src/render/gizmos/sector_thrust_gizmo_utils.hpp
  src/render/mesh/render_lighting_utils.cpp
  src/render/mesh/render_lighting_utils.hpp
  src/render/mesh/render_mesh_utils.cpp
  src/render/mesh/render_mesh_utils.hpp
  src/render/mesh/thing_render_geometry_utils.cpp
  src/render/mesh/thing_render_geometry_utils.hpp
  src/render/overlays/thing_shape_overlay_utils.cpp
  src/render/mesh/wireframe_overlay_utils.hpp
  src/render/selection/selection_geometry_utils.cpp
  src/render/selection/selection_geometry_utils.hpp
  src/render/selection/surface_selection_utils.cpp
  src/render/selection/surface_selection_utils.hpp
  src/render/snap/surface_transform_snap_utils.cpp
  src/render/snap/surface_transform_snap_utils.hpp
  src/render/texture/texture_alpha_utils.cpp
  src/render/texture/texture_alpha_utils.hpp
  src/render/translucency/translucent_order_utils.hpp
  src/render/upload/buffer_upload_utils.cpp
  src/render/upload/buffer_upload_utils.hpp
  src/thing/thing_resource_utils.cpp
  src/thing/thing_resource_utils.hpp
  src/thing/thing_parameter_editor_utils.hpp
  src/thing/thing_parameter_schema.cpp
  src/thing/thing_parameter_schema.hpp
  src/thing/thing_parameter_utils.hpp
  src/thing/thing_snap_utils.cpp
  src/thing/thing_snap_utils.hpp
  src/ui/cog/cog_script_diagnostics_utils.cpp
  src/ui/cog/cog_script_diagnostics_utils.hpp
  src/ui/cog/cog_script_editor_widget.cpp
  src/ui/cog/cog_script_editor_widget.hpp
  src/ui/cog/cog_script_format_utils.cpp
  src/ui/cog/cog_script_format_utils.hpp
  src/ui/cog/cog_script_assignment_utils.cpp
  src/ui/cog/cog_script_assignment_utils.hpp
  src/ui/cog/cog_script_insert_utils.cpp
  src/ui/cog/cog_script_insert_utils.hpp
  src/ui/cog/cog_script_language_utils.cpp
  src/ui/cog/cog_script_language_utils.hpp
  src/ui/cog/cog_script_navigation_utils.cpp
  src/ui/cog/cog_script_navigation_utils.hpp
  src/ui/cog/cog_script_outline_utils.cpp
  src/ui/cog/cog_script_outline_utils.hpp
  src/ui/cog/cog_script_symbol_utils.cpp
  src/ui/cog/cog_script_symbol_utils.hpp
  src/ui/cog/cog_script_tab_utils.cpp
  src/ui/cog/cog_script_tab_utils.hpp
  src/ui/cog/cog_script_theme_utils.hpp
  src/ui/cog/cog_script_text_utils.cpp
  src/ui/cog/cog_script_text_utils.hpp
  src/ui/checks/level_check_utils.cpp
  src/ui/checks/level_check_utils.hpp
  src/ui/audio/audio_transition_utils.cpp
  src/ui/audio/audio_transition_utils.hpp
  src/ui/audio/sector_sound_utils.cpp
  src/ui/audio/sector_sound_utils.hpp
  src/ui/audio/wav_preview_metadata.cpp
  src/ui/audio/wav_preview_metadata.hpp
  src/ui/episode/episode_editor.cpp
  src/ui/episode/episode_editor.hpp
  src/ui/components/live_color_popup.cpp
  src/ui/components/loose_document_watcher.cpp
  src/ui/components/live_color_popup.hpp
  src/ui/components/precise_numeric_input.cpp
  src/ui/components/precise_numeric_input.hpp
  src/ui/components/ui_decimal_format.cpp
  src/ui/components/ui_decimal_format.hpp
  src/ui/components/ui_decimal_policy.hpp
  src/ui/components/scrub_float_edit.cpp
  src/ui/components/scrub_float_edit.hpp
  src/ui/components/property_editor_controls.cpp
  src/ui/components/property_editor_controls.hpp
  src/ui/components/spin_scrub_utils.cpp
  src/ui/components/spin_scrub_utils.hpp
  src/ui/components/preview_resource_disk_watcher.hpp
  src/ui/components/preview_resource_revision_utils.hpp
  src/ui/layers/layer_color_utils.cpp
  src/ui/layers/layer_color_utils.hpp
  src/ui/lighting/light_bake_utils.hpp
  src/ui/navigation/outliner_types.hpp
  src/ui/project/project_startup_utils.cpp
  src/ui/project/project_startup_utils.hpp
  src/ui/query/level_query_utils.cpp
  src/ui/query/level_query_utils.hpp
  src/ui/recovery/document_recovery_utils.cpp
  src/ui/recovery/document_recovery_utils.hpp
  src/ui/recovery/partial_level_salvage.cpp
  src/ui/recovery/partial_level_salvage.hpp
  src/ui/resources/project_resource_settings.cpp
  src/ui/resources/project_resource_settings.hpp
  src/ui/resources/resource_picker_kind_utils.cpp
  src/ui/resources/resource_picker_kind_utils.hpp
  src/ui/resources/template_resource_utils.cpp
  src/ui/resources/template_resource_utils.hpp
  src/ui/templates/template_editor_utils.hpp
  src/ui/sector/sector_join_utils.cpp
  src/ui/sector/sector_join_utils.hpp
  src/ui/sector/sector_import_failure_dialog.hpp
  src/ui/sector/sector_import_utils.cpp
  src/ui/sector/sector_import_utils.hpp
  src/ui/sector/sector_shape_utils.cpp
  src/ui/sector/sector_shape_utils.hpp
  src/ui/sector/sector_wire_utils.cpp
  src/ui/sector/sector_wire_utils.hpp
  src/ui/selection/selection_tool_availability_utils.cpp
  src/ui/selection/selection_tool_availability_utils.hpp
  src/ui/surface/surface_extrude_preview_utils.cpp
  src/ui/surface/surface_extrude_preview_utils.hpp
  src/ui/surface/surface_extra_light_utils.hpp
  src/ui/uv/uv_island_geometry_utils.cpp
  src/ui/uv/uv_island_geometry_utils.hpp
  src/ui/uv/smart_uv_project_dialog.cpp
  src/ui/uv/smart_uv_project_dialog.hpp
  src/ui/uv/smart_uv_project_utils.cpp
  src/ui/uv/smart_uv_project_utils.hpp
  src/ui/uv/follow_active_quads_utils.cpp
  src/ui/uv/follow_active_quads_utils.hpp
  src/ui/uv/uv_surface_geometry_utils.cpp
  src/ui/uv/uv_surface_geometry_utils.hpp
  src/ui/shortcuts/shortcut_context_utils.cpp
  src/ui/shortcuts/shortcut_context_utils.hpp
  src/ui/shortcuts/selection_modifier_utils.cpp
  src/ui/shortcuts/selection_modifier_utils.hpp
  src/ui/shortcuts/key_sequence_display.cpp
  src/ui/shortcuts/key_sequence_display.hpp
  src/ui/transform/rotation_utils.cpp
  src/ui/transform/rotation_utils.hpp
  src/ui/viewport/thing_placement_drop_utils.cpp
  src/ui/viewport/thing_placement_drop_utils.hpp
  src/ui/viewport/thing_placement_panel_resize_utils.cpp
  src/ui/viewport/thing_placement_panel_resize_utils.hpp
  src/ui/viewport/thing_placement_wheel_utils.cpp
  src/ui/viewport/thing_placement_wheel_utils.hpp
  src/ui/viewport/viewport_zoom_utils.cpp
  src/ui/viewport/viewport_zoom_utils.hpp
  src/ui/viewport/viewport_orbit_utils.cpp
  src/ui/viewport/viewport_orbit_utils.hpp
  src/render/math/gizmo_math.cpp
  src/render/math/gizmo_math.hpp
)

set(sed_app_tests_moc_srcs ${sed_app_moc_srcs})
list(FILTER sed_app_tests_moc_srcs INCLUDE REGEX "moc_(scrub_float_edit|ui_decimal_policy)\\.cpp$")
target_sources(sed_app_tests PRIVATE ${sed_app_tests_moc_srcs})

target_include_directories(sed_app_tests
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/tests
)

target_link_libraries(sed_app_tests
  PRIVATE
    sed_core
    Qt${QT_VERSION_MAJOR}::Widgets
    Qt${QT_VERSION_MAJOR}::OpenGL
    Qt${QT_VERSION_MAJOR}::Concurrent
)

if(QT_VERSION_MAJOR EQUAL 6)
  target_link_libraries(sed_app_tests PRIVATE Qt6::OpenGLWidgets)
endif()

target_compile_definitions(sed_app_tests PRIVATE
  QT_NO_KEYWORDS
  SED_APP_RECOVERY_TEST_ACCESS
  SED_APP_THEME_STORAGE_TEST_ACCESS
)

if (MSVC)
  target_compile_options(sed_app_tests PRIVATE /FS)
endif()

add_test(NAME sed_app_tests COMMAND sed_app_tests)
add_test(NAME sed_app_cleave_measurements
  COMMAND sed_app_tests --group cleave_measurements)
add_test(NAME sed_app_viewport_banner_settings
  COMMAND sed_app_tests --group viewport_banner_settings)
add_test(NAME sed_app_toast_overlay_painter
  COMMAND sed_app_tests --group toast_overlay_painter)
add_test(NAME sed_app_cleave_hud_presentation
  COMMAND sed_app_tests --group cleave_hud_presentation)

# Allocation injection is isolated from the QApplication-based suites.
add_executable(sed_app_orbit_allocation_tests
  tests/orbit_allocation_tests.cpp
  src/ui/viewport/viewport_orbit_utils.cpp
  src/viewport_camera.cpp
)
target_include_directories(sed_app_orbit_allocation_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(sed_app_orbit_allocation_tests PRIVATE sed_core Qt${QT_VERSION_MAJOR}::Gui)
target_compile_definitions(sed_app_orbit_allocation_tests PRIVATE QT_NO_KEYWORDS)
if (MSVC)
  target_compile_options(sed_app_orbit_allocation_tests PRIVATE /FS)
endif()
add_test(NAME sed_app_orbit_selected_allocation COMMAND sed_app_orbit_allocation_tests)
add_test(NAME sed_app_orbit_fallback_allocation COMMAND sed_app_orbit_allocation_tests --fallback)
add_test(NAME sed_app_orbit_ui_selected_allocation COMMAND sed_app_orbit_allocation_tests --ui-selected)
add_test(NAME sed_app_orbit_ui_fallback_allocation COMMAND sed_app_orbit_allocation_tests --ui-fallback)
set_tests_properties(
  sed_app_orbit_selected_allocation sed_app_orbit_fallback_allocation
  sed_app_orbit_ui_selected_allocation sed_app_orbit_ui_fallback_allocation
  PROPERTIES TIMEOUT 30 LABELS "allocation;contracts"
)


get_target_property(SED_APP_MAIN_WINDOW_TEST_SOURCES sed_app SOURCES)
list(FILTER SED_APP_MAIN_WINDOW_TEST_SOURCES EXCLUDE REGEX "src/main\\.cpp$")

add_executable(sed_app_main_window_tests
  tests/main_window_event_filter_test_main.cpp
  tests/native_caption_visibility_tests.cpp
  tests/frame_add_move_behavior_tests.cpp
  tests/frame_history_selection_tests.cpp
  tests/frame_render_performance_tests.cpp
  tests/uv_command_precision_tests.cpp
  tests/thing_transform_command_tests.cpp
  tests/thing_light_transform_history_tests.cpp
  tests/light_gizmo_selection_tests.cpp
  tests/mots_arch_lighting_dialog_tests.cpp
  tests/mots_arch_lighting_staging_tests.cpp
  tests/mots_arch_lighting_render_tests.cpp
  tests/mots_arch_lighting_picking_tests.cpp
  tests/mots_arch_lighting_invalidation_tests.cpp
  tests/mots_arch_lighting_workspace_recovery_tests.cpp
  tests/translate_menu_contract_tests.cpp
  tests/thing_find_lighting_menu_tests.cpp
  tests/viewport_shortcut_tests.cpp
  tests/navigation_preferences_tests.cpp
  tests/viewport_thing_overlay_state_tests.cpp
  tests/consistency_precision_dialog_tests.cpp
  tests/editing_rejection_ui_tests.cpp
  tests/transform_numeric_precision_tests.cpp
  tests/designer_ui_precision_tests.cpp
  tests/uv_numeric_input_precision_tests.cpp
  tests/uv_zoom_preference_tests.cpp
  tests/ui_decimal_policy_tests.cpp
  tests/ui_decimal_consumer_tests.cpp
  tests/resource_ui_decimal_tests.cpp
  tests/project_resource_precision_tests.cpp
  tests/resource_picker_release_tests.cpp
  tests/resource_authoring_editor_tests.cpp
  tests/resource_authoring_presentation_tests.cpp
  tests/resource_authoring_lifecycle_tests.cpp
  tests/resource_authoring_sprite_geometry_tests.cpp
  tests/resource_authoring_history_tests.cpp
  tests/resource_io_boundary_tests.cpp
  tests/runtime_lighting_switch_tests.cpp
  tests/cog_interaction_regression_tests.cpp
  tests/cog_shortcut_context_tests.cpp
  tests/cog_document_docking_tests.cpp
  tests/loose_document_watcher_tests.cpp
  tests/native_save_edit_validation_tests.cpp
  tests/template_archive_refresh_tests.cpp
  tests/template_collision_bounds_preview_tests.cpp
  tests/template_duplicate_type_tests.cpp
  tests/parameter_helper_dialog_tests.cpp
  tests/thing_collision_overlay_cache_tests.cpp
  tests/toast_expiry_timer_tests.cpp
  tests/hold_modifier_binding_tests.cpp
  tests/priority_mouse_release_tests.cpp
  tests/light_mode_shortcut_tests.cpp
  tests/cleave_precision_ui_tests.cpp
  tests/surface_multicut_regression_tests.cpp
  tests/thing_placement_behavior_tests.cpp
  tests/thing_ortho_thin_placement_tests.cpp
  tests/thing_move_drag_hotpath_tests.cpp
  tests/cog_script_lifecycle_tests.cpp
  tests/cog_inline_assignment_tests.cpp
  tests/cleave_chain_body_recovery_tests.cpp
  tests/cleave_bounded_recovery_tests.cpp
  tests/cleave_connection_history_tests.cpp
  tests/cleave_shape_overlay_tests.cpp
  tests/cleave_workspace_persistence_tests.cpp
  tests/cleave_apply_workspace_history_tests.cpp
  tests/cleave_advanced_interaction_tests.cpp
  tests/cleave_input_routing_tests.cpp
  tests/cleave_wheel_session_tests.cpp
  tests/cleave_shape_apply_tests.cpp
  tests/main_window_event_filter_tests.cpp
  tests/sector_ambient_property_tests.cpp
  tests/mots_arch_bake_flag_tests.cpp
  tests/mots_arch_bake_pipeline_tests.cpp
  tests/surface_active_property_tests.cpp
  tests/surface_selection_history_tests.cpp
  tests/thing_sector_property_navigation_tests.cpp
  tests/test_common.hpp
  ${SED_APP_MAIN_WINDOW_TEST_SOURCES}
)

target_include_directories(sed_app_main_window_tests
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/tests
    ${CMAKE_CURRENT_BINARY_DIR}
)

target_link_libraries(sed_app_main_window_tests
  PRIVATE
    sed_core
    Qt${QT_VERSION_MAJOR}::Widgets
    Qt${QT_VERSION_MAJOR}::OpenGL
    Qt${QT_VERSION_MAJOR}::Concurrent
    Qt${QT_VERSION_MAJOR}::Multimedia
)
if(WIN32)
  target_link_libraries(sed_app_main_window_tests PRIVATE Dbghelp Dwmapi Ole32 Shell32)
endif()

if(QT_VERSION_MAJOR EQUAL 6)
  target_link_libraries(sed_app_main_window_tests PRIVATE Qt6::OpenGLWidgets)
endif()

target_compile_definitions(sed_app_main_window_tests PRIVATE
  QT_NO_KEYWORDS
  SED_APP_MAIN_WINDOW_TEST_ACCESS
  SED_APP_RECOVERY_TEST_ACCESS
  SED_APP_THEME_STORAGE_TEST_ACCESS
)

if (MSVC)
  target_compile_options(sed_app_main_window_tests PRIVATE /bigobj /FS)
  # This target intentionally keeps the complete Qt MainWindow interaction
  # matrix in one monolithic integration routine. Its heavyweight modal and
  # rendering fixtures need extra call-frame headroom under MSVC. Keep this
  # reserve test-only; product executables retain the default stack.
  target_link_options(sed_app_main_window_tests PRIVATE /STACK:16777216)
endif()

add_test(NAME sed_app_main_window_tests COMMAND sed_app_main_window_tests)
if(WIN32)
  add_test(NAME sed_app_native_caption_visibility
    COMMAND sed_app_main_window_tests --native-caption-visibility)
  set_tests_properties(sed_app_native_caption_visibility PROPERTIES
    TIMEOUT 30 LABELS "module;ui;windows;native")
endif()
add_test(NAME sed_app_cleave_chain_origin
  COMMAND sed_app_main_window_tests --cleave-chain-origin)
add_test(NAME sed_app_surface_multicut
  COMMAND sed_app_main_window_tests --surface-multicut-regression)
add_test(NAME sed_app_cleave_bounded_recovery
  COMMAND sed_app_main_window_tests --cleave-bounded-recovery)
add_test(NAME sed_app_cleave_connection_history
  COMMAND sed_app_main_window_tests --cleave-connection-history)
add_test(NAME sed_app_cleave_shape_overlay
  COMMAND sed_app_main_window_tests --cleave-shape-overlay)
add_test(NAME sed_app_cleave_family_controls
  COMMAND sed_app_main_window_tests --cleave-family-controls)
add_test(NAME sed_app_cleave_workspace_persistence
  COMMAND sed_app_main_window_tests --cleave-workspace-persistence)
add_test(NAME sed_app_cleave_document_reset
  COMMAND sed_app_main_window_tests --cleave-document-reset)
add_test(NAME sed_app_cleave_apply_workspace_history
  COMMAND sed_app_main_window_tests --cleave-apply-workspace-history)
add_test(NAME sed_app_cleave_advanced_interaction
  COMMAND sed_app_main_window_tests --cleave-advanced-interaction)
add_test(NAME sed_app_cleave_input_routing
  COMMAND sed_app_main_window_tests --cleave-input-routing)
add_test(NAME sed_app_cleave_fly_custom_shortcuts
  COMMAND sed_app_main_window_tests --cleave-fly-custom-shortcuts)
add_test(NAME sed_app_cleave_uv_custom_owner
  COMMAND sed_app_main_window_tests --cleave-uv-custom-owner)
add_test(NAME sed_app_cleave_shape_apply
  COMMAND sed_app_main_window_tests --cleave-shape-apply)
add_test(NAME sed_app_mots_arch_workspace_recovery
  COMMAND sed_app_main_window_tests --mots-arch-workspace-recovery)
add_test(NAME sed_app_light_gizmo_selection
  COMMAND sed_app_main_window_tests --light-gizmo-selection)
add_test(NAME sed_app_light_gizmo_selection_hidpi
  COMMAND sed_app_main_window_tests --light-gizmo-selection)
add_test(NAME sed_app_thing_light_history
  COMMAND sed_app_main_window_tests --thing-light-history)
add_test(NAME sed_app_thing_ortho_thin_placement
  COMMAND sed_app_main_window_tests --thing-ortho-thin-placement)
add_test(NAME sed_app_thing_placement_id_exhaustion
  COMMAND sed_app_main_window_tests --thing-placement-id-exhaustion)
add_test(NAME sed_app_mots_arch_lighting
  COMMAND sed_app_main_window_tests --mots-arch-lighting)
add_test(NAME sed_app_mots_arch_bake_flags
  COMMAND sed_app_main_window_tests --mots-arch-bake-flags)
add_test(NAME sed_app_mots_arch_bake
  COMMAND sed_app_main_window_tests --mots-arch-bake)
add_test(NAME sed_app_thing_find_lighting_menu
  COMMAND sed_app_main_window_tests --thing-find-lighting-menu)
add_test(NAME sed_app_template_collision_preview
  COMMAND sed_app_main_window_tests --template-collision-preview)
add_test(NAME sed_app_template_duplicate_type
  COMMAND sed_app_main_window_tests --template-duplicate-type)
add_test(NAME sed_app_parameter_helper_dialogs
  COMMAND sed_app_main_window_tests --parameter-helper-dialogs)
add_test(NAME sed_app_cog_menu_instance
  COMMAND sed_app_main_window_tests --cog-menu-instance)
add_test(NAME sed_app_cog_instance_picker
  COMMAND sed_app_main_window_tests --cog-instance-picker)
add_test(NAME sed_app_cog_overwrite_lifetime
  COMMAND sed_app_main_window_tests --cog-overwrite-lifetime)
add_test(NAME sed_app_cog_interaction_regressions
  COMMAND sed_app_main_window_tests --cog-interaction-regressions)
add_test(NAME sed_app_cog_shortcut_context
  COMMAND sed_app_main_window_tests --cog-shortcut-context)
add_test(NAME sed_app_cog_document_docking
  COMMAND sed_app_main_window_tests --cog-document-docking)
add_test(NAME sed_app_resource_picker_release
  COMMAND sed_app_main_window_tests --resource-picker-release)
add_test(NAME sed_app_resource_authoring
  COMMAND sed_app_main_window_tests --resource-authoring)
add_test(NAME sed_app_resource_authoring_presentation
  COMMAND sed_app_main_window_tests --resource-authoring-presentation)
add_test(NAME sed_app_resource_authoring_history_caret
  COMMAND sed_app_main_window_tests --resource-authoring-history-caret)
add_test(NAME sed_app_resource_workspace_regression
  COMMAND sed_app_main_window_tests --resource-workspace-regression)
add_test(NAME sed_app_resource_authoring_lifecycle
  COMMAND sed_app_main_window_tests --resource-authoring-lifecycle)
add_test(NAME sed_app_resource_authoring_sprite_geometry
  COMMAND sed_app_main_window_tests --resource-authoring-sprite-geometry)
add_test(NAME sed_app_resource_document_history
  COMMAND sed_app_main_window_tests --resource-document-history)
add_test(NAME sed_app_runtime_lighting_switches
  COMMAND sed_app_main_window_tests --runtime-lighting-switches)
add_test(NAME sed_app_cog_inline_assignment
  COMMAND sed_app_main_window_tests --cog-inline-assignment)
add_test(NAME sed_app_native_save_edit_validation
  COMMAND sed_app_main_window_tests --native-save-edit-validation)
add_test(NAME sed_app_strict_surface_selection_recovery
  COMMAND sed_app_main_window_tests --strict-surface-selection-recovery)
add_test(NAME sed_app_strict_surface_context_topology
  COMMAND sed_app_main_window_tests --strict-surface-context-topology)
add_test(NAME sed_app_minor_grid_appearance
  COMMAND sed_app_main_window_tests --minor-grid-appearance)
add_test(NAME sed_app_uv_grid_points_appearance
  COMMAND sed_app_main_window_tests --uv-grid-points-appearance)
add_test(NAME sed_app_viewport_grid_combo_popup_lifetime
  COMMAND sed_app_main_window_tests --viewport-grid-combo-popup-lifetime)
add_test(NAME sed_app_frame_history_selection
  COMMAND sed_app_main_window_tests --frame-history-selection)
add_test(NAME sed_app_frame_add_move_behavior
  COMMAND sed_app_main_window_tests --frame-add-move-behavior)
add_test(NAME sed_app_frame_render_performance
  COMMAND sed_app_main_window_tests --frame-render-performance)
add_test(NAME sed_app_thing_collision_cache
  COMMAND sed_app_main_window_tests --thing-collision-cache)
add_test(NAME sed_app_toast_expiry_timer
  COMMAND sed_app_main_window_tests --toast-expiry-timer)
add_test(NAME sed_app_hold_modifier_bindings
  COMMAND sed_app_main_window_tests --hold-modifier-bindings)
set_tests_properties(sed_app_hold_modifier_bindings PROPERTIES
  ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
  TIMEOUT 60
  LABELS "module;integration;cross_layer;offscreen")
add_test(NAME sed_app_viewport_window_mouse_release
  COMMAND sed_app_main_window_tests --viewport-window-mouse-release)
set_tests_properties(sed_app_viewport_window_mouse_release PROPERTIES
  ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
  TIMEOUT 30
  LABELS "module;integration;ui;offscreen")
add_test(NAME sed_app_viewport_shortcuts
  COMMAND sed_app_main_window_tests --viewport-shortcuts)
add_test(NAME sed_app_light_mode_shortcuts
  COMMAND sed_app_main_window_tests --light-mode-shortcuts)
add_test(NAME sed_app_navigation_preferences
  COMMAND sed_app_main_window_tests --navigation-preferences)
add_test(NAME sed_app_navigation_shortcut_context
  COMMAND sed_app_main_window_tests --navigation-shortcut-context)
add_test(NAME sed_app_surface_roundoff_multicut
  COMMAND sed_app_main_window_tests --surface-roundoff-multicut)
add_test(NAME sed_app_cleave_wheel_session
  COMMAND sed_app_main_window_tests --cleave-wheel-session)
add_test(NAME sed_app_loose_document_watcher
  COMMAND sed_app_main_window_tests --loose-document-watcher)
add_test(NAME sed_app_ui_decimal_policy
  COMMAND sed_app_main_window_tests --ui-decimal-policy)
add_test(NAME sed_app_ui_decimal_consumers
  COMMAND sed_app_main_window_tests --ui-decimal-consumers)
add_test(NAME sed_app_resource_ui_decimal
  COMMAND sed_app_main_window_tests --resource-ui-decimal)
add_test(NAME sed_app_uv_numeric_precision
  COMMAND sed_app_main_window_tests --uv-numeric-precision)
add_test(NAME sed_app_transform_numeric_precision
  COMMAND sed_app_main_window_tests --transform-numeric-precision)
add_test(NAME sed_app_resource_io_boundary
  COMMAND sed_app_main_window_tests --resource-io-boundary)

set(SED_REPO_ROOT "${CMAKE_SOURCE_DIR}")
set(SED_APP_TEST_TEMP_ROOT "${CMAKE_BINARY_DIR}/test-temp/app")
file(MAKE_DIRECTORY "${SED_APP_TEST_TEMP_ROOT}")
set_tests_properties(sed_app_tests PROPERTIES
  ENVIRONMENT "TMPDIR=${SED_APP_TEST_TEMP_ROOT};TEMP=${SED_APP_TEST_TEMP_ROOT};TMP=${SED_APP_TEST_TEMP_ROOT}"
  LABELS "unit;module;domain;integration;cross_layer;cross_app_core;qt"
  RESOURCE_LOCK sed_gui_settings
  TIMEOUT 600
  WORKING_DIRECTORY "${SED_REPO_ROOT}"
)
set_tests_properties(sed_app_strict_surface_selection_recovery PROPERTIES
  ENVIRONMENT "QT_QPA_PLATFORM=offscreen;TMPDIR=${SED_APP_TEST_TEMP_ROOT};TEMP=${SED_APP_TEST_TEMP_ROOT};TMP=${SED_APP_TEST_TEMP_ROOT}"
  LABELS "module;integration;cross_layer;ui;offscreen;qt"
  RESOURCE_LOCK sed_gui_settings
  TIMEOUT 120
  WORKING_DIRECTORY "${SED_REPO_ROOT}"
)
set_tests_properties(sed_app_strict_surface_context_topology PROPERTIES
  ENVIRONMENT "QT_QPA_PLATFORM=offscreen;TMPDIR=${SED_APP_TEST_TEMP_ROOT};TEMP=${SED_APP_TEST_TEMP_ROOT};TMP=${SED_APP_TEST_TEMP_ROOT}"
  LABELS "module;integration;cross_layer;ui;offscreen;qt"
  RESOURCE_LOCK sed_gui_settings
  TIMEOUT 120
  WORKING_DIRECTORY "${SED_REPO_ROOT}"
)
set_tests_properties(
  sed_app_viewport_banner_settings
  sed_app_toast_overlay_painter
  sed_app_cleave_hud_presentation
  PROPERTIES
  ENVIRONMENT "TMPDIR=${SED_APP_TEST_TEMP_ROOT};TEMP=${SED_APP_TEST_TEMP_ROOT};TMP=${SED_APP_TEST_TEMP_ROOT}"
  LABELS "unit;qt"
  RESOURCE_LOCK sed_gui_settings
  TIMEOUT 120
  WORKING_DIRECTORY "${SED_REPO_ROOT}"
)
set_tests_properties(
  sed_app_main_window_tests
  sed_app_cleave_chain_origin
  sed_app_surface_multicut
  sed_app_cleave_bounded_recovery
  sed_app_cleave_connection_history
  sed_app_cleave_document_reset
  sed_app_cleave_input_routing
  sed_app_cleave_fly_custom_shortcuts
  sed_app_cleave_uv_custom_owner
  sed_app_cleave_shape_apply
  sed_app_light_gizmo_selection
  sed_app_light_gizmo_selection_hidpi
  sed_app_thing_light_history
  sed_app_thing_ortho_thin_placement
  sed_app_thing_placement_id_exhaustion
  sed_app_mots_arch_lighting
  sed_app_mots_arch_bake_flags
  sed_app_mots_arch_bake
  sed_app_thing_find_lighting_menu
  sed_app_template_collision_preview
  sed_app_template_duplicate_type
  sed_app_parameter_helper_dialogs
  sed_app_cog_menu_instance
  sed_app_cog_instance_picker
  sed_app_cog_overwrite_lifetime
  sed_app_cleave_family_controls
  sed_app_cog_interaction_regressions
  sed_app_cog_shortcut_context
  sed_app_cog_document_docking
  sed_app_resource_picker_release
  sed_app_loose_document_watcher
  sed_app_resource_authoring
  sed_app_resource_authoring_presentation
  sed_app_resource_authoring_history_caret
  sed_app_resource_workspace_regression
  sed_app_resource_authoring_lifecycle
  sed_app_resource_authoring_sprite_geometry
  sed_app_resource_document_history
  sed_app_resource_io_boundary
  sed_app_runtime_lighting_switches
  sed_app_cog_inline_assignment
  sed_app_native_save_edit_validation
  sed_app_minor_grid_appearance
  sed_app_uv_grid_points_appearance
  sed_app_viewport_grid_combo_popup_lifetime
  sed_app_frame_history_selection
  sed_app_frame_add_move_behavior
  sed_app_frame_render_performance
  sed_app_thing_collision_cache
  sed_app_toast_expiry_timer
  sed_app_viewport_shortcuts
  sed_app_light_mode_shortcuts
  sed_app_navigation_preferences
  sed_app_navigation_shortcut_context
  sed_app_surface_roundoff_multicut
  sed_app_cleave_wheel_session
  PROPERTIES
  ENVIRONMENT "TMPDIR=${SED_APP_TEST_TEMP_ROOT};TEMP=${SED_APP_TEST_TEMP_ROOT};TMP=${SED_APP_TEST_TEMP_ROOT}"
  LABELS "module;domain;integration;cross_layer;cross_app_core;ui;qt"
  RESOURCE_LOCK sed_gui_settings
  TIMEOUT 900
  WORKING_DIRECTORY "${SED_REPO_ROOT}"
)
set_property(TEST sed_app_light_mode_shortcuts APPEND PROPERTY ENVIRONMENT
  "QT_QPA_PLATFORM=offscreen")
set_property(TEST sed_app_navigation_shortcut_context APPEND PROPERTY ENVIRONMENT
  "QT_QPA_PLATFORM=offscreen")
set_tests_properties(sed_app_ui_decimal_policy sed_app_ui_decimal_consumers
  sed_app_resource_ui_decimal sed_app_uv_numeric_precision
  sed_app_transform_numeric_precision PROPERTIES
  ENVIRONMENT "TMPDIR=${SED_APP_TEST_TEMP_ROOT};TEMP=${SED_APP_TEST_TEMP_ROOT};TMP=${SED_APP_TEST_TEMP_ROOT}"
  LABELS "module;domain;integration;cross_layer;cross_app_core;ui;qt;decimal"
  RESOURCE_LOCK sed_gui_settings
  TIMEOUT 120
  WORKING_DIRECTORY "${SED_REPO_ROOT}"
)
set_property(TEST sed_app_light_gizmo_selection APPEND PROPERTY ENVIRONMENT
  "QT_ENABLE_HIGHDPI_SCALING=0" "QT_SCALE_FACTOR=1" "QT_SCREEN_SCALE_FACTORS=1"
  "QT_SCALE_FACTOR_ROUNDING_POLICY=PassThrough" "SED_TEST_EXPECTED_DPR=1")
set_property(TEST sed_app_light_gizmo_selection_hidpi APPEND PROPERTY ENVIRONMENT
  "QT_ENABLE_HIGHDPI_SCALING=1" "QT_SCALE_FACTOR=1" "QT_SCREEN_SCALE_FACTORS=2"
  "QT_SCALE_FACTOR_ROUNDING_POLICY=PassThrough" "SED_TEST_EXPECTED_DPR=2")
set_property(TEST sed_app_toast_expiry_timer APPEND PROPERTY ENVIRONMENT
  "QT_QPA_PLATFORM=offscreen")
set_property(TEST sed_app_resource_authoring_presentation APPEND PROPERTY ENVIRONMENT
  "QT_QPA_PLATFORM=offscreen")
set_property(TEST sed_app_resource_authoring_history_caret APPEND PROPERTY ENVIRONMENT
  "QT_QPA_PLATFORM=offscreen")
set_property(TEST sed_app_thing_placement_id_exhaustion APPEND PROPERTY ENVIRONMENT
  "QT_QPA_PLATFORM=offscreen")
include("${CMAKE_CURRENT_LIST_DIR}/../../cmake/runtime_data.cmake")
set(SED_RUNTIME_DATA_COPY_COMMANDS)
foreach(SED_RUNTIME_DATA_FILE IN LISTS SED_RUNTIME_DATA_FILES)
  list(APPEND SED_RUNTIME_DATA_COPY_COMMANDS
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
      "${SED_REPO_ROOT}/data/${SED_RUNTIME_DATA_FILE}"
      "$<TARGET_FILE_DIR:sed_app>/data/${SED_RUNTIME_DATA_FILE}"
  )
endforeach()
if(SED_RUNTIME_DATA_COPY_COMMANDS)
  add_custom_command(TARGET sed_app POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:sed_app>/data"
    ${SED_RUNTIME_DATA_COPY_COMMANDS}
  )
endif()

string(JOIN "|" SED_RUNTIME_DATA_SERIALIZED ${SED_RUNTIME_DATA_FILES})
add_test(NAME sed_package_layout
  COMMAND "${CMAKE_COMMAND}"
    "-DSED_PACKAGE_EXECUTABLE=$<TARGET_FILE:sed_app>"
    "-DSED_PACKAGE_PLATFORM=${CMAKE_SYSTEM_NAME}"
    "-DSED_PACKAGE_MSVC=$<BOOL:${MSVC}>"
    "-DSED_PACKAGE_QT_MAJOR=${QT_VERSION_MAJOR}"
    "-DSED_PACKAGE_DEBUG=$<CONFIG:Debug>"
    "-DSED_PACKAGE_RUNTIME_DATA=${SED_RUNTIME_DATA_SERIALIZED}"
    -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/verify_package_layout.cmake"
)
set_tests_properties(sed_package_layout PROPERTIES
  LABELS "system;package"
  TIMEOUT 60
)

add_test(NAME sed_package_layout_tests
  COMMAND "${CMAKE_COMMAND}"
    "-DSED_PACKAGE_LAYOUT_VERIFIER=${CMAKE_CURRENT_SOURCE_DIR}/tests/verify_package_layout.cmake"
    "-DSED_PACKAGE_LAYOUT_TEST_ROOT=${CMAKE_CURRENT_BINARY_DIR}/package-layout-tests"
    -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/verify_package_layout_tests.cmake"
)
set_tests_properties(sed_package_layout_tests PROPERTIES
  LABELS "module;system;package"
  TIMEOUT 60
)

add_test(NAME sed_release_package_staging_tests
  COMMAND "${CMAKE_COMMAND}"
    "-DSED_STAGE_TEST_ROOT=${CMAKE_CURRENT_BINARY_DIR}/release-package-staging-tests"
    -P "${CMAKE_SOURCE_DIR}/cmake/tests/stage_release_package_tests.cmake"
)
set_tests_properties(sed_release_package_staging_tests PROPERTIES
  LABELS "module;system;package"
  TIMEOUT 60
)

# Release packaging is deliberately an explicit target.  The ordinary app
# build may run without a complete installed-kit notice set; discovery and
# notice completeness are deferred to the target invocation.
if(WIN32 AND QT_VERSION_MAJOR EQUAL 6)
  set(SED_RELEASE_PACKAGE_OUTPUT_DIR
    "${CMAKE_SOURCE_DIR}/build/package"
    CACHE PATH "Output directory for the explicit SED release package target")
  set(SED_RELEASE_PACKAGE_PROJECT_LICENSE_FILES
    "${CMAKE_SOURCE_DIR}/LICENSE|${CMAKE_SOURCE_DIR}/THIRD_PARTY_NOTICES.md"
    CACHE STRING "Pipe-separated project license and notice files for release staging")
  set(SED_RELEASE_QT_REQUIRED_VERSION "" CACHE STRING
    "Optional Qt version check for the explicit release package target")
  set(_sed_release_qt_version "${QT_VERSION}")
  if("${_sed_release_qt_version}" STREQUAL "" AND DEFINED Qt6_VERSION)
    set(_sed_release_qt_version "${Qt6_VERSION}")
  endif()
  set(SED_RELEASE_QT_VERSION "${_sed_release_qt_version}" CACHE STRING
    "Qt version recorded and used for release notice discovery")
  set(SED_RELEASE_QT_VERSION_ROOT "" CACHE PATH
    "Qt version root containing the selected kit (auto-derived from Qt6::Core)")
  set(SED_RELEASE_QT_KIT_ROOT "" CACHE PATH
    "Selected Qt kit root (auto-derived from Qt6::Core)")
  set(SED_RELEASE_QT_DEPLOY_TOOL "" CACHE FILEPATH
    "Override for the selected Qt windeployqt executable")
  set(SED_RELEASE_QT_LICENSE_FILES "" CACHE STRING
    "Override pipe-separated Qt license and attribution HTML files")
  set(SED_RELEASE_QT_SBOM_FILES "" CACHE STRING
    "Override pipe-separated Qt SBOM files")
  set(SED_RELEASE_MSVC_NOTICE_FILES "" CACHE STRING
    "Override pipe-separated selected Visual Studio notice files")
  set(SED_RELEASE_MSVC_VS_ROOT "" CACHE PATH
    "Selected Visual Studio installation root (auto-derived from the MSVC compiler)")
  set(SED_RELEASE_MSVC_RUNTIME_FILES "" CACHE STRING
    "Override pipe-separated selected MSVC runtime DLLs")

  set(_sed_release_qt_core_location "")
  if(TARGET Qt6::Core)
    foreach(_sed_qt_location_property IN ITEMS IMPORTED_LOCATION_RELEASE IMPORTED_LOCATION)
      get_target_property(_sed_qt_location Qt6::Core ${_sed_qt_location_property})
      if(NOT "${_sed_qt_location}" STREQUAL ""
          AND NOT "${_sed_qt_location}" MATCHES "-NOTFOUND$")
        set(_sed_release_qt_core_location "${_sed_qt_location}")
        break()
      endif()
    endforeach()
  endif()

  set(_sed_release_qt_kit_root "${SED_RELEASE_QT_KIT_ROOT}")
  if("${_sed_release_qt_kit_root}" STREQUAL ""
      AND NOT "${_sed_release_qt_core_location}" STREQUAL "")
    get_filename_component(_sed_release_qt_bin_dir
      "${_sed_release_qt_core_location}" DIRECTORY)
    get_filename_component(_sed_release_qt_kit_root
      "${_sed_release_qt_bin_dir}" DIRECTORY)
  endif()
  if("${_sed_release_qt_kit_root}" STREQUAL "" AND DEFINED Qt6Core_DIR)
    get_filename_component(_sed_release_qt_cmake_dir "${Qt6Core_DIR}" DIRECTORY)
    get_filename_component(_sed_release_qt_lib_dir "${_sed_release_qt_cmake_dir}" DIRECTORY)
    get_filename_component(_sed_release_qt_kit_root "${_sed_release_qt_lib_dir}" DIRECTORY)
  endif()

  set(_sed_release_qt_version_root "${SED_RELEASE_QT_VERSION_ROOT}")
  if("${_sed_release_qt_version_root}" STREQUAL ""
      AND NOT "${_sed_release_qt_kit_root}" STREQUAL "")
    set(_sed_release_qt_version_root "${_sed_release_qt_kit_root}")
    get_filename_component(_sed_release_qt_kit_name
      "${_sed_release_qt_kit_root}" NAME)
    if(_sed_release_qt_kit_name MATCHES "^(msvc|mingw|clang|gcc|android|ios|wasm)")
      get_filename_component(_sed_release_qt_version_root
        "${_sed_release_qt_kit_root}" DIRECTORY)
    endif()
  endif()

  set(_sed_release_qt_deploy_tool "${SED_RELEASE_QT_DEPLOY_TOOL}")
  if("${_sed_release_qt_deploy_tool}" STREQUAL "" AND TARGET Qt6::windeployqt)
    set(_sed_release_qt_deploy_tool "$<TARGET_FILE:Qt6::windeployqt>")
  endif()

  set(_sed_release_msvc_vs_root "${SED_RELEASE_MSVC_VS_ROOT}")
  if("${_sed_release_msvc_vs_root}" STREQUAL ""
      AND DEFINED _sed_msvc_vc_dir AND NOT "${_sed_msvc_vc_dir}" STREQUAL "")
    get_filename_component(_sed_release_msvc_vs_root
      "${_sed_msvc_vc_dir}" DIRECTORY)
  endif()

  set(_sed_release_msvc_runtime_files "${SED_RELEASE_MSVC_RUNTIME_FILES}")
  if("${_sed_release_msvc_runtime_files}" STREQUAL ""
      AND DEFINED SED_MSVC_RUNTIME_FILES_SERIALIZED)
    set(_sed_release_msvc_runtime_files "${SED_MSVC_RUNTIME_FILES_SERIALIZED}")
  endif()

  add_custom_target(sed_release_package
    COMMAND "${CMAKE_COMMAND}"
      "-DSED_RELEASE_STAGE_SCRIPT=${CMAKE_SOURCE_DIR}/cmake/stage_release_package.cmake"
      "-DSED_PACKAGE_EXECUTABLE=$<TARGET_FILE:sed_app>"
      "-DSED_PACKAGE_OUTPUT_DIR=${SED_RELEASE_PACKAGE_OUTPUT_DIR}"
      "-DSED_PACKAGE_DATA_DIR=${CMAKE_SOURCE_DIR}/data"
      "-DSED_PACKAGE_RUNTIME_DATA=${SED_RUNTIME_DATA_SERIALIZED}"
      "-DSED_PACKAGE_PROJECT_LICENSE_FILES=${SED_RELEASE_PACKAGE_PROJECT_LICENSE_FILES}"
      "-DSED_PACKAGE_VERSION=${PROJECT_VERSION}"
      "-DSED_PACKAGE_PLATFORM=Windows"
      "-DSED_PACKAGE_MSVC=$<BOOL:${MSVC}>"
      "-DSED_PACKAGE_CONFIGURATION=$<CONFIG>"
      "-DSED_RELEASE_QT_REQUIRED_VERSION=${SED_RELEASE_QT_REQUIRED_VERSION}"
      "-DSED_RELEASE_QT_VERSION=${SED_RELEASE_QT_VERSION}"
      "-DSED_RELEASE_QT_VERSION_ROOT=${_sed_release_qt_version_root}"
      "-DSED_RELEASE_QT_KIT_ROOT=${_sed_release_qt_kit_root}"
      "-DSED_RELEASE_QT_DEPLOY_TOOL=${_sed_release_qt_deploy_tool}"
      "-DSED_RELEASE_QT_LICENSE_FILES=${SED_RELEASE_QT_LICENSE_FILES}"
      "-DSED_RELEASE_QT_SBOM_FILES=${SED_RELEASE_QT_SBOM_FILES}"
      "-DSED_RELEASE_MSVC_VS_ROOT=${_sed_release_msvc_vs_root}"
      "-DSED_RELEASE_MSVC_NOTICE_FILES=${SED_RELEASE_MSVC_NOTICE_FILES}"
      "-DSED_RELEASE_MSVC_RUNTIME_FILES=${_sed_release_msvc_runtime_files}"
      -P "${CMAKE_CURRENT_SOURCE_DIR}/release_package.cmake"
    DEPENDS sed_app
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
    COMMENT "Staging the explicit SED Release package"
    VERBATIM
  )
endif()
