# Legacy compatibility principles

SED 0.4.0 is a ground-up C++/Qt implementation rather than a source-level translation of the Delphi/Pascal editor. The
original SED/JED remains an important behavioral reference for Sith file semantics and established level-design
workflows, but the modern application may use a clearer UI or safer internal architecture.

Use [porting_guide.md](porting_guide.md) for the detailed parity checklist and current gaps.

## Decision order

When changing behavior:

1. Preserve engine and file-format correctness.
2. Preserve designer-authored data, undo/redo, selection, adjoins, layers, and recoverability.
3. Match proven original-editor behavior where it remains useful and safe.
4. Prefer a modern WYSIWYG workflow when the original UI was indirect, ambiguous, or constrained by obsolete tooling.
5. Encode the chosen behavior in focused unit and regression tests.

Do not retain a legacy defect solely for visual parity. Do not remove unfamiliar behavior until its format, engine, and
workflow consequences have been traced.

## Architecture mapping

- Pascal math, geometry, level, value, and format units map to portable code under
  [src/core/include/sed](../src/core/include/sed) and [src/core/src](../src/core/src)
- Forms, dialogs, input routing, resource previews, and renderer code map to [src/app/src](../src/app/src)
- Deterministic behavior discovered while studying UI code belongs in a focused helper or
  [src/core](../src/core), not in a Qt event handler
- Original [shapes.tpl](../data/shapes.tpl), supported-game template databases, and category files are retained as
  repository runtime data under [data](../data)

## Pascal audit scope

The compatibility list is based on the compiled unit set in `sed.DPR`, the visible menu and keyboard entry points in
`src/JED_MAIN.PAS`, and the reachable feature forms behind those entry points. These are plain paths relative to the
former Pascal source root rather than links because the C++ repository does not ship the original Pascal tree. A Pascal
routine counts as an original feature only when a visible command, form control, or live keyboard path can call it.
This keeps implementation helpers and historical source fragments from inflating the parity target.

The audit deliberately excludes:

- hidden Cutscene Helper, Topics, and JED Tutor commands
- the commented-out Whip Ruler shortcut and other commented debug commands
- uncalled toolbar-script loading in `src/TBAR_TOOLS.PAS`
- units outside the compiled dependency path, including `src/FILES_OLD.PAS`, `src/JED_PLUGINS2_.PAS`,
  `src/JED_PLUGINS3.PAS`, `src/JED_OLE.PAS`, `src/GL3DFX.PAS`, and `src/MEMCHECK.PAS`
- renderer-backend implementation details that do not represent a level-designer workflow

## Original Pascal features not implemented

These gaps record original JED/Pascal SED capabilities that are not currently available in the C++ editor. They are
compatibility notes, not an automatic roadmap: any replacement must fit the modern document, undo, threading, and
security architecture.

| Original capability | Current C++ status |
| --- | --- |
| External editor plugins | Not ported. The live Pascal DLL plugin menu and plugin developer mode have no source-compatible C++ replacement. `src/JED_OLE.PAS` is excluded because it is not linked into the original application. A future extension system needs a new, safe C++ design. |
| Revert and backup commands | Modern autosave and recovery are implemented, but **Revert to Saved**, numbered **Backup Copy**, and `.bak` rotation are not. |
| Full clipboard duplication | Viewport entity copy/paste, including cross-instance system clipboard transport, exists, but **Paste in Place** and the original Insert-key duplication of selected Sectors, Things, and editor lights are not exposed. |
| Keyboard step movement | Gizmo movement, exact numeric translation, and snapping are implemented, but the original selection nudge along the construction-grid normal and fixed-step Thing movement commands are not exposed as keyboard actions. |
| Geometry construction and repair commands | Surface/Sector construction from selected vertices/faces, the designer-facing Planarize command, and the one-click **Fix solid surfaces** / **Fix passable adjoins** repairs are missing. The C++ core contains tested surface-flattening math, but no application command invokes it. |
| Make Door assistant | The Tool Window command that converts the selected Thing to a two-frame door and creates a configured `00_door.cog` instance is not implemented. |
| Surface-angle assistant | The Tool Window command that measures two selected surfaces and sets the construction grid to their intersection is not implemented as one workflow. Modern grid alignment and Multi-cut angle guides cover related, but not identical, work. |
| Grid-relative exact transforms | Numeric Translate/Rotate/Scale/Mirror commands and viewport gizmos are implemented, but the Tool Window's exact Rotate and Flip around the authored grid plane and Scale along the grid normal are not exposed as equivalent commands. Grid snapping and view/grid alignment do not provide the same pivot-and-axis operation. |
| Saved-shape deletion | Selected Sectors can be saved to `data/shapes.tpl` and reused by Add Sector, but the original Map Settings command for deleting a saved custom shape has no C++ UI. |
| Current-document project conversion | New projects can target every supported game, but the original command that cycles the open document between JKDF2, MotS, and IJIM is not implemented. |
| 3DO hierarchy authoring | Transactional 3DO Sector import/export, resource loading, and previews are implemented. The original level-hierarchy node editor—add/delete/reparent nodes, edit offsets/type, fill missing nodes, and load/save hierarchy data—is not. The Pascal application did not provide general 3DO mesh editing, so mesh authoring is not a legacy parity gap. |
| Project packaging, archive browsing, and game launch | The original JKDF2/MotS GOB/GOO project build, container browsing, Save-and-GOB, and launch-to-test workflows are not implemented. Runtime archive mounting and resource discovery are implemented separately. The incomplete Pascal IJIM packaging branch is not claimed as a parity target. |
| Preview diagnostics and display controls | The original preview overlays for solid floors, solid surfaces, and surfaces with special properties, its preview gamma adjustment, and its global map backface-culling toggle are not implemented. Selection highlighting, render modes, X-Ray selection, and consistency checks cover different purposes. |
| Camera-driven authoring utilities | Exact numeric camera position/orientation editing, moving a selected Thing to the camera, copying the camera pose as frame text, and inserting a Thing at the camera are not exposed. The old commands that synchronized two separate map/preview cameras are obsolete because C++ SED uses one viewport. |

ASC import is implemented, but **Save As ASC** / **Export As ASC** is not. LEV and ASC writing remain possible
forward-looking format work rather than missing original-editor features: the audited Pascal application imports those
formats and does not save or export them.

Modern SED deliberately replaces several old workflows instead of treating literal Pascal parity as the finish line.
Its transactional undo/history, depth-aware GPU picking and cycling, frame/path authoring, component snapping, and
connected/adjoined Surface or Sector transforms are maintained C++ features with focused tests. A difference from the
old UI or its internal drag loop is not by itself a gap.

The complete retained JKDF2/MotS Level Header is editable in Level Settings, including mipmap/LOD arrays, perspective
and Gouraud distances, pixels-per-unit, and Master CMP. LEV Open also exposes the original scale and texture-handling
choices plus optional matching `.O` object import. These completed workflows are no longer parity gaps.

The remaining compatibility boundaries are concrete: all MAT layouts found across the retail JKDF2/MotS inventories,
CMP palettes, and all 64 classic light-level translation tables are decoded, and JKDF2 Solid rendering resolves its
MAT palette index through the active light row. Non-identity active Sector CMP translations are not yet applied to
textured pixels in every renderer/preview; MotS hierarchy node flag `0x2` special shading is not reproduced;
custom 3DO meshes that are not reachable from the hierarchy may draw in SED but not in the game; type-2 sprite
orientation still needs representative visual comparisons; intersecting/cyclic translucency has no
order-independent-transparency solution; and normalized legacy-output comparison is not continuous. See
[porting_guide.md](porting_guide.md) for the detailed status, links, and intentionally modern replacements.

## Removal policy

Legacy or transitional logic may be removed only after the replacement behavior is understood and protected by tests
or authoritative comparison. Remove dead code, temporary paths, personal fixtures, and obsolete documentation rather
than carrying them as undocumented alternatives.
