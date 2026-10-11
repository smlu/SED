# Original-editor parity guide

This file tracks compatibility between the original SED/JED editor and the modern C++26-first application, with
C++23/C++20 fallback where required by the local compiler/toolchain.

Use this as the living checklist and module map for parity work.

## Contents

- [Related documents](#related-documents)
- [Scope and goal](#scope--goal)
- [Repository layout](#repository-layout)
- [Runtime architecture](#runtime-architecture)
  - [Data and undo](#data-and-undo)
  - [UI and viewport flow](#ui-viewport-flow)
- [Porting checklist](#porting-checklist)
  - [Build, tooling, and project hygiene](#0-build-tooling-and-project-hygiene)
  - [Core formats and assets](#1-core-formats-and-assets)
  - [Level data model](#2-level-data-model-sed_core)
  - [Editing operations](#3-editing-operations)
    - [Lighting](#lighting)
  - [Editor services](#4-editor-services)
  - [Viewport renderer and tools](#5-viewport-renderer-and-tools)
  - [Qt UI](#6-qt-ui-sed_app)
  - [Performance milestones](#7-performance-milestones)
  - [Parity verification](#8-parity-verification)
- [Implemented workflows](#implemented-workflows)
- [Known gaps and deliberate boundaries](#known-gaps-and-deliberate-boundaries)
  - [Pascal audit basis](#pascal-audit-basis)
  - [Verified live Pascal gaps](#verified-live-pascal-gaps)
  - [Forward-looking work that is not Pascal parity](#forward-looking-work-that-is-not-pascal-parity)
  - [Audited replacements and exclusions](#audited-replacements-and-exclusions)
  - [Deliberate or verified boundaries](#deliberate-or-verified-boundaries)
- [Suggested next steps](#suggested-next-steps)

## Related documents

- [Architecture overview](architecture.md)
- [Algorithms](algorithms.md)
- [Lighting reference](lighting.md)
- [Project context](project_context.md)
- Native SED V1 through V4 and recovery V1 contracts: [SED binary format](formats/sed_binary_format.md) and
  [recovery format](formats/recovery_format.md)

## Scope / goal

- Maintain a cross-platform C++ editor (`sed_app`) that can load, edit, and save the same level data as the original
  editor while using clearer modern workflows where appropriate.
- Keep non-UI logic portable and testable in `sed_core`.
- Use `Qt` for UI and `OpenGL` for the preview renderer.

Non-goals:

- exact visual/UI duplication of the Delphi application
- cleanup that weakens parity or removes behavior that is not covered by tests

## Repository layout

- [src/core](../src/core)
  - portable library: math, data model, file formats, geometry/editing ops, editor document/undo, camera helpers
- [src/app](../src/app)
  - Qt shell: startup flow, outliner, properties, menus, overlays, preferences, history, resource dialogs, category
    editing, OpenGL viewport

## Runtime architecture

### Data and undo

- Authoritative state lives in `sed::editor::DocumentState`:
  - `sed::Level level`
  - `sed::editor::Selection selection`
- Undo/redo is managed by `DocumentUndoHistory` using compact entity subsets or full snapshots with dirty flags to drive incremental viewport refreshes.
- User-facing history labels describe completed operations in past tense and name the owning entity first, using real
  singular/plural forms (`Thing: Moved`, `Things: Param 'light' changed`, `Sector: Point light range changed`,
  `Baked Light: Range changed`, `Baked Lights: Range changed`). Do not introduce pseudo-entities such as `Thing Light`
  or `Sector Point Light`, and never use `Thing(s)`/similar placeholders or hide authored Thing-light parameter names. A
  multi-parameter gesture names both authored parameters (for example, `Thing: Params 'lightintensity' and
  'lightoffset' changed`).

<a id="ui-viewport-flow"></a>

### UI <-> viewport flow

- `MainWindow` owns `document_`, performs mutations through `sed::level::*`, pushes undo labels, and refreshes the UI.
- `GlView` owns rendering, GPU picking, overlays, interaction tools, and preview capture.
- `GlView` emits semantic signals such as `thingsTranslateRequested`, `surfacesRotateRequested`, `framePicked`, and box
  select requests; `MainWindow` commits the edits.

## Porting checklist

Legend:
- `OK` implemented
- `LIMIT` implemented with a specifically documented boundary
- `VERIFY` implemented, with a named manual or legacy-comparison check still outstanding
- `TODO` not started or not production-ready

### 0) Build, tooling, and project hygiene
- `OK` CMake build for `sed_core` + optional `sed_app`: [developer documentation](README.md)
- `OK` primary Windows tree uses `build/ninja` with `Ninja Multi-Config`
- `OK` C++ standard selection defaults to C++26 when supported and falls back through C++23/C++20
- `OK` deterministic checked-in SED V1/V2 conformance vectors for JKDF2, MotS, and IJIM, plus focused V3/V4
  arch-lighting/history coverage, including byte-exact native output, canonical JKL/NDY re-export, repair within
  parity, beyond-parity partial salvage, and categorized-resource indexing:
  [SED vectors](../src/core/tests/assets/fixtures/sed_vectors/README.md)
- `VERIFY` automated legacy comparison outside the native-format vectors is not exhaustive; the maintained core and
  application suites remain authoritative for intentional modern behavior:
  [core tests](../src/core/tests) and [application tests](../src/app/tests)
- `OK` repeatable Windows Release configure/build/test workflow through the `msvc-ninja` presets; configuration must
  run where CMake can discover the MSVC toolchain and `QTDIR` identifies a compatible Qt MSVC kit
- `TODO` continuous parity checks against normalized legacy outputs

### 1) Core formats and assets

Source directory: [src/core/src/assets](../src/core/src/assets)

- `OK` JED load/save core support: [jed.cpp](../src/core/src/assets/jed.cpp)
  - application export is intentionally limited to JKDF2/MotS; IJIM keeps JED loading for migration and uses NDY for
    runtime export
- `OK` lossless binary SED V1 through V4 load/save for JKDF2/MotS/IJIM: [sed.cpp](../src/core/src/assets/sed.cpp)
  - `.sed` is the sole native Save/Save As format, labelled **Sith Editor File**; JKL/NDY/JED are separate
    Export/Export As targets. It preserves the complete `Level`, recovery-grade editor/camera/window/COG state, and
    `sed-semantic-base-delta-v1` undo/redo timeline with periodic checkpoints in
    checked `edit`/`hist` sections. `edit` retains stable tool/workspace settings and staged Multi-cut lines, not a
    restartable event stream for active Surface Extrude previews or partially held brush/draw gestures; those finalize
    or cancel at snapshot/apply boundaries. The [persistence inventory](editor_state.md) records resource table
    presentation, UV pivot, modal Template draft, and unresolved JKL COG default-hydration limits
  - native sections use 4096-byte CRC32C slots and systematic stream-major GF(256) stripes: `p=4` for required Level
    and recovery-critical `edit`, `p=2` for lower-priority `hist`/future optionals; recovery reports and defaults/omits
    an over-budget semantic domain as a whole
  - exact native boundaries are fixed: header/descriptor CRC mirrors are outside payload MDS, alignment and unused slot-tail
    padding are canonical zero (strict rejects, recovery normalizes/reports), expanded SHA-256 covers the semantic unit,
    and the footer is a marker; there is no descriptor directory or internal
    record-group salvage. Fixed-slot repair handles substitutions and unreadable/truncated erasures only while the
    absolute 4096-byte phase remains established; mid-section insertions/deletions are not guessed or rephased
  - direct corrupted `.sed` Open is strict first, keeps repaired/identified-optional-loss files associated but dirty,
    confirms required or unidentified loss before candidate resource mutation, detaches accepted partial Levels as
    `Untitled*` (an unidentified unit may conceal a required domain), preserves source bytes, and requires an explicit
    project choice when both headers and `head` are lost
  - recovery `docm` is raw native `.sed` without duplicate outer CRC/SHA; protected `meta`, resilient range reads,
    bounded metadata-only discovery with all-field copy agreement, forensic retention, and non-authoritative
    JKL/NDY+manifest salvage cover the outer crash workflow; required or unidentified loss schedules text salvage, which
    contains only fully decoded semantic domains/defaults and never replaces the forensic binary
  - runtime/legacy imports still perform their established clean camera/editor reset
- `OK` JKL load/save: [jkl.cpp](../src/core/src/assets/jkl.cpp)
  - exact WORLD/SECTORS counts, required Sector `SURFACES`, and world-vertex references reject malformed/truncated
    geometry; extra `SECTOR` records beyond the declared count reject within SECTORS, while comments, unknown
    extension fields, and later sections retain their normal handling. Texture-vertex-index zero fallback and intentional
    resource spare capacities retain their legacy behavior
  - new projects seed Pascal-compatible editable extras, while export writes literal Required + Extra and imported
    NDY/JKL files recover Extra from declared minus parsed counts; zero adds no hidden reserve
  - `World cogs` counts actual runtime instance demand:
    placed rows plus explicit non-`none` template/Thing `cog=` occurrences, including duplicate filenames
  - advanced Level Settings reports Required, editable Extra, and final Export total for every safe pool using the
    writer's own crawler without exposing exact-record geometry/sector/light/layer headers
  - Export/Export As expose independently optional editor `Lights` and `Layers` sections (off by default) and a
    parser-compatible compact LF/tab/no-padding/short-float presentation mode (on by default); compact mode also
    removes parser-only field-comment lines, preserves the copyright block and quoted/resource/name text, and changes
    only safe field separators and finite numeric spellings. The last accepted choices are reused for subsequent engine
    exports in the current editor session
- `LIMIT` custom JKL/NDY `THINGS` rows with sparse or out-of-order numeric labels are appended in file order on
  import; SED compacts their indices, while the game loaders use the row labels. Placed COG Thing references can then
  resolve differently.
- `LIMIT` a placed COG integer token `0x10` hydrates as 16 in SED, while the game parser uses decimal `atoi` and
  reads it as 0. Decimal tokens avoid this difference.
- `LIMIT` an unknown COG symbol declaration makes SED reject the script schema for typed hydration, leaving placed
  arguments raw; the game configuration reader skips the unknown declaration. Typed entity-reference maintenance may
  therefore be unavailable for that script. These three patterns were absent from the original placed-level inputs
  inspected in this audit; the observation does not cover custom levels or every possible resource.
- `OK` LEV load: [lev.cpp](../src/core/src/assets/lev.cpp). File Open presents the live Pascal import choices before
  replacement: validated coordinate scale (`40` by default), **Keep texture names** or **Set all textures to
  DFLT.MAT**, and optional matching `.O` object import
- `OK` ASC load: [asc.cpp](../src/core/src/assets/asc.cpp)
- `TODO` forward-looking LEV save; the live Pascal application does not export LEV
- `TODO` ASC save/export: C++ SED can import ASC, but **Save As ASC** / **Export As ASC** is not implemented. The live
  Pascal application is also import-only, so ASC writing is forward-looking work rather than a legacy-parity gap
- `OK` 3DO load: [3do.cpp](../src/core/src/assets/3do.cpp), with declared-row preflight and aggregate
  128 MiB/2,097,152-item admission for raw geometry, required shared snapshots, and expanded hierarchy clones.
  Independent source, section-view, token-view, and hierarchy-work bounds are editor operational defaults; see the
  [3DO admission contract](architecture.md#asset-loading-and-resource-location).
- `OK` SPR load: [spr.cpp](../src/core/src/assets/spr.cpp)
- `OK` MAT decoding supports indexed/IndexedCols, packed 16/24/32-bit RGB(A), color MATs, multiple cels, mipmaps,
  palette-index-zero alpha reference, and bounds-checked malformed-input rejection in
  [mat.cpp](../src/core/src/assets/mat.cpp). An audit of all 748 extracted retail JKDF2 `Res2_GOB` MATs and all 1,142
  extracted retail MotS `JKMRES` MATs confirmed that indexed MATs reliably use mode/bpp while the remaining
  `ColorInfo` words occur as repeated legacy/uninitialized patterns. SED therefore treats indexed `alphaBPP` only as
  the observed zero/nonzero alpha-reference marker, not as a literal channel width.
  No unhandled `ColorInfo` layout was found in either supported game's retail inventory
- `OK` [CMP parsing](../src/core/src/assets/cmp.cpp) retains the 256-color palette, all 64 256-byte light-level
  translation tables, and the final-table compatibility view used by original SED `LoadCMPTable`; these are
  palette-index translation tables, not CMP header Tint. The core exposes and tests master-palette translation and
  per-MAT Solid-color light-ramp resolution
- `LIMIT` applying a non-identity active Sector CMP translation to textured pixels in every editor renderer/preview
  remains an explicit rendering compatibility boundary; it is not a MAT-decoding gap
- `OK` runtime resource discovery searches configured disk roots and mounted GOB/GOO archives through
  [resource_locator.cpp](../src/core/src/assets/resource_locator.cpp), including IJIM's canonical
  `InstallDir/Resource` preference, revision-aware reads, and transactional archive remount. GOB/GOO creation,
  package browsing, and launch-from-editor remain separate unimplemented workflows rather than locator defects
- `OK` JLH/JLL load/save for editable category trees: [jlh.cpp](../src/core/src/assets/jlh.cpp)
- `OK` legacy-facing COG entity defaults / parsing details:
  - public `cog` / `sector` / `surface` / `thing` references default to `-1`; scalar `int` / `flex` / `float` values
    default to zero; vector declarations have no initializer, and their zero placed value is represented as `(0/0/0)`
    in level data. COG code literals use single-quoted, space-separated text such as `'1 2 3'`
  - absent `ai` / `key` / `material` / `model` / `sound` / `template` resource values remain empty until authored;
    JKL/NDY uses `-1` for an empty resource token and rejects values that cannot fit one engine token
  - local symbols and message declarations are omitted from placed-COG arguments, preserving positional alignment
  - JED `surface` COG values reassemble `sector surface` token pairs on load
  - script and legacy JED `sound` COG fields normalize to `CogType::wav`, retaining WAV-picker behavior in the item
    editor
- `OK` JKL local surface numbering regression fixed and covered by tests; noncanonical `SURFACES` spans preserve typed
  COG targets through import/export, and geometry edits remap Thing and paired Surface references by semantic identity

### 2) Level data model (`sed_core`)
- `OK` core `sed::Level` model: [level.hpp](../src/core/include/sed/level.hpp)
- `OK` templates and Thing values (`TplValue`): [tpl.hpp](../src/core/include/sed/assets/tpl.hpp)
- `OK` editor layer model and load/save support: [level.hpp](../src/core/include/sed/level.hpp),
  [jkl.cpp](../src/core/src/assets/jkl.cpp), and [jed.cpp](../src/core/src/assets/jed.cpp)
- `OK` native `.sed` preserves every field represented by the current Level model. Engine and legacy interchange
  formats are intentionally not a universal 1:1 mapping: each JKL/JED/NDY exporter applies its documented target
  rules, optional editor sections, and known lossy boundary such as IJIM actor Headlight range text. The normative
  ownership and reductions are in [sed_format.md](sed_format.md) and
  [sed_binary_format.md](formats/sed_binary_format.md), so there is no unbounded "verify every default" checklist item

### 3) Editing operations

Source directory: [src/core/src/level](../src/core/src/level)

Transforms
- `OK` translate/rotate/scale Things: [transform.cpp](../src/core/src/level/transform.cpp). Numeric Scale changes selected
  Thing positions around the selection pivot; it does not change a rendered model or sprite's size. Ordinary Things do
  not expose the viewport Scale gizmo.
- `OK` translate/rotate/scale Sectors, Surfaces, edges, vertices, and lights:
  [transform.cpp](../src/core/src/level/transform.cpp)
- `OK` Thing frame/path transforms: [transform.cpp](../src/core/src/level/transform.cpp)
- `OK` Sector and Surface transforms move valid adjoined mate-surface vertices once and recalculate connected
  geometry without reprojecting existing per-corner UV or baked-light values. Edge and vertex tools operate on the
  explicit deduplicated component selection; the modern preview and commit paths use the same selected set instead of
  reproducing Pascal's internal drag loop. Coverage is in
  [level_transform_tests.cpp](../src/core/tests/level/level_transform_tests.cpp) and
  [main_window_event_filter_tests.cpp](../src/app/tests/main_window_event_filter_tests.cpp)

Geometry tools
- `OK` extrude surface: [build.cpp](../src/core/src/level/build.cpp)
  - preview path builds temporary sector/source-cut geometry before commit
  - expand bounds prevent stepping through negative-collapse limits
- `OK` Add Sector primitive and saved-shape construction: [build.cpp](../src/core/src/level/build.cpp)
- `OK` adjoin/unadjoin: [geometry.cpp](../src/core/src/level/geometry.cpp)
- `OK` cleave Surface/edge/Sector: [cleave.cpp](../src/core/src/level/cleave.cpp)
  - surviving authored corners remain exact; only new edge intersections interpolate UV and baked-light values in
    each owning face's source frame, while new cap faces receive a generated mapping
- `OK` join/connect Surfaces and Sectors: [cleave.cpp](../src/core/src/level/cleave.cpp)
  - user-facing tools are named Adjoin / Unadjoin / Connect surfaces / Join sectors / Merge sectors
  - core APIs retain legacy `levelConnect*` / adjoin terminology
  - Sector Join supports preview plus a **Keep overlap pieces** option for legacy keep-pieces vs cutter-only behavior
  - cutter-only join removes knife overlap inside targets and reruns portal/adjoin matching for rotated or complex cuts
- `OK` merge Surfaces/Sectors/vertices: [merge.cpp](../src/core/src/level/merge.cpp)
- `OK` modern edge/vertex editing includes active and multi-selection, safe and forced deletion, grid snapping,
  component-to-component snapping, contact sticking, aligned edge/face snapping, and preview/commit agreement. These
  tested rules supersede undocumented Pascal special cases rather than remaining incomplete solely because they differ
  from the old UI; see [transform.cpp](../src/core/src/level/transform.cpp),
  [surface_transform_snap_utils.cpp](../src/app/src/render/snap/surface_transform_snap_utils.cpp), and
  [main_window_event_filter_tests.cpp](../src/app/tests/main_window_event_filter_tests.cpp)

#### Lighting

- Canonical behavior/equations/source links: [lighting.md](lighting.md)
- `OK` legacy-style baked-light calculations: [lighting.cpp](../src/core/src/level/lighting.cpp)
- `OK` signed `[-1, 1]` surface extra light in JK/MotS and IJIM, including typed/live-scrub editing, Gouraud dimming,
  and JKL/JED round trips; IJIM retains RGB picking and limits extra-light alpha editing to Solid/Textured adjoins
- `OK` light range/intensity clamped non-negative in UI/core paths
- `OK` lighting refresh now updates thing shading as well as sector/surface lighting
- `OK` Light edit-mode background bake controls with visible-sector scope, automatic throttle/coalescing,
  stale-result rejection, progress/status UI, and cooperative cancellation
- `OK` dirty-region automatic Baked-light recalculation, old/new influence retention across cancellation and transform
  preview, copied-level workers, generation/fingerprint publication gates, and no-valid-light stale-output clearing
- `OK` separate Baked, Thing, and IJIM-only Sector owner families with source-correct authoring, picking, gizmos,
  transforms, inherited Thing-value resolution, and runtime transform preview
- `OK` runtime regular Thing/Headlight equations and caps for IJIM and classic projects, project-specific regular
  owner-self behavior, no Headlight self contribution, independent classic Diffuse attenuation/saturation, IJIM flat
  self-light, IJIM Sector zero-range derivation, Gouraud-only Sector participation, GPU std140 transport, and safe
  shader fallback
- `OK` fully capped face/mesh/viewport dynamic targets, transform-safe Diffuse face anchors, and realtime Thing-light
  color/scrub popups with outside-click/Escape dismissal and one undo record per interaction
- `OK` optional persisted per-pixel runtime-light shader; original engine-style vertex/Gouraud evaluation remains the
  default and baked lighting is unaffected
- `OK` JKDF2/MotS Tint as a post-light/pre-texture HW-vertex effect: Game Preview uses the engine camera-sector scope,
  Editor mode exposes owner-local Tint on world geometry and actual 3DO/SPR Things, filled editor adjoins retain their
  owning sector instead of borrowing mirror-sector Tint and use a two-sided authoring presentation, explicit classic
  translucency uses alpha `90/255`, MAT-alpha adjoins enter the no-z-write blend pass, and IJIM is explicitly neutral
- `NOTE` Auto observes Baked `SedLight` changes only; runtime Thing/Sector emitters never enter baked output, and
  arbitrary geometry changes invalidate but do not independently schedule a bake
- `OK` baked portal reachability is regression-covered for front-side rejection, cyclic graphs, `SAF_BlockLight`, and
  `LF_NoBlock`; no missing traversal branch is currently known. Representative pathological authored maps remain useful
  visual/golden maintenance comparisons when the algorithm changes. See
  [level_lighting_tests.cpp](../src/core/tests/level/level_lighting_tests.cpp)

### 4) Editor services

Source directory: [src/core/include/sed/editor](../src/core/include/sed/editor)

- `OK` selection model: [selection.hpp](../src/core/include/sed/editor/selection.hpp)
- `OK` snapshot undo/redo: [undo.hpp](../src/core/include/sed/editor/undo.hpp)
- `OK` modern undo groups each completed user interaction transactionally, validates restored state, preserves the
  timeline in native `.sed`, and uses entity-specific history labels. It intentionally supersedes legacy grouping
  rather than seeking byte-for-byte Pascal history behavior
- `LIMIT` some small edits still publish broad renderer/UI dirty domains and can rebuild more cache or property state
  than necessary. This is an incremental-refresh performance boundary, not an undo safety or grouping defect

### 5) Viewport renderer and tools

Source directory: [src/app/src/render](../src/app/src/render)

Rendering
- `OK` level and Thing rendering for wire/solid/textured modes:
  [gl_view_render.cpp](../src/app/src/render/gl_view_render.cpp)
- `LIMIT` MotS hierarchy node flag `0x2` special shading is not reproduced by the shaded viewport; custom models
  using it can differ from the game. The tree-collision preview for that flag is a separate path.
- `LIMIT` SED visits every parsed 3DO geoset mesh and can render drawable orphan meshes, while
  OpenJKDF2/OpenJones3D traverse hierarchy-reachable meshes. A custom model can therefore show extra geometry in the
  editor.
- `VERIFY` sprite billboard parity: type-0 Game Preview facing matches the engine's unconditional view-space card,
  and type-2 basis/roll behavior is audited against pinned OpenJones3D `develop`; the shared shaded-frame helper has
  deterministic ordinary/near-pole-threshold/+Z/-Z/malformed coverage, while final texture orientation and
  representative near-pole type-2 assets still need visual comparison
- `OK` separated translucent level/Thing/sprite sources share one stable far-to-near queue, and the alpha-depth prepass
  respects final Thing/material alpha
- `LIMIT` intersecting or cyclic alpha-heavy geometry lacks OIT and cannot always be represented by one correct
  sorted order; grouped level primitives, representative depths, and the depth cutoff remain heuristics even though
  the former source-category ordering defect is fixed
- `VERIFY` orthographic depth precision and z-fighting have automated projection/state coverage; representative very
  large and far-from-origin levels remain useful visual stress cases

Picking and selection
- `OK` GPU ID picking with peel/cycle: [gl_view_picking.cpp](../src/app/src/render/gl_view_picking.cpp)
- `OK` Blender-style box selection modifiers with keymap-driven add/remove/path semantics and in-viewport hint text:
  [gl_view_input.cpp](../src/app/src/render/gl_view_input.cpp) and
  [event_filter.cpp](../src/app/src/main_window/events/event_filter.cpp)
- `OK` Surface/edge/vertex point picking uses intentional logical-pixel tolerances, scene-depth occlusion, backface
  rules, X-Ray/Alt traversal, overlap peeling, and stable repeated-click cycling. These modern tested priorities replace
  the Pascal picker rather than waiting for unspecified "fine-tuning"; see
  [viewport_pick_cycle_utils.hpp](../src/app/src/render/picking/viewport_pick_cycle_utils.hpp),
  [level_picking_tests.cpp](../src/core/tests/level/level_picking_tests.cpp), and
  [main_window_event_filter_tests.cpp](../src/app/tests/main_window_event_filter_tests.cpp)

Transform gizmos and overlays
- `OK` select/move/rotate tool modes and gizmo dragging preview:
  [gl_view_gizmos.cpp](../src/app/src/render/gl_view_gizmos.cpp)
- `OK` local/world transform orientation toggle
- `OK` shared select/translate/scale/rotate/orientation shortcuts across edit modes
- `OK` axis widget, transform overlay, mode overlay, toast/hint overlays
- `OK` separately persisted transform-grid controls: direct hard lattice **Snap moves to grid** (default off) and
  **Grid stick** (default on/disableable) icon-only buttons in the top-center Thing/Frame/Sector/Surface/Light Move options bars, with a
  separate rebindable exact-match hold inverter for each grid/geometry/Thing snap behavior
- `OK` compact Move-option tooltips and distinct lighting/Thing-render mode glyphs at viewport-overlay size
- `OK` grouped View active-state menus, a non-toggle-highlighted Thing launcher, and Level Settings document/gear icon
- `OK` exact-match transform modifiers: shared snap-family `Ctrl` for Grid Snap, sector align, and Thing snap;
  Grid Stick `Shift+Alt`; Thing surface stick `Shift`; surface align `Ctrl+Shift`; and rotate-to-snap `Ctrl+Alt`
- `OK` three-state grid visibility: depth-occluded by solid scene geometry by default, optional visible-through-walls
  overlay, or always-behind draw order; Map Settings uses distinct code-drawn icons and highlighted active rows
- `OK` persisted grid opacity, independently configurable minor/major colors, and `0.5`-`10.0 px` line thickness from
  Map Settings and Preferences; 100% opacity produces solid line centres and the analytic grid shader keeps a crisp
  one-pixel antialiased edge as thickness increases; a derivative-filtered 10x major-line hierarchy replaces dense
  distant periods before they can form perspective Moire. Colored axes use stable homogeneous screen-line projection
  and remain independently capped at `4 px`
- `OK` move snapping uses the active rendered fixed/rotated/view-aligned lattice and adaptive minor spacing; free moves
  preserve plane-normal depth, including top-down XY-grid moves of vertices, things, and sectors
- `OK` sector and surface face/edge/vertex moves use support-point plane contact, hysteresis, smooth latched release, and
  no same-drag recapture; multi-vertex surface moves commit the complete deduplicated selection
- `OK` engine/legacy-import and new-project top-view refit with extra level-fit margin; native SED/recovery restore the
  serialized view instead
- `VERIFY` empty-view and unusual restored dock layouts retain guarded projection/layout paths; malformed persisted
  state is covered, while platform-specific native window-manager combinations remain manual UI checks

Thing frame/path editing
- `OK` Thing frame/path editing is a complete modern workflow: frame points can be displayed, picked, multi-selected,
  added, deleted, moved, rotated, snapped to geometry or Things, transformed with their owner when requested, and
  restored through undo/recovery. The core positional scale helper is tested but the viewport Scale gizmo is not exposed
  for frame points because frames carry position/orientation rather than a rendered object size. Core transforms are in
  [transform.cpp](../src/core/src/level/transform.cpp),
  rendering is in [gl_view_render.cpp](../src/app/src/render/gl_view_render.cpp), input is in
  [gl_view_input.cpp](../src/app/src/render/gl_view_input.cpp), and regression coverage is in
  [level_transform_tests.cpp](../src/core/tests/level/level_transform_tests.cpp) and
  [main_window_event_filter_tests.cpp](../src/app/tests/main_window_event_filter_tests.cpp)
  Frame Add selects its new point after count synchronization; property Add/Duplicate/Delete and native Save retain
  selected/active points by path ordinal when stored value indices shift.

### 6) Qt UI (`sed_app`)

- `OK` Level Settings exposes the complete retained classic header for JKDF2/MotS: mipmap distances 1-4, LOD
  distances 1-4, perspective distance, Gouraud distance, pixels-per-unit, and Master CMP. Every edit is undoable; IJIM
  hides the classic group and retains its separate fog controls
- `OK` opening a Dark Forces LEV presents scale, texture-name/default-material, and matching-object choices before
  document replacement; cancel leaves the current document unchanged
- `OK` main window, docking, outliner, properties, messages: [src/app/src/main_window](../src/app/src/main_window)
- `OK` startup screen / pre-editor launch flow
- `OK` Visual Studio-style startup screen with searchable/pinnable recents, clearer `Open...`, and project configure
  affordances
- `OK` recent levels are tracked on successful open, native SED save, and engine export without rebinding the native
  save path; pinned/unpinned ordering is stable
- `OK` preferences storage via `QSettings`: [panel.cpp](../src/app/src/main_window/preferences/panel.cpp)
- `OK` generic resource picker with item-editor integration:
  [resource_picker.cpp](../src/app/src/ui/resources/resource_picker.cpp),
  [resource_picker.hpp](../src/app/src/ui/resources/resource_picker.hpp), and
  [property panel](../src/app/src/main_window/properties/panel.cpp)
- `OK` picker Storage metadata identifies the selected absolute loose path or case-preserved GOB/GOO entry inside
  text/audio metadata or below image previews; bold names separately indicate available loose copies
- `OK` Viewport Settings stays open while selecting from its owned dropdown and dismisses outside its panel/popup
- `OK` picker first-show path uses an opaque loading page, deferred one-shot initial population, and a shared
  revision-keyed disk-resource index instead of repeated per-row filesystem probes; MainWindow pickers adopt the
  published immutable locator generation instead of copying mounted indexes before first paint
- `OK` raw `3DO` and template pickers create an empty hidden `GlView` host before first Show, preventing Qt from
  destroying/remapping a visible modal when its first `QOpenGLWidget` is added; model loading and activation remain
  deferred until selection
- `OK` project-specific category filename policy for template/material/WAV pickers and editable trees for
  template/material pickers and template editor
- `OK` WAV categories use Pascal's project-specific `wavs.jlh` / `mwavs.jlh` / `ijimwavs.jlh` policy, support saved
  project overrides and arbitrary nesting, and seed `Sounds > Uncategorized` from active resources when no pair exists
- `OK` WAV list-mode selection is a stable inset card and ignores native hover painting without changing selection or
  preview state
- `OK` audio pickers hide model-only projection/render/**Fit** controls, and category deletion uses aligned plain text
  with **No** as the safe default/escape action
- `OK` category membership drag/drop carries the complete real-name multi-selection, preserves current category view,
  and supports validated Copy / `Ctrl`-Move semantics from non-`All` categories
- `OK` picker/template membership edits share an alias-preserving, rollback-safe JLH/JLL transaction. Focused coverage
  exercises payload/action routing, selection cleanup, synthetic multi-item drop, Copy/Move/deduplication, failed-save
  restoration, hidden-`General` rollback, and save/reload at the common production seam. Disk pair saves stage both
  outputs, back up both originals, roll back reported commit failures, and retain a named recovery directory if rollback
  itself fails; the two-path commit is not power-loss atomic and successful cleanup may leave an obsolete backup folder
- `OK` raw `3DO` list thumbnails use worker-owned software rasterization rather than hidden-`GlView` repaint/readback;
  selected interactive previews alone remain OpenGL-backed
- `OK` layer manager, visibility/lock menus, and per-layer color controls
- `OK` flag editors with descriptions and raw unknown-bit preservation
- `OK` searchable keymap editor and shortcut persistence
- `OK` shortcut dispatch is guarded so viewport bindings do not steal typing from focused text editors/search fields
- `OK` undo/redo history window with jump-to-state interaction
- `OK` unsaved-state tracking in window title plus save prompts before destructive navigation actions
- `OK` dirty COG tabs now prompt on app close
- `OK` docked COG editor workflow with diagnostics, tabs, pinning, save handling, richer RMB/Insert helpers, and
  theme controls
- `OK` Pascal-style `Find...` / `Find Next` current-mode query workflow with entity-specific filters and browse
  helpers
- `OK` complete active Pascal consistency/resource checker rule coverage, with C++ malformed-data diagnostics,
  COG-dirty refresh, and a persisted default-off sector-overlap pass
- `OK` project-aware Consistency/Resources rule dialogs with cog-wheel entry points, search, alphabetic ordering,
  per-rule active-state persistence, activate-all, descriptions, and default restoration
- `OK` template/material category trees and live template-state browsing in template/resource pickers
- `OK` Blender-style Add Sector draw workflow with shape picker, plane axis, sides/segments, soft cursor guide, and
  non-modal save-selected-sector-as-shape support
- `OK` transactional OBJ/3DO Sector import/export with split/combined convex-sector validation; OBJ import defaults to
  handedness-preserving Y-up-to-Sith-Z-up geometry conversion, converts ordinary normalized OBJ UVs to per-material
  Sith pixel space without transforming their axes, recognizes SED's pixel-UV export marker for exact round trips, and
  leaves native 3DO coordinates and UVs unchanged
- `OK` Surface Extrude preview workflow with temporary geometry, source-surface hide/cut preview, and explicit
  apply/cancel
- `OK` IJIM-only viewport fog toggle plumbing and fog header load/save support
- `OK` active project type marker in the status bar
- `OK` separate `COG links` section in the item editor
- `OK` shared dark UI hierarchy and compact control styling across docks, dialogs, property rows, combo/spin controls,
  tabs, buttons, and scroll bars
- `OK` property color swatches provide continuous live preview, material thumbnails reserve an uncropped area, and
  browse-capable values support value-field double-click
- `OK` Outliner **Things** tab uses a generic placed-object glyph instead of an actor-only stick figure
- `OK` the modern Outliner owns project-aware entity tabs, active/multi-selection synchronization, item properties,
  COG links, template/resource picking, layer state, and generic Thing presentation. It is not required to duplicate the
  old form layout; concrete future defects should be recorded individually rather than as open-ended parity work
- `OK` the native executable, shell, and taskbar use the
  [flat red-circle icon](../src/app/resources/app_flat_red_circle.svg), while Qt title bars and in-app surfaces use the
  borderless [window icon](../src/app/resources/app_window.svg); both maintained families are embedded from checked-in
  PNG/ICO resources
- `OK` cross-platform Qt sound preview and looping sector ambience, including IJIM `INDYWV`, independent editor and
  preview mute state, 500 ms fade/crossfade behavior, and bounded decoded PCM caching

### 7) Performance milestones
- `OK` partial render rebuild caches for level vs things
- `OK` material picker thumbnail loading is deferred and cached instead of loading every thumbnail eagerly
- `OK` resource-picker source/list creation is deferred until after the native dark shell can paint; initial source
  signals are blocked to prevent duplicate full-list population
- `OK` cold raw-3DO rows avoid per-row resource-revision probes, and visible/keepalive discovery is bounded by the
  viewport rather than total model count
- `OK` raw-model/template list rebuilds resolve preview mode once and reuse one implicitly shared placeholder icon
  rather than repeating QSettings and pixmap painting for every row
- `OK` raw-3DO first Show keeps the list hidden behind an opaque dark loading surface, prepares only an empty hidden
  `GlView` host before native mapping, and defers model loading and interactive activation until a valid selection
- `OK` disk-resource origin detection scans each root/kind folder once per revision key and uses set lookups per row
- `OK` picker/template preview caches cover CMP/MAT/SPR/3DO/thumbnails with exact disk/GOB revision keys; rendered 3DO
  keys include the active CMP and every referenced MAT revision
- `OK` complete `3DO` list-thumbnail parsing, hierarchy/bounds preparation, MAT/CMP decoding, and supersampled
  rasterization run in bounded workers
- `OK` cached `3DO` thumbnails are invalidated by live file-watch/revision changes when source models change on disk
- `OK` exact invalidation advances the rendered-image epoch, preventing late worker completion from republishing
  same-size/same-timestamp stale pixels
- `OK` an invalidated worker still wakes its live list owner after retiring the old in-flight key, so classic-project
  resource-generation changes cannot leave visible Thing-placement rows stuck on `Loading`; stale pixels remain barred
  from publication
- `OK` Thing-placement 3DO/SPR producers and consumers share the resolved Level CMP identity, including classic levels
  whose raw Master CMP is empty and inherits the first Sector CMP or `dflt.cmp`
- `OK` mounted GOB/GOO replacement is watched and re-indexed transactionally; delete/atomic-replace/reappearance keeps
  the prior complete index until every replacement mount parses; immutable shared generations keep async prep safe,
  and the dialog creates a new locator generation only when it must remount a changed archive
- `OK` 3DO list thumbnails share the selected preview's hierarchy, UV, material, alpha, and wireframe contracts;
  workers prepare immutable model/material data and final `QImage`s, while the GUI publishes cached icons only after
  active scroll/search and within count- plus time-bounded turns
- `OK` dynamic-buffer churn was reduced across sprites, overlays, gizmos, grid lines, and translucent scratch buffers
- `OK` far-from-origin grid jitter/flicker has been reduced by stabilizing generated grid geometry around the view
- `LIMIT` level and Thing caches update independently, but some geometry mutations still rebuild a wider level mesh
  domain than the exact dirty Sector/Surface set
- `LIMIT` some very small edits still rebuild the full Outliner or property panel; this affects responsiveness, not
  authored-state correctness

### 8) Parity verification
- `OK` checked-in SED V1 golden vectors plus V3/V4 native arch-lighting/history tests: legacy import -> native save,
  native open -> canonical JKL/NDY export,
  four-slot repair, five-slot partial salvage, and every fixed resource category across all three project types
- `TODO` continuous comparisons against normalized output from the original Pascal editor
- `VERIFY` deterministic interaction coverage is extensive but not every possible pointer/window-manager sequence is
  automated; focused Qt tests cover the high-risk tool, recovery, layer, frame, transform, and startup paths
- `OK` focused unit tests for layer handling, frame sync, core transforms, and JKL surface numbering

## Implemented workflows

- Completed the authored-light editing and runtime-preview pass:
  - regular Thing, actor/player Headlight, IJIM Sector point-light, and baked-light owners have distinct gizmos and
    transient live editors;
  - template-inherited Thing lights remain visible, materialize an instance override on edit, and cannot be removed
    until they are overridden;
  - the default **Gouraud (per-vertex)** renderer follows the retail engine sampling and clamp points, while optional
    **Phong (per-pixel diffuse)** uses the pinned OpenJones3D OpenGL smooth falloff without changing saved data or
    baked lighting;
  - runtime collection is view-stable, applies the IJIM/classic source caps only on overflow, and restores source
    traversal order before order-sensitive accumulation.
- Added the persistent **Multi-cut** cleave workflow for Sector and Surface modes. Authored sector cuts are stored as
  world-space planes, remain stable when the camera moves, support line/plane editing, optional chaining, geometry
  snapping, unit/angle guides, optional arbitrary angle-step snapping, transactional Apply/Cancel, and closed-loop
  shape handling while preserving convex sector output. Settled drafts and their local history survive ordinary
  tool/edit-family switches and Preview entry without owning input while inactive. Sector and Surface retain independent
  drafts/options/local histories. Cleave-local G/R/S defaults are configurable in Preferences; Ctrl-wheel edits selected
  lines without changing global transform defaults or zooming the camera. True corner guides distinguish joined angles
  from standalone construction-plane bearings, and centered transform badges use two decimals. Add Shape offers Rectangle,
  N-Gon, Filled arch, and Star with project-aware units, wheel-editable dimensions, Ctrl+Shift-wheel rotation, and a default-off Sector
  Camera plane option for free placement. Clicking a completed line restores its authored orbit,
  projection/FOV/orthographic scale, near/far planes, and clip-enabled flag; edge-on line clicks remain valid and
  within-two-pixel pointer noise reverts instead of moving the plane.
- Reworked Sector properties into optional Sound, Thrust, and Point Light blocks with compact add/remove/copy/paste
  actions, live multi-selection propagation, linked point-light ranges, and a plane-selectable thrust direction wheel.
- Refined the docked Surface UV editor, selection overlays, independent Editor/Preview display settings, and fixed-screen property
  action feedback.

- Consolidated current lighting behavior in [lighting.md](lighting.md), including all mode namespaces, asynchronous and
  automatic bake contracts, runtime equations/GPU layout, known gaps, tests, and commit-pinned OpenJones3D/OpenJKDF2
  source concordance.

- Added startup/start-screen flow before showing the editor, including new-project/open/configure paths.
- Completed the original designer-facing classic Level Header and LEV import choices: classic fields are undoable in
  Level Settings for JKDF2/MotS, and LEV Open prompts for scale, texture handling, and optional matching objects.
- Switched the active Windows build tree to `build/ninja` using `Ninja Multi-Config`; fixed the multi-config
  Release-vs-Debug config bug in CMake.
- Reworked template/material category handling:
  - `data/*.jlh` / `*.jll` override support
  - editable category trees
  - inline create/rename/delete
  - save-back through core JLH/JLL support
- Added list-side category membership editing and ongoing multi-select/drag plumbing in picker/template lists.
- Completed the WAV picker follow-up: nested saved-project categories, stable inset selection without hover artifacts,
  hidden model-only preview controls, safe aligned delete confirmation, and WAV browsing for legacy COG `sound` fields.
- Moved `3DO` list thumbnails to bounded background QImage rasterization and made caches revision-aware for external
  file changes and cross-project reuse
- Added loading keepalive/self-healing for stuck visible thumbnails, smoother scrolling, and Explorer-style tile
  selection cards.
- Reworked resource-picker startup so the native dark loading shell paints first, source/list population runs once on
  the next event-loop turn, disk-origin lookup uses a shared directory index, and thumbnails remain lazy.
- Unified application chrome and property controls under shared dark styles, including compact text/combo/spin fields,
  crisp chevrons, live color swatches, stable material previews, and a generic-object Outliner icon.
- Updated query/find so it behaves like the Pascal `Q_*` dialogs:
  - no entity dropdown
  - current-mode entity
  - irrelevant filters hidden
  - disabled in unsupported modes, including `Light`
- Improved item editor/resource parity:
  - surface material preview fields
  - COG material variable previews
  - base-template hiding in thing-template picking
- Completed the active Pascal consistency/resource checker rule surface: sector/surface structure, flags, normal,
  planarity/convexity, adjoins, Things, typed COG extensions, referenced resources, and project CMP limits. Normal-length
  parity uses the corrected absolute `1e-4` boundary; reverse-adjoin and legacy resource failures are Errors while
  planarity/convexity findings are Info. The shared relevance helper includes COG dirtiness, and the legacy quadratic
  sector-overlap pass is available through a persisted default-off option.
- Fixed JKL local surface numbering and added regression tests for multiple-surface sectors.
- Tightened light editor/light calculation parity:
  - intensity shown for all projects
  - non-negative range/intensity handling
  - thing shading refresh after recalculation
- Added app-close prompts for dirty COG tabs and centralized larger message-box sizing/styling.
- Reworked thumbnail list behavior in picker/template flows:
  - shared visible-range-first thumbnail loading
  - keepalive/self-healing refresh for stuck `Loading` placeholders
  - revision-aware cache invalidation for externally edited `3DO`, `SPR`, `MAT`, and CMP-sensitive previews
  - live watcher invalidation for cached `3DO` thumbnails
- Improved category membership drag/drop:
  - multi-selection payloads use the whole selected set and underlying resource/template names
  - drop assignment no longer changes the current category view
  - assignment is batched to reduce post-drop stalls
  - `Ctrl` moves items out of the current non-`All` category into the target
  - drag cursor/action feedback and thumbnail pixmap were added
  - lingering rectangle/multi-select state cleanup was tightened
  - picker/template writes now use one copy-first JLH/JLL transaction and restore the precise previous graph on no-op,
    save failure, or exception
  - core pair persistence stages/checks both files before replacement, retains recoverable originals through commit,
    and rolls both paths back on reported failure. This is not one OS-wide atomic two-file primitive: power loss can
    interrupt the rename sequence, and best-effort cleanup can leave an obsolete transaction/backup directory
  - focused direct regression cases cover action/payload routing, a synthetic `Ctrl` multi-item drop, Copy/Move,
    case-insensitive deduplication, failed-save and hidden-child rollback, and save/reload
- Tuned viewport grid rendering:
  - orthographic near-parallel dense line buildup is reduced
  - perspective far clipping uses linear reconstructed-ray distance instead of non-linear depth, keeping ordinary
    grid brightness consistent with orthographic projection while retaining the narrow clip and grazing-angle fades
  - far-from-origin grid jitter/flicker was reduced during camera movement
- Fly navigation claims its movement-key shortcut overrides while RMB is held. `Shift+S` therefore accelerates
  backward flight without also activating legacy **Snap Grid to Item**; that command remains `Shift+S` while idle.
- Added transform-grid interaction controls and math:
  - optional hard snapping follows the exact active rendered lattice and adaptive spacing, with free-move depth
    preserved; each grid, sector-alignment, surface-contact/alignment, and Thing-to-Thing snap/rotation option has its
    own exact-match, rebindable hold inverter, with live checked-button feedback that restores on release
  - default-on, disableable plane sticking holds a Thing/frame/light reference point or first geometry support contact,
    then releases smoothly without same-drag recapture
  - thing, frame, light, sector, surface face/edge/vertex, and multi-vertex move paths are covered; multi-vertex commits now
    match preview
  - top-center contextual boolean options use consistent checked-button presentation
- Stabilized docked UV editor behavior:
  - modal transform lifecycle and no-op commits are safer
  - component/multiselect masks and active island rebuilds were tightened
  - Smart UV Project reports failure instead of applying empty results
  - raw texture space remains the default; optional **View Face** uses the active viewport face to derive one shared
    canvas basis, including a handedness mirror and 2D rotation, so texture, grid, every selected island, hit testing,
    and edit directions stay coherent
  - multi-surface selection uses one active-face decision, with the prior orientation retained for behind-camera,
    edge-on, or degenerate projection; the mode rotates or mirrors the canvas as needed but does not foreshorten it
  - panel show/hide now uses one short S-curve transition from a controlled one-pixel start, avoids redundant animation
    restarts/early minimum-size jumps, and rebases viewport compensation before hiding after an unrelated resize
  - configured-grid snapping no longer competes with hard-coded whole-texture candidates; shared-edge Stitch uses the
    authoritative edge as its hinge on angled/banked faces and retains the original disconnected fallback
- Fixed Thing-placement/tool ownership in both directions and baked-light double-click routing through stationary
  overlapping transform handles; simplified light-mode and falloff tooltips are project-neutral.
- Added Blender-style Add Sector drawing:
  - press-drag footprint, release for height, final click to commit
  - `Shift` constrains equal sides/height and `Esc` cancels
  - shape sources include built-in primitives and original SED `data/shapes.tpl` shapes
  - Add Sector options stay visible for shape, start plane, and sides/segments
  - selected sectors can be saved as reusable shapes without opening a blocking dialog
  - idle placement now shows a softened cursor guide with a center cross
- Added a preview-first Surface Extrude workflow:
  - top-center `Length` / `Expand` options
  - live temporary-sector preview
  - explicit apply/cancel and `Esc` cancel
  - negative-expand bounds and reset-after-apply behavior
- Reworked Sector Join into a preview-first workflow with clearer **Join sectors** / **Merge sectors** naming and a
  **Keep overlap pieces** option.
- Tightened cutter-only Sector Join behavior for rotated knife sectors and complex target geometry:
  - removes knife overlap inside target sectors
  - preserves required holes/adjoins
  - reruns candidate portal matching after cleanup
- Unified configurable select/translate/scale/rotate/orientation shortcuts across edit modes and protected focused text
  fields from viewport shortcut capture.
- Improved startup/window behavior:
  - main app stays hidden behind the startup dialog
  - default window starts maximized and persists geometry/state
  - recent files are recorded after successful open, native SED save, and engine export; export does not replace the
    active native SED document path
  - the explicit periodic-snapshot browser is `File > Open Recent > Autosave History`; autosaves stay out of ordinary
    recents
  - startup recents support pin/unpin and project/file-type icons
  - title bar shows `*` for unsaved levels
- Added/refined viewport and project affordances:
  - top-view level refit with extra margin after engine/legacy import or New Project; native SED/recovery preserve the
    serialized camera/editor view
  - eye cursor/icon while fly navigation is active
  - project type status marker
  - IJIM-only fog option plumbing
- Maintained application icon assets:
  - the in-app icon is maintained as [app_window.svg](../src/app/resources/app_window.svg) with checked-in PNG sizes
  - the native shell/taskbar icon is maintained as
    [app_flat_red_circle.svg](../src/app/resources/app_flat_red_circle.svg), its PNG sizes, and a multi-size ICO
  - the checked-in PNG/ICO assets are embedded directly by the build

## Known gaps and deliberate boundaries

### Pascal audit basis

This parity list was rebuilt from the original application's actual entry graph rather than from unit names alone.
Pascal references are plain paths relative to the former Pascal source root, not links, because the C++ repository does
not ship that source tree:

- `sed.DPR` defines the compiled form and support-unit set
- `src/JED_MAIN.PAS` defines the visible main menu, generated keyboard menus, and map commands
- `src/U_PREVIEW.PAS` defines the separate legacy 3D Preview commands
- `src/U_TOOLS.PAS` defines numeric transforms, the door helper, and the two-surface angle helper
- `src/U_LHEADER.PAS` defines the complete designer-editable Level Header surface
- `src/U_DFI.PAS` defines the live LEV import choices
- `src/U_3DOFORM.PAS` defines level 3DO-hierarchy authoring

A routine is counted only when it is reachable from a visible menu item, active form control, or live keyboard command.
The C++ side was checked against [src/core](../src/core), [src/app](../src/app), the maintained
[core tests](../src/core/tests), and [application tests](../src/app/tests). A different modern UI is treated as parity
when it preserves the useful designer workflow and authored result.

### Verified live Pascal gaps

| Original capability | Live Pascal entry point | Current C++ state |
| --- | --- | --- |
| DLL editor plugins | `LoadPlugins`, the visible Plugins menu, and Plugin Developer Mode in `src/JED_MAIN.PAS` / `src/U_OPTIONS.pas` | No source-compatible DLL loader, Plugins menu, or plugin developer mode. Any replacement needs a safe extension contract around the C++ document model, undo transactions, ownership, threading, and security boundaries. |
| Revert and backup commands | **Revert to Saved**, **Make a Backup Copy**, and automatic `.bak` handling in `src/JED_MAIN.PAS` | Autosave, recovery, native SED history, and recovery browsing exist. The two manual commands and automatic backup of the previous on-disk level before Save overwrites it are absent. Atomic replacement protects Save itself but does not preserve a separate `.bak`. |
| Paste in Place and selection duplication | `Ctrl+Shift+V` plus Insert-key duplication in `src/JED_MAIN.PAS` | Validated same-process/cross-process copy/paste exists for Sectors, Things, and baked lights. It places the clipboard anchor at the pointer/camera target; no Paste in Place action exists. Add tools do not duplicate the selected Sector/Thing/light set with its authored properties as the original Insert path did. |
| Keyboard step movement | `RaiseObject` in `src/JED_MAIN.PAS` nudges the current selection along the grid normal; `src/U_PREVIEW.PAS` also gives Things fixed-step directional movement | Move gizmos, exact numeric Translate, grid snapping, and surface-aware Thing placement are implemented, but equivalent rebindable nudge actions are absent. |
| Build Sector / Build Surface recovery | `Alt+B` dispatches `BuildSector` from selected faces or `BuildSurface` from selected vertices in `src/JED_MAIN.PAS` | Add Sector, Extrude, Cleave, Join, Merge, and import exist, but there is no equivalent command for reconstructing missing topology from an existing component selection. |
| Planarize Surface command | `Ctrl+P` calls `FlattenSurface` in `src/JED_MAIN.PAS` | The tested core operation exists as [`levelFlattenSurface`](../src/core/src/level/geometry.cpp), but no application action invokes it. |
| One-click flag repairs | **Fix solid surface(s)** and **Fix passable adjoin(s)** are generated Recovery-menu commands in `src/JED_MAIN.PAS` | Consistency checks report the relevant problems and Properties can edit flags manually, but the original batch repair commands are absent. |
| Make Door assistant | **Make Door** in `src/U_TOOLS.PAS` calls `TJedMain.MakeDoor` | No command converts a selected Thing into a two-frame door and creates/populates its `00_door.cog` instance. Frame and placed-COG editing are otherwise implemented. |
| Two-surface angle/grid assistant | **Calculate Angle** in `src/U_TOOLS.PAS` measures two selected surfaces and moves/orients the grid to their intersection | Grid-to-view/selection, exact grid transforms, alignment snapping, and Multi-cut angle guides exist, but no single command reproduces this measured two-surface setup. |
| Grid-relative exact transforms | The Tool Window in `src/U_TOOLS.PAS` can Rotate and Flip around the authored grid plane and Scale along its normal | Exact Translate/Rotate/Scale/Mirror commands and viewport gizmos exist, but their equivalent numeric grid-plane pivot/axis choices are absent. Grid snapping and F2/F3 view/grid alignment do not perform that same transform. |
| Saved-shape deletion | **Delete** on the Shapes page of Map Settings in `src/U_TBAR.PAS` removes a custom shape from `shapes.tpl` | Add Sector reads saved shapes and can append the selected Sector as a new shape, but there is no command to delete a saved custom shape. |
| Current-document project conversion | Double-clicking the project marker cycles JKDF2, MotS, and IJIM in `src/JED_MAIN.PAS` | New projects support all three targets, but an already-open document cannot be converted in place. A modern implementation would need explicit lossy-field/resource warnings rather than silently cycling the type. |
| Level 3DO hierarchy editor | `src/U_3DOFORM.PAS` adds/deletes/reparents nodes, edits node offsets/type, fills missing nodes, and loads/saves hierarchy data | 3DO loading, previews, and transactional Sector import/export exist. General hierarchy-node authoring does not. The Pascal application does not offer arbitrary 3DO mesh editing, so direct mesh authoring is not a legacy gap. |
| Project packaging and launch-to-test | GOB Project, Save JKL and GOB, and Save JKL and Test in `src/JED_MAIN.PAS`, with container access in `src/CONTAINERS.PAS` | Runtime archive mounting/discovery exists, but JKDF2/MotS GOB/GOO project creation, container browsing, and game launch are absent. The Pascal IJIM package branch says `TODO`, so IJIM package creation is excluded from the parity claim. |
| Preview diagnostic overlays | **Highlight Solid Floor**, **Highlight Solid Surface**, and **Highlight Surfaces With Special Properties** in `src/U_PREVIEW.PAS` | Selection overlays, X-Ray, render modes, and consistency results exist, but there are no equivalent whole-level preview overlays for these face-property classes. |
| Preview gamma and global map backface toggle | `+` / `-` preview gamma and **Disable backface culling** in `src/U_PREVIEW.PAS` / `src/JED_MAIN.PAS` | The current renderer has no user gamma control or global viewport backface-culling switch. Per-face double-sided flags and selection-through controls are separate behavior. |
| Camera-driven authoring helpers | Numeric camera editing in `src/U_TOOLS.PAS`, plus **Set Thing To Camera**, **Copy camera as frame**, and **Insert thing at camera** in `src/U_PREVIEW.PAS` | Camera navigation, framing, recovery, surface-aware Thing placement, and numeric grid transforms are implemented. Exact camera pose fields and these camera-to-Thing/frame transfer commands are not exposed. The old map-camera/preview-camera synchronization commands are obsolete because C++ SED uses one viewport. |

### Forward-looking work that is not Pascal parity

- LEV save/export remains unimplemented C++ format work; the live Pascal application is also import-only
- ASC loading is implemented, but **Save As ASC** / **Export As ASC** is not; this is forward-looking work because the
  live Pascal application is also import-only
- continuous normalized-output comparison against the original remains a verification improvement rather than a
  missing designer command
- editor scripting/macros and LLM/MCP integration are new automation work, separate from existing COG script
  authoring. The uncalled Pascal toolbar-script loader remains excluded from live parity; a modern plugin API would
  instead provide a replacement for the live DLL-plugin workflow listed above

### Audited replacements and exclusions

These live Pascal workflows have intentional modern replacements and are not gaps:

- the separate editable 3D Preview is replaced by WYSIWYG editing in the main textured viewport plus a read-only Game
  Preview mode; commands used only to synchronize the two old cameras are no longer applicable
- Next/Previous, Jump to Object, and Goto Adjoin are covered by Outliner navigation, Find/Find Next, repeat-click cycle
  picking, history/check **Go To**, and the selectable mirror-surface property
- manual Reload Templates is replaced by revision-aware resource discovery, file watching, and cache invalidation
- legacy texture shift/rotate/straighten/stitch commands are covered by the docked UV editor, Auto Align, Stitch,
  arbitrary transforms, snapping, and undoable multi-island editing; selecting all Surfaces and running Auto Align
  covers the old **Straighten All textures** button
- the old independent grid-line and grid-dot interval controls are replaced by the adaptive grid, with optional minor
  points and major lines; the authored grid size remains the exact base lattice used by transforms and snapping
- the old map-camera zoom-lock minimum/maximum fields are replaced by bounded orbit, perspective, and orthographic
  camera behavior; they are renderer safeguards rather than authored level data
- the old choice between software, Direct3D, and OpenGL backends is an implementation detail; the maintained renderer is
  the Qt/OpenGL WYSIWYG path
- the original option to disable Undo is intentionally omitted; transactional Undo/Redo and native-history recovery are
  always part of safe editing
- Episode/Campaign editing, startup COG generation, goals, and COG strings are implemented in
  [episode_editor.cpp](../src/app/src/ui/episode/episode_editor.cpp)

The audit excludes non-live source rather than treating it as missing:

- hidden Cutscene Helper, Topics, and JED Tutor commands
- the commented-out Whip Ruler shortcut and commented debug/plugin commands
- compiled but uncalled toolbar-script loading in `src/TBAR_TOOLS.PAS`
- unlinked `src/FILES_OLD.PAS`, `src/JED_PLUGINS2_.PAS`, `src/JED_PLUGINS3.PAS`, `src/JED_OLE.PAS`,
  `src/GL3DFX.PAS`, and `src/MEMCHECK.PAS`
- obsolete renderer plumbing with no independent level-designer behavior

### Deliberate or verified boundaries

These are not open-ended parity defects:

- new projects retain Pascal's tested `2 x 2 x 1` starter-room bounds for every project. JKDF2 and MotS also retain
  classic face anchors and `RecalcAll` UV initialization; IJIM keeps its project-specific face order and
  `RecalcNewAll` initialization. C++ uses a project-aware player, resolved default CMP/material, centered Baked Light,
  and top-down camera instead of a blank `MasterCMP` and `walkplayer` at `z = 0.1` for every project
- selected interactive 3DO previews remain OpenGL/GUI-thread widgets by Qt ownership; list-thumbnail parsing, material
  decoding, hierarchy preparation, and software rasterization run in bounded workers
- type-0 Game Preview follows
  [`rdSprite_Draw`](https://github.com/smlu/OpenJones3D/blob/b9c0eaa56bb51307b0787add4b614d9468e5d372/Libs/rdroid/Primitives/rdSprite.c#L158-L321);
  representative near-pole type-2 texture orientation remains a visual verification boundary
- stable far-to-near translucency is implemented, but intersecting or cyclic transparent geometry has no OIT solution
- raw UV texture space is the default; **View Face** derives the active face's viewport basis, applying the needed
  handedness mirror and 2D rotation to the whole canvas. It does not foreshorten the canvas to imitate an arbitrary
  viewport pose
- CMP palette colors and all 64 classic light-level translation tables are decoded; JKDF2 Solid rendering resolves the
  MAT palette index through the selected light row, while applying a non-identity active Sector CMP translation to
  textured pixels in every renderer/preview remains incomplete

Rerun the full Release suite after significant renderer, geometry, serialization, or Qt ownership changes.

## Suggested next steps

1. Restore the small high-value editing helpers: Paste in Place, selection duplication, keyboard nudge, Planarize,
   Build Surface/Sector, flag repairs, saved-shape deletion, Make Door, and camera/frame transfer
2. Design modern hierarchy authoring, extension, packaging, archive-browsing, and launch-to-test workflows without
   importing unsafe legacy ownership assumptions
3. Continue forward-looking save-format work and visual sprite/UV stress verification
