# Developer documentation

This directory contains the maintained technical and level-designer contracts for SED. The repository is C++26-first
with C++23/C++20 fallback; `src/core` is portable C++, while `src/app` owns Qt and OpenGL integration.

Start with [README.md](../README.md) and the document for the subsystem you are changing. AI- and LLM-assisted work
must also follow [AGENTS.md](../AGENTS.md).

Path notation: inline repository paths are relative to this C++ project root (`cpp/` in the combined checkout); Markdown
links are relative to the document that contains them.

## Build and test

The build produces `sed_core`, `sed_app`, and their test targets. If Qt is unavailable, CMake still builds the core;
pass `-DSED_BUILD_APP=OFF` to request that explicitly.

For a Windows Qt Release build, use Ninja Multi-Config and set `QTDIR` to a compatible Qt kit. The included presets
use schema version 6 and require CMake 3.25 or newer; manual configuration requires at least CMake 3.21.

```powershell
$env:QTDIR = "<Qt MSVC kit directory>"
$env:QT_QPA_PLATFORM_PLUGIN_PATH = Join-Path $env:QTDIR 'plugins/platforms'
cmake --preset msvc-ninja
cmake --build --preset msvc-ninja-release --parallel 1
ctest --preset msvc-ninja-release
```

Replace `<Qt MSVC kit directory>` with one exact Qt kit directory on your system, which contains `bin/qmake.exe`.
Do not point `QTDIR` or `SED_QT_ROOT` at a parent directory containing multiple Qt kits; pass the intended kit root so
CMake cannot select a different version or ABI during discovery. The platform-plugin path lets deployed offscreen tests
load the selected kit's `qoffscreen.dll` when the test directory contains only `qwindows.dll`; each test retains its
configured Qt platform.

MSVC Release builds apply `/Brepro` to C++ executable and shared-library linking for the core and application targets.
Debug builds retain the existing Edit-and-Continue settings. Reproducible output still depends on identical source
inputs, build configuration, dependencies, and compiler/linker toolchain.

Do not run concurrent Ninja writers against the same `build/ninja` tree; use a different build directory for genuinely
parallel builds. The default Release executable is `build/ninja/src/app/Release/sed.exe`.

Without presets:

```powershell
cmake -S . -B build/ninja -G "Ninja Multi-Config" -DSED_QT_ROOT="$env:QTDIR"
cmake --build build/ninja --config Release --parallel 1
ctest --test-dir build/ninja -C Release --output-on-failure
```

Useful options:

- `SED_CPP_STANDARD=26|23|20` selects the preferred language mode.
- `SED_BUILD_APP=OFF` creates a Qt-free core build.
- `SED_BUILD_FUZZERS=ON` exposes the supported Clang/libFuzzer file-format target.
- `SED_APP_CONSOLE=ON` keeps a Windows console on the application target.

The complete Release gate includes `sed_core_tests`, `sed_app_tests`, `sed_app_main_window_tests`, isolated allocation
tests for document/resource operations, Frame transforms, geometry recalculation/transforms/duplication,
Cleave/Join/Merge and orbit resolution, plus
`sed_package_layout` and `sed_package_layout_tests`. Build all default targets before running the complete gate;
building only the ordinary test executables omits the isolated allocation executables. The real package-layout test verifies
the built `sed.exe`, required Qt runtime/plugins, and distributed editor data without launching the application. Its
companion module/system test exercises Windows Release/Debug, multimedia fallback, non-Windows, and missing, empty,
or directory-in-place-of-file branches with synthetic layouts. Run focused targets during iteration and the full CTest suite
before handoff. Tests must remain independent from personal settings, game installations, loose files, and previous
build output.

Foreground-dependent native GUI/input tests activate their own test windows and require an idle test desktop. Test
runners must not minimize, restore, reposition, or close unrelated applications. When the desktop is in use, run
background-safe checks and report native checks as pending; do not weaken guards or count a partial run as the full
Release gate. Release validation remains incomplete until the complete Release CTest gate passes.

Dedicated allocation executables inject failures only around the operation under test, keeping allocator
instrumentation out of ordinary core and application processes. They verify exact state, selection, timeline,
record IDs, output parameters, runtime geometry fields and retained recovery snapshots after rejected allocations.
Each sweep tracks whether the scheduled fault actually fired and reaches a successful run without an injected fault.

CTest labels describe the coverage layers rather than creating duplicate executables. Core tests carry `unit`, `module`,
`domain`, and `integration`; focused application tests add `cross_layer`, `cross_app_core`, and Qt coverage; MainWindow
tests cover module, domain, integration, cross-layer, and UI behavior; package checks carry module/system/package labels. See the
[core test guide](../src/core/tests/README.md) and [application test guide](../src/app/tests/README.md) for the ownership
matrix, checked-in vectors, focused groups, and rules for adding coverage.

## Runtime and icon assets

The Windows build and release package include the default 11-file runtime-data set from `data/`, including `shapes.tpl`
and each supported game's non-empty `.tpl` template database. The `ijimtpls.jlh`/`ijimtpls.jll` template-category
pair and `ijimwavs.jlh`/`ijimwavs.jll` WAV-category pair are optional user-authored or project-local data and are
omitted from the default package. Without an IJIM template category pair,
templates remain available through All and Thing placement uses type-based filters; the Template Editor hides its
category pane. Without an IJIM WAV category pair, discovered sounds appear under the generated `Sounds > Uncategorized`
category. JKDF2/MotS WAV categories are generated from discovered sounds when no project-local or mounted pair exists.
When a Thing category file is missing, empty, or invalid, the picker falls back to generated type-based categories (or
`Other`) so entries remain available. Tests reject a Release package that omits a supported project's non-empty
`.tpl` database.

Application icons and the embedded `eye.cur` fly cursor are owned under `src/app/resources`. Windows embeds
`app_flat_red_circle.ico` for the native shell/taskbar icon. Qt embeds the maintained PNG sizes and cursor through
`app_icons.qrc`, so normal startup has no filesystem fallback. `app_flat_red_circle.svg` and `app_window.svg` are the
editable masters for the two active icon families. Icon experiments and superseded variants are outside the maintained
source tree and are not part of the SED solution or Release gate.

Do not commit generated icon experiments, deployed Qt plugins, binaries, or build-tree copies of `data/`.

## Source publication and binary packaging

Publish the source files selected by Git, including tests, their synthetic fixtures, runtime `data/`, documentation,
and license notices. Local build trees, IDE state, logs, recovery files, test scratch files, and banner concepts are
ignored. Ignore rules do not remove files already tracked by Git; review `git status --short` and `git ls-files`
before publishing. Archiving the entire working directory bypasses these exclusions.

Keep living documentation free of dated status notes and developer-machine paths. Fixed synthetic paths and timestamp
values in tests are intentional contract vectors; public project/source links and required copyright/license text
are also intentional. Do not remove these to satisfy a blanket text search.

Review documentation images visually, including every animation frame, and inspect embedded metadata before
publication. Use neutral synthetic project and resource paths for captures; remove accidental private data and
developer checkout or installation paths from pixels and metadata while preserving required public credits.

The build output directory is a development workspace, not a ready-made release archive. Stage the application,
required Qt runtime/plugins, official app-local MSVC runtime DLLs selected for the build toolchain, editor data, and the
notices described in [THIRD_PARTY_NOTICES.md](../THIRD_PARTY_NOTICES.md) into a clean package. Exclude test
executables, extra development tools/copies, local settings, logs, recovery files, and debug symbols from the public
runtime package. Validate the staged layout and launch the extracted package on Windows before distribution; passing
file-layout tests alone does not establish interactive acceptance.

For a Windows Qt 6 MSVC package, configure with the selected Qt kit and invoke the explicit release target from this
C++ project root:

```powershell
cmake --build build/ninja --config Release --target sed_release_package
```

`sed_release_package` is intentionally excluded from ordinary builds. It stages `build/package`, copies the explicit
11-file editor-data set plus `LICENSE` and `THIRD_PARTY_NOTICES.md`, runs the selected imported Qt kit's
`windeployqt --release`, copies the selected app-local MSVC CRT set, and writes `RELEASE_MANIFEST.json` with relative
paths and SHA-256 hashes. It discovers the selected Qt 6 version's license, module attribution HTML, and SBOM files
from the selected kit/root and records that exact version in the manifest; it also requires the selected Visual Studio
`ThirdPartyNotices.txt` and `Redist.txt` when MSVC packaging is enabled. Missing or empty notice files fail the explicit
target before staging. On Windows, the staging pass queries the selected `windeployqt` dry-run mapping and excludes
outputs whose source is under the current `SystemRoot`; ICU files sourced from the selected Qt kit remain package-local.
The target does not infer redistribution permissions; source-delivery and complete license review remain a
maintainer/distribution gate.

The low-level [`cmake/stage_release_package.cmake`](../cmake/stage_release_package.cmake) script remains available for
maintainers who need an explicitly supplied file set. Package discovery can be adjusted through the target's
`SED_RELEASE_QT_VERSION_ROOT`, `SED_RELEASE_QT_KIT_ROOT`, `SED_RELEASE_QT_DEPLOY_TOOL`,
`SED_RELEASE_QT_VERSION`, `SED_RELEASE_QT_REQUIRED_VERSION`, `SED_RELEASE_QT_LICENSE_FILES`,
`SED_RELEASE_QT_SBOM_FILES`, `SED_RELEASE_MSVC_NOTICE_FILES`, and `SED_RELEASE_MSVC_RUNTIME_FILES` CMake cache
variables. `SED_RELEASE_MSVC_VS_ROOT` can override the selected Visual Studio root used for the standard notice paths.
`SED_RELEASE_QT_REQUIRED_VERSION` is an optional policy check; the selected imported Qt version remains the default and
is recorded in the manifest. Do not copy an unrelated system or old build-tree DLL to make the package pass. After
staging, inspect the manifest, extract the package into a clean Windows directory, and launch `sed.exe` there.

### Release preparation checklist

- Keep the source and binary deliveries separate. Publish the Git-selected source, tests and fixtures, active runtime
  data, maintained documentation, and license notices; stage the executable and its runtime files separately for a
  binary release.
- Preserve active resource masters and required companions, including the editable icon sources under
  `src/app/resources` and the checked-in README banner JPEG
  `docs/images/sed-readme-banner-no-logo-v5.jpg`. Keep experiments and superseded assets outside the maintained source
  tree and out of documentation links.
- Exclude from the source publication compiler artifacts such as object files and executables, build directories, IDE
  state, logs, recovery files, local settings, and personal or proprietary level samples. Archive obsolete experiments
  outside the maintained source tree and keep normal build and IDE workspaces ignored.
- Review the actual publication set with `git status --short` and `git ls-files`; ignored files that are already tracked
  still belong to the publication set until they are deliberately removed from tracking.
- Build a clean binary package with `cmake --build build/ninja --config Release --target sed_release_package`. The
  registered CTest package-layout checks and synthetic staging fixture cover layout and rejection contracts; inspect
  the actual staged package separately, then extract it to a clean location and launch it on Windows. A layout check
  alone does not prove interactive startup.

## Documentation map

- [user_guide.md](user_guide.md) - level-designer manual for projects, editing, selection, geometry, UVs, Things,
  lighting, COGs, top resource-group tabs/file tabs and Save All, layers, preferences, saving, checking, autosave, and recovery.
  Cleave covers Bounded workflows, Add Shape, shared endpoints, group transforms, editable cut ranks, and saved draft
  Undo/Redo with exact pending-cut restoration when undoing Apply. It also explains independent Sector/Surface draft
  retention, corner-angle guides, local keymap contexts, shape rotation and scaling by wheel, project-aware units,
  centered transformation readouts, and optional Sector Camera plane placement.
  Number-row edit modes and Shift-number submodes are listed beside their workflows. Synthetic UI sheets are
  linked beside the relevant Sprite, Sound Class, UNI, COG, and viewport instructions; COG resource pickers and
  symbol/script flag workflows are described separately.
- [project_context.md](project_context.md) - project context, implemented systems, deliberate behavior, and
  current limitations.
- [architecture.md](architecture.md) - runtime ownership, data flow, thread boundaries, and subsystem map.
- [algorithms.md](algorithms.md) - picking, transforms, snapping, grid, geometry tools, rendering, resource, UV, COG,
  and performance contracts.
- [lighting.md](lighting.md) - canonical authored, baked, runtime, GPU, and source-parity lighting reference.
- [porting_guide.md](porting_guide.md) - original-editor parity checklist and modernization roadmap.
- [porting.md](porting.md) - concise compatibility principles used when replacing legacy behavior.
- [sed_format.md](sed_format.md) - accessible overview of the native `.sed` format.
- [editor_state.md](editor_state.md) - authored Level and shared EDIT persistence inventory, application preferences,
  transient/presentation exclusions, and native/autosave/recovery degradation boundaries.
- [resource_authoring.md](resource_authoring.md) - detachable SPR/SND/UNI editors, project-backed controls, engine mode
  semantics, authored sprite geometry, source metadata, sound-column sizing, engine filename limits,
  exact-byte UNI keys/live external row counts, raw-draft preservation, and
  loose-file authoring contracts.
- [formats/sed_binary_format.md](formats/sed_binary_format.md) - normative `.sed` V1 through V4 binary contract.
- [formats/recovery_format.md](formats/recovery_format.md) - normative recovery/autosave container contract.

Living documentation must remain date-free and describe the current repository. Put durable architectural decisions
in the appropriate document rather than adding temporary session notes.
