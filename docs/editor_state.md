# Editor-state persistence inventory

Native `.sed` Save/Open, live recovery, and Autosave History share the same application EDIT capture, validation, and
restore codec. They preserve the authored Level and the represented settled workspace; they do not serialize every
member of a live widget or resume a held input gesture. The normative wire contracts are
[SED binary format](formats/sed_binary_format.md) and [recovery format](formats/recovery_format.md).

This inventory describes persistence ownership and limits. It is not a claim of exhaustive runtime or visual acceptance.

## Shared checkpoint and restore

[`captureRecoveryEditorState`](../src/app/src/main_window/document/recovery.cpp) captures one GUI-owned, canonical
CBOR EDIT payload. The shared parser validates known types, bounds, references, nested histories, and topology before
applying it. Native Save maps Frame selections by logical ordinal into its normalized save candidate; native Open and
recovery use the same restore path after installing the checked Level. Resource and COG hosts are prepared before Qt
restores dock layout, and floating windows are constrained to available screens.

The writer receives an immutable Document/EDIT snapshot. Publication uses checked staging and atomic replacement;
failures retain the preceding complete file/checkpoint. Document identity and epoch checks prevent an old asynchronous
Save or recovery result from rebinding a replacement workspace. Authored revisions and independent history signatures
prevent newer authored state from being marked saved. Changes during a same-document recovery write queue a newer
capture behind that coherent write.

Editor-only camera, selection, tool, UV, and layout changes can update a periodic workspace checkpoint without dirtying
the Level or creating an automatic recovery offer. Authored Level, COG/resource buffer, and pending Cleave changes
have their separate recovery eligibility. A native Save embeds loose-file buffers without externally saving them or
clearing their external modified markers. COG's optional `acceptedSourceHash` records exact accepted disk bytes,
independently of the native saved content baseline. Ordinary loose Save checks it before formatting, with
Reload/Overwrite/Cancel on external drift or an unknown source baseline; watcher Keep does not rebase it.
A checkpoint protects work only through its completed publication boundary.

## Required Level and history domains

The native required-domain schema, rather than the C++ object's entire memory layout, defines the authored persistence
contract. Finite signed-zero bits, ordered inventories, and applicable cross-project values are retained.

| Native domain | Durable authored state |
| --- | --- |
| `head` | Project/header settings, fog, all twelve export buffer extras, master CMP, PPU, LVIS, and `curId`. |
| `rsrc` | Fourteen categorized resource/name catalogs plus ordered material, colormap, and layer inventories, including duplicates. |
| `sect` | Sectors, vertices, Surfaces, adjoins, UVs/scales, RGBA lighting, flags/layers, Sector point lights, thrust, and collide boxes. |
| `thng` | Thing identity, template and instance names, transforms, typed values/metadata, layers/boxes, retained MotS ARCH bindings/table, and authored bake provenance. |
| `tmpl` | Authored templates, inheritance, typed values/metadata, descriptions, and boxes. |
| `cogs` | Placed COG IDs/numbers, source names, typed values, and metadata; these differ from open source-editor buffers. |
| `lite` | Editor baked-light RGBA, intensity, range, flags, layer, and position. |
| `hnod` | Authored level 3DO hierarchy nodes. |
| Optional core `jimp` | Pending JKL COG import marker and imported Surface-reference map when that provenance is meaningful. This protected V1 section is outside the eight required domains; unused maps are omitted without a native file-version bump. |
| Optional `hist` | Checked Level Undo/Redo, per-state selections/navigation hints, and optional `jklCogImportV1` metadata for the current and represented history states; compact or full snapshots are reconstructed from the exact current-Level base. Nested history Levels still contain exactly eight required sections. |

Native MotS Save canonically filters only known dormant/unreferenced all-zero ARCH records on an isolated snapshot,
remapping retained bindings as specified by [`thng`](formats/sed_binary_format.md#thng). The live Level remains intact.
This canonicalization is also the recovery writer's verification contract.

Pending JKL `importedJklCogRowsPresent`/`importedJklSurfaceRefs` provenance is stored in the optional core-owned
`jimp` V1 section. It is represented when the row marker is true, or when the Surface map is nonempty and the Level
contains raw `paramN` values with `CogType::unk`; false-marker maps without those pending COG rows are treated as empty,
so ordinary healthy files gain no section or byte changes. This lets a native reload retry deferred script-default
hydration and retain imported Surface ordering. Unresolved ambiguous Surface ordering is still rejected before native
Save. The optional HIST `jklCogImportV1` map keeps the current state and sparse metadata for affected undo/redo states;
`HistoryNested` frames remain exactly the eight required Level sections. Strict loading rejects malformed known import
metadata. Permissive recovery can discard a damaged or invalid `jimp`, clear only its marker/map, preserve the verified
Level, and report the optional loss. Supplied raw `paramN` arguments remain durable. Represented Surface maps repeat
for each affected HIST state and count against its existing aggregate reference, CBOR, and encoded-size budgets.
Large unavailable-script imports can therefore lose optional saved history with a warning while the current Level and
its `jimp` provenance remain retained.

## EDIT workspace domains

| Domain | Represented settled state | Boundary |
| --- | --- | --- |
| Editor and Game Preview viewports | Separate orbit/pan/orientation, projection/FOV/ortho/clip, render/fog/Thing/hidden-Thing choices, lighting modes, overlays, gizmos, classic LOD, and initialized markers. | Active and inactive slots remain independent; no rendered image or GPU cache is saved. |
| Grid and common viewport controls | Grid basis/center/spacing/follow/alignment/adaptive modes, visibility/colors/axes/opacity/width/dots, depth options, runtime-light switches, X-Ray, orbit policy, hidden-layer Preview policy, and active light-category overrides. | Named extension defaults are field-specific; present valid document values override app defaults. |
| Document edit context | Entity/component/Frame selections, seven active indices, edit mode, and Surface/Light submodes. | References are checked against the saved Level; unavailable project-specific light submodes fall back safely. |
| Surface selection context | Frozen Strict face-owner set and remembered Face selection, including intentional empty sets, in the checked optional `surfaceSelectionContext` V1 map. | Absent-only legacy inference uses current owners. Structural geometry reindexing clears frozen scope/stale mode cache/components, seeds remembered faces from committed current faces, and returns to Face. Cold history checks actual owner/connectivity changes even with unknown, incomplete, or unsupported dirty masks; mixed jumps prepare adjacent-state checks before live changes. Pure Move and Thing-only changes retain compatible context and committed Face selection. |
| Layers | Visibility, locking, colors, color-by-layer choices, and hidden-layer Outliner policy. | Level semantic layers and workspace presentation have separate ownership. |
| Transform and pointer-tool ownership | Active/suspended Select/Move/Rotate/Scale state, explicit no-transform state, modification/placement owner, local/world orientation, snap/stick choices. | Join/Extrude previews and live drag matrices are not restartable input transactions. |
| Placement and brush | Thing template/layer/light family, Sector shape/plane/view choices, brush shape/size/count/spacing/scatter/rotation/air and palette layout. | Held placement/brush strokes are transient. |
| Independent Sector and Surface Cleave | World lines/planes, endpoints/links/origin, authored cameras, target context, Bounded, execution order, line selection, Move/Rotate/Scale, shape options/armed choice, guides/units/angle step, and separate local Undo/Redo cursors. | Both active and parked families persist. Capturing a held wheel/pointer transform uses its settled baseline; accumulators and hover/rank-editor text do not persist. |
| Cleave Apply and Layer history attachments | Checked family-specific Apply mementos and historical Layer visuals, bound to exact adjacent HIST states. | Missing, degraded, or invalid HIST drops these optional attachments with a warning; current drafts remain independent. |
| MotS Arch workspace | Authoring/target modes, exact source-vertex identities, and implicit owner-expansion provenance. | Missing model resources retain checked pending identities for later resolution; hover, channel previews, and caches are transient. |
| UV editor | Dock extent/splitters, View Face basis, repeat/snap/grid/sub-grid/dots, scale-link controls, zoom/pan/mirror/rotation, active island and exact per-island Face/Edge/Vertex masks and display alignment. | Topology and document selection are checked before applying. Explicit Face/component pivot choices and modal transform bookkeeping are transient. |
| COG source buffers | Exact canonical plain-text projection, path/name/loose/archive/draft identity, modified/pin state, optional accepted source SHA-256, cursor/anchor, both scrollbars, whitespace/zoom/Find options, splits/disclosure, and bounded local command journal. | Diagnostics/highlighting are derived again. Source capture does not write the external file. |
| COG windows | Flat tab order/active, stable host IDs, membership, requested Editor visibility, floating state/geometry, and local active tab. | Preview's temporary hide does not erase requested visibility; moving a tab retains its live document/history. |
| Sprite buffers | Accepted scalars/modes/material/RGBA/offsets, pending raw numeric text, source metadata/external/native baselines, and local Undo/Redo. | Invalid raw input survives; explicit external SPR Save validates engine output. |
| Sprite preview presentation | FPS, paused choice, Bounds, global preview geometry, splitter sizes, and camera. | Current MAT cel phase is derived; authored SPR geometry/modes remain separate. |
| Sound Class buffers | Ordered raw cells/accepted rows/flags/radii/volume, search/collapse, preview-loop choices, empty mode groups, source baselines, and local command journal. | Empty groups are undoable native presentation; external output/dirty comparison uses authored rows. Playback does not resume on restore. |
| UNI buffers | Ordered raw key/number/text rows, imported capacity, search, source baselines, and local journal. | Invalid or unrepresentable raw text survives; external Save validates and writes the live row count. |
| Resource-pane presentation | Optional per-pane `editorPresentation` V1 caret/anchor state, UNI current cell/ranges, SND stable current item/column, table scroll offsets, manual SND widths, and deferred cell-editor carets. | Presentation capture is excluded from authored dirty state and resource history. A deferred cell caret applies when ordinary editing begins; restore never reopens a live delegate. |
| Resource groups and windows | Independent Sprite/Sound Class/UNI groups, tab order/pin/active, stable occupied hosts, kind/membership, requested visibility, floating geometry, and native baselines. | Same-kind transfer has one owner; source identity/profile/membership are validated before restore. |
| Main window and docks | Main and nested workspace Qt geometry/state, named splitter sizes, Outliner/Checks active tabs, and logical Editor layout while Preview hides tools. | Invalid layout decoration can warn; docked resource geometry belongs to the Qt workspace layout. |
| Template Editor | Optional last-used/live dialog geometry. | Unaccepted working templates, local editing, search/selection, and preview are a modal transaction and are excluded. Accepted changes belong to Level/template persistence. |
| Engine export association | Last compatible project-aware JKL/NDY/JED target. | Export does not replace the native SED document path. |

The source owners are [`GlView` editor state](../src/app/src/gl_view.hpp),
[workspace capture/restore](../src/app/src/main_window/document/recovery.cpp),
[COG editor](../src/app/src/main_window/cog/editor.cpp),
[resource pane codec](../src/app/src/ui/resources/resource_authoring_editor.cpp),
[resource host topology](../src/app/src/main_window/resources/editor.cpp), and
[UV editor](../src/app/src/main_window/uv/editor.cpp).

## Application preferences and transient state

Application settings own keymaps/navigation, theme/antialiasing, numeric input policy, resource roots, recovery policy,
marker/normal appearance, snapping radii/default policies, per-pipeline mute, and the Strict-selection Boolean.
A saved document may carry actual workspace choices that override defaults. The input locale of unfinished raw numeric
text is not stored; a restored editor uses the current numeric policy and may require correction before accepting it.

SPR/SND/UNI panes optionally serialize presentation in `editorPresentation` V1 inside each EDIT resource page. Its
stable line-edit names are `spr.material`, `spr.width`, `spr.height`, `spr.extra_light.scalar`, `spr.offset.x`,
`spr.offset.y`, `spr.offset.z`, `snd.search`, and `uni.search`; each stores `{cursor, anchor}` in UTF-16 code units.
The endpoints retain forward/reverse selection direction and are restored directly, rather than translated into
grapheme-step counts, including in supplementary and combining text. `tables.uni.entries`
stores a nullable `{row, column}` current cell, `{top, bottom, left, right}` selection ranges, and horizontal/vertical
offsets. `tables.snd.entries` stores a nullable current item identified by `{kind, sourceOrdinal}` for a source
alternative or `{kind, modeBytes}` for a mode, add-alternative, or column-header item, plus its column and
horizontal/vertical offsets. Explicit manual sizing stores six widths in Play/Loop, Sound, Flags, Min, Max,
Volume order; the respective minima are `80/160/100/56/56/230` logical pixels, each width is at most `16384`, and their
sum is at most `65536`. The `tableEditors` map keys are `uni.entries/<source row>/<column>` or
`snd.entries/<source ordinal>/<column>` and store the same caret pair. Caret/anchor positions for table-cell editors are
deferred and applied when ordinary editing starts; restore does not reopen or commit the live delegate. A data change
or Undo/Redo invalidates obsolete deferred cell carets. Successful Sound Class Save normalization does likewise when
only the raw numeric spelling changes. Capturing this map does not change authored dirty state, create a
resource Undo command, or arm recovery by itself. Existing raw drafts and local Undo/Redo remain their separate durable
state. COG has its own represented cursor/anchor and scrolling contract. Modal Template Editor working drafts and
explicit UV pivot choices remain excluded.

Other transient state includes held buttons/pointer grabs, Fly movement, hover/repeated-pick cycles, old mode-selection
convenience memories outside the represented context, diagnostic results, audio leases/playback, file-watcher queues,
resource/model/thumbnail caches, render acceleration structures, GPU objects, and derived runtime lighting. Restore
rebuilds derived state from authored data and resource configuration. It does not store half-applied previews as Level
fields.

## Admission, degradation, and extension limits

EDIT is one checked canonical CBOR value with root schema V1. Native versions V1–V4 do not change that schema. The
complete payload is limited to 128 MiB, 2,000,000 CBOR nodes, depth 64, and a bounded decoded graph; nested domains have
additional limits. Resource/COG document journals retain at most 1,024 commands and bounded 16 MiB encodings. Independent
Cleave timelines retain at most 64 states per family under a shared 16 MiB workset budget. Apply/Layer attachments have
their own 64-transition/16 MiB cap. HIST has separate encoded, reconstruction, transient Level, and aggregate retained
snapshot budgets, detailed in the [normative HIST contract](formats/sed_binary_format.md#hist).

Invalid known EDIT fields reject the workspace before partial mutation; losing optional EDIT permits the checked
Level to open with clean/default workspace and a warning. Optional malformed HIST opens the current Level without the
timeline. Required or unidentified Level loss has a separate confirmed partial-recovery policy and does not imply that
the document was fully restored.

Core returns validated unknown optional sections opaquely, but application Save emits only sections it owns. Unknown
named EDIT keys and unknown `editorPresentation` map members may be ignored and dropped on resave; malformed known
presentation fields are rejected with that EDIT resource state rather than partly applied. This is bounded
forward-compatible reading, not preservation of arbitrary future app state. Existing fixed record layouts cannot
silently grow without a deliberate schema/version change; additive checked named maps follow their documented
compatibility rules.
