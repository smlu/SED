# Architecture

This document explains SED's runtime structure so contributors can navigate the codebase safely and understand where
modernized workflows deliberately differ from the original Delphi UI while preserving relevant Sith/JED behavior.

Related docs:
- Porting checklist / module map: `docs/porting_guide.md`
- Algorithms: `docs/algorithms.md`
- Canonical lighting reference: `docs/lighting.md`
- Current project state: `docs/project_context.md`
- Persistence inventory and exclusions: [editor_state.md](editor_state.md)
- Native/recovery format contracts: `docs/formats/sed_binary_format.md` and
  `docs/formats/recovery_format.md`

## Contents

- [Repository layout](#repository-layout)
- [Core library (`sed_core`)](#core-library-sed_core)
  - [Level model](#level-model)
  - [UV coordinate ownership and format boundaries](#uv-coordinate-ownership-and-format-boundaries)
  - [Values, references, categories, and COG defaults](#values-references-categories-and-cog-defaults)
  - [Document and undo](#document-and-undo)
  - [Viewport entity clipboard](#viewport-entity-clipboard)
  - [Episode and campaign editor](#episodecampaign-editor)
  - [Editing operations](#editing-operations)
  - [Lighting ownership and pipelines](#lighting-ownership-and-pipelines)
    - [Interactive MotS ARCHLIGHTING submode](#interactive-mots-archlighting-submode)
  - [Asset loading and resource location](#asset-loading-and-resource-location)
- [Application (`sed_app`)](#app-sed_app)
  - [Startup flow](#startup-flow)
  - [Add Sector workflow](#add-sector-workflow)
  - [Join and merge workflows](#join--merge-editing-workflows)
  - [Surface extrude workflow](#surface-extrude-preview-workflow)
  - [`MainWindow` responsibilities](#mainwindow-responsibilities)
  - [Resource discovery and pickers](#resource-discovery-categories-and-pickers)
  - [Audio playback and sector ambience](#audio-playback-and-sector-ambience)
  - [Shared styling and property controls](#shared-desktop-styling-and-property-controls)
  - [Preferences, keymap, query, and history](#preferences-keymap-queryfind-and-history)
  - [Autosave and crash recovery](#autosave-and-crash-recovery)
  - [Consistency and resource checking](#consistency-and-resource-checking)
  - [COG authoring](#docked-cog-authoring)
  - [Resource authoring](#docked-resource-authoring)
  - [UV editing](#docked-uv-editing)
  - [`GlView` responsibilities](#glview-responsibilities)
  - [Sector property ownership](#sector-property-ownership)
  - [Branding and icon assets](#branding-and-icon-assets)

## Repository layout

- `src/core/` (portable library)
  - level data model (`sed::Level`, `sed::Sector`, `sed::Surface`, `sed::Thing`, `sed::SedLight`, `sed::Cog`)
  - editor document model (`sed::editor::DocumentState`, selection, snapshot undo)
  - editing operations (cleave/connect/merge/extrude/transforms/lighting)
  - asset parsers and resource locator (`JKL`, `JED`, `LEV`, `ASC`, `3DO`, `SPR`, `MAT`, `TPL`, `COG`, `JLH/JLL`)
  - tests in `src/core/tests`
- `src/app/` (Qt desktop app)
  - `MainWindow` (UI shell, document owner, startup flow, menus, properties, preferences, history, resource/category
    workflows, query/find)
  - `GlView` (OpenGL viewport renderer, picking, overlays, gizmos, navigation, preview capture)
  - reusable UI widgets in `src/app/src/ui/`
  - `MainWindow` feature modules split under `src/app/src/main_window/`
  - rendering implementation split into `src/app/src/render/`

Boundary:
- `sed_core`: no Qt, no OpenGL; deterministic logic intended to stay testable
- `sed_app`: Qt/OpenGL presentation and input plumbing; calls into `sed_core` for edits

## Core library (`sed_core`)

### Level model

Primary header: `src/core/include/sed/level.hpp`

Key types:
- `sed::Level`
  - `sectors`, `things`, `lights`, `cogs`, `templates`, `materials`, `colormaps`, `layers`
  - `header` (`LevelHeader`) and per-project options (`ProjectType`)
- `sed::Sector`
  - `vertices`, `surfaces`, ambient/extraLight/tint, sound fields, thrust, `layer`
- `sed::Surface`
  - polygon vertex indices, texture vertices, geometry/lighting/texture mode, flags, adjoin/mirror references,
    adjoin flags, UV basis/scale fields, `num`
- `sed::Thing`
  - position, `pch/yaw/rol`, `templateName`, `name`, `sector`, `layer`, `values`
- `sed::SedLight`
  - position, range/intensity/color, flags, `layer`
- `sed::Cog` + `sed::CogValue`

Level entity IDs use `curId` as the next candidate. `INT32_MAX` is reserved as the exhausted sentinel, so
`INT32_MAX - 1` is the final assignable ID. `Level::canAllocateIds(count)` uses constant-time wide arithmetic to
preflight known batches; exact remaining capacity succeeds, while exhaustion rejects before publication. Core factories,
clipboard, app placement, COG, and light operations use this check. Cleave/Join perform checked incremental allocation
inside a private candidate rather than reserving all split IDs before its Level copy; a later shortage discards that
candidate and preserves the source Level.

Layer helpers are implemented directly on `Level`:
- `getLayerName`
- `addLayer`
- `addMissingLayers`

### UV coordinate ownership and format boundaries

[`Surface::texVertices`](../src/core/include/sed/level.hpp) is the authoritative editable level UV representation for
every project type. A live `Level` is canonical in normalized texture space and records that contract explicitly in
`Level::uvCoordinateSpace`. Signed and multi-tile coordinates remain valid: normalized does not mean clamped to
`0..1`. `Surface::uScale` and `vScale` use the same coordinate space as the surface UVs, so conversion divides or
multiplies the scale by the corresponding texture width or height as one transaction.

[`Model3doTexVertex`](../src/core/include/sed/assets/3do.hpp) remains separate and retains the representation authored by
the 3DO version until a model-to-Level or rendering boundary performs an explicit conversion. The current contract is:

| Owner or boundary | Wire/input coordinates | Published/retained coordinates | Conversion owner and regression |
|---|---|---|---|
| Live `Level` | Normalized | Normalized, explicitly tagged | [`level.hpp`](../src/core/include/sed/level.hpp), [`uv.cpp`](../src/core/src/level/uv.cpp), and [`level_uv_tests.cpp`](../src/core/tests/level/level_uv_tests.cpp) |
| Native `.sed` V2/V3 | Normalized | Normalized, bit-exact through Save/Open; V3 also retains the MotS ARCHLIGHTING table field and optional authored bake provenance | [`sed.cpp`](../src/core/src/assets/sed.cpp) and [`sed_level_tests.cpp`](../src/core/tests/assets/sed_level_tests.cpp) |
| Native `.sed` V1/V4 | Legacy texture pixels | Raw codec/direct-open result is explicitly tagged pixel space; V4 also retains the MotS ARCHLIGHTING table field and optional authored bake provenance, and MainWindow then applies the configured complete-document policy | [`sed.cpp`](../src/core/src/assets/sed.cpp) preserves both profiles exactly; [`uv_promotion.cpp`](../src/core/src/editor/uv_promotion.cpp) converts the current Level and all Undo/Redo states atomically only after real MAT dimensions resolve |
| JKDF2/MotS JKL | Texture pixels | Normalized when every material-bearing surface's referenced MAT dimension resolves; no-material surfaces follow the explicit exception below | [`jkl.cpp`](../src/core/src/assets/jkl.cpp) and [`jkl_save_tests.cpp`](../src/core/tests/assets/jkl_save_tests.cpp) |
| IJIM NDY | Normalized | Normalized | [`jkl.cpp`](../src/core/src/assets/jkl.cpp) and [`jkl_save_tests.cpp`](../src/core/tests/assets/jkl_save_tests.cpp) |
| Legacy JED, ASC, and LEV | Texture pixels | Normalized when resource-backed import resolves every referenced MAT dimension | [`jed.cpp`](../src/core/src/assets/jed.cpp), [`asc.cpp`](../src/core/src/assets/asc.cpp), [`lev.cpp`](../src/core/src/assets/lev.cpp), and their focused asset tests |
| 3DO 2.1 | Texture pixels | Version-native model pixels; Sector import/export converts at the Level boundary | [`3do.cpp`](../src/core/src/assets/3do.cpp), [`model3do_tests.cpp`](../src/core/tests/assets/model3do_tests.cpp), and [`sector_import_utils.cpp`](../src/app/src/ui/sector/sector_import_utils.cpp) |
| 3DO 2.3 | Normalized | Version-native model normalized values; Sector import/export remains normalized | The parser does no hidden scaling and model-version tests pin nontrivial signed/multi-tile values |
| External OBJ | Normalized by default | Normalized | A legacy SED pixel-coordinate marker is admitted only through real per-face MAT dimensions; implementation and coverage are in [`sector_import_utils.cpp`](../src/app/src/ui/sector/sector_import_utils.cpp) and [`thing_utility_tests.cpp`](../src/app/tests/thing_utility_tests.cpp) |

Classic pixel-to-normalized admission is deliberately failure-atomic. The converter in
[`uv.cpp`](../src/core/src/level/uv.cpp) stages every affected coordinate and `uScale`/`vScale`, resolves each
material-bearing surface's actual first-cel MAT dimensions through [`material_dimensions.cpp`](../src/core/src/assets/material_dimensions.cpp), and
publishes only when all required surfaces succeed. It never substitutes a fake `1x1` image for a missing material
dimension. A surface explicitly carrying no material is the format exception: generic conversion validates its finite
coordinates, scales, and corner counts but preserves its authored UV values, while JKL import gives all-zero values
the zero mapping and gives nonzero values a fresh geometry-proportional normalized island (or finite zero/default
values for degenerate geometry), retaining corner colors. Export passes those material-less values through unchanged.
A JKL import that cannot prove dimensions for a material-bearing surface therefore remains explicitly tagged
`TexturePixels` with a diagnostic that names the missing MAT file, or an operation that requires canonical coordinates
rejects without partially modifying the Level. A JKL no-resource or failed-resource load retains the original pixel data.
JKL project-kind detection is resource-independent; after complete decoding, the application stages the detected
JKDF2 or MotS locator before restoring typed COG schemas/values and running this conversion, so a successful same-name
COG or MAT lookup from a provisional locator cannot use the wrong game's schema or dimensions. If a COG script is
unavailable, its raw wire arguments remain unchanged.
When delayed resource availability retries a generic promotion, it converts only the material-bearing surfaces whose
dimensions are now proven and carries no-material UV values forward unchanged.

Direct native `.sed` validation deliberately ends at that raw compatibility boundary. MainWindow then reads the
persisted legacy-pixel admission policy after staging the candidate's resource locator. Automatic mode (the default)
normalizes
the current Level and all native HIST transitions together; Ask mode offers Normalize, Keep Pixel Space, or a
failure-atomic Cancel; compatibility mode keeps the whole document in pixels. One unresolved MAT in either the current
or a historical state prevents all conversion. A normalized workspace opened from legacy pixel space separately retains
the source's legacy identity, so Save requires an explicit normalized V2/V3 upgrade or writes a genuinely pixel-space
Level-plus-HIST copy when
the designer chooses Keep legacy pixel space. The raw codec never performs this UI policy.

Pixel-space compatibility is visible through an amber status-bar marker. Resource-file, mounted-archive, and resource
configuration generations trigger an asynchronous re-evaluation from immutable Level, compact-history, and locator
snapshots. Publication rechecks the document epoch, exact Undo/Redo signatures, selection, resource and locator
generations, and persisted policy. Thus delayed MAT availability can promote current state plus history atomically
without blocking the GUI or accepting stale dimensions; repeated notifications for the same missing MAT are reduced to
a single dependency probe.

JKL and NDY do not serialize `Surface::uScale` or `vScale`. Their loader reconstructs
those editor fields from a stable non-degenerate geometry/UV triangle, including valid
sub-centimetre faces below the preferred basis size. Finite dormant legacy surfaces
with repeated corners may have no such basis; compatibility keeps the safe default
scales, matching the Pascal loader, rather than rejecting the complete Level. UV tokens
that are non-finite or outside the supported float range still fail at the parse boundary.

Level rendering consults the explicit `Level::uvCoordinateSpace` tag. Model rendering consults the parsed 3DO model
version, not the project type, so JKDF2/MotS 2.1 pixels and IJIM 2.3 normalized values sample equivalently without
changing the authored model data. Both paths are implemented in
[`gl_view_mesh.cpp`](../src/app/src/render/gl_view_mesh.cpp).

Geometry algorithms operate in the Level's declared space and do not perform hidden format conversion. Merge and
extrude preserve existing UVs and scales. Move/Rotate/Scale either retain them unchanged or use the explicit
connected-surface plan described below in that same space; cleave preserves surviving corners and interpolates only new
edge intersections. The low-level primitive helpers retain their legacy pixel-space construction formulas because those
formulas match the original mapping. Application construction boundaries convert a complete generated Sector with its
real material dimensions before publishing it into a normalized Level. This rule prevents a newly added or starter
Sector from carrying coordinates whose units disagree with the owning Level.

The format, transform, cleave, extrude, merge, starter-room, Add Sector, renderer, UV-editor, Undo/Redo, history,
recovery, and cross-process clipboard matrices are covered by
[`level_uv_tests.cpp`](../src/core/tests/level/level_uv_tests.cpp),
[`level_transform_tests.cpp`](../src/core/tests/level/level_transform_tests.cpp),
[`level_cleave_tests.cpp`](../src/core/tests/level/level_cleave_tests.cpp),
[`level_extrude_tests.cpp`](../src/core/tests/level/level_extrude_tests.cpp),
[`level_merge_tests.cpp`](../src/core/tests/level/level_merge_tests.cpp),
[`level_clipboard_tests.cpp`](../src/core/tests/level/level_clipboard_tests.cpp), and
[`main_window_event_filter_tests.cpp`](../src/app/tests/main_window_event_filter_tests.cpp).

### Values, references, categories, and COG defaults

Files:
- `src/core/include/sed/values.hpp`
- `src/core/src/values.cpp`
- `src/core/src/assets/cog_script.cpp`
- `src/core/src/assets/jed.cpp`
- `src/core/src/assets/jkl.cpp`
- `src/core/include/sed/assets/jlh.hpp`
- `src/core/src/assets/jlh.cpp`

Important details:
- `TplValue::tryGetFrame(...)` parses frame values without silent `0,0,0` fallback.
- `TplValue::setFrame(...)` writes back canonical text.
- JED-facing thing/sector/surface reference formatting is used more consistently than raw internal indices.
- JKL load now sets `surface.num` correctly and tests cover multi-surface sectors.
- JED `surface` COG values are reassembled correctly from `sector surface` token pairs on load.
- public placed-COG symbols include `cog`, `sector`, `surface`, and `thing` entity references; an absent default for any
  of them is `-1`, scalar `int`/`flex`/`float` values start at zero, and vector declarations have no initializer.
  The engine parser zeroes vector storage rather than using a declaration initializer. Placed vectors start at zero and
  use `(0/0/0)` in the level representation; COG runtime code uses single-quoted, space-separated literals such as
  `'1 2 3'`.
- Absent `ai`, `key`, `material`, `model`, `sound`, or `template` resources remain empty until authored. Explicit
  resource names, including `sound`/WAV names, remain valid; empty resource values use the engine's `-1` token when
  exported. Placed vectors are emitted as one canonical `(x/y/z)` token, and a placed value that cannot be represented
  as one engine token is rejected before export.
- local symbols and message declarations are excluded from placed-COG argument lists. Thing deletion, vertex deletion
  with paired Surface text, and noncanonical JKL `SURFACES` spans remap references by semantic target before publishing.
  A reordered global-surface import uses a wire-span map to hydrate typed values. `importedJklCogRowsPresent`
  and `importedJklSurfaceRefs` retain pending provenance in the optional core-owned `jimp` V1 section when meaningful;
  optional HIST `jklCogImportV1` carries the explicit current state and sparse metadata for affected undo/redo states.
  A false marker with an unused map is normalized away, leaving ordinary healthy native bytes unchanged. The eight
  required Level schemas and `HistoryNested` profile remain unchanged. Missing or ambiguous targets/schemas fail closed
  before publication or save; identity-order input may use its raw row-index fallback.
- JLH/JLL load/save is now a first-class core path so picker/template category editing is not a UI-only ad hoc format.

### Document and undo

Primary headers:
- `src/core/include/sed/editor/document.hpp`
- `src/core/include/sed/editor/undo.hpp`
- `src/core/include/sed/editor/selection.hpp`

Runtime model:
- `DocumentState` holds `Level level` and `Selection selection`.
- `DocumentUndoHistory` owns full and compact entity snapshots with copy-on-write payloads;
  `Document::applyUndoable(label, dirtyFlags, fn)` records the affected domains so the UI can prefer partial refreshes
  over full rebuilds.
- Multi-cut draft history is app-owned by `GlView`, separate from this authored Level timeline. Completed line edits
  use bounded line-only snapshots rather than `Document` snapshots; successful Apply creates one normal document
  transaction with a pre-Apply workspace memento and ends the active draft session. EDIT retains local history and
  checked HIST-bound Apply/Layer attachments for native Save and recovery.
- Undo/Redo prepares copy-on-write payloads, destination records, and output buffers before changing the current
  document. Compact targets are validated together; failed allocation or invalid indices preserve both history stacks,
  selection, dirty outputs, and record identity. Selection uses member swaps, and immutable navigation metadata keeps
  history-cap trimming non-throwing on both MSVC and portable standard libraries.
- Recovery-history import builds a private history before swapping it into place. Native Open, recovery application,
  and whole-document legacy UV promotion prepare the complete incoming Document before their final authorization and
  teardown boundary, so failed import preparation cannot destroy the live workspace.
- Frame selections store Thing/value indices, while `numframes` synchronization may move values. Frame Add resolves
  the appended value after synchronization and commits its selection inside the same undo transaction. Properties
  Frame Add/Duplicate/Delete remap surviving selections by logical frame ordinal only within each affected Thing.
  Removing ordinary parameters or restoring template overrides uses the same owner-local remapping when earlier
  values shift frame indices.
  Native Save validates live EDIT first, then maps selected and active frames into its normalized candidate Level and
  copies that mapped selection into the HIST snapshot; the live document remains unchanged.
- Undo/Redo probes Frame owners from the current selection and the transition's before/after selection metadata,
  plus a singleton selected Thing. It captures counts and current selected-frame ordinals before applying history,
  without copying the Level or retaining borrowed model references. Changed counts or stored frame indices use the
  restored snapshot's FrameRefs, including when Delete emptied the current frame selection. A pose-only edit with
  stable frame storage retains the user's current selection or deselection.
- Frame Add builds a compact visual-owner proxy from the selected Thing and caches it by document signature and resource
  identity; the proxy removes repeated Frame values but keeps local overrides, inherited resolution, and layer. Hover and
  click share the resolved pose, while camera, option, and modifier changes refresh the preview without writing authored
  state.
- Frame Add, translation, rotation, and snap transform use owner-subset history and targeted Frame invalidation. Frame
  Move captures one virtual mover/support snapshot and lazily caches compact stationary-anchor poses with repeated
  Frame values stripped, preserving each authored transform and local visual overrides. The owning Thing stays in the
  stationary table as a valid snap target; pointer samples reuse these proxies instead of copying long Frame payloads.
  Targeted Frame publication invalidates the addressed pose/ghost records and Thing-picking peel cache without
  rebuilding the ordinary Thing geometry cache.
- `GlView` keeps a bounded pose cache per Thing plus a bounded topology cache keyed by the resolved immutable 3DO
  object. Non-active model ghosts share one validated, per-mesh-deduplicated local line buffer and use persistent
  per-owner instance buffers; model bounds are culled against six clip planes plus the positive-`w` guard before each
  instanced draw. Camera motion therefore checks Frame instances without reparsing values, visiting source edges,
  resolving resources, or uploading unchanged transforms. Pose publication invalidates only addressed owners, while
  Level/template/resource publication clears stale identities. Context teardown releases GPU state and retains CPU
  caches. The overlay helpers are `render/overlays/frame_pose_cache_utils.*`, `frame_ghost_topology.*`,
  `frame_render_cache.cpp`, and `frame_ghost_render.cpp`. Limits are 64 owner entries / 32 MiB for parsed poses and
  instance matrices, eight model entries / 64 MiB for topology and expanded endpoints, and 64 MiB for ghost GPU
  buffers. Each model is capped at 512K unique edges, a 16 MiB endpoint buffer, and 8M source-edge visits. An oversized
  path uses its cached prefix plus an indexed selected-pose lookup, and an over-budget model topology is negatively
  cached with model ghosts omitted while the path and selected full model remain available.

### Viewport entity clipboard

Primary files:
- typed value payload and remap core: [clipboard.hpp](../src/core/include/sed/level/clipboard.hpp) and
  [clipboard.cpp](../src/core/src/level/clipboard.cpp)
- value-owned MotS ARCHLIGHTING snapshot, bounded codec, and paste core:
  [arch_lighting_clipboard.hpp](../src/core/include/sed/level/arch_lighting_clipboard.hpp) and
  [arch_lighting_clipboard.cpp](../src/core/src/level/arch_lighting_clipboard.cpp)
- Edit actions and viewport-targeted commit: [clipboard.cpp](../src/app/src/main_window/clipboard.cpp)
- MotS ARCHLIGHTING clipboard actions and vertex workflow: [arch_lighting.cpp](../src/app/src/main_window/arch_lighting.cpp)
- rebindable viewport dispatch: [shortcuts.cpp](../src/app/src/main_window/shortcuts.cpp)
- protected container definition: [sed.hpp](../src/core/include/sed/assets/sed.hpp),
  [sed.cpp](../src/core/src/assets/sed.cpp), and the
  [SED V1 binary contract](formats/sed_binary_format.md)
- core codec/remap coverage: [level_clipboard_tests.cpp](../src/core/tests/level/level_clipboard_tests.cpp)
- ARCHLIGHTING codec/paste coverage:
  [arch_lighting_clipboard_tests.cpp](../src/core/tests/level/arch_lighting_clipboard_tests.cpp)
- application and cross-window coverage:
  [main_window_event_filter_tests.cpp](../src/app/tests/main_window_event_filter_tests.cpp)

Ownership and behavior:
- for entity families, the application clipboard owns one typed, self-contained value payload for Sectors, Things, or
  baked editor lights; it contains no `QObject`, vector-element pointer, or selection reference and therefore remains
  safe across document replacement and vector growth. Entity Copy also publishes a protected native SED payload under
  its private system MIME type, so another running SED instance can paste it. Entity Decode is bounded and validates
  container parity/checksums, payload family, layer metadata, finite anchor data, entity counts, and trailing bytes
  before document mutation;
- Sector copy stores only adjoins whose destination is also copied and converts their Sector component to payload-local
  indices. Paste appends fresh identities, remaps those internal adjoins, restores severed external portals as solid
  boundaries, translates
  vertices plus authored collision bounds and the owned IJIM point light, and preserves material/UV metadata;
- Thing paste preserves template/value/frame/bounds/orientation data, translates well-formed frame positions with the
  owner, then recomputes containing Sector. Layers for every family are resolved by stable name and imported only when
  the destination lacks that name;
- normal `Ctrl+C`/`Ctrl+V` is viewport-only, configurable through the existing keymap, and leaves native text-widget
  copy/paste untouched. Light mode routes Thing and Sector light categories to their owning entity payload and Baked
  to `SedLight`. Paste targets the world point under the viewport pointer (camera target fallback), publishes one
  undo transaction, switches to the matching edit mode, and selects every newly appended entity with the newest active;
- the core accepts a null target for a future Paste-in-Place command, but no separate UI binding is exposed yet.

The clipboard family follows the owning Level containers rather than every edit submode. Surface, Edge, and Vertex
selections therefore cannot be copied independently: copy the owning Sector when that geometry must move between SED
instances. Thing-light and Sector-light submodes similarly copy their owning Thing or Sector; only baked editor lights
have an independent light payload. The private system format is
`application/x-sed-entities-v1`; its ordinary text summary is informational and Paste never reconstructs
entities from that text.

MotS ARCHLIGHTING uses a separate value-owned clipboard family. It snapshots one validated record, normalized model
identity, and all-LOD geoset mesh/vertex prefixes, so the source `Level`, selected Thing, and resource cache can be
replaced immediately after Copy. The application publishes it as
`application/x-sed-mots-archlighting-v1`; this payload is a bounded version-1 core codec, not a protected SED
`ProtectedV1` container. It has no SED section parity, CRC32C, MDS parity, or SHA-256 claim, and the protected-format
checks below apply only to `application/x-sed-entities-v1`.

#### MotS ARCHLIGHTING system clipboard representation

Copy publishes one `QMimeData` object with the following representations:

| MIME representation | Contents | Paste behavior |
| --- | --- | --- |
| `application/x-sed-mots-archlighting-v1` | Bytes returned by `encodeMotsArchLightingClipboard(...)` | The only representation accepted by ARCHLIGHTING Paste |
| `text/plain` | An informational `SED: ARCHLIGHTING <model identity>` summary | Never parsed to reconstruct authored values |

The version-1 binary begins with the eight-byte `SEDARCH\0` signature and a little-endian `u32` version. It then
stores the normalized model identity, geoset count and each geoset's mesh-prefix vertex counts, followed by the
record wire index, mesh wire indices, vertex counts, and four IEEE-754 `float` bit patterns per authored vertex.
Identity is canonicalized by trimming, changing backslashes to forward slashes, and lowercasing ASCII letters. The
topology prefixes cover every LOD and must match the destination model topology exactly.

The codec rejects malformed, non-finite, truncated, trailing, or over-budget data before paste begins. Identity is
limited to 4,096 bytes, aggregate geosets/mesh prefixes/vertices to 1,000,000 items, and the complete binary payload
to 16 MiB. Paste accepts compatible selected MotS model Things; an unbound target does not need the Arch flag beforehand.
It reuses an existing structurally valid record when all authored values match bit-for-bit; otherwise it stages one new
record with a unique wire index. The document transaction publishes the staged record, binding, and instance
Arch flag together, preserving value ownership and copy-on-write undo behavior while leaving failed
validation and allocation paths unchanged.

#### Entity system clipboard representation

Copy publishes one `QMimeData` object to the operating-system clipboard with two representations:

| MIME representation | Contents | Paste behavior |
| --- | --- | --- |
| `application/x-sed-entities-v1` | Binary bytes returned by `encodeEntityClipboard(...)` | The only representation accepted by **Paste Entities** |
| `text/plain` | A human-readable summary such as `SED: 2 Sectors` | Informational for clipboard viewers and other applications; never parsed by SED |

The private representation is a complete SED V1 `ProtectedV1` container, not a C++ object dump and not a process-local
handle. Its synthetic transport `Level` records the source `ProjectType`, contains exactly one non-empty entity family,
and uses a placeholder `Clipboard` layer while original per-entity layer references live in the metadata. Thing owner
Sector indices are cleared because containment is recomputed after placement. No source `Level`, `Selection`, Qt
object, raw pointer, external resource file, or undo history is placed on the system clipboard.

The container adds one optional `clip` section. Its current little-endian metadata is defined by
[`encodeEntityClipboard(...)`](../src/core/src/level/clipboard.cpp) and contains:

1. the eight-byte `SEDCLIP\0` signature
2. metadata version `1`
3. one entity-family discriminator: Sectors, Things, or baked lights
4. the three finite `double` coordinates of the placement anchor
5. one layer-reference count
6. for every transported entity, its signed source-layer index plus a byte-counted layer-name string

The entity bodies use the canonical native SED Level sections rather than a second clipboard-only serialization. The
required Level sections therefore receive the normal four-stream parity tier, while optional `clip` metadata receives
the two-stream tier. Header and descriptor mirrors, per-slot CRC32C, systematic Vandermonde MDS parity, decompression
bounds, and expanded-section SHA-256 are all validated by the ordinary protected SED reader before clipboard metadata
is trusted. This protection detects corruption; it is not encryption or an authenticity signature.

Paste treats the OS clipboard as untrusted input. The codec rejects an empty or greater-than-256-MiB container,
metadata greater than 16 MiB, decoded graphs outside the native SED 128-MiB object/2,000,000-item budgets, missing or
duplicate `clip` metadata, an unknown signature/version/family, non-finite anchor coordinates, mismatched layer/entity
counts, a mixed Sectors/Things/lights body, invalid Sector vertex/adjoin references, and metadata trailing bytes. The
complete decode and Sector-reference preflight finish before the destination transaction starts. The authoritative
limits and binary framing remain those in [sed.hpp](../src/core/include/sed/assets/sed.hpp) and the
[SED V1 binary contract](formats/sed_binary_format.md), rather than a separately versioned undocumented clipboard
layout.

Source project type is retained for lossless decoding but currently does not block paste into a different project
type. Asset names remain references and resource bytes are not embedded, so cross-project paste may require compatible
templates, MATs, models, sprites, COGs, sounds, CMP semantics, and entity fields. A private MIME payload can be supplied
by any process; successful parity/checksum validation proves internal consistency, not that another SED instance was
its author. The payload is not confidential either: another application with clipboard access can read the authored
entity bytes as well as the plain-text count/family summary. Current readers require SED V1 framing and clipboard
metadata version `1`; an unsupported future version is rejected rather than guessed.

### Episode/campaign editor

Primary files:
- format model, strict parsers, graph validation, merge helpers: `src/core/include/sed/assets/episode.hpp` and
  `src/core/src/assets/episode.cpp`
- modal JKDF2/MotS editor and startup helper: `src/app/src/ui/episode/episode_editor.*`
- Edit-menu action, dual-file migration, atomic persistence, and level placement: `src/app/src/main_window/menus/map_menu.cpp`

Ownership and safety:
- `sed_core` owns complete `episode.jk` sequence values and case-insensitive `cogstrings.uni` entries, including the
  legacy numeric field. Parsers require closed records, exact runtime grammar/bounds, and a valid branch graph; the
  legacy COG-string reader accepts overdeclared `MSGS` source capacity without inventing rows. Its writer emits the
  live row count and canonicalizes keys to uppercase;
- campaign strings remain raw game bytes. The dialog maps each byte reversibly through Latin-1 for display and rejects
  unrepresentable Qt text; it never silently transcodes an existing table to UTF-8;
- the dialog owns drafts only. Accept serializes both outputs before publishing them; cancelling makes no filesystem or
  document mutation;
- root, `misc`, and legacy `ui` COG-string tables are unioned before migration, and differing shared values/metadata require an explicit user
  choice. Save first takes a known-good existing/absent episode snapshot, each file uses atomic replacement, and a
  failure to write strings attempts to restore exactly that prior episode state;
- the Goals view owns 50 indexed runtime slots and merges with direct Strings edits per key by edit serial, preserving
  sparse keys and whichever view was changed last;
- optional startup COG generation is JKDF2-only because its bins are JK-specific. Placement into the Level is a normal
  undoable document mutation; MotS retains sequence/string editing without the incompatible generator.

### Editing operations

Primary directory: `src/core/src/level/`

Major modules:
- `transform.cpp`: translate/rotate/scale sectors, surfaces, vertices, things, lights, and thing frames
- `build.cpp`: surface extrude, sector primitive construction, saved-shape construction, and `shapes.tpl` loading
- `geometry.cpp`: adjoin/unadjoin, texture arrangement/recalc, sector search, etc.
- `cleave.cpp`: cleave surface/edge/sector, join/connect surfaces and sectors
- `merge.cpp`: merge surfaces, sectors, vertices
- `lighting.cpp`: legacy-compatible CPU calculation of persisted baked texture-vertex colors and optional derived
  sector ambient; it is not the runtime dynamic-light renderer

Geometry Move/Rotate/Scale commands optionally run through the default-on connected-surface preservation algorithm in
[`connected_surface_uv.cpp`](../src/core/src/level/connected_surface_uv.cpp). Core captures an immutable, material-free
baseline in the Level's declared UV space, evaluates preview and commit from that same basis and shared
double-precision transform descriptor, and validates every vertex/surface/topology target before publication. Complete
rigid faces remain exact; deformed neighbors and scaled faces add only the geometry-derived delta to each authored U/V,
so non-affine and multi-tile mappings survive while all baked-light colors and unmoved corners remain bit-exact. The app
previews these results through transient VBO overrides without mutating the Document, restores them on cancel, and
commits a validated candidate Level as one Undo step. The older `scaleTextures` metadata option remains separate.

Cleave carries every surviving corner unchanged and interpolates only a newly inserted edge corner in each owning
face's own UV/light frame; only newly constructed cap faces receive a generated mapping. Regression coverage lives in
[`level_transform_tests.cpp`](../src/core/tests/level/level_transform_tests.cpp),
[`connected_surface_uv_tests.cpp`](../src/core/tests/level/connected_surface_uv_tests.cpp),
[`level_geometry_tests.cpp`](../src/core/tests/level/level_geometry_tests.cpp), and
[`level_cleave_tests.cpp`](../src/core/tests/level/level_cleave_tests.cpp).

Surface Extrude keeps projected and source support records in typed P/P, P/S, S/P, and S/S buckets. Every candidate
crossing is checked directly against its immutable two-line supports before a representative is reused; an ambiguous
support relationship is rejected rather than selected by a rounded coordinate. `addOriginal(projected, sourceWorld)`
also validates the stored projection frame and retains the source-world operands, so source collinearity survives
rounding at the construction boundary. The registry carries no UV/RGBA attributes and never aliases by solved XYZ or
corner attributes; the separate postconstruction cleanup handles only appended vertices with exact XYZ and required
UV/RGBA agreement.

### Lighting ownership and pipelines

Lighting has two independent execution pipelines and three runtime/baked owner families. MotS `ARCHLIGHTING` adds an
editor-only model-color family described below. The complete model, equations, upstream source concordance, and
maintenance rules live in `docs/lighting.md`.

- `sed_core` owns `SedLight`, IJIM `PointLight`, surface/sector lighting fields, serialization, and the deterministic
  baked-light/ambient algorithm.
- `MainWindow` owns copied-level background bake jobs, automatic `SedLight` source tracking, dirty influence
  coalescing, generation/fingerprint validation, GUI-thread result publication, and baked-light transform preview.
  `GlView` republishes accepted baked colors through cached source-vertex/VBO ranges; it rebuilds complete mesh batches
  only when that cache mapping is unavailable or structurally invalid.
- `GlView` owns frame-local runtime preview for regular Thing lights, actor/player Headlights, and IJIM Sector point
  lights. It resolves inherited Thing values, collects a view-stable scene-wide eligible set (using camera distance
  only if a project cap overflows), restores deterministic source order, and evaluates the project formulas through
  an OpenGL std140 payload.
- **Baked**, **Thing**, and IJIM-only **Sector** are the runtime/baked owner categories; MotS **Architectural** is the
  editor-only model-color category described below. Viewport **Fully Lit/Lit/Diffuse/Gouraud** is a separate global
  ceiling, while each surface/3DO face also has a persisted authored mode.
- Non-negative range/intensity UI constraints apply to Baked `SedLight` records. IJIM runtime Thing RGB/range is kept
  in its raw authored representation, including RGB above one.
- Automatic baked recalculation observes only `Level::lights`. Geometry changes invalidate an in-flight worker through
  its fingerprint but do not schedule a bake; Thing/Sector runtime sources never enter baked output.

MotS `ARCHLIGHTING` is a separate persisted per-Thing model-color domain, not a fourth runtime point-light family. The
portable [`arch_lighting.hpp`](../src/core/include/sed/level/arch_lighting.hpp) API validates MotS project type,
effective Arch-flag ownership, finite channel values, and the current all-LOD mesh/max-vertex topology before applying
one failure-atomic `Initialize`, `Multiply`, `SetVertex`, or `Reset` operation. An effective Arch Thing with
archive index `-1` is a virtual all-zero owner until an edit materializes a record. Shared source records use
copy-on-write when selected and unselected Things would otherwise alias the same authored payload; dormant records
remain in SED and can be restored on reflag only while the all-LOD topology still matches.

The MotS viewport Arch editor stages selected-vertex RGB or Mono edits against the current document, selection,
resolved resource identity, and model topology. Dismissing a valid changed popup commits one undoable document
transaction; a no-op, invalid value, or stale target leaves authored data and history unchanged. The accelerated
viewport overlays record RGB while preserving model alpha; `mono` remains available to the software-engine path and
is not an opacity channel. This authoring path never creates a runtime point-light emitter or range gizmo. The viewport
uses one ARCH owner marker for every effective Arch Thing; in MotS it replaces the separate generic badge for that Thing.
Its persisted visibility is an editor/preview visualization preference and does not alter the level's dynamic-light
payload.

#### Interactive MotS ARCHLIGHTING submode

The Architectural submode restricts vertex selection and forced persistent owner gizmos to eligible, visible,
unlocked MotS Arch Things, including virtual all-zero owners with archive index `-1`. An unflagged model Thing
becomes eligible when its instance `thingflags` value is authored with bit `0x8`; compatible ARCHLIGHTING Paste can
set that bit while preserving unrelated flags and the Thing template. The interactive submode has no Add-placement
action. The persistent Architectural owner overlay defaults on in
Editor and off in Game Preview; active Editor submode forces it on for every effective owner, while Game Preview never
enables interactive vertex preview. The renderer publishes one selected-owner, read-only vertex
cache containing Thing, LOD, source-mesh, vertex, and transformed world-position identity; picking and marker drawing
consume that cache without per-mouse resource loads or a scene-wide rebuild. The owner light icon and painted Thing body
share the published pick anchor, so body and marker clicks resolve the same eligible owner with ordinary depth
occlusion; X-Ray may peel through unrelated Things. Marquee selection emits all eligible owners in the rectangle with
Replace/Add/Remove semantics and clears its drag state on release, cancellation, or mode change.

Expanded hierarchy meshes carry `Model3do::meshSourceIndices`, so a clone is resolved against the authored source mesh
ordinal used by the archive record. Missing or malformed mapping rejects the ARCH preview rather than guessing from the
expanded ordinal; raw/simple models use ordinal identity. Resource and level publication, template/model replacement,
ARCH table notification, and mode exit invalidate the cache and restore any staged triangle patches. Exact source-vertex
patches preserve the ordinary lighting composition and are sparse and renderer-only; Mono/RGB preview selection never
mutates the persisted four-channel record. Before an interactive viewport edit commits, it revalidates the owner resource identity, vertex topology, baseline
record, and layer visibility. Shared imported records split with copy-on-write when selected and unselected owners
diverge.

### Asset loading and resource location

The portable 3DO parser has independent operational budgets: 128 MiB of literal source bytes, 64 MiB of aggregate
retained section-line view capacity, at most 2,097,152 temporary token views per line, and 128 MiB of logical decoded
model storage with at most 2,097,152 decoded items. Byte-span and stream/path entry points share source admission; path reads use binary
mode so CRLF bytes count literally. Declared row counts are checked against available records before count-driven
allocation; raw geometry, required shared-mesh snapshots, and hierarchy-expanded clones are charged before copying.
Repeated nodes cannot bypass admission by multiplying individually valid meshes.

Hierarchy transform admission separately caps work at 16,777,216 vector visits, counting mesh vertices and both normal
arrays through their parent walks. An O(N) memoized node-depth preflight checks aggregate work before hierarchy clones
or transforms. Raw authored-mesh loading bypasses that transform-work charge because it does not apply the hierarchy;
source, scanner, token, and decoded-model budgets still apply. These are editor operational defaults shared by rendering,
resource previews, and Sector import, rather than retail-format maxima or a total process-memory bound.

JKL/NDY import distinguishes resource capacity pools from exact geometry counts. Invalid WORLD/SECTORS counts,
truncated Sector records, extra `SECTOR` records beyond `WORLD SECTORS` within the SECTORS section, missing required
`SURFACES`, and invalid world-vertex references reject the candidate. Comments, unknown extension fields, and later
sections do not count as extra Sector records. The established bad texture-vertex-index UV fallback is retained
independently of world-geometry admission.

Primary directory: `src/core/src/assets/`

Notable files:
- `jkl.cpp`, `jed.cpp`: level load/save, including editor layer persistence
- `cog_script.cpp`: COG `symbols` parsing used by load/save and editor workflows
- `3do.cpp`, `spr.cpp`, `mat.cpp`, `tpl.cpp`
- `jlh.cpp`: category tree/list persistence
- `resource_locator.cpp`: searches disk roots and mounted `GOB`/`GOO` archives, provides `tryReadBinary` /
  `tryReadText` APIs

The metadata-only `ResourceLocator::findSelectedOnDisk` query returns a loose path only when it wins the normal
candidate/root/archive precedence. It reads no payload and does not materialize lazy primary overrides. The explicit
byte-budget `ResourceLocator::tryReadBinary` overload preserves the same winning-source precedence.
A loose file's size metadata is only a reserve hint: the reader captures through EOF within the caller's budget and
probes for excess bytes, so growth or a stale small hint cannot admit a truncated prefix. An oversized or unreadable
winner does not fall back to a smaller shadow. A successful bounded read can report its winning loose path; archive,
refusal, missing, and exceptional results clear that output. Trusted primary-resource callbacks are checked after
materializing their result because they supply no size metadata; this API does not bound allocations inside a callback.

The [3DO parser](../src/core/src/assets/3do.cpp) selects one requested GEOSET, retains its source meshes, and
expands hierarchy references. The [viewport mesh builder](../src/app/src/render/gl_view_mesh.cpp) visits every resulting
mesh, including an unreferenced source mesh. The game renderers traverse hierarchy-reachable meshes, so a custom 3DO
with drawable orphan meshes can show extra geometry in SED. JKDF2/MotS viewport model LOD is opt-in and defaults to
GEOSET 0. The app caches parsed variants by resource identity and selected GEOSET; camera or live level-header LOD
threshold changes select the visible variant from Thing-origin forward depth for ordinary Things. Effective Arch
Things (`thingflags & 0x8`) remain at GEOSET 0, matching the classic sector render pass. Staying inside one tier
reuses the mesh presentation, while crossing a threshold rebuilds the Thing mesh presentation and its visual pick
ranges. A Thing-only rebuild reuses the composed static Level geometry and surface-UV index when their cache
boundaries remain valid, then replaces and uploads the Thing suffix; a failed boundary check uses full composition.
Collision, Visual Bounds, MotS ARCH editing identities, and placement guides remain based on GEOSET 0;
the placement preview model and Thing Type gizmo clearance use the selected visible GEOSET. Level-header distances
and the viewport toggle are separate: header
values are authored Level data, while the toggle is independent Editor/Game
Preview workspace state saved in the native EDIT section and application preferences.

MAT/CMP compatibility boundary:

- [MAT decoding](../src/core/src/assets/mat.cpp) supports indexed/IndexedCols, packed 16/24/32-bit RGB(A), color MATs,
  multiple cels, mipmaps, palette-index-zero alpha reference, and bounded malformed-input rejection
- the compatibility audit covered all 748 retail JKDF2 `Res2_GOB` MATs and all 1,142 retail MotS `JKMRES` MATs; indexed
  assets consistently use mode/bpp, while their other `ColorInfo` words repeat legacy or uninitialized patterns
- indexed `alphaBPP` is consequently interpreted only as the observed zero/nonzero alpha-reference marker, not as a
  literal channel width; no unsupported retail `ColorInfo` layout was found
- [CMP decoding](../src/core/src/assets/cmp.cpp) retains the 256-color palette, all 64 256-byte light-level translation
  tables, and the final table compatibility view used by Pascal `LoadCMPTable`; these tables are not CMP-header Tint
- the core exposes and tests master-palette translation plus per-MAT Solid-color light-ramp resolution; applying a
  non-identity active Sector CMP translation to textured pixels in every renderer and preview remains a rendering
  boundary, not a MAT parser gap

## App (`sed_app`)

### Startup flow

Primary files:
- `src/app/src/main.cpp`
- `src/app/src/main_window/main_window.hpp`
- `src/app/src/main_window/core.cpp`
- `src/app/src/main_window/project/startup.cpp`
- `src/app/src/ui/project/project_startup_utils.*`

Behavior:
- app can start in a dedicated startup dialog before the main editor is shown
- startup flow can:
  - create a new project
  - open a level
  - jump to preferences/project configuration
- startup dialog uses a Visual Studio-style recent-file layout with search, file/project icons, pin/unpin, a clearer
  `Open...` action, and project configuration shortcuts
- recent-file state is updated after a successful level open, native SED save, or engine-level export; an export is an
  openable recent but never replaces the active native SED save path. Pinned recents stay grouped at the top while
  unpinned entries return to last-used ordering
- filesystem dialogs remember their last accepted folder. Open and Save share one folder; Import and Export each
  become independent after their first use. An unset group begins in the most recently used folder from any group,
  while a suggested output filename is retained when its parent folder is replaced
- `Autosave History...` lives in `File > Open Recent` even when there are no ordinary recents. It opens the explicit
  periodic-snapshot browser; `.sedautosave` paths are not inserted into the ordinary recent-file list
- the startup recent list uses one `itemActivated` Open route for both keyboard and double-click activation. A gesture
  must never enter `openLevelFromFilename` twice or show a second load/progress/recovery sequence after installation
- eligible live recovery is offered before the startup chooser and before the main editor is shown. For Open or Open
  Recent initiated from the chooser, the chooser remains visible while matching-recovery and current-workspace
  Save/Discard/Cancel decisions are resolved. The editor is revealed exactly once after replacement is authorized and
  before the selected document is loaded or applied; cancellation or rejection leaves the chooser visible
- closing the startup dialog exits the app instead of opening an unconfigured editor
- the startup window has no operating-system title bar. Its custom minimal frameless shell uses a rounded translucent
  outer border and a client-drawn title bar. The explicit Close control stays fully inside the far-right title-bar
  lane at both preferred and minimum sizes. Initial geometry is centered inside the selected monitor's available area
  with a small high-DPI-safe frame margin
- new project creation seeds:
  - one `2 x 2 x 1` starter sector matching Pascal `TJedMain.NewProject` room bounds
  - Pascal face anchors and `RecalcAll` UV initialization for JKDF2/MotS; IJIM retains its project-specific face order
    and `RecalcNewAll` initialization
  - one player thing
- the main editor starts maximized by default, remembers window geometry/state, and shows the taskbar icon while the
  startup dialog is visible
- the startup dialog owns the `SED - Sith Editor` branding; the main editor title is only `<path>*`, or `Untitled*`
  for a new unsaved level, where `*` means the current document has unsaved changes
- the status bar includes a small current-project-type marker

### Add Sector workflow

Primary files:
- `src/core/src/level/build.cpp`
- `src/app/src/main_window/sector/draw.cpp`
- `src/app/src/main_window/sector/shapes.cpp`
- `src/app/src/main_window/viewport/overlay.cpp`
- `src/app/src/render/gl_view_render.cpp`

Behavior:
- Add Sector is a Blender-style draw workflow: press-drag the footprint, release for height, click to commit, `Esc` to
  cancel.
- Built-in sector primitives include cube, cone, cylinder, UV sphere, and ico sphere.
- Cylinder/cone/sphere shapes expose sides/segments in the always-visible Add Sector options overlay.
- The overlay shares the transform system's default-off **Grid Snap** action. A placement captures the active grid
  frame at press time, including an authored origin/rotation and adaptive displayed minor spacing, so pointer motion,
  preview publication, commit, cancellation, preference persistence, and recovery use one coherent snap state. A
  same-cell hard-snap result remains invalid rather than being expanded by the ordinary minimum-dimension clamp;
  height minimum/maximum bounds are applied as whole frozen-grid steps.
- The start plane can be switched between `X`, `Y`, and `Z`; `Z` is the default.
- **At view** uses a camera-facing interaction plane through the current target
  so pointer input remains available even when the selected global X/Y/Z plane
  is edge-on, then maps and snaps that hit on the selected global output plane.
  The completed sector remains aligned to that selected global axis. **Free**
  instead captures the camera right/up/forward basis, hides the
  irrelevant axis/At view controls, and creates a genuinely view-oriented
  sector. Both paths retain inward polygon winding.
- Original SED-style reusable shapes are loaded from `data/shapes.tpl`, and the UI now labels them as `Shapes`.
- `Save Selected Sector as Shape` writes the single selected sector back into `data/shapes.tpl` without a blocking
  naming dialog, records its Level UV-space tag, auto-selects the new shape, and keeps the Add Sector flow active.
  Untagged historic shape blocks default to pixel UVs; placement converts those through real MAT dimensions, while a
  newly saved normalized shape is copied without a second conversion.
- Idle Add Sector mode draws a soft placement guide at the cursor with a center cross on the selected start plane.

### Join / merge editing workflows

Primary files:
- `src/core/src/level/cleave.cpp`
- `src/core/src/level/merge.cpp`
- `src/app/src/main_window/sector/join.cpp`
- `src/app/src/main_window/viewport/tools_overlay.cpp`

Behavior:
- The UI uses distinct editor-facing names: **Join sectors**, **Merge sectors**, **Adjoin**, **Unadjoin**, **Merge**,
  and **Connect surfaces**.
- The core API still uses legacy terms such as `levelConnectSurfaces`, `levelConnectSectors`, and
  `levelUnadjoinSurface`; these names describe the underlying adjoin/portal operations and should not be renamed just
  to match the UI labels.
- **Join sectors** previews the result for two or more selected sectors before the edit is committed.
- With **Keep overlap pieces** disabled, the selected knife sector acts as a cutter: the overlap inside target sectors
  is removed while required holes and adjoins are kept.
- Cutter-only join has extra cleanup/retry passes for rotated knife sectors and complex target sectors: leftover knife
  fragments inside targets are deleted, outside knife pieces can remain, and compatible back-to-back surfaces are
  rechecked for adjoins after cleanup.
- **Merge sectors** accepts two or more selected sectors and repeatedly merges adjacent valid pairs to a fixed point
  in one undo transaction. Nonadjacent or non-convex candidates are skipped and remain selected, while valid
  independent pairs and newly formed chains continue deterministically.
- Surface **Merge** follows the same fixed-point selection rule within each sector. Cross-sector, non-coplanar,
  non-convex, or adjoin-unsafe candidates are skipped and remain selected instead of aborting other valid merges.
- In Surface mode, **Adjoin** creates/adjoins compatible back-to-back surfaces, **Unadjoin** removes an adjoin,
  **Connect surfaces** directly connects two selected surfaces in different sectors, and **Merge** merges two
  same-sector surfaces that share an edge.
- Surface **Merge** also accepts larger selections. It deterministically
  rescans after every successful merge so adjacent chains reach a fixed point,
  leaves skipped/unpaired faces selected, and retains the lower-index
  survivor's authored UV frame.
- `Edit > Sector > Import Sector(s)...` accepts 3DO and Wavefront OBJ geometry.
  Non-empty 3DO hierarchy nodes and OBJ objects/groups can become separate
  sectors (the default) with matching portal faces auto-adjoined, or be combined
  into one sector. OBJ import defaults to a handedness-preserving rotation from
  conventional Y-up coordinates to Sith Z-up coordinates,
  `(x, y, z) -> (x, -z, y)`. The import dialog can disable this rotation for an
  OBJ already authored in Sith coordinates. The coordinate conversion touches
  positions only: per-corner UV indices, corner order, and UV axes remain
  authored exactly. External OBJ `vt` values and live Level surfaces are both
  normalized, so ordinary OBJ import needs no material-size scaling. Older
  SED-written OBJ can declare pixel UVs in a comment marker; those coordinates
  are admitted only through each face material's real MAT dimensions. 3DO
  remains in its version-native Sith coordinate and texture space. Standard homogeneous OBJ
  position weights are resolved before rotation; polygon winding is preserved,
  and normals are derived from final Sector geometry rather than imported `vn`
  records. Object/group and `usemtl` names keep their complete trimmed text.
  When a level exporter places many complete sectors in one
  OBJ object, a face-order scan partitions only prefixes whose every
  undirected edge has exactly two uses; this handles shared-portal edge counts
  without weakening manifold validation or guessing connected components. The
  3DO import requests the asset loader's raw authored meshes and independently
  applies each hierarchy node's pivot, local transform, and complete parent
  chain. Multiple nodes may therefore instance the same mesh at different
  positions without cumulative mutation. Ordinary renderer-facing 3DO loads
  retain their transformed-mesh contract and now clone shared mesh storage per
  node before transforming it. Invalid or cyclic hierarchy parent chains are
  rejected instead of being partially applied. Import is atomic and rejects open,
  non-manifold, non-planar, non-convex, or consistency-error geometry without
  changing the document. Topology errors separately count open boundary edges
  and edges shared by more than two faces, giving hole-closing guidance for the
  first and internal/overlapping-face guidance for the second. A bounded structured diagnostic names the source
  mesh/object, face/corner where applicable, welded edge endpoint indices and positions, owning face indices, and edge
  use count. The dialog exposes those details without overwhelming its main designer-facing summary. Exactly one
  categorized Core record writes the source path, summary, and same bounded details to `sed.log`.
- `Edit > Sector > Export Sector(s)...` is enabled for a non-empty sector
  selection and writes atomically to OBJ or 3DO. OBJ emits one polygon object
  per selected sector. 3DO emits one zero-transform hierarchy node/mesh per
  sector and preserves polygon positions, per-corner UVs, and material names.
  OBJ output marks its UVs as normalized so round trips are exact. Classic
  3DO 2.1 export converts through MAT dimensions to pixels, while IJIM 3DO 2.3
  writes normalized UVs; editor-only sector properties and adjoin relationships
  are outside the 3DO representation. Re-import reconstructs compatible matching portals.

  JKL/NDY export shares world vertices by exact authored coordinate equality by default; decimal serialization retains
  the existing text precision separately. The export dialog provides an opt-in nearby-vertex weld with a default
  maximum Euclidean distance of `0.00001`. Welding retains the first representative in deterministic traversal order
  and never replaces representatives, so proximity chains do not drift. It is an export-only projection: the live
  Level, native SED data, and Undo/Redo history are unchanged. SED rejects collapsed distinct corners or degenerate
  changed faces, and rejects newly nonplanar or concave faces and newly nonconvex sectors after welding. A welded
  coordinate may move by up to the configured threshold.
  Surface Extra Light export has a separate **Clamp extra light** option, enabled by default. It clamps finite classic
  scalar and IJIM RGB values to signed `[-1,1]` for the exported file while leaving the document and imported values
  unchanged. Clearing it preserves the authored finite values; alpha remains unchanged. Renderer evaluation is
  unaffected by this export setting.
- Surface marquee selection publishes polygon owners in Face mode, exact
  edge refs plus derived edge-touching owners in Edge mode, and exact `SurfaceVertexRef`s plus derived
  owning-wire context in Vertex mode. Non-X-Ray solid selection performs one
  batched scene-depth readback for exact face/edge/vertex candidates; a CPU ray-depth
  fallback preserves the same front-wall boundary before the pick framebuffer is
  available. X-Ray and the explicit Ignore Filters chord keep their deliberate
  through-selection behavior. The selection overlay emits no inferred
  vertex points: only valid explicit vertices receive vertex colors, the active
  vertex is emitted once in red with its owner wire primary, and other vertices
  and owner wires remain secondary. The exact active vertex is a GlView sidecar
  consumed by Outliner refresh rather than replaced by the minimum set member.
  Surface Item Edit and the Outliner current row consume that same validated
  GlView primary owner for Face, Edge, and Vertex multi-selections. They never
  derive focused fields from unordered selection iteration; a lexicographic
  face is used only when no valid editor sidecar exists (for example a legacy
  restored state or a new marquee selection).
  Edge refs are held as transient GlView
  state with local edge and endpoint ids, revalidated after geometry changes,
  and cleared across document/mode/submode boundaries. Selected edges are
  removed from their owner-wire range: non-primary members are emitted once in
  a wider secondary-color range, only the active member is emitted once in
  the active-color range, and the active owner remainder is emitted in the
  primary range. Coincident exact segments are de-duplicated with active
  precedence. Add/remove reconciliation publishes the surviving exact active
  Edge owner as the primary Surface before rebuilding overlays; the retained
  independent Face context cannot override that sidecar with an unrelated
  lexicographic owner. Existing full-wire stencil and occluded passes remain
  in place, without duplicate color layers on selected segments.
- Surface Face **Delete** removes selected faces and may leave an open Sector. Ordinary **Delete** in Surface Vertex
  mode removes only unused vertices or
  redundant in-segment collinear edge vertices whose two owners restore the
  same unsplit endpoint pair. The clone-then-commit operation updates every
  owner and its per-corner UV array atomically; valid closed reciprocal topology
  remains closed. Structural vertices in a mixed selection are skipped.
  Edge/Vertex mode never falls through to Face deletion, so stale owner-only or
  invalid sub-element state is a no-op instead of deleting polygons.
  In Vertex mode, **Shift+Delete** is a separately key-mappable, confirmed recovery action that
  removes any selected vertex and may deliberately leave invalid surfaces; the
  operation is atomic/undoable and validation or export can reject the level
  until it is repaired.

### Surface extrude preview workflow

Primary files:
- `src/core/src/level/build.cpp`
- `src/core/include/sed/level/build.hpp`
- `src/app/src/main_window/surface/extrude.cpp`
- `src/app/src/ui/surface/surface_extrude_preview_utils.*`
- `src/app/src/main_window/viewport/tools_overlay.cpp`

Behavior:
- Surface **Extrude** stays active until `Apply`, `Cancel`, or `Esc`.
- The options strip exposes exact `Length` and `Expand` numeric controls plus explicit apply/cancel actions.
- Preview geometry is built as a temporary sector and source-surface cut preview without mutating `sed::Level`.
- Preview fill uses a readable 74% alpha, and every preview wire is clipped in
  homogeneous space against the complete camera frustum before QPainter sees it.
- Negative expand values use source-aware minimum/step logic so the UI cannot step past the collapse limit.
- Length must be positive and finite. Generated solid and portal validation rejects collapsed or invalid output before
  publication, including when expansion performs additional cuts.
- Applying the operation commits through `sed::level::levelExtrudeSurface(...)` and resets the preview values.

### `MainWindow` responsibilities

Primary files:
- `src/app/src/main_window/main_window.hpp`
- `src/app/src/main_window/*.cpp`
- `src/app/src/main_window/*/*.cpp`
- reusable helpers in `src/app/src/ui/*`

`MainWindow` owns:
- `sed::editor::Document document_`
- global UI state (edit mode, sub-mode, preferences, outliner/properties/checks/messages/history widgets)
- resource settings (`resourceRoots_`, `resourceGobs_`, `resources_`)
- layer view state (`layerVisible_`, `layerLocked_`, `layerColors_`)
- keymap bindings and unsaved-state tracking
- docked tool workflows such as the UV editor and `COG Scripts`
- persisted query/find state for sectors, surfaces, and things

Key responsibilities:
- route viewport signals into `sed::level::*` mutations and `document_.applyUndoable(...)`
- rebuild UI panels (outliner, properties, checks, messages, history)
- keep viewport overlays in sync with selection, tool state, and preferences
- manage project resource roots and mounted archives
- publish hidden-layer and Preview-policy changes through one stable-pick-ID mesh rebuild. Open/new/recovery stage
  layer state until the existing final Level/EDIT publication; an interactive single-layer or **Show All** rebuild
  that exceeds 500 ms
  repaints only its indeterminate status-bar widget, without `processEvents`, input pumping, or reentrant edits.
  Controllable CPU loops and 8 MiB graphics-buffer writes provide internal paint boundaries. Opaque spatial-index
  calls and graphics-driver allocations receive explicit pre/post boundaries; one synchronous driver allocation
  cannot repaint again until that non-yielding call returns
- drive item-editor workflows such as resource picking, category editing, flag editing, frame editing, layer
  assignment, and COG links
- show save prompts before destructive navigation actions and on app close for dirty COG tabs

### Resource discovery, categories, and pickers

Implementation:
- defaults/resources panel: `src/app/src/main_window/resources/panel.cpp`,
  `src/app/src/ui/resources/project_resource_settings.*`
- startup/project helpers: `src/app/src/ui/project/project_startup_utils.*`
- picker UI: `src/app/src/ui/resources/resource_picker.*`
- shared category widgets: `src/app/src/ui/components/category_tree_widgets.hpp`
- shared membership mutation/transaction helpers: `src/app/src/ui/components/category_membership_utils.hpp`

Key concepts:
- install directory is per-project and is used to discover known archives and validate roots
- configuration rejects a path only when its standard archive names clearly identify one other supported game;
  incomplete/custom layouts and shared roots containing signatures for multiple games remain usable
- IJIM prefers `InstallDir/Resource` as the disk root when present
- mounted archives are opened as `sed::assets::GobArchive`
- the generic resource picker is parameterized by `ResourcePickerKind`

Current picker kinds:
- `cog`, `material`, `model3do`, `key`, `wav`, `ai`, `template_`, `message`, `cmp`, `snd`, `pup`, `spr`, `par`

Dialog startup contract:
- `ui::pickResource(...)` constructs and polishes the complete dialog shell before entering the modal event loop
- application dialog chrome is finalized during pre-show Polish. A late Polish may maintain the safe caption width,
  but it never mutates Qt or native window flags on a visible modal because the resulting hide would terminate
  `QDialog::exec()`
- MainWindow picker calls pass the already-published immutable `ResourceLocator` generation by shared ownership;
  opening a modal picker does not copy mounted GOB/GOO indexes before the native shell can paint. Archive replacement
  creates and publishes a new picker-local snapshot only after the replacement index has loaded successfully
- `ResourceLocator` value copies share private root/archive configuration; a successful mutation detaches it and
  advances the configuration identity. A selected 3DO/SPR source creates a cheap snapshot with a lazy primary-resource
  override. Its bytes, missing result, or read failure materialize once across copied snapshots; other asset names keep
  normal project dependency precedence. Revision lookup does not materialize the override
- the resource list starts disabled and hidden behind an opaque dark loading page, so native window creation cannot
  expose an unpainted white client area or a partially populated list
- source discovery and initial row population are deferred long enough for the opaque loading shell to paint once
- rebuilding the initial source tree blocks selection signals, then calls the resource refresh path exactly once;
  this avoids the duplicate full enumeration that previously made material picking pause for several seconds
- after population, the requested initial resource is restored, the list is enabled, and the loading page is replaced
- disk-origin emphasis uses a shared, revision-keyed set of normalized disk resource names. Each root and
  kind-specific directory is scanned once per cache key instead of calling `ResourceLocator::findOnDisk()` for every
  row. Where applied, bold is a loose-copy-availability hint; a selected archive can still be the actual preview source.
  Template and model thumbnail rows do not apply this font hint, so their normal text does not imply disk absence
- every generic picker has a separate selected-item Storage line, including image/model preview kinds. It resolves
  the current section's winning loose path or archive basename and case-preserved entry path, or reports Unavailable.
  In All, a scoped primary resource override reports Project data through its revision identity without invoking its
  lazy reader. Explicit disk/archive sections retain their selected source.
  Template entries identify project template-database data rather than an individual `.tpl` resource file.
  Selection updates this line independently of asynchronous preview completion; clearing selection clears it.
  Metadata refresh follows the existing silent WAV path and does not start or restart audio

Category/browser behavior:
- a complete category pair beside a saved level overrides shared `data/*.jlh` / `*.jll`; mounted copies remain a
  fallback
- template, material, and sound category filenames are selected by `ProjectType`; picker edits save through shared
  core JLH/JLL support and use the saved level's project directory when available
- missing WAV pairs seed a normal nested `Sounds > Uncategorized` model from the active project's discovered WAVs
- `All` is a top-level entry rather than the parent of every category
- categories split into:
  - structural branches
  - leaf categories backed by assignable JLL lists
- list-side membership editing exists through right-click actions and drag/drop
- drag/drop membership assignment carries the whole multi-selection, does not change the current category view after
  drop, batches the assignment work, and supports `Ctrl` move from a non-`All` source category
- both the resource picker and template editor commit membership changes through the same copy-first transaction: an
  alias-preserving JLH/JLL clone is mutated and saved, then retained only on success; no-op, save-failure, and exception
  paths restore the exact prior in-memory graph
- core pair persistence stages and checks both outputs beside the destination before moving either original, keeps
  backups through installation, and rolls both paths back after a reported commit failure. A failed rollback leaves a
  named recovery directory. The two destination renames are not one OS-wide atomic operation, so power loss remains a
  boundary; best-effort cleanup after a successful commit can also leave an obsolete transaction/backup directory
- drag feedback uses explicit action/cursor feedback plus a thumbnail pixmap, and selection rectangle state is cleaned
  up after drag/drop
- a hidden internal `General` child can be used as storage for assignable branch categories while remaining invisible
  in the UI
- focused tests cover the shared action resolver, complete real-name MIME payload, synthetic `Ctrl` multi-item drop,
  stable current view, selection cleanup, membership Copy/Move/deduplication, transaction rollback (including hidden
  `General` creation), and JLH/JLL save/reload. This exercises the common seam used by category-backed picker variants
  and the template editor rather than duplicating the same contract in each dialog

Preview/thumbnail behavior:
- picker/template preview pipeline shares caches for CMP, MAT, SPR, 3DO, and rendered thumbnail output
- asset-authoring previews explicitly show inherited hidden/disabled Things so a template's gameplay flags cannot make
  its picker, placement, Template Editor, or thumbnail preview disappear
- material and other tile thumbnails remain visible-range-first and are queued in bounded timer batches after the
  dialog becomes interactive; full-list thumbnail generation is not part of the first-show path
- resource search uses one short single-shot refresh timer; presentation-only filtering, sorting, and layout changes
  retain an unchanged selected preview and raw-`3DO` dependency watch plan
- rapid search edits cancel queued thumbnail work; filtering publishes only after typing pauses, and thumbnail batches
  resume after a separate quiet interval so search remains responsive instead of competing with preview rendering
- scrolling likewise postpones completed-icon publication until 96 ms after the last scroll event. The picker,
  Template Editor, and Thing Placement use coalesced single-shot scheduling rather than accumulating one callback per
  worker completion or scroll event
- raw `3DO` and template browsers register model/MAT file dependencies only when a row becomes visible or selected;
  the raw picker never resolves the complete model list synchronously before its first thumbnail timer can run
- raw `3DO` and template pickers construct an empty, hidden `GlView` host before first Show so Qt chooses the required
  OpenGL-capable top-level surface on its initial native mapping. This avoids the destructive modal remap caused by
  adding the first `QOpenGLWidget` after the dialog is visible. Model reads, scene binding, GPU uploads, and visible
  interaction remain deferred until a valid selection is prepared, so the dark loading shell and delayed list
  population still paint first. A low-rate dialog-owned cache retry complements the worker-completion signal so a
  prepared selection cannot remain stuck on its loading page after a delayed queued notification
- cold raw-`3DO` rows consult a cheap rendered-image hint before computing any resource revision. A cache miss therefore
  does not issue one loose-file/archive lookup per listed model. On a hit, the GUI consumes the exact
  revision-labelled key already published by the worker and checks its invalidation epoch instead of synchronously
  re-statting the 3DO, CMP, and every MAT; the shared file watcher advances that epoch and clears hints on change
- list construction reads the thumbnail-mode preference once and assigns one implicitly shared loading icon to every
  cold row instead of repeating a settings lookup and QPainter/QPixmap construction for each listed model
- visible-range and keepalive discovery sample a bounded viewport grid rather than walking every list item, so timer
  work is independent of the number of off-screen models
- `3DO` parsing, hierarchy expansion, model-bound preparation, and referenced MAT/CMP read/decode run in one
  process-wide, low-priority preview pool rather than Qt's global pool. The configured worker count is bounded to
  1-16 and defaults to 2-4 according to the available CPU count; closing an owner or publishing a new resource epoch
  prevents a late job from updating the current list
- loose resources and mounted entries already arrive as complete byte buffers. Preview 3DO parsing scans that byte
  span with non-owning line/token views and `std::from_chars`, without a whole-file string or per-line copy; stream/path
  callers use one reusable 8 KiB read buffer before entering the same span parser. MAT decode transfers ownership of
  its existing resource buffer instead of cloning it. Memory mapping is deliberately avoided because it does not help
  mounted-archive payloads and adds setup overhead to typical small loose files
- equivalent immutable `ResourceLocator` snapshots share parsed models, prepared bounds, and rendered images through
  exact disk/GOB revision keys rather than being isolated by the locator object's address
- fast lookup hints and queued preparation keys include the resource configuration identity. Authoritative disk
  revision keys still share work across equivalent snapshots; archive keys also include successful mount identity,
  so an explicitly reloaded same-metadata archive cannot borrow an older parsed result. Revision lookup requires a
  regular file, matching the actual loader rather than allowing a directory to shadow the winning source
- the heavy parsed-model, rendered-image, MAT-image, and sprite caches use byte-bounded two-queue retention. A first
  access is probationary, a repeated access becomes protected, and eviction removes the oldest one-hit asset first;
  this approximates K=2 LRU with constant-time operations and a combined ceiling below 1 GiB. Publishing a new
  resource snapshot or project generation advances the epoch and clears the shared caches
- the same worker job that prepares the exact model/CMP/MAT revision rasterizes the list thumbnail into a `QImage`.
  The worker renderer consumes every hierarchy-transformed mesh and shares the viewport's classic/IJIM UV,
  zig-zag polygon, MAT alpha/face-flag, material-orientation, and wireframe contracts. It renders at 2x dimensions and
  reduces with aspect preservation so small hierarchy meshes remain clear without squeezing wide or tall models
- each worker publishes preparation readiness and its revision-keyed `QImage` only when its immutable resource
  generation and invalidation epoch are still current. The configured pool is the global preparation/render
  concurrency limit, so multiple pickers cannot multiply CPU load. GUI code only converts completed images to
  `QPixmap`/`QIcon` and assigns them in a configurable 1-8 item batch (default 2). A turn converts at most one newly
  completed shared image, guarantees one visible icon can advance, and then stops at a 2 ms GUI budget. Further turns
  are paced at 40 ms so list repaints do not compete with every 33 ms interactive-preview frame; the current row is
  published first and off-screen rows are never drained first
- pending visible rows do not poll parsed-model/bounds readiness or resource revisions while preview workers are
  active. Worker completion wakes the owner for its next bounded publication turn. A completed icon and row state are
  committed with list-model notifications suppressed, followed by repaint of only the fixed-size tile, so filling a
  page cannot trigger a full list layout or disturb the rotating selected preview
- preview revision keys follow the exact disk/GOB resource selected by `ResourceLocator`; a rendered model key includes
  the invalidation epoch, renderer version, its `3DO`, active CMP, and referenced MAT revisions. The epoch prevents a
  capture spanning invalidation from publishing stale pixels after an exact same-size/same-timestamp notification
- decoded SPR images and every outer sprite icon key include the SPR, active CMP, and the SPR's referenced MAT
  revision. Sprite rows and selected previews share the same canonical unmirrored pixels and exact alpha, and a
  MAT-only edit cannot leave a warm sprite preview stale
- Thing-placement template icons use the MainWindow-owned preview cache, which is bounded to 192 entries. Resource
  generation invalidation clears that cache before a new request can publish an icon from an obsolete dependency;
  repeated requests for the same current revision reuse the cached image
- a shared debounced file watcher invalidates and requeues the main viewport, raw/template picker, Template Editor, and
  Thing-placement previews for loose `3DO`, `SPR`, `MAT`, and CMP changes, including atomic replacement and new
  higher-priority overrides. Exact file notifications remain revision-independent, but parent-directory notifications
  invalidate only when a resolved dependency revision actually changes. Unrelated autosave/editor writes therefore do
  not clear render caches, retry missing resources, or repeat missing-resource diagnostics; repeatedly observing an
  already registered resource also does not rebuild the filesystem watch plan. A new visible dependency extends that
  plan incrementally instead of rescanning every previously visited model and MAT; explicit resource remounts still
  perform a complete rebuild. It watches every mounted GOB/GOO plus
  its parent directory; archive replacement is
  re-indexed transactionally before preview invalidation. Modal picker/template dialogs own locator copies and defer remounting
  while one of their bounded background model-prep reads is active. Failed or temporarily missing replacements
  retain the complete prior mount index while automatic reload retries use 250, 500, 1000, 2000, and 4000 ms backoff,
  then continue at 4000 ms until the reload succeeds or the owner is torn down. One warning is emitted for the failed
  notification/retry state. A fresh notification resets the retry delay to 250 ms; warning suppression resets only after
  a successful reload.
  MainWindow's viewport owns the mounted archive indexes and their retry state. Hidden thumbnail, selected placement,
  and the outer placement watcher delegate mounted-archive reloads to that owner while retaining their loose-file watches;
  successful nested picker and Template Editor reloads refresh their owning rows and icons after the new generation is
  published.
  Candidate-directory
  notifications are compared with the exact resolved dependency revision, so an unrelated sibling write (including a
  recovery/autosave file next to a level) cannot clear viewport caches, rebuild meshes, or retry any registered
  known-missing dependency. The same revision filter is extension-agnostic and is regression-tested for every
  ResourceLocator category (3DO, SPR, MAT, CMP, WAV, SND, AI, KEY, PUP, PAR, PER, COG, TPL, UNI, and DAT)
- visible-row keepalive/self-healing logic requeues stuck `Loading` thumbnails during fast scrolling
  but does not wake the pending-row path while a worker completion is already expected. If cache invalidation makes
  that completion stale, the worker may not publish its result but still wakes a live owner after retiring its old
  in-flight key; the owner can then queue the row against the current resource generation. Destroyed owners receive
  neither publication nor wake-up

### Audio playback and sector ambience

Primary files:
- playback/ownership: `src/app/src/ui/audio/app_sound_player.*`
- ambient transition/cache policy: `src/app/src/ui/audio/audio_transition_utils.*`
- camera-to-sector request policy: `src/app/src/ui/audio/sector_sound_utils.*`
- main-window polling/mute routing: `src/app/src/main_window/viewport/audio.cpp`
- portable PCM/INDYWV decoding: `src/core/src/assets/wav.cpp`, `src/core/src/assets/indy_wv.cpp`

Ownership and layering:
- `sed_core` validates and decodes file formats without Qt or an audio device
- `AppSoundPlayer` owns Qt playback objects and arbitrates between `resourcePicker` and `viewport` playback owners
- `MainWindow` decides which viewport pipeline is active, persists its independent mute state, resolves the camera's
  containing sector, and submits an ambient request
- Ambient loose/archive reads and INDIWV conversion run against an immutable ResourceLocator snapshot in the worker
  pool. MainWindow generation-checks completion before publishing it to `AppSoundPlayer`, so crossing a Sector cannot
  synchronously block fly navigation or starve the current audio sink.
- Camera containment is indexed by `GlView::SectorContainmentIndex`: the all-Sector BVH is reused by normal fly
  navigation and the 125 ms ambience timer, while hidden authoring layers remain part of logical containment. A staged
  level/SED/recovery replacement invalidates this cache before deferred render publication; source identity is checked
  in addition to sector count so an in-place document replacement cannot reuse old bounds.
- Qt 6 playback feeds decoded PCM to `QAudioSink`; Qt 5 retains a `QMediaPlayer` compatibility path

Runtime behavior:
- picker preview is a one-shot, immediate sound
- sector ambience loops and transitions over 500 ms
- same-resource volume changes reuse the current voice; different resources crossfade current/outgoing voices
- decoded ambient PCM is cached in a 64-entry / 64 MiB LRU
- cache identity includes normalized resource roots and archives, preventing cross-project data reuse
- a modal picker may temporarily own playback; viewport ambience resumes after modal ownership ends

The picker remains separate from playback: project-specific sound JLH names select JKDF2/MOTS/IJIM category state,
while a generated editable tree covers projects that do not yet have a JLH/JLL pair.

### Shared desktop styling and property controls

Primary files:
- `src/app/src/ui/components/shared_ui_styles.hpp`
- `src/app/src/ui/components/property_editor_controls.*`
- `src/app/src/ui/components/desktop_input_policy.hpp`
- `src/app/src/ui/components/dialog_window_policy.hpp`
- `src/app/src/ui/components/live_color_popup.*`
- `src/app/src/ui/components/viewport_icons.*`

Behavior:
- shared application styles separate darker title/menu chrome, front-facing panels, and inset list/editor surfaces
  instead of letting each dock or dialog inherit unrelated platform defaults
- line edits, combo boxes, spin boxes, command buttons, inline tool buttons, tab overflow controls, scroll bars, and
  disabled states use common compact metrics and explicit hover/focus/pressed states
- combo and spin controls use embedded, resolution-stable chevron assets rather than platform-dependent arrows
- application-level wheel routing never changes combo-box choices by wheel. Ordinary spin boxes and sliders accept wheel
  edits only while that editor owns keyboard focus. The four Cleave shape numeric controls explicitly opt into hovered
  wheel editing through `sedSpinWheelEnabled`; other property controls keep their focus guard. Qt `WheelFocus` is
  removed at polish/show time, and hovering any blocked value editor forwards the same delta once to its enclosing
  scroll panel when present, so a long
  property/preferences list scrolls without silently changing a value
- ordinary modal/tool dialogs share one caption policy: native minimize/maximize/help buttons are removed and the
  minimum width accounts for the full title. Native file pickers and explicitly app-window-like dialogs opt out; the
  large startup window retains its intentional minimize affordance. Floating Sprite, Sound Class, UNI, and COG editor
  hosts are document windows with native Maximize/Restore controls
- material property rows reserve a stable thumbnail area without drawing an extra button frame over the preview
- color properties are pure swatches and open `LiveColorPopup`, whose hue/saturation/value/alpha changes are previewed
  continuously while the pointer moves
- compact copy/paste feedback is a non-activating top-level popup positioned in global screen coordinates. It remains
  fixed at the action's original screen position if the property list scrolls underneath it, fades in/out, and uses an
  opaque rounded panel with only the pixels outside its rounded path transparent
- auxiliary property editors can be opened by double-clicking their value field as well as by using the adjacent
  browse button
- the Outliner **Things** tab uses a filled isometric generic-object glyph, representing actors, props, rocks, trees,
  and other placed game objects without implying that every thing is a person

### Preferences, keymap, query/find, and history

Primary files:
- `src/app/src/main_window/preferences/panel.cpp`
- `src/app/src/main_window/docks/layout.cpp`
- `src/app/src/main_window/core.cpp`
- `src/app/src/main_window/shortcuts.cpp`
- `src/app/src/main_window/menus/*.cpp`

Implemented behavior:
- UI numeric formatting has an application-owned cached policy in `ui/components/ui_decimal_format.*`, with
  **General > Numbers > Decimal separator** choosing Dot (default), Comma, or System locale. Shared scrub, precise,
  and native spin controls use it without changing `QLocale::setDefault`. Context-owned notifications refresh settled
  manual readouts and resource cells without rebuilding documents or authoring new values. A modified numeric field
  retains its exact raw draft and input locale until it commits during that live edit. Native EDIT preserves raw resource
  drafts, but does not serialize their input locale; restored editors use the current policy. Group separators are omitted and rejected. Decimal-only
  Preferences changes bypass the general viewport/resource reapplication path. Engine/resource/clipboard text and
  native, JSON, and CBOR serialization remain independent of this UI policy.
- built-in Viewport and COG editor themes are embedded and immutable; **Add New** copies the selected palette into a
  named user theme; Add New, Update, and Delete commit the named file immediately, while Update and Delete operate on that named copy
- user themes are stored as one typed JSON definition per theme relative to the active settings file: viewport/app themes
  default to the `data/themes` root, and COG themes default to `data/themes/cog`. Viewport/app definitions under optional
  `data/themes/app` and COG definitions at the shared root are also discovered. `.app.json` and `.cog.json` suffixes
  keep same-name formats distinct; embedded `sed.app-theme.v1` and `sed.cog-theme.v1` tags determine the owning editor.
  Matching per-user application-data locations are used when portable storage cannot be written. The top-level display
  field is `name`, while the complete color set (including COG's `label` color) stays under `colors`.
  New stores require no manifest. COG editor per-color overrides remain independent of saved theme files
- preferences sections use collapsible UI components and default to collapsed
- the Viewport/Rendering dynamic-light preference persists **Gouraud (per-vertex)** by default or the optional
  **Phong (per-pixel diffuse)** enhancement and applies it directly to `GlView` without rebuilding mesh data; the
  choice changes runtime preview evaluation only, not NDY/JED/JKL data or baked texture-vertex lighting
- Architectural vertex markers have a persisted 3–24 logical-pixel size (14 by default), draw once-scaled for display
  DPI, and expose a shared size-derived direct-picking radius. Authored ARCHLIGHTING RGB is the default marker color;
  Preferences can switch to a persisted fallback color, while a dark outline keeps either color readable against the
  viewport. These controls change only the editor overlay.
- Map Settings routes non-editing **Baked lights**, **Thing lights**, **Sector lights**, and thrust-vector visibility
  through separate Editor and Game Preview keys. Editing panels are different: Preview always hides the edit-mode,
  transform, tool/options, UV, Thing-placement, and COG Script editor panes. An epoch-bound snapshot restores exact
  UV splitter sizes, expanded/collapsed Thing geometry, and COG dock width as well as explicit visibility on return to
  the same document. Dock/custom animations are suppressed for that one restore layout pass but remain available for
  normal user commands. Document replacement invalidates the snapshot. Pipeline and deferred refresh guards prevent
  a hidden editor pane from reappearing while Preview remains active
- fog rendering is another independent Editor/Game Preview view preference. A settings-free first run defaults it off
  in Editor and on in Game Preview. Existing settings remain authoritative; valid native SED/recovery EDIT state
  restores the exact per-document pipeline values, while New and engine-format Open reapply the current clean-workspace
  preferences instead of leaking the preceding document's restored fog choice
- Game Preview is a centralized read-only boundary for the live document and persisted editor state. Entry cancels
  transient modification/placement/UV/light-bake previews, disables mutation actions and direct shortcut/signal
  handlers, hides the edit-mode bar, and uses checked Select only as a non-transforming input sentinel. Its actual
  point picker is a separate Auto policy that does not mutate the remembered Editor mode, Surface/Light submode,
  pick mode, exact-edge state, or tool. Camera/render controls, viewport point selection, Copy, and camera-only History
  Go To remain available; marquee/area selection does not. Timeline row activation, COG extraction/editor
  opening, layer persistence, property edits, and edit-mode/submode changes are rejected until Editor mode resumes
- Item Edit publishes one recursive live editability policy for its rows and embedded controls. A Preview point pick
  may rebuild those controls read-only, but returning to Editor recomputes them from the current layer visibility and
  lock state instead of retaining Preview's disabled child widgets
- Viewport Preferences and Map Settings share the enabled-by-default
  `viewport/orbit_around_selection` option. Plain MMB captures the current semantic selection center for that drag;
  deactivation clears any in-flight selection pivot immediately, while Shift-pan, Ctrl-dolly, an empty selection,
  and a disabled option retain fixed-camera-target navigation
- Grid Transform command buttons derive their displayed shortcut suffix directly from each live `QAction`, so keymap
  changes do not leave stale labels. The optional near-plane view-aligned grid guide is render-only: it projects the
  lattice phase onto a safe parallel proxy without modifying the authoritative origin or transform snap frame.
  **Align View to Grid Plane** is camera-only navigation: it maps the active fixed/custom/view-aligned grid axes to
  camera right/up with a deterministic pole fallback, animates the orbit target to the authoritative grid origin while
  preserving distance/projection/orthographic scale and grid state, and occupies JED's collision-free `F2`.
- shortcuts are initialized through `initializeKeymapBindings()` and persisted under `keymap/shortcuts`.
  Map Settings and transform-overlay actions created after that pass restore their saved bindings during creation,
  including explicit empty bindings
- modifier-only actions retain every saved alternative as an independent chord: each modifier in a chord is required,
  and any configured chord may activate the action. Selection Add/Remove/path/filter matching is inclusive, while
  X-Ray cycling, Thing placement mode switches, and transform hold toggles use exact modifier sets. Thing placement
  resolves Free Insert before Surface Tilt when both exact bindings match; an explicitly empty binding disables its
  action instead of restoring a former default. The keymap is normalized into cached modifier masks at startup and
  Preferences publication, so hot viewport input does not reread the keymap or QAction shortcuts per tick. Tooltips
  show all alternatives; status hints identify the matched selection alternative
- default main-row `1`/`2`/`3`/`4` select Sector/Surface/Thing/Light. Shift-number bindings select Surface
  Face/Edge/Vertex, toggle Thing Frame, or choose the project's Baked/Thing/third Light family. JKDF2 has no third family;
  MotS uses Architectural and IJIM uses Sector. The existing Surface letter aliases and Thing Shift+Tab remain. The
  numbered-layout migration replaces only exact former defaults and preserves custom bindings. Numpad camera bindings
  and text/control/resource focus guards remain independent
- the preferences dialog has a searchable, hierarchical keymap page with `QKeySequenceEdit`. The visible labels and
  category paths are user-facing (for example **Viewport > Camera**); hover help explains the action and includes its
  stable internal binding ID. Toggle Grid is exposed even though its QAction is hosted in Map Settings. Each action
  has one leaf, a compact alternative selector, and add/remove controls. Categories and actions are ordered
  case-insensitively by displayed names at every level, retaining their stable IDs and row controls. Row painting
  suppresses hover backgrounds while keeping keyboard focus and tooltips. Align Grid Plane to View retains Numpad 0 and F3
  defaults. Conflict validation compares all alternatives in overlapping execution contexts, including chord prefixes,
  while retaining exact intentional default overlaps. Existing viewport and Map Settings commands are exposed through
  their canonical actions with unbound defaults where appropriate. Persistence remains under `keymap/shortcuts` IDs
- shortcut presentation formats each `QKeySequence` stroke with spaces between modifiers and keys, retaining literal
  Plus keys, explicit Numpad keys, and comma-separated chords. Display formatting does not change sequence identity,
  conflict checks, input routing, or PortableText persistence. Shortcut editors refresh their internal display even
  when their signals are blocked or the same sequence is reapplied. Menu style options format the shortcut suffix
  without changing action captions or bindings, including stylesheet-painted menus. App-owned menus reserve enough
  width for the formatted shortcut column while respecting caller-specified layout constraints
- viewport Mouse Controls preferences store validated physical button/modifier mappings for Pick/Edit, Fly Look,
  Orbit, Pan, Dolly, and Fast Dolly, plus wheel Zoom/Fly Speed modifiers, inversion, and disabling. Startup and explicit
  preference publication cache the map in `GlView`; mouse samples and Fly ticks read no settings. Routing considers
  only modifiers configured on that physical button. Custom Pick/Edit activation modifiers are consumed; additional
  selection/snap modifiers and all default controls retain their existing meaning. Captured button ownership survives
  unrelated releases and Qt's preliminary non-widget window release. The application filter cancels an outside release
  only for an actual QWidget recipient; the final viewport-widget release owns click, marquee, Cleave, and placement
  completion. The camera-drag family can change its mode with modifiers during a drag. Remapping or losing input
  capture cancels unfinished edit/placement gestures. Duplicate
  active chords are rejected before Preferences publication. Horizontal and vertical camera inversion are cached
  default-off settings applied only to mouse camera deltas, without changing editing, keyboard navigation, or wheel
  direction. Wheel scrolling controls are separate from physical-button mappings; Middle button means wheel click.
  Disabled actions cannot become modifier-only mouse modes. Fly Faster and default-Ctrl Fly Slower share the Camera category as
  hold-key actions; slower multiplies movement by `0.5`, and held movement-key releases remain owned regardless of
  intervening modifier changes. Fly consumes its speed-key input rather than passing it into editing commands.
  Zoom In/Out and Fly Speed Up/Down are independent Camera keymap commands, defaulting to Numpad +/− in exclusive
  ordinary-viewport/Fly contexts. Keyboard steps ignore wheel disabling, modifiers, and inversion. Camera-only
  changes retain the cached navigation map and do not mutate the Level or rebuild Thing meshes.
  The Wheel scrolling note has a Keymap prefix and separate Zoom In/Out and Fly Speed Up/Down links inside the same Preferences
  dialog. Each clears the filter, reveals its Camera pair, and highlights both target rows for 500 ms while preserving
  pending changes. One tree-owned single-shot timer restarts on navigation and clears the preceding pair. Compact wheel modifier controls retain readable labels alongside the upper mouse bindings.
  UV hover does not take focus while the owning viewport is flying; an explicit UV click still transfers focus.
  Opening an inline Cleave rank editor completes its label-click ownership before transferring keyboard focus.
- application-owned `ViewportBannerSettings` stores separate duration and RGBA text/background/border colors for
  General messages, Projection, Fly speed, Errors, Cleave Rotate, Cleave Scale, and Placement yaw. Values are validated
  and persisted under `viewport/banners/<stable-id>/`; malformed fields fall back independently. General/Error retain
  caller durations unless their override is selected. Other durations are bounded to 100–30000 ms; both Cleave kinds
  default to 400 ms. The typed settings are cached in `GlView`; input, paint, and timer paths read no QSettings.
  Startup populates the cached settings. Persistence accepts canonical INI boolean strings, and a failed save restores the exact
  previous cached settings, including absent keys. Duration controls preserve full millisecond precision so unchanged
  acceptance remains a no-op. Banner-only Preferences publication saves/publishes only that cache and repaints without
  rebuilding meshes, mutating Level/history, or entering the resource reapplication pipeline. Banner colors remain
  independent of app themes and native EDIT workspace state. Each Cleave sample records its explicit banner kind so
  armed-shape rotation/scaling uses the right timer and palette even when the toolbar mode differs.
- accepting unchanged Preferences returns before the settings/keymap write and live-application pipeline: it does not
  rebuild Thing cubes, invoke viewport setters, remount project archives, reapply layout/layer/outliner presentation,
  or reload every open COG editor. Cancel restores a live COG-font
  preview only when the font controls actually differ from their opening state. A changed active install directory is
  staged on a worker and atomically replaces the resource locator; a single status message appears only if that work
  remains active for 300 ms, so the dialog closes immediately and fast updates do not flash progress. Changed inactive
  project installs are discovered when that project is next opened rather than scanning unused archives on Accept
- **Viewport > Overlays > Extrude preview opacity** persists
  `viewport/surface_extrude_preview_opacity` and applies its 5%-100% value directly to `GlView` (74% default)
- select/translate/scale/rotate and local/world orientation shortcuts are shared across edit modes instead of being
  thing-mode-only; text-entry widgets are guarded so viewport shortcuts do not steal normal typing
- RMB fly navigation claims its rebindable movement chords during Qt's `ShortcutOverride` phase. Consequently
  `Shift+S` remains accelerated backward movement while flying and cannot also invoke the legacy **Snap Grid to Item**
  action; outside fly navigation, `Shift+S` retains that original grid command
- history is exposed via a dedicated dock window and `jumpToUndoDepth(...)`
- window title reflects current path plus `*` dirty marker based on `document_.undoSignature()` vs
  `savedUndoSignature_`
- shared checkbox/message-box styling is now applied centrally rather than widget-by-widget
- Outside-click dismissal of viewport settings follows the active Qt popup's widget-parent chain to establish panel
  ownership and checks the press against that popup's global bounds. Owned combo popups can accept choices outside the
  panel rectangle; an unrelated popup still counts as an outside press.
- process-wide Qt diagnostics are written to `sed.log` beside `sed.ini`. Each record carries severity plus one of four
  filterable categories: **App** for editor workflows and uncategorized application records, **GUI** for Qt, native
  window-system, window, widget, and message-box diagnostics, **Core** for portable validation/edit operations reported
  at the app boundary, and
  **Resources** for lookup, parsing, previews, models, textures, sprites, and audio. All categories default on
- Preferences exposes a master **Enable file logging** switch, the four independent category switches, minimum detail,
  and bounded rotation size. The master switch is a quick global stop and does not erase the user's category choices.
  When it is off, the dependent minimum-detail, category, and rotation controls are disabled until file logging is
  enabled again
- a one-button Warning/Critical notification is logged by the GUI filter unless an explicitly logged operation sets
  `sed.logging.skip`. Suppression is explicit rather than a time/subtext heuristic, so unrelated messages cannot hide
  each other. A multi-choice dialog is not logged merely because it appeared; its
  resolved choice is logged instead. Discard/destructive outcomes are Warning, affirmative/Save outcomes are Info,
  and Cancel/No outcomes are Debug. Callers can attach `sed.logging.context`, such as the active level path and intended
  operation, and `sed.logging.destructive` marks an affirmative result as Warning for destructive workflows. Debug
  detail also records tool-rejection diagnostics without making logging part of editor correctness
- `Edit -> Find...` and `Find Next` provide a current-mode Pascal-style level query workflow with:
  - sector/surface/thing scopes
  - field-specific operators
  - browse helpers
  - focus/add/subtract result modes
  - selection-only scope
  - per-kind remembered criteria
  - no entity dropdown
  - disabled state outside supported edit modes

### Autosave and crash recovery

Primary files:
- background format/storage helpers: `src/app/src/ui/recovery/document_recovery_utils.*`
- `MainWindow` scheduling, generation gating, prompts, and publication: `src/app/src/main_window/document/recovery.cpp`
- save/open/new/close integration: `src/app/src/main_window/menus/file_menu.cpp`,
  `src/app/src/main_window/project/startup.cpp`, and `src/app/src/main_window/lifecycle.cpp`

Behavior and ownership:
- the GUI thread owns `Document` and takes one coherent value snapshot plus the RFC 8949 CBOR editor-state capture;
  complete native SED serialization, undo-timeline expansion, compression, disk output, flush, and publication run on
  a worker;
- Recovery V1 is a discovery/autosave envelope whose single authoritative `docm` chunk is one complete native SED V1
  through V4, selected by the captured Level's explicit texture-pixel or normalized UV-space tag and retained MotS
  arch-lighting objects or authored bake baselines.
  That document contains the lossless Level plus checked `edit` editor/COG/resource state and `hist` undo/redo sections,
  independent of whether the runtime export is JKL, NDY, or legacy JED. Recovery keeps no separate outer copies of
  those semantic domains;
- restored session state includes multi-selection and its active item; edit/surface/light mode; Editor and Game
  Preview cameras (orbit position/orientation, projection, FOV, orthographic scale, clipping); active pipeline;
  geometry/lighting/Thing render modes; grid, X-Ray Selection, orbit-around-selection, thrust and light-gizmo
  settings; transform tool/orientation/grid snapping; layer visibility/locks/colors; preview overlays; window/dock and
  tab layout; every named persistent splitter; UV repeat/snap/grid controls; active placement/modification tools;
  Thing placement/brush settings; every staged Multi-cut line and target/authored-camera context; and open COG and
  resource-editor buffers including unsaved source text, editor state, and each COG tab's editor/outline pane split;
- Both Surface and Sector Multi-cut workspaces retain their settled drafts, options, line selections, and independent
  line-only Undo/Redo timelines/cursors as durable EDIT state.
  They survive ordinary tool/edit-family changes, Multi-cut off, and Preview entry without capturing commands while
  dormant. Restoration preserves the saved document mode/selection and does not arm Cleave merely because cuts exist.
  Legacy current-only records start a fresh local baseline when restoration admission permits; otherwise only the
  current draft is retained. Apply workspace mementos and historical Layer visuals use
  an optional checked `workspaceHistory` sidecar bound to adjacent HIST states, independently of resource/COG histories;
- authored pending-cut changes participate in native dirty state, close/replacement prompts, recovery offers, periodic
  archives, and async Save/replacement fingerprints even when the Level is clean. The saved-baseline projection includes
  settled geometry/frames/links, targets, order, Chain origin, and Bounded while lines exist. Selection/camera/guides,
  future shape/angle settings, and a history cursor with unchanged authored lines remain editor-only. Native Save
  checkpoints the exact captured draft as clean without requiring Apply. Settled authored revisions invalidate a cached
  projection; mouse/title/focus/camera paths do not add full-Level copies or hashes;
- editor-state V1 validates numeric ranges, enum values, booleans, object references, layout blobs, splitter records,
  and COG/resource-buffer fields before applying authoritative state. Main-window geometry, both dock hierarchies, floating-dock
  positions, and every named persistent splitter are captured; transient modal dialogs and an in-progress pointer drag
  are intentionally checkpoint boundaries rather than replayable windows;
- each saved document has a stable normalized-path identity. Untitled projects receive a session UUID. A readable
  filename combines the display basename, project tag, explicit `recovery` marker, and one bounded 26-character
  lowercase-base32 128-bit identity/session family token. SED
  prefers portable `recovery/` beside `sed.ini`, verifies that atomic
  replacement works there, falls back through per-user local application data, the user's `.sed` directory, and a
  last-resort temporary location when needed, and scans every known fallback/legacy location on later launches;
- a generation gate prevents an obsolete worker from replacing newer state. A short publication mutex closes the
  final generation-check/rename race with normal Save and clean-close cleanup. Same-document edits queue behind the
  single active writer without invalidating it, so continuous edits cannot prevent the first coherent checkpoint from
  becoming durable; the queued newest state replaces it immediately afterwards. The canonical publication key tracks
  undo and redo stack identities independently in addition to the Level and `edit` digests. Undoing all the way back to
  an already-published Level therefore still publishes the new redo-only timeline, while the window's clean/dirty state
  uses the undo-side saved position together with saved authored Level/buffer/Cleave baselines;
- `QSaveFile` writes a sibling temporary file and atomically replaces the previous complete snapshot. Direct-write
  fallback is disabled, so an unsupported atomic replacement fails without damaging the older recovery;
- ordinary SED/JED/JKL/NDY Save uses the same no-fallback atomic staging policy through a buffered `std::ostream`.
  SED Save additionally captures the recovery-grade `edit` workspace/COG/resource payload on the GUI thread and expands the
  `hist` undo timeline on the save worker. History uses a semantic shared fragment pool of explicit `HistoryNested` SED
  Levels plus canonical reference-list splice deltas and independent 16-state checkpoints, rather than storing one
  independent complete byte string per state. New writers use `sect-record-v1` for the large `sect` frame so unchanged
  Sector records remain reusable when neighboring records move; other sections use fixed 8192-byte fragments, a bounded
  writer-only hybrid shifted-tail layout may resynchronize safe size changes, and oversized record layouts deterministically
  fall back to explicit `fixed-v1`. Adjacent states reuse exact generated section frames, with invalidation derived from
  the actual undo Snapshot variant and broadened whenever `rsrc` changes. SED Open restores both optional sections and suppresses
  the ordinary camera reset/refit, while engine/legacy formats retain that reset behavior;
- COG and resource editor Save use atomic loose-file replacement. Recovery is removed only after a commit succeeds, so
  a failed or interrupted save preserves both the old source file and its latest checkpoint. Saving a Level while
  COG/resource buffers remain modified keeps the old checkpoint until its post-save replacement has durably published;
- open loose COG and resource buffers use normalized source identities across ordinary Open, SED EDIT restore, drafts,
  and post-Save path changes. A draft cannot be saved over another live tab with that identity, so native Save cannot
  capture duplicate external-resource ownership;
- after native SED restore, a loose COG or resource source changed since the SED checkpoint is kept as the captured
  buffer and offers the named Reload/Keep decision through the same serialized external-change queue used by clean
  and modified open files. Keep retains the embedded buffer and its history without rewriting the source;
- every native SED semantic section independently owns one compression attempt only in the inclusive 4 KiB..64 MiB
  range (retained only if smaller), one expanded
  SHA-256, and stream-major fixed 4096-byte slots whose last four bytes are CRC32C. Each stripe is a systematic GF(256)
  Vandermonde/MDS code with adaptive `k <= 16`: required `head`/`rsrc`/`sect`/`thng`/`tmpl`/`cogs`/`lite`/`hnod`,
  core-owned `jimp`, and recovery-critical `edit` use `p=4`, while lower-priority `hist` and future optional sections use
  `p=2`. HIST is
  intentionally optional and discardable when a stripe exceeds its repair budget, so p=2 adds 12.5% parity relative to
  a 16-slot data stripe instead of p=4's 25% and keeps normal Save/Open work lower. The current Level and `edit` (which
  may contain the only unsaved COG source) remain authoritative and retain p=4. The guarantee is `p` erasures per
  independent stripe. Native SED has no
  FEC tables, whole-file CRC, or duplicate stored/raw CRCs. Checked 100-byte `sectv1\n\0` primary and 116-byte
  `sedmir1\n` mirrored descriptors protect framing and provide resynchronization anchors. Nested history Levels use the
  explicit raw `HistoryNested` profile: inner descriptors and payloads retain CRC32C, while the enclosing `hist` section
  supplies the one expanded SHA-256/compression/parity layer. Header/descriptor CRC mirrors sit outside payload MDS,
  alignment and unused slot-tail padding are canonical zero (strict rejects
  nonzero; corruption recovery normalizes/reports it), and the footer is a marker. V1 has no descriptor
  directory or record-group shards, so an over-budget stripe loses that complete semantic section. Fixed-slot repair
  handles byte/bit substitutions and unreadable/truncated ranges only while checked framing preserves the absolute
  4096-byte phase; a mid-section insertion/deletion shifts later boundaries and is not guessed or rephased;
- the outer recovery envelope does not recompress or parity-wrap `docm`. It adds primary JSON discovery metadata, a
  parity-protected `meta` mirror, and optional `warn`. `docm` is raw native `.sed` with the outer native flag and zero
  outer CRC/SHA fields; only `meta` retains the outer 16+2 P/Q/table/IEEE-CRC-32 scheme. If the `docm` chunk header is
  damaged, scanning recognizes native framing and invokes strict or corruption-aware native recovery;
- writer-side metadata/project-type, size, arithmetic, and stream-status invariants reject an inconsistent generation
  before it can replace the last good checkpoint. Parser/restore exception boundaries reject hostile or badly corrupted
  files without terminating the editor, and chunk/count limits bound hostile allocation. Before staging, the worker
  validates every generated outer chunk, requires an exact current-Level round trip and exact optional payload bytes,
  and checks the canonical HIST envelope. The recovery worker also performs the bounded compact-HIST semantic admission
  used by Open before publishing the optional timeline, so a stream that would be rejected on the next restart is omitted
  with an explicit warning. Any remaining reader-side failure drops only optional history, never the independently
  checked current Level or `edit`;
- native recovery carries `optional<ProjectType>` plus `projectTypeTrusted`; a checked fixed header or `head` supplies a
  trusted identity, while loss of both leaves `Level.kind` as a non-authoritative placeholder. Project-dependent history,
  resource setup, and native text export must wait for explicit selection or stay blocked;
- launch scans valid snapshots newest-first. Opening a matching level checks its exact slot. A source-file conflict is
  reported rather than used to hide potentially unique unsaved work, and recovery opens as dirty state that still
  requires an explicit Save;
- successful normal Save, explicit recovery discard, and clean application close invalidate any worker before removing
  the relevant live slot, including matching legacy-directory slots. Open/New Project Discard retires only that live
  slot; periodic `.sedautosave` history is durable user data and is removed only by the explicit Autosave History
  Delete action or post-publication retention pruning. Repaired/partial input gets a unique `.corrupted-*`
  forensic copy while the selected original stays discoverable until a clean canonical checkpoint publishes. Structured
  loss of a required Level section or an unidentified lost unit also schedules a non-authoritative project-native
  JKL/NDY plus provenance/limitations manifest beside the forensic input; identified optional-only `jimp`/`edit`/`hist` loss
  does not. Unidentified manifests record exact uncertainty rather than inventing a type. These collision-safe artifacts never
  overwrite existing files and never replace the forensic binary. They contain only completely decoded semantic domains
  and defaults; the binary remains the only source for bytes in a lost section. A verbose `recovery.log` records repair
  details;
- ordinary Open builds the candidate project's roots, archives, and locator in detached storage; the active document's
  resource state remains installed throughout parsing, modal confirmation, and final authorization. Candidate resources
  publish only at the no-event-pump Level commit boundary. Direct `.sed` Open additionally defers even that staging until
  strict/recovery parsing succeeds and required or unidentified loss is accepted. Repaired
  complete/identified-optional-loss documents remain associated but modified; required loss and any unidentified unit
  open detached as `Untitled*`, because damaged descriptors may conceal a required domain. Cancel preserves the old
  document, path, resources, and selected source. If checked project identity is unknown, one combined choice/confirmation resolves it
  before history decode or resource setup. Direct salvage stays beside the unchanged source and a worker re-read must
  match captured metadata, normalized loss, unidentified count, and project classification before export;
- ordinary reads fall back to three independent 4096-byte attempts only after a real short/I/O read. Persistently
  unreadable ranges are zero-filled as erasures, coalesced and reported, allowing later native stripes/sections to be
  recovered. Corruption-only results name missing required/optional sections and default or omit each lost domain as a
  whole. Checksum-valid required Level schema errors remain hard failures; invalid application-owned `edit`/`hist`
  recovery payloads are rejected atomically and fall back to default workspace/no history with warnings and a clean-save
  requirement, without vetoing or detaching an intact Level;
- when recovery is enabled, startup/document switch first publishes a clean baseline; this is required because a hard
  process kill cannot execute cleanup or crash callbacks. Authored Level/COG/resource/history mutations use a 650 ms debounce
  before capture/serialization. Editor-only camera/tool/layout changes only mark the live checkpoint as required and
  wait for the periodic recovery tick (or an explicit shutdown flush), so they do not launch a full Level+HIST encode
  after every orbit/fly gesture. Editor-only changes update this live crash checkpoint without marking the document
  dirty, setting automatic-prompt eligibility, or rotating periodic `.sedautosave` history. `edit` represents stable
  workspace/tool configuration, staged Multi-cut lines, and unsaved COG/resource-editor
  buffers, not every live input transaction: an active Surface Extrude preview and a partially held brush/draw or other
  pointer gesture are finalized or cancelled at snapshot/apply boundaries. Preferences' 1–30 minute interval remains a fallback. Transient
  capture/publication failures retry with bounded exponential backoff. A direct
  GUI-owned snapshot capture is deferred while RMB fly navigation is active and resumes after navigation ends; the
  pending checkpoint is retained rather than discarded. A direct Qt session-shutdown callback forces one final coherent
  capture, drains queued generations, and makes two immediate
  retries after a failed attempt (three attempts total). Destruction paths that bypass `closeEvent` also force the
  final capture. Windows
  WER/restart, SEH, and `std::terminate` minidump hooks record crash diagnostics but
  never serialize compromised Qt/editor memory;
- a transition back to the exact saved state invalidates a stale dirty writer and replaces it with a clean workspace
  baseline; COG/resource tab destruction participates in this transition, preventing deliberately discarded source text from
  being offered after a crash. Clean application close removes the checkpoint. COG/resource pages are explicitly destroyed in
  the derived `MainWindow` destructor while the editor map and recovery members are still alive, avoiding QObject
  base-teardown callbacks into destroyed C++ members. Active Fly navigation is cancelled in that same derived
  destructor before member teardown, so synchronous navigation-state receivers still have valid MainWindow state;
- `meta.requiresUserRecovery` separates a durable clean hard-kill baseline from an automatic unsaved-work offer. Before
  each periodic authored-data replacement, the previous checked dirty snapshot is captured and atomically archived without pruning;
  the staged live `QSaveFile` commits second, and pruning to 10 entries per identity/session family happens only after both
  durable outputs exist. A successful native Save gate-retires the stale dirty live offer immediately after its atomic
  SED commit, preserves periodic history, and republishes a non-prompting clean hard-kill baseline asynchronously. File >
  Open Recent > Autosave History browses/deletes those points. Target recovery choice, current Save/Discard/Cancel, and replacement
  commit are separate phases, so cancellation or load failure cannot retire either durable family. Successful
  recovered/ordinary Open and New Project gate-retire exact superseded live paths so an in-flight writer cannot recreate
  them. Startup Discard deletes only the selected session; older same-identity live sessions are suppressed for this run
  while remaining available through history and on the next process start;
- replacement authorization binds every COG/resource prompt to the exact authored state resolved by that answer and validates
  the complete tab set again after all nested modal event loops. Open/recovery also rechecks the authorized Level,
  COG/resource, and authored-Cleave fingerprint immediately before installation, so timer/plugin edits made during a long load abort instead of being
  overwritten;
- prompt eligibility compares modified COG/resource buffer identity/content with a separate saved baseline instead of using the
  raw editor `modified` flag. Native SED Save can therefore embed still-modified external buffers without immediately
  generating another offer/history point; later text changes remain eligible, and resolving one tab removes only
  that tab from the baseline. Pending cuts have a separate cached saved authored projection, including Bounded only
  while lines exist, so native Save can checkpoint them before Apply and later changes remain prompt-eligible.
  A recovery selected by Open is scoped as
  protected while the current document's Save branch runs, so same-session cleanup cannot delete the input before apply;

Limits:
- the synchronous coherent `Document` copy and bounded CBOR workspace capture are the only autosave work on the GUI thread. The
  live history uses compact dirty-domain snapshots and restored history is compacted again, but copying the document
  and background expansion/compression of a very deep history can still be measurable on exceptionally large maps.
  Native Open consumes HIST directly into compact snapshots instead of first
  retaining one complete Level per history entry. During Save, nested Levels
  emitted by the trusted writer use framing-only section splitting; untrusted
  files still undergo complete semantic validation before their boundaries
  are exposed. This removes one redundant full-Level decode per history state
  without changing V1 bytes or recovery checks. Recovery/autosave parsing also
  hands its already validated native Level to the consuming install transaction
  instead of decoding `docm` twice; known-project HIST and consumed DOCM byte
  buffers are released once their compact/decoded forms become authoritative.
  The HIST consumer validates every reconstructed nested frame through the
  strict native framing/checksum/hash parser, but rematerializes a semantic
  domain only when its complete checked frame differs from the preceding
  accepted state. It always decodes `rsrc`, expands a catalog change to all
  indexed domains, and repeats whole-Level graph/catalog validation. It moves
  the already reconstructed byte vector into the accepted trust context only
  after success, avoiding a second full-state byte copy.
  A partial recovery with required or unidentified Level loss retains its raw
  `docm` only until the background last-resort text-salvage worker has captured it.
  Admitted file reads reserve/resize once, large atomic writes bypass the small
  stream staging buffer, and output vectors grow geometrically between sections.
  Periodic/debounced capture is deferred during fly navigation, and generic mouse/key releases compare the compact
  EDIT hash first so no-op interactions do not trigger the full copy;
- `TerminateProcess` cannot execute a handler; a hard kill can therefore lose the short interval after the last
  successful live checkpoint. Storage/device failure can still defeat an atomically replaced local file;
- more than four checked erasures in one required-Level, `jimp`, or `edit` stripe, or more than two in one `hist`/future
  lower-priority optional stripe, cannot be corrected. Expanded SHA-256 detects CRC-consistent corruption but cannot
  localize it for MDS repair; healthy sections remain recoverable and the damaged binary is preserved.

### Consistency and resource checking

Primary files:
- pure collectors and issue/navigation model: `src/app/src/ui/checks/level_check_utils.*`
- dirty relevance mask: `src/app/src/ui/checks/check_rebuild_utils.hpp`
- dirty scheduling and checks-tree adaptation: `src/app/src/main_window/checks.cpp`
- searchable project-aware rule dialog: `src/app/src/main_window/checks.cpp`

Ownership and behavior:
- the collectors own deterministic rule evaluation; `MainWindow` owns only the 200 ms dirty debounce, tree rows,
  counters, and navigation metadata adaptation
- the rule table covers the complete active Pascal `Cons_checker.Check` and `CheckResources` surface: sector/surface
  structure, flags and geometry, adjoin reverse/overlap/visibility, Thing sector containment, typed COG resource
  extensions, referenced MAT/CMP/WAV/template/COG availability, and the project CMP-count limits
- defensive C++ additions report malformed indices/array counts and non-finite/out-of-range Thing placement without
  unsafe geometry access, plus missing/inherited template SPR/3DO resources and underscore-prefixed base-template use
- legacy severity is retained: missing/mismatched reverse adjoins and missing legacy MAT/CMP/WAV/template/placed-COG
  resources are Errors, while non-planar/non-convex surface reports are Info. The extra inherited SPR/3DO checks warn
- normal-length comparison uses the legacy absolute `1e-4` tolerance (`< 1e-4` passes; `>= 1e-4` warns)
- the expensive sector-pair overlap pass defaults off and is enabled through the Consistency settings dialog, matching
  Pascal `CheckOverlaps`; enabled evaluation skips directly adjoined pairs and emits one Info row per sector
- `checksDirtyIsRelevant(...)` owns the rebuild mask; `DocumentDirty::Cogs` participates alongside geometry, Things,
  lights, and structure, so COG value edits refresh checker output
- focused direct tests encode the rule boundaries, including the valid-sector base-template fixture, normal threshold,
  adjoin match/mismatch, overlap default/opt-in paths, COG values, resource failures, and CMP limits. These checks are
  part of the registered Release regression suite

### Docked COG authoring

Primary files:
- `src/app/src/main_window/cog/editor.cpp`
- `src/app/src/main_window/cog/scripts.cpp`
- `src/app/src/ui/cog/*`
- `src/app/src/main_window/properties/panel.cpp`

Behavior:
- COG Open into a new buffer admits the selected source to 16 MiB before text copies or archive working-copy
  extraction. Oversized winning sources reject without trying a shadowed resource; bounded read failures are reported
  before installing a tab or creating extraction output. The `.cog` suffix lookup is attempted only when the original
  name is genuinely missing, not after an oversized winner or read failure. First loose Open and every Reload use
  complete bounded source capture. Metadata-only resolution can instead activate an already owned winning loose
  buffer or selected archive working-copy buffer without rereading its payload. This requires an actual live page;
  an existing working-copy file alone does not bypass new-buffer admission. Archive origin is captured before
  extraction changes the loose candidate set.
- COG scripts open in a docked `COG Scripts` tool with tabs, exposed by Tools > COG Script Editor.
  Its group toolbar owns New, Open, and Save with a Save All dropdown; each page keeps its own Undo/Redo,
  Whitespace, Format, and Insert controls. Save All replaces each modified file atomically and stops on cancellation
  or failure while preserving selection and later drafts across every registered COG host. Individual script tabs
  transfer the same editor and QTextDocument between same-owner COG hosts; primary aliases remain stable. Floating
  hosts retain native captions with Maximize/Restore and Dock return controls. Strict optional host metadata preserves
  topology, active selection, and reachable floating geometry independently of authored source and compact document history.
  File transfers hide an emptied primary host while retaining its registry and Tools reopening path; emptied auxiliary
  hosts retire after native layout animation completion. This cleanup is transfer-scoped, not a general empty-group
  visibility policy.
- Ordinary loose COG Save checks the exact current source bytes against the accepted SHA-256 baseline before
  format-on-save or mutation. Drift, or an unknown source baseline, offers Reload from Disk,
  Overwrite Disk, or Cancel. Reload rereads bounded latest bytes; Cancel preserves the exact buffer/history/caret.
  Watcher Keep acknowledges a notice without rebasing Save. Only successful Reload or committed Save advances the
  optional persisted `acceptedSourceHash`; malformed present hashes reject the EDIT workspace atomically.
- Saving a COG script reparses its public `symbols` section synchronously and updates every placed instance using that
  script. A matching name and type at the same declaration occurrence keeps the placed authored payload, new declarations
  receive their schema defaults, and removed declarations are discarded. A valid empty schema clears the placed argument
  list; missing, unreadable, or malformed schemas preserve the previous placed values and report the failure. On
  Windows, script identity matching normalizes path separators and case before occurrence matching, so equivalent native
  path spellings still refer to the same loose script.
- A changed schema is one undoable Level transaction across all matching placed instances. A no-op schema refresh adds
  no history entry, and undoing it changes the Level only; it never rewrites the loose script. External loose-file and
  directory notifications are debounced and bound to the current document/resource identity, so stale callbacks cannot
  update a replacement project. Deletion leaves in-memory instances intact without recreating the file, while a
  directory watch notices a later reappearance.
- tabs are movable and pinnable; pinned tabs stay grouped on the left, retain fixed pin/close layout slots while
  visually showing only the pin, and must be unpinned before closing. **Reveal in Explorer** is limited to existing
  authored loose files. The tab RMB menu provides **Close Others**, **Close to the Right**, **Close Saved**, and
  **Close All**. Every bulk command follows the visible tab order, skips pinned tabs, prompts modified buffers through
  the normal Save/Discard/Cancel path, and stops before later targets when Cancel is chosen
- the editor supports diagnostics, separate splitter-linked Symbols/Code outline lists, case/whole-word find,
  selected-identifier reference marking outside strings/comments, formatting,
  autocomplete, insert helpers, and go-to-definition
- message completion, highlighting, and diagnostics use a project-specific engine vocabulary: JKDF2 uses the base
  OpenJKDF2 registrations, MotS adds only the `Main_bMotsCompat` handlers, neither includes `Main_bDwCompat`, and IJIM
  retains its independent established handler table
- Find and zoom share one reserved bottom chrome line inside the editor, so showing Find cannot resize source text;
  Find uses temporary cursors for incremental searches from a stable anchor and disables navigation for empty/missing matches.
  The complete visible chrome contributes a source-editor minimum width, preventing narrow dock drags from overlapping
  controls, while an internal stretch pins the Find hide action to the far-right edge;
  Format is a menu-button whose main action applies current settings and whose inline menu edits them, while Insert is
  an icon-only dropdown at the end of the document toolbar. The group New/Open/Save controls and document
  Undo/Redo/Whitespace/Format/Insert controls use the shared white/cyan rounded vector glyphs at their existing
  20-pixel icon size and 30-pixel button height
- source edits debounce the complete-document outline/diagnostic pass and reuse one text snapshot. Syntax highlighting
  classifies identifiers in one token pass using cached semantic-name sets instead of compiling symbol-sized regular
  expressions for each block
- the collapsible, vertically resizable bottom diagnostics pane has an animated full-width disclosure header with
  inline error/warning counts and line/column locations; its completed disclosure state and splitter height are
  preserved in SED/recovery editor state without publishing intermediate animation frames
- diagnostics mark source in code/flags/symbols directly with severity-colored wave underlines and expose a
  copy-message context action
- terminated `call` and `goto` statements may be followed immediately by one or more closing braces; the trailing-text
  diagnostic still rejects anything other than the semicolon, whitespace, and structural braces
- the product-owned **SED Dark** syntax theme distinguishes local versus non-local symbols, label variables,
  message handlers, function calls, predefined globals, the standalone `flags` keyword, and the two section boundaries.
  The common engine-backed script-flag model exposes bits `0x001` through `0x200` for every current project type,
  identifies pulse/timer state as runtime-managed, and leaves unknown bits available for lossless UI preservation.
  Its dedicated value parser mirrors the engine `%x` contract: both bare and `0x`-prefixed tokens are hexadecimal,
  the full A-F digit range is accepted, overflow is checked against `uint32`, and editor output is canonical padded
  `0x%08x`. The independent symbol `mask` parser keeps its existing decimal/hex and `-1` compatibility behavior.
  Diagnostics reject authored `/*` and `*/` delimiters because COG supports line comments only, while the
  position-preserving analysis sanitizer excludes strings and existing `#`/`//` comments from that check and from
  code-label classification, so punctuation inside quoted output text cannot become script structure. Retired built-in
  theme ids are canonicalized when preferences are read, so existing installations retain their palette without
  exposing a third-party product name
- context-sensitive Insert captures the source cursor before menus take focus, hides Symbol Attribute off declarations,
  revalidates modal edits against the live document revision and identifier, and groups paired symbol/handler insertion
  into one undo unit. The flags editor uses the shared project schema, preserves runtime/unknown bits and both comment
  syntaxes, and is reachable from Insert or the recognized flags-line context menu
- the configurable Insert key uses the focused source caret: Symbols opens the existing type menu, and Code between
  handlers opens the existing message/custom-label menu. Within an active handler, Insert retains native text behavior.
  Sanitized structural context is cached by text revision; keyboard focus actions are claimed before viewport
  accelerators, so Insert cannot toggle Grid Always Behind from script text. Add Symbol, Add Message, Add Code Label,
  and Edit Flags also expose unbound COG Script Editor actions using those same transaction-safe flows
- COG code-font family fallbacks/style come from an editable type-ahead installed-font combo, default to
  `Consolas, Courier New, monospace`, and apply to existing and new editors. The closed combo ignores wheel changes,
  while its open font list retains normal wheel/touchpad scrolling. Presentation zoom is tab-local EDIT state restored
  from SED/recovery; the compact control occupies a
  reserved lower-left strip within the editor frame, keeps an application-UI font independent of source scaling, uses
  a compact right-aligned percentage field, and
  remains fixed at the left while Ctrl+wheel over source applies clamped 10% steps. Changing font or zoom affects only
  widget fonts, not document text.
  The optional Symbols/Code outline split extends EDIT state
  compatibly; older records retain the default split
- reverse `COG links` in sector/surface/thing properties navigate into the active COG/variable context
- `COG links` are shown in a dedicated collapsible list at the bottom of the item editor instead of being mixed into
  the tree
- Thing parameters use their own collapsible, vertically resizable list between Item Edit and `COG links`; its
  persistent search field is the first control and uses an embedded magnifying-glass cue. The section uses a flat
  compact header and inset splitter divider rather than another field-like outer border
- `New` from the COG outliner or **Edit > COG > New COG Script** opens an unsaved in-memory draft, adds a placed COG,
  and selects it. The initial buffer
  contains only empty `symbols` and `code` sections. Creation never writes a file or directory; the first explicit
  Save resolves to `<project directory>/cog/<name>.cog`, or the configured loose-resource root's `cog` directory for
  an Untitled project; no native project Save or destination chooser is required
- draft identity, contents, modified state, cursor, pinning, diagnostics disclosure/split, and dock tab state use the normal COG recovery/SED EDIT payload,
  so an untitled-project draft survives recovery without pretending that its virtual identity is a disk file
- loose script identities are normalized case-insensitively, and saving a draft is refused when another tab already
  owns the resolved path
- the code editor routes RMB through the custom editor context menu instead of the stock Qt menu
- new message declarations are inserted after the last existing `message` declaration in `symbols`
- whole-line copy/paste works when no selection exists, and `Shift+Enter` inserts a real new line

### Docked resource authoring

Primary files:
- `src/app/src/main_window/resources/editor.cpp`
- `src/app/src/ui/resources/resource_authoring_editor.*` for common chrome, source identity, and persistence
- `src/app/src/ui/resources/resource_authoring_pane.hpp` for the private pane declaration
- `src/app/src/ui/resources/resource_authoring_sprite.cpp` and `resource_authoring_tables.cpp` for the independent panes
- core format APIs under `src/core/include/sed/assets/`

Behavior:
- three persistent native docks own homogeneous Sprite, Sound Class, and UNI file tabs. The major groups are tabified
  at the top by default with one visible group and can dock or float independently. The workspace uses North tabs in
  all four native dock areas. The title/grip remains usable after top-area
  redocking; its context menu offers only Undock while docked and no menu while floating. A file tab can detach, redock,
  or join another host of the same kind. Floating hosts use decorated native captions with Maximize/Restore and a Dock
  return control; docked hosts restore their custom title/grip. COG uses a separate homogeneous host registry with the same file-tab
  transfer behavior and its existing native titlebar.
  Transfers move the existing pane and preserve raw draft, local undo, source identity, external modified state, and
  recovery ownership. Tools contains direct Sprite, Sound Class, UNI, and COG Script Editor commands with icons;
  their dock toggles are excluded from View > Windows. Matching window icons also appear on native grouped dock tabs.
  Opening a group does not create a file. File tabs show only the basename and modified marker; a cheap source
  tooltip carries the full loose path or `<archive basename>: <case-preserved entry path>` without capturing the pane
  or repeating a label inside it. Archive metadata resolves once at Open using the winning source and is cached as
  optional `archiveTooltip` in resource-pane/COG recovery records. It is display-only, bounded to 32768 characters
  without NUL, and admitted only for archive sources. Native restoration keeps the recorded origin even without the
  original mount; legacy records retain their existing fallback. Successful loose Save clears the origin, while
  cancellation or failure preserves it. COG working-copy paths and normalized ownership remain separate.
  Scoped resource bodies use a lighter dark canvas than their titlebars; native header drags can split docks at their
  edges. A completed native center drop onto the effectively visible same-kind editor joins the existing file widgets
  into its document tab host, retaining independent histories, pin order, the dropped current file, and stable primary
  aliases. A visible other-kind target retains native editor-window tabification even when a hidden same-kind peer
  shares that window group. Completion waits for the actual native plug transition and validates context, ownership,
  and public Qt tabification before transferring any file.
  Gesture-scoped Join feedback is mouse-transparent and resolves only an effectively visible same-kind target,
  including an open empty group. Cancellation, context replacement, owner/target destruction, or terminal release
  clears it. Moving the last file out hides a primary source without unregistering it; Tools reopens that empty group.
  Empty detached sources retire after the transfer, preserving the destination documents.
- each group host owns one compact New/Open/Save icon toolbar; embedded pane chrome hides its duplicate buttons.
  New and project-resource Open remain usable in an empty group and add a file without replacing the caller's draft.
  Cancelled or rejected Open leaves existing tabs intact. Fresh New defaults remain clean; genuine authored changes,
  including pending raw numeric text, enable Save. Save targets that host's selected pane, including toolbar focus
- the Save dropdown's Save All snapshots modified buffers of its kind across primary and detached hosts, skipping
  unchanged file tabs. Empty and all-clean groups disable Save. Each modified file is
  validated and replaced atomically; cancellation or error stops the sequence without closing buffers or clearing later
  modified markers. Earlier successful files remain saved, unrelated kinds remain unchanged, and global selection is
  preserved. Document-context and guarded pane identity checks reject stale callbacks
  The dropdown remains usable for modified peers when the selected file is clean; its plain Save click then writes nothing
- New creates a named virtual draft without touching the filesystem or Level. Project resources, including archive
  entries, are read into memory; archive data is never extracted. Reopening a normalized source already represented by a
  tab activates its existing buffer instead of replacing it
- each resource page contains document Undo/Redo buttons separate from its group file toolbar. Availability queries
  inspect the compact command cursor without capturing complete drafts. Explicit buttons use document history even
  while a native text field retains focus; keyboard routing retains native text undo priority. The configured Edit
  bindings are the only keyboard actions, and button tooltips follow their actual bindings
- explicit Save/Save As validates the chosen format and atomically writes a loose resource. Virtual drafts ask for a
  loose-file destination. Normal archive Save resolves the project's corresponding loose
  resource category and safe basename; path and case-insensitive tab collisions are checked before replacement
  and a brief success notice follows the committed loose identity. Explicit Save As retains its chooser
- resource pages derive their UI profile from the active project. Unsupported imported tokens and untouched source
  floats remain intact; constrained controls do not coerce them during refresh, tab movement, or unchanged Save
- SPR pages own a copied resource locator and isolated preview Level, so interactive preview updates cannot mutate the
  caller's Level or shared caches. Material animation caches decoded MAT cels by material/CMP revision, then updates
  only the isolated preview texture. Hidden panes pause their timers; FPS 0 pauses playback. Bounds, FPS, paused playback,
  splitter sizes, and full preview camera framing are presentation-only and captured in the optional per-pane EDIT map;
  the current MAT cel is derived at runtime. None of these choices change SPR bytes or authored dirty state. Dimension
  and XYZ edits preserve orbit/target/pan with expand-only framing; Recenter fits the card and displayed bounds. Classic
  camera-facing types interpret XYZ as camera right/depth/up, while IJIM fixed type 2 ignores offsets. Clipping choices
  remain unchanged; impossible depth reports a status while retaining camera and raw authored values. Bounds starts on
  without a saved override, and ignored IJIM fixed-type offset rows are hidden. Numeric scrub updates are coalesced at
  33 ms and change the isolated geometry cache without rebuilding the level mesh cache
- IJIM Sprite Preview geometry is independent presentation state, with global modes 0–4 (Not drawn, Vertices,
  Wireframe, Solid, Textured) and default 4. The authored Geometry/Texture rows are hidden for IJIM and their tokens stay
  intact. Preview mode changes do not mark the resource modified, rebind resources, rebuild its cache, or reset the
  camera. Optional `presentation.globalGeometryMode` retains the selected integer; classic captures may include its
  hidden default 4
- SND uses one tree as the raw draft owner, with collapsible exact mode names, separately painted engine-slot badges,
  and editable alternative rows. Visual mode groups follow engine-slot order while
  stable source-row identity/order determines serialization. Unknown modes and flag bits remain editable and intact;
  the 4096-row admission bound is editor safety, not an engine per-mode cap. Pane Undo/Redo handles row/content changes
  and playback ownership prevents one pane from stopping another pane or a picker. Mode groups initially collapse;
  expanded modes own their column headings and spanned Add Entry footer; a dedicated Add Mode button opens a searchable
  mode picker. Top Expand All/Collapse All buttons and one-click mode headers control presentation. Explicit empty
  modes survive native pane restoration in the optional `soundEmptyModes` list without becoming authored SND rows.
  This sorted, unique engine-name list is excluded from the canonical authored projection and external SND output;
  New, Open, and source reload clear it. Five shared controls keep the tree compact: a hovered Play/Loop/Remove set
  and a playing Stop/Loop set. Only an active pane-owned playback lease runs the status timer; constant-time row and
  ownership checks clear stale controls without walking the tree or capturing history. Rows use no selection highlight
  while retaining current-item editing and native text Undo. Flags, Min, and Max headings and values are centered;
  radius help retains world-distance units. Painted volume controls clip to their own cells when the tree narrows. Optional per-row preview Loop
  choices are presentation state, separate from authored play flags.
  The six Sound columns use minima `80/160/100/56/56/230` and defaults `80/220/100/64/64/230` logical pixels. The
  default fit allocates the flexible width, then shifts width from Volume to Sound while keeping Volume at or above
  its 230-pixel minimum. Columns remain manually resizable, respect their minima, and can retain horizontal scroll.
  Pane minima reserve actual styled scrollbar chrome independently of visibility. Inactive and active Volume layouts
  share a Remove reservation inside the visible cell, hiding it when the remaining controls cannot fit. Shared hover
  buttons retain 22-pixel bounds and use matching 13-pixel icon canvases at every display scale.
- UNI pages use the generic ordered string-table API and retain raw game-byte text without routing through the legacy
  uppercase-key COG-string compatibility wrapper. Every project hides the unused legacy-number column without
  discarding its signed value
- core external writers validate engine filename buffers separately from the bounded raw import contract: SPR MAT
  names admit 31 bytes for JKDF2/MotS and 63 for IJIM; SND WAV names admit 31 bytes for JKDF2/MotS and 121 for IJIM.
  `saveToSoundClassText` accepts an explicit `SoundClassProfile`, with classic as the compatibility default. Rejected
  serialization cannot append partial stream bytes. Generic UNI profiles both compare exact game-byte key identities
  and emit `MSGS` with the live row count, while retaining imported capacity in memory and native EDIT. The independent
  `CogStringsFile` episode compatibility wrapper keeps its uppercase-key/case-insensitive behavior
- open tabs, including virtual identity, resource kind/profile, source metadata, tab order/pinning, active selection,
  resource-window membership, per-buffer native baseline hashes, and raw draft cells are part of the optional
  `edit.resourceEditors` V1 extension. Its flat tab list points to at most 64 occupied primary/detached hosts; the
  independent file limit remains 64. Optional typed host metadata and a named `groups` map retain all three primary
  groups, including empty-group visibility/floating geometry without consuming occupied-host budget. Legacy mixed hosts
  are partitioned by resource kind while preserving every raw file record and native baseline. EDIT capture reads an
  active table delegate or Sprite numeric editor's current text/caret without committing it, moving focus, or resetting
  its cursor. Optional per-page `editorPresentation` V1 stores line-edit carets, UNI current/selection ranges, SND stable
  current identity and column, table scroll, manual widths, and deferred cell-editor carets. Pending Sprite numeric text
  is stored separately from its last accepted finite values. Presentation is excluded from authored dirty/history state;
  deferred cell carets apply only when ordinary editing starts, and restore never reopens a live delegate. The extension uses the
  same capture/validation path for native Save, autosave, and recovery; restored floating geometry is clamped to an
  available screen. Preview textures and filesystem watchers remain transient
- resource drafts join COG buffers in Save/Discard/Cancel replacement checks and recovery eligibility; resource prompts
  identify the current filename. Native SED Save
  embeds their current text without writing loose files or clearing their external modified state. Discarding or saving
  one external resource retires only that buffer's separate recovery baseline
- resource-focused Save and Save As shortcuts stay with the active pane, including group-toolbar focus and cancelled or
  failed saves. A bounded focus/activation registry retains the last active host when floating-window focus changes.
  Actual widget focus takes priority over a floating native window whose activation can lag a return to the viewport.
  Document Undo/Redo routes to the focused resource or COG timeline before level history; native text editors retain
  their own Undo/Redo.
- SPR, SND, UNI, and COG loose buffers share the transient `ui/components/loose_document_watcher` service. File and
  reference-counted parent-directory watches detect atomic replacement, deletion, and recreation. SHA-256 fingerprints
  distinguish accepted source bytes from a disk revision acknowledged by Keep. One asynchronous queue offers named
  Reload/Keep choices for dirty and clean buffers after mouse grabs and other dialogs finish. Registration incarnation,
  document context, and authored revision guards reject stale work; owner destruction retires queued and visible offers.
  Open, successful Save, and Reload supply exact accepted bytes, so the editor's own writes need no timed suppression.
  COG's source-context Reload and configured Refresh action remain synchronous intentional operations; there is no
  document-toolbar Reload button. Recovered newer-source choices use the shared queue. This buffer service remains
  separate from the placed-COG schema watcher and never serializes watch metadata or captures document history.
  Level Delete/transform shortcuts do not escape from focused resource controls
- pane teardown destroys borrowed-data preview widgets before their backing members. OpenGL context loss releases all
  context-owned programs, buffers, vertex arrays, framebuffers, and textures while the context is current when it can be
  reacquired. If teardown cannot make that context current, GPU readiness and the dynamic-light buffer name are
  invalidated without GL calls; stale wrappers are discarded with no context current before a replacement context is
  initialized. CPU resource caches and authored state survive recreation, and floating/redocking cannot reuse stale GPU
  handles. MainWindow explicitly tears down resource pages while its callbacks and recovery state remain alive
- tab registries and focused-pane checks are bounded by the workspace admission limit. Generic viewport movement,
  transforms, and key handling do not capture resource drafts or scan table rows

The external-write admission rules follow the audited engine buffers and record readers. The filename bounds reserve
the terminating byte; IJIM WAV names also reserve the six-byte `sound\\` prefix in its 128-byte path buffer. These
bounds apply to external serialization, independently of SED's bounded raw-import and native-draft retention rules.

| Contract | Classic engine evidence | IJIM engine evidence |
| --- | --- | --- |
| SPR MAT name and scalar/RGBA layout | [32-byte MAT buffer and scalar extra light](https://github.com/shinyquagsire23/OpenJKDF2/blob/02297602cb289b113df27be33b5a7dddbe3d91c1/src/World/sithSprite.c#L119-L139) | [64-byte MAT buffer and RGBA extra light](https://github.com/smlu/OpenJones3D/blob/b9c0eaa56bb51307b0787add4b614d9468e5d372/Libs/sith/World/sithSprite.c#L333-L364) |
| SND WAV name | [32-byte cached filename copy](https://github.com/shinyquagsire23/OpenJKDF2/blob/02297602cb289b113df27be33b5a7dddbe3d91c1/src/Devices/sithSound.c#L183-L194) | [128-byte prefixed load path](https://github.com/smlu/OpenJones3D/blob/b9c0eaa56bb51307b0787add4b614d9468e5d372/Libs/sith/Devices/sithSound.c#L238-L250) |
| UNI live count and retained signed metadata | [Declared-count loop and opaque integer assignment](https://github.com/shinyquagsire23/OpenJKDF2/blob/02297602cb289b113df27be33b5a7dddbe3d91c1/src/General/stdStrTable.c#L83-L132) | [Declared-count loop and opaque integer assignment](https://github.com/smlu/OpenJones3D/blob/b9c0eaa56bb51307b0787add4b614d9468e5d372/Libs/std/General/stdStrTable.c#L78-L111) |
| COG `PlaySoundClass` argument | [Thing plus integer mode selects a random alternative](https://github.com/shinyquagsire23/OpenJKDF2/blob/02297602cb289b113df27be33b5a7dddbe3d91c1/src/Cog/sithCogFunctionSound.c#L285-L304) | [Thing plus integer mode selects a random alternative](https://github.com/smlu/OpenJones3D/blob/b9c0eaa56bb51307b0787add4b614d9468e5d372/Libs/sith/Cog/sithCogFunctionSound.c#L299-L320) |

### Docked UV editing

Primary files:
- `src/app/src/main_window/uv/editor.cpp`
- helpers under `src/app/src/ui/uv/`

Behavior:
- UV editing is a docked workflow rather than only a detached popup
- the UV panel uses an internal splitter so the grid and controls can be resized independently
- The optional UV Sub-grid has independent Lines/Points and `0.5`–`10` logical-pixel point-diameter controls, defaulting
  to Lines and `2` pixels. Appearance preferences use `uv_editor/grid_minor_dots` and
  `uv_editor/grid_minor_dot_size_px`; settled native/recovery workspace values use `window.uvGridAppearance` in EDIT.
  Appearance changes repaint the canvas without editing UVs or changing grid spacing.
- showing/hiding the UV editor uses one short S-curve transition. A newly revealed panel is constrained and starts at
  a controlled one-pixel splitter extent, redundant presentation does not restart the animation, and the expanded
  minimum is applied only after the animation owns the extent. If Preview interrupts that transition, the request is
  finalized through the same non-animated constraint path. Preview captures the complete logical splitter request
  without forcing an event-loop layout; returning to Editor lets Qt realize it to the same exact naturally constrained
  allocation as an uninterrupted transition
- viewport compensation follows projected object scale rather than repeatedly fitting the full pre-panel rectangle:
  opening/closing or resizing the panel introduces only the crop/reveal implied by the available viewport extent and
  the incremental inverse preserves navigation performed while Surface mode is active. Hide rebases from the live
  viewport after unrelated window/splitter resizing, and view-facing winding refresh pauses during the transition
- rebuild logic forces panel sync after more real UV mutations and tries harder to avoid stale display-state reuse
- modal `G`/`S`/`R` edits guard no-op commits, cancel cleanly on hide/focus transitions, and block conflicting wheel
  zoom while active. Pointer previews patch only the affected texture-coordinate
  ranges in the existing level VBO; the authoritative undo transaction and full
  derived refresh happen once, only after a changed transform is committed
- Smart UV Project delegates to the directly tested `ui/uv/smart_uv_project_utils.*` normal-clustering/project/pack
  pipeline and uses `smart_uv_project_dialog.*` for its Blender-compatible option surface. Projection discovery and
  active-image aspect semantics follow Blender; adjacency is a post-projection island-connectivity rule, while the
  final deterministic shelf pack remains an explicit local approximation of Blender's internal polygon packer
- Smart UV Project returns/reports failure atomically instead of applying empty invalid output
- raw stored texture space remains the default. Optional **View Face** derives the active selected face's viewport
  basis and applies a shared horizontal mirror when its handedness differs plus a 2D rotation from the stored/projected
  reference edge: texture, grid, all selected islands, hit testing, and edit directions use the same basis
- the active face controls that one global decision for multi-surface selections. Behind-camera, edge-on, or degenerate
  projections preserve the previous orientation. The view rotates and may mirror the canvas for facing, but it does not
  foreshorten it to imitate the full viewport pose

### `GlView` responsibilities

Primary files:
- `src/app/src/gl_view.hpp`
- `src/app/src/render/gl_view_*.cpp`
- focused helpers under `src/app/src/render/*/`

`GlView` is a `QOpenGLWidget` that owns:
- OpenGL resources (shader programs, VAOs/VBOs, textures, pick FBO)
- renderer caches (level batches, thing batches, overlays, sprite/gizmo textures)
- viewport state (camera, render mode toggles, sub-mode toggles, tool state)
- selection overlays, transform gizmos, grid, axis widget, toasts, and selection hints
- ordinary viewport toast expiry requests a final repaint when its timer clears the last active message, then stops
  until another message needs expiry; an empty toast list does not sustain timer-driven repaint churn
- Blender-style selection orbit navigation: `GlView` captures one drag-scoped pivot from the complete selection and
  rigidly revolves its eye/look rig around it, so MMB press cannot jump the camera and the selected point remains
  screen-stationary. Disabling the option idempotently clears the drag pivot so the same drag can return immediately
  to legacy camera-target orbit. The pure policy/math lives in `ui/viewport/viewport_orbit_utils.*`
- IJIM Sector-mode direct thrust-arrow selection/dragging plus normalized direction and magnitude properties
- persistent **Multi-cut** authoring in Sector and Surface modes. Each line owns an explicit plane; Sector planes are
  world-space authoritative and therefore do not move or change result when the camera changes. `GlView` owns line,
  plane, finite authored handles in Bounded mode, and authored-camera edge handles when drawing/editing independent
  unbounded planes (including a not-yet-linked first Chain plane). Toggling Bounded alone preserves existing handle
  geometry. It also owns chain-safe guides/leaders, pre-click construction-point snap markers, angle-step snapping,
  and render-only live drag state;
  `MainWindow` owns the persisted overlay choices, captures the immutable target selection, and publishes one
  transaction; `sed_core` applies the ordered planes while maintaining convex descendants and valid
  adjoin topology. Independent Sector batches skip a stage that splits no admitted descendant, including bounded
  strokes that reach no splittable piece; the complete batch must contain at least one effective split. Closed Sector
  chains also permit a non-splitting plane while a non-empty interior survives. An empty interior or invalid cut rejects
  the complete batch without publishing earlier stages. For plane-only open Surface batches, a line that misses every current
  descendant is skipped only when its supporting normals pass a component-wise roundoff check and the compensated
  point-to-point dot proves zero signed
  displacement from an earlier successful plane. A distinct miss retains its authored order and failure index; if a
  selected face is classified as crossing but the core cleave rejects it, the complete batch rolls back. Angles,
  Lengths, and Numbers visibility is presentation state kept in application preferences and the optional named EDIT
  `guides` map, serialized by native Save, autosave, and recovery outside the authored Level and its undo history.
  Native finite strokes have a separate construction-roundoff admission for a straight continuation sharing an
  exact endpoint with an earlier genuinely splitting line. It checks finite nonzero segments, consistency of each
  normal with its own stroke, and component-wise collinearity/normal bounds derived from the represented endpoint
  coordinates. Unresolved or underflow-ambiguous requests fail conservatively. This adds no absolute world/angle
  tolerance and leaves the exact plane-only proof and core crossing-failure contract unchanged in both cut modes.
  A plain endpoint click chooses an optional next Chain origin owned by `GlView`; its ring is visible only with Chain
  enabled. The live line/endpoint reference follows edits, survives independent cuts, and advances after chained cuts
  without reordering Apply. Only canonical tail-second continuation creates neighboring links or automatic closure;
  arbitrary shared origins retain their Sector handles and use ordinary ordered planes. EDIT stores the optional
  `chainOrigin` pair in the existing named Cleave map, with strict bounds validation before workspace restoration.
  Shift-joined arbitrary endpoints remain shared during whole-line movement even when the lines were drawn independently
  with Chain off. Shared-handle editing does not reorder the staged vector or promote a mixed arrangement to the
  canonical closed interior-cut path, which still requires one continuous authored Chain.
  Open Multi-cut arrangements default to Bounded: each later finite world stroke admits only descendants reached
  through the half-spaces of their actual earlier splits. The cut itself remains an infinite plane within admitted
  descendants. Turning Bounded off preserves the global arrangement, and closed chains keep their interior-only
  optimization. The choice is an application preference and an optional named Cleave EDIT Boolean; older staged
  arrangements without that field restore the established global semantics. This gating runs during Apply and does
  not add work to ordinary transform previews.
- bounded line-only Multi-cut draft history. Physical lines, authored frames/links, context, chosen Chain origin,
  execution permutation, and restoration selection form the plain snapshot DTO. Completed changed add/edit/Move/Rotate/
  Scale/join/detach/remove/rank/Clear/origin operations record one state; camera/options/selection and pointer samples
  do not. With matching enabled Cleave ownership in Editor and Multi-cut on, configured viewport/Level Undo/Redo actions
  stay local, including at timeline ends. Focused resource/COG
  histories and native editable-text history retain priority. New edits replace Redo; cancelled/no-op gestures do not
  trim history. Authored draft/history admission allows 64 retained snapshots and 16 MiB of estimated owned payload,
  charging capacities, live/cursor data, rollback baselines, and admitted edit/restore candidate worksets before
  copies/reserves. Shape-planning and rendering caches are separate. A legacy current-only draft may omit an
  optional initial Undo baseline when that extra clone cannot fit, provided its source, live workspace, and prepared
  current draft fit the aggregate 16 MiB workset. Required rollback or memento storage must fit before mutation or
  Apply; an over-budget actual workset is never admitted or partially published.
  Clear is undoable and failed Apply preserves the draft/history. Ordinary tool/edit-family changes, Multi-cut off,
  and Preview entry suspend the settled draft/options/history and roll back an unfinished drag. Dormant work neither
  renders nor owns input/Undo. Compatible same-Level coordinate, light, UV, and direct-append changes retain world planes.
  Direct Sector/Surface Delete, Join, and Merge explicitly invalidate pending cuts; History swaps carrying both Structure
  and LevelGeometry conservatively do the same without a typed identity/remap contract. Checked Apply memento restoration
  takes precedence after publication. Invalid captured source references also clear the draft with a toast. Explicit
  cancellation, document lifetime reset, and successful Apply clear the affected draft. Temporary Outliner mismatch
  preserves it. Native EDIT stores both independent Surface and Sector workspaces, including settled options,
  drafts, line selections, and local timelines/cursors; legacy current-only records begin at a fresh baseline when restoration
  admission permits, retaining the current draft without that optional baseline otherwise. Invalid timelines reject
  EDIT atomically. In-flight gestures, hover caches, and inline rank-editor text are not resumed.
- separate physical identity and execution rank. Physical indices own topology, origin references, selection, and accent
  colors; `applyOrder` is a validated permutation. The inline rank editor clamps `1..N`, shifts other ranks in one local
  Undo step, and preserves endpoints/joins/origin/colors. Apply reads the permutation while closed-chain topology still
  follows the continuous authored physical Chain.
- Cleave-local group selection, Move/Rotate/Scale, joint cycling, and marquee. Common-plane Rotate/Scale pivot about the
  centroid of distinct selected endpoints and display signed angle/positive scale. One prepared sparse workset carries
  shared neighboring endpoints once, pins their opposite ends, and restores exact lines/selection on Escape. Pointer
  previews copy no Level and record no snapshots. Bare G/R/S select these modes only while active Multi-cut owns
  viewport input; normal G toggles the grid. Ctrl-only wheel rotates by Angle step (Free uses 5 degrees per detent),
  uniformly scales selected lines, or scales armed shape dimensions, and avoids camera zoom. Ctrl-held selected-line
  wheel samples reuse one admitted baseline and sparse prepared workset, displaying the cumulative angle/factor.
  Control release or tool/mode/family/focus ownership loss finishes one local command; Escape restores the exact
  baseline without trimming Redo. Identity and complete-turn results create no command. Stable EDIT capture during
  the gesture retains the settled pre-gesture draft/history without finalizing it; its later commit publishes one
  authored revision and makes the native document dirty. Control release immediately clears the HUD and cancels its
  expiry, including release outside the viewport. Only a live wheel session owns that release; presentation-only
  cleanup preserves normal routing. HUD samples rearm a per-banner single-shot idle expiry, 400 ms by default, including
  while wheel/pointer input is held. Expiry clears presentation without ending the gesture; subsequent input shows
  its cumulative result again. New samples supersede stale deadlines; completion retains the remaining deadline
  instead of restarting it. Brief presentation-only fade edges fit inside that deadline. Hiding changes presentation
  only, without a revision or history mutation. A separate single-shot timer requests repaints during the short fade
  edges and sleeps through the opaque interval; it cannot extend the authoritative hide deadline or finish a gesture.
  Add Shape offers Rectangle, N-Gon, Filled arch, and Star as ordinary bounded strokes, with 1
  displayed-unit defaults or `0.1` Bare and project-aware Meters/IJU-or-JKU/Bare choices.
  Armed-shape wheel sessions change future placement options without a line-history command; capture keeps the live
  option values, and Escape restores their session baseline. Its four numeric controls
  accept hovered wheel edits; unit-only changes preserve authored physical dimensions. Legacy Square stays codec-readable
  and canonicalizes to linked Rectangle. Sector Add Shape has a default-off Camera plane option for a free camera-facing
  frame; Surface hides it and always uses its face. The optional strict Boolean lives in current/Apply shape maps without
  a version or array-position change. Its Star division lines
  precede the perimeter, and actual cuts may produce extra convex fragments. Concave Star outlines are extruded by
  selecting each resulting convex face separately.
- `MainWindow` Apply workspace mementos attached to concrete Level records. Each captures only the applied Sector or
  Surface family. Its exact pre-Apply draft/history/options, line and document selection/active items/modes restore
  after geometry notifications and QAction/overlay sync on Undo. Restore lines and their selection before setting the
  saved transform mode, so validity checks see the restored work rather than the empty post-Apply draft. A compatible
  peer workspace remains current, including edits
  made after Apply or Undo. Redo clears only the applied family before mesh notifications. Immediately after Apply Undo,
  matching document Redo wins over retained local Redo while the applied family owns input and its Apply-affecting draft
  is unchanged. An active peer retains priority for its available local Redo; without one, ordinary document Redo of a
  known Apply clears only the applied family. Explicit History navigation follows the same family-only clearing rule.
  An Apply-affecting edit, including Bounded changes, or successful local Undo/Redo in the applied family invalidates
  paired priority; peer edits, family changes, selection/hover/camera, guides, and future shape/angle choices alone do not.
  Optional EDIT `workspaceHistory` binds mementos and Layer visual transitions by nearest-first Undo/Redo position and
  exact adjacent required-Level SHA-256 digests from checked HIST. An Apply attachment names only its applied family;
  a recognized peer workspace inside it rejects the optional attachment transaction. Cleave refs are validated against the pre-Apply Level;
  Layer row metadata is checked against each before/after Level. Checked import remaps to fresh runtime RecordIDs;
  branch/trim prunes unreachable attachments. A separate 64-transition/16 MiB aggregate cap includes nested local
  timelines, Layer vectors, and import/restore worksets before allocation/publication. Missing, degraded, or invalid HIST
  discards the optional attachment transaction with a warning while current Level/EDIT draft and other fields survive.
- each completed Multi-cut line also owns its authored camera. A no-drag line-body click restores orbit, projection,
  FOV, orthographic scale, near/far planes, and clip-enabled flag; the hit action remains available when the construction
  plane is edge-on, and movement within the two-pixel click tolerance rolls the line back to its press baseline
- authored runtime-light inspection and GPU preview. Thing/head emitters use a deterministic view-stable editor
  collector instead of portal/PVS traversal. When the per-project cap overflows, it chooses the nearest eligible
  sources from the complete scene and then restores Thing traversal order before sharing the original contribution
  equations through a 2048-byte std140 payload. IJIM Sector point lights are carried per 3DO Thing: they affect Gouraud vertices but not
  Diffuse faces, matching the original sector slot's retained zero `bEnabled` state. Diffuse faces carry flat front/back
  light colors (including IJIM's order-dependent mutable attenuation) and select the rendered side in the fragment
  shader. IJIM model RGB remains unbounded through authored/ambient/face-extra and dynamic accumulation, then is clamped
  per completed vertex before interpolation. IJIM world geometry instead clamps authored+dynamic before applying signed
  surface+sector extra light and a final clamp. Classic regular lights additionally raise only their owner's ambient
  floor, while Headlights do not; classic Diffuse uses independent per-light attenuation and preserves the first
  saturation overshoot through signed face Extra Light. JKDF2/MotS Sector Tint is a separate completed-vertex effect,
  clamped before texture modulation: Game Preview selects it from the camera sector for every scene batch, while
  Editor mode carries local Tint on world geometry and actual 3DO/SPR Things so it remains visible from outside. Filled
  translucent adjoins retain the owning sector's Tint and use two-sided authoring presentation in Editor mode, so the
  mirror sector cannot recolor their MAT; Game Preview retains the camera-sector effect and original backface culling.
  Explicit classic translucent faces use `90/255`
  vertex alpha, and alpha-textured adjoins share the delayed no-z-write blend pass. Classic CMP parsing retains all 64
  light-level translation rows; each loaded MAT caches the resolved RGB ramp for its Solid palette index, and a JKDF2
  Solid draw selects that ramp before the screen-Tint stage. Prepared preview assets carry the identical ramp without
  moving OpenGL ownership off the GUI thread. Palette-only CMP inputs remain usable for texture decoding, while MotS
  RGB Gouraud and IJIM bypass the JKDF2 scalar ramp. Not Drawn portal geometry and editor
  helper geometry remain visually neutral. It is
  explicitly disabled for IJIM. Original
  Gouraud vertex evaluation remains the default. Optional Phong evaluates effective Gouraud targets per fragment with
  interpolated model normals and the pinned OpenJones3D OpenGL smooth falloff; Diffuse targets retain the original flat
  face-anchor path. Both choices consume the same UBO and leave baked lighting unchanged
- Light-mode category state for Baked entities, runtime Thing emitters, and IJIM-only Sector point lights; each
  category routes selection to its owning outliner/property model, while independent per-family always-show settings
  affect visualization only. Baked transforms edit the baked-light entity; Thing-light Move targets either the regular
  emitter's owner or a Headlight's local offset and Scale uses source-correct range/intensity semantics; IJIM
  Sector-light Move/Scale edit only point-light position and explicit falloff fields. Light Rotate is unavailable.
  The shared Baked/Thing/Headlight/Sector light popup embeds its palette and type-specific scrub controls, publishes
  every change live, closes on outside click or Escape, and creates at most one undo entry per interaction.
  MotS Architectural selection records whether its current all-vertex set came from an owner expansion, allowing the
  first plain vertex click to narrow it to one exact identity while Shift/Ctrl build an explicit set. In an explicit
  set, a plain click on a selected member preserves the batch and an unselected member replaces it. **Delete** resets
  the owner's ARCHLIGHTING values to zero while preserving the binding and instance flag. Compatible snapshot Paste can
  promote an unflagged target in one undoable mutation and never changes the Thing template.
  Unsupported operations are hidden and disabled instead of making the entire transform toolbar disappear. Category
  icons use authored-color source/ray silhouettes and the normal depth buffer for drawing, plus a shared dedicated
  solid-scene depth pass for direct and marquee selection. That pass follows the level solid toggle while independently
  including visible shaded 3DO Things; marquee selection samples candidate centers after one pass rather than
  allocating a viewport-spanning readback
- interactive asset-preview and preview-capture rendering
- point-pick visibility in both projection types. Sector selection treats the
  enclosing volume as two-sided, while ordinary Surface elements, Things, and
  Preview Auto selection share a front-facing solid-Level boundary in both the
  GPU depth pass and CPU fallback. Explicit X-Ray/through-selection restores
  two-sided traversal without changing hidden-layer or edit-family filters

### Sector property ownership

The Sector item editor treats Sound, Thrust, and IJIM Point Light as optional authored blocks rather than permanent
rows containing placeholder zeros:

- an absent block is represented by one **Add** action; present blocks expose compact remove/copy/paste actions in the
  block header;
- point-light minimum/maximum ranges are linked by default and can be unlinked. Live scrub updates preserve the linked
  ratio immediately in both the item editor and viewport popup;
- Thrust stores one vector. The wheel edits a normalized direction on a selectable plane and a separate magnitude;
  numeric and wheel edits propagate the primary sector's exact vector to the rest of a multi-selection;
- viewport gizmo edits select their owning sector first. Delete removes the active authored light/thrust component,
  not the containing sector or Thing.

Render pipeline (high level):
1. rebuild CPU meshes when dirty
2. upload GPU buffers, with partial updates where possible
3. draw opaque level geometry
4. draw opaque things
5. merge translucent level batches, per-Thing triangles, and sprite runs into one stable far-to-near draw queue
6. draw overlays (wire/highlights/points/gizmos/grid/widget/text)

Ordinary level-wire overlays keep scene-depth testing enabled. Their screen-space expansion may match the projected
support plane only for a valid Solid/Texture owner with usable UV data and no explicit translucent or no-Z-write face
flag. This includes opaque adjoins: the adjoin flag alone does not suppress depth writes. NotDrawn, Vertex, Wireframe,
projected-sky, preview-only, explicitly translucent, no-Z-write, and invalid-UV surfaces retain their real edge depth;
their support plane cannot pull wire fragments in front of an unrelated foreground surface. Full level-mesh builds and
selected-surface highlight rebuilds use the same eligibility rule, while coplanar edges backed by their own filled face
retain the inward-side match needed for a stable thick overlay. MAT alpha is resolved after mesh construction. Base
adjoin line spans therefore retain explicit material/colormap ranges independent of pick registration; after a loaded MAT
classifies an adjoin for the translucent no-depth-write pass, the renderer clears that span's support references in both
the base and selected overlays. Texture visibility and resource-cache transitions trigger one bounded reclassification
and VBO update, not a mesh rebuild or per-frame scan. Missing/unresolved MATs and opaque MATs continue to use the solid
fallback and keep their support references.

Current note:
- `GlView` serves the main editor viewport and interactive asset previews. The shared 3DO list-thumbnail pipeline
  performs parsing, hierarchy/bounds preparation, dependency texture decode, supersampled rasterization, and `QImage`
  publication in the dedicated low-priority preview pool. Some cold standalone MAT and SPR preview decode and metadata
  work remains synchronous; the separate ResourceContext design is a proposal, not the current implementation
- Translucent sorting now crosses the old level/Thing/sprite pass boundaries, and the near-opaque depth prepass uses
  final material-modulated alpha rather than texture alpha alone for eligible
  Thing meshes and non-adjoin level batches. Thus opaque texels of a translucent solid wall occlude
  translucent Things behind it without making the wall's transparent texels
  opaque. Grouped level primitives, representative depths, and the depth cutoff remain heuristics, and this
  is not OIT: intersecting or cyclic alpha geometry still has no universally correct total order.
- A filled adjoin with Face **Translucent** or **No Z Write** remains filled but is routed to the sorted alpha queue;
  the adjoin **Visible** flag controls portal traversal and Geometry **Not Drawn** remains pixel-invisible
- 3DO list-thumbnail parsing, hierarchy/bounds preparation, material decode, and supersampled rasterization are
  worker-owned. During scroll/search interaction the GUI postpones icon assignment, then publishes the current row
  first in count- and time-bounded turns. Newly converted icons are paced below the interactive preview frame rate,
  while all four owners reuse the same revision-keyed image across Template Picker, raw 3DO Picker, Template Editor,
  and Thing Placement
- viewport grid rendering uses per-axis derivative footprints and four exact decimal major-grid LODs. Dense levels
  disappear before under-sampling while a coarser subset remains, without reintroducing projection-depth dimming;
  orthographic near-parallel views, perspective grazing angles, and far-from-origin stability still deserve direct
  local verification
- minor-grid appearance defaults to Lines and optionally draws Points with an independent `0.5`–`10` logical-pixel
  diameter, default `2`. Major levels and colored axes keep their line paths. The analytic grid uses the existing
  fullscreen pass and camera-relative lattice phases; the CPU fallback bounds point generation and keeps major/axis
  lines when minor density exceeds its budget. Appearance changes update grid presentation without rebuilding scene
  meshes or altering the shared snap lattice. App settings and the optional EDIT `gridAppearance` map retain both choices
- level/selection wire overlays retain strict, unbiased scene-depth rejection. Segments are clipped against the
  homogeneous near plane before screen-space expansion. Valid filled Solid/Texture edges receive a supporting-plane
  reference only when their face flags permit depth writes; this includes opaque adjoins. NotDrawn, Vertex, Wireframe,
  sky, preview-only, explicitly translucent, no-Z-write, and invalid-UV edges keep their authored edge depth. The
  thick-line shader matches projected surface depth only toward an eligible supporting face and bounds that correction
  at the reference-side projection, so narrow/grazing faces cannot extrapolate a wire arbitrarily toward the camera.
  Material-keyed adjoin line spans are reclassified after MAT alpha becomes available: alpha adjoins lose support-plane
  references in both base and selected overlays because their translucent pass does not write depth, while opaque and
  unresolved-MAT fallbacks keep them. Cache reload and texture-visibility changes perform this update once per transition.
  The screen-space quads render double-sided, and the pass exactly restores blend, multisample, line, cull,
  depth, and polygon-offset state; no opposing polygon offset or fixed clip-space pull promotes a rear sector through a
  foreground wall at grazing angles
- clip-enabled orthographic rendering uses a transient depth range derived from the cached level and transformed-Thing
  mesh bounds, intersected with the authored near/far planes. This improves depth-buffer precision without changing
  saved camera settings; invalid/empty bounds fall back to the authored range. The depth-tested grid footprint is
  included when bounded, while Always Behind and Visible Through Walls grid passes keep the authored projection.
  Screen projection and picking use the same effective matrices as rendering

### Branding and icon assets

Primary files:
- `src/app/CMakeLists.txt`
- `src/app/src/main.cpp`
- `src/app/src/app_icon.cpp`
- `src/app/resources/app_icons.qrc`
- `src/app/resources/app_window.svg` and `app_window_*.png`
- `src/app/resources/app_flat_red_circle.svg`, `app_flat_red_circle.ico`, and `app_flat_red_circle_*.png`

Behavior:
- `sed_app` embeds `src/app/resources/app_flat_red_circle.ico` through a generated Windows RC file for the native
  shell/taskbar icon. No runtime icon file is copied beside the executable.
- `app_icon.cpp` loads the window PNG sizes embedded by `app_icons.qrc`, avoiding launch-directory and optional ICO
  decoder dependencies. The fly-navigation eye cursor is embedded in the same Qt resource collection; if a platform
  cannot decode CUR data, the viewport uses its code-generated eye cursor instead of probing the filesystem.
- `main.cpp` applies the decoded icon to both `QApplication` and `MainWindow` before the startup flow. The startup
  dialog inherits it from the main window, covering every in-app title bar as well as the taskbar.
- `app_window.svg` and `app_flat_red_circle.svg` are the editable masters for the in-app and native-shell icon families.
  Superseded experiments do not belong in the compiled resource directory.
