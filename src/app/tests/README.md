# Application test guide

The application suites cover Qt-facing helpers, complete editor workflows, package layout, and the boundary between the
application and `sed_core`. CTest labels make the test layers discoverable without duplicating the same expensive UI
workflow in several executables.

The MainWindow runner creates its temporary INI directly before application logging or `settingsStore()` runs.
It seeds empty legacy and per-project roots/archives plus controlled empty install directories, so first-run
registry migration and game auto-discovery cannot import a developer's game paths. The initial settings guard
checks this baseline without modifying any user INI or registry.

Foreground-dependent native GUI/input tests activate their own test windows and require an idle test desktop. Test
runners must not minimize, restore, reposition, or close unrelated applications. When the desktop is in use, run
background-safe checks and report native checks as pending; do not weaken guards or count a partial run as the full
Release gate. Release validation remains incomplete until the complete Release CTest gate passes.

## Test layers

| Layer | Purpose | Representative coverage |
|---|---|---|
| Unit | Deterministic UI-independent helper behavior | [undo_label_tests.cpp](undo_label_tests.cpp), [layer_utility_tests.cpp](layer_utility_tests.cpp), [episode_utility_tests.cpp](episode_utility_tests.cpp) |
| Module | One application subsystem with its production dependencies | [render_preview_utility_tests.cpp](render_preview_utility_tests.cpp), [thing_utility_tests.cpp](thing_utility_tests.cpp), [app_sound_player_pcm_device_tests.cpp](app_sound_player_pcm_device_tests.cpp) |
| Domain | Level-designer behavior expressed through application policy and editor state | [thing_utility_tests.cpp](thing_utility_tests.cpp), [sector_ambient_property_tests.cpp](sector_ambient_property_tests.cpp), [surface_active_property_tests.cpp](surface_active_property_tests.cpp) |
| Integration | Files, settings, Qt objects, workers, or several application modules working together | [document_recovery_tests.cpp](document_recovery_tests.cpp), [partial_level_salvage_tests.cpp](partial_level_salvage_tests.cpp), [main_window_event_filter_tests.cpp](main_window_event_filter_tests.cpp) |
| Cross app/core | Application state and UI orchestration crossing the real core codec, Level model, resources, or edit algorithms | [document_recovery_tests.cpp](document_recovery_tests.cpp), [partial_level_salvage_tests.cpp](partial_level_salvage_tests.cpp), [main_window_event_filter_tests.cpp](main_window_event_filter_tests.cpp) |
| UI/system | Complete MainWindow interaction and distributable package contracts | [main_window_event_filter_tests.cpp](main_window_event_filter_tests.cpp), [verify_package_layout.cmake](verify_package_layout.cmake), [verify_package_layout_tests.cmake](verify_package_layout_tests.cmake) |

`sed_app_tests` owns the focused unit, module, domain, integration, and cross-layer groups. It accepts repeated
`--group <name>` arguments for focused runs. `sed_app_main_window_tests` builds the complete production application
without `main.cpp` and exercises real MainWindow workflows. The two package tests validate both the built distribution
and the verifier's success and failure branches.

The package checks have a deliberately narrow boundary. `sed_package_layout` checks the built executable, every file
listed by `SED_RUNTIME_DATA_FILES`, the four supported-game template databases, and, on Windows with Qt 6, the required
Qt DLLs, `qwindows` platform plugin, and at least one non-empty multimedia backend. `sed_package_layout_tests` runs the
verifier against synthetic success, missing, empty, directory, unsupported-version, and wrong-name layouts. These tests
do not inspect extra files, prove that the staged directory is free of developer state, or satisfy Qt and codec license
notice obligations; those remain release-staging checks. Build the explicit `sed_release_package` target for that
final package boundary: it stages the 11-file editor-data set, deploys the selected Qt kit with `windeployqt`, includes
selected Qt/MSVC notices and app-local CRT files, and records relative file hashes in `RELEASE_MANIFEST.json`.
IJIM template and WAV category pairs are optional project data; their absence leaves templates available through All
and places discovered sounds under `Sounds > Uncategorized`.
The lower-level `cmake/stage_release_package.cmake` remains available for explicitly supplied data and notice lists;
its output allowlist covers the Qt Multimedia FFmpeg DLL names deployed by the supported kit. See the
[packaging contract](../../../docs/README.md#source-publication-and-binary-packaging).

## Viewport banner preferences

- [viewport_banner_settings_tests.cpp](viewport_banner_settings_tests.cpp), in
  `sed_app_tests --group viewport_banner_settings` / `sed_app_viewport_banner_settings`, checks independent category
  defaults, duration bounds, strict malformed-field fallback, exact ARGB persistence, invalid-save nonmutation, and
  caller-duration preservation for General/Error messages. These settings remain independent of themes and native EDIT.
- [toast_overlay_painter_tests.cpp](toast_overlay_painter_tests.cpp), in
  `sed_app_tests --group toast_overlay_painter` / `sed_app_toast_overlay_painter`, checks configured viewport toast
  colors and opacity through the real painter while preserving category layout.
- [toast_expiry_timer_tests.cpp](toast_expiry_timer_tests.cpp), available through
  `sed_app_main_window_tests --toast-expiry-timer` and CTest `sed_app_toast_expiry_timer`, is forced to the offscreen
  Qt platform and blocks OpenGL context initialization. It checks mixed toast expiry, delivery of the final repaint
  after the last ordinary viewport message clears, and that timer callbacks and repaint requests then stop.
- Preferences integration covers per-banner draft edits, Cancel/OK, restoring defaults, and banner-only publication
  without scene rebuilding. Native two-link tests cover exact Camera targets, transient pair highlighting, repeated
  navigation, and safe dialog-owned timer lifetime while retaining pending mouse and keymap edits.

## Viewport input and modifier bindings

- [thing_utility_tests.cpp](thing_utility_tests.cpp), in `sed_app_tests --group thing`, covers modifier chord normalization, OR alternatives,
  inclusive selection matching, exact X-Ray/transform matching, and explicit empty bindings.
- [hold_modifier_binding_tests.cpp](hold_modifier_binding_tests.cpp), available through
  `sed_app_main_window_tests --hold-modifier-bindings` and CTest `sed_app_hold_modifier_bindings`, checks that
  MainWindow caches all selection, placement, and transform alternatives, publishes viewport bindings into GlView,
  and refreshes or clears them when the keymap changes. It also checks placement precedence and selection hints.
  CTest forces the offscreen Qt platform.

- [priority_mouse_release_tests.cpp](priority_mouse_release_tests.cpp), available through
  `sed_app_main_window_tests --viewport-window-mouse-release` / `sed_app_viewport_window_mouse_release`, checks
  preliminary QWindow releases, delivery to the real GlView click path, unrelated physical-button releases,
  outside-widget cancellation, and capture loss. It stays hidden on the offscreen platform with no OpenGL context
  or native input.

- [light_mode_shortcut_tests.cpp](light_mode_shortcut_tests.cpp), available through
  `sed_app_main_window_tests --light-mode-shortcuts` / `sed_app_light_mode_shortcuts`, checks family-specific Light Add
  dispatch, the real Sector/Thing/Light Add commands with A as their sole default, preserved custom and cleared
  bindings, shortcut ownership, repeat suppression, focused text input,
  UV pointer ownership, context-aware conflicts, light deletion without removing its Thing owner, and active Fly
  cancellation before MainWindow member teardown. CTest forces the offscreen
  platform; no native mouse or keyboard input is sent.

- [navigation_preferences_tests.cpp](navigation_preferences_tests.cpp), through the focused
  `sed_app_main_window_tests --navigation-shortcut-context` / `sed_app_navigation_shortcut_context` selector, checks
  active-context conflicts, global actions overlapping focused editors, and Fly navigation ownership. This selector
  forces the offscreen platform and avoids the broader suite's native cursor and hover checks.

## Resource I/O boundaries

[resource_io_boundary_tests.cpp](resource_io_boundary_tests.cpp), available through `--resource-io-boundary` /
`sed_app_resource_io_boundary`, exercises ordinary Save back to an existing loose SPR/SND/UNI source outside the
active project, across all three project types. It verifies exact destination identity, persisted edits, and the saved
checkpoint. COG cases cover over-budget Open, clean and dirty manual Reload, a source growing during the discard
question, explicit 16 MiB diagnostics, and unchanged text, document-history timeline, cursor, and selection on rejection.
Archive COG cases cover oversized winning GOB/primary sources without extraction or shadow fallback, exact-name
precedence over a loose `.cog` suffix alias, reported archive read failure, and complete ordinary/exact-limit working
copies. Selected-source cases preserve archive identity over later loose aliases. Existing loose and archive tab
activation keeps the live buffer/history/caret after external source changes without payload rereads; a working-copy
file alone cannot bypass first-Open admission. Fixtures use isolated synthetic files and owned dialogs; no native mouse
or keyboard input is sent.

## Cross-layer contracts

Cross app/core tests must use real production boundaries rather than replacing the core with a look-alike stub. Current
coverage includes:

- Native SED save, strict open, protected-section repair, partial recovery, long-path JKL/NDY salvage publication, EDIT
  state, HIST undo/redo, and pending JKL COG import metadata across current and sparse undo/redo states.
  [document_recovery_tests.cpp](document_recovery_tests.cpp) also checks `jimp` optional-loss recovery preserves the
  verified Level while clearing only import metadata.
- ResourceLocator-backed 3DO, MAT, sprite, template, sound, and thumbnail behavior
- External loose-file replacement from disk through the shared watcher, revision/epoch invalidation, 3DO-to-MAT and
  SPR-to-MAT dependency discovery and reload, classic indexed MAT-to-CMP invalidation, unavailable 3DO and malformed
  MAT/SPR rollback, repaired-file recovery, stale worker rejection, and renderer-facing preview publication
- Direct ResourceLocator and parser-value tests for external episode and JLH/JLL project-list replacements, including
  candidate-before-assignment failure behavior without claiming application-level atomic publication
- Level-wide entity-ID exhaustion at the `INT32_MAX` sentinel, including placement/brush and COG failure before
  publication, exact-capacity COG duplication, and Join preview atomic rejection/exact-fit behavior
  ([thing_placement_behavior_tests.cpp](thing_placement_behavior_tests.cpp),
  [thing_utility_tests.cpp](thing_utility_tests.cpp)).
- [viewport_thing_overlay_state_tests.cpp](viewport_thing_overlay_state_tests.cpp), through
  `--strict-surface-context-topology` / `sed_app_strict_surface_context_topology`, transports native EDIT+HIST and
  checks Undo/Redo plus cold History jumps across Sector deletion with None, Structure-only, and unsupported dirty
  masks, including unsupported bits combined with both topology bits. It requires stale Strict scope/components and
  remembered Face state to clear, including a parked Surface submode, and unknown-mask cases to refresh meshes. Mixed
  multi-step jumps start from committed Strict Face context and cover a known paired prelude followed by an ambiguous
  topology edit, plus pure Move/Thing-only controls that retain compatible context and committed Face selection.
  All-sentinel controls keep the existing known-paired behavior.
- Level and selection mutations committed through MainWindow with undo/redo and renderer invalidation
- [frame_history_selection_tests.cpp](frame_history_selection_tests.cpp) drives the real Frame Add action and viewport
  click signal with missing, trailing, duplicate, and canonical `numframes` plus interleaved parameters. It checks
  frame-typed selected indices, exact Undo/Redo selection, production recovery store/read, and native Level+EDIT+HIST
  codec restoration. Save-candidate helper checks cover selected/active ordinal remapping and rejection of non-frame
  source references or missing candidate frames. Properties removal checks preserve selected frame ordinals across
  two Things. Real Delete/Undo/Redo QActions cover last-frame removal with multiple selected Things and frames-only
  selection; a pose-edit Undo checks that stable storage retains the user's later frame deselection. Its focused
  MainWindow entry is `--frame-history-selection`.
- [frame_add_move_behavior_tests.cpp](frame_add_move_behavior_tests.cpp) exercises Frame Add hover and click through
  production viewport signals, compares every committed pose field with the preview, and covers same-pixel camera
  changes, held modifiers, local SPR overrides, and owner orientation fallback. Ctrl+Shift alignment checks the picked
  Surface while requiring unchanged Level bytes/history before click commit; a custom Alt binding also aligns the
  preview, and key release restores the saved option. Its multi-sample Frame Move gesture checks cached resource/support
  work and preserves the owning Thing as a snap candidate; real Properties edit/copy/delete actions check owner-subset
  history and warm Thing geometry. Its focused MainWindow entry is
  `--frame-add-move-behavior`.
- The focused `--surface-multicut-regression` run stages and applies an asset-free open Surface arch through GlView,
  compares it with the unique-plane core sequence, and checks duplicate/reversed/scaled planes, distinct-plane and
  precision rollback boundaries, authored UV/RGBA, appended-face selection, and exact Undo/Redo.
  Bounded arrangements additionally check one-sided and crossing strokes, default-on UI state, and real Surface
  and Sector staged Apply routing, dual-selected reciprocal portals, and exact Undo/Redo; disabling Bounded
  retains the global-plane arrangement.
- The focused `--surface-roundoff-multicut` / `sed_app_surface_roundoff_multicut` run recreates the geometry-only
  33-line arch/rotated-shape arrangement from the reported saved example in both Bounded and unbounded modes.
  It checks shared-endpoint construction roundoff, meaningful tilt/offset boundaries, complete topology and UV/RGBA
  preservation, appended-face selection, and exact draft/Level Undo/Redo without bundling private level resources.
  Apply Undo restores valid Rotate/Scale tools after their selected lines; paired Redo preserves the current valid
  selection and active item while restoring the exact applied geometry.
- The focused `--cleave-bounded-recovery` run checks the named Boolean in EDIT, strict malformed-value rejection,
  preserved staged geometry and history, ignored future keys, legacy pending-line defaults, and application
  preferences taking precedence over a preceding document when an empty legacy record has no Bounded value.
- [cleave_input_routing_tests.cpp](cleave_input_routing_tests.cpp), available through `--cleave-input-routing` and
  CTest `sed_app_cleave_input_routing`, checks actual Cleave/UV/grid/tool shortcut ownership, key release, Ctrl-wheel
  transforms and hover-shape boundaries, camera preservation, local Undo/Redo, and pointer-gesture mode changes.
  Orthographic and perspective fixtures cover Sector Camera plane placement and stationary-pointer camera changes.
  Custom and cleared local transform bindings cover native ShortcutOverride, conflicting global keys, and Preferences
  presentation. Armed-shape Ctrl+Shift wheel checks frame rotation without draft/history or camera changes, then verifies
  the placed frame.
  Focused `--cleave-fly-custom-shortcuts` / `sed_app_cleave_fly_custom_shortcuts` and `--cleave-uv-custom-owner` /
  `sed_app_cleave_uv_custom_owner` runs exercise conflicting custom Cleave keys while fly navigation or the UV canvas
  owns input, including actual UV island selection, move commit, and cancellation.
- [cleave_wheel_session_tests.cpp](cleave_wheel_session_tests.cpp), available through `--cleave-wheel-session` /
  `sed_app_cleave_wheel_session`, checks cumulative Ctrl-held Scale/Rotate across both projections and Cleave families,
  fractional and pixel-wheel input, one-command completion, exact cancellation and identity with Redo preserved,
  rotations beyond a full turn, reusable bounded worksets, tool/family/focus handoff, global Control release, and
  native Save/reopen of the settled baseline and completed local timeline. HUD tests exercise
  immediate Control-release hiding, independent Rotate/Scale idle deadlines of 400 ms by default during held wheel/pointer
  gestures, deterministic stale-timeout injection, actual 400 ms timer delivery and rearming, completion
  preserving the remaining idle deadline, unconsumed presentation-only releases,
  and unchanged authored state/history when it expires. Continuation retains cumulative values and one prepared
  workset, restores the readout, and finishes one exact Undo step; Escape retains the rollback baseline and Redo.
  Standalone viewport checks release any outer MainWindow Cleave context first, so the real application event filter
  cannot claim their modifier releases based on the host cursor position.
  [cleave_hud_presentation_tests.cpp](cleave_hud_presentation_tests.cpp), through group `cleave_hud_presentation` and
  CTest `sed_app_cleave_hud_presentation`, checks injected timestamp fade edges, short custom lifetimes, continuation,
  renewal during fading, reset, and the production badge painter's RGBA output on a transparent image.
  Configure Multi-cut options after activating the intended family, because Surface and Sector have independent
  workspaces. Exact Apply/Undo UI checks must trigger the application's Undo/Redo actions; `undoForTest()` and
  `redoForTest()` exercise the Level document only and bypass viewport workspace restoration. Native shortcut
  fixtures must activate their owned window before the gesture, and check its live state before opening another window.
- [navigation_preferences_tests.cpp](navigation_preferences_tests.cpp), available through `--navigation-preferences` /
  `sed_app_navigation_preferences`, exercises actual Preferences controls, Toggle Grid, compact alternative
  add/remove/reconstruction for every action, unchanged mouse defaults, button/chord remapping and release ownership,
  wheel modifiers, camera inversion across both projections and a banked orbit, malformed-settings fallback,
  Cancel/OK publication without mesh rebuilds, cached routing, and held-Ctrl half-speed Fly. Middle-button Fly checks
  retain wheel-click versus wheel-scroll separation, complete camera-family remapping, modifier-held activation,
  release ownership, and editable wheel modifiers, inversion, and disabling. Separate Zoom In/Out and Fly Speed
  Up/Down checks cover exclusive-context Numpad +/− defaults, editable alternatives and persistence, repeat and
  inverse/clamped steps, wheel-setting independence, text/UV ownership, and unchanged authored history, meshes,
  and recovery capture counts. Separate Zoom/Fly-speed wheel-note links reveal and briefly highlight their respective Camera command pairs without publishing or
  losing pending edits; layout checks compare compact modifier controls at narrow and wide dialog sizes. Conflict regressions
  distinguish actual global, viewport, COG, local UV, and exclusive Fly scopes, check all alternatives and exact
  intentional default overlaps, and deliver real keys to Preview and Map Settings numeric controls. Ordering checks
  cover every category/action sibling level and stable row-owned controls after filtering and Reset. Native-cursor
  pixel checks cover category, normal, alternate, and selected rows under application and inherited hover styles.
- [cog_shortcut_context_tests.cpp](cog_shortcut_context_tests.cpp), available through `--cog-shortcut-context` /
  `sed_app_cog_shortcut_context`, covers caret-aware Insert ownership, primitive/resource menus, message/custom-label
  insertion, cancellation and stale-menu rejection, remapped and disabled bindings, and script-versus-viewport/UV/text
  focus. Sanitized structural vectors cover comments, strings, nested and conditional returns, complete/incomplete
  handlers, and EOF without a final newline. Accepted constructs preserve one local Undo and Level history.
- [transform_numeric_precision_tests.cpp](transform_numeric_precision_tests.cpp), available through
  `--transform-numeric-precision` / `sed_app_transform_numeric_precision`, checks real numeric transform dialogs,
  cancellation, and exact document history. Numeric and viewport Scale cases share source-relative geometry admission,
  preserve valid small geometry, reject degeneracy without a new history entry, and retain authored fields and selection.
  Its Mirror action swaps selected Things between synthetic rooms and checks containing Sectors, unchanged authored
  fields, committed selection cleanup, and exact Undo/Redo restoration.
- [uv_numeric_input_precision_tests.cpp](uv_numeric_input_precision_tests.cpp), available through
  `--uv-numeric-precision` and CTest `sed_app_uv_numeric_precision`, checks controlled UI locales, subnormal and
  scientific input, invalid-input cancellation, and actual UV commands without losing numeric precision.
- The focused `--cleave-document-reset` / `sed_app_cleave_document_reset` run checks that a new document clears
  both Cleave workspaces and their local histories, releases input ownership, and restores the saved Multi-cut
  preference. The same assertions run in the MainWindow recovery integration suite.
- [ui_decimal_policy_tests.cpp](ui_decimal_policy_tests.cpp), available through `--ui-decimal-policy` and
  CTest `sed_app_ui_decimal_policy`, checks Dot-by-default, Comma, and controlled System locale formatting, actual
  Preferences OK/Cancel, precise and scrub input, native controls, malformed settings, grouping rejection, and raw
  draft preservation through a live preference change. Independent byte vectors check SPR, SND, UNI, JSON, CBOR,
  native SED, and JKL/NDY output under each UI policy.
- [ui_decimal_consumer_tests.cpp](ui_decimal_consumer_tests.cpp), available through `--ui-decimal-consumers` and
  CTest `sed_app_ui_decimal_consumers`, checks actual camera FOV, light popup previews, canonical engine-light values,
  Cleave transform readouts, typed Thing and placed-COG values, numeric query fields, and read-only measurements.
  [resource_ui_decimal_tests.cpp](resource_ui_decimal_tests.cpp), available through
  `--resource-ui-decimal` and CTest `sed_app_resource_ui_decimal`, checks Sprite and Sound Class numeric fields in all
  three projects, Template numeric fields, exact external bytes, local Undo/Redo, pending native drafts, and unchanged
  table identities and scroll position during locale refresh.
- The focused `--light-gizmo-selection` run checks Baked and Thing lights in JKDF2/MotS and Sector lights in IJIM,
  through orthographic and distant perspective views on both CPU and initialized GPU depth paths. It covers
  invisible single-sided backfaces, opaque and double-sided walls, hidden layers, X-Ray, Alt-transform marquee
  modifiers, and logical-pixel hit margins. Separate CTest entries force normal and high-DPI screen scales.
  The fixtures use synthetic geometry and isolated settings.
- The focused `--cleave-options-layout` run checks distinct accessible icon toggles, three project-aware unit choices
  per project, unit changes preserving authored physical dimensions, and wide/narrow layouts with every visible
  control inside the viewport. It covers legacy hidden-length and None-unit
  preferences, a new Lengths choice surviving another window startup, and explicit EDIT guide choices taking precedence
  over the legacy shared flag. Shared MainWindow recovery coverage round-trips named guide choices, normalizes legacy
  None units, and checks missing-field defaults, ignored unknown names, and atomic rejection of malformed maps, known
  field types, and unit values.
- [cleave_shape_utils_tests.cpp](cleave_shape_utils_tests.cpp), in `sed_app_tests --group cleave_shape`, checks centered
  2D Rectangle, N-Gon, Filled arch, Star, and legacy Square plans, linked dimensions, finite size/count boundaries,
  deterministic ordering, and convex cells covering the outline. Star checks include the central polygon, every tip,
  noncrossing internal cuts, and strictly indented three-point valleys.
- [cleave_measurement_utils_tests.cpp](cleave_measurement_utils_tests.cpp), in `sed_app_tests --group cleave_measurements`
  and CTest `sed_app_cleave_measurements`, checks actual joined-corner angles in rotated, small, and translated frames,
  endpoint deformation, branches, degenerate/non-finite lines, presentation storage admission, count overflow, and
  complete transform HUD bounds at viewport edges.
- [cleave_shape_overlay_tests.cpp](cleave_shape_overlay_tests.cpp), available through `--cleave-shape-overlay` and
  CTest `sed_app_cleave_shape_overlay`, checks accessible Add Shape controls, the nonmodal settings panel, linked and
  independent dimensions, unit conversion, segment/point limits, shared Grid Snap, and default Move with unavailable
  Rotate/Scale controls disabled. Changing these controls preserves an empty draft until viewport placement.
- The same file's `--cleave-family-controls` run, CTest `sed_app_cleave_family_controls`, checks that switching between
  Sector and Surface publishes each family's own Multi-cut, Chain, Bounded, guide, Angle, and unit controls without
  reapplying stale checkbox values from the preceding owner.
- The focused `--cleave-chain-origin` run checks plain endpoint clicks versus dragging and modifiers, the visible Chain
  origin, mixed independent/chained cuts, ordered Apply, chosen-source angle snapping, canonical closure versus branches,
  Sector camera attachment, source remapping/reset, completed endpoint/body edit recovery notifications, and strict optional
  `chainOrigin` EDIT persistence. Surface and Sector fixtures cover orthographic and perspective picking using synthetic geometry.
- [cleave_connection_history_tests.cpp](cleave_connection_history_tests.cpp), available through
  `--cleave-connection-history` and CTest `sed_app_cleave_connection_history`, checks Shift joins between independent and
  chained lines without changing authored order, including Chain-off joins. Whole-line moves keep the selected line
  rigid, carry shared endpoints, and pin collateral opposite endpoints; collapse rejection and Alt detachment have
  separate checks. Real MainWindow gestures cover exact local Undo/Redo of a chosen Chain origin and a multi-sample
  drag, Undo during an endpoint preview, mid-gesture selection-mismatch rollback, and native angle-field text Undo/Redo
  without consuming Level history. Actual Outliner selection clearing, failed Apply, selection restoration, undoable Clear,
  successful Apply as one document step, and Level Undo restoring exact pre-Apply draft/options and Sector/Surface
  selection are covered with synthetic geometry and isolated settings.
- [cleave_advanced_interaction_tests.cpp](cleave_advanced_interaction_tests.cpp), available through
  `--cleave-advanced-interaction` and CTest `sed_app_cleave_advanced_interaction`, checks real pointer selection,
  shared-grid/Ctrl inversion, endpoint constraints, shared-joint cycling and transforms, batch Delete, marquee
  add/remove, rendered signed Rotate/positive Scale HUD, visible highlighting of all selected lines, common-plane
  gating, and exact Escape rollback. Actual rank-editor keyboard input covers native focus, clamping/overflow,
  remapping without physical identity changes, and one local Undo; timeline no-op retention and atomic restore
  observers have separate checks.
- [cleave_shape_apply_tests.cpp](cleave_shape_apply_tests.cpp), available through `--cleave-shape-apply` and CTest
  `sed_app_cleave_shape_apply`, exercises actual Add Shape controls and viewport placement, bounded Surface/Sector
  Apply, positive-area convex output, silhouette coverage/non-overlap, conserved volumes/reciprocal portals, and
  one-face-at-a-time convex extrusion. Three-, five-, and six-point Stars allow extra convex fragments. Real
  pointer-authored cuts and native rank typing distinguish three-piece from four-piece order-dependent Apply.
- [cleave_workspace_persistence_tests.cpp](cleave_workspace_persistence_tests.cpp), available through
  `--cleave-workspace-persistence` and CTest `sed_app_cleave_workspace_persistence`, checks current draft/local-history
  roundtrips, strict types/references/permutations/storage admission, and legacy migration. Restore cases cover exact
  large retained timelines, explicit over-budget refusal without partial viewport publication, and current-only draft
  retention when an initial baseline cannot fit. It also checks draft-only dirty/recovery policy and event-driven
  Save/replacement races. Presentation-only changes remain separate from authored cuts.
- [cleave_apply_workspace_history_tests.cpp](cleave_apply_workspace_history_tests.cpp), available through
  `--cleave-apply-workspace-history` and CTest `sed_app_cleave_apply_workspace_history`, checks Sector/Surface Apply Undo,
  exact historical draft/options/selection, immediate paired Level Redo versus applied-family editing or an active peer's
  local Redo, native HIST remapping, malformed/missing/trimmed attachments, Layer presentation, History navigation, and
  transient-preview normalization. Both Apply families preserve a compatible independently edited peer through dormant, local,
  document, and History routes. A recognized peer workspace inside a historical Apply attachment drops only optional
  attachments while preserving the current dual workspace and checked HIST.
- [mots_arch_lighting_workspace_recovery_tests.cpp](mots_arch_lighting_workspace_recovery_tests.cpp), available through
  `--mots-arch-workspace-recovery` and CTest `sed_app_mots_arch_workspace_recovery`, checks bounded codec payloads,
  unknown optional names, exact settled live channel/target/selection and owner-expansion provenance, deferred
  resource resolution, and invalidation. Live getter assertions distinguish applied state from pending DTO storage.
- Private system-clipboard entity payloads encoded and decoded by the core, then pasted through a second MainWindow
- Project-specific Thing, lighting, geometry, import/export, and package-data behavior
- Resource authoring covers three independently dockable homogeneous editor groups with one visible major dock tab,
  per-document history buttons, remapped document bindings with native field priority, four icon Tools entries,
  North tab placement above the group header in every native dock area,
  Tools opening empty groups, explicit New/Open file creation, top-area title/grip and docked-only Undock menus,
  one toolbar per host, dirty-only Save and Save All across same-kind detached files with cancellation/error
  preservation, 64-file/64-occupied-host admission, typed group
  recovery metadata, and legacy mixed-host migration. It also covers raw invalid drafts and pending Sprite numeric text
  through native EDIT recovery, bounded 4096-row capture counters around
  input and viewport events, same-document Save/Open context, modal picker lifetime, classic/MotS project-profile and
  focused-control behavior, a two-cel MAT framebuffer update with invalid-texture preservation, FPS/Pause/Stop behavior
  and double-click picker cancellation, detached tab transfer with Escape cancellation, and resource-buffer baseline
  retirement, group-toolbar focus, and local Sound Class/text Undo routing. The focused
  [resource_authoring_presentation_tests.cpp](resource_authoring_presentation_tests.cpp) coverage, available through
  `--resource-authoring-presentation` / `sed_app_resource_authoring_presentation`, checks optional `editorPresentation`
  V1, stable SND/UNI selection
  identities, scrolling/manual widths, line-edit and deferred cell carets, strict malformed-state rejection, and
  presentation-only capture that does not commit editors, dirty resources, advance local history, or arm recovery.
  Supplementary/combining-text fixtures assert exact UTF-16 cursor/anchor offsets and restored forward/reverse selections.
  The dedicated `--resource-authoring-history-caret` / CTest `sed_app_resource_authoring_history_caret` coverage validates
  the current caret independently of historical SPR values while retaining strict rejection of malformed current caret
  positions and historical Sprite values.
  A restored Sound Class `+0.25` Min draft checks canceled Save preservation, successful loose-Save normalization to
  `0.25`, stale deferred-caret removal, valid pane/aggregate EDIT capture, and native EDIT roundtrip. UNI
  search tests clear a live query through the actual clear control, restore the late selected cell to view, and retain
  edit buffers, history, selection, and legacy metadata; shared unsaved-prompt checks cover the three resource editors'
  titles and Save/Discard/Cancel style.
  Held file-tab drags require visible mouse-transparent Join feedback only for valid same-kind targets, with clearing
  on cancellation and owner/context replacement. Last-file transfer hides the primary source while retaining Tools
  reopening, and retires empty detached sources without changing the moved document's raw state or history
- Synthetic GOB/GOO archive tests require exact basename and case-preserved entry hover text for SPR, SND, UNI, and COG,
  separate source ownership/working-copy paths, retained origin after native restore without mounted archives,
  cancellation/failed Save preservation, and full loose-path hover after successful Save. COG detach retains the same
  cached origin on its live page; winning-source tests keep candidate-before-root precedence
- Generic picker Storage tests compare exact winning loose paths and GOB/GOO basename plus case-preserved entry paths,
  including image/model previews, unavailable and cleared selections, selected-source overrides, and silent WAV metadata
  refresh. Template entries report project template-database data. Where applied, bold names retain the independent
  loose-copy-availability meaning; template/model thumbnail rows do not establish disk absence through normal text
- Sprite animation fixed vectors cover early and late 20 FPS frame boundaries, carried phase, upload latency, and
  near-boundary rounding. Live playback of a synthetic two-cel archived MAT during 40 width edits requires both cel
  colors, exact final width 5.975, continued playback, and unchanged mesh-rebuild count
- Sound mode tests cover engine-sorted groups with separate slot badges and unchanged source-row order, unhighlighted
  rows with current-item focus and native editing, full-row and gutter pixels unchanged by pointer hover or current-item
  changes, and preserved mode expanders. They cover centered Flags/Min/Max headings and values, radius-unit hover help, one-click
  headers, top Expand All/Collapse All controls, Add Entry footers, native-only empty modes, and volume painting in
  narrow cells. Layout vectors pin minima `80/160/100/56/56/230` and defaults `80/220/100/64/64/230`, a stable minimum
  with actual themed scrollbar chrome. Exact fitted vectors pin 682=`80/160/100/56/56/230`,
  758=`80/220/100/64/64/230`, 1000=`80/462/100/64/64/230`, and
  1400=`80/806/100/64/64/286` after the 40% Volume-to-Sound transfer. Real unscrolled and horizontally
  scrolled Remove clicks must stay inside the visible Volume cell and retain exact row/Undo state. Icon checks retain
  22-pixel buttons and 13-pixel logical icon sizes while requiring antialiasing at 2x density. A playing row retains its
  Stop/Loop pair while another row is hovered and after pointer exit, with at most five shared controls and unchanged
  authored rows
- Document-history limits use 1025 real commands in SPR, SND, UNI, and COG, with exact native restore, retained clean
  checkpoints, and Undo/Redo. COG byte-budget eviction uses a roughly 2.8 MiB authored text edit and verifies unchanged
  current source, selection, scroll, and highlighting before exercising restored commands; capture itself does not trim
  or commit the live document. Identical nonempty COG selection replacement separately checks the real native dirty
  command, immutable valid capture, and exact restored clean Undo/dirty Redo without changing source bytes
- The isolated `--cog-overwrite-lifetime` run and CTest `sed_app_cog_overwrite_lifetime` use
  [cog_interaction_regression_tests.cpp](cog_interaction_regression_tests.cpp) to destroy a COG draft page or its
  MainWindow during the actual overwrite confirmation. Existing file bytes and an unrelated tab's exact text,
  revision, dirty state, native Undo branch, and Level history must remain unchanged. Process isolation keeps a
  lifecycle crash separate from the other MainWindow groups.
- Viewport shortcuts cover number-row edit modes, Shift-number project-specific submodes and Qt punctuation key
  representations, exact-default migration, custom bindings, text/control focus guards, and unchanged Numpad camera keys
- The focused `--minor-grid-appearance` MainWindow group covers default Lines and `2`-pixel dot size, Lines/Points controls
  in Viewport Settings and Preferences, the inclusive `0.5`–`10` range, enabled state, and app-settings reconstruction.
  Preferences checks require OK to persist the choices and Cancel to preserve the exact live and persisted values.
  EDIT checks require absent-map and missing-field defaults independent of app settings, known-field round trips,
  integer numeric diameter acceptance, unknown-key acceptance, and exact unchanged workspace state after malformed
  types, non-finite diameter, or out-of-range values
- The same group samples actual top-down orthographic and oblique perspective framebuffers. It requires minor dots
  at lattice intersections, an empty minor-line midpoint in Points mode and a restored line there in Lines mode,
  complete radial support for an oblique dot, and major lines and colored axes in both styles. A real level first
  activates mesh-rebuild and mesh-upload counters; style, diameter, and camera changes must leave both counts unchanged
- UV Sub-grid tests independently check Lines/Points controls, persisted logical-pixel diameter, actual canvas raster
  support with pan/mirror/rotation, density limits, and `window.uvGridAppearance` capture/restore. They require legacy
  defaults independent of app settings, integer diameter and unknown-key acceptance, and exact unchanged workspace
  state after malformed map/style, non-finite diameter, or either range-bound violation.
- Map Settings popup tests send real press/release events to combo options outside the panel bounds. They require
  selection with the owning panel retained, Escape closing the combo only, global-bounds dispatch behavior, and an
  unrelated active popup receiving its click while dismissing Map Settings.
- Dedicated COG document-docking checks cover real per-file detach, same-owner transfers, wrong-kind and foreign-owner
  rejection, Escape cancellation, native caption split/dock return, exact QTextDocument history and cursor, all-host
  Save/Save All, shared unsaved-prompt cancellation/discard/rejection, owner destruction during its modal prompt,
  teardown, legacy migration, strict 64-host topology admission,
  and complete native EDIT/Preview recovery without external file writes
- Windows floating COG and resource-host checks require visible, enabled native Maximize geometry from
  `TITLEBARINFOEX` and bounded pointer activation on the foreground test-owned caption. They verify both native and Qt
  maximization, exact frame and cursor restoration, and retained detached/recovered document identity, raw drafts,
  history, and Dock return controls. These checks activate their test-owned caption and do not move unrelated windows;
  blocked clicks report the target, pointer-hit, and first overlapping window with bounded titles, classes, process IDs,
  and overlap bounds.
- Dedicated loose-document watch checks use real atomic source replacement, deletion, and recreation for SPR, SND,
  UNI, and COG in each project profile. They require exact named Reload/Keep choices for clean and dirty buffers,
  preserved raw identity/selection/history after Keep or failed Reload, acknowledged revisions, silent own Save,
  Save As rebinding, one deduplicated watcher, serialized mouse-grab-safe offers, and owner-deletion safety.
- COG toolbar checks require exactly Undo, Redo, Whitespace, Format, and Insert on each document page, with no Reload
  button. They cover distinct themed glyphs at the existing 20-pixel icon size and 30-pixel button height, unchanged
  Format/Insert menus and configured Reload action, whitespace presentation without a source edit, and continued
  automatic named Reload/Keep offers in every project profile

Core asset tests independently pin classic/IJIM SPR and SND filename maxima and maximum-plus-one rejection,
raw-import retention, profile errors, unchanged stream sentinels, exact external bytes, and generic UNI exact-case
keys/live row counts. The separate episode `CogStringsFile` tests retain the legacy canonicalization contract.

The resource group header popup owner-destruction case was added with the parentless stack-menu safety correction.
The previous `QMenu` had a deletable widget parent across `exec()`, so deleting that owner could delete the stack object
and terminate the shared test runner. That ownership risk was established by code audit; a live pre-correction crash
run was not performed. The bounded regression verifies the corrected owner-destruction path.

The checked-in application HIST vectors and their regeneration rules are documented in
[Application SED V1 HIST vectors](assets/README.md)

## Consistency-check regressions

Consistency-check coverage in `consistency_geometry_contract_tests.cpp` compares complete fixed issue vectors,
including severity and every navigation field, for rounded original-format geometry and invalid geometry in all
three game projects. It covers both precision modes, rule toggles, malformed inputs, persisted enum values, and
resource-settings isolation. `consistency_precision_dialog_tests.cpp` exercises the real settings dialogs: exact
labels and warnings, Cancel, Accept, Restore Defaults, persistence, and immediate checker refresh.

## UV cross-layer regressions

- Worker-rendered thumbnails prove equivalent 3DO 2.1 pixel UVs and 2.3 normalized UVs sample identically, including signed multi-tile coordinates
- Sector import/export tests cover ordinary normalized OBJ, exact case-insensitive legacy pixel markers, contradictory and near-prefix markers, classic 3DO 2.1, IJIM 3DO 2.3, pixel-to-normalized boundaries, invalid or overflowing UV data, anisotropic MAT conversion, and unresolved-dimension rollback
- OBJ and 3DO export preflight rejects incomplete or non-finite UV topology and invalid scales before opening the destination, preserving an existing file byte-for-byte on failure
- Real MainWindow tests cover JKDF2, MotS, and IJIM starter/Add Sector UV publication, including legacy saved shapes with anisotropic MAT dimensions and failure rollback, before and after native SED round trips, plus application Open transactions for JKDF2/MotS JKL, IJIM NDY, MotS JED, ASC, and Dark Forces LEV with exact adjacent-MAT normalization
- The complete Sector, Surface, Edge, and Vertex transform matrix verifies commit, Undo, Redo, authored UV bits, texture-scale bits, geometry changes, and render-cache publication; combined snap/orient handlers also cover translation-only, rotation-only, combined, and no-op paths, while irregular signed multi-tile UVs repeat the matrix after native Save/Open
- Geometry-transform preview planning mirrors the exact Surface and Sector commit vertex set across reciprocal portals, moves adjacent faces only at shared corners, keeps point and wireframe overlays attached, and preserves authored normalized UV/color tuples
- Production Surface Extrude Apply tests cover normalized SED V2 and legacy pixel-space SED V1 documents, complete generated texture state, reciprocal adjoins, selection cleanup, Undo/Redo, and render-cache publication
- UV editor integration covers normalized missing-MAT editing, pivots, exact and modal Move/Rotate/Scale, mirrors, Auto Align, Pack, Smart UV Project, Stitch, Copy Reference, Follow Active Quads, main/sub-grid snapping, nearby-island snapping, and Undo/Redo; exact Rotate/Scale additionally cover normalized versus anisotropic pixel storage, signed coordinates, identity, representable sub-tolerance edits, overflow, invalid dimensions/tags, colour preservation, and atomic rollback; malformed stale panel topology, non-finite coordinates/scales, and invalid coordinate-space tags disable transforms instead of inventing coordinates
- Smart UV Project rejects every invalid enum and non-finite numeric option without changing island state or publishing a partial report, while UV snapshot comparison treats non-finite values as unequal
- UV snapshot change detection retains representable near-zero edits below display tolerances; the real canvas rotation test verifies sub-millionth coordinates around the active pivot and one-step bit-exact Undo/Redo
- Legacy pixel documents stage the same operations through real per-surface MAT dimensions; missing resources and late malformed second-surface failures restore every earlier conversion, UV bit pattern, scale, coordinate-space tag, and history depth atomically
- Renderer-boundary tests require exact finite UV cardinality and finite scales before publishing filled Level triangles, while preserving wireframe visibility for malformed editable geometry and recovering after valid data is restored
- Native SED editor state, recovery, history, and cross-process clipboard tests retain the Level UV-space tag and exact authored values
- Grid and nearby-geometry tests reject overflow in transform and UV lattice projection, clamp broad-phase cells safely, preserve signed floor semantics, and cover invalid Vertex/Edge/Face candidate inputs without publishing a correction

These regressions are maintained in [render_preview_utility_tests.cpp](render_preview_utility_tests.cpp) and
[main_window_event_filter_tests.cpp](main_window_event_filter_tests.cpp)

## Function contract assertions

For each function under test, record successful, rejected, empty, boundary, and
non-finite inputs where applicable. Check the return value and every output or
in/out argument. Seed failure outputs with distinct sentinels and verify whether
the documented contract preserves or resets them; also check persistent state,
selection, dirty flags, and history when the operation can mutate a document.

Compare deterministic string outputs against complete fixed test vectors, including
punctuation, whitespace, case, ordering, and locale-specific decimal separators.
For temporary filenames, substitute only the fixture path into a fixed diagnostic
template. Substring or prefix checks do not establish the complete output contract.
Expected values must not be generated by the formatter or serializer being tested;
round trips supplement independent vectors rather than replace them. Membership
checks remain appropriate when testing filtering rather than a complete string.

A passing suite establishes the covered input classes, not exhaustive coverage of
all possible inputs. Record untested branches or environment-dependent behavior
explicitly instead of describing representative tests as complete coverage.

## Adding coverage

- Start with a focused regression that fails for the reported behavior
- Keep pure helpers in `sed_app_tests` and full action, modal, focus, settings, rendering, and history flows in `sed_app_main_window_tests`
- Cover success, cancellation, malformed input, stale asynchronous completion, unavailable resources, and rollback paths
- Assert the application result and the resulting core Level or serialized state at cross-layer boundaries
- Keep settings, recovery files, temporary files, and window state isolated from the developer's machine
- Let the focused app runner finish shared Qt worker tasks and deferred QObject deletion between module groups
- Register every new source in [application CMakeLists.txt](../CMakeLists.txt)

Run the focused group or executable while iterating, then run the complete Release CTest gate described in the
[developer documentation](../../../docs/README.md)

## Isolated orbit allocation tests

`sed_app_orbit_allocation_tests` exercises selected-Frame and active-Frame fallback
resolution without a QApplication event loop. It checks the complete pivot and
optional output sentinel, input immutability, and the UI boundary's distinction
between an absent pivot and an allocation failure. Actual viewport tests separately
check that a rejected gesture leaves camera and grid state unchanged.
