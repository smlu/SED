# Algorithms (Viewport + Editing Tools)

This document describes the key algorithms and interaction models used by SED. It is meant as onboarding
material and as a practical "do not break this accidentally" reference.

For the full lighting data model, mode taxonomy, equations, UI workflow, parity boundaries, tests, and pinned
OpenJones3D/OpenJKDF2 code links, use `docs/lighting.md`. The lighting sections below are implementation summaries.

## Contents

- [GPU picking and peel cycling](#gpu-picking--peel-cycling)
  - [Overview](#overview)
  - [Important details](#important-details)
  - [Orbit around selection](#orbit-around-selection)
- [Selection modifiers and box selection](#selection-modifier-model-and-box-selection)
- [Selection highlight overlays](#selection-highlight-overlays)
- [Grid transform stability](#grid-transform-stability)
- [History entity navigation](#history-entity-navigation)
- [Level entity-ID capacity](#level-entity-id-capacity)
- [Thing frame and path points](#thing-framepath-points)
- [Cleave](#cleave-surface--edge--sector)
- [Join and connect](#join--connect-surfaces--sectors)
- [Merge](#merge-surfaces--sectors--vertices)
- [Transforms](#transforms-including-frames)
- [Viewport entity clipboard](#viewport-entity-clipboard)
- [Episode and campaign parsing](#episodecampaign-parsing-and-migration)
- [IJIM sector thrust editing](#ijim-sector-thrust-viewport-editing)
- [Surface extra light](#surface-extra-light)
- [Asynchronous baked lighting](#asynchronous-baked-lighting)
- [Runtime dynamic-light preview](#runtime-dynamic-light-preview)
- [UV coordinate compatibility](#uv-coordinate-compatibility)
- [Startup and new-project seeding](#startup--new-project-seeding)
- [Add Sector shape drawing](#add-sector-shape-drawing)
- [Surface extrude preview](#surface-extrude-preview)
- [Imported sector geometry](#imported-sector-geometry)
- [Resource pickers and thumbnail loading](#resource-picker-enumeration-categories-and-thumbnail-loading)
- [Sound decoding and preview](#sound-decoding-preview-and-ambient-transitions)
- [Viewport grid rendering](#viewport-grid-rendering-and-draw-order)
- [Query and find](#query--find-workflow)
- [Consistency and resource checking](#consistency-and-resource-checking)
- [NDY/JKL export capacity](#ndyjkl-world-capacity-export)
  - [JKL/NDY presentation options](#jklndy-presentation-options)
- [UV editor display-state rebuilds](#uv-editor-display-state-rebuilds)
- [COG diagnostics and source marking](#cog-diagnostics-and-source-marking)
- [Layer lifecycle, visibility, and locking](#layer-lifecycle-visibility-and-lock-gating)
- [Dynamic sprite and render uploads](#dynamic-spriterender-upload-strategy)

## GPU picking + peel cycling

Primary implementation: `src/app/src/render/gl_view_picking.cpp`

### Overview
- a dedicated pick shader renders the scene to an offscreen FBO where each selectable primitive encodes a stable
  integer ID into RGBA
- the pixel under the mouse is read back and decoded into a hit kind (surface/thing/light/frame/etc.)
- overlapping candidates can be cycled by peeling: re-render while excluding previously hit IDs

### Important details
- pick IDs are assigned per renderable instance
- some sub-modes (edge/vertex) also use CPU-side hit testing with tolerances when GPU picking is not enough
- **Map Settings > X-Ray Selection** is persisted under `viewport/xray_selection_cycling` and defaults off.
  When enabled, repeated clicks in the same place retain/peel candidates behind the front occluder; if the visible
  front candidate is already anywhere in the current selection, the first unmodified click advances immediately.
  Plain clicks and the exact configured Add chord participate in the same depth cycle: repeated Shift-clicks with the
  default bindings add progressively deeper candidates, and an already-selected front candidate is skipped on the
  first additive click. The explicit **Surface Ignore Filters** chord is also a through-pick operation: its first click
  selects the nearest otherwise-filtered face, and repeated clicks fully peel/cycle the Surface candidates, including
  coincident back-to-back adjoin faces. Remove and path chords still target one exact candidate. GPU and CPU fallback
  paths use the same policy. Hidden-layer and wrong-edit-family eligibility remains enforced.
- a cycle continues only within an 8-pixel Manhattan tolerance and 1.5-second timeout, and resets when the transform
  tool, pick mode, Surface submode, selection modifiers, camera/scene serial, level, or option changes. This applies
  to click selection owned by Select, Move, Rotate, and Scale; drag-box selection is deliberately unchanged.
- cycling is intentionally scoped to the active edit family. MainWindow resolves Sector, Surface, Thing/Frame, and
  Light edit modes to an explicit `PickMode`. In the `Auto` fallback, a selected-front skip stays within the kind of
  the front candidate, so X-ray does not jump from a Thing to an unrelated Surface merely by depth.
- Game Preview is the deliberate exception to Editor family scoping. It leaves the Editor `PickMode`, Surface
  submode, exact-edge selection, and tool sidecars untouched, but evaluates every point click as Preview Auto.
  Eligible Thing and currently visible baked-light-gizmo candidates are merged by depth with the first visible Surface and its owning Sector;
  equal-depth ties use Thing, baked Light, Surface, then Sector. Ordinary repeated clicks wrap within candidates at or
  in front of the first solid level boundary. With X-Ray enabled, repeated clicks extend the same ordered set behind
  that boundary. Candidate visibility still uses the normal layer/hidden-Thing/render filters, and camera/scene,
  pointer, timeout, or modifier changes reset the cycle.
- Ordinary Sector picking treats the nearest solid face/depth hit as authoritative and does not let the auxiliary
  projected wire-edge tolerance reorder it. Sector pick passes remain double-sided so a camera inside a sector or
  immediately in front of a wall cannot lose the front sector because its visible face is back-facing. The X-Ray option
  alone enables intentional front-to-back traversal.

### Orbit around selection

- Free-orbit pitch is cyclic rather than clamped. `ViewportCamera::BasisFromOrbit` owns the complete frame: forward
  comes from yaw/pitch, right is the yaw-owned horizontal axis for the entire revolution, up is
  `cross(right, forward)`, and roll rotates right/up together about forward. `ViewportCamera::RotatedOrbit` applies
  input to that complete frame: horizontal motion rotates around world Z, then vertical motion rotates around the
  yawed *visible* screen-right (including roll) before recovering the nearest equivalent yaw/pitch/roll branch. This
  keeps the view matrix, panning, camera-facing sprites, public camera axes, axis-widget drag, and
  fixed-target/selected-pivot MMB paths on one frame. In particular, a bank installed by F2 is not held as a stale
  scalar while pitch moves around some different unrolled axis.
  A world-up `cross(forward, Z)` is intentionally not used because it reverses screen-right as `cos(pitch)` changes
  sign at a pole. Pitch is wrapped to an equivalent `[-180, 180)` representation only to keep stored state bounded;
  it is never clamped for free orbit. RMB Fly remains a conventional world-upright camera: yaw is around world Z,
  pitch is constrained to `[-90, 90]`, roll is always zero, and lateral movement is reconstructed from forward and
  world up. On RMB press, `UprightFlyOrbit` converts the *actual visible forward* from an F2-banked or pole-crossed
  orbit into that canonical yaw/pitch representation. The eye and current target are retained; after F2 that target
  is already the exact grid origin. Thus Fly straightens the presentation without changing its navigation origin or
  inheriting the inverted scalar axes that represented the banked view. Editor and Game Preview use this same path.
- This intentionally avoids two behaviors found in the original Pascal source: `TCamera.RotatePitch` clamps at the
  poles, while the alternate revolution path rotates basis members independently without rebuilding one coherent
  orthonormal frame. Neither behavior is required by the F2/F3/Shift+S compatibility contract. Exact axis views still
  use deterministic canonical yaw/pitch targets in both projection types.
- Original Pascal F2 assigns renderer `zv` from the grid normal and `xv` from grid X. Its MR_new vertical camera drag
  calls `RotAxisAngle(nz, ny, dy)`, which preserves that `xv`. The C++ full-frame delta keeps the same important
  designer contract while retaining the modern world-Z horizontal orbit: after F2 on a banked face, vertical MMB
  pitch preserves screen-right/grid-X exactly. Orbit Around Selection uses the same orientation delta while rigidly
  rotating eye and target around the semantic pivot, so roll is part of the returned rig state rather than discarded.

- **Orbit Around Selection** is exposed in both Viewport Preferences and Map Settings, persists through the shared
  `viewport/orbit_around_selection` key, and defaults on.
- Plain middle-mouse orbit resolves one semantic pivot from the complete current Sector, Surface/sub-element, Thing,
  Frame, or baked-Light selection. Multiple entities use the mean of their individual semantic centers; active entity
  state is a deterministic fallback when a legacy/Auto path has not populated the aggregate selection.
- The pivot is scoped to the current MMB drag. The camera eye and its existing look target rotate rigidly around it,
  so merely pressing MMB cannot recenter the view and the selected pivot retains its screen position while revolving.
  Turning the option off also cancels a captured pivot immediately, including during an active MMB drag; that drag
  and subsequent drags resume the legacy fixed-camera-target revolution. Empty selections, disabled selection orbit,
  Shift+MMB pan, and Ctrl+MMB dolly keep that previous navigation behavior.

## Selection modifier model and box selection

Primary implementation:
- `src/app/src/render/gl_view_input.cpp`
- `src/app/src/main_window/events/event_filter.cpp`
- `src/app/src/main_window/viewport/signals.cpp`
- `src/app/src/ui/shortcuts/*`

Current behavior:
- viewport mouse gestures retain their physical-button capture through Qt's preliminary QWindow release. Only an
  actual outside QWidget release for the captured button cancels them; the viewport-widget release completes picking
  or editing. Unrelated button releases are ignored, and capture loss still cancels unfinished gestures
- in Select tool mode, left-drag starts box selection by default
- Move, Rotate, and Scale retain their normal click/handle behavior, but Alt held at left-button press temporarily
  routes the drag through the same box-selection path without switching tools. Alt is removed before resolving the
  selection operation: Alt replaces, Alt+Shift adds, and Ctrl+Alt removes with the default bindings
- click selection uses the mouse event's modifier snapshot rather than re-reading global keyboard state after the
  signal is delivered
- a configured add/remove click is dispatched even when the clicked entity is already the active viewport entity;
  this lets the default Ctrl-click remove that exact member from a multi-selection and promote a remaining member
  without changing the normal unmodified click-on-active behavior
- add/remove/path/bypass behavior is resolved through the keymap helpers:
  - `selectionAddModifierBindings()`
  - `selectionRemoveModifierBindings()`
  - `selectionShortestPathModifierBindings()`
  - `selectionBypassSurfaceFiltersModifierBindings()`
- every configured modifier-only alternative is an independent chord; all modifiers in one chord are required, and
  alternatives are ORed. Add/Remove/path/bypass selection matching is inclusive, so extra held modifiers do not cancel
  a matching selection chord. Tooltips list the alternatives; viewport status hints identify the matched one
- X-ray cycling admits an exact configured Add alternative in addition to no modifiers. Extra modifiers intentionally
  make it a different explicit command, so remove, path, and filter combinations cannot unexpectedly peel behind the
  entity on which that command was invoked
- X-Ray applies to both ordinary Select-tool marquees and the temporary Alt marquee used by Move, Rotate, and Scale.
  It removes only scene-depth occlusion; hidden-layer and edit-family eligibility still apply
- Game Preview admits point selection only. It rejects Select drags, Alt-transform drags, and their X-Ray variants at
  both the `GlView` gesture boundary and MainWindow signal boundary, so a stale/programmatic marquee cannot alter the
  Preview selection.
- Fly navigation owns the complete pointer/key stream until its captured configured button is released (RMB by default).
  Fly movement/fast/slow keys are dispatched first;
  all other shortcuts are consumed, and Shift/Ctrl/Alt do not update transform, placement, or cleave modifier previews.
  Releasing the captured Fly button restores the current physical modifier state for subsequent editor interaction

Surface-specific rules:
- normal surface box selection filters out non-solid/backfacing surfaces based on current surface render mode
- bypass modifiers let the same operation include surfaces that would otherwise be filtered
- shortest-path selection can traverse through skipped surfaces while only committing valid target surfaces unless the
  bypass modifier is also held
- point picking also follows the wireframe the user can actually see. A wireframe viewport, or a solid viewport with
  the level-wire overlay enabled, admits a narrow projected-edge hit for Sector and Surface faces, including
  non-rendered adjoin faces. Repeated clicks can therefore walk the faces sharing that edge. Solid rendering with the
  overlay disabled does not make invisible edges clickable. Face/Sector supplemental projected edges must be within
  six logical pixels of the pointer; Edge/Vertex candidates use the same 16 logical-pixel radius in the GPU projected
  supplement, CPU world-space broad phase, and final screen-space acceptance. The GPU supplement also stays within
  1.5 pixels of the closest eligible edge, so both owners of one shared edge remain candidates without admitting an
  unrelated projected wire.
  Perspective CPU tolerance converts the horizontal FOV with logical viewport width (not height); DPR cancels and
  changing aspect ratio cannot silently change the horizontal target size.
  With X-Ray off, the edge depth must also be at or in front of the sampled opaque scene depth, so a rear wire cannot
  select through a solid wall. Explicit through-selection and X-Ray traversal are the only depth bypasses
- ordinary Face, Edge, and Vertex marquee candidates use that same sampled opaque-depth boundary. Edge samples come
  from the clipped part of the projected segment inside the box, and the CPU point/fallback paths apply the same test
  before merging supplemental wire owners, so a rear sub-element cannot be reached by skipping a front face
- scene-depth comparison uses only a small raster/float error band rather than a fixed normalized-depth offset, so
  near-behind geometry remains occluded in far perspective views. Point picking uses the exact pointer pixel when it
  contains geometry and consults the four neighboring pixels only when that exact pixel is background at a raster seam
- Sector volume selection remains two-sided, but ordinary Face, Edge, Vertex,
  Thing, ARCH owner/model-vertex, and Preview Auto point/depth passes match
  visible Level-face eligibility. A nearer single-sided backface from outside
  its Sector is neither selectable nor an occluder, so it cannot block an otherwise
  unobstructed Surface element or Thing in the same or another Sector. A
  genuinely nearer front-facing or double-sided solid wall still occludes the
  target. This contract is projection-independent and applies equally in
  perspective and orthographic views. The nearest eligible boundary retains
  every coplanar/shared-edge owner while rejecting genuinely deeper
  front-facing walls. The GPU path uses normalized scene depth; its CPU
  fallback filters facing first and then uses a scale-aware double-precision
  ray-distance band. X-Ray and the explicit through-selection modifier restore
  backface selection and deeper traversal
- Baked, Thing, and IJIM Sector light-gizmo clicks and marquees use this same visible-face occlusion policy, even
  though a Sector light's owner is a Sector. Their depth pass rejects invisible single-sided backfaces in both CPU
  fallback and GPU paths. Icon hit targets remain measured in logical pixels, independent of distance and DPR.
  Clicks and marquees share the float-safe scene-depth comparison; neither uses a fixed normalized-depth allowance
  that would expose lights behind opaque walls in distant perspective views.
- **Strict** Surface selection snapshots the selected faces when entering Edge or Vertex mode, then filters both GPU
  and CPU fallback candidates to that face set. It is available to Select, Move, Rotate, and Scale because the filter
  belongs to the shared viewport picking path. Native/autosave/recovery EDIT retains this frozen set and the
  remembered Face selection in optional `surfaceSelectionContext`; legacy map-absent records infer from current owners,
  while present-empty sets remain empty. Known Structure+LevelGeometry changes invalidate frozen scope/stale mode
  cache/components, seed remembered Face selection from committed current faces, and force Face. Cold native History
  navigation checks actual owner/connectivity changes for None, partial topology masks, and unsupported dirty bits;
  matching geometry-only transforms and Thing-only structural changes preserve compatible context and committed Face
  selection. The all-domain `All` sentinel remains a known paired topology mask
- switching Sector -> Surface selects every face of explicitly selected sectors. A Surface -> Sector -> Surface round
  trip recognizes the automatically mapped owner sectors and restores the exact previous face selection instead
- switching Face -> Edge/Vertex remembers the selected owner faces. Returning to Face restores every still-valid
  remembered face (or derives owners from the selected vertices when no memory exists), clears the vertex selection,
  republishes the primary face to the viewport, and refreshes Cleave/Extrude availability immediately. Any armed
  Face-only tool is disarmed on entry to Edge/Vertex; a settled Face Multi-cut draft is retained for return to its
  matching context. Explicit Edge Cleave remains available in Edge mode
- Surface marquee selection is submode-specific. Face mode uses projected polygon overlap; Edge mode records every
  exact projected polygon edge that crosses the box; Vertex mode records only the exact projected vertices inside the box.
  Vertex-mode owner `SurfaceRef`s are derived context for wire rendering and strict scope, while explicit
  `SurfaceVertexRef`s remain authoritative across replace/add/remove operations. Ordinary solid selection batches
  scene-depth samples at those exact projected vertex points, so a partially visible face cannot admit a vertex
  hidden behind nearer geometry; X-Ray and explicit through-selection retain every eligible projected vertex
- exact Edge Ctrl-remove follows the visible selection overlay's canonical endpoint-coincidence test. It removes every
  selected owner ref for the picked shared/adjoin segment at that world-space depth while preserving adjacent or
  merely collinear edges with different endpoints; ordinary facing and scene-depth eligibility remain unchanged

## Selection highlight overlays

Primary implementation:
- `src/app/src/render/gl_view_mesh.cpp`
- `src/app/src/render/gl_view_render.cpp`
- `src/app/src/render/mesh/wireframe_overlay_utils.hpp`
- helpers under `src/app/src/render/selection/`

Behavior:
- highlight lines are built as a dynamic `GL_LINES` list
- highlight points are built as a `GL_POINTS` list
- level-surface line vertices carry a non-collinear point only when their owner is a valid Solid/Texture polygon with
  usable UV data and no explicit translucent/no-Z-write face flag. The thick-line geometry shader reconstructs that
  fill's projected depth plane on the inward side of the edge, rather than incorrectly holding both sides of the
  expanded quad at the centre-edge depth. Opaque adjoins remain eligible because the adjoin flag alone does not disable
  depth writes. NotDrawn, Vertex, Wireframe, projected-sky, preview-only, explicitly translucent, no-Z-write, and
  invalid-UV surfaces keep their true edge depth. Once MAT alpha is available, material-keyed ranges disable support
  references for alpha adjoins in base and selected overlays; texture toggles and cache reloads reclassify once per
  transition. Missing/unresolved MATs retain the opaque fallback. The correction stops at the projected reference-side
  boundary, so a very narrow/grazing surface cannot extrapolate an edge arbitrarily toward the camera
- line segments are clipped to all six OpenGL homogeneous frustum planes plus a positive-`w` guard before the geometry
  shader divides by `w` and expands them. Rejected or partially visible segments therefore cannot flip into an
  unbounded screen quad at the near, far, or side planes
- geometry-shader line quads render double-sided. Their triangle-strip winding depends on projected endpoint order,
  so inheriting face culling from a preceding grid or Thing-outline pass would make edges disappear as the camera
  crosses an angle boundary
- scene fills and wire overlays use unbiased depth. The former positive-fill/negative-wire polygon offsets and fixed
  clip-space pull could exceed the true separation between nearby sectors at grazing angles and expose rear-sector
  edges through solid walls
- the line pass retains `LEQUAL` scene-depth testing. For surfaces whose depth-write routing is known during mesh
  construction, only their filled face can match expanded same-surface fragments; a nearer scene surface remains
  authoritative. The thick-line triangle strip is split at the edge center and, when the supporting reference falls
  within half its width, at that exact reference boundary. These vertices preserve the bounded depth correction during
  raster interpolation, keeping it off the outward half and preventing thin supporting faces from dropping wire pixels
  as the camera moves. Adjoin line ranges are recorded by material during mesh construction, then reclassified after MAT
  resolution: alpha-textured adjoins lose their depth support in base and selected overlays because their translucent
  pass does not write depth; opaque and unresolved-MAT adjoins retain it. Blend enable/functions, multisampling, line
  smoothing/hint/width, cull enable/mode/winding, depth enable/function/write mask, and polygon-offset values and
  enables are restored exactly after the line pass. The enclosing point/line overlay also restores its entry depth
  state, including when no later gizmo pass runs
- clip-enabled orthographic rendering and screen projection share a transient near/far range derived from cached level
  and transformed-Thing bounds, intersected with the authored clip planes. The stored camera settings are unchanged.
  The depth-tested grid plane's visible viewport footprint is included when finite; Always Behind and Visible Through
  Walls grids retain the authored projection because they do not use scene depth. Empty/invalid scene bounds retain the
  authored projection as well
- selected Sector/Surface edges are drawn after ordinary wireframe with a narrow dark contrast halo and a tiny
  selection-only coplanar depth lift. This masks the duplicate white edge and keeps the selection readable over
  textured fills while retaining scene-depth occlusion by genuinely foreground geometry
- selected Sector/Surface geometry also builds contiguous triangle ranges into the existing level VBO when the
  selection changes. The line pass marks those ranges in one stencil bit, clips only the dark outer halo to the
  outside of the selected fill, and then draws the complete selected polygon wireframe with the normal depth test.
  Internal surface edges therefore remain visible: the stencil pass is only an outline/halo mask and is never a
  silhouette-only replacement. Hidden layers are never included, and the temporary stencil bit/capabilities are
  restored before later overlays.
- in Vertex mode only valid, explicitly selected sector vertices are emitted as colored points; duplicate references
  through adjacent owning faces collapse to one point, invalid references emit nothing, and unselected vertices are
  never inferred from an owning Surface. Shift+LMB makes the exact clicked vertex active: it is emitted once in red,
  previous members remain blue, its owner face is emitted in the primary yellow range, and other owner faces remain
  secondary blue. Outliner synchronization preserves that exact active ref instead of replacing it with unordered-set
  iteration or a lexicographic fallback
- in Edge mode owner `SurfaceRef`s remain context while a transient, endpoint-validated local-edge set is authoritative.
  Every selected edge is omitted from the thin owner wire. Non-primary selected edges are emitted once in a wider
  dedicated orange top layer (configurable separately from Face multi-selection), the active member is emitted once
  in the active-selection color, and its remaining owner wire is primary yellow, in both visible and occluded passes.
  Other selected owner faces retain their secondary-blue wires when Edge selection is added or removed; only a plain
  replacement click replaces that Face context. Coincident exact refs through adjacent owners collapse
  to one top-layer segment with the active color taking precedence. Point replace/Shift-add/Ctrl-remove and marquee
  replace/add/remove update that exact set. Removing the active exact Edge promotes a deterministic surviving Edge;
  its owner is published as the primary Surface in the same update, so the removed owner cannot remain primary while
  the new active Edge is omitted from both exact-edge color ranges. Document,
  mode, and submode transitions clear it; geometry publication revalidates each local edge against its captured
  endpoint ids, so undo/redo or topology changes cannot silently retarget a stale local number. This keeps unselected
  owner edges in face-wire styling and prevents secondary/active antialiasing colors from blending on one segment.
  An explicit selected edge also suppresses every coincident copy emitted by
  another selected owner face: the edge's orange/red role takes precedence over
  the other face's blue/yellow wire role, including the common case where the
  other face is the active yellow face
- the optional **Normals** overlay draws an arrow from every selected face's planar polygon area centroid along its authored
  normal in face, edge, and vertex submodes, using the same selected-geometry through-wall overlay policy. The shared
  area-centroid definition supplies the singleton face-mode transform pivot; Cleave computes the same centroid
  contract in a scaled local double frame through `cleaveSurfaceCenter(...)`. An edge/vertex gizmo can still use its topology-specific hit pivot because the normal represents the face,
  not the sub-element. For multi-selection each arrow stays on its own face while the transform gizmo uses the
  aggregate selection AABB. In Edge mode it also draws the selected edge's
  normal at the edge midpoint: the normal-manifold case uses the bisector of
  the two owning face normals, with the active face normal as the recovery
  fallback. The arrowhead has one camera-facing pair of wings; only its compact line-vertex range is
  patched when the camera changes, so billboarding does not rebuild selected geometry or dirty editor state.
- the overlay can be toggled in the Surface sub-mode row or persisted under Preferences > Viewport > Overlays as
  **Show selected surface normals**; both controls update the same live setting. Preferences also expose clamped
  length and arrowhead scale factors (`0.1x..4.0x` and `0.1x..3.0x`) plus an arrowhead visibility toggle. Changes
  rebuild only the selection overlay and apply while the Preferences dialog is open.
- thing multi-selection uses stencil-style outline rendering rather than generic wireframe fallback

## Grid transform stability

- Map Settings groups grid controls into adjacent, initially closed **Grid Behavior**, **Grid Appearance**, and
  **Grid Transform** disclosures. Appearance owns spacing, colors, thickness, and opacity. Transform edits the
  recovered origin (`X/Y/Z`) and presents angles in `Pitch/Yaw/Roll` order; XY/XZ/YZ quick-plane buttons and its sole
  Reset (XY, origin `0,0,0`) update the live GlView state. The panel reserves its widest expanded disclosure at
  construction, so opening a section changes only height and never shifts/resizes the panel horizontally. Grid command
  buttons show their current Preferences shortcut after the command name; `QAction::changed` refreshes that suffix
  immediately after a keymap edit.
- **View > Grid > Align View to Grid Plane** reconstructs an orbit-camera yaw/pitch/roll whose screen-right and
  screen-up axes match the current authored grid axes. The same animation targets the authoritative grid origin by
  changing orbit pan only; grid origin/orientation, orbit distance, projection, and orthographic scale remain unchanged.
  Programmatic F2 animation samples bypass the manual-camera-change grid-reset policy; on completion the reset policy is
  re-anchored to the new aligned camera so the next actual manual rotation can still clear temporary grid overrides.
  The action uses a deterministic pole yaw and retains JED's `F2` binding; the reciprocal Grid-to-View command retains
  JED's `F3`, while Find Next uses `Ctrl+G`.
  Rotate-Grid-to-View naturally resolves to the current view and is therefore a no-op. After a full free-camera
  revolution, F2 still preserves the authored origin/axes, F3 preserves the origin while copying the analytic camera
  right/up frame, and Shift+S still derives its result solely from the active design item.
- **Snap Grid to Item** resolves only the active edit/pick mode; retained selections from another mode cannot steal the
  command. Sector, Thing, Frame, and a single Surface vertex move the origin without changing orientation. Light mode
  resolves the authoritative emitter position for the three emitter-oriented families: baked lights, regular/Head Thing
  lights, and IJIM Sector point lights; the editor-only ARCHLIGHTING family has model-vertex selection rather than a
  runtime emitter, and presentation-only lifted Thing-light icons never displace the grid.
  Surface Face uses its first vertex/edge and normal, Surface Edge uses the active edge's first vertex/direction and
  face normal, and three selected Surface vertices may define a new plane while retaining the active vertex as origin.
  One or two vertices do not invent a plane and therefore preserve the current grid axes, matching legacy JED.
- View-aligned axes are rebuilt from a finite orthonormal camera basis with deterministic pole fallbacks. Failed
  ray-footprint estimates use the orthographic scale/viewport height and the shader footprint is never allowed to
  collapse below eight major cells. This prevents rotate-to-view grids disappearing during orthographic camera
  rotation or close perspective zoom.
- **Keep rotated grid visible** is a separate default-off display policy, not a change to Rotate Grid to View or the
  authored snap plane/lattice phase. If the authored view-aligned plane is within/behind a safe near-plane distance, rendering uses a parallel
  guide at `max(4 * abs(near), 0.05)` in front of the eye. The authored origin is projected along the view normal onto
  that guide so both in-plane lattice coordinates retain their exact phase. Transform snapping continues to use the
  authoritative plane/origin; adaptive spacing alone is measured on the visible proxy for both drawing and snapping.

## History entity navigation

- History rows navigate from compact before/after selection metadata and exact structural post-operation navigation
  metadata, never by parsing their display label. Sector and Surface cleaves therefore select/frame only their exact
  new result, not the source or every related face
- double-click and **Go to & select** cancel modification/placement owners, switch to the required edit/submode, arm
  Select, and install the target selection. **Go to** changes only the history cursor and camera, preserving selection,
  mode, and tool state when their references remain compatible. A topology-changing history step clears affected
  Surface Strict/component context and returns that Surface submode to Face
- when Surface context needs protection, History navigation prepares exact topology evidence: Sector count,
  IDs/numbers and vertex/Surface counts, Surface numbers, ordered vertex indices, and adjoin references. None, partial
  Structure/LevelGeometry masks, and masks containing unsupported bits are ambiguous for this check. The five defined
  domains are LevelGeometry, Things, Lights, Cogs, and Structure; `All = 0xFFFFFFFF` remains the known all-domain sentinel.
  Unsupported bits use the local unknown-dirty UI refresh path without rewriting stored history or HIST masks
- a protected multi-step jump whose metadata places a known paired topology step before an ambiguous step prepares
  exact adjacent-state comparisons by walking a detached Document copy before live cursor changes. Its current Level
  is copied while history uses copy-on-write storage. Required topology copies/captures finish there, and the probe is
  destroyed before live swaps. The live topology check consumes the recorded first differing step, with invalidation
  sticky for the rest of the jump. Other protected jumps use the jump-start baseline, and known paired masks retain
  their existing path. No new fallible topology preparation occurs in the live swap loop
- coordinates, UVs, lighting, and Things are outside the topology evidence, so pure Move and Thing-only controls retain
  compatible Surface context and committed Face selection. Failure in the new topology preparation rejects navigation
  before the first live history swap. This preparation is confined to history navigation; ordinary Move/Rotate paths
  do not gain a detached Document walk. Cleave draft cancellation and workspace mementos retain their separate raw-mask
  policy
- bounds framing uses the same navigation preference path as outliner navigation. Its animation begins before the
  heavier outliner/property refresh so undo/redo publication and camera movement do not produce a visible second jump

## Thing frame/path points

Primary implementations:
- viewport overlay + picking: `src/app/src/render/gl_view_input.cpp`,
  `src/app/src/render/gl_view_picking.cpp`
- UI selection + tools: `src/app/src/main_window/viewport/tools_overlay.cpp`,
  `src/app/src/main_window/properties/panel.cpp`
- core transforms: `src/core/src/level/transform.cpp`
- parsing/formatting: `src/core/include/sed/parse_frame.hpp`, `src/core/src/values.cpp`

Storage format:
- frames are stored as repeated `TplValue` entries named `frame` with `ValueType::frame`
- serialized token format is `(x/y/z:pch/yaw/rol)`
- `numframes` is synchronized automatically
- Selection identifies a frame by its owning Thing and raw `Thing::values` index. Synchronizing `numframes` may insert
  or remove earlier values, so viewport Add resolves the appended frame after synchronization and installs selection
  inside the authored undo callback. Properties Frame Add and Duplicate preserve selected frame ordinals; Frame Delete
  drops the deleted ordinal and shifts later ordinals. Only selections on the affected Thing are remapped.
- Removing an ordinary Thing parameter or restoring a template override preserves selected frame ordinals on each
  affected Thing, so erasing an earlier parameter does not leave a raw reference aimed at a different value.
- Undo/Redo captures frame counts for owners named by current FrameRefs, the transition's before/after FrameRefs, and
  a singleton selected Thing. Current selected-frame ordinals are captured before the history operation. A changed
  count or same-ordinal stored index makes the UI retain the restored snapshot's frame selection; this also restores
  last-frame Delete selections when the current set is empty. Stable pose changes preserve current selection and
  explicit deselection. The probe copies only metadata and scans those owners' values, never the complete Level.
- Native Save first validates the captured live EDIT selection, maps selected/active frames by ordinal into the
  normalized candidate, and installs that mapped selection in the isolated HIST snapshot. Historical frame-count
  normalization independently performs the same ordinal conversion for each historical selection. Invalid source
  references or missing corresponding candidate frames reject the save preparation rather than selecting another value.
- Frame Add keeps one compact visual-owner proxy per selected Thing, document signature, and resource identity. It omits
  repeated frame values while retaining local overrides, inherited template resolution, and the owner layer. Persistent
  option values and the currently held modifier produce one effective option state; hover and click use the same resolved
  pose, and preview never edits the document.
- Frame Add and Frame Move publish owner-subset history. A Frame Move gesture captures one virtual moving pose and its
  support points, while a separate stationary-anchor cache lazily stores compact poses with repeated Frame values
  stripped. It retains each anchor's authored transform and local visual overrides; the owning Thing remains eligible
  as a snap target. Pointer samples reuse these snapshots and the existing conservative solver without rebuilding
  support from the full Level or copying long Frame lists.
- Viewport Frame Add/Move changes invalidate only the owning Frame pose/model-ghost records and Thing-picking peel cache.
  Ordinary Thing geometry remains reusable. Selection is restored from the raw FrameRef resolved after `numframes`
  synchronization inside the same authored transaction.

Robust parsing contract:
- Frame tokens require exactly three position components or six position/orientation components; partial angles,
  extra components, malformed numbers and unrepresentable numbers are rejected instead of silently discarded
- `TplValue::tryGetFrame(...) -> bool` leaves all six output arguments unchanged on rejection; successful
  three-component input explicitly sets all three angles to zero
- `TplValue::getFrame(...)` defaults all six outputs to zero for malformed text; allocation exceptions propagate
  without changing any caller output. Parsing helpers that allocate do not promise `noexcept`
- critical code paths avoid silent fallback to `0,0,0`
- Thing translation and Frame translation/rotation/positional scale/reflection stage selected values before publication;
  arithmetic overflow rejects the whole batch and allocation failure preserves the Level. Frame scale changes stored
  positions only; neither frame points nor ordinary Things expose a rendered-object size scale in the viewport
- these transform APIs return true only for an authored numeric pose or sector change. No-op/rejected edits retain
  original Frame spelling, including noncanonical legacy tokens. Repeated references retain sequential application
- viewport Thing/Frame transform commands use a compact selected-Thing history transaction; rejected edits preserve
  both history stacks. Frame orbit resolution distinguishes an absent pivot from allocation failure, and rejects
  the complete failed mouse gesture without moving the camera

Viewport representation:
- frame mode draws a visible path polyline plus point markers in stored order
- selected point is highlighted more strongly
- sizing is world-space oriented rather than fixed pixel size
- path points and polyline segments use CPU/QPainter homogeneous-frustum projection. Points behind the near/camera-front
  plane are rejected and crossing segments are clipped before perspective division; the seven clip constraints (six
  frustum planes plus the positive-`w` guard) are solved as one double-precision interval, and final endpoints tolerate
  and clamp float-sized boundary noise before QPainter projection. Model-edge ghosts use the separate instanced GPU path
  described below, with matching seven-plane clipping and bounds culling
- each owner parses at most 65,536 stored Frame values into a bounded pose cache. Larger lists draw the cached prefix;
  a selected value beyond it uses a one-index lookup, so selection and gizmos remain available without expanding the
  path cache to the full authored list
- non-active model ghosts use one validated local edge buffer per resolved immutable 3DO source, with undirected edges
  deduplicated within each mesh while mesh boundaries remain distinct. Persistent per-owner instance buffers carry
  Frame transforms; camera changes transform cached model bounds and cull instances against the same seven clip
  constraints before drawing visible ranges. This keeps camera motion independent of source-face traversal and buffer
  uploads while preserving the existing depth-disabled, through-wall ghost appearance
- pose and topology retention are bounded. A pose edit invalidates only its owner; Level publication, template/model
  source replacement, and resource changes invalidate affected identities. The caches admit 64 owner entries / 32 MiB
  of parsed poses and instance matrices, eight model entries / 64 MiB of topology and expanded endpoints, and 64 MiB
  of ghost GPU buffers; each model is capped at 512K unique edges, a 16 MiB endpoint buffer, and 8M source-edge visits.
  Context recreation releases GPU objects but retains CPU pose/topology data for the next context. If the Frame prefix
  is exceeded, only that prefix supplies path points; a model whose validated topology exceeds its cap is negatively
  cached and its model ghosts are omitted. The path, point markers, and selected full model continue to draw
- Add Frame and Frame Move pose a temporary copy of the owning Thing at the frame coordinates. The copy retains the
  owner's inherited model/sprite/bounds/connectors, so Surface Stick, Align to Surface, Thing Snap, Rotate to Snap,
  free placement, and grid-plane contact use the same visual support semantics as ordinary Thing tools. Only the
  serialized frame value is committed; the owner Thing remains unchanged

## Cleave (surface / edge / sector)

Primary implementation: `src/core/src/level/cleave.cpp`
Public API: `src/core/include/sed/level/cleave.hpp`
Viewport implementation:
- interaction/state: `src/app/src/render/gl_view_input.cpp`, `src/app/src/gl_view.hpp`
- drawing/labels: `src/app/src/render/gl_view_render.cpp`,
  `src/app/src/render/cleave/cleave_measurement_utils.hpp`
- options and commit routing: `src/app/src/main_window/viewport/cleave_options_overlay.cpp`,
  `src/app/src/main_window/viewport/signals.cpp`

Shared primitives:
1. classify vertices against a plane
2. split crossing edges and insert new vertices
3. split surfaces
4. preserve complete reciprocal adjoins, rejecting and rolling back incomplete mirror repairs

Core plane classification and vertex reuse use component-wise double roundoff bounds, not a fixed world-space
welding distance. Editing admission distinguishes bounded source rounding from errors introduced by an operation.
Source face planarity is capped by both the legacy 0.001 world-unit limit and 0.3% of intrinsic face width
(`2 * projected area / perimeter`), so a long axis cannot hide material warp in a thin face. Source reflex depth also
has a feature-size bound in addition to the legacy corner-turn limit; crossing and backtracking boundaries are rejected.
These admission limits are independent of the Consistency Checker's precision preference and never act as weld distances.
For Multi-cut, bounded rounding already present in the source is inherited for compatibility; stricter planarity,
convexity, and topology checks apply to newly created geometry and operation-introduced errors.

Plane intersections evaluate from the endpoint nearest the cutting plane; UV/RGBA interpolation evaluates from the
endpoint nearest the inserted point. Direct endpoint-relative parameters retain tiny offsets and avoid cancellation
when an edge is traversed in the opposite direction. Failed intersection calls preserve every output argument.

Closed-boundary admission first checks the authored indexed shell: every raw edge must be nonzero, occur exactly twice,
belong to two different faces, and have opposite directed incidence. A shell that satisfies that proof is accepted
without coordinate aliasing, so a nearby vertex on an unrelated edge cannot create a false open boundary. Inputs with
coordinate aliases or T-junctions use the geometric fallback, which canonicalizes coincident vertices and expands each
edge into one deterministic split chain before checking the same two-sided incidence rule.

Cleave captures each source face's geometric normal, corners, plane residual interval and supporting half-space minimum
once per transaction. Descendants retain those fixed profiles through face splits, Sector remapping and repeated cuts;
validation does not recapture a larger allowance after each stage. Retained reflex corners require their original
incident-edge witnesses. Closed inputs must remain closed, and rejected edits restore their complete input state.

For a source-compatible rounded portal, the immutable profile also records the indexed boundary-chain evidence needed
to match a long edge with a subdivided counterpart. Each side uses its own native intersection with the requested cut
plane; an authored vertex is never moved to manufacture a shared coordinate. Captured chain evidence retains each
sector's indexed original vertex identity, so a coordinate duplicate on a disconnected face cannot supply a portal proof.
Repeated cuts and save/reload reconstruct
the evidence from persisted topology and source coordinates, so correctness does not depend on a transient edit journal.

Structural edits capture valid COG Sector/Surface targets before changing geometry. Surface targets follow the
corresponding retained or moved face, then receive the final flattened number used by game export. Changed references
discard stale paired text. When a valid original fallback target exists, invalid preferred text cannot become a different
target merely because a face or Sector was appended. Originally unresolved raw values remain unchanged. Reference publication cannot allocate and happens only
after geometry validation, within the same undo transaction.

JKL/NDY input-device errors reject the complete read rather than treating a truncated stream as ordinary EOF. Sector
`SURFACES` spans require both bounded nonnegative integers and a range inside the global face array before iteration;
an empty span at the array end is valid. Float-backed colour and light channels reject non-finite or unrepresentable
values while retaining signed HDR and decimal spellings that round to a finite maximum float. The core JKL/NDY path
writer validates and serializes first, writes and closes an exclusively owned sibling staging file, then replaces the
destination atomically. Reported write, flush, close, or publish failure retains the previous destination and removes
the unpublished staging file. Caller-owned stream output does not provide path-replacement rollback.

Thing deletion clears or decrements later COG Thing references, and vertex deletion updates paired Surface text together
with the typed global target. JKL import maps each wire `SURFACES` span to the editor's global face identity before
hydrating typed COG values, so noncanonical span order cannot retarget a placed argument. The wire-span map resolves
hydration, while `importedJklCogRowsPresent` and `importedJklSurfaceRefs` retain pending provenance when needed: the
native writer stores meaningful state in optional core-owned `jimp` V1, and optional HIST `jklCogImportV1` carries the
explicit current state plus sparse undo/redo provenance. A false marker with an unused map is normalized away, so
ordinary Levels keep their established bytes; nested `HistoryNested` Levels remain exactly the eight required sections.
For reordered global-surface input, missing or ambiguous mappings or schemas are rejected before publication or save;
identity-order input may use the raw row-index fallback. See the [native import-state contract](formats/sed_binary_format.md#jimp)
and [HIST schema](formats/sed_binary_format.md#hist) for encoding and recovery behavior.

An exact convex solid cut through its interior yields two convex solids. Rounded original files can already encode a
slightly reflex literal boundary, whose exact cross-section inherits that bend. Such a cap is accepted only when its
vertices are certified original-edge/cutting-plane intersections and its edges match the complete split side boundary.
Every cap remains simple and lies on the requested plane; every new cutting half-space remains arithmetic-strict.
Strictly convex input lineage still requires strictly convex caps. The operation does not flatten or move original
corners to manufacture mathematical convexity.

Texture-corner data follows the topology instead of being reprojected after a cut. Every retained original corner keeps
its exact UV and baked-light RGBA values. A genuinely new edge-intersection corner linearly interpolates those values
along that face's source edge, including on a neighboring face or mirrored adjoin whose UV frame differs. New Sector
cap faces are the only cleave output that receives a newly generated default UV frame and derived floor classification.
The same native-edge rule applies when a reciprocal portal represents one boundary with additional collinear corners:
the target face computes its own intersection and interpolates its own source edge tuple, while all authored positions
and existing UV/RGBA tuples remain byte-for-byte unchanged.

Surface and Edge Cleave retain rollback copies of all affected sectors, including mirrored portal neighbors. Sector
Cleave stages a candidate Level. Allocation failures in preflight, interpolation, material copying and portal matching
reach the public rejection boundary instead of terminating through an internal `noexcept`. Multi-cut prepares its full
result vector before the no-throw Level publication; allocation rejection returns `CleaveFailed`, the input Sector,
no failing plane/surface index and no output descendants. Runtime marks and renderer caches are restored on rejection
alongside authored data. A failed rounded-chain certificate or propagated portal edge follows the same rollback path;
partial mirror topology is never published.

Immediate Surface Cleave validates a scoped set consisting of the selected face, faces whose indexed or coordinate edge
receives the cut vertex, and reciprocal portal faces that are directly split or reconnected. It still proves the whole
affected Sector boundary and convex result, and it rejects malformed data on every face in that scope. Unrelated faces
retain all authored fields, cached values, UV corners, and portal references even when a full Sector Cleave would reject
the same legacy defect. Sector Cleave continues to validate every face before publication.

Splitting an adjoined Surface must produce two complete reciprocal portal pairs. Mirror splitting or reconnection
failure rejects the operation and restores the affected sectors; a partial match must never become a successful cut
with missing adjoins. The same repair failure propagates through Sector Cleave's candidate transaction.

`levelCleaveSector` in particular:
1. marks vertices against the plane
2. rejects non-splitting planes
3. cleaves every surface in the sector
4. gathers plane edges into a loop
5. creates a new sector and partitions surfaces/vertices
6. creates cap surfaces and adjoins them
7. re-sectorizes affected things
8. keeps the body containing the pre-cut Sector center in the original `id`/`num` slot; if the plane classifier placed
   that body in the appended slot, it exchanges the two bodies and remaps adjoins, Thing containment, and baked-light
   containment. Authored Sector COG references deliberately keep the original number, so they continue to target the
   retained main body. The appended slot is therefore the cut-off/new Sector selected by the viewport.

`levelCleaveSectorByPlanes` is the transactional core used by Sector **Multi-cut**:
1. validates the selected convex solid and directly affected adjoin partners
2. copies the Level so a later invalid line cannot leave a partial edit
3. normalizes each plane and classifies current descendants before cleaving, skipping pieces the plane does not cross
4. applies a plane only to the descendants that existed at the start of that stage
5. validates affected topology/adjoins, fixed source profiles, certified cap boundaries and every new cutting half-space
6. commits the copy only when every stage succeeds; otherwise it reports the failing plane/Sector/surface and leaves the source Level unchanged

With **Bounded** disabled, independent Multi-cut lines remain an arrangement of infinite cleave planes and subdivide every current descendant. An
explicitly closed Chain instead supplies `SectorCleaveBatchOptions::closedShape` plus an interior sample: each later
plane clips only the current interior descendant. A four-sided inset therefore produces the interior plus four exterior
pieces rather than a nine-cell global grid, while the same planarity/convexity/adjoin validation still guards the commit.

The viewport defaults **Bounded** on for open Surface and Sector arrangements. Each descendant remembers its side
only for earlier planes that actually split it. Before a later plane can split that descendant, its finite world-space
stroke is clipped against those inherited half-spaces. A positive-length interval admits the descendant; endpoint-only
contact does not. A stroke lying along an earlier boundary can reach both sides. The gate ignores original source
extents and camera/construction directions: the admitted descendant is still cleaved by the complete infinite plane.
The first cut is therefore unchanged, a later stroke confined to one earlier child cuts only that child, and a stroke
crossing an earlier boundary can cut both children. Closed chains retain the existing interior-descendant path.
Disabling Bounded restores the global arrangement without changing staged order or line numbers.

Viewport authoring behavior:
- The original one-line Sector/Surface cleave stays available. **Multi-cut** retains each completed plane until
  **Apply** or **Cancel**, permits an existing plane or endpoint to be selected and edited, and lets `Delete` remove only
  the selected Multi-cut lines. (`stagedCleave*` remains an internal compatibility name, not a user-facing mode.)
  Sector and Surface Face own independent workspaces, including draft/history/cursor, captured targets, order,
  selection, Chain origin, and all Cleave options. The active family occupies the hot input/render members; a move-swapped
  dormant peer is not copied on pointer or paint paths. Fresh/reset workspaces independently use application preference
  defaults rather than another family's customized options. The overlay publishes core state with signals blocked before
  deriving visibility and eligibility, preserving pending numeric text when its effective value is unchanged.
  The **Clear** action removes only the current family's lines as an undoable draft edit. Leaving Multi-cut, switching the
  owning tool/edit family, or entering Preview suspends the settled draft, local history, order, line selection, and options;
  an active pointer gesture rolls back to its baseline. The dormant draft neither renders nor captures input or Undo.
  Matching enabled Cleave ownership in Editor with Multi-cut on resumes it without replacing document selection.
  A temporary Outliner target-selection mismatch also preserves the captured draft and history; operation eligibility
  can remain disabled until the original selection returns. Explicit cancellation, document replacement, successful
  Apply, or invalid captured source references clear the draft.
- `GlView` owns a bounded, line-only Multi-cut Undo/Redo timeline. Each completed changed addition, endpoint edit,
  Move, Rotate, Scale, join, detach, removal, rank change, Clear, or chosen Chain-origin change records one state.
  Pointer samples, camera navigation, line selection, and overlay options do not create entries. A new edit discards
  local Redo; cancelled and no-op gestures preserve the completed timeline and do not trim it. Snapshots contain physical
  lines, construction/authored-camera frames and links, target context, Chain origin, Apply-order permutation, and
  restoration line selection. They contain no Level snapshots or document history records.
  Ctrl-held Rotate/Scale wheel input is one cumulative gesture, using one admitted rollback baseline and prepared
  sparse endpoint workset for all samples. The HUD measures its total angle/factor against that baseline. Control
  release or tool/mode/family/focus ownership loss completes one command; Escape and exact identity restore the
  original draft and preserve its Redo branch. Capture during the live gesture returns its settled baseline and
  completed timeline without recording or ending the gesture.
  Authored draft/history admission allows 64 retained snapshots, including baseline/current, and 16 MiB of estimated
  owned payload. It charges actual container capacities, live lines/context/order/selection, retained cursor data,
  rollback baselines, and admitted edit/restore candidate worksets before deep copies or reserves. Shape-planning and
  rendering caches are separate from this admission budget. Pointer samples do not copy the Level or record snapshots.
  Invalid release or target mismatch restores the exact gesture baseline without changing local Redo. A legacy
  current-only draft can remain retained without an optional initial Undo baseline when that extra clone cannot fit,
  provided the source, live workspace, and prepared current draft fit the aggregate 16 MiB workset. Editing, Clear, or
  Apply is refused before mutation when required rollback or memento storage cannot fit; an over-budget actual workset
  is never admitted or partially published.
  In viewport/Level command context, configured Undo/Redo is local and exclusive while the session is active, even
  at a timeline end. Focused resource/COG histories and native editable-text history retain priority. Dormancy releases
  local command ownership while retaining the timeline. Compatible same-Level coordinate, lighting, and UV notifications
  preserve authoritative planes/history; invalid captured source references clear them with an explanatory toast.
  Clear is undoable and failed Apply preserves the draft/history.
  Native EDIT retains both settled family drafts, options, and timelines/cursors. Named `currentFamily` selects the
  hot slot and optional `peerWorkspace` carries the opposite family; absent legacy fields preserve the old current
  draft and initialize a fresh independent peer from preferences. Legacy current-only records start a baseline when
  restoration admission permits; otherwise the admitted current draft is retained without that optional baseline.
  Active gestures are normalized to a settled boundary; invalid timelines reject the workspace atomically.
- `MainWindow` records an Apply workspace memento with the corresponding Level transaction. It retains the applied
  family's exact pre-Apply draft/history/options and line selection, plus document selection/active items and edit modes.
  Undo restores that family after mesh notifications and owning QAction/overlay synchronization; it never overwrites a
  peer workspace edited after Apply or Undo. Known Apply Redo clears only the applied family before geometry publication.
  Paired document Redo takes priority while that family's Apply-affecting authored revision is unchanged, even if its local
  timeline has a Redo branch. Applied line/order/Bounded changes and its successful local history movement revoke this
  priority. Family switches, peer edits, camera/hover/selection, guide settings, and future shape or Angle choices preserve
  it. An active peer's local Redo takes priority when available; otherwise the known Apply can redo without clearing or
  changing the compatible active peer. Failed Apply preserves the current draft/history; a certified non-crossing Sector
  batch adds no Level checkpoint or workspace memento and keeps the pending arrangement.
  The optional EDIT `workspaceHistory` sidecar binds Apply mementos and Layer visual transitions through nearest-first
  Undo/Redo positions and exact adjacent required-Level SHA-256 digests from the checked HIST stream. Cleave targets are
  validated against the pre-Apply Level, including all local snapshots; Layer rows are checked against each adjacent
  Level. Checked import maps positions to fresh runtime RecordIDs. Branch/trim removes unreachable attachments.
  This separate sidecar has a 64-transition/16 MiB aggregate owned-storage cap, including nested timelines, Layer vectors,
  and import/restore worksets. Malformed, missing, or degraded HIST drops the optional sidecar with a warning while
  retaining the current Level, current EDIT draft, and other workspace fields.
- Physical vector indices retain line identity, joins, origin references, and accent colors. A separate `applyOrder`
  permutation owns execution ranks and Apply order. Clicking a number opens an inline integer editor; Enter or focus
  loss commits a rank clamped to `1..N`, including overflow-safe input, and Escape cancels it. The other ranks shift
  around that physical line in one local Undo step. Camera, endpoints, links, origin, and colors do not move with ranks.
- Plain click replaces draft selection; Shift-click adds and Ctrl-click removes. Pressing a selected line keeps the
  group during a drag; no-drag release narrows to that line. Alt-click on a shared joint cycles incident physical lines
  in stable index order. Alt-drag on empty space adds intersecting lines by marquee; Alt+Ctrl removes them. Delete removes
  the selected batch as one step. Move is the default Cleave-local mode; Rotate and Scale require a common authored plane.
  Their pivot is the centroid of distinct selected endpoints, Rotate uses signed cursor polar angle and Angle step,
  and Scale uses a positive uniform factor. Selected-endpoint worksets are prepared once; exact shared-handle tuples
  carry an unselected neighbor's attached endpoint once while its opposite end stays fixed. Sparse pointer previews do
  not clone the Level. Escape restores the exact draft/selection baseline. G/R/S default to local Move/Rotate/Scale
  while active Multi-cut owns viewport input. Editable `cleave.transform.move/rotate/scale` keymap actions publish cached
  bindings to GlView; custom and cleared mappings consume conflicting global tool/grid commands in that context. UV
  pointer ownership, native text, resource/COG editors, and Fly retain priority. Fixed Cleave mouse gestures are searchable
  read-only Preferences rows because `QKeySequence` does not represent the wheel. Outside Cleave, G toggles the grid and
  global transform bindings retain their normal contexts. Losing common-plane/selection eligibility returns an active
  Rotate/Scale mode to Move. Ctrl-only wheel rotates by Angle step (Free uses 5 degrees per detent) or scales selected
  lines by `1.1` per detent, accumulating one local history step until Control release or ownership transfer.
  Angle deltas use 120 units per detent and pixel deltas use 80; scaling supports
  fractional/inverse deltas, and configured rotation steps round fractional input to complete steps. Scale remains
  positive with a per-event factor in `0.05..1000`. Active pointer gestures consume the
  relevant wheel without nesting another edit. The HUD formats rotation and scale with exactly two decimals, using
  font metrics for its padded badge below the projected prepared pivot. The complete badge is clamped to an eight-pixel
  viewport inset; invalid projections use viewport center and narrow viewports elide text. The HUD retains the cumulative
  angle/factor for the whole active gesture, including idle time with Ctrl held. Each new HUD sample rearms one
  single-shot idle deadline selected by its explicit Rotate/Scale banner kind (400 ms by default). Expiry hides presentation even during a held wheel/pointer
  gesture, preserving its baseline, cumulative transform, prepared workset, and history. Further input shows the
  cumulative result and rearms expiry. Control release immediately clears the readout; a reset cancels its deadline.
  Completion preserves the remaining idle deadline instead of extending it. Hiding changes presentation only.
  Fade edges last at most 40 ms each, capped at one tenth of the configured hide time, inside the idle deadline.
  Continued input preserves a fade-in already in progress; input during fade-out restores opacity without resetting
  the transform baseline. A presentation timer wakes during transitions and sleeps through the opaque interval.
  Cached application banner preferences choose each kind's duration and RGBA palette; no settings read or mesh rebuild
  occurs during input or rendering. Toast snapshots carry their selected colors, preserving configured alpha during
  fading; transparent borders retain the previous borderless style. General/Error messages retain their caller
  duration unless an explicit preference overrides it.
  Outside a live Cleave wheel session, other
  wheel modifiers retain camera routing, including RMB fly-speed adjustment, except the armed shape's explicit
  Ctrl+Shift rotation gesture. While Control still owns a live session, changed chords are consumed instead of zooming.
- Add Shape offers centered Rectangle, N-Gon, Filled arch, or Star plans as ordinary bounded strokes.
  Legacy Square remains a readable kind but normalizes live to linked Rectangle with width authoritative. Fresh
  dimensions are 1 displayed unit, or `0.1` Bare; physical dimensions remain `0.005..10000` world units with `0.1`
  fallback for nonfinite input. Polygon/arch segments and Star points are `3..64`. N-Gon always links dimensions,
  and N-Gon width is its circumcircle diameter. Other shapes honor the Link choice. Filled arch
  has straight jambs and a half-ellipse cap. Star uses inner-valley radius ratio `0.45`, with division strokes before
  its perimeter. Its concave silhouette is partitioned into convex cells; actual bounded cuts can create extra fragments
  beyond the helper's central polygon/tip plan. Each resulting face is extruded separately; the whole concave outline
  cannot be one convex prism. The four shape numeric controls accept hovered wheel edits without changing the shared
  property scrub policy. Ctrl-only wheel over the viewport scales armed shape dimensions by `1.1` per detent using a
  uniform clamp that preserves their aspect. Ctrl+Shift wheel rotates an armed shape by configured Angle step, rounding
  fractional detents to complete steps; Free uses 5 degrees per detent. Rotation changes only the placement frame and
  preview, preserving the cached 2D plan, physical dimensions, camera, and settled draft/history. The optional named
  `shape.rotationDegrees` field defaults to zero; typed setters normalize finite values to `[-180,180)` and use zero for
  nonfinite input, while the wire parser rejects a present nonfinite or nonnumeric value. Rotation is one rigid axis
  transform in the construction plane, with snapping applied to the anchor rather than separately to each perimeter
  endpoint. Sector placement defaults to a picked surface frame; its default-off
  Camera plane option intersects the pointer ray with a camera-facing plane through the orbit target, using camera
  right/up axes instead of a wall pick. Grid snapping moves the anchor on that plane; Shift may anchor it to geometry
  while retaining its camera axes. Surface placement always uses the captured face and hides the
  camera option. During a live shape wheel session Escape restores its option baseline and keeps placement armed;
  otherwise Escape disarms shape placement without discarding authored cuts. Capture keeps current future-placement
  option values without retaining a scalar wheel accumulator or adding a line-history command.
- The first valid Multi-cut press captures the target context. Every Sector line immediately becomes an authoritative
  world-space plane with its own construction frame; camera orbit, zoom, projection, and viewport-size changes do not
  alter it or change the planes submitted by **Apply**. The visible filled polygon is the infinite plane intersected
  with each target Sector. Dragging that plane/its authored line translates it; dragging either endpoint rotates and
  deforms it inside the stored world frame. A render-only copy refreshes the edited plane and any linked neighbor on
  every pointer sample, so intersection fills/edges follow the drag before release; new Sector planes also preview
  during their initial drag. Surface Face lines use the same world-authoritative endpoint frame while
  the original source face and complete target Surface set remain locked.
- With **Bounded** on, Sector lines retain their finite authored endpoints during new-stroke drawing, endpoint edits,
  and group translation previews/releases. With it off, drawing or editing an independent Sector plane places its
  visible line handle on the intersection-polygon edge nearest its authored camera; an unlinked first **Chain** line
  follows the same rule. Toggling Bounded alone does not move existing endpoints. The stored world-space plane remains
  camera-independent in both modes. Linked or coordinate-shared endpoints keep their staged arrangement positions and
  same-color leaders, preserving neighboring planes and open/closed-chain Apply semantics.
- Every completed line also retains the camera from which it was authored. Clicking the line body without dragging
  restores that exact orbit, projection type, horizontal FOV, orthographic scale, near/far planes, and clip-enabled flag.
  The line-body action is armed independently of the construction-plane ray intersection, so it still works when the
  plane is edge-on to the current view. Pointer movement within the two-pixel click tolerance restores the pre-press
  line baselines instead of committing a tiny translation; larger movement remains the existing plane drag.
- Each Multi-cut line receives one of six stable accent colors shared by its authored line and every target-Sector
  plane intersection. A dotted same-color leader connects the line midpoint to each intersection centroid unless they
  already overlap; the selected line/plane remains yellow. This is presentation-only and does not change plane order.
- **Chain** resolves its next start from an optional line-index/endpoint reference. A plain endpoint click within the
  existing two-pixel motion tolerance selects that reference and restores the press baseline; it does not reshape the
  plane. A larger drag edits the endpoint, and modified gestures retain their existing behavior. A ring marks the resolved
  source while Chain is enabled. Disabling Chain hides the marker but retains the reference across independent cuts;
  a successful chained cut advances it to the new line's second endpoint. An absent reference falls back to the last
  staged line's second endpoint. The chosen endpoint's opposite endpoint supplies the incoming angle frame.
  Choosing a source never reorders the physical vector: numbers and Apply follow the separate execution permutation.
  Canonical reciprocal
  links are recorded only for last-line-second to new-line-first continuation. Earlier, reversed, or occupied sources
  instead form coordinate-shared handles and ordinary ordered open Multi-cut branches. Shift-joined arbitrary endpoints
  remain shared during endpoint edits and whole-line movement, including independent lines authored while Chain is off.
  The joined neighboring endpoints follow the picked line without reordering the physical vector or its execution ranks.
  Moving a shared endpoint updates its coincident handles. Alt-dragging one endpoint
  clears that endpoint's reciprocal chain link first, so only the picked line endpoint moves. Endpoints snapped to the
  same position form one normal manipulation handle even across non-adjacent lines. Snapping the ripped endpoint back
  onto its authored adjacent endpoint without Alt also recreates both sides of the explicit chain link, so subsequent
  grabs move the shared endpoint as one.
  Canonical tail-second continuation released near the first endpoint closes a chain, after which Apply uses the optimized interior-descendant path. The
  normal click-to-apply Surface Edge cleave remains immediate rather than entering the Multi-cut workflow.
  Deleting lines remaps surviving physical references, selection, and execution ranks, or clears a deleted source to use
  the fallback. Physical survivors keep their relative vector order. Clear, explicit cancellation, document replacement,
  invalid captured references, and successful Apply clear the source with the lines. Tool dormancy retains it.
  Native Save, autosave, and recovery retain it in the optional named Cleave `chainOrigin` field.
  Completed staged edits and source changes notify recovery after the interaction state is settled. Pointer samples
  keep their existing preview behavior and do not schedule a recovery snapshot for every movement.
- Closed-chain Apply requires a finite, simple, strictly convex loop with unique non-degenerate vertices, no
  self-intersections, valid plane normals, and one consistent interior half-space. A closed chain cannot be mixed with
  independent Multi-cut planes; apply it separately before authoring another arrangement.
- Holding **Shift** snaps to target vertices, edge midpoints, surface centers, and existing Multi-cut endpoints in both
  Sector and Surface Face mode. Start points and edited endpoints use an 18-pixel geometry radius; endpoint handles
  themselves can be grabbed within 10 pixels so short line bodies remain independently selectable.
  Small staged endpoints receive an additional 12-pixel acquisition allowance in both paths and take priority over
  broad geometry markers inside that bounded radius; actual screen distance still chooses within each target class.
  While Shift is held, every vertex, unique edge midpoint, and surface center on all selected target Sectors/Surfaces
  remains visible, including before drawing begins; releasing Shift or leaving the applicable tool/mode hides them
  immediately. The nearest candidate is emphasized and remains the actual snap target, including when Shift changes
  while the pointer is stationary. Vertices use a red diamond, surface centers a red cross, edge midpoints a short red
  tick orthogonal to the source edge, and staged endpoints a red ring. The status-bar hint describes these cleave
  candidates rather than selection.
- Dragging a completed Multi-cut line body follows the Cleave-local transform mode. For Move, Alt is an independent
  topology modifier: a real drag detaches the picked endpoints; a click keeps their links. With no higher-priority
  modifier, shared Grid Snap snaps the first moved endpoint to the interaction-plane lattice, and Ctrl reverses that
  choice. Without grid snapping, Move uses the free pointer-plane translation. Shift alone snaps either moved endpoint
  to the closest geometry/staged target, excluding the selected moving handles, then applies one rigid translation to
  the group. Ctrl+Shift retains the original plane-normal constrained offset. Rigid translation preserves the picked
  lines' direction/length and carries their joined neighboring endpoints unless detached.
  Endpoint edits use the same shared Grid Snap/Ctrl inversion. Shift prefers geometry/staged endpoints; Ctrl+Shift
  explicitly requests the angle constraint. A plain endpoint edit uses configured Angle step when grid snapping is off.
  Candidate discovery excludes the edited original handles so a coincident endpoint cannot pull the drag back to its
  start. Snapping onto an endpoint creates its shared handle and restores compatible reciprocal chain links. Sector
  endpoint edits keep the chosen position instead of reattaching it to a camera-facing polygon edge on release.
- Cleave grid snaps for anchors, live points, endpoint edits, Move, and Add Shape resolve the active grid frame used by
  rendering: authored origin, orthonormal axes, configured minor spacing, and adaptive spacing. The lattice is projected
  onto the interaction plane so the point stays on its face/construction plane. Parallel planes retain authored phase
  exactly, including the F3 Grid-to-View then F2 View-to-Grid workflow. Shift geometry snapping takes priority for new
  points and shape placement; combined-modifier endpoint/Move exceptions are described above.
- The compact angle-step control accepts positive finite steps up to `360.0` degrees, including steps smaller than `0.1`. Its
  hover and pressed steppers remain compact while the complete `360.0` value stays readable and manually editable. Zero is
  shown as **Free** and disables angle snapping. New Sector/Surface Face lines snap relative to their stored reference or
  chosen source segment by default when geometry/grid snapping does not take priority. Plain endpoint edits use it
  while grid snapping is off, and Ctrl+Shift explicitly requests it. The immediate
  single-cut path is unchanged. The value persists as `cleave/angle_step_degrees`.
- Compatible same-Level coordinate, light, UV, and direct-append changes retain the captured context and
  world-authoritative Multi-cut planes in both families. Direct Sector/Surface Delete, Join, and Merge invalidate both
  draft payloads explicitly because they can remove or reindex targets. History swaps carrying both Structure and LevelGeometry conservatively
  invalidate it without a typed identity/remap contract; checked Apply workspace-memento restoration takes precedence
  after publication. Source-reference validation also rejects deleted or out-of-bounds targets explicitly. Tool changes
  and Game Preview suspend settled work and roll back unfinished gestures; New/Open creates a new document lifetime
  and clears it.
- Cleave overlay preferences independently control angle guides, length labels, and editable execution ranks. Angles and
  lengths default on, line numbers default off; each toggle persists in application preferences. The **Units** control
  offers metres, IJU, or Bare for IJIM and metres, JKU, or Bare for JKDF2/MOTS, defaulting to metres. An unsupported
  saved project unit normalizes to metres without resizing physical shape dimensions. Units do not hide labels; the
  Lengths toggle owns visibility. Angle guides identify exact shared endpoint tuples with exactly two finite non-degenerate
  incident segments and report their unsigned smaller 3D angle between outward rays, `0..180` degrees, once. A rectangle therefore reports four 90-degree corners
  independently of line direction, authored chain links, or camera. Branches with three or more incident segments use the
  deterministic existing authored-chain/reference bearing instead of choosing an arbitrary pair. Isolated lines retain
  their authored construction-plane bearing. Sorted `2N` endpoint guides and endpoint masks are cached by authored and
  transient-geometry revisions; camera changes project the cache without rebuilding it. A separate 16 MiB presentation
  budget charges retained capacities and scratch/output worksets before allocation. Exhaustion falls back to authored
  bearings until geometry changes and never changes draft/history admission, Undo, or Apply. These conversions also
  format Add Shape dimensions while the shape plan retains world units; unit changes preserve authored size. For example, bare
  `0.1` is shown as `1 m`, `1 JKU`,
  or `1 IJU`. Legacy EDIT unit value `4` is normalized to Meters with length labels off; explicitly saved angle and
  line-number visibility remains unchanged. An obsolete shared preference with measurements off retains the last real
  unit and initializes Lengths off, then is canonicalized so later visibility changes can persist independently.
- Applying Multi-cut Sector lines routes the complete plane list through `levelCleaveSectorByPlanes` under one document
  edit, so a failed later plane rolls back the whole operation and the validator protects planar faces, convex faces,
  convex Sector volumes, topology, and adjoins. Multi-cut Surface Face lines are likewise applied as one document edit;
  closed chains clip only their interior face descendant, and every affected face/Sector/adjoin is validated before a
  failed line or invalid result can publish a partial edit. If the app classifies a selected Surface as crossing a
  plane but the core cleave rejects it, the whole Surface batch fails and rolls back; that rejection cannot be treated
  as a boundary no-op while other selected faces or later lines continue.
- For open Surface Multi-cut arrangements, the app retains planes that successfully split a descendant. A later line
  that misses every current descendant is redundant only when its supporting plane is equivalent to one of those
  successful planes. Plane-only requests use the strict proof:
  normals are compared up to sign using component-wise cross-product roundoff bounds scaled to the products being
  compared. The offset proof uses a compensated point-to-point displacement dot product and accepts only an exactly
  zero translated signed displacement; ambiguous overflow or underflow is rejected. There is no absolute angular or
  world-space distance floor, so a provable tilt or any nonzero translated offset remains a distinct line and follows
  ordinary non-split rejection. Skipping does not reorder authored lines, so a genuine failure still reports its original
  execution rank as a 1-based line number.
  Native finite-stroke requests additionally recognize a straight construction-roundoff continuation sharing an exact
  endpoint with a previously splitting stroke. This separate proof requires finite nonzero segments, own-plane/stroke
  consistency, and component-wise direction and normal bounds derived from represented coordinates. It has no
  absolute world/angle tolerance and conservatively rejects unresolved directions or underflow ambiguity. Both
  Bounded and unbounded Apply retain the original endpoints for this check. Distinct misses and actual core crossing
  failures keep their atomic rejection semantics.
- After any successful Surface cleave (immediate or Multi-cut), only the newly appended output face(s) are selected;
  the source face remains unselected and the newest appended face is active. This matches the Sector behavior and makes
  a follow-up Delete operate on the cut-off output rather than the retained source fragment.
- After either an immediate Sector cleave or Sector Multi-cut succeeds, only the appended Sector pieces are selected;
  the newest piece is active and the original Sector is left unselected. This makes deleting the cut-off pieces an
  immediate follow-up operation.
- A single Sector split keeps the body containing the source Sector's pre-cut center in the original vector slot and
  exchanges the two completed bodies when plane sign would put that body in the appended slot. The original `num`/ID
  and authored COG Sector references therefore stay with the retained room; adjoins and contained Things follow their
  physical body, while the small cut-off fragment receives the new number selected above. Closed-chain Multi-cut
  resolves its next interior descendant geometrically after this exchange rather than assuming a fixed plane sign.

## Level entity-ID capacity

`Level::curId` is the next ID to allocate, and `INT32_MAX` is the exhausted sentinel. The final assignable value is
`INT32_MAX - 1`; a successful allocation advances `curId` to the sentinel. `Level::canAllocateIds(count)` computes the
remaining capacity in constant time using a widened integer, avoiding a loop or mutation just to discover whether a
batch fits. `Level::newId()` checks one ID and throws the checked exhaustion failure without advancing the sentinel.

Factories and batch authoring paths preflight their known ID count before publishing anything: Level factories,
clipboard import/paste, Thing and brush placement, COG creation/duplication, and editor light paths. Cleave and Join
check their entry conditions, then allocate IDs incrementally through checked factories while preparing a private
candidate; they do not reserve the complete multi-split count before copying the Level. Exact remaining capacity is
admitted. If a later split exhausts that capacity, the private transaction is discarded and the source Level, selection,
and existing Undo/Redo branch remain unchanged. The low-level multi-target Join API retains its separate partial-success
policy described below; the application preview discards its whole private candidate on exhaustion.

## Join / connect (surfaces / sectors)

Primary implementation: `src/core/src/level/cleave.cpp`

- The user-facing UI uses **Adjoin** / **Unadjoin** for surface adjoin operations, **Connect surfaces** for directly
  connecting two selected surfaces, and **Join sectors** for sector portals. The core API keeps the legacy
  `levelConnect*` and `levelUnadjoin*` names because those functions implement the old editor operations.
- `levelConnectSurfaces` cleaves as needed, verifies overlap, and creates matching adjoins.
- `levelConnectSectors` finds overlapping candidate surfaces between two selected sectors and connects them.
- `levelConnectSectorsKnife` uses one selected sector as a knife to cut the target sectors before connecting them.
- The Sector **Join sectors** viewport tool previews the generated portal/cut result for two or more selected sectors.
  Knife descendant processing stays bound to the selected knife/target family. Every initially unselected original
  Sector remains outside all descendant families and their preview, including corner-touching rooms; legitimate thin
  generated descendants remain eligible. Before copying/preparing a preview, Join requires unique Sector IDs and a
  `curId` greater than every allocated owner ID and below the exhausted sentinel. These entry checks do not reserve
  the complete candidate's ID count. Splits allocate checked IDs incrementally in the private preview Level and may
  consume exact remaining capacity through `INT32_MAX - 1`; exhaustion at any later stage discards the complete
  preview without changing the source Level.
- Sector Join has one important option, **Keep overlap pieces**.
- With **Keep overlap pieces** on, the join preserves sector pieces created inside the target by the knife cut. This is
  the legacy "keep target pieces" behavior.
- With **Keep overlap pieces** off, the knife acts as a cutter only: it cuts holes/adjoins into the target sectors and
  removes the knife overlap that lies inside those targets.
- The cutter-only path is intentionally conservative with target sectors: it should only split a target where required
  to keep the sector convex and to create the portal hole. It should not leave solid knife remnants inside targets.
- After knife cutting and cleanup, the app/core re-run candidate checks so back-to-back surfaces produced by
  rotated/complex intersections become adjoins only when their full boundaries coincide within arithmetic roundoff.
  Candidate proximity never admits a real plane gap.

Individual Surface/Sector Join, Knife and auto-adjoin protect their complete candidate/preflight paths against
allocation failure. Internal checked operations preserve the legacy Keep overlap pieces relinking policy while allowing
exceptions to reach the candidate owner. The multi-target candidate API intentionally keeps connections already
committed before a later failure; its return value states whether any connection was committed.

## Merge (surfaces / sectors / vertices)

Primary implementation: `src/core/src/level/merge.cpp`

Geometry admission uses local-coordinate double calculations and expression-scaled roundoff allowances. Plane tests
measure distance; convex turns use area units and account for stored-coordinate rounding. Half-space checks propagate
coordinate uncertainty from the complete face into its area-derived normal and evaluate that uncertainty in the actual
query direction, preserving cancellation along tangents of thin or distant faces. Unresolved or nonfinite area data
rejects the operation. These arithmetic bounds are separate from snapping and welding distances. A proximity search never
authorizes a portal: both complete 3D boundary loops must coincide, including redundant collinear corners. Volumetric
merge inputs and outputs must have closed, oppositely directed edge incidence after splitting collinear edge runs.
Parallel surface-only low-level fixtures retain their existing contract.

Closed source solids may retain bounded original coordinate rounding. Merge records each admitted input face and its
support residual before mutation. Only an unchanged ordered face with unchanged original coordinates can use that
record; vertices from another sector must satisfy that face's half-space within arithmetic error. Newly combined
polygons and their mirrored portals remain strictly planar and convex. NaN or oppositely facing normal caches reject
atomically; an unset cache may be derived from the geometry. This permits removing a rounded internal portal or merging
elsewhere in a rounded sector without admitting a newly concave union.

Merge UV preservation selects the largest finite nonzero origin-fan triangle before solving affine gradients with
cross-product dual vectors. A merely nonzero triangle formed by nearly coincident leading corners can amplify stored
float rounding or lose an entire UV gradient; the wider basis avoids that defect without an arbitrary area cutoff.
Cross-product duals avoid subtraction of nearly equal Gram products on long, thin faces. Existing source-corner UV/RGBA
values, per-axis scales, and the document coordinate-space tag remain authoritative.

- `levelMergeSurfaces` first rejects non-coplanar/nonadjacent pairs, applies the topology change to a private Level,
  and publishes only when every changed face remains planar/convex, each affected sector remains convex, and portal
  adjoins remain reciprocal. Portal faces preflight and merge their mirrored faces atomically; the actual cleaned
  output polygon enforces the project vertex cap. Surface COG pointers are remapped by semantic face, not stale
  global number, and deleted donor references map to the survivor
- `levelMergeSelectedSurfaces` scans selected same-sector faces deterministically and repeatedly merges valid adjacent
  pairs to a fixed point. A newly merged polygon is reconsidered for chain merges; invalid and cross-sector pairs are
  skipped without preventing independent merges. Selection remapping and publication are one undo transaction
- `levelMergeSectors` chooses keep/delete sectors, removes internal faces, repairs references, and transactionally
  rejects non-convex or adjoin-unsafe output. `levelMergeSelectedSectors` applies the same checked operation to selected
  adjacent pairs until a fixed point, remapping surviving indices after every deletion and skipping invalid pairs.
  Donor Things move to the survivor; Sector/Surface COG pointers are rebuilt after every deletion, while references
  to removed internal portal faces are cleared
- `levelMergeVertices` collapses selected vertices into one vertex and rewrites referencing surfaces

## Transforms (including frames)

Primary implementation:
- [`transform.cpp`](../src/core/src/level/transform.cpp)
- [`connected_surface_uv.cpp`](../src/core/src/level/connected_surface_uv.cpp)
- [`gl_view_gizmos.cpp`](../src/app/src/render/gl_view_gizmos.cpp)
- [`viewport_grid_utils.hpp`](../src/app/src/render/grid/viewport_grid_utils.hpp)
- [`signals.cpp`](../src/app/src/main_window/viewport/signals.cpp)
- [`extrude.cpp`](../src/app/src/main_window/surface/extrude.cpp)
- [`surface_extrude_preview_utils.hpp`](../src/app/src/ui/surface/surface_extrude_preview_utils.hpp)

- viewport Scale applies to Sector, Surface, and Light owners. Ordinary Things and Frames do not expose viewport Scale;
  their supported viewport transforms remain Move and Rotate where applicable. Numeric texture scaling is a separate
  Surface operation.
- Thing Rotate, Scale, and axis/plane Mirror recompute containing-Sector ownership after changing positions. They
  retain a valid strict-interior owner, resolve boundary ties by lowest index, and assign `-1` outside every Sector.
  Multi-Thing batches reuse one lazily built containment index.
- Core geometry transforms prepare all allocating scratch state before changing authored or runtime Level data.
  The `Checked` recalculation, translation, rotation, scale, and Sector-flip APIs report preparation failure and leave
  the Level untouched; application commands use that status before publishing an edit. Sector duplication follows
  the same prepare-before-mutation rule. Legacy void transform wrappers retain their existing public signatures.
- Numeric and viewport Sector/Surface Scale use the same source-relative geometry admission after preparing the
  candidate and connected UV values. They retain admitted source rounding, reject new degeneracy, and publish no
  authored state or Undo entry on rejection. Valid small faces are not rejected by an unrelated absolute area cutoff.
- frame transforms parse the frame value, apply the delta, and serialize back via `TplValue::setFrame`
- Move, Rotate, and Scale capture one immutable connected-surface UV plan before geometry changes when **Preserve UVs**
  is enabled (the default). Complete rigid Move/Rotate faces retain their exact authored UVs. Partially deformed
  neighbors derive an affine U/V gradient from their original geometry and mapping, anchor at an unmoved corner, keep
  every unmoved corner and every baked-light RGBA value bit-exact, and add only the geometric U/V delta to each moved
  corner. This retains authored non-affine distortion, signed coordinates, and multi-tile offsets. Scale includes
  complete selected and reciprocal-adjoin faces to preserve texture density
- capture, preview evaluation, and commit operate directly in the Level's declared normalized or texture-pixel space;
  they require no MAT dimensions. Duplicate/stale vertex references, changed face topology, degenerate mappings, and
  non-finite results reject the complete operation before authored geometry or Undo history is published
- drag preview applies the same core evaluation only to transient CPU/VBO UV ranges. A shared double-precision
  transform descriptor uses the core commit math, including large fractional coordinates; the preview never mutates
  the Document, restores exact authored ranges on cancel, and is cleared before the candidate Level is committed as
  one Undo step
- legacy `scaleTextures` remains independent: the numeric uniform-Scale prompt can still adjust `uScale`/`vScale`
  metadata, while connected-surface preservation controls per-corner UV reprojection
- Flatten and transforms performed with **Preserve UVs** disabled retain the previous behavior of leaving per-corner
  UV coordinates unchanged. Existing Surface flags remain authored data and are not silently reclassified
- Connected-surface UV gradients use cross-product dual bases, avoiding squared-length cancellation and an extra
  absolute area cutoff for small valid faces. Face-normal degeneracy and finite-output checks still apply, identically
  in normalized and compatibility pixel space
- UV canvas snapshot comparison does not suppress changes with a display-space tolerance. The final document writer
  compares the rounded stored `float` coordinates, so representable small edits participate in preview and Undo/Redo
- Low-level `levelOffsetByNode` and `levelUnoffsetByNode` accept only complete finite ancestry ending at parent `-1`.
  Invalid parents, cycles, non-finite inputs, and arithmetic overflow leave all output coordinates unchanged; traversal
  is bounded by the model's node count
- select/translate/scale/rotate shortcuts are resolved through the keymap layer and shared across edit modes; text
  inputs are explicitly guarded so typing does not trigger viewport tool changes
- local/world transform orientation has its own shared shortcut binding
- transform-grid move controls are independently persisted and exposed as direct, visibly active icon-only buttons in
  the top-center Thing/Frame/Sector/Surface/Light **Move** options bars; the larger glyphs keep the bars compact and
  each button retains its complete behavior description as a hover tooltip:
  - **Snap moves to grid** is off by default and rounds the active target reference on the displayed minor-grid
    lattice; its rebindable hold inverter defaults to `Ctrl`; while active, the grid lattice remains authoritative
    over downstream geometry/Thing snap solvers
  - **Grid stick** is on by default, can be disabled, and applies to thing, frame, light, sector, and surface
    face/edge/vertex moves; its independent hold inverter defaults to `Shift+Alt`
- every Move snapping behavior owns an exact-match, rebindable hold inverter. Any configured alternative can trigger
  the inverter, but its complete modifier set must match exactly; extra held modifiers do not activate a shorter
  binding. An explicitly cleared binding disables that inversion. `Ctrl` coherently inverts the snap
  family: grid snapping plus sector **Snap align** or Thing-to-Thing **Snap**, as applicable. The remaining defaults
  are Thing **Surface stick** `Shift`, Thing **Align to surface** `Ctrl+Shift`, and Thing-to-Thing **Rotate to snap**
  `Ctrl+Alt`. Holding a binding changes the affected top-bar button visually and the drag result, then restores the
  saved state on release without persisting the temporary value; clearing a binding disables only that inversion
- contextual boolean tool options consistently use active toggle buttons. Thing/frame **Surface stick** captures rendered
  bounds against solid geometry; **Align to surface** can additionally rotate a freely moved Thing's local up axis
  (sprite forward axis) to the contact normal. Thing-to-Thing **Snap**/**Rotate**, sector **Snap align**, and sector
  Join's **Keep overlap pieces** remain distinct persisted behaviors. Frame Add/Move exposes the same four visual
  contact and Thing-alignment choices, resolving the selected frame as the owning Thing's virtual pose
- Thing Move builds a `ThingMoveSnapCache` once per real transform gesture and clears it when the drag state resets.
  It keeps selected indices, poses, and document-pivot-relative double bounds, while resolving local boxes, model-only
  3DO bounds, rendered support points, and connector metadata lazily by visual/template identity. Surface Stick uses
  indexed solid-surface queries and prefers sprite or model support points before falling back to the authored origin;
  generic local boxes do not become rendered supports. Ordinary Thing-to-Thing broad culling uses the cached bounds,
  while connector-capable rail/track primaries bypass that cull so connector matching retains its full candidate set.
- Thing-to-Thing broad-phase cutoffs add a five-radius probe envelope and the cursor-to-pivot offset to the moving and
  anchor bounds. This covers the preferred 3DO face solver's weighted vertical score and lateral/out-of-plane
  allowances. The outer sphere is yaw-invariant and remains valid during Rotate; inner model/box culls are skipped when
  rotation is active, while connector-capable primaries retain their connector-specific candidate handling. A non-finite
  cursor offset disables this cull.
- Surface **Nearby Geometry Snap** is a Move-only solver. Vertex mode ranks
  finite free-move candidates by actual correction distance and never falls
  back to a whole-surface plane when the active Vertex cannot be resolved.
  Edge mode aligns the midpoint to a similarly oriented Edge and, for free
  movement only, can finish the smallest direction correction around that
  midpoint, independent of endpoint order. Face mode accepts either parallel
  normal sign through a full quarter-turn and chooses the one requiring the
  least rotation. This preserves the normal opposed/back-to-back adjoining
  result while avoiding a 180-degree flip when the moving Face is already
  closer to same-facing. The persisted
  **Rotate to Nearby** Move option gates orientation for both Edge and Face
  without adding a modifier binding; Nearby Snap still owns target acquisition
  and translation. Preview and commit use the same rotate-then-translate order.
  Axis Edge moves do not rotate. Face orientation applies during both free and
  axis-handle moves; constrained Face translations are rejected when their
  off-axis residual exceeds capture tolerance
- a move drag freezes the active rendered grid origin, axes, plane normal, and effective minor spacing at drag start;
  fixed, rotated, view-aligned/top-down, and adaptive-spacing grids therefore use the same lattice for rendering and
  transform math during that drag. When **Keep rotated grid visible** activates its camera-side proxy, adaptive cell
  size is measured on that displayed plane for both rendering and snapping, while the transform frame retains the
  authored grid origin and snap plane
- Surface Extrude exposes the shared **Grid Snap** state directly in its live options row. When enabled, Length and
  Expand are signed displacement scalars rounded to the effective minor spacing of that same active grid; Expand
  chooses the closest grid cell above the source face's negative-collapse bound. When disabled, typed numeric values
  remain unquantized and retain the authored double independently of displayed decimals. The negative Expand limit
  follows the core's strict collapse bound by selecting the next representable value above it; the UI does not impose
  an absolute epsilon or millimeter lattice
- authored Move translations and Rotate/Scale pivots use document doubles. Nearby geometry and Thing snap solves
  subtract a document pivot before forming float orientation/display values; accepted corrections retain their double
  translation and pivot through connected-UV preview and commit. Camera/GPU matrices and displayed grid frames remain
  float, so this does not promise unlimited far-origin visual precision
- accepted nonzero snap rotations use the same representable angle during contact solving, UV preview, and commit;
  an arbitrary small-angle publication cutoff must not discard part of an acquired transform. Ordinary model snaps
  retain their yaw-only orientation contract; differently pitched or rolled anchors use posed bounds for contact
  instead of scoring an orientation the commit cannot represent
- free-move lattice snapping corrects only the two in-plane coordinates, preserving the active reference's signed
  distance from the grid plane; axis-handle moves remain constrained to their handle
- grid-plane sticking keeps support points, signed distances and clutch contact/release offsets in double precision.
  It uses the moved Thing/frame/light reference point or geometry support points so the first point on the
  current approach side contacts the plane; room sectors that already span the grid remain eligible because vertices
  on the opposite side do not participate in that direction's contact search
- while held, lateral motion remains free and a normal-direction hysteresis band prevents immediate fall-through;
  continued motion releases through a continuity offset, then latches the release for the rest of the drag so the
  geometry cannot be captured back onto the plane
- surface vertex commits resolve and deduplicate the complete selected vertex set, so a multi-vertex preview and its
  committed edit move the same vertices
- persisted snapping tolerances are distinct by algorithm: Vertex and Edge
  use their exact configured capture radius; Face and Sector use separate
  broad/exact search radii; Thing snapping uses a configured base radius with
  proportional adaptive probes for placement, generic connectors, and paired
  rails; Surface/Wall Stick has its own adaptive maximum; and Grid-plane Stick
  uses its exact configured capture band. One global automatic-rotation limit
  caps Thing, surface-alignment, Face, Sector, and Edge corrections, while the
  conservative built-in Edge and Sector angle caps still apply

## Viewport entity clipboard

Primary implementation:
- [clipboard.hpp](../src/core/include/sed/level/clipboard.hpp)
- [clipboard.cpp](../src/core/src/level/clipboard.cpp)
- [clipboard.cpp](../src/app/src/main_window/clipboard.cpp)
- [SED V1 binary contract](formats/sed_binary_format.md)
- [level_clipboard_tests.cpp](../src/core/tests/level/level_clipboard_tests.cpp)
- [main_window_event_filter_tests.cpp](../src/app/tests/main_window_event_filter_tests.cpp)

Copy first filters the current entity-family selection to sorted, unique, valid indices and chooses the active item as
the placement anchor (falling back to the last valid selected index). The payload owns complete entity values plus
stable layer names. Sector adjoins are retained only when both endpoints are in the copied set and their Sector index
is rewritten into payload-local space; links to unselected geometry are cleared and their copied side is restored as
a solid boundary rather than an invisible portal to nowhere.

The anchor is the calculated center of the primary Sector, the primary Thing's position, or the primary baked light's
position. If the requested primary index is not among the filtered selection, the greatest remaining source index is
used. This anchor moves as one group reference; it is not serialized into every entity body.

The three payload variants match Level ownership:

- `SectorClipboardPayload` owns complete Sectors, referenced layer names, and the anchor Sector
- `ThingClipboardPayload` owns complete Thing values, frames, orientation/bounds/template fields, per-entry layer
  references, and the anchor Thing
- `LightClipboardPayload` owns independent baked `SedLight` records, group offsets, layer references, and the anchor
  light

There is deliberately no independent Surface, Edge, or Vertex payload. Those edit selections are stored inside a
Sector, so cross-instance geometry copy operates on the owning Sector. Thing-light and Sector-light modes likewise
route to their Thing/Sector owner rather than manufacturing a detached runtime-light record.

The same typed value payload is encoded into a protected native SED container and published to the operating-system
clipboard under SED's private MIME type. This permits cross-process paste without relying on pointers or shared
memory. The native MIME type is `application/x-sed-entities-v1`; its accompanying plain-text description
is for other applications and is never parsed as entity data.

### Encoding and system MIME contract

`encodeEntityClipboard(...)` creates a synthetic `Level` whose project type is the source project and whose only
non-empty entity vector is the selected family. Sector bodies are complete value copies. Thing bodies retain their
authored data but their source containment index is changed to `-1`; containment is derived again in the destination.
Baked lights are stored as complete `SedLight` values. The transport `Level` has one placeholder layer named
`Clipboard`, every body uses layer index zero, and the original layer identity is carried separately. This keeps the
canonical SED entity sections self-consistent without confusing a source vector index with a stable destination layer.

The resulting native `ProtectedV1` container carries the normal eight required Level sections and an optional
four-character `clip` section. `ProtectedV1` names the protection profile, not the native file version. A normalized
payload without retained MotS arch-lighting objects or bake baselines uses native SED V2; a normalized payload
retaining either uses V3. Its legacy pixel-space counterpart uses V1 without either or V4 when either remains, so its
units and arch data are explicit and recoverable.
The `clip` payload is little-endian and has this exact logical sequence:

| Field | Meaning |
| --- | --- |
| `SEDCLIP\0` | Eight-byte clipboard metadata signature |
| `u32 version` | Clipboard metadata version, currently `1` |
| `u32 kind` | Exactly one of Sectors, Things, or baked lights |
| `f64 anchorX`, `f64 anchorY`, `f64 anchorZ` | Primary entity's world-space placement anchor |
| `u32 layerCount` | Number of following per-entity layer references |
| repeated `i32 sourceIndex`, `u32 nameBytes`, `nameBytes` bytes | Source layer index and byte-counted layer name for each entity |

The entity-body byte layout is not duplicated here: it is the canonical Level layout defined by the
[SED binary contract](formats/sed_binary_format.md) and implemented by
[`saveToSedBytes(...)`](../src/core/src/assets/sed.cpp). The required Level sections use four systematic Vandermonde
MDS parity streams per stripe. The optional `clip` section uses two. Each protected 4096-byte slot carries CRC32C,
expanded semantic sections carry SHA-256, and the fixed header and section descriptors have independently checked
mirrors. These mechanisms provide corruption detection and bounded repair, not confidentiality or proof of origin.

The Qt boundary publishes those bytes under `application/x-sed-entities-v1` and also calls
`QMimeData::setText(...)` with a summary such as `SED: 2 Sectors`. Paste requires the private MIME representation,
decodes it afresh, and never falls back to the plain text or to the controller's in-process value copy. Consequently a
second running SED process receives the same validated value payload without shared memory or pointer identity.

### Decode and admission order

System clipboard bytes are untrusted. Admission is failure-before-mutation and proceeds in this order:

1. Require the private MIME type, then reject an empty payload or one larger than the native 256-MiB file limit
2. Run the complete strict native SED reader for the declared V1-through-V4 document: framing and mirrors, CRC32C slots,
   parity reconstruction, decompression and
   expanded-size limits, SHA-256, required-section schema, indices, finite numbers, and the decoded object graph
3. Find exactly one optional `clip` section and require it to be no larger than 16 MiB
4. Validate `SEDCLIP\0`, metadata version `1`, one recognized entity-family discriminator, and a finite three-coordinate
   anchor
5. Bound the layer count by the native 2,000,000-item limit, read every byte-counted layer reference, and reject
   truncation or trailing metadata
6. Require a non-empty vector for the declared family, exact equality between entity and layer-reference counts, and
   empty vectors for the other two clipboard entity families
7. Before a Sector paste, validate every polygon vertex index and every payload-local adjoin Sector/surface reference

The native reader also applies its 128-MiB per semantic-section limit, 256-MiB combined expanded-payload limit,
128-MiB decoded-object limit, and normal string/count/arithmetic bounds. The public constants are defined in
[`sed.hpp`](../src/core/include/sed/assets/sed.hpp). Valid unrelated native sections are not imported into the
destination: decoding publishes only the declared Sector, Thing, or baked-light payload plus its source project,
anchor, and layer references.

### Paste transaction and remapping

Normal Paste resolves a world target under the viewport pointer and translates the payload anchor there. It validates
Sector vertex/adjoin references before mutation, converts a legacy pixel payload to the destination Level's declared
UV space through actual per-face MAT dimensions when necessary, appends fresh IDs/numbers, resolves layers by name,
offsets authored world-space geometry/light/collision data, then remaps local Sector adjoins into the appended global range. Thing
translation includes frame values and recomputes containment; baked lights retain their authored parameters and group
offsets. The app clears the old selection only inside the successful mutation, selects all appended indices, publishes
the matching render/check refresh, and records exactly one undo command. A missing destination layer forces a full undo
snapshot so layer import is restored exactly; ordinary Thing/light paste retains the smaller partial snapshot.
Uniform Sector translation deliberately retains copied normals and exact UV data instead of recursively recalculating
the same connected component for every pasted Sector; the append/remap path is linear in payload size. A missing MAT
dimension or invalid coordinate-space tag rejects a cross-space Sector paste before the destination changes.

Layer resolution first reuses an exact destination name and otherwise appends that named layer. An unnamed reference
may reuse its source numeric index only when that index is valid in the destination; otherwise it creates `Layer0` for
an empty destination or uses destination layer zero. This fallback prevents a stale source index from creating an
out-of-range owner while keeping named layers stable across documents.

Normal Paste always targets the current viewport point, with the camera target as fallback. Although the core accepts
no target as a Paste-in-Place primitive, the application does not currently expose a separate Paste in Place command.
The payload records its source project type, but asset names remain references rather than embedded resources; a paste
into another project is not currently blocked and therefore needs compatible templates, MATs, models, sprites, COGs,
sounds, CMP semantics, and project-specific fields. The native container is checksummed rather than authenticated: a
different application can place the private MIME type on the clipboard, so successful admission establishes a
self-consistent bounded payload, not a trusted author. Clipboard encoding is versioned independently through its
`clip` metadata version even though its body reuses the native SED V1-through-V4 codec. It is also unencrypted: any process with clipboard access
can read the authored entity bytes and the text count/family summary. Readers reject a future SED or `clip` version
they do not understand rather than attempting a best-effort paste.

## Episode/campaign parsing and migration

Primary implementation:
- `src/core/src/assets/episode.cpp`
- `src/app/src/ui/episode/episode_editor.cpp`
- `src/app/src/main_window/menus/map_menu.cpp`

`episode.jk` loading consumes the declared sequence rows, requires `END`, rejects discarded trailing data, then builds
the line-ID set used to reject duplicates and dangling `gotoA`/`gotoB` targets. Self-targets and backward targets are
valid because retail MotS uses both. Runtime field/line widths and reserved values are validated before output.
`cogstrings.uni` similarly reads through `END`, uses case-insensitive keys, preserves the numeric message field plus
literal whitespace/backslashes, and rejects embedded quotes/NUL/DOS EOF unsupported by runtime parsing. UI display is
a lossless one-byte Latin-1 mapping, not UTF-8 transcoding. Its `MSGS`
number is not used as an exact row count: retail MotS declares capacity `1000` with fewer live records. Rows beyond
the declared maximum are rejected because the runtime stops at that count; the writer emits the canonical live count.

Root, `misc`, and legacy `ui` string files are parsed independently. Unique keys are unioned; conflicting common keys
or numeric metadata pause for explicit precedence. The accepted table is written atomically to `misc/cogstrings.uni`;
only then are legacy copies removed. Episode output is also atomic, and a later string-write failure restores its known
prior contents. Goals are 50 fixed indexed slots; a per-key edit serial resolves the Goals and generic Strings views
without deleting sparse data. Startup placement is separate from file persistence and uses one undo transaction.

## IJIM sector thrust viewport editing

Primary implementation:
- exact property editing: `src/app/src/main_window/properties/panel.cpp`
- viewport tool activation: `src/app/src/main_window/viewport/tools_overlay.cpp`
- pointer selection and direct drag: `src/app/src/render/gl_view_input.cpp`
- visualization: `src/app/src/render/gl_view_gizmos.cpp`
- screen-scale arrow math: `src/app/src/render/gizmos/sector_thrust_gizmo_utils.hpp`

- Sector thrust is exposed only for IJIM projects and only while Sector edit mode is active. It remains absent from
  JKDF2 and MotS UI because those formats do not serialize the IJIM `THRUST` field.
- While the direct Sector Thrust tool is active, every visible, unlocked sector can receive a thick red arrow or a
  screen-scale center handle for a zero vector. Its persisted top-center **Show all** option defaults on; turning it off
  filters completely unset Sector handles from both drawing and picking while retaining explicitly authored thrust.
  Outside the tool, the persisted viewport **Show thrust vectors** setting defaults on and shows every visible,
  unlocked Sector with a finite, nonzero thrust vector, independent of selection. Turning it off hides all passive
  arrows but leaves the active tool's existing visibility rules unchanged. Passive arrows do not create zero-vector
  edit handles or magnitude labels.
  The tool is disabled in Game Preview. Magnitude labels are limited to hovered/selected tool handles (and capped for
  unusually large multi-selections) so text layout and visibility work do not scale with every sector in the level.
- Arrow handles use the normal Sector document selection. Picking a new handle selects its owning Sector and refreshes
  the Outliner/properties; activating an already selected handle makes it primary without collapsing the current
  multi-selection. The thrust tool has an explicit selection-input exception because ordinary entity selection is
  suppressed while a modification tool owns clicks. Configured selection add/remove modifiers remain available
  through the same selection path, and that synchronous selection update occurs before a handle drag begins.
- A handle drag derives one absolute vector from the primary Sector and copies it exactly to editable members of the
  current selection. A stale property row or drag candidate cannot modify a Sector that has since left the selection.
  One drag release creates one undoable edit.
- The property direction wheel integrates relative, unwrapped pointer angles. Entering or crossing its center dead zone
  re-anchors the next valid pointer sample, including coalesced samples whose connecting chord crosses the center. This
  avoids the otherwise ambiguous half-turn jump. Release commits the last preview without another pointer evaluation,
  and each accepted preview is painted synchronously so coalesced widget paints cannot make the final direction appear
  to snap. Release keeps the same pad instance and refreshes only its numeric X/Y/Z/magnitude mirrors rather than
  destroying and rebuilding the property row.
- Arrow direction is an exact shared world-space vector. Parallel world vectors can appear non-parallel at different
  screen positions under perspective projection; this is a camera projection effect rather than per-Sector direction
  drift. Display length uses a bounded logarithmic screen-space scale tuned so the common magnitude `0.5` and normalized
  magnitude `1.0` arrows are long enough for precise direction reading, while remaining below twice the previous cap.
  Direct manipulation measures radial deltas from
  the handle's actual projected drag-start radius and saturates growth at the visible handle range, preventing an
  off-handle cursor from creating runaway magnitudes. Imported magnitudes beyond the visual cap remain unchanged when
  grabbed and can be reduced; exact over-cap entry remains available in the property editor.
- Hit visibility is evaluated at the closest point on the clicked shaft/head segment against the same dedicated scene-depth
  pass used by category gizmos. Wall occlusion is enforced for picking even when solid fill is hidden, while the GL arrow
  pass remains depth-tested against visible level surfaces and shaded 3DO Things.
- Sector centers are cached with the level mesh, off-screen handles are rejected before arrow construction, and drag-preview
  vectors use indexed lookup. In particular, painting no longer recalculates every sector center or performs one whole-level
  CPU surface ray cast per magnitude label each frame. Regular and selected line sets are each uploaded once, then reused for
  their dark outline and bright core passes.
- Sector properties expose normalized X/Y/Z plus an independent magnitude scrub and a continuous 360-degree direction
  wheel. A flat button immediately beneath the wheel cycles **XY (Z axis)**, **XZ (Y axis)**, and **YZ (X axis)**; the
  plane is not presented as a separate property field. A gesture accumulates relative
  angular movement from the displayed direction, so pressing away from the arrow does not jump it and crossing the
  `-180/180` boundary remains continuous. The normal-axis component stays fixed. If an exact component edit aligns a
  nonzero direction with the selected plane's normal axis, that plane has no defined azimuth: the wheel rejects the
  gesture and asks for either stable alternate plane instead of inventing a perpendicular direction and jumping. A
  zero vector may still be initialized in any plane. The eight wheel marks are bearings rather than snap points.
  Pointer movement is the sole preview source, so release never re-evaluates a coalesced/rounded cursor position and
  cannot add a final jump. One press/drag/release gesture creates one undo record, and the property tree preserves its
  scroll position when rebuilt.
- Editing a component keeps the typed component exact and rescales the other two components onto the unit sphere for the
  primary Sector; the resulting full vector is copied exactly to the current selection. Choosing a pad direction on a
  zero vector initializes magnitude `1`; setting magnitude to zero clears both the vector and `SECF_IJIM_UseThrust`.
  X/Y/Z and magnitude scrub previews update every editable selected Sector and the viewport continuously, then commit
  the whole gesture as one undo entry; finishing a scrub or wheel gesture does not rebuild the direction pad.
  Compact icon-only **Remove**, **Copy**, and **Paste** actions live directly in the Thrust group header and operate on
  the exact raw vector; Remove and Paste apply to the current multi-sector selection as one undoable edit, and every
  path keeps the flag synchronized.
- The arrow pass snapshots and restores the depth-test/function/mask, blend factors/enabled state, cull state/mode,
  fallback line width, and touched generic vertex attributes before the transform gizmo pass.
- Sound and Thrust use the same explicit absent/present interaction as Point Light: when every selected Sector lacks
  the property, their fields collapse to one full-width Add action. Copy/Paste feedback appears in a compact,
  mouse-transparent fading popup below the clicked action; valid copies/pastes report locally (including an identical
  no-op paste), while an empty, malformed, or inapplicable clipboard reports a local error.

## Surface extra light

Primary implementation:
- scalar/color editing: `src/app/src/main_window/properties/panel.cpp`
- format conversion: `src/core/include/sed/color.hpp`, `src/core/src/assets/jkl.cpp`,
  `src/core/src/assets/jed.cpp`
- viewport shading: `src/app/src/render/mesh/render_lighting_utils.cpp`

- JK/MotS surface extra light is a signed additive value clamped to `[-1, 1]`; positive values brighten and negative
  values subtract from Gouraud vertex light before the normal sector-light floor and final display clamp
- signed values survive JKL/JED and reusable-sector-shape round trips; sector ambient and sector extra-light fields
  retain their existing unsigned behavior
- IJIM surface extra light remains signed RGB, but the editor displays its absolute RGB magnitude and carries the
  brighten/dim polarity in a signed `[-1, 1]` intensity inside the same live color popup. Intensity magnitude is the
  color's HSV Value, so changing it moves the palette marker vertically and updates Hex. Both halves of the intensity
  ramp show that magnitude, with a separate border marking the negative/dimming side and short edge ticks marking zero
  without resembling the full-height slider marker. Starting at black and entering a negative value uses a neutral hue
  reference, producing useful negative RGB instead of multiplying black by a sign.
  Both controls share one live-preview/undo transaction and recombine into signed RGB for every selected surface
- the popup's alpha/opacity control is shown only for an adjoin rendered in Solid or Textured geometry mode; color and
  intensity edits preserve alpha on every selected surface where alpha is not meaningful. Other color properties keep
  the normal unsigned color popup without the signed-intensity strip

## Asynchronous baked lighting

Canonical detail: `docs/lighting.md#baked-lighting`

Primary implementation:
- editor orchestration/UI: `src/app/src/main_window/light_bake.cpp`
- source validation/result transfer: `src/app/src/ui/lighting/light_bake_utils.hpp`
- lighting algorithm: `src/core/src/level/lighting.cpp`

- Light range gizmos are camera-facing rings in `render/gl_view_gizmos.cpp`. A single or primary selected light keeps
  the existing solid ring; every secondary selected light receives a thinner dashed ring in its own light color.
  Secondary rings follow the same live group translation and range scaling as the primary ring and are drawn first so
  an overlapping primary remains visibly solid.
- Light edit mode exposes a compact **Light bake** overlay with a remembered **Recalc. Visible** / **Recalc. All** /
  **Recalc. Selected**
  utility menu, persisted **Visible only**, opt-in
  **Ambient**, and persisted **Auto** controls; the overlay owns the first top-center row, and
  **Edit > Light > Auto Recalculate Lighting** mirrors the same persisted Auto state
- overlay **Recalc. Visible** / **Recalc. All** uses the corresponding visible-only/all-unlocked scope.
  **Recalc. Selected** uses the selected Baked lights'
  current and last-observed old range footprints to define the destination region, then evaluates every current light
  for those vertices so overlapping-light totals stay correct. It obeys Visible-only/locked-layer filtering, remains
  disabled with no valid selected Baked light, and preserves click-time dirty footprints if queued behind another
  worker. Its source poll runs before pending Auto ranges transfer, so an unrelated just-edited light's old/new
  footprint joins the superseding selected job instead of being cleared unseen. An empty retained-default click cannot
  consume unrelated pending Auto work. **Recalculate Lighting** asks for
  Selected/Visible/All unlocked sectors before starting the same worker path
- a manual request queued behind a running worker is stored as one immutable request tuple: exact sector targets,
  permitted-target filter, dirty influences, Ambient option, and scope-specific past-tense undo label. Completion
  restarts that tuple directly rather than re-reading the later selection or toolbar state
- regular workers receive a GUI-thread bake-only snapshot containing project type, Sectors, and baked lights rather
  than copying unrelated Things, COGs, templates, hierarchy, and resource tables. A MotS bake with **Affect Arch Things**
  sources additionally snapshots Things, ARCH records and their authored baselines, templates, plus an immutable resource
  locator view. Declared 3DO resources are loaded and all-LOD model geometry is resolved on the worker; unresolved or
  malformed declared resources reject the ARCH publication atomically. No-model Arch Things are skipped. The worker
  recomputes ARCH output from the preserved authored baseline plus current opted-in sources, and the GUI publishes it only
  after source, Thing identity, and resource revisions still match. Without the MotS flag, the lightweight snapshot path
  remains in use. Automatic exact affected-sector discovery and source fingerprinting run on the worker snapshot.
  Transform-preview workers copy an immutable drag baseline on the worker thread. `QtConcurrent` runs both on the global
  thread pool and never mutates the live `Document`
- source fingerprints omit baked vertex colors and sector ambient, but include geometry, adjoin/blocking data, target
  scope, and every light position/color/flags/range/intensity input; a monotonically advancing generation invalidates
  jobs when observed inputs change
- automatic requests are throttled and coalesced for Baked `SedLight` changes; the 50 ms source poll does not watch
  geometry or Thing/Sector runtime emitters. Geometry/topology still participates in the stale-result fingerprint but
  does not independently schedule a replacement bake. The dirty region is the union of each edited light's
  old and new range, including superseded in-flight and retained transform-preview states. Input changes cooperatively
  cancel obsolete work at bounded work checkpoints, and a newer pending bake starts only after the previous worker has exited
- live transform previews also run on the global thread pool; the viewport keeps displaying the last complete preview while a
  newer drag state is calculated, cancels superseded states, and applies results only on the GUI thread
- automatic and transform-preview passes zero and recompute only texture vertices inside that dirty union. Every current
  light whose range touches the target sector is still evaluated for those vertices, preserving unchanged crossing-light
  contributions and Pascal-compatible portal blocking without expanding an unchanged light's entire footprint into the bake.
  A sector-AABB broad phase is followed by an exact referenced-vertex test, so a sphere merely crossing empty space inside a
  large sector bound does not enqueue that sector
- the core bake computes each target-sector bound once, skips lights whose range sphere cannot touch it, and uses a
  generation-stamped portal visitation table instead of clearing every sector for every light-to-vertex ray. This branchy,
  topology-heavy ray/portal work stays on cancellable CPU workers; GPU dispatch/transfer would not improve this editor-sized,
  rapidly superseded workload without replacing the compatibility algorithm
- completion copies only target surface vertex colors and, when **Ambient** is active, derived sector ambient, and
  only when both generation and source fingerprint still match. A manual recalculation is one undoable edit;
  automatic publications are derived output of the originating light edit and do not add a second undo record for
  every throttled result. Light history snapshots include the baked colors/Ambient, and an automatic result rotates
  the source record identity so save-state tracking observes a result that arrives after Save
- result transfer validates every target shape before copying any output. Texture-vertex counts participate in the
  source fingerprint, and **Ambient**-off jobs never overwrite a newer authored Ambient value with their start-time
  snapshot
- accepted colors are republished through the cached level source-vertex mapping: only affected cached colors and VBO
  ranges are rewritten, without rebuilding geometry/material batches or uploading the complete Level and Things
  meshes. Derived Ambient changes use the existing fast Thing recolor path. Invalid cache/source mappings retain a
  conservative full-mesh fallback
- manual work shows one status-bar percentage message; clicking it opens modeless progress UI with a safe **Stop**
  action. Automatic palette/scrub/typed previews suppress transient progress/completion traffic, and canceled or
  superseded results skip live geometry validation. Manual completion reports both usable candidates and target
  sectors; shutdown cancels and waits for the active future before `MainWindow` state is destroyed
- recalculating with no valid lights clears stale baked RGB while preserving vertex alpha; edited sector Ambient stays
  unchanged unless **Ambient** is active
- Pascal SED's `CalcLighting` and `CalcLightingNew` always called `CalcSectorAmbients`: it normalizes and averages baked
  texture-vertex colors, separately averages Surface Extra Light, and stores the stronger result. The C++ core keeps
  that compatibility behavior; the app option can preserve edited Ambient, and the Sector Ambient row has a manual
  icon action for applying the derivation to selected sectors
- a Light transform commit restores baseline outputs only for the undo snapshot, then republishes the last calculated
  drag preview until the committed async bake arrives, avoiding a one-frame old/no-sector lighting flash

## Runtime dynamic-light preview

Canonical detail and upstream links: `docs/lighting.md#runtime-dynamic-light-preview`

Primary implementation:
- emitter resolution, falloff, view-stable collection, and deterministic caps:
  `src/app/src/render/lighting/dynamic_light_utils.hpp`
- 3DO/world vertex payloads: `src/app/src/render/gl_view_mesh.cpp`
- GPU evaluation and editor camera collection: `src/app/src/render/gl_view_render.cpp`

Behavior:
- Light edit mode has four owner-oriented editor families: **Baked** selects `SedLight` entities, **Thing** selects only
  Things that resolve to an original dynamic `light` or Headlight emitter, IJIM-only **Sector** selects sectors with a
  usable point light, and MotS-only **Architectural** selects eligible Arch Thing model vertices. The first
  three are runtime/baked light families; Architectural is an editor-only ARCHLIGHTING model-color submode and does
  not create a runtime point light. Each family remembers its own selection and uses a distinct icon or vertex overlay
  in the viewport. Map Settings owns independent persisted **Baked lights**, **Thing lights**, and **Sector lights**
  choices; Architectural visibility follows its active submode. Editor and Game Preview keep separate runtime/baked
  values, with a fresh Preview defaulting all three off. Outside Light edit mode only those explicit runtime/baked
  overrides remain visible, rather than treating Baked as an implicitly active family. Picking and properties remain
  constrained to the active family. The active family is drawn last so a constantly visible overlapping family cannot
  hide its selectable icon. Repeated clicks at a coincident icon position deterministically cycle owner/emitter
  candidates within that family.
- Architectural vertex markers use a persisted 3–24 logical-pixel size (14 by default). The renderer applies display
  DPI exactly once; direct picking uses a bounded radius derived from the same logical size. The default authored RGB
  pass is outlined with a dark contrast point, while the preference can select a persisted fallback color for a
  background-contrast mode. Marker preferences are presentation-only and do not rewrite ARCHLIGHTING records.
- Architectural owner selection initially expands to every published vertex. The first plain vertex click narrows
  that provenance-marked expansion to one exact vertex. In an explicit subset, clicking a selected member preserves
  the batch and clicking an unselected member replaces it; Shift adds vertices and Ctrl removes them.
  A vertex double-click edits the exact set, while an owner-icon double-click edits every published vertex of all
  selected eligible ARCH Things. Every effective MotS `thingflags & 0x8` Thing has one ARCH owner marker and a logical
  ARCHLIGHTING record; an unbound archive index reads as virtual zero values and exposes those model vertices. Delete
  resets values to zero without clearing the flag. Clearing the flag leaves custom data dormant in SED, and reflagging
  restores it only when the all-LOD topology still matches. Bulk Initialize default and compatible snapshot Paste can
  promote an unflagged model Thing by setting the instance flag; they preserve unrelated flags and templates. MotS JKL
  export writes effective zero defaults unless the opt-in omit-zero setting writes `-1`, selecting the game's 3DO vertex
  lighting fallback, which can differ from the editor preview.
- The real Add actions `edit.add_cube` (Sector), `edit.add_thing` (Thing), and `edit.add_light` (Light) each have **A**
  as their sole default. Menus, tools, and Keymap resolve the same QAction and configured binding, without synthetic
  duplicate Add rows; custom and explicitly cleared bindings are preserved. Light Add dispatches to the active
  family's Baked, Thing/Headlight, or IJIM Sector placement action. Arch has no Add action. Matching light commands
  are consumed even when disabled or unavailable; they cannot fall through to another edit mode's command. Text
  input, resource editors, Game Preview, and active Fly navigation retain their existing ownership. Shared **Delete**
  keeps its mode-specific behavior; explicit **Shift Delete** is available only in Surface Vertex mode.
- Thing-light mode exposes **Thing light** and **Actor headlight** choices. The visible placement action describes them
  as **Attach point light to thing** and **Attach head light to actor**, while the Sector placement action reads
  **Place sector light**. The placement cursor carries the Thing-light glyph and is crossed/red until the current hit
  is a valid, unlocked, sector-placed owner;
  inherited regular `light` fields and enabled, authored Headlight intensities count as existing sources and prevent a
  duplicate. A missing or malformed intensity remains attachable; a zero inherited default is replaced when the
  Headlight flag is not enabled, while an enabled authored black Headlight remains a selectable non-emitting source.
  Placement clicks still
  publish their freshly picked Thing while ordinary selection is suppressed, so the tool cannot reuse a stale prior
  click. A successful placement is one undoable edit, selects its owner, and exits placement mode
- New regular lights default to `1/1/1/1` for IJIM and `1` for JKDF2/MotS. Headlights default to `1/1/1/1` for IJIM and
  `1` for JKDF2/MotS, set the original Headlight `typeflags` bit while preserving effective inherited flags, and
  materialize local `lightintensity` and `lightoffset` fields on the actor. Usable inherited IJIM intensity is
  canonicalized to four components (legacy RGB receives the same one-unit range as a new Headlight);
  disabled/invalid intensity falls back to the visible project default and invalid offset to `0/0/0`. The generic Thing **Add Value** row applies these same
  defaults at commit time when `light`, `lightintensity`, or `lightoffset` is blank, independent of focus-event order.
  Existing values expose a structured RGB/range, scalar-intensity, or XYZ-offset popup on their ellipsis button and
  on value-field double click. The color palette is embedded directly above the relevant scrub fields; every palette,
  scrub, and valid typed change updates the document and viewport immediately. The popup has no OK/Cancel step:
  clicking outside or pressing Escape closes it, and one open/close interaction remains one undoable edit
- selected Thing and Sector lights show their runtime inner/outer range rings. Baked lights expose Move and range
  Scale. Thing-light Move follows the selected source: regular `light` emitters move their Thing owner, while
  Headlights edit the Thing-local `lightoffset`; the regular transform pivot remains at the Thing origin even though
  its icon is lifted above the visual. A Headlight icon, direct/marquee pick target, range center, and transform pivot
  all use the exact runtime `Thing position + Thing orientation * lightoffset`; owner 3DO/sprite bounds never lift it,
  and its preview never presents Thing XYZ as moved. Scale applies each source's authored range/intensity semantics.
  IJIM Headlight Scale preserves RGB and edits its fourth-component range, materializing the current derived outer
  radius first for a legacy three-component value. Runtime emitters have no
  independent orientation, so Rotate is unavailable. IJIM Sector lights expose Move for `pointLight.position` and
  Scale for explicit nonzero min/max ranges only; zero remains the engine-derived sentinel. Each commit is undoable,
  filters mixed selections to owners that actually resolve the transformed source kind, and previews the emitter,
  range, and resulting runtime illumination live. Sector-light preview payloads are keyed by owning sector, so their
  transient shading does not require moving or rebuilding sector geometry
- effective template fields participate directly in regular-Thing and Headlight rendering/gizmo discovery. Editing an
  inherited emitter writes a local field override; deleting that override restores the template value. An inherited-only
  light is immutable from the placed Thing and reports that constraint instead of deleting its owner. Delete in Thing- or
  Sector-light mode removes only the selected light data, while Delete with the Sector thrust tool clears the selected
  thrust vectors and their use flag rather than deleting sectors
- Thing render field lookup remains local-first; `model3d` is the preferred model field and `model3do` is consulted
  only when `model3d` is absent, while `sprite` has render priority when usable. `thingGeometrySignature` includes the
  template name and all three local render fields, so changing
  `+gearstatue` to `+bruiser` without a local override invalidates the fast path and rebuilds the visual. A local override
  intentionally continues to render its authored resource. Template selection also refreshes the instance bbox from
  the new direct template while preserving every local value, preventing old-template fallback/framing bounds.
- Thing/Sector light gizmos use authored-color source bodies with readable dark outlines and range rings. Baked sources
  use their authored color only when the independent **Tint with light color** display option is active, and use a
  radial source-and-rays marker; regular Thing lights add a linked owner anchor; Headlights use a lamp and directional
  ray fan; IJIM Sector point lights use the rounded-square source with outward rays shared by the placement tool. Painted
  light-gizmo artwork is opaque apart from empty billboard background and antialiased edge coverage. Icons are depth-tested against solid
  world/3DO geometry. Direct and marquee icon picking share one dedicated,
  single-sample scene-depth FBO instead of sampling the transient `QOpenGLWidget` framebuffer. The pass includes
  level geometry only while solid level display is active, but includes visible shaded 3DO Things independently of
  that toggle; marquee release renders the pass once and samples only projected candidate centers. Placement and COG
  entity-pick interactions bypass category-icon interception
- runtime illumination is enabled by `kRuntimeDynamicLightPreviewEnabled`. SED intentionally uses a view-stable
  editor collector: every valid authored scene emitter is eligible without an individual camera-frustum test. If the
  set fits the project cap, camera motion cannot change membership. On overflow, the nearest sources are selected from
  the complete eligible set by camera distance and Thing index, then restored to deterministic Thing traversal order
  before upload because IJIM world clamps and Diffuse attenuation make accumulation order observable. This replaces
  portal/PVS traversal and does not claim above-cap membership parity; payload interpretation, equations, and caps do
  follow the original renderers
- Preferences > Viewport > Rendering selects **Gouraud (per-vertex)** or optional **Phong (per-pixel diffuse)**
  dynamic-light evaluation. Gouraud is the persisted default and evaluates runtime light at the original engine
  vertices before interpolation. Phong is used only for targets whose effective authored/mesh/viewport mode is
  Gouraud: it interpolates world position and model normals, normalizes the normal per fragment, and applies the pinned
  OpenJones3D `OpenGL` branch's smooth IJIM falloff. Authored Diffuse targets remain on the original flat face-anchor
  algorithm rather than being silently converted to Phong. The choice changes neither baked output nor saved data
- IJIM `light=(r/g/b/range)` values retain raw authored RGB and range, including values above one. The helpers classify
  range `<= 0.01` as the original flat/self-light case and larger ranges as dynamic lights with the original
  `distance * 0.8` world-vertex falloff and equal 3DO inner/outer radii. World-surface dynamic RGB is clamped after
  combining with authored vertex RGB; signed surface+sector extra light is added only afterwards and the result is
  clamped again, preserving the engine order for negative extra values
- IJIM actor/player head lights require the original `typeflags` Headlight bit. Their raw `lightintensity` RGB uses
  average RGB to derive radii through the IJIM camera attenuation, and their `lightoffset` is rotated with the Thing
  before being added to its position. OpenJones3D's NDY parser scans only RGB even though its runtime field has alpha;
  SED materializes NDY RGB as an editor `(r/g/b/1)` value but writes RGB only for engine compatibility. Consequently
  an edited fourth range is intentionally lossy across NDY save/reload until the engine text-parser TODO is implemented;
  regular IJIM `light=(r/g/b/range)` remains four-component
- JKDF2 and MotS use scalar values for both regular `light` and actor/player Headlight `lightintensity` emitters; the
  latter uses the same rotated `lightoffset` rule. Both derive radii from the `0.4/0.8` attenuation installed by
  `sithCamera` (rather than `rdCamera`'s unused construction defaults). World vertices use `I - distance * 0.8`;
  Gouraud 3DO vertices use `(I - distance * 0.4) * N.L` inside the minimum radius. A classic regular source also raises
  only its owner's camera-ambient floor to its clamped scalar intensity; a Headlight does not. Regular and head
  emitters share one cap: IJIM permits 64 collected Thing lights and the classic projects permit 32. When more
  eligible editor candidates exist, nearest camera distance and then Thing index provide a stable editor cap. That
  deterministic ordering intentionally replaces the engines' portal traversal order; consequently the formulas and
  caps match, but exact light membership above the cap is not claimed to be 1:1 with runtime
- disabled, invisible, and invalidly placed Things do not emit. An authored `light` field is sufficient because the
  original parser set the runtime emit flag as a side effect; SED therefore does not require that transient bit in
  stored `thingflags`. Transform previews pass their live sector result to the collector, so an emitter turns off while
  outside the level and resumes immediately after entering a valid sector
- Gouraud 3DO lighting uses the original per-vertex normal and inner/outer-radius equations. Diffuse 3DO lighting is a
  separate path: it uses the signed point-to-face-plane distance for range/falloff and the direction to the face's first
  vertex for `N.L`, matching `rdLight_CalcFaceIntensity` rather than reusing Gouraud vertex falloff. Every vertex uses
  the flat face normal; rendered double-sided backfaces use its negation. The vertex shader produces flat front/back
  colors and the fragment shader selects with `gl_FrontFacing`. The IJIM path preserves the original routine's mutable
  attenuation bug: after one contributing light, the next light inherits the prior signed plane-distance product, so
  camera-light order is intentionally observable. The classic shader instead matches OpenJKDF2: every Diffuse light
  starts with independent `0.4` attenuation, accumulation stops after the first result at or above `1`, and that
  overshoot is retained until signed face Extra Light and the final clamp. IJIM Sector
  point lights are appended only to 3DO Things in that sector. The authored `minRange` is the outer cutoff and
  `maxRange` is the inner full-intensity radius; a zero value independently derives that radius from average RGB as
  `rdCamera_AddLight` does. The original static Sector-light slot is never initialized with
  `rdLight_NewEntry`: Gouraud's vertex routine ignores its zero `bEnabled` field and applies it, whereas Diffuse checks
  that field and skips it. Collected Thing lights remain active in both modes. Preview sector crossings rewrite the
  affected Thing's point-light payload immediately instead of waiting for transform commit. Lit, Diffuse and Gouraud
  faces all receive face extra light, but only Gouraud receives authored vertex RGB. For IJIM 3DOs, the camera
  ambient is the independently clamped sum of sector ambient and sector extra light. Gouraud starts with authored vertex
  RGB and accumulates every dynamic-light term without an intermediate clamp; Diffuse accumulates its dynamic terms with
  face extra light.
  Both modes then add camera ambient, while IJIM has no Sector Tint stage. Original evaluation clamps the completed
  RGB once per vertex before interpolation and material modulation, matching `rdCache`; optional Phong evaluation
  performs the corresponding clamp after fragment runtime-light evaluation
- the runtime target uses the fully effective `min(face, mesh, viewport)` light mode. Incremental Thing transforms
  update the cached Diffuse face anchor as a point as well as vertex positions, while normals remain translation-free
- classic Sector Tint has two presentation scopes. Game Preview selects the camera sector once per frame, applies its
  scale to every scene batch, and therefore ignores each rendered object's sector. Editor mode instead stores local tint
  on world vertices and actual 3DO/SPR Thing vertices so every authored tinted sector remains inspectable from outside.
  An editor adjoin retains its owning sector's Tint so the mirror sector cannot recolor the portal MAT before the camera
  crosses it; a translucent filled side is shown from both directions so it remains visible through a Not Drawn mirror.
  Explicit classic translucent faces use vertex alpha
  `90/255`, while either face translucency or MAT alpha places an adjoin in the delayed no-z-write blend pass. Selection,
  grid, gizmo, and missing-resource helper draws stay neutral. Tint uses
  `1 + tintChannel - 0.5 * (otherTintChannels)` after completed lighting and clamps before interpolation and texture
  modulation. Classic Tint and opacity branches are disabled for IJIM
- JKDF2 Solid geometry does not continuously multiply the MAT's palette RGB by scalar light. The completed light is
  truncated to one of 64 CMP levels, the MAT cel's `solidColor` index is translated through that row, and the classic
  screen Tint is applied to the resulting RGB. Ordinary resource loading and worker-prepared picker/thumbnail assets
  carry the same 64-color ramp. Palette-only CMP input still decodes textures but deliberately supplies no Solid ramp.
  MotS RGB Gouraud and IJIM retain their separate color paths
- the 64 pairs of Thing-light `vec4` payloads use the existing 2048-byte OpenGL 3.3 std140 block without
  consuming another 512 default-uniform components

## UV coordinate compatibility

The ownership matrix is in
[`architecture.md`](architecture.md#uv-coordinate-ownership-and-format-boundaries). The operational rule is that
`Surface::texVertices`, `uScale`, and `vScale` in canonical, non-compatibility live Levels are normalized texture
coordinates for every project. A native SED V1/V4 document that cannot or must not yet be promoted is also a supported
live state, with the complete Level and history explicitly tagged `TexturePixels`. Signed values and coordinates
outside `0..1` are valid in either space and are never clamped merely because the canonical representation is
normalized. `Level::uvCoordinateSpace` makes the contract explicit and prevents a compatibility document from being
silently interpreted in the wrong units. `Model3doTexVertex` remains version-native so model parsing, Thing rendering,
and model-to-Level import each make an explicit boundary decision.

The operational boundary rules are:

1. **Classic pixel formats.** JKDF2/MotS JKL, legacy JED/ASC/LEV, native SED V1/V4, explicitly marked legacy SED OBJ, and
   3DO 2.1 store texture pixels. Admission resolves the positive first-cel width and height of every referenced MAT,
   divides U and `uScale` by width, and divides V and `vScale` by height before publishing canonical editor state.
   Explicit no-material faces with nonzero UVs receive a fresh geometry-proportional normalized island; all-zero faces
   remain zero. Conversion stages the complete result and is failure-atomic. Missing, unreadable, or dimensionless MAT
   data never falls back to a fictitious `1 x 1` texture: the compatibility Level remains tagged `TexturePixels` with
   a precise diagnostic, or the operation that requires normalized state rejects without partial mutation.
2. **Normalized formats.** IJIM NDY, ordinary Wavefront OBJ, 3DO 2.3, and native SED V2 already carry normalized UVs.
   They retain signed and multi-tile values directly. Native V2 Save/Open is bit-exact for coordinates, per-axis scale,
   and baked vertex colors.
3. **Export conversion.** JKL and JED export multiply canonical U/`uScale` by MAT width and V/`vScale` by MAT height.
   No-material UVs pass through unchanged on export; import-only fresh mapping is not repeated.
   Classic 3DO 2.1 export similarly expands its per-corner UVs but reconstructs scale when re-imported because 3DO does
   not serialize the editor's `uScale`/`vScale` fields. NDY, OBJ, and IJIM 3DO 2.3 write normalized coordinates directly.
   Export rejects atomically if texture-corner data is incomplete, coordinates or scales are non-finite, or a required
   classic MAT dimension cannot be resolved. JKL/NDY wire semantics come from `ProjectType`: JKDF2/MotS require header
   version 1, while IJIM accepts header version 2 or 3, so a stale header cannot produce a hybrid file.
4. **Version-native models.** [`3do.cpp`](../src/core/src/assets/3do.cpp) preserves 2.1 pixel values and 2.3 normalized
   values without consulting the project type. Model rendering consults the parsed model version. Sector import
   converts 2.1 through each actual face MAT and admits 2.3 directly; classic Sector export writes 2.1 pixels and IJIM
   Sector export writes 2.3 normalized UVs.
5. **Live MAT replacement.** Canonical Level UV values do not change when a MAT is replaced or its dimensions change.
   Normalized rendering samples the replacement texture with the same authored coordinates. Only a later export to a
   pixel-space format resolves the new dimensions.

Direct native V1/V4 Open adds a document-wide boundary above these Level conversions. After raw legacy pixel-space decode preserves
`TexturePixels` and the candidate resource locator is staged, the configured automatic or Ask policy uses
[`convertDocumentUvCoordinateSpace`](../src/core/src/editor/uv_promotion.cpp) to convert the current Level and every
Undo/Redo state into normalized coordinates as one transaction. It rebuilds compact history only after every state
succeeds. One missing or invalid current/historical MAT dimension leaves all input state byte-for-byte unchanged. The
Keep policy bypasses conversion. Saving a normalized workspace back over its associated legacy pixel-space path similarly converts an
isolated Level-plus-history snapshot to pixels for **Keep legacy pixel space and Save**, producing V1 without retained
ARCH objects or bake baselines, or V4 when either remains, so the live Level remains normalized and the
written history never mixes versions.

If a compatibility document remains in pixels because a MAT is unavailable, resource revision notifications first
probe that exact dependency and then prepare the full conversion on a worker only when it resolves. The app publishes
the converted current Level and compact history only if document, selection, locator, resource, and policy generations
are unchanged. Automatic mode publishes immediately, Ask prompts once on a newly convertible generation, and Keep
never requests promotion. The amber status marker distinguishes blocked pixel space from a document that is ready but
intentionally retained.

JKL and NDY carry coordinates but no independent editor `uScale`/`vScale` fields. Loading derives both scales from a
stable surface triangle before any optional pixel-to-normalized conversion. The fallback searches all corners so valid
sub-centimetre faces are not mistaken for degenerate geometry. If a finite legacy surface contains only repeated or
collinear corners and therefore has no geometric UV basis, its safe default scales are retained as in the Pascal loader;
malformed numeric UV tokens remain hard parse errors.

Geometry algorithms operate in the Level's declared space and never perform an implicit format conversion. Flatten,
merge, and extrusion preserve existing coordinates and per-axis scale. Move/Rotate/Scale either retain them unchanged
or apply the explicit connected-surface correction described above in that same space. Cleave retains existing corners
and interpolates only the new edge intersections. Auto Align, Stitch, Smart UV Project, Follow Active Quads,
Pack, numeric scaling, and modal Move/Rotate/Scale all read and publish normalized editor values; they preserve the
coordinate-space tag and commit through one failure-atomic undo transaction.

The Transform menu's Translate/Rotate/Scale and Surface UV Move/Rotate/Scale dialogs share
`src/app/src/ui/components/precise_numeric_input.cpp`. They retain full-precision decimal and scientific input, using
the UI decimal policy selected in Preferences (Dot by default, Comma, or System locale). The cached locale suppresses
and rejects digit grouping; the application never changes the process-wide Qt default locale for this preference.
Display formatting is concise and independent of the authored value. Validation checks the raw text
before spinbox fixup; invalid or nonfinite input and cancellation of either axis leave the document and history intact.
A UV command publishes only when the final stored UV coordinates change. Translate retains selection and both
history stacks for zero or unrepresentable Thing movement. The shared dialog retains the existing command ranges
and native unit stepping; concise repaint formatting never changes the authored numeric value.

Regression ownership is intentionally split by boundary. Conversion primitives and UV-basis algorithms live in
[`level_uv_tests.cpp`](../src/core/tests/level/level_uv_tests.cpp); JKL/NDY, JED, ASC, LEV, native SED V1 through V4, and
3DO-version boundaries live in the corresponding tests under [`src/core/tests/assets`](../src/core/tests/assets).
Transforms, cleave, extrusion, merge, and cross-space clipboard paste are covered by
[`level_transform_tests.cpp`](../src/core/tests/level/level_transform_tests.cpp),
[`connected_surface_uv_tests.cpp`](../src/core/tests/level/connected_surface_uv_tests.cpp),
[`level_cleave_tests.cpp`](../src/core/tests/level/level_cleave_tests.cpp),
[`level_extrude_tests.cpp`](../src/core/tests/level/level_extrude_tests.cpp),
[`level_merge_tests.cpp`](../src/core/tests/level/level_merge_tests.cpp), and
[`level_clipboard_tests.cpp`](../src/core/tests/level/level_clipboard_tests.cpp). Application tests cover starter rooms,
Add Sector, sector import/export, rendering, every UV editor transform/pivot/snap path, Undo/Redo, history, recovery,
and cross-process clipboard publication in
[`thing_utility_tests.cpp`](../src/app/tests/thing_utility_tests.cpp),
[`render_preview_utility_tests.cpp`](../src/app/tests/render_preview_utility_tests.cpp), and
[`main_window_event_filter_tests.cpp`](../src/app/tests/main_window_event_filter_tests.cpp).

## Startup / new project seeding

Primary implementation:
- `src/app/src/main.cpp`
- `src/app/src/main_window/project/startup.cpp`
- `src/app/src/main_window/document/state.cpp`
- `src/app/src/ui/project/project_startup_utils.*`

Behavior:
- startup flow can stop before the main editor is shown
- the startup dialog shows a Visual Studio-style recent list with search, project/file-type icons, pin/unpin actions,
  clearer open/configure entry points, and configured-project paths
- its preferred `1180 x 720` geometry is centered inside the selected monitor's available rectangle and reduced with a
  16-pixel frame margin when the logical screen is smaller. The frameless translucent shell paints a rounded outer
  border with no operating-system title bar, while the client-drawn Close control remains fully inside the far-right
  title-bar lane under high DPI
- one startup recent-item activation owns one Open transaction. `itemActivated` covers keyboard and desktop
  double-click; it is not also connected through `itemDoubleClicked`, avoiding duplicate Parsing/Applying/Finalizing
  and a second late autosave offer
- recents are recorded after a successful open, native SED save, or engine-level export. Exporting records the JKL,
  NDY, or applicable JED as another openable recent without rebinding the active native SED document path; pinning
  appends the item below existing pinned entries, and unpinning returns the item to last-used sort order
- `File > Open Recent > Autosave History` opens the explicit periodic-snapshot browser. Autosave snapshots remain
  separate from ordinary recents and never become document paths merely because the user browses the history
- new-project header defaults and deterministic starter-map seeding live in testable helpers in
  `project_startup_utils.*`; the real startup action calls those same helpers
- new project seeds the original Pascal `2 x 2 x 1` room bounds with ground at `z = 0`, one project-specific player, and the
  centered white baked light whose defaults are intensity `10` and range `2`
- JKDF2 and MotS starter rooms retain Pascal `NewProject` face order, zero-UV `RecalcAll` initialization, and UV
  anchors. Wall height remains the first UV axis and wall width the second
- C++ retains project-aware player templates, a resolved default CMP/material, and normal insertion-offset handling
  instead of copying Pascal's blank `MasterCMP` and `walkplayer` at `z = 0.1` for every project
- player placement attempts to reuse the same insert-offset logic as normal thing insertion
- a new project or engine/legacy import resets to the top-down camera, and delayed refit logic fits the whole level with
  extra margin after startup/layout churn. Ordinary Open arms this refit at the transaction-scope exit, after slow
  Outliner/check/resource-recovery finalization, and immediately before viewport updates resume; progress-event pumps
  therefore cannot consume the request against an intermediate layout. Native SED Open and recovery instead
  keep/restore their serialized camera and editor state and suppress that ordinary reset/refit
- main-window geometry/state is persisted; default startup is maximized, while the main window stays hidden until the
  startup dialog chooses a project or level
- focused helper coverage checks JKDF2/IJIM header defaults and the exact starter data; existing viewport coverage checks
  the exact top-down forward vector and XY fit margin. MainWindow coverage also exercises the deferred-refit state
  across a startup-hidden viewport and the authoritative show retry. Final visible dock timing depends on native Qt
  `restoreState()`/splitter event ordering and persisted geometry, so no brittle wall-clock timer assertion is used;
  that presentation, the axis gizmo, and the final player resource-derived insert offset remain interactive checks

## Add Sector shape drawing

Primary implementation:
- `src/core/include/sed/level/build.hpp`
- `src/core/src/level/build.cpp`
- `src/app/src/main_window/sector/draw.cpp`
- `src/app/src/main_window/sector/shapes.cpp`
- `src/app/src/main_window/viewport/overlay.cpp`
- `src/app/src/render/gl_view_render.cpp`

Shape sources:
- built-in primitives are created through `sed::level::makeSectorPrimitiveShape(...)` and
  `levelCreateSectorPrimitiveBoxAxes(...)`
- saved/original SED-style shapes are parsed from `data/shapes.tpl` through `loadSectorShapesTpl(...)`
- selecting one sector and using `Save Selected Sector as Shape` appends a reusable `SEC ... END` block to
  `data/shapes.tpl`. Newly saved shapes include an explicit normalized/pixel UV-space marker. Untagged historic shapes
  remain pixel-space and are converted through their MAT dimensions exactly once when placed

Interaction model:
- enabling Add Sector shows a non-modal options overlay for shape, start plane (`X`, `Y`, `Z`), and sides/segments when
  the active shape supports them
- idle Add Sector mode draws a soft cursor guide on the selected start plane, including a center cross so the insertion
  point is easy to pinpoint without implying that a cylinder will be created
- press-drag with LMB creates the 2D footprint on the selected start plane
- releasing LMB switches to height adjustment
- the next click commits the sector, while `Esc` cancels the in-progress draw
- holding `Shift` constrains the footprint to equal sides and the height to match the footprint; releasing `Shift`
  returns to free movement
- **Grid Snap** shares the default-off viewport Grid Snap action and Ctrl inversion. At press time Add Sector freezes
  the active grid origin, orthonormal axes, and effective displayed minor spacing. The anchor and footprint pointer are
  snapped with `render::snapPointToGridOnPlane(...)`; this preserves the active construction plane while projecting an
  oblique or rotated grid into it. X/Y/Z, At View, and Free placement therefore use the same transformed-grid contract
- hard-snapped footprints do not pass through the ordinary `0.005` magnitude clamp. A same-cell endpoint remains
  invalid, so the clamp cannot manufacture an off-lattice dimension. Dimensions below the geometry minimum remain
  invalid until enough whole cells are crossed
- height is rounded and bounded in a positive whole number of frozen minor cells. This keeps both the `0.005` geometry
  minimum and `10000` safety maximum on lattice even when the minor spacing does not divide either limit
- hard Grid Snap takes precedence over Shift where a projected or rotated lattice cannot also satisfy an exact square
  or equal height. Holding Ctrl inverts hard snapping off, after which Shift retains its exact constraint
- changing the persistent toggle or holding/releasing Ctrl updates the live guide/preview without committing an edit.
  Cancel clears the frozen grid frame; commit remains the existing single undo transaction
- normal X/Y/Z placement intersects the selected global construction plane;
  **At view** instead captures input on a camera-facing plane at the current
  target depth, then maps and snaps the result on the selected global output
  plane. It does not use a camera-plane lattice. This prevents an edge-on
  global plane from losing the cursor guide or drag
- **Free** captures a right/up/forward camera basis at press time and builds the
  sector in that oriented frame. While active, X/Y/Z and At view are hidden
  because they do not participate in the free-placement result

Axis mapping:
- the selected start plane controls which world axis receives the shape's local height axis
- shape vertices are normalized from their source bounding box into the drawn box, so the same saved shape can be
  reused at different sizes and on different planes
- the built-in Cube retains Pascal `CreateCubeSec` face order and starting corners, so every new wall receives the
  classic edge-aligned UV anchor instead of an equivalent but visibly rotated mapping
- the current minimum draw dimension is `0.005`, allowing sub-`0.01` sector creation when zoomed in

## Surface extrude preview

Primary implementation:
- `src/core/include/sed/level/build.hpp`
- `src/core/src/level/build.cpp`
- `src/app/src/main_window/surface/extrude.cpp`
- `src/app/src/ui/surface/surface_extrude_preview_utils.*`

Interaction model:
- Surface **Extrude** enters a modal preview state. It stays active until `Apply`, `Cancel`, or `Esc`.
- The top-center options strip owns the current `Length` and `Expand` values.
- Length and Expand use compact precise numeric controls, retaining fine decimal/scientific input without expanding the
  options strip or rounding the staged values.
- RMB/mouse movement does not commit or cancel the operation; only explicit apply/cancel does.
- The preview uses temporary sector/cut geometry and does not mutate `sed::Level`.
- The source surface is hidden when the preview sector covers or exceeds the source footprint.
- For negative expand, the source-cut preview extracts the actual retained face fragments around the opening,
  including nonrectangular outlines, instead of substituting a rectangular approximation.
- `surfaceExtrudeExpandSpinConfig(...)` derives a safe minimum and step from the selected source surface so the spinbox
  cannot step through the collapse limit.
- Applying the extrude resets Expand to zero and retains Length for repeated extrusions.

Core geometry notes:
- normal extrude requires positive finite `Length` and offsets the source polygon outward, opposite its inward-facing
  normal; zero and inward lengths are rejected without publishing geometry
- nonzero expand follows the legacy `Extrude && Expand` shape: it constructs a rectangle from the source face's local
  bounds with the requested margin, then extrudes that rectangle. Connecting it to the source clips the two footprints;
  this is not a uniform scale of the source polygon
- the UI keeps negative expand above the opening-collapse limit; the core rejects nonpositive or unrepresentable
  destination spans. Every exactly nonzero Expand selects the rectangle construction, without an absolute small-value cutoff
- generated solids retain distinct extrusion corners and a closed boundary. Original cap rounding is preserved by
  exact identity/translation correspondence and its frozen source residual; it is not a new snapping tolerance
- when the expanded rectangle strictly contains the source footprint, the connection uses a contained-ring candidate:
  the complete authored source contour remains one portal and the annulus is triangulated from the source contour and
  existing rectangle corners. The ring kernel works with stable 2D corner identities and preserves every source corner
  for the caller to materialize with its UV/RGBA/mark/cache data; it accepts simple legacy-rounded contours, rejects
  repeated or backtracking edges, and does not add Steiner points
- rounded reflex footprints are partitioned from immutable original triangles. Their boundary lines and the generated
  rectangle share an intersection registry, so repeated cuts reuse the same node instead of recomputing it from shortened
  edges. Trial mesh indices remain private to each attempt. Source height and corner attributes come from the original
  geometry; generated vertices are lifted only after the complete shared footprint is known and its area is checked
- intersection incidence and common-edge order are certified from immutable source-world endpoint operands and defining
  supports. A rounded generated coordinate is used only when its propagated interval is decisive. Uncertain classifications
  and orderings require the applicable exact or interval certificate; unresolved cases fail closed. If ordinary
  reconstruction is uncertain, both independent immutable supports use the exact binary64 ratio/intersection certificate
  and must agree before a node can enter the candidate
- queries enumerate every applicable typed support combination: projected/projected (P/P), projected/source (P/S),
  source/projected (S/P), and source/source (S/S), including the shared-actual-line fallback. Every candidate support
  representative runs the direct immutable two-line proof before a node can be reused; a relationship with more than
  one valid representative remains ambiguous and is rejected
- canonical intersection lookup reuses a node only after an exact defining-line pair and source/generated identity proof,
  so repeated crossings share one canonical node even when a later edge is reconstructed from a shortened boundary. The
  registry has no UV/RGBA attributes and does not alias nodes by comparing solved coordinates or corner attributes. A
  separate postconstruction cleanup aliases an appended vertex only when its exact XYZ and required UV/RGBA values agree;
  distinct representable source points remain distinct
- registering an original point validates `addOriginal(projected, sourceWorld)` against finite exact reprojection in the
  stored frame and retains the immutable source-world operands. Those source support and point certificates preserve
  authored collinearity when rounded coordinates would otherwise introduce a false bend
- the contained-ring candidate has its own projection frame: it validates every source XYZ as finite, selects the largest
  component of the immutable source normal with a deterministic tie rule, and drops that component directly. It does not
  fit an oblique plane or subtract an anchor before the 2D exact-orientation tests, preserving authored collinearity and
  its winding. This ring construction is separate from the generated side-face ear partition; both still require the final
  planar, convex, boundary, UV, and COG checks
- newly generated side faces must remain planar and convex. A planar reflex side created by an expansion cut is split
  into convex faces using only existing corners, preserving their UVs and colors; portal boundaries are checked again
  after connection
- each connection attempts at most three private candidates in order: the direct connection, the contained ring, and
  the source-fragment partition. Every candidate runs complete geometry, source-boundary, portal, and COG-input
  validation before publication. A rejected candidate restores the immutable input, including temporary metadata and
  adjoins; allocation failures propagate without retrying another candidate. A successful retry clears the discarded
  failure report
- the inherited-source-cap exception is intentionally narrow: it applies only to generated face zero whose complete
  boundary is an exact reverse cyclic match of the immutable source contour. Invalid indices, non-finite coordinates,
  repeated points, changed coordinates, same-facing order, and one-ULP changes fail the identity proof; this proof does
  not replace the remaining height, support, portal, or generated-side checks
- all preview and commit entry points return an optional structured rejection reason with the requested sector/surface.
  Invalid live geometry disables Apply and explains the cause. Allocation failure preserves the original Level

The maintained core regression keeps the quantized rounded-prism family in a fixed 72-case matrix: three source
slopes, three translated origins, four inherited-bend magnitudes, and positive and negative nonzero Expand values.
Each committed case checks exact report fields, source corner and UV retention, reciprocal portal boundaries, finite
generated geometry, COG pair text, and the independently measured `0.9465` XZ source footprint. Its closure oracle
requires every undirected edge to occur twice with opposite winding on distinct faces; strict planar/convex checks are
limited to generated sides and the outer cap, while inherited cap fragments are checked through selected-face identity,
source-envelope correspondence, and reciprocal portals. Positive Expand cases check full portal coverage; negative
Expand cases check source-cut plus portal area conservation. Three representative members run all five public entry
points (wire, solid, bounds, source-cut, and commit) against the same fixture family.

Extrude wire endpoints remain document `TVector` values through CPU homogeneous projection, so small segments are
not rounded through absolute float world points before clipping. Camera matrices and GPU solid rendering retain their
existing float representation; double authored geometry is not a promise of unlimited viewport precision.

Preview wire segments use the shared homogeneous seven-halfspace line clip
(`x/y/z` frustum planes plus a positive-`w` guard). This prevents join,
extrude, and Add Sector previews from flashing across the viewport when a line
crosses the camera or a side plane. The temporary solid defaults to 74% alpha;
**Preferences > Viewport > Overlays > Extrude preview opacity** can adjust it
from 5% to 100%.

## Imported sector geometry

`Edit > Sector > Import Sector(s)...` supports 3DO and Wavefront OBJ. OBJ
`o`/`g` records and non-empty 3DO hierarchy nodes define the default split
boundaries. Empty nodes are ignored. The optional combined mode flattens those
meshes into one candidate sector. By default, OBJ positions are rotated from a
conventional right-handed Y-up frame into the right-handed Sith Z-up frame:

`(x, y, z) -> (x, -z, y)`

This is a +90-degree rotation around +X, not a Y/Z reflection, so handedness and
face winding stay intact. **Transform OBJ coordinate system to SED coordinate
system** can be disabled for OBJ geometry that is already Z-up, including OBJ
written by SED. OBJ `vt` values and their per-face corner order are never
transformed by the coordinate option. 3DO data already uses Sith coordinates
and keeps its established position and UV path.

Wavefront OBJ convention and live SED Sector surfaces both use normalized UVs.
Ordinary OBJ `vt` values therefore enter the canonical Level without material-size
scaling. A legacy SED OBJ can carry the explicit
`# SED texture coordinates: pixels` marker. Import then resolves the MAT named
by every face's `usemtl`, duplicates shared `vt` entries when materials require
different dimensions, and converts the marked pixels through those exact
per-face dimensions before publication. An unresolved dimension rejects the
complete import instead of leaving a mixture of coordinate spaces. Current SED
OBJ export writes `# SED texture coordinates: normalized`.
Faces without authored `vt` values first receive the established classic default projection, including its pixel-space
rounding, and are then normalized exactly once through the face MAT. Missing dimensions reject the complete candidate;
they do not publish zero UVs with pixel-style scale metadata in a normalized Level.
The supported OBJ record contract is deliberately narrow and explicit:

- `v x y z [w]` supplies positions. The optional standard homogeneous weight
  is divided out before the pure rotation and must be finite and nonzero.
  Non-standard vertex-color components are ignored
- `vt u v [w]` supplies the two stored texture axes. The coordinate-system
  option leaves `u` and `v` unchanged. Ordinary values remain normalized; only
  an explicitly marked legacy pixel OBJ takes the independent MAT-backed
  pixel-to-normalized admission path described above. An optional third
  texture component is not representable and is ignored
- `f` preserves authored polygon corner order and resolves positive or negative
  vertex/UV indices. A `vn` suffix is accepted but ignored. The determinant-one
  axis rotation does not reverse winding; later inward-sector normalization
  reverses vertex and UV corner arrays together when required
- `o`, `g`, and `usemtl` retain their complete trimmed names, including spaces
- `vn` records are not stored because final Sector normals are derived from the
  converted polygon geometry. If directional OBJ vectors become authored state
  later, they must use the same pure `(x, y, z) -> (x, -z, y)` rotation with no
  translation
- `mtllib`, smoothing groups, parameter-space vertices, points, lines, curves,
  and free-form surfaces are ignored. Sector import does not parse an MTL file;
  it maps `usemtl` names directly to SED material names

The import path requests untransformed 3DO
mesh storage and applies each node's pivot, local transform, and complete parent
chain to a private copy. Multiple hierarchy nodes can therefore reference one
authored mesh and still become independent sector instances. The default 3DO
loader remains renderer-facing and returns transformed meshes; when nodes share
a mesh it clones that authored mesh per node before applying transforms, so
node transforms never accumulate through shared mutable storage. Mesh-bearing
nodes must have an in-range, acyclic parent chain; malformed ancestry rejects
the complete import rather than silently truncating a world transform.

The shared 3DO reader preflights declared material, vertex, texture-vertex, face/corner, and hierarchy rows before
count-driven allocation. It admits at most 128 MiB of aggregate decoded storage and 2,097,152 aggregate logical items,
charging raw geometry, needed immutable shared-mesh snapshots, and hierarchy-expanded clones before copying or
reserving them. Repeated hierarchy nodes cannot multiply a small source past that envelope. Rejection does not publish
a partial model. Independent source, section-view, per-line token, and hierarchy-work budgets also apply; raw
authored-mesh mode bypasses only hierarchy-transform work. These reader limits also apply to viewport and
resource-preview loads. See the [operational budgets](architecture.md#asset-loading-and-resource-location).

Import is transactional. Each candidate is index-compacted, converted to
sector surfaces, oriented so normals face the volume interior, and checked for:

- finite vertices and valid polygon indices;
- at least four vertices and four faces;
- planar, convex faces;
- a closed two-face-per-edge manifold;
- convex sector volume; and
- enabled consistency-checker errors on the imported sector range.

Only after all candidates pass are matching opposite faces auto-adjoined and
the candidate `Level` published. 3DO and OBJ are intentionally the only formats
in this first version: both already map to explicit polygon groups. glTF would
need a material/scene-transform policy and a triangulated-mesh-to-convex-sector
decomposition contract before it can be imported safely.

Topology diagnostics distinguish three actionable cases instead of collapsing
them into one “open or non-manifold” message: edges used by one face identify
holes/open boundaries; edges used by more than two faces identify internal,
overlapping, or duplicate faces in an otherwise closed mesh; and mixed input
reports both counts. Each captured issue identifies its source mesh/object and,
where meaningful, face/corner, welded edge endpoint indices and coordinates,
owning face indices, and edge-use count. Diagnostics are deterministic and
bounded to 16 entries, followed by the number omitted. The dialog shows a short
repair-oriented summary with expandable details. One explicit categorized Core
record writes the selected path, summary, and those same details; the dialog
suppresses generic notification logging so one failure produces one log entry.

An OBJ object/group that is already one closed shell remains one candidate.
Some level exporters instead write all sector faces contiguously under one
object; in that case the parser ends a candidate only when every undirected
edge in the current face-order prefix has exactly two uses. An incomplete
suffix is retained and rejected normally, never discarded. This makes the
partition deterministic and preserves the strict two-face manifold rule even
when shared portal topology makes whole-object edge counts 4, 6, or higher.

`Edit > Sector > Export Sector(s)...` is available with one or more selected
sectors. OBJ writes one object per sector with polygon faces, per-corner normalized
UV indices, and the SED normalized-UV marker. 3DO writes one zero-transform
mesh/hierarchy node per sector with material indices. JKDF2/MotS export uses 3DO
2.1 and converts normalized Level UVs to pixel wire values through the actual
MAT dimensions; IJIM export uses 3DO 2.3 and writes normalized values directly.
The files are committed atomically, and a failed required conversion writes no
partial export.
Neither interchange format carries editor-only sector metadata; 3DO also has
no sector-adjoin relationship, so compatible portals are rediscovered on
split re-import.

Surface Face **Delete** removes selected polygons and may leave an open Sector. Surface Vertex **Delete** is
topology-preserving: an authored vertex is removed
only when it is unused or lies strictly inside a straight polygon edge and both
two-face owners restore the same unsplit endpoint pair. Ordinary closed sectors
retain reciprocal edge incidence; equal-winding recovered/imported owners are
not rejected merely because of winding. All owner polygons and their matching
per-corner UV entries are validated on a cloned Sector and committed together.
Structural vertices in a mixed
selection are skipped. Edge and Vertex submodes categorically never fall
through to Face-mode polygon deletion: an owner-only/invalid Vertex selection
and every Edge selection make ordinary Delete a no-op. In Vertex mode, **Shift+Delete** invokes
a separate force path confirmed by an OK/Cancel warning whose safe default is
Cancel. It removes every selected vertex reference
and keeps all remaining indices in range, but it may leave a polygon with fewer
than three corners or an open sector. The document snapshot makes the entire
operation undoable; validation, interchange export, or save policy may reject
the intentionally broken geometry until it is repaired.

## Resource picker enumeration, categories, and thumbnail loading

Primary implementation:
- `src/app/src/ui/resources/resource_picker.cpp`
- `src/app/src/ui/components/category_tree_widgets.hpp`
- `src/app/src/ui/components/category_membership_utils.hpp`
- regression coverage: `src/app/tests/thing_utility_tests.cpp` and
  `src/app/tests/main_window_event_filter_tests.cpp`

First-show sequence:
1. construct the complete dialog and install an explicit dark native palette/background; raw `3DO` and template
   pickers also construct an empty hidden `GlView` host so the first native mapping already uses an OpenGL-capable
   top-level surface
2. show an opaque loading page while keeping the real resource list hidden and disabled
3. polish and activate the dialog layout before entering `exec()`
4. on the first event-loop turn, rebuild the source tree with its signals blocked and populate the selected source
   exactly once
5. restore the requested initial row with list signals blocked, enable the list, and replace the loading page
6. after another paint opportunity, bind the prepared model and activate the precreated `GlView` only if that
   selection needs a 3D preview, then start paced visible-row thumbnail batches

This ordering lets the modal shell become fully painted before filesystem/archive enumeration begins. The empty host
does not read a model or build a preview scene, and creating it before Show prevents Qt from destroying/remapping the
visible modal when its first `QOpenGLWidget` is added. The ordering also prevents source-tree current-item
notifications from triggering a second initial population.
MainWindow supplies its existing immutable resource-generation snapshot to the picker, so the work before step 1 does
not include copying mounted archive indexes. The reference-taking compatibility overload creates a snapshot for
standalone callers and tests.

Enumeration:
- resources are collected from mounted archives and on-disk roots
- results are normalized to stable display names and grouped by source
- for category-backed pickers, a complete JLH/JLL pair beside the saved level overrides shared `data/`; configured
  disk roots and mounted copies remain fallbacks
- disk override detection builds an uppercase basename set by scanning each root and kind-specific subdirectory once;
  row creation performs set lookups instead of repeated `ResourceLocator::findOnDisk()` filesystem probes
- the shared disk-name index is keyed by resource kind plus normalized root/folder/archive revision tokens and is
  bounded to avoid unbounded growth across project switches
- views using cached disk-name emphasis show bold for names with a matching loose copy in configured roots; it is
  independent of the selected preview source, so an explicitly selected archive may still supply a bold entry.
  Template and model thumbnail branches omit that emphasis; normal text there does not establish disk absence
- a separate selected-item Storage line resolves the current source section through the same candidate/root/archive
  precedence as preview reads. It displays the full winning loose path, or GOB/GOO plus archive basename and exact
  case-preserved entry path, or Unavailable. Empty selection clears it. All generic resource kinds use this line,
  including rendered image/model previews. In All, an active scoped primary override reports Project data using its
  revision identity without running the lazy reader; explicit source sections still report their chosen file/archive.
  Template identities report project template-database data rather than
  pretending to resolve each entry as an individual `.tpl` file. Asynchronous thumbnail/model completion cannot replace
  newer metadata
- refreshing Storage is metadata work and retains the sound picker's existing silent initial/category refresh path.
  An actual user WAV selection or Replay starts audio; changing Loop rebuilds an active voice only

Category model:
- `All` is a dedicated top-level entry, not the implicit parent of all categories
- structural categories are branch/grouping nodes
- leaf categories are assignable JLL-backed lists
- some assignable branches use a hidden internal `General` storage child so the user can drop onto a branch without
  seeing a fake visible category
- if a WAV pair is absent, discovery produces a normal `Sounds > Uncategorized` tree whose leaf contains the active
  project's deduplicated WAV names; the same nested editing/membership code then applies

Category editing:
- right-click tree context menu supports add/rename/delete/subcategory/leaf-category actions
- creation/rename is inline; blank names are removed on unfocus
- category deletion uses a left-aligned, vertically centered plain-text confirmation whose default and escape action
  are both **No**; the persistent minimum width includes platform title-font and caption-button metrics so the native
  show pass cannot truncate the dialog title
- list-side context menus support add/remove membership actions
- multi-select drag/drop builds the payload from the full selected set and uses the underlying resource/template names,
  not only the pressed row or its decorated display text
- dropping onto a category assigns/moves membership without switching the currently viewed category
- holding `Ctrl` while dragging from a non-`All` category moves entries out of the current category into the target
- a plain drop is Copy; a `Ctrl` drop is Move only when it has a real non-`All` source and a distinct assignable target;
  root, empty, same-source, and `All`-source move attempts are rejected
- assignment is batched to reduce UI stalls after release
- drag feedback includes action/cursor feedback and a thumbnail drag pixmap; rectangle/multi-select state is cleaned up
  after the drag finishes
- resource-picker and template-editor membership changes are copy-first transactions: clone the JLH tree while
  preserving shared JLL identity, mutate the working graph, save the JLH/JLL pair, and publish only on success. A no-op,
  failed save, or exception restores the exact pre-edit graph, including removing any unsaved hidden `General` child
- disk persistence stages and checks both new files in an adjacent transaction directory before touching either
  original. Existing files are moved to recoverable backups, the JLL is installed before the JLH, and any reported
  backup/install failure rolls both paths back. If rollback itself fails, the error names the retained recovery
  directory. This is failure-atomic for reported filesystem errors, not one OS-wide atomic operation across two paths:
  a power loss can still interrupt the rename sequence, and successful commits may leave an obsolete transaction
  directory if best-effort backup cleanup fails
- direct coverage exercises action resolution, real-name multi-selection payloads, stale-selection cleanup, a synthetic
  `Ctrl` multi-item drop with stable current view, case-insensitive add/remove and deduplication, Copy/Move semantics,
  failed-save rollback, hidden-`General` rollback, and JLH/JLL save/reload. These tests use the same shared widgets and
  transaction helper wired by the template editor and category-backed resource pickers

List selection and preview controls:
- **Source order** is the authored provider order, never an implicit alphabetical fallback. For a category it is the
  JLL entry order followed by nested subcategories in depth-first JLH order, with the first case-insensitive duplicate
  retained; archive entries, template records, categories, and subcategories retain their corresponding source order.
- wrapped ListMode flows top-to-bottom and fits an integral number of columns to the current viewport. Column width is
  rounded upward, both scroll axes use per-item steps, and each item size hint matches `gridSize()`; this prevents a
  retained pixel offset or clipped leading/final row, while any right-edge cell overhang remains inside the delegate's
  trailing content padding
- list-mode selection is painted by the picker delegate as one borderless filled marker bounded to the elided
  filename width; native mouse-over state is ignored, so hovering selected or adjacent WAV rows cannot add a second
  stripe, a column-sized outline, or change selection/preview state
- projection, render-style, and **Fit** controls are created only for their model-preview role and are hidden for WAV
  and other non-model picker kinds
- WAV preview metadata uses the canonical PCM decoder to derive duration and channel layout before the same bytes are
  handed to playback; raw byte size is not shown
- **Replay** resets queued sink data and rewinds the picker-owned voice, including during active playback. **Loop** is
  checkable and off by default; toggling it rebuilds the selected voice from its beginning, with no drain gap in loop
  mode. The `0..100` preview-volume slider applies immediately and persists under
  `resource_picker/sound_preview_volume_percent`; it does not alter viewport ambience volume

Material thumbnail loading:
- thumbnails are not rendered for the entire list eagerly
- picker computes the visible row range, loads only a limited batch, and uses a timer to continue loading while the
  list is visible
- thumbnails are cached by normalized material name plus revision-sensitive preview inputs
- thumbnail work begins only after the deferred resource population has revealed the list, so it cannot delay or
  expose an incomplete first frame
- Surface property/outliner MAT thumbnails undo the renderer-only vertical mirror carried by the shared preview image
  before creating their widget icon; picker and viewport texture orientation remain unchanged

Template / `3DO` browser loading:
- template editor lists, template-valued pickers, and raw `3DO` picker all use the same visible-range-first model
- both raw and template lists add file-watch dependencies only for selected/visible models and their materials; the
  raw picker does not scan every listed `3DO` revision before visible thumbnail work can begin
- an off-screen row satisfied immediately from the bounded rendered-image cache also registers its exact model/CMP/MAT
  dependencies, so the icon cannot remain stale after a live external edit merely because it skipped the visible loader
- a cold rendered-thumbnail lookup first checks a bounded name/CMP/size hint index. A hinted candidate consumes the
  exact revision key published with the worker image and validates its cache epoch, avoiding synchronous GUI-thread
  3DO/CMP/MAT revision probes. File-watch invalidation advances the epoch and clears both the hint and rendered cache
- one list rebuild resolves its thumbnail mode once and creates one loading/unavailable icon for all matching rows;
  `QIcon` implicit sharing replaces thousands of identical placeholder paint operations in a large raw-model list
- visible-row discovery samples at most a bounded grid of viewport positions. It never calls `visualItemRect()` for
  every off-screen resource, including from the 150 ms keepalive timer
- picker search coalesces rapid typing for 120 ms before one list rebuild; bulk row replacement suppresses intermediate
  selection callbacks, preserves the current 3D preview when its resource name is unchanged, and does not recreate an
  unchanged raw-`3DO` filesystem watch plan
- `3DO` parse/hierarchy/bounds prep and referenced MAT/CMP read/decode use the configured bounded worker jobs against
  immutable locator snapshots. Equivalent picker, Template Picker, and Template Editor snapshots share parsed models,
  bounds, and decoded material pixels by exact resource revision, so opening a second browser does not repeat parsing
  merely because it owns a different locator object
- the ResourceLocator already returns one complete loose-file or GOB payload. The preview parser scans 3DO byte spans
  through non-owning line/token views and parses numeric fields with `std::from_chars`, avoiding a duplicate whole-file
  string and per-line strings; stream/path callers fill one reusable 8 KiB buffer before using that same parser. The
  parser independently bounds source bytes, section/token views, decoded storage, and hierarchy vector visits; see
  [3DO admission](architecture.md#asset-loading-and-resource-location). Raw authored-mesh mode omits hierarchy work
  while retaining the other bounds. MAT parsing takes ownership of the existing byte vector. Memory mapping does not reduce archive work and adds setup
  overhead for typical small loose assets
- the initially selected raw `3DO` starts its bounds preparation before background thumbnail batches. Worker completion
  normally refreshes it directly; a 30 ms single-shot cache retry covers delayed native-window completion delivery and
  stops as soon as the prepared selection is displayed or proven unavailable
- workers publish the complete hierarchy-transformed parsed model, bounds, decoded MAT pixels, and final list
  thumbnail `QImage`. The worker raster follows the selected preview's mesh inclusion, classic/IJIM material mapping,
  alpha/face flags, zig-zag triangulation, neutral asset lighting, and wireframe contracts. World-space normal rejection
  uses the scale-safe numeric epsilon; projected-area rejection remains a later screen-space decision, so dense small
  parts such as actor heads are not discarded before camera fitting
- up to the configured number of combined preparation/render jobs run at once. The visible loader never drains
  off-screen rows; completed images are assigned in short configured GUI batches. Each cold thumbnail is rasterized at
  twice the requested dimensions and smoothly reduced with aspect preservation to the exact icon canvas for clearer
  small meshes and edges
- scroll and search activity reset a 96 ms publication quiet interval. A single-shot timer is reused instead of
  enqueueing independent callbacks, and the current row is tried before its visible neighbors once interaction stops
- one GUI publication turn always permits one completed icon so progress cannot stall, converts at most one new shared
  `QImage` to `QIcon`, then obeys both the configured 1-8 icon limit and a 2 ms elapsed-time budget. Publication turns
  are 40 ms apart, below the 33 ms interactive-preview frame cadence, before yielding back to input and painting
- when a visible row starts preparation, its owner stops periodic polling until a worker completion signal arrives.
  Repeated pending checks therefore perform neither resource revision I/O nor cache-key reconstruction. Publishing a
  completed icon blocks the fixed tile model's `dataChanged` signal and repaints only that tile, avoiding per-thumbnail
  list layout work while a Page Down result set fills
- registering a new visible 3DO or MAT extends the existing filesystem watch plan from that dependency alone. The
  growing set is not rescanned after every completed tile; explicit resource/archive remounts retain a full rebuild
- rendered-image keys include that same invalidation epoch as well as unambiguously labelled model, dependency,
  renderer version, wireframe, and size fields. The capture compares its starting epoch before publication and stays
  pending if a same-size/same-timestamp file notification cleared caches while it was rendering
- a worker finishing after an epoch change may not publish its stale model/image result, but it still sends a
  stale-completion wake-up to a live owner. The owner then retires the old in-flight key and schedules the same visible
  row against the new generation; owner destruction suppresses both publication and wake-up
- loading placeholders and keepalive/self-healing timers recover stuck visible rows without polling while bounded
  workers are active
- shared preview caches exist for CMP palettes, MAT images, parsed `SPR`, sprite preview images, parsed `3DO`, and
  rendered thumbnails; cache identity follows selected disk/GOB revisions instead of a dialog-local locator address.
  Heavy caches are byte-bounded below a combined 1 GiB and use a two-queue K=2 approximation: one-hit entries remain
  probationary, repeated entries become protected, and the oldest probationary data is evicted first. Exact file
  revisions reject stale entries, while project/resource snapshot publication clears the complete shared epoch
- Thing-placement template preview icons add a MainWindow-owned 192-entry cache on top of the shared revision-keyed
  rendered-image cache. Dependency or resource-generation invalidation clears the icon cache, and a key that remains current
  is served from it without another model or image conversion
- Thing-placement preview producers and consumers resolve one shared CMP identity through the Level fallback chain.
  Classic levels with an empty raw Master CMP therefore use the first Sector CMP or `dflt.cmp` consistently instead of
  publishing worker results under a key that the list never polls
- thumbnail lists use per-pixel scrolling. `Shift + wheel`, Page Up, and Page Down move by complete tile rows while
  retaining one visible row of overlap, so a partially visible edge row is not skipped
- sprite picker also supports thumbnail layouts, including `No thumbs`

`3DO` preview:
- raw picker and template preview share the same preview widget behavior
- authoring previews always show the selected asset even when its template inherits engine `Disabled` or `Invisible`
  Thing flags; those flags continue to hide Things in the actual Game Preview viewport
- preview supports `Textured`, `Solid`, `Wireframe`, `Vertices`, `Orthographic`, `Perspective`, and `Fit`
- render style is applied to the preview Thing model. `Wireframe` therefore uses the model-edge renderer instead of
  the empty helper Level's wireframe flag; preview controls intentionally do not expose Thing bounding boxes or
  Thing-origin dots. `Vertices` draws the transformed vertices of the actual model as points, not Level vertices or a
  single Thing-origin marker
- default motion is configurable preview auto-rotation
- click-drag changes yaw/pitch directly

Sprite picker and template preview:
- sprites use a dedicated decoded-image preview path instead of being forced through the `3DO`/thing preview widget
- template editor and picker sprite preview behavior has been pushed closer together
- every decoded-image and outer sprite thumbnail/icon key includes the selected SPR revision, active CMP revision, and
  the referenced MAT revision. List and selected previews use the same unmirrored decoded pixels and alpha, so a
  MAT-only replacement refreshes the preview without flipping or transposing it

Sprite authoring preview:
- `resource_authoring_sprite.cpp` publishes an isolated one-Thing Level to the normal Game Preview renderer. Geometry
  edits retain the previous camera's orbit angles, center and pan, then expand framing only when the displayed
  geometry needs more room. **Recenter** explicitly returns to a straight-on orthographic view. **Bounds** starts
  enabled unless a saved presentation explicitly disables it
- that temporary Level has zero ambient illumination and retains the Gouraud viewport ceiling, so authored Sprite
  light modes and extra light remain inspectable. It does not change the project's Sector lighting. Classic mode 0
  is fully lit and ignores scalar extra light; other offered Sprite modes use the scalar extra-light input without
  a source vertex-light gradient. IJIM mode 0 likewise bypasses extra RGB, while its Sprite renderer caps modes
  above Diffuse at Diffuse and uses flat extra RGB plus camera ambient. IJIM extra-light alpha remains opacity in
  every mode. See the [classic Sprite draw](https://github.com/shinyquagsire23/OpenJKDF2/blob/02297602cb289b113df27be33b5a7dddbe3d91c1/src/Primitives/rdSprite.c#L171-L244),
  [classic raster composition](https://github.com/shinyquagsire23/OpenJKDF2/blob/02297602cb289b113df27be33b5a7dddbe3d91c1/src/Raster/rdCache.c#L653-L716),
  and [IJIM Sprite draw](https://github.com/smlu/OpenJones3D/blob/b9c0eaa56bb51307b0787add4b614d9468e5d372/Libs/rdroid/Primitives/rdSprite.c#L295-L323)
- classic authored geometry is capped at the view's Textured ceiling: 0 draws no card, 1 displays its corner vertices,
  2 its four outer wire edges, 3 the MAT cel's Solid color, and 4 its texture. The isolated preview retains its six
  cached triangle vertices in every mode. A default-disabled shader uniform uses normalized quad UVs only for this
  view's corner/wire coverage, so the wire has no triangulation diagonal and numeric scrubs retain fixed topology.
  Solid uses the existing MAT/CMP color/ramp path. The uniform resets after each isolated draw; ordinary scene
  batches and normal Thing transform paths retain their existing geometry policy
- IJIM stores SPR geometry but its Sprite projection and raster dispatch use the renderer's global geometry mode,
  not a per-Sprite geometry ceiling. Its loader also skips the texture-mode token. The pane hides those unused
  authored rows while retaining both raw values. Its IJIM-only **Preview geometry** control selects the isolated
  renderer's global mode: 0 draws no card, 1 white corner points, 2 the closed white outer wire, 3 an untextured
  white or extra-light RGB card, and 4 the MAT texture. IJIM Solid omits the material rather than using classic
  MAT/CMP Solid color. The control defaults to Textured and changes neither authored data nor camera, animation
  or mesh caches. The existing isolated shader route retains all six cached vertices, and resets its corner/wire
  uniform after drawing; ordinary scene batches retain their existing policy.
  The upstream allocation path omits UV arrays for geometry at or below Solid while its Sprite projection assumes
  UV input, so those stored values are not claimed as a safe engine wire/point route. See
  [IJIM allocation](https://github.com/smlu/OpenJones3D/blob/b9c0eaa56bb51307b0787add4b614d9468e5d372/Libs/rdroid/Primitives/rdSprite.c#L73-L125),
  [projection](https://github.com/smlu/OpenJones3D/blob/b9c0eaa56bb51307b0787add4b614d9468e5d372/Libs/rdroid/Engine/rdClip.c#L3981-L4082),
  [raster dispatch](https://github.com/smlu/OpenJones3D/blob/b9c0eaa56bb51307b0787add4b614d9468e5d372/Libs/rdroid/Raster/rdCache.c#L165-L181)
  and [point/wire drawing](https://github.com/smlu/OpenJones3D/blob/b9c0eaa56bb51307b0787add4b614d9468e5d372/Libs/rdroid/Raster/rdCache.c#L434-L456),
  and [SPR loader](https://github.com/smlu/OpenJones3D/blob/b9c0eaa56bb51307b0787add4b614d9468e5d372/Libs/sith/World/sithSprite.c#L344-L373)
- classic Texture mode names Affine (0) and Perspective (1), labels 2 and 3 as legacy/unknown, and adds an exact
  preserved-current-value choice for other integers. The classic loader admits mode integers without a 0–3 range
  check; unknown source tokens are not clamped. Its Sprite draw consumes the minimum of stored, global and Thing
  texture modes, while ordinary Thing preparation caps effective mapping at Affine or Perspective. Every billboard
  corner has the same camera depth, so those algorithms produce the same visible card. See the
  [classic enums](https://github.com/shinyquagsire23/OpenJKDF2/blob/02297602cb289b113df27be33b5a7dddbe3d91c1/src/types_enums.h#L31-L38),
  [loader](https://github.com/shinyquagsire23/OpenJKDF2/blob/02297602cb289b113df27be33b5a7dddbe3d91c1/src/World/sithSprite.c#L119-L139),
  [Sprite draw](https://github.com/shinyquagsire23/OpenJKDF2/blob/02297602cb289b113df27be33b5a7dddbe3d91c1/src/Primitives/rdSprite.c#L220-L242)
  and [Thing mapping cap](https://github.com/shinyquagsire23/OpenJKDF2/blob/02297602cb289b113df27be33b5a7dddbe3d91c1/src/Engine/sithRender.c#L2883-L2911)
- **Extra light** has a scalar editor with **Extra light intensity** help for classic profiles and an RGBA swatch
  for IJIM. Signed and HDR source values remain authored exactly: the renderers clamp final lighting channels to
  0–1, and the classic path also enforces the camera ambient floor. Zero ambient in this private preview makes
  negative illumination black rather than silently rewriting it. IJIM alpha remains opacity in every light mode.
  Closing its staged RGB popup copies the close callback locally and guards the popup with `QPointer`, because a
  synchronous commit notification can delete both the resource pane and popup. Normal close commits once; Escape
  cancels the staged color
- native Sprite presentation version 1 optionally stores `globalGeometryMode` as an integer from 0 through 4;
  absence defaults to 4. Validation rejects malformed types and out-of-range values before changing source or
  presentation. Restore applies this renderer-global inspection state independently from the stored SPR geometry
  token, including when the preview is hidden or awaiting camera restoration
- numeric preview updates coalesce on a 33 ms deadline that new scrub events do not postpone. When the asset type,
  material and modes retain the isolated view's fixed topology, `updateIsolatedSpritePreviewGeometry` preflights
  finite coordinates, cached ranges, buffer byte ranges and the owning GL context before replacing the card's six
  vertices and the Bounds wire's 24 endpoints. It retains decoded MAT cels, textures and the Level mesh caches.
  Cold/invalid caches, zero dimensions, topology changes and dependency invalidation use the complete preview rebind
- decoded MAT/CMP dependency keys use the source resource context, so publishing a new draft SPR override does not
  change unrelated archive-material keys. Resource notifications explicitly invalidate those dependencies. In-place
  numeric updates retain the current animation cel and running clock; playback is not restarted by each scrub tick
- the temporary Thing bounds use the same conservative billboard cube as **Bounds**: radius is the Euclidean XYZ
  offset length plus the card's half-diagonal, centered on the owning Thing. IJIM type-2 uses its exact oriented card
  bounds and hides the ignored XYZ offset controls without changing their raw values. Classic types 0, 1 and 2 use
  all XYZ controls. Zero-width/height drafts retain their degenerate geometry; external Save still requires positive
  dimensions
- this billboard cube is an editor orbit-safe envelope, not the current card's tight AABB or an engine collision
  box. A camera-space corner is `offset + (+/-width/2, 0, +/-height/2)`; an orthonormal camera frame preserves its
  length, and the triangle inequality bounds every world-axis component by `length(offset) + halfDiagonal`.
  The [classic](https://github.com/shinyquagsire23/OpenJKDF2/blob/02297602cb289b113df27be33b5a7dddbe3d91c1/src/Primitives/rdSprite.c#L93-L95)
  and [IJIM](https://github.com/smlu/OpenJones3D/blob/b9c0eaa56bb51307b0787add4b614d9468e5d372/Libs/rdroid/Primitives/rdSprite.c#L115-L117)
  Sprite culling radius is only the half-diagonal and excludes offsets. IJIM type-2's displayed AABB
  instead takes component-wise minima/maxima of its actual four oriented corners and ignores all XYZ offsets.
  XYZ field, child-edit and label tooltips explain camera-right X, camera-depth Y and camera-up Z. Y need not move
  or resize an orthographic card on screen, although a nonzero depth offset can enlarge the conservative envelope
- `GlView::fitSpritePreviewToLevel` projects all eight bounds corners onto the existing camera basis. Orthographic
  half-height is `1.06 * max(abs(viewY), abs(viewX) / aspect)`; the camera's aspect-aware orthographic scale follows
  from that extent without an artificial minimum zoom. Ordinary edits only decrease scale when needed, so shrinking
  a card does not undo the user's chosen zoom
- depth fitting is separate from orthographic scale. Enabled clipping grows the near/far interval only as needed.
  Disabled clipping retains the camera's fixed GPU depth interval and clipping flag, choosing a feasible distance
  along the existing view axis. Perspective fits retain projection and solve the distance from both field-of-view
  limits. Orthographic Recenter retains an already valid distance instead of enlarging it with the resource's radius.
  Native presentation restores its saved pose and zoom, derives the depth interval against that camera without
  intermediate notifications, then publishes the final state. These policies apply only to the resource preview,
  not normal viewport camera modes
- the authoring view's existing serialized-SPR context marker keeps the renderer from narrowing those fitted planes
  again using ordinary mesh caches, which omit camera-facing sprite offsets. Normal viewport orthographic depth
  precision fitting retains its existing policy
- a fit is computed on a candidate camera and installed only when representable. If no distance contains the bounds
  within the unchanged depth range, the pane reports `Sprite preview exceeds the camera's depth range.` and keeps the
  prior camera and every authored dimension/offset. Recenter does not clamp or rewrite the resource
- focused synthetic MAT/CMP tests cover JKDF2, MotS and IJIM type geometry, camera-space XYZ, wide/tall frame edges,
  exact orbit/pan retention, repeated dimension edits, real texture pixels, enabled/disabled depth boundaries and
  explicit failure without camera or source mutation. Real held mouse scrubs verify intermediate rendered geometry,
  continuous resource publication, default Bounds and unchanged Level mesh rebuild counts. Synthetic two-cel MAT/CMP
  archives verify paused cel retention and ongoing playback during numeric updates
- a patterned white/blue MAT with an independent red Solid palette index verifies decoded pixels before mode
  edits, quarter-light output for offered lit modes, live RGB picker preview before canonical commit, scalar edits
  without a mesh rebuild, classic no-fill/corner/wire/Solid/Texture output, absence of a wire diagonal, retained
  camera pose, ignored IJIM per-Sprite geometry and exact XYZ tooltip vectors across project profiles
- separate follow-up audits remain for nonzero classic Thing roll and classic stored type-2 gizmo/snapping support.
  The shared renderer still applies Thing roll, although the classic sprite draw routine ignores it; gizmo support
  geometry still branches on the raw type token. The isolated preview framing/update policy does not change those
  normal viewport transform paths

`3DO` thumbnail rendering:
- revision/capture policy is centered in
  `src/app/src/ui/components/preview_resource_revision_utils.hpp` and
  `src/app/src/ui/components/template_model_preview_icon.hpp`
- list-thumbnail pixels are produced by the dedicated preview worker pool as immutable `QImage`s; selected interactive
  previews continue to use `GlView`. The two paths share hierarchy-transformed mesh data and explicit UV,
  triangulation, alpha, material-orientation, lighting, and wireframe contracts. Thumbnail workers never create or
  access OpenGL or widget objects
- the persisted `Shaded`/`Wireframe` preference is normalized once for both picker and Template Editor lists; integer
  `1` remains Wireframe, while the removed `None` mode and legacy Boolean values migrate to Shaded. Wireframe
  thumbnails select the normal Thing model-edge path without helper-Level geometry
- a template with an unreadable SPR continues to its valid 3DO candidate instead of publishing an unavailable icon
- resource revision keys identify the exact disk file or GOB entry selected by `ResourceLocator`, including mounted
  archive metadata; rendered model keys include the `3DO`, active CMP, and every referenced MAT revision
- cached `3DO` thumbnail entries are also invalidated by live file-watch updates when source model files change on disk
- mounted GOB/GOO files and their parent directories are watched independently of the visible dependency set. A change
  transactionally reopens every mount in precedence order before ordinary preview invalidation; parse failure or the
  temporary missing phase of atomic replacement leaves the previous complete indexes published. Automatic reload
  requests retry after 250, 500, 1000, 2000, and 4000 ms, then continue at 4000 ms until success or owner teardown.
  They emit one warning for the failed notification/retry state and retain the old index throughout. A fresh notification
  restarts the delay at 250 ms; warning suppression resets only after a successful remount. Raw/resource picker and
  Template Editor dialogs own locator copies and defer use while their bounded model-prep jobs still read them.
  MainWindow's viewport owns the mounted archive indexes and retry state; hidden thumbnail, selected placement, and the
  outer placement watcher delegate mounted-archive reloads to that owner while retaining their loose-file watches. A successful
  nested picker or Template Editor reload refreshes its owning rows and icons after the new archive generation publishes
- model parsing, texture decoding, supersampled rasterization, and rendered-`QImage` cache publication occur on the
  bounded worker. Only `QPixmap`/`QIcon` creation and small-batch list-item assignment remain on the GUI thread; later
  dialogs reuse the revision-keyed image. The first GUI conversion for a completed image is explicitly distinguished
  from a local icon-cache hit so at most one conversion/repaint enters a paced publication turn
- the worker pool is dedicated to resource previews, runs at low priority, and is globally bounded across all picker,
  Template Editor, and Thing-placement owners. The worker count is configurable from 1-16 (default 2-4), while GUI
  icon publication is separately bounded to 1-8 completed images per update (default 2), pauses during scroll/search,
  converts at most one new shared image every 40 ms, and yields after a 2 ms per-turn budget
- classic JK/MotS 3DO UVs are normalized by decoded MAT dimensions; IJIM 2.3 UVs are already normalized. Polygon
  decomposition uses the same zig-zag order as `GlView`, and alpha behavior follows MAT alpha depth plus face flags
  rather than scanning the final pixels heuristically. Textured faces use the interactive asset preview's neutral
  white ambient instead of thumbnail-only directional darkening, which keeps small child meshes such as actor heads
  visible at icon resolution

## Sound decoding, preview, and ambient transitions

Primary implementation:
- `src/core/src/assets/wav.cpp`
- `src/core/src/assets/indy_wv.cpp`
- `src/app/src/ui/audio/app_sound_player.cpp`
- `src/app/src/ui/audio/app_sound_player_pcm_device.hpp`
- `src/app/src/ui/audio/audio_transition_utils.cpp`
- `src/app/src/ui/audio/sector_sound_utils.cpp`
- `src/app/src/main_window/viewport/audio.cpp`

Decode/playback sequence:
1. read bytes through the active `ResourceLocator`
2. detect IJIM's `INDYWV` signature and convert the compressed stream to canonical PCM WAV when needed
3. parse WAV chunks into a validated `PcmWave` (sample rate, channel count, sample format, sample bytes)
4. convert the PCM description to `QAudioFormat`
5. stream a sequential, unbuffered one-shot or looping in-memory device into `QAudioSink`

The `INDYWV` conversion rejects malformed metadata, unsupported channel counts, truncated escape sequences, and
unreasonable inflated sizes before allocation. Converted output reuses the ordinary WAV/PCM path rather than having a
second playback implementation.

Qt 6 one-shot drain contract:

- decoded PCM length is authoritative; the player does not shorten the `INDYWV` inflated/WAV `data` payload
- a non-looping source appends one reported `QAudioSink` buffer of synthetic silence after its real PCM so queued real
  samples and resampler delay advance before source EOF changes the sink to `IdleState`
- silence is format-correct: `0x80` for unsigned PCM8 and all-zero bytes for signed integer/float PCM
- Qt 6.10+ uses `bufferFrameCount()`; earlier Qt 6 converts `bufferSize()` with `bytesPerFrame()`. If no usable size is
  reported, the frame count uses Qt's documented 250 ms default buffer duration
- Replay and immediate voice replacement call `QAudioSink::reset()`, rewind both real and drain cursors, and restart;
  queued data from the previous pass is discarded rather than appearing before the new pass
- looping sources have no drain padding and report continuous availability, preserving gapless picker Loop and
  ambient-sector playback

Ambient update sequence:
1. the viewport timer obtains the camera eye position
2. `levelFindSectorForXYZ` resolves the containing sector
3. no sector, no sound, or a muted active pipeline produces no request
4. a valid request contains the sound name and volume clamped to `[0, 1]`
5. unchanged sound/volume keeps playing; same sound/new volume updates the existing fade target; a new sound creates a
   new voice and moves the old voice into the fade-out set

Transition contract:
- duration is 500 ms
- new ambient voices start at zero
- outgoing voices stop only after reaching zero
- incoming and outgoing voices advance on a precise 16 ms Qt timer
- the viewport's approximately 125 ms polling does not restart an unchanged ramp
- picker previews bypass the ambient fade and play immediately; owner-checked volume changes are immediate and Replay
  resets/rewinds the active PCM device (or reloads the selected resource if picker ownership was released); changing
  Loop rebuilds the voice from the beginning

Decoded-sound cache:
- key = sound name + normalized disk roots + normalized mounted archive paths
- maximum 64 entries and 64 MiB
- least-recently-used entries are evicted first
- oversized single entries are played but not cached

Focused playback-source coverage in `src/app/tests/app_sound_player_pcm_device_tests.cpp` checks sequential
one-shot reads across the PCM/padding boundary, finite EOF, format-specific silence, full Replay rewind, empty-input
rejection, and gapless looping. Picker UI tests separately check wrapped edge geometry and Replay/Loop/volume state.
These tests validate deterministic bytes and UI policy; they are not a claim of audible device-level validation.

Sound category selection is separate from decoding. `resourcePickerCategoryFileName(...)` maps WAV pickers to
`wavs.jlh`, `mwavs.jlh`, or `ijimwavs.jlh`. A complete saved-project pair wins over shared data and mounted copies;
without any pair, the picker creates an editable nested fallback from the WAVs visible in the active project. Edits
save to the saved level's project directory when available.

Both COG script declarations and legacy JED COG-value metadata accept `sound` as the legacy spelling of the canonical
`wav` COG type. They retain `CogType::wav` through loading, so item-editor browse buttons and value-field double-clicks
open the WAV picker rather than presenting a plain string field.

Sector properties expose an icon-only Remove action in the Sound group header when the current selection contains
ambient sound data. It clears both the sound resource and volume for every editable selected sector in one undo
transaction; after the last selected source is removed, the Sound fields collapse to one **Add Sound** action. An
empty COG LINKS section is rendered as a disabled `NONE` label with no navigation roles; even a
synthetic double-click cannot resolve it as COG/value index zero.

## Viewport antialiasing and sample requests

Primary implementation:
- src/app/src/main.cpp
- src/app/src/main_window/preferences/panel.cpp
- src/app/src/render/gl_view_render.cpp

Behavior:
- viewport/msaa_samples stores the requested default-framebuffer sample count: 0, 2, 4, 8, or 16; an absent,
  malformed, or unsupported value resolves to the default 4
- startup installs a safe four-sample QSurfaceFormat before QApplication, then reads the portable settings path
  after Qt has initialized the application name and executable directory. It reapplies the validated request before
  constructing MainWindow, so the first viewport context receives it. The graphics driver can clamp the request;
  the actual context sample count may be lower
- sample-count changes require an application restart because an existing QOpenGLWidget context is not recreated by
  the Preferences dialog
- framebuffer MSAA remains enabled for scene rendering according to the requested format even when Wireframe AA is
  off. That preference gates multisampling only for the wire/Thing/highlight line pass, which restores the incoming GL
  state afterward
- fill-owned barycentric wireframe edges use smooth shader coverage while Wireframe AA is enabled and a hard width
  cutoff while it is disabled. The legacy GL_LINES fallback also follows the preference through GL_LINE_SMOOTH

## Viewport grid rendering and draw order

Primary implementation:
- `src/app/src/render/grid/viewport_grid_utils.*`
- `src/app/src/render/gl_view_render.cpp`

Behavior:
- rendering and transform snapping share `effectiveGridSpacing(...)`, including the adaptive 1/2/5/10 zoom steps
- transform drags derive their lattice from the same active origin and orthogonalized axes as the rendered fixed,
  rotated, or view-aligned grid; a top-down view-aligned grid therefore uses the XY lattice with a Z plane normal
- rendering, Move, and Cleave share one camera-footprint fallback order for adaptive spacing: a measured plane
  intersection, projection-derived scale, orthographic scale/height, then the deterministic `0.01` last resort. An
  edge-on or camera-pole view can no longer let one path silently retain the initializer spacing while another adapts
  to the visible grid
- the complete Map Settings spacing range is authoritative. The analytic shader does not clamp the supported
  `0.000001` minor spacing to a coarser visual lattice, and the CPU fallback uses 64-bit major-division indexing. If a
  minor lattice is too dense for its bounded fallback VBO, it omits only those unresolved minor lines and still draws
  the exact major lattice across the visible extent instead of a narrow centre strip
- grid draw order has three explicit modes. The default **Occluded** mode draws late with scene-depth testing, so
  unobstructed/inside fragments remain visible while solid level geometry and shaded Thing geometry hide fragments
  behind them. **Grid visible through walls** draws late without depth rejection, and **Grid always behind geometry**
  draws before scene geometry. The two override actions are mutually exclusive; disabling both restores the default
- grid opacity is persisted and adjustable from Map Settings and Preferences; it scales minor, major, and colored axis
  lines together, with an opaque centre at 100% and only the analytically antialiased outer pixel partially covered
- ordinary line thickness is persisted from `0.5` through `10` Qt logical pixels. The analytic and fallback renderers
  convert it to physical framebuffer pixels using the viewport DPR. Colored axes are independently limited to `4`
  logical pixels so maximum-width grids retain directional cues without becoming broad red/green bands
- minor and major grid-line colors are stored independently; major lines no longer rely on translucency alone for
  emphasis, so both colors remain distinguishable over textured geometry
- **Minor grid** selects Lines (the default) or Points. Points use the same exact minor lattice and minor color, with
  a separate Dot size diameter of `0.5`–`10` logical pixels, default `2`, converted through the viewport DPR. Major
  hierarchy levels and colored axes retain their line rendering; opacity and all three draw-order modes remain shared
- point mode projects a fixed `3 × 3` set of nearby minor-lattice intersections in the existing analytic fullscreen
  grid pass, using camera-relative phases and framebuffer-pixel radial coverage. Both grid directions participate in
  its footprint fade. For physical diameter `d`, the antialiasing support radius is `R = d / 2 + 0.5` pixels; dots fade
  between cell periods `max(4, R)` and `max(12, 4R)` pixels. This retains the minor-density floors while accounting for
  a dot's radial support rather than multiplying line thresholds by its diameter. Unresolved or foreshortened points
  disappear before becoming unstable. The normal path generates no per-point CPU geometry during camera movement.
  The fallback bounds the total point count before allocation and omits minor points across the complete footprint
  when over budget, retaining exact major and colored-axis lines
- minor style and diameter are persisted in app settings and the optional EDIT `gridAppearance` map used by native
  Save, autosave, and recovery. Viewport Settings applies them live; Preferences applies them only after OK, so Cancel
  leaves the viewport and persisted choices unchanged. Style changes invalidate only grid presentation; diameter
  changes repaint. Neither changes snapping, authored Level data, or the scene-mesh cache
- orthographic grid rendering reduces dense bright line buildup as the camera becomes nearly parallel to the grid plane
- repeated lines are filtered independently in each grid direction using the Euclidean `dFdx`/`dFdy` screen
  footprint. Derivatives decide only frequency/LOD visibility; they never define line coverage. Each fragment selects
  its nearest lattice line and measures the exact framebuffer-pixel distance to that line's homogeneous projection.
  Consequently a derivative quad crossing a perspective pole cannot expand an ordinary line into a broad wedge.
  Minor lines disappear before camera motion makes their phase unstable; major lines hand off to exact 10x, 100x, and 1000x
  subsets of the same lattice. This DCC-style hierarchy prevents a fixed dense scale from collecting into distant
  perspective Moire while resolved nearby lines keep their opaque centre and requested thickness. Footprint thresholds
  scale with the requested line thickness so thick lines hand off before their coverage merges into a distant band.
  Lower opacity can soften a grid but is not used as a substitute for frequency filtering
- perspective and orthographic views use the same user-opacity response at ordinary distances. The final far-clip fade
  is measured linearly along each reconstructed view ray rather than in non-linear depth-buffer space, so perspective
  no longer dims a front-facing grid merely because its normal depth value is numerically close to `1.0`
- grid line generation is kept stable around the camera/view origin so large world coordinates far from `(0, 0, 0)`
  do not produce visible jitter or brightness flicker during camera movement
- persisted/view-aligned grid axes are sanitized into a finite right-handed orthonormal frame before rendering or
  recovery. Malformed recovery axes fall back to the XY frame, and regular New/Open document transitions clear
  temporary center/alignment overrides; a valid SED editor-state restore keeps them.
- both repeated lines and colored axes use a one-pixel box-filtered coverage profile. Subpixel lines preserve their
  integrated opacity and have a continuous bounded phase response while moving between pixel centres instead of
  alternating between one opaque row and two dim rows in orthographic views; integer physical widths also preserve
  the exact sampled centroid
- a colored axis replaces the underlying grid RGB while retaining the maximum ordinary/axis alpha. This preserves
  ordinary shoulders when a `10`-pixel grid uses the independently capped `4`-pixel axis and avoids both bright
  source-over fringes and dark mask-then-source-over moats
- colored axes do not reconstruct zero from a camera-relative coordinate plus a large absolute offset. Each infinite
  world axis is projected to a normalized homogeneous screen-line equation on the CPU, then the fragment shader
  evaluates its exact framebuffer-pixel distance. This avoids the float-cancellation plateau that previously looked
  like axis glow. The rare `GL_LINES` fallback prefers real framebuffer MSAA and uses line smoothing only when the
  bound framebuffer has no samples. It restores incoming multisample, smoothing/hint, and line-width state before
  returning
- grid VBO updates use the shared dynamic-buffer grow/write path

Active caveat:
- side/near-parallel grid behavior is visually sensitive and should be checked in the real app after camera, projection,
  or depth-bias changes

## Query / find workflow

Primary implementation:
- `src/app/src/main_window/query/level_query.cpp`
- `src/app/src/ui/query/level_query_utils.*`

Behavior:
- `Edit -> Find...` and `Find Next` expose a Pascal-style level query workflow
- query kind follows current editor mode (`sector`, `surface`, `thing`) instead of a global last-used mode
- `Light` mode intentionally disables `Find`
- each query kind remembers its own criteria and result behavior
- fields choose their own operator families:
  - strings: `=`, `<>`, `Contains`, `Doesn't contain`, `Starts with`, `Ends with`, lexical `>` / `<`
  - numeric values: `Is`, `Is not`, `>`, `<`, and for integer-like fields also `Set` / `No set`
  - flags: `Set` / `No set`
- browse helpers convert editor-visible choices back into stored query values for resources, layers, flags, colors,
  and entity references
- the shared flag picker used by property and query browse actions filters live by bit index, decimal or hexadecimal
  mask, flag name, and description. Its **Bit** column is the single numeric-mask column; **Flag** displays only the
  human-readable name instead of repeating the hexadecimal value
- results can focus, add to selection, subtract from selection, or restrict matching to the current selection only
- material/resource browse buttons inside the query dialog no longer accidentally accept/close the whole dialog

Important detail:
- entity-reference queries compare against the same visible sector/surface/thing numbering the editor shows, not raw
  internal indices

## Consistency and resource checking

Primary implementation:
- `src/app/src/ui/checks/level_check_utils.*`
- `src/app/src/ui/checks/check_rebuild_utils.hpp`
- `src/app/src/main_window/checks.cpp`
- checker controls and searchable settings dialog: `src/app/src/main_window/checks.cpp`
- direct coverage: `src/app/tests/thing_utility_tests.cpp`,
  `src/app/tests/consistency_geometry_contract_tests.cpp`, and `src/app/tests/consistency_precision_dialog_tests.cpp`

Collection is split from presentation. `collectConsistencyCheckIssues(...)` and `collectResourceCheckIssues(...)`
return deterministic issue rows with severity and navigation metadata; `MainWindow::rebuildChecks()` only maps those
rows into the Consistency and Resources trees and updates their counters.

The active rule table covers the complete rule surface of Pascal `Cons_checker.Check` and `CheckResources`:
- sector metadata/topology: required non-IJIM colormap, at least four surfaces and vertices, and convexity for non-3DO
  sectors; undersized sectors retain the legacy early-out before per-surface work
- surface flags and shape: solid/translucent and solid geometry-mode mismatches, Floor without Collision, project
  maximums of 24 classic or 64 IJIM vertices, the three-vertex minimum, IJIM Track's four-vertex maximum, unit normal,
  planarity, and polygon convexity
- adjoins: reverse link, geometric overlap, and Visible on translucent or geometry-zero adjoins
- Things: sector assignment and position containment
- COG values: `.ai`, `.key`, `.mat`, `.wav`, and `.3do` extensions for the corresponding typed parameters
- resources: referenced MATs, non-IJIM master/sector CMPs, Sector WAVs, placed templates and COG scripts, plus the MotS
  one-CMP and JKDF2 three-CMP limits

Legacy severity is preserved where it affects triage: a missing/mismatched reverse adjoin and missing legacy-referenced
MAT/CMP/WAV/template/placed COG are Errors; non-planar and non-convex surface findings are Info. Inherited SPR/3DO
availability is an additional C++ warning rather than part of the legacy resource-error set.

The C++ checker also keeps defensive diagnostics that the pointer-based Pascal model could not express safely: invalid
surface vertex/adjoin indices, vertex/texture-vertex count mismatch, non-finite Thing positions, out-of-range Thing
sector indices, missing template names, underscore-prefixed base-template placement, and inherited SPR/3DO availability.
The unit-normal rule uses Pascal's exact `IsClose` boundary: absolute length error below `1e-4` is accepted and an error
at or above `1e-4` warns.

The Consistency settings dialog exposes **Geometry precision**, saved separately for JKDF2, MotS, and IJIM under
`checks/geometry_precision/<project-key>`. **Game-compatible** is the default and the fallback for invalid settings.
Its diagnostic predicates accept plane/support distances through `0.001` and convex-turn deviations through `0.0001`
(area units), plus arithmetic error bounds, matching the physical thresholds in Pascal `lev_utils.pas`.
Finite geometry, valid indices, nondegenerate faces, and consistent winding remain required. Portal comparison uses
reversed cyclic 3D boundary equivalence after redundant-corner reduction: identical rounded, slightly warped loops
can match without a coplanarity prerequisite. Unlike Pascal's proximity comparison, this retains arithmetic precision
so real gaps are not accepted. **Precise** uses the strict geometry predicates and displays a warning about rounding
diagnostics on original levels. Neither mode changes editing guards, snapping, or the unit-normal rule above.

Sector-pair overlap is deliberately separate because it is quadratic. The **Sector overlap** rule in the searchable
Consistency settings dialog defaults off like legacy `CheckOverlaps`; when enabled, the pass broad-phases AABBs,
skips directly adjoined pairs, applies the legacy two-way plane test with `0.01` tolerance, and emits one navigable Info
row for each sector in a detected pair.

Checker rebuilds are debounced for geometry, Thing, light, COG, and structure dirtiness. The shared
`checksDirtyIsRelevant(...)` helper in `check_rebuild_utils.hpp` owns that mask and includes `DocumentDirty::Cogs`, so
typed COG-parameter edits refresh their extension diagnostics without requiring an unrelated geometry edit. Focused
source tests cover sector early-outs, Thing placement and a valid-sector
underscore-base-template fixture, both sides of the `1e-4` normal boundary, matching/mismatching adjoins, default-off
and enabled overlap behavior, typed/empty COG values, resource misses, and project CMP limits. These checks are part of
the registered Release regression suite.

## NDY/JKL world-capacity export

NDY/JKL `World ...` headers have two different meanings. Geometry, colormap, Sector, editor Light/Layer, and MotS
ARCHLIGHTING headers are exact record counts; increasing one without writing records corrupts parsing. Resource lists,
COGs, templates, and Things are capacity pools: the engine allocates the declared size and then consumes the records
present before `END`. Additional slots follow the target reader's rules; IJIM Materials has the specific restriction below.

Import rejects malformed/negative exact WORLD/SECTORS counts, truncated Sector records, extra `SECTOR` records
beyond the declared count within the SECTORS section, a missing required `SURFACES` field, and invalid/out-of-range
world-vertex references rather than publishing incomplete geometry. Comments and unknown extension fields remain
ignorable, and records in subsequent sections do not count as extra Sectors. The
legacy Pascal fallback for an invalid texture-vertex index remains separate: its UV uses the established zero fallback.
Resource capacity headers continue to admit their intentional spare slots.

Capacity is now literal and inspectable: every resource pool writes `required records + Extra`. There is no second
non-editable floor or fallback, so an Extra value of zero adds no spare slots. New projects initialize the editable
fields from the original SED headroom (IJIM Sounds 80 and Materials 64; AI 64; Models 1024; Sprites 128; Keyframes
512; Puppets 64; Soundclasses 128; COG scripts 128; Things 256), while JKDF2/MotS Sounds and Materials plus all
COG instances and Templates start at zero. In particular, Pascal used 1024 as its model new-project allowance for all
projects, including IJIM.

| Extra pool | JKDF2 | MotS | IJIM |
|---|---:|---:|---:|
| Sounds | 0 | 0 | 80 |
| Materials | 0 | 0 | 64 |
| AI classes | 64 | 64 | 64 |
| Models / 3DO | 1024 | 1024 | 1024 |
| Sprites | 128 | 128 | 128 |
| Keyframes | 512 | 512 | 512 |
| Puppets | 64 | 64 | 64 |
| Sound classes | 128 | 128 | 128 |
| COG scripts | 128 | 128 | 128 |
| COG instances | 0 | 0 | 0 |
| Templates | 0 | 0 | 0 |
| Things | 256 | 256 | 256 |

These are editor New Project defaults. IJIM NDY export requires Materials Extra to be exactly 0 or 64: the declared
material capacity is the required dynamic MAT row count plus that allowance. This matches the original retail reader
constraint and Pascal's 64-slot default. OpenJones3D's removed reader check does not widen the original retail
format contract. Zero adds no reserve, and zero total capacity is not rejected solely because it is zero. Other
Materials Extra values remain authored data on import, native Save/Open, and report-only measurement, but export
rejects them with a message pointing to Level Settings > Extra Buffers. No value is silently clamped.

Other imported Extra values are retained exactly and displayed for inspection; valid values are used on export.
Preferences > Resources > New Project Extra Buffers can override each field independently for JKDF2, MotS,
and IJIM; those preferences affect only subsequently created projects. Restore Defaults in that Preferences section
returns to this table, while the Level Settings button restores the currently selected project's configured defaults.
The table reserves enough height for every resource row and does not add a second vertical scrollbar inside the
Preferences page scroll area.

The Level Settings **Extra buffers** dialog runs this same dependency crawler in report-only mode, stopping before
geometry serialization. Every pool displays Required, editable Extra, and Export total. Restore Defaults reinstates
the project defaults above; users may then set any field to zero. If a dependency cannot be inspected, the dialog
warns that Required may be incomplete but does not invent hidden capacity.
The dialog takes immutable Level, template, and resource snapshots, presents
its controls immediately, and runs the dependency crawl through QtConcurrent;
acceptance remains disabled until the report is complete, so a large resource
tree cannot make the Level Settings button appear unresponsive.

COG script resources and COG instances must not share one count. Script filenames are deduplicated for the
`COGSCRIPTS` list, but `sithThing_ParseArg` calls `sithCog_Load` for every explicit nonempty/non-`none` `cog=` token.
The required COG-instance count is therefore:

```text
placed COG rows
+ explicit cog= occurrences on every emitted template
+ explicit local cog= occurrences on every placed Thing
```

Repeated filenames still consume separate instances. A COG inherited only by copying a parent template pointer does
not allocate again. COG-typed symbols inside a COG script are existing instance indices and are not script-load
requests. This fixes the original Pascal exporter's `placed COGs + unique script filenames` proxy, which underallocated
`Untitled_test.ndy` as three slots for four template occurrences.

Editor projects persist extras in the optional JED `SEC: EXPORTBUFFERS` section. Opening an NDY/JKL infers each value
as `declared capacity - parsed required records`, so a deliberately small, zero, or large allowance survives a
native load/save round trip exactly, including an IJIM Materials value that must be repaired before runtime export.

### JKL/NDY presentation options

The JKL/NDY Export and Export As actions first use Qt's native `QFileDialog` for
path selection. After a matching runtime path is accepted, a separate modal Qt
`QDialog` presents Include lights, Include layers, Compact output, Clamp extra
light, Weld nearby vertices, and Max distance. The options dialog has only its
title and close controls in the window frame, plus explicit **Export** and
**Cancel** buttons. It is fixed-size after its controls are laid out, hides
minimize and maximize controls, stacks the four independent options vertically,
and keeps Weld nearby vertices with Max distance as the only horizontal control
pair. The native chooser keeps its standard path, filter,
overwrite, and recent-location behavior. Ordinary Open/Save actions (including
native `.sed` Save As) are unchanged.

Open and Save use one remembered filesystem folder, so saving to a new location also changes the next Open starting
folder. Import and Export keep separate folder histories after their first accepted selection. Until one of those
groups has a value, it falls back to the most recently used Open/Save, Import, or Export folder. Suggested Save/Export
filenames are preserved while only their parent directory is replaced.

JKDF2/MotS may still select legacy `.jed` from this export picker. Runtime-only
controls are not shown for a resulting `.jed`; an explicitly typed `.jkl` path
remains authoritative. IJIM uses `.ndy` in the same way. An incompatible
runtime suffix leaves the remembered options unchanged and is rejected by the
writer. Closing or cancelling either chooser/dialog returns an empty target and
does not commit export settings or the export folder history. Accepted runtime
choices commit the options only after the separate dialog accepts them.

The fresh picker defaults are `Include lights` off, `Include layers` off,
`Compact output` on, `Clamp extra light` on, `Weld nearby vertices` off, and
`Max distance` at `0.00001`. The distance editor is disabled until welding is
checked and displays enough decimal places for the default. Clamp extra light
limits classic scalar surface intensity after averaging and each IJIM RGB
surface channel to `[-1, 1]`; clearing it preserves authored values outside
that range while alpha keeps its existing wire clamp. When welding is enabled,
`Max distance` is the maximum Euclidean world-space distance for joining
vertices. The export snapshot normalizes finite values to at least
`0.00000001`; non-finite input uses `0.00001`.
Accepted runtime choices, including Clamp extra light, welding, and distance,
remain available to repeat Export during the current editor session. The
options are captured with the immutable export Level snapshot before the
asynchronous writer starts; these controls are explicit runtime-export options
and do not affect native SED Save or recovery.

Clearing either optional-section control omits that entire editor-owned section
(including its header/count and, for Layers, the visibility string), without
changing engine geometry, resource, Thing, or COG counts. The loader already
treats these editor sections as optional, so the resulting runtime file remains
parseable and simply has no editor records for the omitted part.

The exact control label is **Compact output**. Its tooltip says that it reduces
file size without changing level content. Compact output keeps
the copyright block, uses LF line endings, converts field separators to single
tabs, removes padded template columns, and shortens finite decimal floats
(`0.01500000` becomes `.015`, `-0.01500000` becomes `-.015`). Quoted text is
left untouched. This is a presentation-only transform over the same writer
output, so capacities, indices, and parsed values are unchanged; the same
JKL/NDY parser is used for round-trip validation.

## UV editor display-state rebuilds

Primary implementation:
- `src/app/src/main_window/uv/editor.cpp`
- `src/app/src/ui/uv/uv_*_utils.*`

Behavior:
- showing/hiding the docked UV editor uses one short S-curve animation. First presentation constrains the hidden panel
  and starts its splitter extent at one pixel, instead of letting a size hint produce a visible pre-animation jump;
  redundant presentation requests do not restart the transition
- UV **Sub-grid** can draw Lines or Points with a separate `0.5`–`10` logical-pixel point diameter, default `2`.
  These controls affect canvas presentation only, independently of UV snapping and repeated-tile borders. App settings
  retain the style and diameter under `uv_editor/grid_minor_dots` and `uv_editor/grid_minor_dot_size_px`; EDIT retains
  them in the optional `window.uvGridAppearance` map. Older workspaces restore Lines and `2` pixels, regardless of
  current app preferences. Invalid known map values reject workspace restoration before mutation.
- viewport zoom compensation runs incrementally when the UV editor appears, hides, or is resized. It follows the
  camera projection (fixed-horizontal-FOV perspective and the editor camera's aspect-aware orthographic bounds), so
  projected world/object scale stays stable without restoring a stale camera snapshot; the panel only crops or
  reveals the scene along the axis whose usable viewport extent changes. Hide rebases its compensation from the live
  viewport after ordinary window/splitter resizing, and viewport-facing winding refresh is suspended while the panel
  animation owns the viewport size
- real UV mutations (modal commit, auto align, stitch, copy-ref, undo/redo) force immediate panel sync and can force
  rebuild from stored UVs so stale display transforms do not linger
- active selected surface is pushed back into the UV editor as the active island after rebuild
- reference-based UV actions prefer the active island instead of arbitrary selection order
- rebuild-state reuse is deliberately strict so stale display transforms do not survive a rebuild
- modal `G`/`S`/`R` transforms guard no-op commits, cancel on panel hide/focus loss, block wheel zoom while active, and
  clean up rectangle/pointer state more aggressively
- UV canvas zoom keeps its ordinary 48 default and 8 minimum. Its default maximum is 48,000 px per UV unit so tiny
  islands remain usable; **Preferences > Viewport > Grid > UV editor maximum zoom** persists a bounded 480..480,000
  maximum. Wheel zoom preserves the cursor anchor; fit-to-content and recovered workspace state use the same configured
  maximum, while an applied lower maximum preserves the canvas center. Vertex snap keeps its 14-pixel radius in UV
  units without a large-zoom floor.
- modal mouse previews mutate the working UV values but use the retained
  source-Sector/Surface/texture-vertex mapping to patch only affected `u/v`
  fields in the cached CPU mesh and GPU VBO. They do not rebuild level
  geometry or selection overlays per pointer event, and do not publish a
  camera/view-state change (which would retrigger View Face and recovery work).
  Cancel restores the
  baseline without an undo record; a nonzero commit creates exactly one record
- bare UV `G`/`S`/`R`/`A` shortcuts are owned only while the pointer is over the visible UV canvas. They are not
  forwarded from the controls area, while another modal dialog is active, or while an editable text/spin control has
  focus. Escape/Enter may still finish an already-active UV transform after the pointer leaves the canvas
- component-mode selection masks and active island normalization were tightened for multiselect editing
- point picking collects every UV candidate under one pixel position in deterministic topmost/distance order. Repeated
  clicks cycle overlapping faces, edges, and vertices; face cycles wrap directly, while edge/vertex cycles include a
  terminal no-primary step which preserves the component selection. The exact active edge/vertex is painted in a
  final red overlay pass so a later overlapping island cannot obscure it
- the exact active vertex or active-edge midpoint is the transform and grid-snap pivot. With no exact primary,
  transforms retain the selected-point center fallback. Face mode can retain a vertex pivot without writing component
  masks; clicking that face's centroid clears the explicit pivot, and pivot-only face clicks preserve the cached
  component selection used when returning to Edge/Vertex mode
- Smart UV Project is now based on Blender's projection-normal clustering rather than transitive adjacency merging:
  selected faces are area-sorted, faces within half the angle limit contribute to each optionally area-weighted
  projection normal, the most angularly distinct remaining face seeds the next group, and every face is assigned to
  its closest generated normal. Adjacency is used only afterward to keep compatible shared-edge faces in one island,
  so a 0/40/80-degree chain cannot leak through a 66-degree limit as one projection
- the Smart UV dialog exposes the matching useful operator surface: Angle Limit, Margin Method, Rotation Method,
  Island Margin, Area Weight, Correct Aspect, and Scale to Bounds. Projection and packing defaults follow Blender
  (66 degrees, Scaled, zero margin/area weight, aspect on, bounds scaling off); SED defaults rotation to the
  architecture-safe **Geometry (World Vertical)** option described below. **Island Scale** is an intentional
  SED extension: before the dialog opens, a unit-scale dry run compares corresponding current/projected UV edge
  lengths and uses their median ratio as the initial multiplier. The accepted multiplier uniformly scales every
  final connected island and repacks expanded islands without overlap; values above one may deliberately extend
  beyond 0..1 so repeating level textures retain useful density
- Correct Aspect uses one coherent active-face texture aspect for the complete selection, as Blender does. A wide
  image shrinks normalized U by height/width; it no longer applies the inverse width/height expansion. When selected
  materials differ, per-face aspect transforms are deliberately not mixed inside connected islands
- projected components receive a deterministic compact axis-aligned shelf pack. The grouping/projection controls and
  semantics follow Blender, but the final polygon packing is an intentional approximation rather than a copy of
  Blender's internal packer
- Blender's Horizontal/Vertical rotation methods describe the fitted island bounding box; they do not mean world X/Z
  texture orientation. SED keeps those meanings for parity and adds **Geometry (World Vertical, Recommended)** for
  architecture. Each connected component derives its positive UV-V direction from its genuinely upward geometry
  edges after texture-aspect correction. When no useful vertical edge exists, positive world Z is projected into the
  group's plane. Horizontal faces, where world up has no stable in-plane direction, fall back to Blender-style
  minimum-box rotation. Projection/group normals now
  come from evaluated polygon vertices, like Blender mesh normals, rather than possibly stale serialized Surface
  normals; the world-up direction also receives the same texture-aspect correction as projected points. A component
  with a coherent authored vertical UV direction preserves that existing positive/negative V sign, preventing Smart
  Project from introducing an otherwise arbitrary 180-degree texture flip. This also prevents a slightly
  backward-banked wall from crossing a stale dominant-axis boundary and rotating by 90 degrees. The
  Blender-compatible Axis-aligned Vertical option still only makes the fitted bounds tall; it does not mean world-up
- Smart UV Project returns/reports failure atomically instead of silently committing empty or partially invalid output
- **Auto Align** and both Stitch commands retain the original SED/JED UV-basis workflow. Auto Align mirrors Pascal
  `TJKSurface.RecalcAll`: it recalculates the geometric normal, recovers the authored U/V frame from the existing UVs,
  and rearranges the polygon around its first UV anchor. It does not force U onto each face's local edge zero; adjacent
  faces whose vertex loops begin on different world-space edges therefore keep their orientation and shared-edge UVs.
  Reconstructed coordinates are calculated without fixed stored-unit rounding, then checked for finite,
  float-representable results before publication, so equivalent pixel and normalized Levels remain equivalent for
  anisotropic materials.
  Invalid/incomplete UV geometry is rejected atomically instead of being resized into synthesized zero coordinates.
  Stitch validates complete source/target geometry and texture vertices before computing into a temporary target, so malformed data cannot be
  partially modified while reporting success. A complete shared world-space edge is the authoritative hinge for
  transporting the source basis onto angled/banked targets; this prevents rounded stored normals from drifting the UV
  seam. The original common-vertex/normal-intersection behavior remains the fallback for disconnected surfaces and
  opposite coplanar portal faces. Stitch recovers mirrored axis signs from the authored UV gradients before applying
  retained scale/flip state; its orthogonal frame and target density policy remain unchanged. Collinear source
  geometry cannot supply a projection basis, while redundant leading collinear corners may use a later valid triangle.
  Shared non-coplanar edges, oblique rounded-normal hinges, disconnected legacy
  anchoring, scale/flip, malformed inputs, Smart grouping/aspect/margins, and the dialog defaults have focused coverage
- **Pack** and Smart Project are modern Blender-inspired tools; **Copy Ref** copies material/scale/flip and uses the
  legacy stitch basis when possible. Copy requires complete finite UV/scales and valid finite referenced geometry;
  its fallback copies only U/V and preserves target baked corner colors. **Stitch Ref** applies one active reference
  to every selected target, while **Stitch Chain** propagates only through selected surfaces sharing a world-space
  edge or an adjoin whose full boundaries coincide, including collinear subdivisions. A stale adjoin across a gap
  does not establish connectivity
- Every actionable UV panel/header control and Surface-menu UV command has a concise tooltip. Action-backed panel
  buttons source that text from their shared `QAction`, including later action changes, so the menu and panel cannot
  describe different behavior. In particular, **Pack** says that it repositions selected islands without changing
  shape/rotation/scale, while **Fit** says that it only frames all islands in the UV view and does not modify stored UVs
- UV transform snapping first prefers compatible nearby unselected UV edges in Edge mode, then nearby vertices on
  wholly unselected islands, then an unselected-island center when a complete island moves. An island with any moving
  vertex contributes no vertex, edge, or center targets, so partial component moves cannot self-snap to their owner.
  Edge acquisition considers both endpoint orders, requires orientation within five degrees, and rejects endpoint
  residual which a translation cannot remove. Exact zero-delta geometry acquisition is still a successful snap and
  suppresses grid fallback. Otherwise every snapped move receives the shortest two-axis delta to a visible grid point;
  it cannot retain a free unsnapped position. **Grid size** owns the main lattice. The default-off **Sub-grid** draws and
  permits four subdivisions per main cell, and the fallback chooses the nearer permitted main/sub point. There is no
  third or implicit texture snap lattice
- repeated texture rendering and repeated-tile separators are independent state. **Repeat** continues to draw repeated
  texture images, while the default-off **Tile grid** gates only their white one-UV boundary pass. **1 UV** sets the
  main grid spacing to `1.0`; the optional separator state is recovered compatibly, with older payloads defaulting off
- Vertex and Edge modes draw one aggregate selected-point center ring. Clicking it clears only the exact active
  component and pick-cycle state, preserving the component masks and returning the transform pivot to that center
- UV main/sub colors are loaded from `uv_editor/colors/grid_main` and `uv_editor/colors/grid_sub`, exposed in
  Preferences > Viewport > Appearance > Theme, and republished to the live UV canvas without rebuilding level geometry
- the Transform row's **Scale ×** field accepts an arbitrary uniform multiplier from `0.01` through `1000` and
  commits one UV edit through **Apply**. It fully subsumes the removed fixed `×0.5` and `×2` controls and uses the
  same exact-component/face/selection-center pivot precedence as modal scaling
- Blender behavior is documented against these pinned source revisions:
  [`uvedit_unwrap_ops.cc`](https://github.com/blender/blender/blob/76d2e97a2f49f49e429629623fc8d20a7615ff54/source/blender/editors/uvedit/uvedit_unwrap_ops.cc#L3122-L3480)
  owns Smart projection-normal grouping and dominant-axis projection, while
  [`uv_pack.cc`](https://github.com/blender/blender/blob/76d2e97a2f49f49e429629623fc8d20a7615ff54/source/blender/geometry/intern/uv_pack.cc#L213-L306)
  owns minimum-AABB fitting and Horizontal/Vertical stand-up behavior. The separate Blender
  [`Follow Active Quads`](https://github.com/blender/blender/blob/76d2e97a2f49f49e429629623fc8d20a7615ff54/scripts/startup/bl_operators/uvcalc_follow_active.py)
  requires a selected active quad and manifold, non-seam quad loops. SED now provides a distinct **Follow Active
  Quads** tool: it preserves the active quad exactly, walks only selected quad faces breadth-first across one
  unambiguous selected neighbor per tolerance-matched world edge, and exposes Blender's Even, Length, and Length
  Average spacing. Extra unselected portal/duplicate owners therefore cannot block a valid selected strip. Branches
  and cycles use stable face/edge order so the first discovered path owns a face. Length Average indexes complete
  level quad edge loops, including unselected quads. Invalid UVs and unreachable or ambiguous selected topology fail
  without changing UVs; one successful operation is one undo step. If the panel retained a selected non-quad display anchor,
  non-quads remain selected but are ignored and the first selected quad with a compatible neighbor becomes the
  operation root; an isolated earlier quad cannot hide a valid later strip. Length Average still uses a canonical
  complete-level edge-loop index. SED has no seam-edge metadata, so authored mesh seams cannot stop traversal; a
  selected world-space discontinuity is not treated as an implicit seam
  (see the [Blender 5.0 UV operator manual](https://docs.blender.org/manual/en/5.0/modeling/meshes/editing/uv.html#smart-uv-project))
- raw stored texture space remains the default. The optional **View Face** mode compares the active selected face's
  stored UV polygon with its current viewport projection and applies one global rigid 2D rotation plus the required
  handedness reflection to the whole canvas: texture, grid, every selected island, hit testing, and edit directions
  transform together. The longest stable stored active-face edge provides a camera-stable orientation basis
- one active face controls that shared orientation even for a multi-surface selection; islands are never mirrored or
  rotated independently. Stored UVs are unchanged. When the active face is behind the camera, near edge-on, or
  projects to a degenerate area, the prior whole-view orientation is retained instead of flickering
- Surface Extrude keeps exact UV/RGBA tuples on a plain translated cap. For expanded caps, matching authored corners
  retain their exact tuples; new inner and outer coordinates extend the source mapping from the largest-area
  well-conditioned source triangle anchored at source corner zero. This is a deterministic extension for non-affine
  authored UVs, while generated side walls retain their geometric mapping. If the extension is not finite or cannot be
  represented as floats, geometric mapping remains and only matching authored corners are restored.
- deterministic utility coverage uses selected islands with opposing stored windings, including one island already
  carrying a display mirror/offset, plus a 90-degree projection and reflected/rotated round trip. A real MainWindow
  fixture verifies the action produces a visible 90-degree canvas change; unstable projection retains the previous
  whole-canvas transform

Active caveat:
- **View Face** is a rigid 2D orientation aid, not a perspective projection: it rotates and reflects but does not
  foreshorten individual UV geometry. Disabling it returns to unmodified stored texture-space presentation

## COG diagnostics and source marking

Primary implementation:
- `src/app/src/main_window/cog/editor.cpp`
- `src/app/src/main_window/cog/scripts.cpp`
- `src/app/src/ui/cog/*`

Behavior:
- COG diagnostics are computed structurally from the editor text (sections, handlers, symbol table, simple statement
  termination, and explicit multiline-continuation checks)
- authored text changes restart one short single-shot analysis timer. When it expires, Outline, Diagnostics, and
  selected-identifier ranges reuse one source snapshot; formatting and rapid typing therefore avoid duplicate
  synchronous whole-document scans
- the horizontal source/Outline splitter gives the shared Symbols/Code pane a 260-pixel child minimum in addition to
  disabling collapse, so Qt cannot reduce the outline to a handle-width sliver. The source editor separately propagates
  the complete Find-control minimum through the containing splitter
- zoom and Find occupy one reserved source-editor strip without resizing source text. A dynamic Find-visible property
  owns the strip background and lower accent: hiding Find clears both while leaving zoom visible, and showing or
  recovery-restoring Find re-polishes the property selector immediately
- the bottom diagnostics pane uses one full-width animated disclosure header with inline error/warning counts; it
  lists line/column locations, copies complete rendered messages, jumps to exact source ranges, and persists only its
  completed collapsed state and splitter height
- editor marks diagnosed source with wave underlines in `code`, `flags`, and `symbols`
- error markings are red; warning markings are yellow
- hover over marked source shows the matching diagnostic text
- string/comment stripping respects quotes/comments so the verifier is less aggressive than the early port
- a closing brace may be followed immediately by the engine-valid `if` keyword, as in `}if (condition)`; arbitrary
  identifier text after `}` remains diagnosed
- formatting removes whitespace-only blank indentation and trailing line whitespace, aligns handler bodies outside
  braces, and optionally removes file/section/brace-boundary blanks while collapsing repeated interior blanks to one.
  Standalone comments keep authored indentation unless the explicit **Indent standalone comments** policy is enabled;
  the editor aligns typed, split, or Enter-terminated closing braces with their matching opener, including compressed
  one-line scopes
- formatter section transitions require a complete `symbols`, `code`, or `end` structural line after comment removal;
  malformed lines with trailing authored tokens remain ordinary source and are never truncated by format-on-save
- syntax highlighting is synchronously reapplied after SED/recovery text restoration; `if` keeps keyword color with
  or without a condition, while `symbols`/its `end` and `code`/its `end` use colors distinct from types and ordinary
  code keywords. The default product-owned **SED Dark** theme gives control-flow keywords a rose tone, types a blue tone, local and non-local
  symbols separate tones, `global0` through `global15` a macro-like tone, function calls a color distinct from message
  handlers, and matching declared-symbol/code-label pairs one non-white label color. Comments remain green and nested
  braces use a repeating blue/yellow/purple cycle that ignores strings and comments
- symbol initializer right-hand sides are literal resource values, even when their spelling matches a declared
  symbol; quoted values use string color and wholly numeric initializers retain number color. A local, line-only span
  parser exposes the helper for `material`, `sound`, `template`, `model`, `ai`, and `keyframe` declarations, including
  declarations without an initializer. Hover lookup uses the active-section state cached on the hovered text block and
  cached section/line-local token spans, so pointer motion does not scan the full document. The resource ellipsis sits
  to the right of the visible initializer in the editor viewport and follows scrolling and presentation zoom; it is
  hidden when that anchor is outside the viewport. Choosing a resource replaces only the initializer, or inserts the
  exact ` = value` after the symbol name when no initializer exists, preserving other attributes, whitespace, and
  comments. Choices use the current project resource snapshot and template/CMP context
- mask editing targets the exact `mask` attribute token at the hovered source span. Token lookup excludes quoted
  resource/description values, comments, and code, and duplicate attributes remain independently addressable. The
  engine `-1` all-source sentinel and prefixed hexadecimal aliases such as `0xffff` and `0xffffffff` are shown as all
  32 bits set; accepted aliases retain their original spelling until a bit changes. Unrepresented 32-bit mask values
  survive edits, and changed values use canonical
  hexadecimal text. Flags and mask popups anchor to the global position carried by the delivered click or context-menu
  event; invocations without a pointer event fall back to the helper button. The popup is clamped to the available
  screen. Before applying their result, the editor rechecks document identity, revision, source block/span, and
  read-only state; stale buffers discard the result without restoring stale selection or scroll state. Cancel and
  unchanged choices create no dirty or undo entry, while an accepted change is a single undo step
- semantic highlighting scans declarations/labels into a debounced document cache. The real code editor's stable
  `textChanged` path reports the edited cursor block to the highlighter, which checks it against cached declaration/label
  block numbers and current/adjacent line syntax in O(1); block-count changes and external full-document replacements
  remain conservative. Ordinary statement edits therefore keep Qt's incremental block highlighting, while a semantic
  edit performs one full rehighlight only when the resulting name sets actually change. Selecting a complete identifier
  clears old marks immediately, then shares the editor's debounced source snapshot for a case-insensitive lexical pass
  that excludes quoted strings, `#` comments, and `//` comments
- operators and punctuation have their own neutral tone; number, string, comment, brace, and semantic-name passes
  deliberately override that base where token ranges overlap
- Preferences configure an ordered monospace family fallback list, base point size, bold, and italic. Each tab applies
  its recovered 20%-400% presentation zoom from that base font, never cumulatively and never by rewriting source text
- symbol-attribute removal/de-duplication edits only the selected token and adjacent delimiter, preserving exact leading
  indentation, quoted attribute contents, trailing whitespace, and comment spacing
- diagnostic label checks follow the editor/compiler authoring contract: non-message code labels are expected to have
  an int/flex/float symbol declaration, while built-in message handlers and existing call/goto target checks remain separate.
  A missing backing declaration and a redundant symbol-line semicolon are warnings, not hard syntax errors. The
  semicolon check analyzes string/comment-sanitized source while retaining original columns, so quoted values and
  trailing comments containing `;` are not mistaken for declaration terminators
- editor viewport routes RMB through the custom COG context menu instead of the stock Qt text menu
- `edit.find` and `edit.find_next` are the sole application `Ctrl+F`/`Ctrl+G` actions. Their handlers inspect the
  actually focused widget: COG children search the owning script; all other focus retains level-query behavior. No COG
  Find Next action registers `F3`, which remains Grid to View
- format, completion, reload, go-to-definition, whitespace, and previous-find shortcuts are likewise owned by keymap
  `QAction`s and are enabled only while a COG tab or one of its children owns focus. General Edit-action refreshes
  publish viewport Find availability as separate base state, then reapply the current focus route, so neither a mode
  change nor a broad UI refresh can enable COG shortcuts in the viewport or restore stale Find state
- context-aware insert actions expose direct `Message`, `Code section...`, and `Symbol attribute` actions
- app close now checks dirty open COG tabs too

## Layer lifecycle, visibility, and lock gating

Primary implementation:
- `src/core/include/sed/level.hpp`
- `src/core/src/level.cpp`
- `src/app/src/main_window/menus/map_menu.cpp`
- `src/app/src/main_window/layers/layers.cpp`
- `src/app/src/main_window/selection/context.cpp`
- `src/app/src/main_window/properties/panel.cpp`
- `src/app/src/ui/layers/layer_color_utils.*`

Rules:
- layer arrays are normalized to at least one layer and extended to match level data
- malformed/sparse imported indices are normalized in the same Undo transaction as the accepted Layer Manager edit;
  opening or cancelling the dialog never silently mutates the document
- rows can be drag-reordered or moved with **Up/Down**; Sector, Thing, and baked-light indices are atomically remapped by
  `Level::remapLayers()` so each object remains on the same semantic row
- **Delete Layer** refuses any layer used by a Sector, Thing, or light; **Compact Unused** removes only zero-owner rows
  and always retains at least one layer
- layer names remain index-based and need not be unique
- visibility, transient lock, and editor color values travel with their rows across reorder and compaction; new rows use
  visible/unlocked/default-color state
- the accepted layout and its parallel visual rows share one history transition: production **Undo** restores the
  previous visibility/lock/color identities, **Redo** restores the reordered identities, and either path republishes
  the restored color order to settings rather than leaving restart-time indices stale
- hidden layers are filtered before viewport draw, with preview filtering controlled by preference. A retained
  selection on a hidden layer is read-only just like a locked-layer selection
- one centralized selection traversal resolves Sector owners for Surface/vertex selections, Thing owners for Frame
  selections, and direct Sector/Thing/light layers. Mutation actions, properties, UV tools, transform gestures,
  extrude, Delete, and viewport click-tools all consume its hidden-or-locked editability result
- read-only selection does not make the viewport controls disappear. The edit-mode strip, Select-capable transform
  strip, and applicable left tool strip remain in place; mutating controls are disabled and an amber lock badge names
  **Hidden layer**, **Locked layer**, or **Hidden + locked**. Entering this state cancels placement/click tools, switches
  Move/Rotate/Scale to Select, and clears their gizmos; making the layer editable does not silently restore the old
  transform tool. Item Edit remains visible but disables every field, Thing parameter, and COG link except Layer, so
  Sector/Surface-owner, Thing/Frame-owner, and baked-light selections can be moved to an editable layer
- Item Edit applies that policy recursively to rows and embedded editors after every rebuild and action refresh.
  Returning from Game Preview therefore re-enables an entity on a visible, unlocked layer even when Preview rebuilt
  its controls while read-only. Single-layer and **Show All** visibility changes use the same mesh-publication progress
  path, whose indeterminate status indicator appears after 500 ms and is removed when publication finishes

## Dynamic sprite/render upload strategy

Primary implementation:
- `src/app/src/render/gl_view_mesh.cpp`
- `src/app/src/render/gl_view_upload.cpp`
- `src/app/src/render/gl_view_render.cpp`
- `src/app/src/render/gl_view_picking.cpp`
- `src/app/src/render/gl_view_fast_update.cpp`
- `src/app/src/render/translucency/translucent_order_utils.hpp`
- helpers under `src/app/src/render/upload/`, `src/app/src/render/mesh/`, and
  `src/app/src/render/fast_update/`

Behavior:
- the sprite path was simplified back toward the older stable behavior:
  - JKDF2 and MotS SPR types admitted by the engine loader (`0`, `1`, `2`) all use the classic camera-facing card;
    the stored token remains unchanged. The classic renderer does not branch on this token. XYZ offsets are added
    in camera right/forward/up space, so Y changes depth rather than lateral position in a straight-on orthographic view
  - every type-0 Game Preview sprite uses the inverse-view camera basis even when its Thing has nonzero pitch/yaw/roll;
    Editor mode may still show the authored transform as an authoring aid
  - IJIM type-2 uses the Thing forward axis, normalized `cross(+Z, forward)` (falling back to `cross(+X, forward)` near the
    pole), then `cross(right, forward)` for its up axis; SPR offsets are not applied to type-2
  - raw quad submission
  - mesh/resource/transform mutations mark sprite vertices dirty; camera motion does so only for type-0 Game Preview
    billboards, because editor-authored and type-2 bases are camera-independent
- this policy is pinned to OpenJones3D `develop` commit `b9c0eaa56bb51307b0787add4b614d9468e5d372`:
  [`rdSprite_Draw`](https://github.com/smlu/OpenJones3D/blob/b9c0eaa56bb51307b0787add4b614d9468e5d372/Libs/rdroid/Primitives/rdSprite.c#L158-L321)
  constructs type-0 in view space and type-2 from `orient->lvec`;
  [`sithSprite_Initialize`](https://github.com/smlu/OpenJones3D/blob/b9c0eaa56bb51307b0787add4b614d9468e5d372/Libs/sith/World/sithSprite.c#L411-L423)
  copies authored Thing roll into the separate sprite roll value
- classic admission and camera-space offsets are audited against OpenJKDF2 commit
  `2c21bcc0531f2ffb0cfa790c0d2262a9102545a8`:
  [`sithSprite_Load`](https://github.com/shinyquagsire23/OpenJKDF2/blob/2c21bcc0531f2ffb0cfa790c0d2262a9102545a8/src/World/sithSprite.c#L123-L143)
  admits unsigned type values through `2` and positive dimensions;
  [`rdSprite_Draw`](https://github.com/shinyquagsire23/OpenJKDF2/blob/2c21bcc0531f2ffb0cfa790c0d2262a9102545a8/src/Primitives/rdSprite.c#L124-L169)
  has one camera-space card construction regardless of the admitted type
- the shaded type-2 path uses one directly tested source-shaped frame helper. Tests pin normalized `+Z x forward`,
  both sides of the near-pole threshold, the `+X x forward` fallback at both vertical poles, and a finite fallback for
  malformed input; real-asset texture
  vertical orientation remains a pixel-comparison task
- dynamic VBO uploads for sprites, overlays, gizmos, grid lines, and transient translucent scratch data use
  grow-and-write capacity tracking instead of repeated reallocations
- oversized or zero-byte uploads invalidate only the affected draw metadata instead of aborting unrelated uploads
- persistent SPR geometry is indexed by Thing pick ID. Translation, rotation, owner relighting, and an owner's IJIM
  Sector point-light transition regenerate only that owner's cached six-vertex card, world bounds, and merged dirty
  VBO range. Each instance also owns a cached AABB; an interior owner updates its shared material batch in O(changed
  sprites), while moving a current min/max owner performs one exact batch-local rescan. Broad lighting publication
  that can affect unrelated owners still takes the conservative full sprite rebuild
- Thing Visual Bounds use the same generated card vertices for fixed Editor type-0 and type-2 sprites, then draw
  their world-aligned AABB. A type-0 Game Preview billboard keeps a camera-independent conservative cube because its
  card orientation changes with orbit; that visual-bounds cache is not rebuilt on every camera frame
- independent Thing overlays draw amber Visual Bounds before the thicker green collision wire, preserving the existing
  opaque-scene depth test so the stronger collision cue does not appear through walls. A selected Thing gets a brighter,
  wider outline on each active overlay; the pass remains depth-tested and obeys that overlay's visibility toggle
- collision and Visual Bounds line ranges retain the owning Thing pick identity. Viewport Move/Rotate and Item Edit
  transform previews use the existing per-Thing GPU matrix to move those ranges with the visual Thing on every sample.
  GPU pick passes omit overlay ranges; authoritative Item Edit commits patch the cached lines for changed owners.
  No per-sample overlay resource lookup, CPU rebuild, or full-Thing upload is added to the transform hot path
- the overlay renderer caches each class's draw ranges per mesh revision and submits them with one multi-draw call
  per class where the OpenGL entry point is available. Selected-owner ranges refresh only when the selection changes.
  A context without multi-draw support uses the same depth-tested ranges through individual draw calls. When both
  Collision Shapes and Visual Bounds are hidden, a Thing rebuild skips their geometry until either is enabled again
- direct and marquee Thing picking render the normal sprite pick geometry when it is available. A cached projected
  sprite-bounds fallback also covers a temporarily unavailable/stale GPU pick range; ordinary selection depth-tests the
  fallback against solid scene geometry, while X-Ray Selection deliberately bypasses that occlusion
- Thing type gizmos split static owner metadata from camera-facing vertices. The static cache resolves inherited
  `type`, pick ID, and already-rendered owner bounds only when Things/layer visibility change; camera motion rebuilds
  six billboard vertices per cached entry without walking Things, templates, or model resources. Ordinary repaint and
  selection reuse the uploaded VBO; GPU picking has a separate scratch VAO/VBO so a click cannot clobber that cache.
  Move-tool previews and Item Edit previews apply pick-ID shader transforms, while
  rotate/scale pointer previews rebuild camera-facing vertices from static metadata to keep icons facing the view.
  The **Thing Type > Offset** preference adds clearance above the rendered model bounds; each icon is automatically
  raised by its own half-size and a fixed `0.002` clearance even when this offset is zero. One global Stack spacing
  preference adds the same world-space gap between marker lanes in every project (default `0.01` units), and the
  draw and pick paths use identical positions. Thing Light and Arch Thing offsets add further clearance at their lanes
  and carry later lanes upward. The Arch Thing marker and its picking center use the same position. MotS ARCH owner markers use the combined stack
  offsets. Actor Headlights remain at their authored emitter positions. The shared Arch Thing visibility and offset
  preferences are available in every project, but the marker is shown only in JKDF2/MotS. IJIM flag `0x8` means Whip
  climb and does not produce an Arch marker.
  Changing these presentation settings never writes to Thing positions.
- classic 3DO Model LOD is an opt-in JKDF2/MotS viewport choice, off in both Editor and Game Preview by default.
  Off always draws geoset 0. Effective Arch Things (`thingflags & 0x8`) also stay at 0 in JKDF2/MotS: the
  original sector render pass draws them without the ordinary Thing LOD branch
  ([OpenJKDF2 `sithRender.c`](https://github.com/shinyquagsire23/OpenJKDF2/blob/master/src/Engine/sithRender.c#L2241-L2317)).
  With the option on, an ordinary model with one geoset stays at 0; two geosets compare the Thing
  origin's camera-forward depth to header LOD distance 2; three compare against distances 1 and 2; four or more
  compare against distances 1, 2, and 3 and never select above geoset 3. Equality selects the farther tier and
  behind-camera origins keep geoset 0. Header distance 4 remains parsed, edited, saved, and exported but is not
  referenced by the game 3DO-selection branch. SED has no MotS gameplay zoom state, so it uses the unzoomed
  camera-forward depth in both perspective and orthographic views. The policy follows
  [OpenJKDF2 `sithRender_RenderThings`](https://github.com/shinyquagsire23/OpenJKDF2/blob/master/src/Engine/sithRender.c#L2615-L2672).
  Parsed variants are cached per resource revision and geoset. Camera and header changes inspect only cached
  visible multi-geoset ordinary owners; staying in the same tier does not rebuild, reread, or reparse their meshes.
  A tier change rebuilds/uploads the Thing mesh presentation, including visual picks, wireframes, and outlines.
  The Thing-only composition retains the static Level triangle, line, and point prefixes, Level pick ranges, and
  surface-UV index when their cache boundaries still match; otherwise it falls back to full composition. The old
  Thing suffix is removed before the new tier's suffix is appended, and the upload keeps its partial-buffer path.
  This opt-in transition currently rebuilds the Thing presentation as a whole, so many simultaneous crossings in
  a large level can require a noticeable update. An unavailable lower geoset is negative-cached and falls back to
  geoset 0 without repeated paint rebuilds; resource republishing clears that cache.
  Collision and Visual Bounds overlays, ARCH editing identities, and placement guides continue to use geoset 0.
  Placement preview models may show the chosen visible geoset. Thing Type gizmo clearance above the model follows
  the selected visible geoset's bounds, including when stacked with other Thing markers
- MotS Things with `collide=2` show one green narrow-phase collision sphere for each usable hierarchy-node mesh. The engine
  parses each 3DO `MESH RADIUS` as `mesh.radius`; model post-processing computes a separate model/renderer bound and does not
  replace that mesh radius. Tree collision normally tests `mesh.radius × 0.75`. During MotS rendering, hierarchy nodes with
  flag `0x2` may have their mesh radius overwritten from the nearest unflagged ancestor; SED retains hierarchy flags and
  applies that substitution as a stable preview approximation. Before the first draw, the engine can still test the authored
  child radius, while shared-mesh references may make the persistent render-time mutation order-dependent. The preview places
  spheres at default-pose hierarchy centers and applies the Thing transform; its configurable radius factor is presentation-only
  and never changes `treesize`, templates, levels, or exports. A separate magenta sphere shows the broad-phase radius centered
  on the Thing from effective `treesize`; changing it updates only that guide. Animated puppet joints and amputations are not evaluated.
- Item Edit position/rotation previews apply the same transient owning-Thing pose to transform axes, regular/headlight
  gizmos, selected Thing-light range centers, and the Thing type gizmo without mutating Level per scrub sample.
- Item Edit and viewport Thing-transform finalization retain that exact last shader pose until the direct commit has
  taken its subset undo, resolved final containment, and patched the persistent per-pick render ranges. Only then is
  the preview cleared. Existing XYZ/PYR/Sector widgets receive the authoritative values directly; inherited Thing
  parameter/resource rows are not reconstructed at mouse release. Focused counters assert targeted publication
  without a Thing mesh rebuild.
- Thing Item Edit distinguishes interaction intent. Typing a Position/Rotation number is an explicit absolute
  assignment to every selected Thing. Mouse scrubbing and the arrow steppers instead calculate a delta from the
  displayed active-Thing value. Multi-selection uses the viewport transform contract: Position preserves all relative
  offsets, while Rotation applies one world-axis matrix around the active Thing pivot and rotates both positions and
  orientations. Single-Thing Rotation retains exact component assignment.
- The displayed active Thing and property-transform pivot come from the viewport's primary Thing when it remains in
  the selection. `Selection::things` is unordered and is never used to infer selection order; if no valid viewport
  primary exists, Item Edit and the preview use the lowest valid selected index as the same deterministic fallback.
  This keeps Position/Rotation axes anchored while scrubbing or stepping a multi-selection.
- Scrub/step delta gestures keep Level and undo history untouched during preview. One matrix plus the selected Thing
  pick IDs drives persistent models, selection overlays, type icons, transform axes, and Thing-owned light gizmos on
  the GPU. Release takes one subset undo, applies the delta once, resolves final containment, patches only the selected
  persistent ranges, and refreshes their checker rows. Arrow clicks use the same path as a one-step scrub; typed edits
  continue through `valueCommitted`. Existing property widgets synchronize in place without reconstructing inherited
  parameter/resource rows.
- Only an actual `QLineEdit::textEdited` sequence can enter that absolute typed-value path. Focus loss after a scrub or
  arrow gesture is inert, so moving from one Position/Rotation axis to another cannot recommit the previous active
  Thing value into every selected Thing. Arrow auto-repeat samples share one press/release transaction and therefore
  one preview, final publication, and undo record.
- Compact Thing-subset history records report their exact owner indices during Undo/Redo. The MainWindow restores only
  those persistent transform ranges and checker rows and synchronizes the existing Item Edit controls in place; it no
  longer rebuilds every Thing/model/type-gizmo cache. The opt-in History timeline is rebuilt only while its dock is
  visible, avoiding work proportional to the complete undo depth after every hidden transform command.
- Targeted publication keeps its owner-local follow-up work: aggregate Thing bounds normally update from changed
  owners, Thing-light cache entries are replaced only for changed owners, and an existing type-gizmo VBO patches six
  vertices per changed icon. A complete bounds scan remains necessary only when a changed Thing supplied a previous
  min/max extreme.
- shaded Thing selection outlines reference exact persistent 3DO/sprite VBO ranges. They do not resolve models, copy
  selected triangles, or upload a duplicate outline buffer on the normal path. The outline pass disables alpha test
  and forces opaque alpha so translucent/cutout sources keep a solid editor outline; stale/missing ranges retain the
  copied fallback. Primary/multi-selection identity travels with each source range.
- full checker builds index Thing-owned diagnostic rows. A targeted transform refresh removes and replaces only rows
  for the changed Things and leaves column sizing intact, while still executing those Things' consistency rules
  synchronously. Position/rotation commit paths that already resolved the final containing Sector explicitly reuse
  that result, so the checker does not repeat the same exact convex containment test; invalid sector references,
  finite-position checks, and base-template hygiene remain active.
- `SectorContainmentIndex` is an all-Sector BVH rebuilt with Level geometry. It deliberately ignores authoring-layer
  visibility because hiding geometry must not change a Thing's logical Sector. A valid preferred Sector is exact-tested
  first to preserve stable ownership at overlaps; otherwise BVH AABBs reduce the candidate set before the unchanged
  `sectorContainsPoint()` predicate chooses the lowest matching Sector index. Traversal is allocation-free. Cache
  identity includes the source `Level` instance, not only its sector count, and staged `GlView::setLevel(...,
  rebuildRenderCache=false)` clears the index even when the document mutates its existing `Level` in place. This keeps
  recovery/SED restores from querying bounds from the previous generation while the final render cache is deferred.
- Targeted Thing transform publication does not reserve, initialize, or compare the full per-Sector lighting cache.
  Sector-wide cache comparison remains only in broad lighting publications. Hidden/non-rendered Things update only
  their authoritative transform state: retained historical pick IDs are ignored, no fallback point bounds are
  recreated, and hidden owners do not contribute to the aggregate render bounds.
- Every rejected targeted transform fast path records a named reason and count (`sed.thingTransformFastFallbackReason`
  and `sed.thingTransformFallbackRebuildCount`) before taking the conservative Things-only rebuild directly, without
  retrying a broad all-Thing fast scan. This keeps a future cache/topology regression diagnosable instead of appearing
  only as an unexplained release-time pause.

Thing-placement hot path:
- modifier-only Free Insert and Surface Tilt alternatives are exact-match chords and are ORed within each action.
  When both actions match the held modifiers, Free Insert takes precedence; an explicitly empty binding disables
  that action. Cached normalized modifier masks are used during viewport input rather than reading QSettings or
  QAction shortcuts on each sample
- placement-preview triangles and wire lines are isolated as per-preview tail ranges; sprite instances use a stable
  `placementPreviewIndex` because sprite material sorting invalidates vector offsets
- stable topology/count updates transform and relight only those ranges. Geometry/runtime-light signatures force a
  fallback when the rendered representation changes; Sector transitions refresh ambient and IJIM point-light payloads
- persistent Thing append validates the cached prefix, strips the transient tail, appends up to one 256-Thing brush
  batch, and re-emits the preview rather than resolving every prior Thing asset again. Each appended Thing retains its
  factory-allocated `id` and `num` when preview content is copied into it, in both single and brush placement. Known-count
  ID capacity preflight uses constant-time arithmetic; it adds no whole-Level scan or copy to placement. Existing
  persistent SPR cards are copied from their pick-ID ranges; only the appended owners and re-emitted previews regenerate frame/lighting
  vertices before the topology-changing contiguous upload
- `updateThingPlacementPreview` validates the active template's model and sprite resource revisions once at the start
  of each synchronous preview event. Brush samples, preview lines, and snap builders receive that validated state
  through the call stack, so revision checks do not multiply with the representative sample count. Standalone
  placement and commit paths validate independently, and resource/template invalidation forces the next event to
  resolve fresh data
- `SurfacePickIndex` is rebuilt with Level geometry or layer visibility, not Thing edits. The viewport index contains
  only visible-sector layers, so placement cannot target or exact-test deliberately hidden geometry. Its BVH produces
  candidates only; the shared exact picker remains authoritative. Bounds include the per-edge tolerance
  `CloseEnough / |normal × edge|` and non-planar projection displacement, while uncertain shapes are carried in an
  always-test list. The exact picker keeps its all-positive/all-negative `CloseEnough` edge rule. Supporting convex
  projected faces use consecutive non-collinear edge planes for their finite displacement bound; exactly parallel
  contour subdivisions do not define a corner. Mixed-side and rounded faces can use the dual half-plane bound when,
  for each sign branch, six signed coordinate objectives have conservative certificates expressed as non-negative
  combinations of one or two edge-plane normals plus the surface normal. Certificate residuals are bounded with a
  common infinity-norm bootstrap before the expanded coordinate box is formed. Missing or numerically uncertain
  certificates, non-finite geometry, and degenerate faces remain in the exhaustive always-test list; near-parallel
  constraints are never merged approximately. Reversing winding or normal changes only the sign branch, not the
  conservative bound.
- angled orthographic surface snapping prefers a walkable floor/ceiling when the view ray first crosses a wall.
  It samples both sides of ordinary surface hits; when neighboring hits from one Sector are closer than those fixed
  probes, it also tests their interval midpoint with exact Sector containment before casting the vertical floor ray.
  This preserves placement on very thin sectors without increasing the probe work across unrelated sectors
- free placement reuses the containing Sector already resolved by the indexed viewport ray. Brush air/free samples
  test the previous/center Sector first, then use the all-Sector containment BVH to reject almost every candidate
  before the exact convex containment test; this retains authored containment across hidden layers without an
  all-plane scan
- Thing snap anchors cache resolved transforms, boxes, and connectors against document identity. The active template's
  corresponding placement data is cached until template/resource invalidation. Conservative distance rejection may
  skip only candidates that cannot reach the unchanged exact score threshold
- Thing transforms snapshot only the addressed indices; append operations snapshot only the old tail boundary.
  A brush gesture records that append boundary once and later batches call `touchLatestUndoRecord()` so autosave/save
  identity changes without another whole-Things copy. Finalization appends new Outliner rows and replaces only those
  Things' consistency/resource rows instead of scheduling a whole-level checker pass

Active placement caveats:
- brush preview and commit both use the selected 1-256 Count and the same scatter sampling policy; invalid targets
  are excluded from both. Authored scatter offsets and stroke distances retain double precision
- the template list and placeholder icons are prepared before presentation. Raw model parsing/bounds, MAT/CMP decode,
  worker rasterization, and readiness publication remain asynchronous and bounded. Completed `QImage`s publish only
  after active scrolling stops and in short time-budgeted GUI batches without reading the main preview's framebuffer
- first preview creation and preview topology/count or validity changes still use a Things-only rebuild
- sprite preview transforms use post-material-sort owner-to-instance, instance-to-six-vertex, and instance-to-batch
  caches, so finding and regenerating transient instances is O(preview sprite count). Preview SPR cards use a separate
  batch namespace from persistent sprites; cursor motion refits only preview bounds and uploads only their merged
  six-vertex subranges. Type-0 Game Preview camera-facing and type-2 Thing-forward frames share the exact full-build
  generator, so the optimization cannot drift from normal SPR orientation
- low-level 3DO/SPR support, insertion-offset, and rail-connector caches are bounded and include resolved resource
  revisions. A changed or newly created loose file and a distinct project source cannot reuse old same-name results.
  Loose-file detection uses path, size and modification time; preserving both metadata values can evade that check
- a point lying inside many overlapping Sector bounds can still require several exact containment tests; the cached
  broad phase deliberately preserves exact Sector ordering and does not change authored containment semantics

Camera/fly containment and ambience:
- RMB fly navigation only changes the camera rig and emits the normal view-transform signal; it does not scan all
  Sectors on every mouse/timer sample. MainWindow's 125 ms ambience tick resolves the camera eye through the same
  `GlView::findContainingSector()` BVH (with the previous Sector as the preferred overlap owner), then submits the
  resulting sound request to the audio transition/cache path.
- Periodic/debounced recovery capture is deferred while RMB fly navigation is active and resumes after the gesture.
  Recovery Level hashing/serialization, HIST expansion, compression, FEC, and atomic publication remain worker-thread
  work on a dedicated single-thread low-priority pool, isolated from interactive resource/audio jobs. The GUI capture
  shares immutable copy-on-write undo payloads and copies the current Level once; completion does not serialize it again.
  Ending a no-op fly/look gesture first compares the compact EDIT state and does not copy the Level or undo history when
  nothing changed.
- Native HIST loading converts each validated expanded state directly to the live compact snapshot domain. Its trusted
  adjacent-state decoder retains only values from semantic domains that were rematerialized and uses those values for
  transition/subset checks, without a complete previous-Level copy. Unchanged geometry is therefore neither retained nor charged once
  per Thing/light/COG history entry; structural records remain complete checkpoints.
- Adjacent nested HIST states use a core-owned trust chain. The parser still validates each complete canonical
  eight-section `HistoryNested` document, including both descriptor copies, payload CRC32C, project/timestamp, and
  final resource/object graph. The enclosing HIST section retains expanded SHA-256 plus FEC, so an inner failure drops
  only optional history while the authoritative Level+EDIT remain intact. Semantic decoding is skipped only for an
  exactly byte-identical checked frame from the
  previously accepted state; `rsrc` is decoded every time and any catalog change invalidates every indexed domain.
  Persisted dirty flags cannot authorize reuse. The compact reader constructs the bounded nested byte vector once and
  transfers ownership to the decoder only after the transition succeeds.
- Entering a new ambient-sound Sector no longer reads a loose file/GOB entry or converts IJIM INDIWV on the GUI
  thread. An immutable ResourceLocator generation performs that work in the worker pool; a generation token discards
  stale completions if the camera or document changes, and the GUI thread only publishes the resulting WAV to Qt
  audio. Re-entering a decoded sound reuses the bounded PCM cache.
- A level-generation swap invalidates the containment index before any deferred SED/recovery render publication. The
  temporary fallback is the legacy exact all-Sector resolver, so a restore cannot return a stale Sector; after the
  final mesh publication the BVH is rebuilt once. MainWindow also clears its previous ambience-sector hint at every
  document replacement, because an old numeric index could be valid but mean a different overlapping Sector in the
  new Level. Overlapping authored Sectors retain lowest-index ownership unless the preferred current Sector from that
  same Level still contains the eye.

Translucent scene ordering:
- a Solid or Textured adjoin with the Face **Translucent** flag is still routed to the sorted alpha queue. The adjoin
  **Visible** flag controls portal traversal rather than blending, while Geometry **Not Drawn** contributes no filled
  pixels. Transparent texture pixels on an alpha adjoin neither add tint nor enter the adjoin-excluded depth prepass
- translucent level batches, per-Thing triangles, and sprite runs enter one stable far-to-near queue rather than three
  category passes; equal-depth sprite effects are blended after ordinary level/Thing translucency
- Things retain per-triangle ordering while level surfaces stay grouped to avoid introducing water/adjoin seams. That
  grouping and its representative depth remain ordering heuristics inside the now-correct global source-category order
- the near-opaque depth prepass tests final texture-times-material/vertex alpha
  for eligible non-adjoin level and Thing batches. Near-opaque texels of a blended solid
  wall therefore populate depth and occlude later translucent content, while
  its transparent texture regions remain revealable; explicit z-write-disabled
  faces never enter the prepass. The cutoff/depth prepass remains a pragmatic
  depth heuristic rather than a general transparency solution
- this remains sorted alpha blending, not order-independent transparency: intersecting/cyclic alpha geometry and other
  cases without a valid total back-to-front order remain a renderer boundary and still need direct visual verification

Active caveat:
- the type-0 facing-policy defect is fixed and type-2 construction is source-audited, but representative type-2 assets
  near the vertical-axis fallback and final texture vertical orientation still need pixel/visual comparison against the
  game. IJIM type `1` remains accepted by the SPR parser but has no draw branch in the audited upstream function;
  classic type `1` uses the same camera-facing construction as the other admitted classic types
