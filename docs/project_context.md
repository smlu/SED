# Project context

This file summarizes the current application, its major systems, and the main limitations to keep in mind before
touching code.

For the original-editor parity checklist and module map, see `docs/porting_guide.md`.

Additional onboarding docs:
- Level-designer manual: `docs/user_guide.md`
- Architecture overview: `docs/architecture.md`
- Algorithms: `docs/algorithms.md`
- Canonical lighting reference: `docs/lighting.md`
- Native/recovery format contracts: `docs/formats/sed_binary_format.md` and
  `docs/formats/recovery_format.md`

## Contents

- [Project goal](#project-goal)
- [Current architecture](#current-architecture)
- [Editor data flow](#editor-data-flow)
- [Current implementation highlights](#current-implementation-highlights)
  - [Startup flow and new projects](#startup-flow-and-new-projects)
  - [Autosave and recovery](#autosave-and-recovery)
  - [Add Sector drawing and reusable shapes](#add-sector-drawing-and-reusable-shapes)
  - [Large-level Thing placement](#large-level-thing-placement)
  - [Join, merge, and portal editing](#join-merge-and-portal-editing)
  - [Surface extrude workflow](#surface-extrude-workflow)
  - [Resource discovery and pickers](#resource-discovery-categories-pickers-and-template-browsing)
  - [NDY/JKL export capacities](#ndyjkl-export-capacities)
  - [Preview and thumbnails](#previewthumbnails)
  - [Sound preview and sector ambience](#sound-preview-and-sector-ambience)
  - [Shared styling and property editing](#shared-ui-styling-and-property-editing)
  - [COG workflow and authoring](#cog-workflow-and-authoring)
  - [Query and find](#query--find-workflow)
  - [Item editor and property parity](#item-editor--property-parity)
  - [Viewport, overlays, and frame editing](#viewport--overlays--frame-editing)
  - [Docked UV editing](#docked-uv-editing)
  - [Consistency checker and parity work](#consistency-checker-and-parity-work)
  - [App icon and branding](#app-icon-and-branding)
- [What remains and known gaps](#what-remains--known-gaps)
- [Recommended next steps](#recommended-next-steps)

## Project goal

- Maintain a modern C++26-first SED/JED editor, with C++23/C++20 fallback where the compiler requires it.
- Keep non-UI/editor logic portable and testable in `sed_core`.
- Build the Qt desktop editor (`sed_app`) around an OpenGL viewport and modern WYSIWYG workflows.
- Preserve legacy semantics where they matter, but do not force old Delphi UI layout if a cleaner modern workflow is
  better.

## Current architecture

- `src/core/` (portable library)
  - level model (`sed::Level`)
  - geometry/editing operations (`sed::level::*`)
  - document + selection + undo (`sed::editor::*`)
  - asset parsers and resource locator (`JKL`, `JED`, `LEV`, `ASC`, `3DO`, `SPR`, `MAT`, `TPL`, `COG`, JLH/JLL)
  - tests under `src/core/tests`
- `src/app/` (Qt application)
  - `MainWindow` owns the `Document` and drives editing actions, undo/redo, startup flow, resource workflows,
    query/find, category editing, and UI refresh
  - `GlView` is the OpenGL viewport: rendering, picking, overlays, gizmos, selection, preview capture, and navigation
  - reusable UI widgets under `src/app/src/ui/`, including shared picker/category-tree widgets

## Editor data flow

- Authoritative state: `sed::editor::DocumentState`
  - `sed::Level level`
  - `sed::editor::Selection selection`
- Undo/redo: `DocumentUndoHistory` with compact entity subsets, full snapshots, and dirty flags
- Viewport flow:
  - `GlView` renders the current `Level` and emits semantic interaction signals
  - `MainWindow` applies mutations, pushes undo records, and issues the smallest practical viewport refresh

## Current implementation highlights

Native SED V2 is the current project format for a normalized Level without retained MotS arch-lighting objects or
authored bake baselines. V3 is the normalized format when either remains; its table field may be empty when only bake
provenance remains. V1 and V4 are the corresponding legacy texture-pixel formats without and with that arch data.
The wire contract and the editor's failure-atomic legacy-pixel admission/overwrite policy are
documented in [sed_binary_format.md](formats/sed_binary_format.md); any incompatible byte-level change must deliberately
update that normative contract/version.

### Startup flow and new projects

- App startup can now stop at a dedicated start screen before the editor is shown.
- Start screen supports:
  - new project by configured project type
  - open level
  - jump to preferences/configuration
- The start screen now follows a Visual Studio-style layout with a searchable recent list, clearer `Open...` action,
  project configuration shortcuts, project/file-type icons, and pin/unpin support.
- Recent files are updated after successful open, native SED save, and engine export; export leaves the active native
  SED save path unchanged. Pinned entries stay above normal recents, new pins are appended below
  existing pins, and unpinned entries fall back to last-used ordering.
- Closing the startup screen exits the app instead of falling through into a half-configured editor.
- The startup screen has no operating-system title bar. It uses a frameless rounded translucent shell and a minimal
  client-drawn top bar. Its explicit Close control remains fully visible in the far-right lane. The window fits its
  initial geometry inside the selected monitor's available area, including high-DPI logical screens and secondary
  monitors with negative origins.
- New project seeding now creates:
  - one exact `2 x 2 x 1` starter sector with ground at `z = 0`, matching Pascal `TJedMain.NewProject` room bounds
  - one project-specific player thing
  - one centered white baked light with the default intensity `10` and range `2`
- Layers without an explicit valid editor color default to opaque white. New-project creation resets all seeded layer
  colors to white instead of inheriting a previous global vector; colors explicitly selected inside the current
  document remain preserved through layer reorder/Undo/Redo.
- Room bounds match Pascal `NewProject` for all projects. JKDF2 and MotS also retain the classic face anchors and
  `RecalcAll` UV initialization; IJIM keeps its project-specific face order and `RecalcNewAll` initialization. C++
  deliberately keeps project-aware player templates, a resolved default CMP/material, and normal player
  insertion-offset handling.
- The main editor starts maximized by default, remembers window geometry/state, and shows the taskbar/app icon as early
  as the startup dialog.
- The main editor title bar shows only `<path>*`, or `Untitled*` for a new level; the `*` appears while the level has
  unsaved changes. Product branding remains in the startup dialog instead of being repeated beside every path.
- A small bottom-right status marker shows the current project type without project-colored chrome.
- The project defaults and starter seed are shared, directly testable helpers. Focused checks cover JKDF2/IJIM headers,
  exact starter geometry/player/light data, and the top-down camera/fit math. MainWindow coverage also proves that a
  startup-hidden viewport keeps its refit pending, that its timer quiesces while hidden, and that showing the editor
  restarts the deferred refit. Final visible dock timing still depends on Qt's native `restoreState()`/splitter event
  ordering and persisted window geometry, so it is intentionally an interactive check together with the axis gizmo
  and final resource-derived player insert offset rather than a synthetic timer-duration assertion.

### Autosave and recovery

- With recovery enabled, startup and every document switch publish a coherent baseline even before the first edit. This
  pre-existing checkpoint is what protects Task Manager hard-kill and power-loss cases, where no termination callback
  can run. Authored Level/COG/resource/history and settled Cleave mutations use a 650 ms debounce; editor-only camera/tool/layout changes mark the
  live checkpoint and wait for the periodic tick (or an explicit shutdown flush), avoiding a full Level+HIST encode for
  every orbit/fly gesture. The enabled-by-default five-minute Preferences interval remains a 1–30 minute fallback.
- Same-document changes arriving during an active write no longer invalidate that coherent write. It publishes first
  and the queued newest state follows, ensuring continuous edits on a large map cannot indefinitely postpone creation
  of the first durable checkpoint. Save, clean-state removal, and document switches still cancel obsolete writers.
- Snapshots prefer `<directory containing sed.exe>/recovery/<readable-name>-<project>-recovery-<family-token>.sedrecovery`,
  where the family token is a single 26-character lowercase-base32 128-bit identity/session digest;
  if that location cannot prove
  atomic replacement, SED tries per-user local application data, `%USERPROFILE%/.sed/recovery`, then a last-resort
  temporary directory. Later launches scan all known durable/fallback locations, including the legacy portable
  directory. Saved documents hash their normalized absolute path, while Untitled documents hash a
  per-session UUID. `sed.ini` remains beside the executable.
- Recovery-envelope V1 wraps one complete native SED V1 through V4 in its authoritative `docm` chunk, selected from the
  captured Level's explicit pixel or normalized UV-space tag and retained MotS arch-lighting objects or bake baselines.
  The native document contains the
  lossless Level, `edit` selection/workspace/COG state, and `hist` undo/redo timeline. It therefore captures modes,
  cameras/rendering, layers, window/dock/tab layout, every named persistent splitter, UV repeat/snap/grid controls,
  active tools, Thing placement/brush state, current Multi-cut lines/context/order/selection and retained local history,
  checked Apply/Layer attachments, settled Arch channel/target/vertex selection, and open COG/resource buffers without
  separate outer copies.
- The UI thread takes one coherent `Document` value snapshot and a bounded CBOR editor-state block. Binary SED serialization,
  undo expansion, section compression, writing, flush, and atomic replacement happen in a worker. Restored history is
  converted back to compact dirty-domain snapshots. Direct `.sed` Open uses
  that compact consuming path as well; the expanded history DTO is inspection
  only. Adjacent nested states reuse unchanged Level vectors through a
  failure-atomic domain transaction; compact import derives dirty-domain and
  Thing/Sector-subset facts from that transaction and never copies a complete
  previous Level for ordinary Thing/light/COG/geometry records. Save-side history pooling no longer semantically decodes each freshly
  emitted nested Level a second time just to recover its section boundaries;
  strict untrusted splitting remains unchanged. Exact SectorSubset and
  SurfaceSubset transitions additionally reuse unchanged checked `sect`
  records and serialize only the touched sector indices; full, baked-light,
  or topology-changing Sector transitions retain the complete encoder.
- Normal SED/JED/JKL/NDY and COG Save are also staged with `QSaveFile` and direct-write fallback disabled. Their recovery
  state is cleared only after the atomic commit succeeds. If Level Save succeeds while a COG tab is still modified,
  the prior checkpoint remains until a replacement containing that COG buffer is atomically published. A complete native
  SED Save already contains Level, `edit` (including unsaved COG buffers), and `hist`, so it gate-retires the stale live
  offer immediately and republishes a clean non-prompting hard-kill baseline asynchronously without deleting periodic
  history.
- Stable path identities and UUID-backed Untitled identities keep documents separate. Generation validation prevents
  an older worker from publishing after a newer edit, Save, document switch, or close.
- Startup and matching level-open paths never hide a valid snapshot just because the source also changed. The dialog
  labels that conflict; recovery never overwrites the level automatically and remains dirty until explicit Save.
- Automatic prompts consider live checkpoints whose checked metadata marks authored Level/COG/resource or pending-Cleave
  work, or a recovered
  document that still requires an explicit Save. Editor-only camera/tool/UV/layout changes update the live hard-kill
  checkpoint but do not make the document dirty, do not set prompt eligibility, and do not rotate periodic history.
  Clean hard-kill baselines are ignored; foreign-session baselines are
  never deleted without reliable ownership/liveness proof.
  Modified COG buffer content uses a separate saved digest because native SED Save embeds that content in `edit`; the
  editor may remain externally modified without making the post-Save checkpoint eligible until its text changes again.
  Pending cuts use their own saved authored projection: geometry/frames/links, targets, origin, execution order, and
  Bounded while lines exist. Selection/camera/guides and unchanged authored lines at another local history cursor do not
  make it dirty. Native Save checkpoints the exact captured draft without requiring Apply.
  Replacing a changed live point atomically archives its previous checked bytes first, commits the staged live point
  second, and prunes only after both exist; `File > Open Recent > Autosave History` exposes up to 10 full native SED
  restore points per document identity/session family. Recover/Open Saved is a
  two-phase transaction: target selection and current Save/Discard/Cancel are non-destructive until replacement commits.
  Successful Open/New gate-retire exact superseded live paths. Startup Discard removes only the selected session and
  suppresses older same-document live prompts for this editor run, preserving concurrent-session files for history and a
  future launch. Open's Save branch protects its exact selected recovery through Save cleanup, including same-session
  recovery, and releases it only after the replacement transaction reaches its commit boundary.
- COG replacement prompts form one checked transaction: every answer binds the exact tab content it resolved, later
  modal-loop edits abort replacement, and Open/recovery rechecks the authorized Level+COG fingerprint immediately before
  installing a loaded document.
- Native SED sections attempt compression exactly once only in the inclusive 4 KiB..64 MiB range and retain it only when
  smaller. The deterministic heap matcher checks at most four chained candidates per position with a bounded comparison
  budget and abandons a losing candidate before it grows to the raw size; this reduced the reference `test.sed`
  canonical re-save by 8.79% without changing native tokens. Sections then use
  stream-major fixed 4096-byte slots
  (4092 data + CRC32C). Adaptive `k <= 16` systematic GF(256) Vandermonde/MDS stripes use `p=4` for the eight required
  Level sections and recovery-critical `edit`, and `p=2` for lower-priority `hist`/future optional sections; the repair
  budget applies independently to every stripe. One
  expanded SHA-256 catches CRC-consistent corruption but cannot localize its slot. Checked 100-byte `sectv1\n\0` and
  116-byte `sedmir1\n` descriptor copies repair framing and permit checked resynchronization. Native SED has no FEC
  tables, descriptor directory, record-group shards, whole-file CRC, or duplicate stored/raw CRCs. Header/descriptor
  mirrors are CRC-checked outside payload MDS; alignment and unused slot-tail padding are canonical zero (strict rejects,
  corruption recovery normalizes/reports); the footer is only a marker. An over-budget
  stripe drops/defaults that complete semantic section. Fixed-slot repair covers byte/bit substitutions and
  unreadable/truncated ranges only while checked framing preserves the absolute 4096-byte phase; a mid-section byte
  insertion/deletion shifts later slots and is outside all native versions rather than guessed or rephased.
- `hist` uses a semantic shared pool (`sed-semantic-base-delta-v1`) rather than independent complete Level byte strings.
  Each undo/redo stack has canonical reference-list splice deltas with a full checkpoint every 16 states. New writers use
  `sect-record-v1` for the large `sect` frame so unchanged Sector records remain reusable when neighboring records move;
  other sections use fixed 8192-byte fragments, with a bounded writer-only hybrid shifted-tail layout for safe size changes
  and deterministic `fixed-v1` fallback when the record layout would exceed its reference budget. Nested states retain
  canonical framing, mirrored descriptors, and payload CRC32C through the explicit `HistoryNested` profile, while the
  enclosing optional `hist` section owns the strong expanded SHA-256, compression, and `p=2` protection once. HIST is
  optional/discardable, so p=2 adds 12.5% parity relative to a 16-slot data stripe instead of p=4's 25%; authoritative
  Level sections and `edit` (including unsaved COG source) remain p=4. Pool entries are
  unique, first-use ordered, fully referenced, and decoded history is moved across recovery handoff rather than copied.
  Save-side traversal now derives required-section invalidation from the concrete undo Snapshot variant and shares exact
  generated HistoryNested frames in independent undo/redo caches. Snapshot-derived invalidation crosses into the writer
  only through a noncopyable token bound to the exact temporary Level being visited. The inner frames use canonical
  CRC32C while the
  enclosing HIST retains SHA-256 plus FEC. It always rebuilds/compares `rsrc`, broadens a
  catalog change to every indexed section, and commits a cache only after the entry succeeds. The matching native HIST
  reader strictly validates each canonical required-only nested document but semantically decodes only changed exact
  checked frames. It always decodes `rsrc`, broadens a catalog change to every indexed domain, repeats final graph and
  canonical-catalog validation, and never uses persisted dirty flags to authorize reuse.
  A representative Release benchmark over a deterministic 77-entry, topology-preserving Sector timeline measured
  about 1.3 seconds to encode HIST and about 1.6 seconds to strictly decode it. Changed-frame interning touched roughly
  1.2 MiB; the preceding whole-frame path had scanned about 511 MiB and took about 6.4 seconds to save on the same
  machine. These figures describe the relative hot-path improvement, not a portable performance guarantee.
- Native Save normalizes frame counts in the candidate Level, then remaps selected and active EDIT frame references
  by logical path ordinal. The captured current selection and HIST snapshot use that same candidate, so inserting
  `numframes` cannot make a saved frame selection refer to another value. This does not mutate the live document.
- Manual `.sed` Save keeps Level+`edit` authoritative. It attempts `hist`, but an expected history encoding/semantic/64 MiB
  validation failure omits only `hist` and emits a Messages warning; allocation/logic failures and any Level, `edit`, or
  atomic-write failure remain hard. Recovery/autosave follows the same data-priority rule and now performs the same
  bounded compact-HIST semantic admission on its worker before publication; a stream that would be rejected on the next
  open is omitted rather than published. Any degradation is persisted in `warn`.
- A repaired or partial source receives a unique `.corrupted-*` forensic copy while its selected original remains
  discoverable until a clean checkpoint is atomically published. Structured required-Level loss also schedules a
  non-authoritative JKL/NDY plus manifest beside the forensic binary; unidentified lost units do the same with an exact
  uncertainty manifest, while identified optional-only `jimp`/`edit`/`hist` loss does not. The text
  contains only fully decoded semantic domains/defaults, never residual records from an unrecoverable section; the
  forensic binary remains the only source for those bytes. Transient publication failures retry with bounded exponential
  backoff.
- Direct corrupted `.sed` Open is strict first. Complete repaired/identified-optional-loss documents remain associated
  but dirty; required loss or an unidentified damaged unit prompts before candidate resource/settings/snap-cache
  mutation or old-document teardown and opens only as detached `Untitled*`. Unidentified units are conservative because
  they may conceal required Level data. Cancel leaves document/path/resources/source unchanged. Direct salvage is written beside the
  unchanged source after a worker re-read confirms size/time plus normalized loss/project classification. If both fixed
  headers and `head` are lost, the codec reports no trusted project and one combined dialog requires an explicit
  JKDF2/MotS/IJIM choice before history, resources, application, or JKL/NDY selection; synthesized JKDF2 is never trusted.
- The outer recovery envelope keeps `docm` raw rather than duplicating native compression/parity. It supplies primary
  discovery JSON, a parity-protected `meta` mirror, and optional `warn`; the native `docm` flag requires zero outer
  CRC/SHA fields. Only `meta` retains the outer 16+2 P/Q/table scheme with IEEE CRC-32. Discovery can scan past a damaged
  outer chunk header to native SED anchors and validates the native document before offering it.
  Writer-side metadata, project-type, size, arithmetic, stream-status, byte-round-trip, and semantic checks fail before
  atomic publication. Native recovery exposes checked project identity as optional/trusted state; when both fixed headers
  and `head` are lost, the default Level kind is only a placeholder and project-dependent restoration/export must wait
  for an explicit choice or remain blocked.
- If an ordinary recovery read is short/fails, each 4096-byte range is retried three times; persistent failures are
  zero-filled as erasures and reported as coalesced unreadable ranges. Partial recovery names missing required/optional
  domains, defaults or omits each lost section as a whole, and keeps checksum-valid required Level schema errors hard.
  Invalid optional `edit`/`hist` recovery content is rejected atomically and falls back with a warning and clean-save
  requirement; it does not veto or detach an otherwise intact Level.
- Native `.sed` and recovery containers now share a practical 256 MiB complete-file cap; individual native sections are
  capped at 128 MiB, aggregate expanded section bytes at 256 MiB, each retained decoded Level at 128 MiB/two million
  items, and the optional history timeline at 768 MiB/32 million items,
  `hist` at 64 MiB, and outer `warn` at 1 MiB. Recovery
  applies type-specific admission before copying/decompression and bounds damaged marker/candidate fallback by count and
  cumulative bytes. Full recovery input uses an exact-size allocation with bounded 1 MiB device transfers plus a one-byte
  growth probe, and canonical `docm` is a
  non-owning view until native validation accepts one owned copy. `edit` is streaming-preflighted before Qt CBOR
  materialization (dynamic amplification with a 128 MiB absolute graph/verification cap, 2,000,000 nodes, depth 64,
  bounded strings and unique text map keys, one canonical value). A focused Clang
  libFuzzer target instruments the native codec under ASan/UBSan and leaves unexpected exception families as findings.
- Recovery discovery uses separate suffix-filtered live and historical scans, each bounded to 4,096 matching candidates
  and 256 retained descriptors. A full live-slot budget or unrelated directory entries cannot crowd periodic history
  out of its slots. It skips a canonical `docm` extent without materializing it and still
  detects a conflicting protected `meta` after the document. Metadata-loss salvage reuses its already decoded `docm`.
  One optional `warn` is materialized; redundant `docm` validation, `meta` descriptor count, partial-salvage manifest output,
  and the rotating recovery audit log all have explicit caps. Forensic outputs visibly escape untrusted control and
  bidirectional-format characters before applying those caps. Captured `edit` runs through the complete restore schema
  non-mutating before autosave/recovery publication or manual `.sed` replacement.
- Recovery COG buffers bind to their captured normalized absolute paths, so duplicate script basenames in different
  resource roots cannot restore into or later save over the wrong file.
- Ordinary Open stages the candidate resource roots, mounted archives, and locator separately; parse failure, prompt
  cancellation, or a post-authorization Level/COG edit leaves the old document and its active resources untouched.
- Returning exactly to the saved state (including Undo or explicitly discarding the last modified COG buffer)
  invalidates stale in-flight state and republishes a clean workspace baseline. A successful Save likewise retires the
  pre-save generation before publishing the new saved baseline; resolving one COG tab updates only that buffer's
  baseline entry. Clean application close removes only the live session checkpoint; periodic `.sedautosave` history
  remains available through File > Open Recent > Autosave History, including when the close prompt chose Discard.
- Windows restart/shutdown forces a final coherent snapshot, drains queued work, and makes two immediate retries after
  publication failure (three attempts total) via Qt session management. Destruction that bypasses the normal close event
  performs the same final capture, and COG pages are destroyed while their owning `MainWindow` state is still valid.
  WER/restart plus SEH and `std::terminate` exception
  marker/minidump hooks are installed; Task Manager hard termination cannot execute callbacks and relies on the last
  live atomic checkpoint. The coherent UI-thread Document copy can still be measurable for an exceptionally large
  map/deep history.
- Recovery offers use a non-resizable content-sized top-level window with an application taskbar icon. When more than
  one current snapshot exists, **Other Recoveries** expands a selectable list; Recover/Discard applies to the selected
  entry. Decoding and integrity repair run on a worker behind a taskbar-owned progress window.
- A successfully restored editor-state payload cancels the constructor's deferred initial camera refit, so the saved
  Editor/Game Preview camera is not overwritten after the window appears. Recovery also tears down ambient playback
  before and after replacing the level/resource locator, then reevaluates Sector audio from the final camera.

### Add Sector drawing and reusable shapes

- Add Sector is now a Blender-style drawing workflow:
  - press-drag the 2D footprint
  - release LMB to adjust height
  - click to commit the sector
  - `Esc` cancels the in-progress draw
- `Shift` constrains the footprint to equal sides and constrains height to match the footprint; releasing `Shift`
  returns to free movement.
- The non-modal Add Sector options overlay stays visible while placement is active:
  - shape selector
  - start plane axis (`X`, `Y`, `Z`, default `Z`)
  - **At view** for camera-facing input with global-axis output, and **Free** for
    genuinely camera-oriented output
  - default-off **Grid Snap**, shared with the move tools and temporarily inverted by Ctrl; it honors transformed-grid
    origin/orientation plus the adaptive minor spacing visible at placement start. Hard snapping keeps minimum and
    maximum dimensions on whole grid steps, leaves a same-cell footprint invalid, and takes precedence over Shift
    when both constraints cannot be satisfied
  - sides/segments for cylinder, cone, and sphere-style primitives
- Built-in shapes include cube, cone, cylinder, UV sphere, and ico sphere.
- Original SED/Pascal `data/shapes.tpl` shapes are available under the user-facing `Shapes` menu.
- Release packaging also ships the JKDF2, MotS, and IJIM template databases plus their category files. Data-root
  discovery prefers this complete set over a nearer partial `data/` folder, preventing empty Template Editor/picker
  lists.
- `Save Selected Sector as Shape` is available when exactly one sector is selected; it appends an auto-named shape to
  `data/shapes.tpl`, selects it, and keeps Add Sector active.
- Idle Add Sector mode shows a softened cursor-plane guide with a center cross so the placement origin is visible
  without implying that the current shape is circular.

### Large-level Thing placement

- The placement panel has a Layer selector. **Auto** inherits the visible target Sector layer and otherwise uses the
  last visible layer, eliminating the old implicit layer-0 assignment; an explicit layer remains available for
  intentional overrides. Single and brush paths share the resolver, and layer reorder/compaction remaps an active
  explicit choice. The control is a dedicated row below **Snap to Thing** and **Rotate to Align**; placement-mode and
  snapping tooltips are complete and no longer expose the internal `No key:` wording.
- Single and brush placement previews use recorded transient mesh ranges. Stable cursor motion, rotation, Sector
  changes, and lighting refreshes no longer rebuild every persistent Thing.
- Repeated surface queries use a cached geometry BVH, followed by the unchanged exact legacy plane/polygon test.
  Ambiguous, concave, degenerate, or extreme-tolerance surfaces are always tested exhaustively.
- Free-placement and brush containment test the previous/hovered Sector first, then reject non-candidates through
  cached per-Sector bounds before running the exact containment test. Hidden layers still participate in authored
  Thing-sector assignment even though hidden surfaces are excluded from surface picking.
- Placed-Thing boxes/connectors and the active template's placement data are resolved once per valid document/resource
  context instead of once per pointer sample. Existing low-level filename-only asset caches remain a same-name live
  replacement caveat.
- One brush gesture is one compact append-tail Undo command. Each stamp remains visible and selected live, while
  Outliner and Properties publication is deferred to the gesture boundary. The boundary appends only new Outliner
  rows and refreshes consistency/resource checks only for the new Thing indices.
- Initial preview creation, representation/count changes, and invalid/valid transitions retain a conservative rebuild.
  Stable placement-preview SPR movement regenerates only its indexed six-vertex cards in a preview-only material
  batch and uploads those subranges. Persistent Thing translation/rotation uses the same owner-local update; cached
  per-instance AABBs avoid scanning a shared material batch unless a moved owner supplied one of its extrema. A
  committed SPR append reuses all unchanged persistent cards by pick ID and generates only the new owner plus any
  active previews before the required topology upload.

### Join, merge, and portal editing

- Surface and sector tools use short user-facing names:
  - Sector mode: **Join sectors**, **Merge sectors**
  - Surface mode: **Adjoin**, **Unadjoin**, **Merge**, **Connect surfaces**
- Internal core functions still use the legacy `connect` / `adjoin` terminology, such as `levelConnectSurfaces`,
  `levelConnectSectors`, and `levelUnadjoinSurface`.
- Sector **Join sectors** previews the result for two or more selected sectors before the edit is committed.
- The **Keep overlap pieces** option controls the knife-sector behavior:
  - enabled keeps overlap pieces produced by the cut
  - disabled uses the knife as a cutter only, removes overlap inside target sectors, and keeps the holes/adjoins
- The cutter-only path has had additional cleanup passes for rotated and complex knife intersections so leftover knife
  fragments inside target sectors are removed and missed back-to-back portals are rechecked/adjoined.
- Geometry Move/Rotate/Scale uses default-on **Preserve UVs**: rigid faces remain exact while scaled faces and neighbors
  deformed through shared vertices are reprojected from their original texture basis. Unmoved corners and baked-light
  data stay exact, reciprocal adjoins are included, and the checked preview/commit remains one Undo operation in either
  normalized or legacy pixel UV space. Flatten and an explicitly disabled preservation option retain authored UVs
  unchanged. Surface, Edge, and Sector cleaves retain original corners and interpolate only genuinely inserted cut
  corners in each face's own UV/light frame; newly created cap faces receive a fresh default mapping.

### Surface extrude workflow

- Surface **Extrude** is now an explicit preview workflow instead of an immediate one-shot edit.
- The top-center options strip exposes `Length`, `Expand`, `Apply`, and `Cancel`; `Esc` cancels.
- The live preview builds a temporary sector-shaped preview without mutating the level until `Apply`.
- The source surface is hidden for normal/larger extrudes; negative expand can preview the source surface with a
  temporary cut-hole overlay when the new sector is smaller.
- Expand limits are derived from the selected surface so the spinbox cannot step through the collapse boundary.
- Applying an extrude resets Expand to zero and retains Length for repeated extrusions.

### Resource discovery, categories, pickers, and template browsing

- Generic resource picker exists in `src/app/src/ui/resources/resource_picker.*` and supports:
  - `COG`, `MAT`, `3DO`, `KEY`, `WAV`, `AI`, `TPL`, `MSG`, `CMP`, `SND`, `PUP`, `SPR`, `PAR`
- Picker storage metadata displays the absolute loose path or archive basename plus case-preserved entry without
  an extra Filesystem/GOB/GOO prefix. It appears inside text/audio metadata and below image/rendered previews.
  Bold list names indicate available loose copies independently of the currently selected source.
- Sprite, Sound Class, and UNI authoring contracts live in [resource_authoring.md](resource_authoring.md). Both
  Sprite profiles expose authored Geometry mode names and the isolated preview follows values 0–4; regular IJIM
  viewport rendering retains its engine-global geometry path. Default Sound/Volume fitting first assigns Sound 65% of
  width beyond column minima, then transfers 40% of the resulting Volume width into Sound down to Volume's 230-pixel
  minimum. Manual resizing remains available; playback buttons retain 22-pixel hit areas and 13-pixel icons at 2x.
- A saved level's project directory may override the shared `data/*.jlh` / `data/*.jll` category pair; shared data
  then mounted resources remain fallbacks. Category edits copy/write the active project-specific pair beside the
  level when a project directory is available.
- Category support exists for:
  - templates (`tpls.jlh`, `mtpls.jlh`, `ijimtpls.jlh`, matching `.jll`)
  - materials (`mats.jlh`, `mmats.jlh`, `ijimats.jlh`, matching `.jll`)
  - sounds (`wavs.jlh`, `mwavs.jlh`, `ijimwavs.jlh`, matching `.jll`)
- Category trees are editable in both:
  - picker dialogs
  - template editor
- Category editing now supports:
  - add category
  - add subcategory
  - add leaf category
  - rename
  - delete with confirmation
  - inline creation/rename
  - blank-entry removal on unfocus
- When no WAV category pair exists, the picker seeds an ordinary editable nested `Sounds > Uncategorized` tree from
  the active project's discovered WAV names. The filename remains game-specific (`wavs`, `mwavs`, or `ijimwavs`).
- WAV categories are project-local when a saved project directory is available and use the same arbitrary nested
  structural/leaf hierarchy as the other category-backed resource pickers.
- `All` is now a normal top-level entry instead of the parent of every category.
- Structural vs assignable categories:
  - structural category = branch/grouping only
  - leaf category = JLL-backed assignable list
- Membership editing exists through right-click list actions and drag/drop:
  - multi-selected list items are assigned together using their real resource/template names instead of only the
    pressed row or decorated display label
  - dropping onto a category no longer changes the current category view
  - `Ctrl` while dragging from a non-`All` category moves entries out of the current category into the target category
  - batched assignment reduces UI stalls after mouse release
  - drag feedback includes action/cursor feedback and an Explorer-style thumbnail pixmap
  - rectangle selection and multi-select cleanup after drag/drop were tightened
  - picker/template membership edits are copy-first JLH/JLL transactions; no-op, failed-save, and exception paths
    restore the exact prior graph, including shared list identity and any would-be hidden `General` child
  - disk pair persistence stages and checks both files beside the destination, backs up both originals, and restores
    both paths after a reported install failure. If rollback itself fails, the exception names the retained recovery
    directory. This is not one OS-wide atomic two-file operation: power loss may interrupt the rename sequence, and
    best-effort cleanup after success can leave an obsolete transaction/backup directory
  - focused direct coverage exercises action resolution, complete MIME payloads, stale-selection cleanup, a synthetic
    `Ctrl` multi-item drop with stable current view, case-insensitive Copy/Move/deduplication, rollback, and save/reload
- Base templates (underscore-prefixed) can be hidden from thing-template picking and this is preference-controlled.
- Picker windows paint an opaque dark loading shell before any resource enumeration, then defer source/list population
  to the first event-loop turn so opening a large material set no longer presents a white or half-built dialog.
- Initial source-tree notifications are blocked so the list is populated exactly once.
- Disk-backed resource emphasis now comes from a shared revision-keyed directory index rather than repeated per-row
  disk probes, substantially reducing material-picker startup latency while preserving disk overrides in bold.
- WAV list selection is a filled marker limited to the rendered filename width and painted independently of native
  hover state, avoiding both the extra hover stripe and the column-wide selection outline.
- Wrapped list mode derives an integral column count from the current viewport, scrolls per item, rounds column widths
  so any right-edge overhang remains inside trailing content padding, and keeps each item's size hint equal to its grid
  cell so the first column and final row stay inside the viewport.
- WAV and other non-model pickers hide the model-only projection, render-style, and **Fit** controls.
- Category-delete confirmation text is left-aligned and vertically centered; **No** is both the default and escape
  choice, and the native caption keeps enough minimum width to display its full title.

### NDY/JKL export capacities

- **Sith Editor File (`.sed`)** is the sole native Save/Save As format for every project and holds the complete
  recovery-grade workspace plus undo/redo in checked `edit`/`hist` sections, not only the `Level`. Opening it restores
  cameras, selection, tools, rendering, windows/docks, unsaved COG buffers, and history without the engine-import view
  reset. This is stable workspace/tool state plus staged Multi-cut data, not a replay log for every held gesture: active
  Surface Extrude previews and partially held brush/draw or other pointer gestures finalize or cancel at snapshot/apply
  boundaries. IJIM runtime export exposes **Infernal Machine Level File (`.ndy`)** but not `.jed`; JKDF2 and MotS retain
  `.jkl` and legacy `.jed` exports, and all engine/legacy imports retain their established reset/refit behavior.
- Resource crawling and section-capacity semantics were re-audited against Pascal `SAVEJKL.INC` and the OpenJones3D
  loaders. Resource-pool headers are literal Required + editable Extra; geometry, colormap, Sector, editor Light/Layer,
  and MotS ARCHLIGHTING headers remain exact record counts.
- `World cogs` no longer uses the original Pascal proxy of placed COGs plus unique script filenames. Its automatic
  minimum is placed COG rows plus every explicit nonempty/non-`none` `cog=` occurrence on each emitted template and
  placed Thing. Duplicate script names allocate distinct instances; an inherited-only template pointer does not.
- Level Settings exposes an advanced **Extra buffers** dialog for the twelve safe resource pools. It invokes the real
  export dependency crawl without serializing geometry and displays Required, editable Extra, and Export total.
  New projects seed the legacy headroom as editable defaults; zero is honored, and NDY/JKL imports infer Extra as the
  declared capacity minus parsed required records. Extras persist in optional JED `SEC: EXPORTBUFFERS`.
  IJIM NDY export requires Materials Extra to be 0 or 64, matching the retail reader constraint; the default is 64
  beyond required dynamic MAT rows. Import, native persistence, and report-only measurement retain other authored
  values so Level Settings > Extra Buffers can repair them. Export rejects an incompatible value without clamping,
  and zero total capacity is not rejected solely for being zero.
- `Untitled_test.ndy` is the motivating parity case: three unique COG scripts, four explicit template uses, and a
  legacy `World cogs 3` header that makes the last `actor_indy.cog` allocation fail. Correct export reserves at least
  four instance slots.
- Sector cleave now treats the body containing the original Sector center as the retained identity. The original
  `id`/`num` and Sector COG links stay on that main body; the appended cut-off body is the one selected for fast Delete.
- Startup recent activation is single-routed through `itemActivated`; double-click no longer starts two `.sed` load
  transactions or produces a post-load duplicate autosave offer.
- Fly navigation caps displacement after a delayed timer frame, while ambient sector audio caches the last camera
  position/sector/resource and does not repeat containment, resource lookup, or playback setup on unchanged ticks.

### Preview/thumbnails

- Template editor, template picker, and raw `3DO` picker now share more of the preview pipeline:
  - CMP palette cache
  - MAT preview image cache
  - parsed `SPR` cache
  - sprite preview image cache
  - parsed `3DO` cache
  - rendered thumbnail cache
- `3DO` list thumbnails are fully prepared off the UI thread: scan/parse, hierarchy/bounds, referenced MAT/CMP
  read/decode, shaded/wireframe QImage rasterization, and revision-cache publication all run in one of two bounded
  workers. The UI thread only converts completed images to list icons, while selected interactive previews remain
  renderer-backed `GlView`s and reuse the same prepared asset caches. New image-to-icon conversions publish one per
  paced 40 ms turn, while local icon-cache hits retain the configured batch limit; GUI handoff uses the worker's exact
  epoch-scoped revision key rather than re-statting every dependency
- pending list rows sleep while workers are active and resume from completion callbacks. Finished fixed-size tiles
  update their icon/state atomically and repaint only their own rectangle, avoiding repeated list-model layout work and
  keeping the selected `GlView` preview responsive while a newly paged row fills. New visible dependency watches are
  added incrementally, so later pages do not rescan the session's accumulated model/material watch set
- Preview revision identity follows the exact disk file or mounted GOB entry selected by `ResourceLocator`. Rendered
  3DO keys include the model, active CMP, and all referenced MAT revisions. A shared debounced watcher wakes the main
  viewport, raw/template picker, Template Editor, and Thing-placement previews for loose `3DO`, `SPR`, `MAT`, CMP, and
  mounted GOB/GOO changes, clears complete parsed/rendered caches, and requeues visible ready rows. Archive files and
  their parent directories are watched so deletion/atomic replacement/reappearance is detected. Reload parses every
  mounted archive into a temporary set and swaps only after all indexes succeed; picker/template dialog locator copies
  wait for bounded background model-prep reads before remounting. Unrelated sibling writes do not trigger a remount.
- Thing-placement 3DO/SPR queueing, cache polling, dependency watching, and decoding use the same resolved Level CMP.
  This includes loaded JKDF2/MotS levels whose raw Master CMP is empty and falls back to the first Sector CMP or
  `dflt.cmp`, preventing a permanent `Loading` state caused by producer/consumer cache-key disagreement
- Loading placeholders, `No preview` placeholders, keepalive reloads, and self-healing visible-row refreshes are in
  place.
- Picker/list scrolling is smoother and more consistent across resource pickers and template lists.
- Rapid resource-search edits are coalesced before one bulk row replacement. Filtering, sorting, and thumbnail-layout
  changes retain an unchanged selected preview and do not recreate the raw-`3DO` dependency watch plan.
- Raw `3DO` and template pickers construct an empty hidden `GlView` host before first Show so their initial native
  mapping already supports `QOpenGLWidget`; this avoids destroying/remapping a visible modal later. Model reads, scene
  binding, GPU uploads, and interactive activation remain selection-driven after the delayed dark-shell paint. An
  initially selected raw `3DO` queues before background thumbnails, and a dialog-owned 30 ms cache retry complements
  the worker-completion callback so a delayed notification cannot leave the completed selection on its loading page.
- Picker, Template Editor, Thing-placement, and hidden thumbnail views deliberately show authoring assets whose
  inherited gameplay flags are Disabled or Invisible.
- Explorer-style tile selection/highlighting is used in picker-style lists.

### Sound preview and sector ambience

- Sound preview and viewport ambience use the cross-platform Qt audio path in
  `src/app/src/ui/audio/app_sound_player.*`.
- Qt 6 decodes WAV/INDYWV to PCM and feeds `QAudioSink` directly; Qt 5 retains a `QMediaPlayer` fallback.
- IJIM `INDYWV` resources are converted to canonical PCM WAV in `sed_core` before playback.
- The sound player distinguishes picker-preview and viewport owners so a modal picker can temporarily own playback
  without permanently disabling sector ambience.
- Picker preview metadata runs through the same ordinary-WAV/`INDYWV` PCM decode path and displays duration plus
  Mono/Stereo/channel layout instead of raw byte size.
- Picker previews expose **Replay**, checkable **Loop** (off by default), and an immediate volume slider. Replay
  discards queued audio and starts from the beginning; changing Loop immediately rebuilds the selected voice from the
  beginning as looping or finite playback. Picker volume is independent of ambient sector volume and persists through
  `QSettings` for later dialogs and sessions.
- Qt 6 one-shots use a sequential, unbuffered PCM device with one sink-buffer of format-correct silent drain padding,
  preventing EOF from stranding low-rate PCM in the sink/resampler queue. Replay and replacement use
  `QAudioSink::reset()` before rewind, while looped picker/ambient voices remain unpadded and gapless.
- Sector ambience follows the camera's containing sector and honors independent persisted mute state for editor and
  game-preview pipelines.
- Ambient transitions match the legacy feel more closely:
  - new sounds fade from zero to sector volume over 500 ms
  - outgoing sounds fade to zero over 500 ms
  - the same sound at a new sector volume ramps without restarting
  - different sounds crossfade
- Decoded PCM uses a bounded 64-entry / 64 MiB LRU keyed by sound name plus normalized resource roots and archives.
- Viewport ambience reads mounted/loose bytes and converts INDIWV on a worker. Camera/document generation checks drop
  stale completions, avoiding UI and current-audio starvation when fly navigation crosses a new sound Sector.
- Remaining idle/fly hitches traced to resource-directory false positives and no-op recovery scheduling are removed:
  watched candidate directories now invalidate only when a dependency's resolved revision changes, so autosave or an
  unrelated sibling file cannot repeatedly clear the viewport and retry missing 3DO, SPR, MAT, CMP, or other registered
  ResourceLocator categories. Viewport ambient WAV loading separately negative-caches a failed resource key. Recovery capture pauses
  for active fly navigation, and a mouse/key release with unchanged EDIT state never reaches full Level/history copy.
- Picker previews intentionally remain immediate; Replay rewinds the current finite or looping voice even when it is
  still playing. Ambient fade timing and ownership behavior are unchanged.

### Shared UI styling and property editing

- Checks, Messages, and History now have stable outer-dock object names. Main-window state V1 gives one corrected
  equal-width Checks-left/Messages-right default, then preserves custom bottom-dock order and widths on later launches.
  A focused regression saves a reversed unequal split and restores it in a fresh MainWindow.
- The top viewport toolbar's Map Settings gear toggles a persistent viewport-owned panel. Actions, spin boxes, sliders,
  and color controls can be changed repeatedly without reopening it; a second gear click hides it. The panel remains
  bounded to the viewport and refreshes/repositions across viewport size and editor/preview state changes.
- Map Settings also owns a default-off **X-Ray Selection** toggle. In Editor mode, repeated same-position clicks walk
  front-to-back candidates behind occluding geometry in Select/Move/Rotate/Scale. An unmodified click skips the front candidate
  immediately when that candidate is already anywhere in the current selection; modifier-driven add/remove/path
  clicks retain their exact visible target. Select-tool and Alt-transform box selection also bypass scene depth while
  X-Ray is active, but continue to reject hidden-layer or wrong-edit-family entities. Cycling stays
  within the active edit family rather than crossing Thing/Surface/Light kinds. The `Auto` fallback similarly keeps
  an immediate skip within the front candidate's kind. Preview instead uses the unified point-only Auto policy
  documented below; it never starts a marquee. The toggle is persisted as
  `viewport/xray_selection_cycling`.
- **Orbit Around Selection** is enabled by default and available in both Viewport Preferences and Map Settings through
  one `viewport/orbit_around_selection` setting. Plain MMB uses the complete semantic selection as a drag-only pivot:
  it rigidly revolves both eye and look target, avoiding a press-time recenter and keeping the selected pivot fixed on
  screen. Deactivation now clears even an in-flight pivot and immediately restores the legacy fixed-camera-target
  revolution; empty selection, Shift+MMB pan, and Ctrl+MMB dolly retain the same prior navigation.
- Orbit review repairs added an exact zero-delta no-op, removed the pure orbit helper's accidental full
  camera linkage, and isolated the synthetic MMB fixture from the monolithic MainWindow test stack frame.
- Free orbit now continues through top/bottom instead of clamping at `+/-90`. One analytic camera frame is shared by
  the rendered view, pan, axis-widget and MMB drag, selection-pivot revolution, public camera axes, and camera-facing
  sprites: yaw owns screen-right across the poles, up is rebuilt from right/forward, and roll is applied to both.
  Pitch storage wraps per revolution but never changes orientation. Perspective and orthographic views therefore do
  not mirror at 90/270 degrees, including reverse drags, while canonical numpad/axis-widget Front/Back/Left/Right/Top/
  Bottom targets remain exact. RMB Fly remains zero-roll and world-upright with its original `+/-90` pitch clamp.
  At RMB press it canonicalizes the actual visible forward vector rather than reusing F2's banked/pole-crossed scalar
  angles, while retaining the eye and target (the exact grid origin after F2). Horizontal, vertical, and lateral Fly
  controls therefore cannot inherit inverted axes, including in Game Preview. This avoids porting Pascal's
  `TCamera.RotatePitch` clamp and its alternate independently-mutated camera-basis path; those are not part of the
  original F2 View-to-Grid, F3 Grid-to-View, or Shift+S Snap-Grid-to-Item contracts.
- F2 follow-up parity now applies vertical orbit input around the current *rolled* screen-right, not by incrementing
  scalar pitch while retaining a bank angle. This matches the useful part of Pascal MR_new, where F2 copies grid
  normal/X into renderer `zv/xv` and vertical `RotAxisAngle(nz, ny, dy)` preserves `xv`. The concrete regression uses
  the banked-grid regression frame for Sector 90, local Surface 27 (world Surface 786): its normal is approximately
  `(0.424434, 0, -0.905459)` and its normalized first edge is `(0.803168, -0.461714, 0.376486)`. Plain MMB in both
  projections and Orbit Around Selection preserve that edge as screen-right while pitching, including complete
  revolutions and reverse motion. The orientation helper recovers the nearest equivalent yaw/pitch/roll branch so
  numpad/axis endpoints and the prior pole-continuity behavior remain unchanged.
- Viewport Camera settings hide the complete FOV row whenever projection is orthographic and restore it for
  perspective. Visibility follows camera changes from the popup, keyboard projection toggle, and restored/programmatic
  camera states rather than only refreshing when the menu opens.
- Main-window chrome, dialogs, docks, menus, inset lists/editors, scroll bars, and title surfaces now use a shared dark
  hierarchy with enough contrast to distinguish the application canvas from interactive panels.
- Ordinary modal/tool dialogs now share a caption policy which removes minimize/maximize/help buttons and reserves
  enough width for the native title. `QFileDialog`, the app-like startup window, and any dialog explicitly marked
  `sed.allowMinMaxButtons` are excluded. Combo boxes never change by wheel; spin boxes and sliders do so only while
  focused. Hover-wheel input over a blocked editor scrolls its enclosing list/panel instead of focusing or changing it.
- Buttons, inline tool buttons, combo boxes, spin boxes, tabs, and disabled states use shared compact metrics and
  consistent hover/focus/pressed feedback; combo/spin arrows come from crisp embedded chevron assets.
- Property rows keep text compact without clipping ascenders/descenders, reserve uncropped space for material
  thumbnails, and render color values as clean swatches without a text-selection focus frame.
- Color swatches open a non-modal-feeling compact popup that previews hue/saturation/value/alpha changes continuously
  while choosing a color.
- Compact copy/paste feedback uses an opaque dark rounded card inside its green success or red error outline. Only the
  widget pixels outside the rounded card are transparent; the mouse-transparent card fades in and out without covering
  the adjacent action buttons. It is created as a non-activating top-level tool window at the action's global screen
  position, so scrolling or rebuilding the property tree cannot carry the visible feedback card with the row.
- Browse-capable property values can open their auxiliary editor through a value-field double-click as well as the
  explicit browse button.
- Property and query flag pickers share a live search over bit index, decimal/hex mask, name, and description. The Flag
  column shows the descriptive name while the Bit column alone carries the hexadecimal mask.
- The Outliner **Things** tab now uses a generic solid object/prefab icon instead of a human stick figure.
- The Outliner omits Sectors, their Surface children, Things, and baked Lights on hidden layers by default. Its compact
  persisted eye toggle reveals those rows without changing semantic indices or the document selection; hidden rows
  remain read-only. Explicitly locked Sector/Thing rows and Surface rows inherited from a locked owning Sector show a
  lock badge, while revealed hidden rows use a distinct hidden-eye badge. Filtering-aware Thing append keeps placement
  and brush strokes incremental instead of rebuilding the complete Outliner.
- Hidden and locked selections share one centralized layer-editability policy across properties, commands, UV tools,
  viewport transforms, and click-tools. The top-left edit-mode strip and applicable left tool strips remain visible in
  read-only state instead of jumping away. An amber lock badge identifies the reason and mutating controls are
  disabled. Entering this state cancels placement/click tools, switches Move/Rotate/Scale to Select, clears their
  gizmos, and forgets the previous transform. Restoring layer editability re-enables the controls but does not silently
  restore that transform.

### COG workflow and authoring

- Docked `COG Scripts` editor is now a serious built-in authoring workflow.
- New COG scripts begin as modified in-memory drafts with empty `symbols` and `code` sections. They are not written at
  creation (including in an untitled project), are included in COG recovery/SED editor state, and resolve to the
  project `cog` directory only on explicit Save. Existing open/resource names are rejected before creating a draft.
  The same safe draft workflow is available from **Edit > COG > New COG Script**; visible UI names consistently use
  the `COG`/`COGs` acronym while physical `.cog` extensions, resource folders, and internal IDs remain lowercase.
- The placed-COG schema is derived from public, non-message symbols. Unset `cog`/`sector`/`surface`/`thing` references
  use `-1`, scalar `int`/`flex`/`float` values use zero, and vector declarations have no initializer; the engine parser
  zeroes their storage. Their zero placed value is shown as `(0/0/0)`. COG runtime code uses single-quoted,
  space-separated vector literals such as `'1 2 3'`. Absent resource symbols use an empty authored value until a
  resource name is supplied. JKL/NDY writes
  use one engine token per placed argument: empty resource values become `-1`, placed vectors use one canonical
  `(x/y/z)` token, and unrepresentable values are rejected.
  Local symbols and messages never shift the placed-argument positions.
- A successful script save refreshes all matching placed instances in one Level transaction. Matching name and type
  preserves authored values at the same declaration occurrence, new declarations receive defaults, removed declarations
  disappear, and a valid empty schema clears old values. Missing or malformed scripts preserve the existing Level values
  and report the problem.
  Equivalent schemas are no-ops; Undo/Redo changes only the Level. Debounced file and directory watches capture the
  current document/resource identity, keep deleted scripts deleted, and apply a valid schema when a file reappears.
- Implemented features include:
  - syntax highlighting optimized for symbol-heavy cutscene scripts; selecting a complete identifier highlights all
    matching declarations/references while ignoring coincidental text in strings and comments
  - separate vertically resizable Symbols and Code outline lists; their split is optional recovery/SED editor state,
    and the shared Outline pane has a practical horizontal minimum so its entries cannot be crushed by a splitter drag
  - diagnostics with wave underlines
  - incremental in-editor Find with **Match case** and **Whole word**, disabled navigation for empty/no-match queries,
    and temporary-cursor misses that preserve the source selection, sharing the fixed bottom chrome line with zoom
    so toggling Find does not move or resize source text. The blue Find strip follows Find visibility while the zoom
    field remains visible on its own. The application's single `Ctrl+F`/`Ctrl+G` action pair
    follows strict focus: a focused COG tab searches source, while the viewport keeps level Find/Find Next
  - autocomplete
  - go-to-definition
  - formatter + whitespace toggle; the Format icon's main action applies current options and its dropdown edits them
    inline without a modal dialog. Formatter preferences cover keyword, condition/group-parenthesis,
    independently configured function-call-parenthesis, and `for`-semicolon spacing, final
    handler-return alignment, optional standalone-comment indentation (off by default), and optional format-on-save.
    Function invocation parentheses are canonicalized compact by default even when condition/group spacing is enabled.
    Formatting preserves string/comment text and authored comment-only indentation unless opted in, removes trailing
    line whitespace and whitespace-only blank-line indentation, and aligns handler code even outside explicit braces.
    COG names are normalized to a single `.cog` suffix. A tab's
    **Reveal in Explorer** action is offered only for an authored loose file; bundled/GOB, draft, and recovery
    buffers never expose their extracted or virtual paths.
  - theme and syntax-color preferences. Product-owned **SED Dark** is the default for new settings. It has a
    three-level matching-brace cycle and distinct
    local/non-local symbol, predefined-global, message-handler, function-call, custom-label, operator, and section colors
  - an editable, type-ahead installed-font picker defaults to `Consolas, Courier New, monospace`, supports ordered
    code-font fallbacks plus size/bold/italic preferences, keeps its open popup wheel-scrollable, previews across open
    tabs, and restores on Cancel only when the preview differs; per-tab 20%-400% display zoom is persisted in SED/recovery
    without modifying script source. Its compact right-aligned lower-left editor control does not add an outer status row and keeps
    a fixed left position and the application UI font at every source zoom, while Ctrl+wheel over source changes zoom
    in 10% steps
  - insert helpers for handlers/symbols/sections/snippets in the last icon-only dropdown on the main toolbar. They retain
    source context before menu focus, infer surrounding indentation, group paired inserts for undo, and live-reject
    invalid or case-insensitive duplicate custom labels
  - symbol-attribute actions (`mask`, `linkid`, `desc`, `local`, `nolink`) preserve exact declaration indentation,
    quoted contents, and trailing comment spacing
  - custom RMB menu
  - line-copy / line-paste conveniences
  - save prompting on app close for dirty open scripts
- The code editor inserts a newline with the current scope indentation. Code braces increase/decrease the next-line
  level, typing `}` on an indentation-only line aligns it to its matching opener, splitting immediately before an
  existing `}` realigns that brace, and pressing Enter anywhere on an over-indented, whitespace-prefixed `}` line
  realigns the existing brace before continuing. Symbol-scope indentation is preserved, braces in strings/comments are
  ignored, and Enter remains owned by the completion popup while completion choices are visible.
- Diagnostics accept the supported spaced `else if` form and an engine-valid `if` immediately following `}`, reject malformed hyphenated `else-if`, reject other tokens glued to braces, and reject statements that reach another
  statement, a closing brace, or the end of `code` without `;`. Explicit multiline expression/call continuations and
  control headers remain valid. A terminated `call` or `goto` may end a compact block as `call target;}` or
  `goto target;}`; arbitrary trailing text remains invalid. Redundant symbol-declaration semicolons and custom code labels without the matching
  int/flex/float declaration are advisory warnings; built-in message handlers and call/goto target validation remain
  separate. The diagnostics are lexical and preserve authored string/comment contents.
- Outline, diagnostic, and selected-reference regeneration is debounced after source/cursor edits and shares one text
  snapshot. Semantic-name highlighting uses a separate debounced cache; the code editor's `textChanged` callback reports
  its stable edited cursor block and schedules analysis only for declaration/label candidates, block-count changes, or
  external full-buffer replacements. Full rehighlight occurs only when declaration or label sets change, so ordinary
  typing and formatting do not repeat complete-document scans.
- Mask selection dialog and shared checkbox styling were modernized.
- Preferences now presents key bindings under friendly hierarchical groups such as **Viewport > Camera**. Rows keep
  their stable `keymap/shortcuts` IDs for compatibility, expose those IDs in descriptive hover help, and search both
  the friendly text and ID. **Viewport > Overlays** also exposes the persisted 5%-100% Extrude preview opacity (74%
  default).
- **Viewport > Grid** exposes the `G` visibility toggle and one Align Grid Plane to View action with separate Numpad 0
  and F3 alternatives. Every action uses one compact alternative editor with add/remove controls and overlapping-context
  conflict checks. Categories and rows are alphabetical by displayed name and ignore hover background highlighting.
  Existing viewport/Map Settings commands without a default start unbound. **Viewport > Mouse Controls**
  configures picking, Fly Look, Orbit, Pan, Dolly, and wheel behavior, plus independent horizontal/vertical camera
  inversion, while retaining the existing controls as defaults. Accepted mappings are cached; mouse and Fly ticks do
  not read settings. A captured camera gesture owns its initiating physical button until release or cancellation.
  Wheel scrolling settings are separate from physical button mappings, where Middle button means wheel click.
  **Fly Faster** and **Fly Slower** are in the Camera key-binding category; held Ctrl halves Fly speed by default and
  remains captured by Fly navigation. Separate **Zoom In/Out** and **Fly Speed Up/Down** Camera commands default to
  Numpad +/− in exclusive ordinary-viewport/Fly contexts; they remain usable when wheel scrolling is disabled or
  the wheel button owns Fly. Keyboard steps are independent of wheel modifiers and inversion.
- Viewport Banners preferences independently configure temporary message, Projection, Fly speed, Error, Cleave
  rotation, Cleave scale, and Placement rotation times and RGBA text/background/border colors. They are application
  settings, independent of themes and native workspace state. General/Error can keep their original message durations.
  Banner-only publication repaints the cached presentation without changing the Level or rebuilding scene resources.
  Separate Keymap links in Mouse Controls reveal and briefly highlight the Zoom or Fly-speed pair in Camera keybindings.
- Preferences disclosures remember their stable page/section expansion state only for the current application run;
  this transient layout choice is deliberately not written to `QSettings`.
- Preferences Accept/Cancel now avoid unchanged live publications. Untouched Accept returns before the complete
  settings/keymap write and live-application path, so it cannot invoke viewport setters or layout publication; an
  unchanged Thing cube size cannot rebuild the Level, layer/outliner and COG appearance refreshes are conditional, and
  unchanged install directories do not rediscover/remount archives. A changed active install is staged asynchronously and publishes atomically with
  delayed status feedback rather than holding the dialog open; changed inactive installs are discovered on their next
  project open instead of scanning archives from the modal Accept path.
- Layer visibility publication batches hidden-layer and Preview-policy changes into at most one stable-pick-ID mesh
  rebuild. Open/new/recovery stage this state without an intermediate rebuild and let the existing final Level/EDIT
  publication build once. Interactive rebuilds pulse across CPU construction, bounded graphics-buffer writes, and
  material batches; a status-bar indeterminate indicator appears only after three seconds. Opaque spatial-index and
  driver-allocation calls have explicit pre/post boundaries, while buffer writes yield at 8 MiB boundaries. One
  synchronous driver allocation cannot repaint internally. The pulse repaints only that widget, never calls
  `processEvents`, and never permits reentrant document mutation.

### Query / find workflow

- `Edit -> Find...` and `Find Next` are now a current-mode query workflow rather than a loose global text matcher.
- Query dialog:
  - follows current edit mode
  - hides irrelevant filters for the current entity type
  - no longer exposes the old entity dropdown
- `Light` mode intentionally disables `Find`.
- Material/resource browse helpers are integrated into query rows.
- One application-level `Ctrl+F` / `Ctrl+G` action pair routes by strict focus between COG source Find/Find Next and
  level Find/Find Next. `F2` and `F3` retain original JED's
  View to Grid / Grid to View pair; Numpad 0 also aligns Grid to View, while Insert alone places the grid behind
  geometry. G toggles the viewport grid outside active Cleave Multi-cut input. Keymap migration changes only exact old
  default aliases and preserves explicit custom or cleared bindings.

### Item editor / property parity

- Surface material fields show small material previews and open the MAT picker directly.
- Inherited Thing-parameter rows remain editable but use a readable muted foreground. Generic parameter commits are
  bound to the row's original text and authoritative parameter name, so a stale focus-loss during a queued Item Edit
  rebuild cannot write an adjacent field such as `maxheadvel` while committing `maxrotvel`.
- The default-off template-picker name replacement is an application-wide authoring choice under
  **Preferences > General > Things**, not a viewport-rendering setting. The Resources page's New Project Extra
  Buffers table expands to show every pool without an inner vertical scrollbar.
- Single-Thing Position and Rotation scrubs use a pick-ID shader transform preview instead of mutating the Level and
  walking every Thing/sector render cache on each pointer sample. The transform gizmo applies the same transient
  matrix, including local axes, so it follows the preview before release. The authored value and containing Sector
  publish once when the scrub finishes through a compact selected-Thing undo record, targeted renderer update, and
  Thing-only checker refresh. Typed/step-button commits use that same indexed publication path immediately.
  Typed Multi-Thing absolute-value commits retain their selection-wide semantics with the same compact subset record;
  continuous multi-Thing scrubs retain the general live-preview path because their per-owner matrices can differ.
- COG `material` vars use the same kind of preview/picker affordance.
- COG declarations and legacy JED COG fields spelled `sound` are normalized to the WAV COG type, so their browse
  button and value-field double-click open the WAV picker.
- `COG links` live in a separate item-editor section and navigate back into scripts.
  When no links exist, `NONE` is a disabled label rather than a link, so double-click cannot fall through to COG 0.
- Surface COG reference formatting/parsing and the underlying loader path were fixed so local surface numbering is
  stable and tested.
- JKL loader now sets `surface.num` correctly; regression tests cover multiple-surface sectors.
- Scalar Thing/template lookup matches left-to-right engine parsing: the last case-insensitive occurrence in one Thing
  or template record wins, with the nearest defining template retaining precedence over its bases. Repeatable
  `debris`/`frame` occurrences are enumerated separately instead of being collapsed by this rule.
- Thing Parameters shows the complete effective instance/template scalar surface. The first inherited-value edit
  materializes an override for each selected target; Restore removes only each target's override and reveals its own
  inherited value, while leaving local-only targets intact. Add Parameter likewise seeds each target's effective typed
  value before using a generic default. Dedicated `thingflags` follows the same per-target restore contract.
- Thing Parameters is a dedicated collapsible list between Item Edit and `COG links`, not a group inside the Item Edit
  tree. A vertical splitter lets users allocate its height independently; the persistent icon-led search is the first
  control in the section, and Thing parameter edits preserve that list's scroll position across rebuilds. Its resize
  boundary is a compact inset divider and its header is flat, avoiding the appearance of a second bordered list while
  retaining the same draggable splitter area. Expansion/collapse animates the splitter-owned outer clip and inner
  content extent together; the content is hidden only after the collapsed extent is reached, so its background cannot
  remain painted during the transition or flash away on completion. Mid-animation direction changes retain the live
  geometry and the prior expanded splitter allocation.
- Thing Parameters and Template Editor Parameters each have local search. Both parameter-name inputs use advisory,
  project-aware completion while still accepting unknown/mod-specific names. The parser-audited schema supplies
  type-aware resource pickers, quick `type`/`move`/`collide` choices, and searchable flag editors for Thing, physics,
  damage, and contextual Actor/Weapon/Item/Explosion/Particle flags. Flag descriptions no longer expose internal
  engine constant names. IJIM descriptions follow Jones3D Documentation `flags.md` at pinned main commit
  `94206bbe5efdaaa80290ecd2e5e4228a8b2b0ad0`, including IJIM's distinct Particle meanings for `0x2` and `0x4`,
  player electric-whip and fall-death Actor flags, and the correct "object cannot be crushed" direction for Thing
  `0x800`; JKDF2/MotS retain their existing project-specific definitions. Every unknown flag row is named simply
  **Unknown** because its bit is already shown in the Bit column. Physics `0x2000` is presented as the active
  **Flying** state rather than the ambiguous "can fly" capability: engine movement branches test `SITH_PF_FLY`, while
  gravity is independently applied only when `SITH_PF_USEGRAVITY` is also set. Flying transitions normally set the
  former and clear the latter.
- Template Editor Parameters also lists effective parent/base values. Editing an inherited scalar materializes a child
  override; Restore removes only that override. Inherited-only and append-only repeatable rows cannot be removed.
  `light`/`lightintensity` use the structured light editor and are identified as IJIM serialized RGB tuples or classic
  scalar intensities according to the project.
- Multi-Thing typed edits preflight every target and commit as one undoable mutation; one incompatible parse rolls the
  entire selection back. Contextual `typeflags` is unavailable across mixed families, while compatible flag changes
  preserve each target's unknown and runtime-managed bits. Sprite resource flags and a path move-info-flags parameter
  remain intentionally absent because the Thing parsers expose neither.
- Inherited repeatable rows are visible but read-only on placed Things. Local `debris` rows and Template Editor
  repeatables remain positional and appendable. Debris accepts four resolved template references in JKDF2/MotS and
  sixteen in IJIM; empty, `none`, and unresolved references are ignored without consuming a slot.
- Inherited Path parsing honors the first positive `numframes` allocation in base-to-derived token order; frames before
  allocation or beyond capacity are ignored and later counts cannot resize the buffer. Saves synchronize the local
  count and place `numframes` before every `frame`. AI waypoint frames remain instance-local rather than inherited.
- The former Thing Template Palette action is named **Show/Hide Thing Placement Tool...** and remains disabled outside
  Thing edit mode.

### Viewport / overlays / frame editing

- Click selection carries the mouse event's modifier snapshot through the viewport signal. Configured add/remove
  clicks are delivered even when the clicked entity is already active, so the default Ctrl-click removes that member
  from a multi-selection and promotes a remaining member; an ordinary click on the active entity remains unchanged.
- Surface Item Edit and Outliner focus follow the explicit validated GlView
  primary owner across Face/Edge/Vertex Shift-selection, removal promotion,
  marquee selection, and editor-state/history refresh. The semantic selection
  remains unordered; focused UI uses deterministic fallback only when no valid
  primary sidecar was restored.
- Repeated-click picking shares an 8-pixel/1.5-second cycle context across GPU and CPU fallback picking. Tool,
  edit/submode, selection-modifier, camera/scene, level, cursor-distance, timeout, and X-ray-option changes reset it.
- Thing frame submode now has a visible path overlay again:
  - line plus point markers
  - selected point highlighted red and larger
  - world-space sizing with preferences for line/point size
  - defaults target `0.0025` line width and `0.0050` point size
- Add Frame exposes the same Surface Stick/free placement, Align to Surface, Thing Snap, and Rotate to Snap controls
  as Thing placement/movement. Frame Move also uses these controls. Both operations resolve the owner Thing's visual
  model/sprite support at the frame pose, so grid and surface contact occurs at the rendered bottom instead of the
  serialized frame origin; move-and-rotate snaps are committed to the frame in one undo step.
- Frame Add resolves its new value index after `numframes` synchronization and selects it inside the undoable mutation.
  Properties Add/Duplicate/Delete remap frame selections by logical ordinal, retaining unaffected points when count
  fields shift value indices. Undo/Redo restores the path and selection together.
- Frame-path points, connecting segments, and per-frame 3DO wire models now share homogeneous-frustum clipping.
  Behind-camera negative-`w` primitives are rejected and crossing edges are clipped before perspective division,
  avoiding camera-stuck stretched lines and mirrored/upside-down path models when viewing from the opposite side.
  The clipping interval is evaluated in double precision across all six frustum planes and the positive-`w` guard;
  tolerance-aware boundary projection removes the float re-test that could intermittently reject or jump a segment
  as it crossed a viewport edge.
- Orthographic preview depth range was tightened for better z precision, then padded back out to avoid blank previews.
- Viewport grid rendering was tuned again:
  - adaptive spacing now uses one measured/projected/orthographic/final-fallback policy in the renderer, Move, and
    Cleave, including edge-on orthographic and camera-pole cases; transform drags no longer keep a separate
    initializer-derived lattice
  - the analytic shader honors Map Settings' `0.000001` minimum minor spacing, while the bounded CPU fallback retains
    64-bit major divisions and drops unresolved minor cells rather than truncating the visible major-grid extent
  - orthographic near-parallel views reduce dense bright line buildup
  - perspective grid fading/flicker near grazing angles was refined
  - local screen derivatives now retire under-sampled minor/major periods and retain exact 10x coarser major-grid
    subsets, avoiding distant Moire without changing the lattice used for snapping
  - the normal late/depth-occluded pass now explicitly uses `LEQUAL` and a tiny `2e-6` NDC coplanar lift, so a grid
    touching an authored surface no longer alternates with it as reconstructed plane depth rounds between frames;
    always-behind remains unbiased and visible-through-walls remains depth-test-free
  - colored axes are projected once as homogeneous screen-line equations and evaluated by exact framebuffer-pixel
    distance. This replaces the camera-relative plus large absolute-coordinate cancellation that could quantize a
    broad screen band to zero and paint it as an axis glow. Axis width is `1.0x` and capped at `4 px` independently of
    thick ordinary grid lines; colored axes replace RGB while retaining the maximum existing/axis coverage, so their
    antialias fringe cannot carve a dark moat into a wider ordinary line
  - the residual pale wedge was ordinary repeated-grid coverage coincident with the colored axis. Repeated coverage
    no longer divides by a potentially unbounded projective `fwidth`; it projects the nearest
    lattice line exactly and uses Euclidean physical-pixel distance. Derivatives are Euclidean and affect only
    frequency/LOD handoff. The box-filtered coverage preserves subpixel energy with a continuous bounded phase
    response (and exact sampled centroids at the tested integer widths), while the logical `0.5..10`
    ordinary / `0.5..4` axis widths are DPR-scaled in both analytic and fallback renderers. The fallback selects real
    MSAA when available and uses line smoothing only for a zero-sample framebuffer. Primary review additionally caught
    and repaired an axis-mask/source-over alpha trough: axes now replace RGB while preserving maximum ordinary/axis
    coverage, including the `10 px` ordinary / `4 px` axis case.
- Far-from-origin grid rendering was stabilized by keeping generated grid geometry camera-relative enough to avoid
  visible line jitter/flicker while panning/rotating.
- Surface normal markers use the canonical planar polygon area centroid shared with cleave centre markers and
  singleton face-mode transform pivots. They stay at the face centre in edge/vertex submodes; only the sub-element
  transform gizmo follows a hit edge/vertex. Multi-selection arrows stay attached to individual faces while the
  transform gizmo retains its aggregate pivot. Arrowhead wings patch a compact VBO range on camera changes so they
  continuously billboard without rebuilding the selection. Preferences provide live, clamped length (`0.1x..4.0x`),
  arrowhead scale (`0.1x..3.0x`), and arrowhead visibility controls.
- Edge mode additionally shows the selected edge normal at its midpoint, using
  the two owning face normals' bisector where the manifold is valid.
- Sector OBJ import recognizes deterministic contiguous closed shells even
  when a level exporter stores all sectors under one object; strict two-use
  edge validation remains unchanged. Conventional OBJ Y-up positions default
  to the handedness-preserving Sith Z-up rotation `(x, y, z) -> (x, -z, y)`;
  **Transform OBJ coordinate system to SED coordinate system** can disable it
  for native Z-up files, and the option never changes per-corner UV axes or
  order. External normalized OBJ UVs enter the normalized live Level directly;
  SED OBJ exports declare normalized UVs. An older pixel-marked SED OBJ is
  converted through each face MAT's real dimensions on re-import. Optional homogeneous position weights
  are resolved first, face winding remains stable, complete object/group and
  material names survive parsing, and normals are derived from final geometry
  instead of `vn`. Raw 3DO import stays in native Sith
  coordinates and independently applies each
  node's complete parent transform to its own mesh instance, including when
  several hierarchy nodes reference the same authored mesh. `Edit > Sector`
  orders Add, Delete, then
  Import/Export Sector(s). Export writes selected sectors atomically as OBJ or
  as one 3DO node/mesh per sector. Vertex Delete removes unused or redundant polygon
  vertices; confirmed Shift+Delete is the Vertex-only undoable topology-breaking recovery
  path. Face Delete removes selected polygons and can open a Sector; Edge Delete is a no-op.
- View-aligned grid axes restored from editor/recovery state are validated and orthonormalized before use; malformed
  axes safely fall back to the XY frame. Temporary grid centering/alignment is cleared for ordinary New/Open and
  malformed recovery fallback paths, while a valid SED editor-state restore preserves it.
- Map Settings contains adjacent, initially collapsed Grid Behavior, Grid Appearance, and Grid Transform disclosures.
  Appearance owns spacing, colors, thickness, and opacity. Transform owns paired XYZ origin fields, Pitch/Yaw/Roll
  editors, XY/XZ/YZ quick planes, grid commands, and the panel's single Reset to XY/origin zero. Command rows display
  their live Preferences key binding after the name and update immediately when it changes. The panel reserves the
  largest expanded width up front, so disclosures never resize it horizontally. These controls update the existing
  grid center/axis state and are consequently captured by SED/recovery. View-aligned camera poles use deterministic finite-axis fallbacks;
  orthographic unit-per-pixel and minimum shader footprint fallbacks prevent rotate-to-view disappearance during
  camera rotation or close perspective zoom. The opt-in, default-off **Keep rotated grid visible** behavior adds a
  phase-matched visual proxy just beyond the near plane only when the fixed view-aligned plane crosses the camera.
  It never changes the authored grid origin or transform-snapping plane; disabling it preserves the exact prior
  Rotate Grid to View behavior. **Reset grid on camera rotate** captures the camera basis at every Map Settings
  origin/orientation edit, so the very first subsequent rotation clears that temporary override rather than merely
  arming the reset for a later input sample.
- Contextual top-center tool rows retain the viewport's true horizontal center at the top. They move down just below
  the lower edge of an actually intersecting horizontal edit-mode/right viewport bar; tall vertical transform/tool
  palettes never contribute their bottom edge. Multiple visible rows stack beneath that first row without being
  pushed toward the middle of the viewport.
- Grid visibility now has a depth-occluded default: the late grid pass respects completed scene depth, so solid level
  geometry and shaded Things hide grid fragments behind them. **Grid visible through walls** switches that late pass
  to a true overlay, while **Grid always behind geometry** draws the grid first. The two overrides are mutually
  exclusive; turning both off restores the default. Map Settings gives each option a distinct code-drawn icon and
  paints checked rows as active controls instead of relying on checkbox marks.
- Transform-grid options are persisted independently and exposed as direct, checked icon-only buttons in the top-center
  Thing/Frame/Sector/Surface/Light **Move** options bars:
  - **Snap moves to grid** defaults off and snaps the active target reference on the exact displayed minor-grid
    lattice while preserving plane-normal depth for free moves; its hold inverter defaults to `Ctrl`
  - the Surface Extrude options row reuses that shared Grid Snap state: enabled Length/Expand values quantize to the
    effective displayed minor spacing (with negative Expand kept above collapse), while disabled values remain precise
  - while grid snap is effective, the lattice result takes precedence over nearby-geometry and thing-to-thing snapping
  - **Grid stick** defaults on, can be disabled, and briefly holds a Thing/frame/light reference point or the first
    face/edge/vertex/whole-sector geometry support at plane contact before continued motion releases it through the
    plane; sectors already spanning the plane use their next support on the current approach side
- Contextual boolean tool options consistently use visibly active toggle buttons. Grid Stick, sector **Snap align**,
  Thing **Surface stick**, Thing **Align to surface**, and Thing-to-Thing **Snap**/**Rotate** each have a distinct,
  rebindable exact-match held inverter. `Ctrl` is shared by Grid Snap, sector **Snap align**, and Thing-to-Thing
  **Snap**; other defaults are Grid Stick `Shift+Alt`, Thing **Surface stick** `Shift`, Thing **Align to surface**
  `Ctrl+Shift`, and Thing **Rotate to snap** `Ctrl+Alt`. Held state is reflected on the affected icon and is never
  persisted. Frame Add/Move shares the four Thing visual-contact/alignment choices; Frame Move also retains the common
  grid buttons.
- Rotate-with-View plus **Keep rotated grid visible** measures adaptive spacing on the same camera-side proxy for the
  renderer, Move, Cleave, and Surface Extrude; only this scale measurement is proxied, so the authored snap origin and
  plane remain unchanged.
- Map Settings and Preferences expose persisted grid opacity, separate minor/major colors, and `0.5`-`10.0 px`
  thickness controls. The blue-gray minor and brighter major defaults avoid conflating the grid with geometry
  wireframes; 100% opacity gives the line centre full coverage while the shader antialiases only its outer pixel.
  The two axes retain their directional colors and stay capped at `4 px`, preventing a maximum-width grid from
  turning them into broad red/green bands.
- Game Preview hides Grid and grid-only Map controls but retains **Baked lights**, **Thing lights**, **Sector lights**,
  and **Show thrust vectors**. Preview persists an independent set of values and defaults all four off; switching back
  restores the Editor values instead of overwriting them.
- Move-option glyphs are larger and keep their full descriptions in hover tooltips, so long option names no longer
  grow the center overlay into the right-side viewport controls. Lighting modes use sun/lamp/rays/vertex-face glyphs,
  while Thing Dots/Boxes/Wireframes/Shaded modes use distinct point, bounds, wire-cube, and solid-cube silhouettes.
- View now groups Render Mode, Lighting, and Things into active-state submenus. The Thing launcher itself is never
  highlighted as a toggle; its crossed glyph indicates all Things are hidden, while Hidden Things, Gizmos,
  Non-visual Cubes, and mode rows visibly highlight their current state. Hidden Things includes effective inherited
  `INVISIBLE`/`DISABLED` flags, filters those owners from runtime Thing-light collection when hidden, defaults on in
  Editor and off in Game Preview, and is remembered per mode. The former
  Level Header control is presented as Level Settings with a
  level-document-and-gear glyph because it edits level-wide gravity, sky, and fog properties.
- Transform grid math freezes the rendered grid frame at drag start, including view-aligned/top-down orientation and
  adaptive spacing. The stick release uses hysteresis plus a continuity offset and remains latched for the drag, so
  pushing through cannot immediately recapture the geometry.
- Multi-vertex surface moves now commit the same deduplicated selected vertex set shown by the gizmo preview.
- IJIM Sector mode displays sector thrust vectors as distinct center arrows tied to their owning Sectors and
  exposes exact vector/flag editing in Sector properties. The primary sector's edited vector is copied exactly to the
  current multi-selection; XY/XZ/YZ wheel planes rotate continuously without a mouse-down jump. Center crossings
  re-anchor coalesced pointer samples rather than injecting an ambiguous half turn, and the wheel no longer paints a
  redundant plane/normal label. A plane whose normal
  is parallel to the current nonzero direction is explicitly rejected until the user selects a stable alternate plane,
  while a zero vector can still initialize in any plane. Icon-only Remove/Copy/Paste actions live in the Thrust group
  header and preserve exact vectors; zero-vector edits and Remove clear the `UseThrust` flag for every selected sector.
  X/Y/Z and magnitude scrubs update the full selected set and viewport live in one undo gesture. With the
  viewport **Sector thrust vectors** setting enabled, every visible, unlocked Sector with a finite, nonzero thrust
  vector displays a passive arrow independent of selection. Disabling it hides passive arrows; the direct tool ignores
  this preference and continues to show authored vectors and zero handles according to **Show all**. The bounded
  logarithmic arrow scale gives common `0.5`/`1.0` vectors more
  direction-reading length without exceeding twice the old cap. A persisted top-center **Show all** toggle can hide
  completely unset thrust handles from drawing and picking.
- Sector Sound, Thrust, and IJIM Point Light properties are optional blocks: absent data collapses the detailed rows
  to a single Add action; present blocks put icon-only Remove/Copy/Paste actions in the field header. Point-light Min
  and Max ranges are vertically stacked with an unframed connected/broken-chain control, linked by default and updated
  live across the selected Sector set. Removing data collapses the block immediately; clipboard payload failures use
  the fixed-screen action feedback described above.
- Sector and Surface cleave expose a **Multi-cut** top overlay for persistent editable cuts. Independent angle/length
  guides, formatting units, and editable execution ranks remain separate. Shared Grid Snap applies to anchors, drawing,
  endpoint edits, group Move, and shape placement; Ctrl reverses it and Shift prefers geometry/staged endpoints.
  Ctrl+Shift requests Angle for edited endpoints and the original plane-normal offset for Move. Plain endpoint edits
  use Angle step when grid snapping is off. The active transformed/adaptive grid frame stays authoritative.
  Sector lines retain world-space planes. Bounded cuts preserve finite authored endpoints; unbounded drawing/editing
  uses authored-camera edge handles for independent planes. Toggling Bounded alone preserves existing geometry, and
  shared endpoints keep their topology positions.
  Surface Face cuts retain the original face and target set. Pointer previews update sparse line/neighbor worksets;
  Apply validates the complete batch as one document transaction. Surface Edge remains the immediate single-line path.
  Independent Sector batches skip planes that split no admitted descendant, including misses under Bounded reach;
  at least one plane must split for the batch to succeed. A closed Sector chain may retain a non-empty interior through
  a non-splitting plane, but an empty interior or invalid cut still rejects the whole batch atomically.
  Open Surface arrangements skip repeated supporting planes using a strict compensated-zero-offset proof. Native
  finite strokes also admit a straight continuation sharing an exact endpoint with an earlier successful cut when
  represented-coordinate bounds explain its construction rounding. This separate check has no absolute world/angle
  tolerance; distinct misses and actual core crossing failures still roll back the entire batch in both cut modes.
- Physical line indices own stable topology, accent colors, origin references, and selection. A separate permutation
  owns numbered execution ranks. Click a visible number to type a rank; Enter/focus loss clamps to `1..N`, remaps the
  remaining ranks, and records one local Undo without moving geometry or joins. Escape cancels the field. Apply follows
  that order; a canonical closed interior cut still requires one continuous authored Chain.
- Plain click replaces selection, Shift-click adds, and Ctrl-click removes. An already-selected line keeps the group
  during dragging; a no-drag release narrows it. Alt-click cycles shared-joint incident lines; a real Alt-drag detaches.
  Empty-space Alt marquee adds intersecting lines, Alt+Ctrl removes them, and Delete removes a batch. Cleave-local Move
  is default. Common-plane Rotate/Scale use distinct-endpoint centroid pivot, signed angle/Angle step, and positive
  uniform scale. Shared joints carry neighboring endpoints once and pin their opposite ends; Escape restores exact
  pre-drag lines/selection. Bare G/R/S select the local modes only while active Multi-cut owns viewport input;
  global transform bindings retain their normal contexts. Ctrl-only wheel rotates by Angle step (Free uses 5 degrees)
  or scales selected lines uniformly. A held-Control gesture accumulates its angle/factor, reuses one prepared workset,
  and finishes one local Undo step on Control release or ownership transfer. Escape restores the exact baseline;
  native Save/autosave/recovery during a gesture capture the settled pre-gesture draft and completed timeline.
  HUD presentation hides after its independently configured Rotate/Scale idle timeout (0.4 seconds by default), even while Ctrl or a pointer
  button remains held. Expiry preserves the gesture baseline, cumulative values, prepared workset, and history;
  further input shows the cumulative result again. Control release hides it immediately.
  Invalid Rotate/Scale selection eligibility returns the active mode to Move.
- Add Shape offers centered Rectangle, N-Gon, Filled arch, and Star plans as ordinary bounded strokes.
  Fresh dimensions display 1 in named units or `0.1` Bare; physical dimensions are `0.005..10000` world units and
  applicable counts are `3..64`. Legacy Square stays readable and normalizes to linked Rectangle; N-gon always links dimensions;
  N-gon width is circumcircle diameter. Arch has straight jambs and a half-ellipse cap. Star's division lines precede
  its perimeter and divide the concave silhouette into convex cells, potentially with extra fragments at higher counts.
  Surface Extrude accepts one selected face at a time, so select/extrude each Star piece separately. Escape leaves shape
  placement while retaining its authored draft, except during a live shape wheel gesture, where it first restores
  the option baseline and keeps placement armed. Shape wheel changes save current future-placement options without
  a line-history command. Project units are Meters/IJU/Bare for IJIM and Meters/JKU/Bare for
  JKDF2/MOTS; unit-only changes preserve authored physical size. Four shape spin controls accept hovered wheel edits,
  and Ctrl-only viewport wheel scales an armed shape uniformly. Sector-only Camera plane defaults off, with the surface
  frame as the default; enabling it uses a free camera-facing plane through the orbit target. Surface ignores/hides
  that preference. Its optional strict Boolean is additive in native current/Apply shape maps; absence means false.
- Multi-cut has bounded line-only Undo/Redo. Each completed changed operation, including group transforms, rank changes,
  Clear, and Chain-origin choices, records a step; camera/options/selection do not. With matching enabled Cleave ownership
  in Editor and Multi-cut on, viewport/Level commands stay local at timeline ends, after resource/COG and native text
  priorities. New edits discard Redo; failed Apply retains state. Tool/edit-family changes, Multi-cut off, and Preview
  entry retain settled lines/history/order/line selection/options, settle live wheel transforms, and roll back
  unfinished pointer drags. Dormant work
  releases rendering/input/Undo. Compatible same-Level coordinate/light/UV/direct-append changes retain world planes.
  Direct Sector/Surface Delete/Join/Merge invalidates pending cuts; History Structure+LevelGeometry swaps do so
  conservatively without a typed identity/remap contract, then checked Apply memento restoration takes precedence.
  Invalid source references also clear them with an explanation. Explicit cancellation, New/Open, and successful Apply clear
  the draft. Temporary Outliner mismatch preserves it.
  Native Save, autosave, and recovery retain current lines/order/selection/options and the local timeline/cursor.
  Legacy current-only records start a fresh baseline when restoration admission permits. Authored pending cuts count
  as unsaved native work; a successful Save checkpoints the exact captured draft without requiring Apply. Camera,
  selection, guides, future shape/angle
  choices, and an unchanged authored draft at another history cursor alone remain editor-only.
- Apply is one Level step with an app workspace memento for the applied Sector or Surface family. Undo restores its
  exact pre-Apply draft/history/options, line selection, and document selection/active items/mode after mesh notifications
  and tool sync, preserving a compatible peer's current workspace and later edits. Matching Level Redo wins over retained local
  Redo only while the applied family owns input and its Apply-affecting draft is unchanged. An active peer keeps its
  available local Redo; without one, ordinary document Redo of a known Apply clears only the applied family. Explicit
  History navigation also clears only that family. Applied-family Apply-affecting edits, including Bounded changes,
  or local Undo/Redo revoke paired priority; peer edits, family changes, selection/camera, guides, and future shape/angle
  choices do not. Native EDIT attaches checked Apply/Layer
  workspace histories to exact adjacent HIST states by stack positions
  and required-Level hashes; import remaps them to fresh runtime IDs and branch/trim removes unreachable attachments.
  Missing or degraded HIST, or an Apply attachment containing a peer workspace, drops optional attachments without
  dropping current EDIT work. Authored draft/history
  admission uses a 64-snapshot/16 MiB budget for estimated owned data; workspace attachments have a separate
  64-transition/16 MiB aggregate cap. Invalid local timelines fail before publication, and admission charges
  live/retained/gesture/edit/restore worksets before allocation. Shape-planning and rendering caches are separate.
- Sector cleave preserves semantic identity on the body containing the pre-cut Sector center: that body keeps the old
  number/ID and COG links, while adjoins/Things follow exchanged geometry and the appended cut-off body is selected.
- The first Multi-cut press captures its construction context and immutable target set. Each Sector line immediately
  becomes an authoritative world-space plane with its own construction frame, so orbiting, zooming, resizing, or
  changing projection neither moves the plane nor changes the result submitted by Apply. Surface Face likewise permits
  camera movement while freezing its source face and target set. Compatible geometry notifications retain this
  authoritative data, while ordinary pipeline/tool changes suspend it. Closed loops
  also reject duplicate/degenerate vertices, self-intersection, non-convexity, and inconsistent planes, and must be
  applied separately from independent lines.
- A completed line retains its authored camera. Clicking its body without a drag restores exact orbit, projection,
  horizontal FOV, orthographic scale, near/far planes, and clip-enabled flag. That action does not depend on intersecting
  the construction plane, so an edge-on line remains clickable; movement within the two-pixel click tolerance restores
  the line's press baseline rather than committing a tiny plane move.
- Level load/new-project framing now resets to top view and fits the whole level with extra margin so long/slender
  levels are not clipped against the viewport edge.
- Fly navigation shows an eye cursor/icon instead of the normal crosshair.
- Light recalculation now refreshes thing shading too, not just sector/surface data.
- Light edit mode has a compact top-center bake overlay for immediate recalculation, visible unlocked sectors only,
  opt-in sector-ambient derivation, and persisted automatic recalculation. Auto requests throttle/coalesce light changes, run against a copied level on
  the Qt thread pool, and discard stale generations instead of overwriting newer edits. Contextual Light tool options
  stack beneath the bake overlay. A clickable status-bar item opens percentage progress with a cooperative Stop action.
- Removing or disabling the last usable light clears stale baked vertex RGB. Sector Ambient is preserved by default;
  **Ambient** enables Pascal-compatible derivation from baked texture-vertex colors and Surface Extra Light, while
  a small action beside the Sector Ambient field applies the same derivation manually. Light drag completion retains
  the final live-bake preview until its authoritative background pass arrives, so release no longer flashes stale light.
- Live light-transform previews calculate off the GUI thread and coalesce superseded drag states. Automatic passes retain
  the union of edited lights' old/new ranges across cancellation and preview commit, then recompute only texture vertices
  inside that dirty region while including every unchanged light that still contributes there. The core pass computes each
  target bound once, culls range-disjoint light/sector pairs, and avoids the legacy all-sector mark reset for each portal ray.
- Auto observes only Baked `SedLight` records. Geometry/topology changes reject an in-flight result through its source
  fingerprint but do not start a bake; regular Thing lights, actor/player Headlights, and IJIM Sector point lights are a
  separate frame-local runtime preview and never enter baked texture-vertex colors.
- Light mode has distinct Baked, Thing, and IJIM-only Sector owner families. Runtime sources have source-correct
  add/move/scale behavior, range gizmos, owner-constrained picking, and live transform preview. The viewport resolves
  inherited fields and evaluates a view-stable scene-wide eligible set through a 2048-byte std140 block. The 64-light
  IJIM or 32-light classic cap is applied only on overflow by nearest-camera membership, after which deterministic
  Thing traversal order is restored for evaluation. The formulas, IJIM flat self-light, Headlight offsets, Sector zero-range derivation, Sector
  Gouraud/Diffuse enabled quirk, collection limits, and parity boundaries are documented in `docs/lighting.md`.
- Actor/player Headlight visualization now uses the exact resolved runtime emitter position: Thing world position plus
  the authored local `lightoffset` rotated by Thing pitch/yaw/roll. Unlike regular Thing-light icons, Headlight icons
  are never lifted above the owner model/sprite bounds. Drawing, direct and marquee picking, range rings, previews,
  and the offset-edit transform pivot therefore agree for IJIM, JKDF2, and MotS.
- Runtime dynamic-light sampling defaults to **Gouraud (per-vertex)**, matching the original vertex evaluation and
  clamp stages. Optional **Phong (per-pixel diffuse)** affects only effective Gouraud targets, using interpolated model
  normals and the pinned OpenJones3D OpenGL smooth falloff; authored Diffuse faces retain their original flat
  face-anchor path. Neither choice changes baked lighting or level serialization.
- JK/MotS Surface **Extra** accepts signed `[-1, 1]` values. Negative values subtract from Gouraud vertex light and
  round-trip through JKL/JED and reusable sector shapes. IJIM Surface **Color** exposes opacity only for Solid or
  Textured adjoins and preserves alpha on ineligible surfaces during multi-edit.

### Docked UV editing

- Raw stored texture-space coordinates remain the docked UV editor's default. Optional **View Face** derives one rigid
  2D rotation and any required reflection from the active selected face's current viewport projection. Texture, grid,
  every selected island, hit testing, and edit directions move together. Multi-surface selections do not receive
  per-island transforms; the active face controls the shared view and stored UVs remain unchanged.
- A behind-camera, near-edge-on, or degenerate projected active face keeps the previous whole-view orientation instead
  of flickering. The mode deliberately does not perspective-foreshorten individual UV geometry.
- Focused coverage exercises opposing-winding multi-surface selections, a deterministic 90-degree orientation,
  reflected/rotated coordinate round trips, prior display mirror/offset removal, and previous-orientation retention
  for missing or unstable active-face projections. A real MainWindow fixture verifies a visible canvas change.
- Modal `G` / `S` / `R` edits:
  - guard no-op commits
  - cancel cleanly on panel hide/focus changes
  - block wheel zoom during active transforms
  - clean up lingering selection rectangles and pointer state more aggressively
  - patch only affected cached texture-coordinate/VBO ranges during pointer
    preview, then publish one undo entry on a changed commit
- Multiselect component masks, active island selection, and forced rebuilds after real UV mutations were tightened.
- Repeated clicks at one UV canvas position now cycle deterministic overlapping Face/Edge/Vertex candidates. Face
  wraps; Edge and Vertex add a terminal no-primary step without clearing the selection. Exact active vertices and
  edges are drawn last in red so overlaps cannot hide them, and their point/edge midpoint owns the transform pivot;
  otherwise the selected-point center remains the fallback. Face mode can set the same red pivot by clicking a face
  vertex and reset it at the centroid without expanding or discarding the cached component selection. Vertex/Edge
  clicks on the aggregate center ring clear only the exact pivot/cycle state and preserve their selected masks.
- UV snapping now treats exact geometry coincidence as acquired, overlays only orientation-compatible edges (including
  reversed endpoint order), acquires vertices from wholly unselected islands, and can align the center of a completely
  moving island with the center of another unselected island. Any partially or wholly moving island is excluded from
  the target pool, preventing owner/self snaps. When no nearby geometry qualifies, an unconditional nearest visible
  grid delta constrains the result instead of accepting a free UV position. **Grid size** is always the main spacing; the
  default-off **Sub-grid** draws and enables quarter-cell snapping, with geometry still taking precedence. **Minor**
  offers Lines (default) or Points with a 0.5–10 logical-pixel diameter (default 2). Point density and emission budgets
  bound drawing without changing snap spacing. Appearance is saved in QSettings and optional named
  `window.uvGridAppearance` in EDIT, autosave, and recovery; older payloads without the map restore Lines/2-pixel dots. Main/sub
  colors are theme preferences, and Sub-grid state joins recovery while old payloads default it off.
- Repeated texture images remain controlled by **Repeat**. A distinct default-off **Tile grid** gates only the white
  repeated one-UV separator pass, and **1 UV** sets the main Grid spacing to `1.0`. Tile-grid state is recovered, while
  older recovery payloads omit it safely and restore borders off. The compact Grid size field right-aligns its value with
  Sub-grid beside it; the Transform row ends with a compact arbitrary `0.01`-to-`1000` **Scale ×** multiplier and a
  wide-enough explicit Apply button.
- Smart UV Project now follows Blender's projection-normal clustering, dominant-axis projection, area weighting,
  rotation, margin, and coherent active-texture aspect behavior. Its extracted dialog exposes Blender-aligned controls;
  SED intentionally defaults rotation to its architecture-safe world-vertical extension. Final island placement is a
  deterministic shelf-pack approximation rather than Blender's internal
  polygon packer, and invalid input still fails atomically.
- Smart UV's Blender-compatible Horizontal/Vertical choices orient the fitted island bounds, not world geometry.
  **Geometry (World Vertical, Recommended)** is the architecture-oriented extension. It derives positive UV V from
  actual upward component edges after aspect correction, then falls back to projected positive world Z and finally
  minimum-box rotation when that direction is undefined on horizontal faces. Projection normals come from current
  polygon vertices, and texture-aspect correction is applied to world up. When current UVs provide a coherent vertical
  direction, their positive/negative V sign is retained so reprojection cannot turn the texture upside-down. This
  eliminates the stale-normal dominant-axis quarter-turn seen on slightly backward-banked walls. The dialog's
  **Island Scale** is initialized from the median corresponding-edge ratio between the current selection and a
  unit-scale dry run; it then uniformly scales each final connected island and repacks expanded islands without
  overlap. **Follow Active Quads** is now a separate tool rather than an alias for Stitch Chain. It preserves the
  selected active quad, or falls back from a non-quad panel anchor to the first selected quad that has a compatible
  selected neighbor. It traverses deterministic selected quad loops across tolerance-matched unambiguous selected
  edges and offers Even, Length, and
  Length Average spacing. Extra unselected owners do not make a selected pair ambiguous. It fails atomically with
  specific feedback and commits one undo step.
- Surface material property thumbnails use ordinary Qt top-left image orientation. Their shared MAT cache remains
  GL-oriented for renderer/3DO consumers; the property icon alone reverses that renderer-only vertical mirror.
- Auto Align and Stitch retain the original SED/JED UV-basis workflow. Auto Align now follows Pascal `RecalcAll` by
  deriving its replacement frame from the authored UVs after refreshing the face normal, rather than rotating every
  face onto local edge zero. Differently ordered adjacent faces keep their orientation and seam coordinates, and
  malformed UV arrays fail atomically. Stitch validates complete source/target UV geometry and publishes a temporary result only after successful computation; focused tests cover shared noncoplanar
  edges, disconnected fallback anchoring, scale/flip, and malformed/mismatched data. Stitch Ref applies the active
  reference to each target, while Stitch Chain propagates only through selected adjoin/shared-edge connectivity.
- UV panel buttons, preview/grid controls, header actions, dock choices, and Surface-menu UV actions now have concise
  tooltips. Buttons backed by a shared `QAction` mirror its tooltip when the action changes. Pack explicitly describes
  its non-overlapping layout mutation without shape/rotation/scale changes; Fit explicitly describes view-only framing
  and does not imply a stored-UV edit. An inventory-style MainWindow regression rejects any future tooltip-less UV
  button or menu command.
- Opening/hiding the docked UV editor uses one short S-curve transition. First reveal constrains the host and starts at
  a controlled one-pixel splitter extent; redundant presentation requests do not restart the animation, and expanded
  minimum sizing waits until the transition owns the panel extent. Viewport compensation preserves projected object
  scale and applies only the crop/reveal caused by the occupied extent, retaining Surface-mode navigation. Hide rebases
  from the live viewport after ordinary window/splitter resizing, and winding refresh pauses during the transition.
- The UV **Grid size** numeric editor prefers a shorter 26-pixel control, but its reusable compact-spin sizing derives
  the final height and editable width from the active font and style. Three-decimal values such as `16.000` retain
  cursor/fraction headroom, and larger UI fonts expand the field instead of clipping it.

### Consistency checker and parity work

- The active rule table now covers the complete Pascal `Cons_checker.Check` and `CheckResources` surface:
  - sector colormap, minimum topology, and non-3DO convexity
  - surface flag/geometry mismatches, project vertex limits, minimum size, corrected unit-normal tolerance, planarity,
    convexity, and IJIM Track limit
  - adjoin reverse link, geometric overlap, and translucent/geometry-zero visibility
  - Thing sector assignment/containment and typed COG `.ai` / `.key` / `.mat` / `.wav` / `.3do` parameters
  - MAT, CMP, Sector WAV, placed template/COG availability and MotS/JKDF2 CMP-count limits
- The normal-length check now matches Pascal `IsClose`: absolute error below `1e-4` passes and error at or above `1e-4`
  warns.
- Legacy triage severities are preserved: missing/mismatched reverse adjoins and missing legacy-referenced
  MAT/CMP/WAV/template/placed COG resources are Errors, while surface planarity and convexity findings are Info. The
  additional inherited SPR/3DO availability checks remain Warnings.
- C++ safety/editor diagnostics additionally cover malformed vertex/adjoin references, vertex/texture-vertex count
  mismatch, invalid/non-finite Thing placement, missing/inherited template SPR/3DO resources, and underscore-prefixed
  base-template placement.
- The slower pairwise **Sector overlap** rule is exposed in the searchable Consistency settings dialog and defaults
  off, matching Pascal `CheckOverlaps`. Enabled evaluation skips directly adjoined sectors and reports both sides of
  each detected pair; the Checks header has no duplicate toggle.
- Checker refresh is debounced for geometry, Things, lights, COGs, and structure;
  `checksDirtyIsRelevant(...)` in `check_rebuild_utils.hpp` owns the shared mask and explicitly includes the COG dirty
  bit, so typed parameter edits refresh without an unrelated geometry change. Undo/redo also schedules relevant
  refreshes.
- Focused direct tests cover sector early-outs, a valid-sector underscore-base-template fixture, both sides of the
  `1e-4` normal boundary, matching/mismatching adjoins, overlap default/opt-in behavior, COG values, missing resources,
  and CMP limits. These checks are part of the registered Release regression suite.
- Surface issue double-click was fixed so it selects the correct surface instead of always `0`.

### App icon and branding

- Startup branding now uses `SED - Sith Editor` wording.
- The active native shell/taskbar icon is maintained in `src/app/resources/app_flat_red_circle.svg`; CMake embeds its
  multi-size `app_flat_red_circle.ico` in the Windows executable.
- The in-app window icon is maintained in `src/app/resources/app_window.svg`. Qt embeds its checked-in multi-size
  `app_window_*.png` files, plus the shell PNG sizes, through `app_icons.qrc` without an optional ICO decoder.
- Superseded icon experiments are kept outside the maintained source tree and release resource set.

## What remains / known gaps

- Level-wide ID exhaustion is checked before complete edits are published. `INT32_MAX` is the exhausted next-ID
  sentinel and `INT32_MAX - 1` is the final assignable ID. Factory, clipboard, placement, COG, and light paths
  preflight known counts. Cleave/Join allocate checked IDs incrementally inside a private candidate, admit exact
  capacity, and discard that candidate if a later split exhausts the remaining IDs.
- Current Thing placement preserves newly allocated IDs/numbers. Previously saved native files with duplicate Thing
  identities are not migrated automatically. A repair policy must validate placed COG Thing targets and history
  bindings; blanket renumbering can retarget COG references after a number-based roundtrip.
- Native persistence carries meaningful pending JKL COG import provenance in optional core `jimp` V1 and in sparse
  HIST `jklCogImportV1` metadata for the current and affected undo/redo states. Unused no-COG maps normalize away, while
  malformed known metadata is rejected and permissive `jimp` loss preserves the verified Level. See the
  [native format](formats/sed_binary_format.md#overview).
- SPR/SND/UNI presentation can be retained by optional EDIT `editorPresentation` V1, including selection, scroll,
  caret/anchor, and explicitly manual Sound Class widths. Deferred cell-editor carets apply through ordinary editing;
  a live delegate is never reopened. Explicit UV pivots and the modal Template Editor's unaccepted working draft remain
  transient; last-used template geometry is saved. Raw drafts and local history remain separate; see the
  [editor-state inventory](editor_state.md).

- A fresh live-Pascal audit, excluding plugins and replaced/dead paths, confirms remaining save conveniences,
  Paste-in-Place/Insert duplication, advanced 3DO hierarchy authoring, project archive/test workflows, and
  document project-type conversion. Classic Level Header controls and LEV import choices are implemented.
- JKDF2/MotS Episode editing is now available from Edit (`F8`) for sequence graphs, searchable COG strings, goals, and
  JKDF2-only startup-COG generation/placement. The action is hidden for IJIM; MotS generation is intentionally gated because the
  legacy JK inventory-bin generator is not MotS-correct. Root, `misc`, and legacy `ui` string files are unioned with
  explicit conflict precedence and migrated only after atomic canonical save. One-byte text, sparse 50-slot goals,
  UNI metadata, runtime bounds, and per-key Goals/Strings last-writer behavior are preserved.
- Startup/new-project data and camera math now have focused coverage; dock/window timing, axis-gizmo presentation, and
  the resource-derived player offset should still be spot-checked interactively.
- Translucent level batches, per-Thing triangles, and sprite runs now share one stable far-to-near queue, and the
  near-opaque depth prepass respects final level/Thing material-modulated alpha.
  Opaque parts of blended solid walls now occlude translucent Things behind
  them without filling transparent texture regions. This fixes global source-category ordering, while
  grouped primitive depths and depth cutoffs remain heuristics. Intersecting or cyclic alpha geometry remains the
  explicit no-OIT case that cannot always be solved by a total sorted order.
- `LEV` save and `ASC` save are still missing.
- Raw texture space remains the UV default; optional **View Face** provides one coherent whole-canvas rigid 2D
  rotation/reflection, while intentionally avoiding perspective foreshortening.
- The type-0 Game Preview facing defect is fixed: nonzero authored Thing PYR cannot turn the original view-space
  billboard edge-on. The type-2 cross-product basis and roll path are source-audited against
  [OpenJones3D `develop` commit `b9c0eaa`](https://github.com/smlu/OpenJones3D/blob/b9c0eaa56bb51307b0787add4b614d9468e5d372/Libs/rdroid/Primitives/rdSprite.c#L158-L321),
  and sprite preview icons invalidate on both SPR and CMP revisions. The shaded path shares a directly tested type-2
  frame helper covering the ordinary basis, both sides of the near-pole threshold, both vertical-axis fallbacks, and
  malformed input. Texture vertical
  orientation, representative real near-pole assets, and accepted-but-undrawn SPR type `1` remain manual/source
  boundaries.
- Thing Placement hides by reversing the full panel's show geometry and reveals the collapsed bar only after the
  transition finishes.
- Viewport functions that own pointer interaction are mutually exclusive across transform tools, click tools,
  placement modes, Sector Join, and Surface Extrude.
- Multi-cut Alt endpoint dragging disjoints the picked explicit chain connection and moves that endpoint
  independently; ordinary dragging preserves connections and coordinate-shared endpoint handles. Surface
  Multi-cut validates only geometry and adjoins changed by the batch rather than rejecting a valid split because of
  unrelated pre-existing sector geometry.
- Viewport Map Settings and Thing Rendering panels accept repeated interaction inside the panel and dismiss only on an
  outside click or their toolbar toggle.
- Layer-lifecycle tests restore their settings and cannot leave synthetic test colors in the deployed `sed.ini`.
  Unavailable layer colors resolve to opaque white while valid explicit colors remain untouched.
- IJIM does not offer JED export; JKDF2 and MotS retain it.
- The primary Windows build uses `build/ninja` with Ninja Multi-Config. Do not run concurrent build writers against
  that tree; use an independent build directory for genuinely parallel work. If Ninja reports a damaged dependency
  database after interruption, stop all writers before using Ninja's normal recovery or rebuilding the generated tree.
- Viewport entity click selection is owned by any visible Select/Move/Rotate/Scale action. Select starts marquee
  selection on an ordinary left drag; while Move, Rotate, or Scale is active, holding Alt at press temporarily routes
  the drag through the same marquee path without switching tools. With the default bindings, Alt replaces,
  Alt+Shift adds, and Ctrl+Alt removes. Gizmo-handle presses still transform, and the optional no-transform state
  disables selection. Modification and placement tools suspend all presented transform actions and restore
  the previous available Select/Move/Rotate/Scale action when released. The cleave angle zero-step label is **Free** at
  the original width.
- Game Preview uses the same suspension contract for Select/Move/Rotate/Scale: a checked, non-transforming Select
  sentinel keeps point input alive while every transform action and interaction is disabled. The edit-mode bar is
  hidden, and Preview point picking is a separate Auto cycle across depth-sorted Thing, baked Light, Surface, and
  Sector candidates up to the first level boundary (or behind it when X-Ray is enabled). Marquee/area selection is
  disabled. The exact prior action, pick mode, Surface/Light submode, and exact-edge sidecar are restored on returning
  to Editor. Settled Cleave lines/history/options are retained while Preview is active and unfinished Cleave gestures
  roll back; Join/Extrude, placement, UV transform, and light-bake previews are canceled.
- Transform toolbar state is optional-exclusive and centrally normalized: Select can be toggled off to disable
  entity selection, modification tools entered from that state restore no transform, and activating a transform while
  a modification tool owns input leaves exactly that one transform checked.
- Move, Rotate, and Scale provide Blender-style ordinary and modifier click selection without switching tools.
- Snapped/coincident endpoints form one normal manipulation handle, including non-adjacent lines; snapping a
  ripped endpoint back to its authored neighbor also restores the reciprocal chain-topology link, while Alt moves only
  the picked endpoint.
- Add Frame uses the shared connector-aware Thing placement solver rather than a bounds-only snap. Frame paths share
  hardened frustum clipping, and the Thing Parameters section remains compact and resizable.
- Grid-axis coverage restores fallback GL state exactly and has large-coordinate regression coverage. X-ray selection
  covers Light-family front-to-back admission and progression; its tested floating-point screen-space bound is
  `1e-4 px`.
- Edited Multi-cut endpoints follow the shared Grid Snap/Ctrl inversion; plain edits use configured Angle step when
  grid snapping is off, while Ctrl+Shift explicitly requests it. Shift geometry/staged-endpoint snapping uses an
  18-pixel base plus the existing staged-handle allowance. The edited line's origin records are excluded so a linked
  endpoint cannot magnetically pull the drag back to its start. A compatible authored neighbor has priority when a
  short target line puts both endpoints inside the wider radius, and the snap immediately creates the shared handle
  and reciprocal chain link. Endpoint handle picking remains 10 pixels so short line bodies stay selectable.
- Large-level Thing placement and selection are optimized at two independent hot paths. Placement's
  exact surface BVH now excludes hidden-sector layers and is rebuilt with geometry/layer visibility; free placement
  reuses the Sector already resolved by the viewport ray, and brush air samples test their preferred Sector before an
  exhaustive fallback. Clean sprite geometry is no longer regenerated merely because any sprite exists: camera motion
  dirties only original type-0 Game Preview billboards, selection consumes cached per-pick sprite ranges, and placement
  fast updates use cached transient-sprite indices after material sorting. Geometry changes retain the existing full
  index rebuild, while Thing-only changes preserve the valid surface index.
- Persistent SPR Things have owner/pick-ID vertex ranges and cached world bounds. Translation and rotation regenerate
  only those six-vertex ranges and perform partial VBO writes. Owner relighting and Sector transitions use the same
  local generator; only broad lighting publication that can affect unrelated owners retains conservative rebuilding.
  Cached instance extrema make material-batch bounds updates O(changed sprites) unless a current extreme moves.
  Direct-click and marquee fallback selection uses
  the projected cached SPR bounds, honors solid-scene depth in ordinary mode, and remains through-selectable in X-Ray
  Selection mode.
- Thing-owned overlay previews stay coherent during deferred Item Edit position/rotation scrubs: transform axes,
  regular/headlight gizmos and range centers, and the Thing type gizmo all follow the uncommitted pose. Thing type
  gizmos cache static resolved type/pick/bounds metadata separately from their camera-facing vertices, so selection
  and ordinary paints do no all-Thing work, camera motion does no inheritance/resource work, and Move/property
  previews use pick-ID shader transforms rather than rebuilding/uploading every visible icon.
- Transform finalization preserves the last preview pose through authoritative targeted publication. Item Edit
  scrubs clear their property preview only after the final Thing ranges have been patched; viewport Thing move/rotate
  commits do the same. Both paths update the existing XYZ/PYR/Sector widgets in place instead of reconstructing
  inherited parameters/resources at mouse release. Tests count successful fast publications and assert that neither
  finalization path increments the Thing mesh rebuild counter.
- Surface nearby-geometry snapping is now Move-only. Free Vertex candidates
  rank by true correction distance, unresolved Vertex/Edge targets never fall
  through to the whole-face solver, and free Edge/Face moves preserve their
  translate-then-small-orientation preview at commit. Axis Edge moves stay
  translation-only; Rotate no longer exposes or retains a nearby-face contract.
- Shaded visual-Thing selection references the selected owners' already-uploaded 3DO/sprite ranges for its stencil
  outline instead of resolving models, copying every triangle, and uploading a duplicate outline VBO. The outline
  pass forces opaque alpha and disables alpha testing, preserving the former solid selection edge for translucent and
  cutout assets. Missing/stale ranges retain the conservative copied-geometry fallback.
- Targeted Thing checker refresh keeps per-Thing row indexes and no longer scans or resizes the complete diagnostics
  tree. The actual consistency rules still run synchronously for the changed Things, so no-Sector/outside-Sector
  errors remain immediate.
- The IJIM low-rate audio tail still lacks audible manual validation.
- Viewport Baked Light placement shares Thing Auto layer resolution. The clicked/containing visible Sector layer
  wins, otherwise the last visible layer is used; all-hidden and empty-layer cases retain deterministic target/layer-0
  fallbacks. The layer is authored before publication, and incremental Lights Outliner insertion honors hidden-layer
  filtering, semantic selection, counts, tint, and lock badges.
- Item Edit applies hidden/locked read-only state to every actionable row after the complete
  panel is built and also rejects mutations centrally; only Layer remains usable. Disabled controls and menu entries
  now have a deliberately muted application-wide presentation. Entity Layer changes rebuild the filtered Outliner and
  clear or replace stale badges immediately. Multi-Thing property Position/Rotation uses subset undo, targeted render
  range publication, one release-time checker refresh, and in-place property updates rather than per-sample dirty
  broadcasts or a final full property rebuild. Thing-placement metadata/loading presentation is prepared before the
  open transition, and its bounded thumbnail workers do not contend with panel animation or the selected preview's
  OpenGL paint loop
- Disabled `ScrubFloatEdit` controls are fully inert and non-focusable, including their
  installed child event filter and hover steppers; disabling cancels rather than commits an active gesture. Multi-Thing
  absolute Position/Rotation Item Edit scrubs now pace persistent range publication at 16 ms and coalesce raw input to
  the newest value. Position containment and targeted checks happen once at release, while the numeric control remains
  immediately responsive. Single-Thing shader preview is unchanged.

## Recommended next steps

1. Spot-check **View Face** with real assets at camera clipping boundaries and in every panel dock; its multi-surface
   active-face decision, 90-degree rotation, and unstable-projection retention now have deterministic regression coverage.
2. Re-verify startup/new-project/open visible dock timing, grid/axis-gizmo presentation, and final player insertion
   offset in the local app. Hidden/pending/show-retry refit state is already deterministic test coverage; native dock
   layout timing is not asserted against a fragile wall-clock delay.
3. Continue the remaining save/load workflows.
