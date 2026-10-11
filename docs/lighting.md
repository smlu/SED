# Lighting in SED

This is the canonical reference for lighting in the C++ editor. It documents the current implementation, the authored
data that survives a level save, the background baked-light workflow, the real-time dynamic-light preview, and the
reference-engine behavior that SED deliberately follows or deliberately does not follow.

The two implementation-oriented summaries in
[algorithms.md](algorithms.md#asynchronous-baked-lighting) remain useful, but this file owns the terminology, complete
data flow, equations, parity boundaries, upstream source concordance, and maintenance rules.

## Contents

- [Lighting terminology](#read-this-first-lighting-mode-has-several-meanings)
- [Light types and what they illuminate](#light-types-and-what-they-illuminate)
- [End-to-end data flow](#end-to-end-data-flow)
- [Ownership and source map](#ownership-and-source-map)
- [Persisted lighting data](#persisted-lighting-data)
  - [Baked editor lights](#baked-editor-light-sedlight)
  - [Stored baked output](#stored-baked-output)
  - [Surface, sector, and 3DO inputs](#surface-sector-and-3do-inputs)
  - [MotS ARCHLIGHTING authoring and preview](#mots-archlighting-authoring-and-preview)
    - [Interactive ARCHLIGHTING viewport submode](#interactive-archlighting-viewport-submode)
  - [Runtime Thing and Sector records](#runtime-thing-and-sector-records)
- [Viewport and authored light modes](#viewport-and-authored-light-modes)
  - [Numeric authored modes](#numeric-authored-modes)
  - [Viewport ceiling](#viewport-ceiling)
  - [Static composition](#static-composition)
- [Light edit mode](#light-edit-mode)
  - [Families and owners](#families-and-owners)
  - [Authoring and transforms](#authoring-and-transforms)
  - [Thing/template parameter serialization](#thingtemplate-parameter-serialization)
- [Baked lighting](#baked-lighting)
  - [User workflow and settings](#user-workflow-and-persisted-settings)
  - [Core inputs and validity](#core-inputs-and-validity)
  - [Baked-light algorithm](#new-baked-light-algorithm)
  - [Portal blocking](#portal-blocking)
  - [Sector ambient derivation](#sector-ambient-derivation)
  - [Automatic recalculation](#automatic-recalculation-exact-scope)
  - [Dirty-region selection](#dirty-region-selection)
  - [Worker and publication contract](#worker-coalescing-and-publication-contract)
  - [Transform preview](#baked-light-transform-preview)
- [Runtime dynamic-light preview](#runtime-dynamic-light-preview)
  - [Scope and status](#scope-and-status)
  - [Project/source matrix](#projectsource-matrix)
  - [IJIM Thing lights](#ijim-regular-thing-light)
  - [Classic Thing lights](#classic-regular-thing-light)
  - [Actor/player Headlight](#actorplayer-headlight)
  - [IJIM Sector point lights](#ijim-sector-point-light)
  - [Editor collector](#editor-collector)
- [Runtime shading equations](#runtime-shading-equations)
  - [World geometry](#world-geometry)
  - [IJIM Gouraud 3DO vertices](#gouraud-3do-vertices-ijim)
  - [Classic Gouraud 3DO vertices](#gouraud-3do-vertices-jkdf2mots)
  - [Diffuse 3DO faces](#diffuse-3do-faces)
  - [Final 3DO composition](#final-3do-composition)
  - [Ordering consequences](#ordering-consequences)
- [Classic Sector Tint](#classic-sector-tint)
- [GPU implementation and failure behavior](#gpu-implementation-and-failure-behavior)
- [Deliberate limits and known gaps](#deliberate-limits-and-known-gaps)
- [Upstream engine source concordance](#upstream-engine-source-concordance)
  - [OpenJones3D OpenGL and optional Phong branches](#openjones3d-opengl-branches--optional-phong-mode)
  - [OpenJones3D and IJIM](#openjones3d--ijim)
  - [OpenJKDF2, JKDF2, and MotS](#openjkdf2--jkdf2-and-mots)
- [Reference-to-SED mapping](#reference-to-sed-mapping)
- [Tests and verification](#tests-and-verification-map)
  - [Core baked tests](#core-baked-tests)
  - [Runtime and orchestration tests](#runtime-and-orchestration-tests)
  - [Maintenance checklist](#maintenance-checklist)

## Read this first: "lighting mode" has several meanings

Four independent concepts have historically been called a light or lighting mode. They must not be treated as one
setting.

| Concept | Current values | Persisted in the level? | What it controls |
| --- | --- | --- | --- |
| Viewport lighting ceiling | **Fully Lit**, **Lit**, **Diffuse**, **Gouraud** | No; viewport state only | The strongest shading path that the editor is allowed to display. |
| Polygon/model light mode | `TLightMode`: `None`, `Lit`, `Diffuse`, `Gouraud`, legacy higher values | Yes, per surface and per 3DO mesh/face | The authored shading mode. It is capped by the viewport mode at render time. |
| Light edit family | **Baked**, **Thing**, IJIM-only **Sector**, MotS-only **Arch** | The selected family is UI state; the edited source data is persisted | Which owner type is selected, drawn, picked, added, and transformed. |
| Evaluation pipeline | CPU baked output or GPU runtime preview | Baked output is saved; runtime preview is not | Whether illumination is written into texture-vertex colors or evaluated for the current frame. |

In particular:

- **Baked** means an editor `SedLight` used by the CPU recalculation pass.
- **Thing** means a runtime emitter owned by a Thing: a regular `light` value or an actor/player Headlight.
- **Sector** means the IJIM `AVERAGE LIGHT` point light stored in a sector.
- **Lit** in the View menu and **Not lit** in the Surface property editor are different controls even though the names
  sound contradictory.
- Thing and Sector runtime lights do not enter the baked texture-vertex calculation.
- A baked light does not become a game runtime Thing light merely because the viewport can draw both as gizmos.

## Light types and what they illuminate

These are the authored light-source types visible to a level designer. They have different owners, outputs, and target
sets; the shared word "light" does not make them interchangeable.

| Light type | What it is | What it illuminates | Where the result lives |
| --- | --- | --- | --- |
| **Baked Light** | An editor `SedLight` sampled by the CPU bake | Level-geometry Surface corners within range and reachable through non-blocking adjoins; in MotS, sources with **Affect Arch Things** also contribute to eligible Arch Thing model vertices | Surface RGB is written into `Surface::texVertices[].color`; optional ambient derivation can summarize baked Surface values into `Sector::ambient`; MotS Arch output is stored in `ARCHLIGHTING` |
| **Thing Light** | A runtime omnidirectional point light stored in a Thing/template `light` parameter | Normal dynamic sources illuminate level geometry and 3DO Things within their engine-defined radius; IJIM range `<= 0.01` is the documented flat/self-light exception | Evaluated every frame by the game and SED viewport; it never writes baked geometry colors |
| **Actor/Player Headlight** | A runtime omnidirectional point light enabled by actor/player `typeflags`, with `lightintensity` and an optional rotated `lightoffset` | Level geometry and 3DO Things within radius, from the actor-relative emitter position | Evaluated every frame by the game and SED viewport; it never writes baked geometry colors |
| **IJIM Sector Point Light** | A runtime point light stored in a Sector's `AVERAGE LIGHT` records | Only 3DO Things in that owning Sector and within radius; it does not illuminate level geometry | Evaluated every frame by the game and SED viewport; it never writes baked geometry colors |

A Baked Light is static authored lighting, not a dynamic light. Its regular output lights level surfaces through their
stored per-corner values. MotS adds an explicit **Affect Arch Things** flag for a separate bake output: flagged sources
also calculate model-vertex ARCHLIGHTING for effective Arch Things. Existing authored ARCH values are retained as the
baseline, so repeated bakes are stable and clearing the last flagged source restores those values. This is persisted in
the SED document and participates in Undo/Redo; export writes the resulting ARCH data through the normal MotS level
format path. The flag does not alter the regular surface bake. If **Ambient** derivation is enabled, Sector ambient may
also contribute to baseline Thing shading. Thing Lights and Actor/Player Headlights remain dynamic point lights in both
the engine and editor preview. The IJIM Sector Point Light is intentionally restricted to 3DO Things in its Sector.

`Sector::ambient`, `Sector::extraLight`, `Surface::extraLight`, authored 3DO vertex colors, and classic Sector Tint are
lighting inputs or color effects, not positioned light sources. They have no independent emitter position and radius.

## End-to-end data flow

```C++
editor SedLight records
        |
        v
copied Level snapshot -- CPU CalcLightingNew --> Surface::texVertices[].color
        |                                      + optional Sector::ambient
        |                                                    |
        +-- generation/fingerprint gate --> manual undo or --+--> static viewport inputs / saved JED-JKL
                                              source edit

Thing `light` / actor Headlight / IJIM Sector point light
        |
        v
resolve inherited values --> view-stable scene collector --> capped GPU payload
        |                                                        |
        +---------------- transform-preview overrides -----------+--> current-frame world/3DO illumination only
```

The split is intentional. Baked work is topology-heavy, cancellable CPU work whose result becomes authored level data.
Runtime work is frame-local visualization that follows the game light payloads and falloff equations without modifying
the document.

## Ownership and source map

| Responsibility | Primary C++ source |
| --- | --- |
| Level lighting records and flags | [level.hpp](../src/core/include/sed/level.hpp), [geometry.hpp](../src/core/include/sed/geometry.hpp) |
| Baked-light public API | [lighting.hpp](../src/core/include/sed/level/lighting.hpp) |
| Baked-light and sector-ambient algorithms | [lighting.cpp](../src/core/src/level/lighting.cpp) |
| JED/JKL/NDY light serialization | [jed.cpp](../src/core/src/assets/jed.cpp), [jkl.cpp](../src/core/src/assets/jkl.cpp) |
| Bake controls, source polling, workers, publication, transform preview | [light_bake.cpp](../src/app/src/main_window/light_bake.cpp) |
| Dirty influence, target, fingerprint, and result-transfer helpers | [light_bake_utils.hpp](../src/app/src/ui/lighting/light_bake_utils.hpp) |
| Viewport lighting menu and global ceiling | [view_menu.cpp](../src/app/src/main_window/menus/view_menu.cpp), [gl_view.hpp](../src/app/src/gl_view.hpp) |
| Static color composition | [render_lighting_utils.cpp](../src/app/src/render/mesh/render_lighting_utils.cpp) |
| Runtime emitter parsing, collection, formulas, and GPU layout | [dynamic_light_utils.hpp](../src/app/src/render/lighting/dynamic_light_utils.hpp) |
| Mesh payload construction | [gl_view_mesh.cpp](../src/app/src/render/gl_view_mesh.cpp) |
| GLSL evaluation, UBO upload, and runtime preview overrides | [gl_view_render.cpp](../src/app/src/render/gl_view_render.cpp) |
| Incremental Thing/Sector lighting refresh | [gl_view_fast_update.cpp](../src/app/src/render/gl_view_fast_update.cpp) |
| Light family state and owner selection | [state.cpp](../src/app/src/main_window/edit/state.cpp) |
| Light properties and source-correct transform UI | [panel.cpp](../src/app/src/main_window/properties/panel.cpp) and viewport transform modules |
| Light icons, range rings, and picking | [gl_view_gizmos.cpp](../src/app/src/render/gl_view_gizmos.cpp), [gl_view_picking.cpp](../src/app/src/render/gl_view_picking.cpp) |

## Persisted lighting data

### Baked editor light: `SedLight`

`Level::lights` contains `SedLight` records:

| Field | Meaning in the bake | Default for a new record |
| --- | --- | --- |
| `id` | Stable editor identity used for change matching | Allocated by the level |
| `flags` | `LF_NoBlock` (`0x01`) bypasses portal blocking; MotS-only `LF_AffectArchThings` (`0x20`) also bakes this source into eligible Arch Thing model vertices | `0` |
| `mark` | Scratch/source-sector index during calculation | `0`; not an authored bake input |
| `layer` | Editor organization and visibility | Current/default layer |
| `color` | RGB multiplier for MotS/IJIM bake output | White |
| `intensity` | Scalar contribution multiplier | `10.0` |
| `range` | Spherical cutoff radius | `2.0` |
| `position` | World-space source position | Origin |

The baked-light **Flags** editor exposes `0x01 - Not blocked` in all projects and `0x20 - Affect Arch Things` only in
MotS. Native `.sed` preserves the source flags and the authored ARCH baseline/provenance used for repeatable MotS
recalculation. JED export writes its editor `LIGHTS` section, while JKL/NDY Export writes
the optional editor `Lights` section only when the user enables **Lights** in the export options; that option is off by
default because the engine does not need editor bake sources at runtime. The starter level seeds one white baked light
at intensity `10`, range `2`. The UI constrains baked range and intensity to non-negative values. Do not generalize that
UI constraint to raw IJIM Thing RGB/range values, which deliberately retain their authored representation.

### Stored baked output

Each `Surface` has parallel polygon vertex indices and `TexVertex` entries. `TexVertex::color` is the baked per-corner
value consumed by Gouraud world rendering. A bake rewrites RGB but preserves alpha while clearing or accumulating. The
ambient-derivation compatibility step normalizes the complete stored color, including alpha, exactly as the legacy
routine did.

The output is part of the Level and is always retained by native `.sed`; compatible JED/JKL/NDY export paths write the
corresponding per-corner lighting values. Runtime lights never write this array.

### Surface, sector, and 3DO inputs

- `Surface::light` is the persisted polygon `TLightMode` integer.
- `Surface::extraLight` is a signed additive value. JK/MotS expose signed scalar intensity and IJIM stores signed RGB.
  Finite authored scalar and RGB values, including values outside `[-1,1]`, are preserved by JKL/NDY load. Export
  clamps them to signed `[-1,1]` by default; the **Clamp extra light** export option can be cleared to preserve them.
  Alpha remains a separate transparency channel with its existing `[0,1]` handling. Extra Light is not a light source
  and has no position or radius. This export choice does not affect renderer evaluation or the open document.
- `Sector::ambient` is the sector floor/camera ambient input. It may be manually authored or derived during a bake. A bake
  can derive RGB ambient from colored Surface texel-vertex values, but JKDF2/MotS Thing and sprite baseline lighting is
  scalar: SED averages the combined Ambient and Sector Extra RGB channels, then clamps that scalar to `[0,1]` before
  applying it to Things. This keeps a colored surface bake from hue-tinting classic Things and sprites. IJIM preserves
  the per-channel RGB baseline. This matches the scalar ambient setup in the upstream
  [Thing renderer](https://github.com/shinyquagsire23/OpenJKDF2/blob/master/src/Engine/sithRender.c#L2538-L2540)
  and [sector renderer](https://github.com/shinyquagsire23/OpenJKDF2/blob/master/src/Engine/sithRender.c#L2911-L2912).
- `Sector::extraLight` is an additional sector contribution.
- `Sector::tint` is a JKDF2/MotS color effect rather than a positioned light. Game Preview applies it as the original
  camera-sector screen effect for every scene draw. Editor mode visualizes it locally on world geometry and actual
  3DO/SPR Things so the authored result remains inspectable from outside the sector. Editor adjoins retain their owning
  sector's Tint instead of borrowing the mirror sector's Tint, so a portal MAT is not recolored before the camera enters
  the other sector. Translucent adjoins remain visible from either side as an editing aid. IJIM does not use Tint. An
  omitted IJIM `TINT` record is imported as `(0,0,0)` to preserve the engine's unused field state and is omitted again
  when still zero; the engine and SED renderer do not apply it as a visual effect.
- 3DO meshes and faces carry their own light modes, authored vertex colors, and face extra light. The effective face
  mode is capped by the mesh mode and viewport ceiling.

For JKDF2 and MotS, the Sector **Ambient** and **Extra** property rows are scalar authored values represented by equal
RGB channels. The editor accepts finite signed and HDR values without a `[0,1]` range; it displays six decimal places,
rejects non-finite or out-of-float-range commits, and keeps the alpha convention separate from the scalar value. The
renderer applies its existing display clamps later in composition, so editing a value does not rewrite it to the display
range.

Relevant flags are:

- `SECF_NOAMBINETLIGHT`: preserve the sector ambient instead of deriving it. The misspelling is the established symbol.
- `SECF_NORGBAMBINETLIGHT`: convert derived baked and surface-extra averages to scalar intensity before comparison.
- `SAF_BlockLight`: prevent the baked portal reachability walk from traversing that adjoin.
- `LF_NoBlock`: let a baked light skip the portal blocking test entirely.

### MotS ARCHLIGHTING authoring and preview

MotS uses effective Thing flag bit `0x8` as the **Arch flag** to identify Arch Things. In SED, this flag marks a
Thing rendered as part of level geometry and makes it eligible for MotS ARCHLIGHTING. Each
such Thing has a logical `ARCHLIGHTING` record: an explicit record is selected by its archive index, while an index of `-1` is treated
as a virtual all-zero record in the editor and synthesized during ordinary JKL export. SED has no separate bulk
Architecture lighting dialog; authoring uses the interactive Arch viewport submode described below. Clearing the
Arch flag leaves non-default ARCHLIGHTING data dormant in the SED document; re-enabling it restores the data when
the model topology is compatible. These records are per-vertex model data, not runtime point lights, so the editor has
no point-light range to edit.


The interactive MotS Arch submode has no Add-placement or Remove-binding tool. Every effective Arch Thing uses
one ARCH owner marker, including a Thing with virtual all-zero lighting; MotS draws no second generic Level
Geometry badge for that Arch Thing.
The marker exposes the model vertices for selection and editing. **Delete** resets an explicit non-default record to
all zero while retaining the Thing's Arch flag; deleting a virtual or already-zero record is a no-op. Compatible
ARCHLIGHTING clipboard Paste can promote an unflagged model target by setting instance `thingflags` bit `0x8`, while
preserving unrelated flags and the Thing template.

MotS JKL export normally writes a zero ARCHLIGHTING record for each effective Arch Thing, including virtual
zero defaults. The optional **Omit default zero ARCHLIGHTING records** export setting instead writes index `-1`; the
game then uses the 3DO's vertex lighting, which can look different from the editor's zero-record preview. The option is
off by default.

ARCHLIGHTING records use the current all-LOD topology signature: every source mesh must match the maximum vertex
count resolved across all geosets. Initialization allocates that maximum shape so lower LODs stay safe; a dormant record
is restored on reflag only if this topology still matches. In the accelerated viewport, eligible records replace the
model vertex RGB while leaving its source alpha untouched before normal face-opacity handling: opaque faces remain
opaque, and explicitly translucent faces follow their authored opacity rules. `mono` remains the record's brightness
channel for the software engine path and is never treated as opacity. Invalid indices, non-finite channels, missing models, or
topology mismatches leave cached model colors unchanged; the resource checker reports the applicable diagnostic and
parses each referenced 3DO filename at most once per check pass.

#### Interactive ARCHLIGHTING viewport submode

Architectural lighting is an editor-only fourth Light edit family. In MotS, every effective Arch Thing is an
ARCH owner, including one with no explicit archive index. An index of `-1` supplies a virtual all-zero record, so these
owners expose zero-valued model vertices for selection, preview, and editing. Their single ARCH owner marker remains
available when ordinary Thing gizmos are hidden. In JKDF2 and MotS, Thing flag `0x8` is the **Arch flag**; it marks
an Arch Thing as part of level geometry and, in MotS, makes it eligible for ARCHLIGHTING. Compatible ARCHLIGHTING
Paste can set that bit while preserving unrelated flags and the Thing template. IJIM uses bit `0x8` as the Whip-climb flag and does not display an Arch Thing marker.
Selection is vertex-first: clicking an owner marker selects
all of that owner's published vertices; the first plain vertex click narrows that owner-expanded selection to the exact
vertex. Once the set is explicit, a plain click on a selected member preserves the batch and an unselected member
replaces it; Shift builds an arbitrary subset and Ctrl removes vertices. A vertex double-click edits the exact current
set, while an owner-icon double-click expands the edit to every published vertex of all selected eligible ARCH Things.
Picking, hover, and markers read per-selected-owner caches of transformed
vertices, so they do not load a resource or rebuild a scene-wide cache per mouse sample. The transform toolbar
exposes **Select** only in Arch mode: Move, Rotate, Scale, grid, and orientation controls are hidden, and **T**/**E** do
not activate transforms. Leaving Arch restores the normal controls for the active editing mode. In Arch mode, a plain click on
empty space, a level surface, or a non-Arch Thing clears the current owner/vertex selection. Shift/Ctrl-click misses and empty marquee
drags preserve it; compatible ARCHLIGHTING Paste can target an unflagged model Thing. When visible, the Thing type,
regular Thing-light, and ARCH owner markers occupy separate lanes above the projected model bounds. In MotS the ARCH
owner marker also represents the Arch flag, so there is no second marker for that Arch Thing. The Arch Thing marker
is controlled through **Viewport Settings > Gizmos > Arch Things** in JKDF2 and MotS. IJIM does not display this
marker because its Thing flag `0x8` is the Whip-climb flag. A Headlight
marker remains at its authored rotated
`lightoffset` emitter position.

Each cached vertex keeps its Thing, LOD, authored/source mesh, and vertex identity. `Model3do::meshSourceIndices`
maps expanded hierarchy clones back to the source mesh ordinal consumed by `ARCHLIGHTING`; malformed mapping or a
transformed hierarchy without that metadata fails closed. Raw/simple models use ordinal identity. A clone therefore
reuses the source archive mesh, while each consumed render mesh still validates its ordered vertex prefix, including
lower LODs and surplus archive meshes.

Staged preview patches match that exact source identity and run through the ordinary lighting composition, preserving
face-extra, ambient, signed/HDR channel values, and source alpha until face-opacity handling. RGB and Mono are separate stored channels. The ARCH editor
normally renders the stored RGB channels; choosing Mono in the vertex popup displays the stored `mono` channel as
grayscale for the currently selected eligible owners. This authoring view is transient editor state and never changes
saved values. Game Preview always returns to the engine's RGB presentation; returning to the ARCH editor resumes its
selected authoring channel, while leaving ARCH mode resets the editor view to RGB.

The popup opens in the active RGB/Mono authoring channel. Its RGB view shows color with an intensity convenience
control over stored `red`, `green`, and `blue`; no intensity field is serialized. Its Mono view writes the stored
`mono` value directly. Input changes stage viewport data while the popup is open and leave the document and history
unchanged. Dismissing the buttonless popup by clicking outside, pressing Escape, or closing it commits a valid changed
selection as one undoable edit. Opening and dismissing without changes creates no history entry. If an edited field is
invalid, dismissal abandons the entire staged transaction and leaves the authored data and history unchanged. Stale
selection, model, topology, resource, or mode state clears the staged transaction. Untouched signed/HDR channels remain
bitwise unchanged, and value edits that split selected and unselected owners sharing a record use copy-on-write.

| Popup control | Behavior |
| --- | --- |
| **Edit channel** | Switch between the separate RGB and Mono values. Mono preview is grayscale in the ARCH editor. |
| **Color picker** | RGB channel only; chooses hue for the selected vertex set. |
| **RGB intensity** | RGB channel only; changing intensity alone scales each vertex's own RGB. The intensity is a UI convenience, not a stored field. |
| **Mono value** | Mono channel only; edits the finite signed/HDR value directly. There is no Mono multiplier. |
| **Mono slider** | Convenience range 0–1, synchronized with the raw Mono value. |
| **Dismiss** | Outside click, Escape, or window close commits one valid changed edit as a single Undo step. No-op dismissal adds no history; invalid or stale edits are discarded as a whole. |

The marker presentation is configured under **Preferences > Viewport > Selection markers > ARCH vertices**. **Marker size**
is a bounded 3–24 logical-pixel value with a 14-pixel default; drawing scales it once by the viewport display DPI, and
direct vertex picking derives its hit radius from the same value. **Use authored vertex color** is enabled by default and
uses the selected ARCHLIGHTING RGB channels. Clearing it uses the configurable **Fallback color** instead. Every marker
gets a dark contrast outline, and these preferences affect the editor overlay only; they never modify ARCHLIGHTING data.

### Runtime Thing and Sector records

Runtime sources are represented by existing game fields rather than a second editor-light array:

- regular Thing emitter: inherited or instance `light` value;
- actor/player Headlight: `typeflags & 0x4`, IJIM `lightintensity=(r/g/b[/range])` or classic scalar
  `lightintensity`, and optional local `lightoffset`;
- IJIM Sector point light: `Sector::pointLight`, serialized as `AVERAGE LIGHT INTENSITY`, `POSITION`, and `FALLOFF`.

An IJIM Sector point light is considered authored when any RGB channel, position component, `minRange`, or `maxRange`
is nonzero; color alpha is not part of the NDY record and is deliberately ignored. When all of those fields are zero,
SED hides the Sector-light gizmo and writes no `AVERAGE LIGHT` records. For an authored light, the NDY writer emits
each record independently only when that record's payload is nonzero. A valid light at the world origin can therefore
omit `POSITION`, and zero falloff values remain the original engine-derived sentinels. **Add Sector Light** creates a
white source at the sector vertex centroid with the first/outer falloff equal to the Sector AABB diagonal and the
second/inner falloff equal to one quarter of that distance. Representative IJIM regression data uses this pattern; the
largest measured text-rounding difference from the AABB diagonal is below `2.5e-7`.

SED resolves Thing fields through `thingFindValue`, so template inheritance is authoritative. The original loaders set a
transient emit flag as a parse side effect; an authored effective field is sufficient in SED, and the editor does not
require that transient bit to have survived in `thingflags`.

## Viewport and authored light modes

### Numeric authored modes

`TLightMode` is defined as follows:

| Value | C++ name | Surface UI name | Current viewport behavior |
| ---: | --- | --- | --- |
| `0` | `None` | **Fully lit** | Starts white and bypasses authored/directional lighting. |
| `1` | `Lit` | **Not lit** | Flat ambient/extra-light path; no authored vertex interpolation. |
| `2` | `Diffuse` | **Diffuse** | Flat face path. Runtime 3DO lights use the face normal. |
| `3` | `Gouraud` | **Gouraud** | Uses authored/baked vertex colors and runtime per-vertex contributions. |
| `4`, `5` | `Gouraud2`, `Gouraud3` | not normally exposed | Preserved as legacy values; the viewport Gouraud ceiling reduces them to Gouraud. |
| `-1` | `VertexDiffuseOnly` | internal/legacy | Returns the supplied vertex color directly. |

For a surface, the effective value is conceptually:

```C++
effective = min(authored surface mode, viewport lighting ceiling)
```

For a 3DO face, the mesh light mode first caps the face mode, and the viewport ceiling caps the result.

### Viewport ceiling

The View > Lighting menu owns `GlView::LightingMode`:

| Viewport choice | Ceiling | Consequence |
| --- | ---: | --- |
| **Fully Lit** | `0` | All normally lit polygons render through the full-white path; dynamic targets are disabled. |
| **Lit** | `1` | Allows the flat ambient/extra path but not Diffuse or Gouraud evaluation. |
| **Diffuse** | `2` | Enables flat 3DO face dynamic lighting where the authored face resolves to Diffuse. |
| **Gouraud** | `3` | Enables baked/authored vertex interpolation and runtime Gouraud lighting. |

The menu tooltips describe the legacy intent. The implementation detail that matters for maintenance is that
`polyCalcLitColor` consumes authored vertex color only when the *effective* mode is exactly Gouraud. Lit and Diffuse use
flat ambient/extra inputs; Diffuse gains normal-dependent behavior for dynamic 3DO face lighting. World dynamic lights
also require an effective Gouraud surface.

### Static composition

`polyCalcLitColor` supplies the static/base color before the runtime shader adds dynamic terms:

- Fully lit starts at white.
- For IJIM world geometry, Lit/Diffuse/Gouraud starts with Sector Extra Light plus Surface Extra Light; Gouraud then
  adds the authored texture-vertex RGB. Sector Ambient is a Thing camera input, not a world-surface input here.
- For IJIM 3DOs, Lit/Diffuse/Gouraud starts with the clamped Thing camera ambient
  (`Sector::ambient + Sector::extraLight`) plus face Extra Light; Gouraud then adds authored model-vertex RGB.
- A static classic path starts with face/surface Extra Light; Gouraud adds authored vertex scalar/color, then clamps
  against the caller-supplied floor (Sector Extra Light for world geometry or Thing camera ambient for 3DOs). A
  runtime-enabled classic path instead leaves authored color, signed extra light, and the ambient floor separate so
  the shader can preserve the original dynamic-light clamp order.
- A runtime-enabled IJIM path defers the RGB clamp until the original composition point in the shader. Alpha is still
  finalized on the CPU.

Classic Sector Tint is applied once later to completed HW vertex color before texture modulation. Game Preview uses
the camera's sector for every scene batch, including translucent adjoins. Editor mode uses the owning sector for world
surfaces and actual 3DO/SPR Things; an adjoin does not borrow the mirror sector's Tint. A classic translucent adjoin is
shown from either side in Editor mode so the filled side remains visible through its Not Drawn counterpart. Fully
transparent texture pixels add no visible tint. Selection, grid, gizmo, and missing-resource helper geometry remain
neutral. Tint is not included in any static/base vertex color above. In JKDF2, a lit Surface or 3DO face in **Solid**
geometry mode quantizes its completed scalar lighting to one of the active CMP's 64 light levels, then resolves the
selected MAT cel's `solidColor` palette index through that level's 256-entry table. Screen Tint is applied to the
resolved RGB afterward. Fully lit Solid geometry uses the base palette color. MotS RGB Gouraud rendering retains its
separate per-channel path instead of being reduced to a JKDF2 scalar lookup. Prepared preview assets retain the same
per-cel Solid color and complete CMP light ramp as the ordinary resource-loading path.

Sky surfaces and sectors marked as 3DO geometry are forced through the fully-lit surface path in the level mesh builder.

## Light edit mode

### Families and owners

Light edit mode is an umbrella over four owner-oriented families:

| Family | Selects/edits | Projects | Runtime or baked |
| --- | --- | --- | --- |
| **Baked** | `Level::lights` / `SedLight` | JKDF2, MotS, IJIM | CPU baked |
| **Thing** | Things resolving to a regular light or Headlight | JKDF2, MotS, IJIM | Runtime preview |
| **Sector** | Sectors with a usable point light | IJIM only | Runtime preview |
| **Arch** | MotS effective Arch Things and their model vertices | MotS only | Editor model-color preview |

Baked, Thing, and IJIM Sector light-gizmo clicks and box selection use the visible Level-face depth boundary in both
perspective and orthographic projection. Invisible single-sided backfaces cannot block a light viewed from outside;
front-facing or double-sided opaque walls still block it. Direct icon hit areas use logical pixels rather than a
world-space radius that shrinks with distance. Clicks and marquees share the same float-safe depth tolerance, and
X-Ray remains the explicit through-selection option. A Sector light uses this emitter policy rather than two-sided
Sector-volume picking simply because its owner is a Sector.

Each family remembers its own selection. Changing family routes outliner, property, picking, gizmo, and transform state
to that family; it does not reinterpret the same index as another owner type. Map Settings exposes the independent
**Baked lights**, **Thing lights**, **Sector lights**, and **Architectural lights** visibility choices. Architectural
visibility controls the MotS ARCH owner overlay and is not a runtime point-light setting: a fresh Editor defaults
it on, while a fresh Game Preview defaults it off. Entering the active Editor Arch submode forces the overlay on for
every effective Arch Thing owner, including virtual all-zero defaults, and leaving the submode restores the persisted
preference. The overlay can therefore be shown outside the submode, but interactive vertex preview is
Editor-only. The overlay has no dynamic emitter or range ring. The settings affect visualization only. Picking and
properties remain constrained to the active family, and the active family is drawn last so an overlapping always-visible
icon cannot conceal it.

Light-gizmo artwork is opaque for readability over level geometry (apart from the empty billboard background and
antialiased edge coverage); the Headlight icon shows an actor beside an omnidirectional source. A regular Thing-light
icon uses a camera-up lane above the owning Thing's complete cached 3DO, sprite, and helper bounds because that source
is owned at the otherwise easily obscured Thing origin. Drawing and direct picking share that presentation position,
while its authored emitter position and range ring retain the engine-space position. A Headlight is different: its
rotated authored `lightoffset` is already an explicit engine-space emitter, so its icon, direct/marquee picking, range
ring, and Move pivot all stay exactly at `Thing position + Thing orientation * lightoffset`, independent of the
owner model's visual bounds. The regular Thing-light Move handle stays at the Thing origin. Headlight
picking covers the painted actor/source footprint, and clicking a rendered Thing representation can select its regular
source in the Thing-light family. A Headlight exposes one effective range ring rather than presenting its two legacy
zero-range attenuation radii as independently authored Sector-style ranges. Regular Thing lights follow the same
single-ring presentation: IJIM authors one range directly, while JKDF2/MotS author one scalar that couples brightness
to two target-specific engine radii. SED keeps both classic radii in the runtime payload but draws only the larger,
outer reach; the dotted inner ring is reserved for genuinely separate falloff values such as an IJIM Sector light.
An IJIM Flat/self light gets the regular Thing icon so its authored parameter remains selectable, but no range ring
because it is not a scene emitter. Authored black Baked, Thing, Headlight, and Sector sources retain their gizmos for
editing; they do not contribute illumination or consume a collected Thing-light slot.

MotS Architectural owner icons use the same cached presentation anchor for drawing and direct picking. A click on the
painted Thing body can select its eligible Arch Thing owner, including a virtual all-zero owner, while ordinary depth
occlusion still applies; X-Ray selection
can peel through unrelated Things. Marquee selection emits every eligible owner inside the rectangle and reports
Replace, Add, or Remove from the active modifier. Release, cancellation, and pick-mode changes clear the rectangle and
drag state. Hidden or locked owners and non-MotS levels do not enter this overlay.

### Authoring and transforms

- In the viewport, the real **Add Light** command uses **A** as its sole default and toggles Add for the active Baked,
  Thing/Headlight, or IJIM Sector family. Menus, tools, and Keymap use the same command and configured binding.
  Custom and cleared bindings are retained. Arch has no Add action: it edits existing effective
  Arch Thing lighting rather than creating a source. The active light command is consumed even when placement is
  unavailable, without invoking another edit mode's Add or Adjoin command. **Delete** uses the shared deletion
  command and keeps its light-family behavior below. Focused text/resource editors and Fly navigation retain input.
- **Baked** adds a `SedLight`; Move edits its world position, Scale edits range, and Rotate is unavailable. Viewport
  placement uses the same automatic layer policy as Thing placement: a visible clicked/containing Sector contributes
  its layer, otherwise the last visible layer is used. If every layer is hidden, a valid containing-Sector layer is
  retained; an empty layer table normalizes to layer `0`. The layer is assigned in the undoable state before the new
  light is published to selection, rendering, and the Outliner.
- **Add Thing light** creates a regular source on a valid unlocked Thing. IJIM defaults to `(1/1/1/1)`; JKDF2/MotS
  default to scalar `1`.
- **Add actor headlight** is limited to actor/player owners. It sets the effective Headlight type flag without erasing
  inherited flags and materializes local `lightintensity` and `lightoffset` fields so the attachment is visible and
  saved on the actor instance. Usable inherited values are preserved and IJIM RGB is materialized in canonical
  four-component form. Legacy inherited RGB receives the one-unit range used by a new Headlight. Missing, malformed,
  or disabled-zero intensity defaults to IJIM `(1/1/1/1)` or classic `1`; an
  invalid offset defaults to `(0/0/0)`. A zero inherited default without the Headlight flag remains eligible for the
  tool to replace; once the flag is enabled, an authored black Headlight is an existing selectable source rather than
  a duplicate-attachment target.
- Regular Thing-light Move uses the Thing origin and moves the Thing owner. Headlight Move edits the local
  `lightoffset` instead; its drag preview changes the exact runtime emitter/icon without presenting the Thing XYZ as
  moved or adding a visual/model-bounds lift.
- Runtime resolution and gizmo discovery always use the effective value chain, so a regular light or enabled Headlight
  inherited only from a Thing template illuminates the viewport without first being copied onto the instance. Editing
  an inherited source materializes only the changed instance field. Deleting that local override reveals the template
  source again; Delete refuses an inherited-only source with an explanatory viewport message because template data
  cannot be removed from a placed Thing. For Headlights the flag, intensity, and offset are treated as one source while
  unrelated local actor flags are preserved.
- The authoring viewport also previews effective lights on disabled or invisible Things because SED keeps those owners
  drawn and editable. This is important for IJIM cutscene actors such as `indy_sh_actor`. It is an editor-state policy:
  the runtime-parity resolver still rejects those flags exactly as OpenJones3D/OpenJKDF2 do, and the Headlight still
  requires effective type flag `0x4`.
- Thing-light Scale edits the source's authored range/intensity semantics. For intensity-derived lights, brightness
  and range remain coupled as in the runtime engine.
- IJIM Sector-light Move edits `pointLight.position`; Scale edits only explicit nonzero falloff fields. An exact zero is
  an engine sentinel and remains derived rather than being replaced by a preview value.
- Runtime sources have no independent orientation, so Light Rotate is unavailable.
- Delete follows the active sub-object tool: Thing-light mode removes the selected regular/Headlight instance override,
  Sector-light mode clears `PointLight` without deleting its sector, and the Sector thrust tool clears the selected
  vectors plus `SECF_IJIM_UseThrust`. It never falls through to deleting the owning Thing or Sector in those modes.

The compact light-parameter popup is shared by Baked, Thing, Headlight, and IJIM Sector sources. RGB-capable sources
show their color palette directly rather than hiding it behind another button, followed only by scrubbable fields
meaningful to that source. JKDF2 Baked uses scalar intensity/range with no RGB palette; MotS/IJIM Baked uses
color/intensity/range; IJIM Thing uses color intensity/range; IJIM Headlight uses color intensity/range plus local XYZ
offset; classic Thing uses scalar intensity; classic Headlight uses scalar intensity plus offset; and Sector uses its
two authored falloff values. IJIM RGB can exceed one, so the popup represents it losslessly as a normalized display
color multiplied by a separate intensity. Double-clicking a gizmo opens the matching source popup. Every palette,
scrub, and valid typed change updates the document and viewport immediately. The comparatively expensive consistency
scan and Thing/Sector/Baked property-tree rebuild are deferred until the popup closes, when the item editor refreshes
once while retaining its scroll position. There are no
**OK**/**Cancel** buttons or native dialog title bar: clicking outside or pressing **Escape** dismisses the popup and the
complete interaction remains one undoable edit. This is the same interaction contract used by the Sector Ambient color
editor.

In the Sector-light family, double-clicking owner-sector geometry selects that Sector and frames its authored point-light
gizmo, even when the source is far from the clicked surface. Double-clicking a Sector without an authored point light
reports that there is no source to focus and does not create or mutate one.

The Sector item editor shows only a full-width **Add Point Light** action while the active Sector has no authored
source. Once present, the **Point Light** field itself contains icon-only remove/copy/paste actions. The Min and Max
scrubs are stacked vertically with one unframed vertical connected/broken-chain toggle at their right; the compact
viewport popup uses the same arrangement. Ranges are linked by default: editing either field preserves the current
ratio, with the Canyon 4:1 outer-to-inner relationship as the fallback for legacy zero/degenerate pairs; unlinking
permits independent edits. During an item-editor scrub, the paired field, every selected Sector, and the viewport range
gizmos update immediately; the complete drag remains one undoable edit.
Removing and immediately re-adding a source performs an atomic UI/model transition and resets the new source to linked.
The old Parameters/Edit row is omitted.

Range rings normally show the authored values the runtime uses. Baked and Thing-owned sources expose one editable
range/intensity relationship and therefore draw one ring at their outer reach. For IJIM Sector lights the serialized
`minRange` is the outer cutoff and `maxRange` is the inner full-strength radius; those legacy field names are
counterintuitive.
OpenJones3D lets `rdCamera_AddLight` derive either exact-zero field from average RGB. SED retains that derivation for
illumination, but omits the corresponding editor ring: changing color must not make a non-authored range control appear
to move. Newly added Sector lights have explicit AABB-derived ranges, so both rings are immediately visible and stable.

### Thing/template parameter serialization

Text TPL, JKL/NDY, and legacy JED files share a generic Thing/template value codec. Known tuple parameters are written
as one parenthesized slash token, including IJIM `light=(r/g/b/range)` and `lightoffset=(x/y/z)`. IJIM NDY
`lightintensity` has one deliberate compatibility exception: OpenJones3D's text parser scans only `(r/g/b)`, so an
RGB-only token loads into SED as `(r/g/b/1)` but saves back as `(r/g/b)`. A four-component editor value also saves as
RGB and therefore reopens with range `1`; authored Headlight range is intentionally lossy until the engine parser's
own fourth-component TODO is implemented. This exception does not alter regular `light`, arbitrary vector values, or
JKDF2/MotS scalar `lightintensity`.

The Template Editor presents inherited `light` and `lightintensity` alongside locally authored parameters. Editing an
inherited row creates a local child-template override and Restore reveals the parent value again. Its Type column and
edit hint make the storage contract explicit: IJIM `light` is a serialized RGB/range `vec4` tuple,
`lightintensity` is a serialized RGB `vec3` tuple, and JKDF2/MotS use scalar intensity for both names. The same
structured light component editor is available for local and inherited rows.

The generic tuple rule also covers `orient`, `vel`, `angvel`, `frame`, `eyeoffset`, `fireoffset`, `voicecolor`,
`flashrgb`, `spritestart`, and `spriteend`. Readers accept old bare slash tuples and normalize them in memory, while
unrelated slash-containing strings remain untouched. JKDF2/MotS scalar `light` and scalar `lightintensity` remain
scalar. `flashrgb` components are signed decimal integers, matching the engines' `%d` parser. `thingflags`,
`physflags`, `typeflags`, and `damageclass` use `%x` semantics and typed hexadecimal serialization, so a bare `10`
means `0x10` and is written canonically with the prefix.

## Baked lighting

### User workflow and persisted settings

The **Light bake** strip appears only in Light edit mode > Baked and is hidden in Game Preview. It contains:

| Control | Default | Behavior |
| --- | --- | --- |
| **Recalc. Visible** / **Recalc. All** | Last choice | Main utility action. Its scope follows **Visible only**; the arrow can instead select **Recalc. Selected** and remembers that choice. |
| **Recalc. Selected** | Off | Recomputes the current and previous affected range of selected Baked lights, while evaluating every light for correct combined output. It obeys **Visible only** and skips locked layers. |
| **Visible only** | On | Limits overlay manual/automatic targets to visible, unlocked layers. Off includes all unlocked layers. |
| **Ambient** | Off | Publishes ambient values derived by the compatibility routine. Off restores the exact pre-bake sector ambient. |
| **Auto** | Off | Watches baked `SedLight` changes and recalculates their dirty region. Mirrored by Edit > Light > Auto Recalculate Lighting. |

The **Recalculate Lighting** menu action is available in Light edit mode and asks for **Selected
sectors**, **Visible layers**, or **All unlocked sectors**. Both entry points call the same asynchronous new algorithm;
the difference is target selection.

All settings are stored in `QSettings` as `lighting/visible_sectors_only`, `lighting/update_sector_ambient`,
`lighting/auto_recalculate`, and `lighting/manual_recalculate_selected`.

Selected recalculation is a destination-scope optimization, not a selected-light-only lighting equation. Its dirty
union contains each selected light's current and last-observed old positive-range sphere, plus pending superseded
footprints when needed. The worker discovers affected sectors from that union and evaluates every current Baked light
for each dirty vertex. If a selected request is queued behind a running worker, its click-time union survives later
selection changes. Every queued manual request likewise retains its exact target/permitted set, Ambient option, and
scope-specific undo label rather than re-reading later UI state. Before transferring pending Auto state, the click observes any as-yet-unpolled light edits and
adds their old/new ranges to the same transaction; an unrelated last-moment edit therefore cannot leave stale baked
output behind. Cancellation, stale generation/fingerprint rejection, and shape validation remain all-or-nothing;
only an accepted result creates the single manual undo record.

### Core inputs and validity

`levelCalcLightingNewInterruptible` receives a copied `Level`, target sector indices, a progress/cancellation callback,
and optionally a set of dirty influence spheres. A light is usable only when:

- its containing sector can be found (`mark != -1` after source-sector lookup);
- `range > 0`;
- `intensity > 0`.

When no usable lights exist, the requested full or dirty scope is still cleared. This is required to remove stale
lighting after the last light is deleted, disabled through zero intensity, moved away, or reduced to zero range.

### New baked-light algorithm

For each requested calculation:

1. Validate targets and locate the containing sector for every usable baked light.
2. If this is a full bake, zero RGB for every target texture vertex. If it is a dirty bake, zero only referenced
   vertices inside at least one dirty influence sphere. Preserve alpha.
3. Compute each target sector AABB once.
4. Build the sector's candidate-light list by intersecting each light range sphere with that AABB.
5. For each structurally valid surface, reject a light whose source is not on the front side of the surface plane:
   `dot(lightPosition - firstSurfaceVertex, surfaceNormal) <= 0`.
6. For each requested/dirty surface vertex, reject the light when squared distance is outside the range sphere.
7. Unless `LF_NoBlock` is set, require portal reachability from the light's containing sector to the target sector.
8. Add the project-specific contribution.
9. Derive sector ambient for the target set. The application may restore the old ambient before publication when the
   **Ambient** option is off.

The scalar falloff is:

```C++
d = distance(lightPosition, vertex)
f = (range - d) / range
contribution = intensity * f^2        for d < range
```

Project composition is:

- JKDF2: add the contribution to the existing scalar intensity and convert back to grayscale color.
- MotS and IJIM in the current C++ core: add `light.color.rgb * contribution` per channel.

The routine does not normalize RGB after every source. Sector-ambient derivation performs the compatibility
normalization step after the bake.

### Portal blocking

The new path does not cast a generic collision ray against every surface. It performs a topology walk from the light's
source sector:

- only surfaces with an adjoin can be traversed;
- `SAF_BlockLight` adjoins are skipped;
- the light-to-vertex segment must approach/cross the portal consistently with its normal;
- the segment must cross the portal polygon;
- reaching the target sector means the contribution is unblocked.

A generation-stamped visited array avoids clearing one mark per sector for every light/vertex query. The generation is
advanced per query and the array is cleared only on integer wrap. `LF_NoBlock` bypasses this entire walk.

The older `levelCalcLighting` implementation remains available for compatibility/testing, but the application uses the
interruptible new path.

### Sector ambient derivation

`levelCalcSectorAmbients` deliberately keeps the legacy side effects:

1. Skip sectors with `SECF_NOAMBINETLIGHT`.
2. Normalize each stored texture-vertex color and average all referenced baked vertex colors.
3. Independently average Surface Extra Light.
4. If `SECF_NORGBAMBINETLIGHT` is set, convert each average to grayscale intensity.
5. Choose the brighter of the baked average and the Surface Extra Light average.
6. Store that value as sector ambient with alpha `0`.

Because this routine normalizes texture-vertex colors even when **Ambient** is off, the app preserves only the old
sector ambient value; normalized baked vertex output still publishes. The Sector Ambient property also exposes a
manual action for applying this derivation to selected sectors.

### Automatic recalculation: exact scope

Auto is intentionally narrower than "recalculate after any level edit":

- A 50 ms poll compares only `Level::lights`, i.e. baked `SedLight` sources.
- Records are matched by stable `id`.
- Position, color, flags, range, and intensity are observed. Scratch `mark` and editor layer do not alter bake math.
- Thing runtime lights and IJIM Sector point lights update the real-time preview and never schedule a bake.
- Geometry, adjoin, surface-mode, or sector-flag changes do not independently schedule Auto.

Geometry and topology still participate in the job fingerprint. If they change while a worker is running, its result is
rejected as stale; the change simply does not start a replacement bake by itself. This distinction should remain
visible in UI text and tests.

### Dirty-region selection

For every changed baked light, Auto forms the union of the old and new influence spheres. A positive finite old range
is retained even if the new light has zero intensity, so the old footprint can be cleared. Wholly zero-range states
have no influence.

Affected sectors are found in two stages:

1. range sphere versus sector AABB broad phase;
2. exact test against vertices actually referenced by sector surfaces.

The exact test prevents a sphere that crosses only empty space inside a large AABB from enqueuing the sector. Target
lists are deduplicated. Nested/duplicate influence spheres are removed or merged, and every current light touching a
target sector is evaluated inside the dirty region. That last rule preserves contributions from unchanged lights whose
ranges overlap the edited footprint.

### Worker, coalescing, and publication contract

- Manual work receives an empty dirty set and recomputes the complete target scope.
- Auto work receives the merged dirty spheres. Exact referenced-vertex target discovery runs in the worker rather than
  in the 50 ms GUI-thread source poll.
- The GUI thread makes a bake-only snapshot containing project type, Sectors, and baked lights. Things, COGs,
  templates, hierarchy, and resource-name tables are not copied because they cannot affect this calculation;
  `QtConcurrent::run` evaluates the snapshot on the global thread pool.
- Workers never mutate `Document` or Qt widgets.
- The 75 ms timer is a throttle, not a trailing debounce. Continuous scrubbing can therefore publish intermediate
  complete states.
- When a request supersedes an in-flight job, its cancellation flag is set. The active targets/influences are merged
  into the pending replacement so an intermediate footprint cannot be stranded.
- Cancellation is checked between sectors and at bounded inner work checkpoints (roughly every 64 work units),
  including portal traversal. Canceled or superseded jobs return without rescanning the live geometry at completion.
- Manual work reports progress through the status-bar percentage button and modeless dialog. **Stop** cooperatively
  cancels and discards pending work. Automatic palette/scrub/typed previews do not enqueue transient percentage or
  completion messages, keeping pointer and keyboard input responsive. Shutdown cancels and waits before `MainWindow`
  state is destroyed.

Publication happens on the GUI thread only when both guards still match:

1. source generation equals the generation captured by the job;
2. a source fingerprint of the current document equals the worker fingerprint.

The fingerprint includes project type, all baked light inputs, target scope, sector geometry/layer/flags/extra light,
surface flags/adjoins/modes/extra light/normals/vertex and texture-vertex structure, and other inputs that affect the
result. It intentionally excludes the output baked vertex colors and sector ambient. Thus applying one result does not
invalidate itself.

A valid manual result copies only target texture-vertex colors and optionally target ambient values into one undoable
result. Automatic publications are derived output folded into the source interaction that requested them; each worker
result does not create another undo snapshot. A Light history snapshot includes both its source records and the current
baked colors/Ambient, so undo/redo restores them atomically; an accepted automatic publication rotates that record's
identity so a result arriving after Save correctly makes the document dirty again. With **Ambient** off, publication
never copies the worker's start-time Ambient over a newer authored value. The viewport recomputes the affected level
colors and uploads only their cached VBO ranges without rebuilding geometry. Cached Thing shading is recolored only
when published Sector Ambient actually changed. Structural or cache-mapping failures conservatively fall back to a
full mesh rebuild, while source-generation or fingerprint mismatches still discard the stale result without partially
publishing it. Successful manual publication refreshes the relevant properties and reports both target sector count
and usable candidate-light count; automatic publication remains silent.

### Baked-light transform preview

When Auto is active, moving or range-scaling selected Baked lights has its own asynchronous preview:

- the drag captures an immutable pre-drag level baseline;
- workers copy that baseline, apply only the current translation or range scale, and bake the union of old/new plus
  pending dirty influences;
- the same visible/unlocked scope rule applies;
- a superseded preview is canceled while the viewport keeps showing the last complete preview;
- returning the handle to its origin restores the exact baseline immediately;
- before commit, baseline output is restored internally so the undo snapshot is correct;
- the last complete preview is then retained until the authoritative post-commit bake arrives, avoiding a one-frame
  flash of old lighting;
- every intermediate footprint painted during the drag is included in the final authoritative request.

This preview applies only to selected Baked lights. Thing and Sector transforms use direct GPU/runtime preview updates.

## Runtime dynamic-light preview

### Scope and status

Runtime preview is enabled by `kRuntimeDynamicLightPreviewEnabled = true`. It evaluates game-style emitters for the
current viewport frame and never writes baked vertex colors. Its goals are:

- preserve authored payload interpretation, radii, attenuation, light-mode gating, and important engine quirks;
- make runtime sources inspectable and transformable in the editor;
- remain deterministic and usable in an editor camera, including views that are not reachable by the game portal walk.

The collector is intentionally not a reimplementation of portal/PVS membership or traversal order. Formula parity and
collection parity are separate claims throughout this document.

Runtime-light evaluation has two persisted shading choices under **Preferences > Viewport > Rendering**:

- **Gouraud (per-vertex)** is the default and evaluates the original engine equations at world/model vertices before
  interpolation.
- **Phong (per-pixel diffuse)** is an optional editor enhancement for effective Gouraud geometry. It evaluates runtime
  diffuse lights in the fragment shader using perspective-correct world position and interpolated, normalized model
  normals. IJIM uses the
  smoother falloff from OpenJones3D's current `OpenGL` renderer branch. It is not an original-software-renderer parity
  mode and can be more expensive on large or translucent polygons.

The choice changes only frame-local runtime-light sampling on effective Gouraud targets. Diffuse retains its original
flat face-anchor evaluation in both choices. Light collection, authored light modes, Sector Tint project gating, and
persisted baked vertex colors are unchanged. Missing or unrecognized persisted values select Gouraud; Phong must be
opted into explicitly.

### Runtime contribution switches

The viewport Lighting popup and **View > Lighting** also expose independent **Thing Lights** and **Sector Point
Lights** checkboxes after the four shading modes. Both default on. Their application-wide preferences are stored at
`viewport/render_thing_lights` and `viewport/render_sector_point_lights`; the same shared actions are shown in the
Editor and Game Preview viewports. **Sector Point Lights** stays visible but disabled for JKDF2 and MotS, and its
checked state is retained when switching project type.

**Thing Lights** controls runtime Thing emitter and actor/player Headlight contributions to the scene, plus the
owner-only regular Thing self-light floor. When disabled, the runtime Thing collector is skipped before its per-frame
transform-override buffers are prepared, and Thing render colors receive no regular-light self contribution. It does
not hide sources or gizmos, change Thing transforms or placement previews, or alter baked colors and authored
lighting.

**Sector Point Lights** gates only the IJIM sector point-light addition while rendering 3DO Things. It leaves Sector
ambient and Extra Light interpretation intact and preserves the engine's existing Gouraud/Phong versus Diffuse
asymmetry. Neither switch changes baked lighting, authored ARCH or SPR lighting, classic Sector Tint, or light gizmos;
neither requests a bake. Changing Thing Lights refreshes only Thing appearance, while Sector Point Lights updates a
shader uniform without rebuilding meshes.

Named EDIT recovery state may include the optional root map
`runtimeLighting: { thingLights: bool, sectorPointLights: bool }`. If the map or either named flag is absent, that
value defaults on. Unknown future keys are ignored. A non-map value or a non-boolean known flag rejects that EDIT
workspace state while allowing the Level itself to recover with a warning.

### Project/source matrix

| Project | Regular Thing `light` | Actor/player Headlight | Sector point light | Shared collected Thing cap |
| --- | --- | --- | --- | ---: |
| JKDF2 | positive scalar intensity | positive scalar `lightintensity`; rotated `lightoffset` | none | 32 |
| MotS | positive scalar intensity | positive scalar `lightintensity`; rotated `lightoffset` | none | 32 |
| IJIM | raw `(r/g/b/range)` | raw `(r/g/b[/range])` `lightintensity`; rotated `lightoffset` | `AVERAGE LIGHT` | 64 |

Regular and Headlight sources share the cap. A Thing can resolve both, so one owner can occupy two slots. OpenJKDF2's
runtime traversal can also consume the same pool with transient saber-blade lights; SED does not simulate that gameplay
state. The engine-parity resolver rejects disabled, invisible, non-finite, or invalidly sectored Things. The viewport
deliberately relaxes only the disabled/invisible owner-state check for authoring inspection; invalid placement and all
actual light/type rules still apply. Destroyed runtime state is not represented in the editor document.

### IJIM regular Thing light

SED parses `light=(r/g/b/range)` without passing through `ColorF` normalization:

- RGB is kept verbatim, including values above `1`;
- a missing fourth component leaves range at `0`;
- negative range is treated as `0` for classification/radii;
- a source with no visible RGB does not emit.

Classification follows the original threshold:

```C++
range <= 0.01  -> Flat/self light
range >  0.01  -> Dynamic light, minRadius = maxRadius = range
```

A Flat source is not inserted into the scene collector. It adds its RGB only to the emitting Thing's camera ambient,
clamped to `[0,1]` while that Thing is drawn. A Dynamic source can illuminate world vertices and 3DOs.

### Classic regular Thing light

JKDF2 and MotS require a finite scalar intensity `I > 0`. SED constructs grayscale color `(I,I,I)` and derives radii
from the attenuation installed by `sithCamera`:

```C++
outer cutoff / minRadius = I / 0.4
inner radius / maxRadius = I / 0.8
```

The `minRadius` name is inherited from `rdLight`; numerically it is normally the larger, outer radius.

The same regular source also raises only its owner's camera-ambient floor to `clamp(I)` when that is brighter than the
sector floor. It remains in the shared scene collector, so the owner receives this self floor and can also receive the
ordinary positional contribution from its own emitted light. This owner-only rule is classic-only; it does not replace
IJIM's range-`<= 0.01` flat/self-light classification.

### Actor/player Headlight

A Headlight is valid only when:

- the effective Thing type is actor or player;
- effective `typeflags` contains bit `0x4`;
- `lightintensity` is present and its RGB is visible/positive for runtime contribution;
- the Thing is finite and in a valid sector; engine-parity resolution also requires it to be enabled and visible.

`lightoffset` is a local-space vector. SED constructs the Thing orientation from pitch/yaw/roll, rotates the offset,
and adds the Thing world position. IJIM stores the runtime value in the engine's four-component
`headLightIntensity`: RGB is color and alpha is copied to both initial radii. A nonzero alpha is therefore one shared
range. When alpha is exactly zero, `rdCamera_AddLight` derives outer `Iavg/0.4` and inner `Iavg/0.8` radii from
`(r + g + b) / 3`. JKDF2/MotS use the scalar intensity directly.

The current OpenJones3D text parser zero-initializes that runtime vector and scans only RGB; its source TODO calls out
the missing range form. For retail-compatible NDY text, SED consequently writes RGB only. NDY RGB input gets an
editor-only default fourth value of `1`, so changing the fourth value in SED is intentionally lossy across save/reload
and OpenJones still follows its zero-alpha derived-radius path for the serialized file. Regular IJIM Thing lights keep
their authored fourth range.
Headlights are scene emitters only: unlike a classic regular `light`, `lightintensity` never raises the owning Thing's
camera ambient.

### IJIM Sector point light

The sector record supplies RGB, position, `minRange`, and `maxRange`. Either range left at exactly zero is derived
independently, as `rdCamera_AddLight` would derive a missing radius:

```C++
Iavg = (r + g + b) / 3
if minRange == 0: outer/minRadius = Iavg / 0.4
if maxRange == 0: inner/maxRadius = Iavg / 0.8
```

The light is appended only while rendering 3DO Things in its owning sector; it does not illuminate level geometry.

There is an important original IJIM initialization quirk. The Thing-light slots are initialized with
`rdLight_NewEntry`, but the static Sector-light slot is left zero-initialized and only its color/radii/position are
copied. Therefore its `bEnabled` remains zero:

- `rdLight_CalcVertexIntensities` does not check `bEnabled`, so Gouraud 3DO vertices receive the Sector light;
- `rdLight_CalcFaceIntensity` checks `bEnabled`, so Diffuse 3DO faces skip it.

SED preserves this asymmetric result. Collected Thing lights remain enabled in both 3DO modes.

### Editor collector

Every frame, `collectRuntimeThingLights`:

1. resolves regular and Headlight candidates for every Thing, using inherited values;
2. applies live source transform and sector overrides when a drag is active;
3. rejects non-Dynamic sources and invalid owners, while retaining disabled/invisible editor-visible owners;
4. retains the complete valid scene set while it fits the 64-light IJIM or 32-light classic cap;
5. only on cap overflow, computes camera distance and chooses the nearest entries from that complete set, with
   collection order as the tie break;
6. restores deterministic Thing traversal order before uploading the selected lights.

Viewport collection is deliberately view-stable: it does not reject a source merely because its influence sphere is
outside the current camera frustum. Gouraud lighting is evaluated at original vertices before raster clipping, so an
off-screen source can light an off-screen vertex whose color is interpolated across the visible part of a clipped
polygon. Frustum-culling that source made the whole contribution pop as the camera moved or turned. No individual
emitter-frustum test participates in scene-light membership.

This makes membership camera-independent while the eligible set remains within the original 64-light IJIM or 32-light
classic cap. Above the cap, SED still selects the nearest sources as an editor fallback, so crossing a cap boundary can
change membership; shader evaluation order remains deterministic.

During a Thing transform, a sector override immediately disables an emitter outside the level and restores it upon
re-entry.

This is an editor policy, not engine collection parity. OpenJones3D and OpenJKDF2 collect while traversing visible
sectors/portals and `rdCamera_AddLight` appends each accepted source. Consequently the same payload and formulas are used,
but membership and order can differ near portals or above the cap. SED uses camera distance only to choose membership
when the cap is exceeded; it never uses distance as shader evaluation order. That distinction prevents unchanged IJIM
Gouraud surfaces from changing color merely because the editor camera crosses between two lights. Order still matters
for per-emitter clamping and for the IJIM Diffuse attenuation bug described below.

## Runtime shading equations

The following notation is used:

- `L`: light position;
- `P`: shaded world/model vertex or the face anchor;
- `d = |L - P|`;
- `C`: IJIM light RGB;
- `I`: classic scalar intensity;
- `N`: normalized vertex or oriented face normal;
- `n = max(dot(N, normalize(L - P)), 0)`;
- `clamp(x)`: component-wise `[0,1]` clamp.

### World geometry

World dynamic lighting is enabled only for an effective Gouraud surface. It is distance-only; surface normals are not
part of the per-vertex runtime contribution.

For IJIM emitter `j` inside its `maxRadius`:

```C++
q_j = C_j - (0.8 * d_j, 0.8 * d_j, 0.8 * d_j)
D_0 = (0,0,0)
D_j = clamp(D_(j-1) + q_j)              // clamp after every emitter
base = clamp(authoredVertexRGB + D_final)
final = clamp(base + surfaceExtraRGB + sectorExtraRGB)
```

Negative per-channel `q_j` terms participate before the per-emitter clamp. This makes collection order observable in
the original Gouraud path.

IJIM Phong world lighting follows the current OpenJones3D `OpenGL` shader instead. For `R = maxRadius`, let
`s = 1 - smoothstep(0.75R, 1.25R, d)`:

```C++
q_j = max((C_j - (0.8*d_j, 0.8*d_j, 0.8*d_j)) * s_j, 0)
D = clamp(sum(q_j))
base = clamp(authoredInterpolatedRGB + D)
final = clamp(base + surfaceExtraRGB + sectorExtraRGB)
```

This makes every accepted term non-negative and smooths the hard radius boundary. The authored/baked vertex value is
still interpolated normally and is not regenerated by the runtime shader.

Classic JKDF2/MotS use the scalar equivalent while `d < maxRadius`:

```C++
q_j = I_j - 0.8 * d_j
D = sum(q_j)                              // every accepted term is positive
base = clamp(authoredVertex + D)
final = max(clamp(sectorExtra), clamp(base + surfaceExtra))
```

The GPU payload stores scalar values as equal RGB channels. Stopping once the classic accumulator reaches full
intensity is equivalent to the shown inner clamp because every accepted contribution is positive. Classic Phong samples
the same equation at each fragment; it does not adopt IJIM's OpenJones3D smooth band. Sector Tint is a later camera-wide
stage and does not participate in this equation.

### Gouraud 3DO vertices: IJIM

The outer test uses `minRadius`; `maxRadius` is the inner full-strength boundary:

```C++
if d >= minRadius or n <= 0: contribution = 0
if d < maxRadius:            strength = 1
otherwise:                   strength = 1 - (d - maxRadius) / (minRadius - maxRadius)
contribution = C * n * strength
```

If the two radii collapse and the vertex is not inside the inner radius, the contribution is zero rather than dividing
by zero. Original authored vertex RGB is the initial Gouraud value.

IJIM Phong uses the newer OpenJones3D model equation for collected Thing/Headlight lights and the owning Sector light:

```C++
inner = 0.75 * maxRadius
outer = 1.25 * minRadius
a = 1 - smoothstep(inner, outer, d)
contribution = C * n * a
```

For a regular Thing light whose two radii are the same authored range `R`, the smooth transition spans `0.75R` through
`1.25R`. A Sector light retains its independently resolved inner/outer radii before those smoothing factors are applied.

### Gouraud 3DO vertices: JKDF2/MotS

For `d < minRadius` and `n > 0`:

```C++
contribution = (I - 0.4 * d) * n
```

The original routine can stop once scalar output reaches full intensity. SED preserves the same completed value; its
classic dynamic Gouraud composition is:

```C++
base = clamp(authoredVertex + sum(contribution_j))
final = max(clamp(cameraAmbient), clamp(base + faceExtra))
```

Classic Phong uses the same contribution and composition at the fragment's world position with its interpolated normal.

### Diffuse 3DO faces

Diffuse is a flat face calculation and all vertices receive the same contribution. The face's first vertex is the
anchor for direction, while falloff uses signed point-to-plane distance:

```C++
P0 = first face vertex
p  = dot(L - P0, N)
n  = max(dot(N, normalize(L - P0)), 0)
```

The face contributes only when `p < minRadius`, the direction length is nonzero, and `n > 0`. Double-sided faces run
separate front/back calculations with `N` and `-N`; the fragment shader selects the appropriate flat color using
`gl_FrontFacing`. The Gouraud/Phong preference does not change this path: matching OpenJones3D's `lightMode == 3`
fragment-light gate, authored Diffuse mode always retains this original face-anchor result.

For IJIM, OpenJones3D's `rdLight_CalcFaceIntensity` mutates the incoming attenuation inside the light loop:

```C++
a_0 = 0.4
a_j = p_j * a_(j-1)
contribution_j = (C_j - a_j) * n_j
```

SED intentionally preserves that order-dependent IJIM behavior.

For classic JKDF2/MotS, the OpenJKDF2 reference calculates each light independently:

```C++
contribution_j = (I_j - p_j * 0.4) * n_j
```

Both the C++ helper and GLSL use a fresh `0.4` for every classic light. Accumulation stops after the first result that
reaches or exceeds `1`, preserving that light's overshoot. The overshoot remains significant because signed face Extra
Light is applied before the final clamp:

```C++
D = accumulate contributions until D >= 1
final = max(clamp(cameraAmbient), clamp(D + faceExtra))
```

### Final 3DO composition

For Lit, Diffuse, and Gouraud faces, face Extra Light is present. Gouraud additionally starts with authored vertex
color. For classic dynamic Gouraud, authored plus dynamic is clamped before signed face Extra Light; Diffuse retains
its first saturation overshoot until Extra Light is added. The independently clamped Thing camera ambient is then used
as a lower floor. For IJIM, camera ambient is the independently clamped sum of sector ambient and sector extra light;
IJIM has no Sector Tint stage.

The renderer clamps evaluated lighting at its existing composition stages. Persistence does not apply that display
ceiling to authored Surface Extra Light, so reopening and exporting an untouched HDR value preserves the source value.
The classic scalar Surface property editor supports HDR values while editing. The IJIM color picker retains its
existing unit-range explicit-editing behavior.

Gouraud clamps the complete per-vertex RGB once before hardware interpolation and texture/material modulation. Phong
performs the corresponding project-specific clamp at the fragment after Gouraud-target runtime evaluation. Diffuse,
Lit, and static targets remain on their existing vertex path in both modes.

### Ordering consequences

Light order can affect the original Gouraud/Diffuse result in three places:

- world illumination clamps its accumulator after every emitter;
- IJIM Diffuse mutates attenuation between contributing lights;
- the collection cap chooses a subset before evaluation.

SED's distance/Thing-index ordering makes editor output stable, but it is not the visible-sector traversal order of the
games. A parity test below the cap should specify the intended light order whenever the formula itself is order-sensitive.

## Classic Sector Tint

JKDF2 and MotS copy the current camera sector's Tint into the screen/palette color effect once per frame. The authored
Tint channels are first clamped to `[0,1]`. `rdCache` then derives these multipliers:

```C++
scaleR = 1 + tintR - 0.5 * (tintG + tintB)
scaleG = 1 + tintG - 0.5 * (tintR + tintB)
scaleB = 1 + tintB - 0.5 * (tintG + tintR)
```

This camera-wide `Sector::tint` is distinct from the state installed while each visible sector is drawn. World
geometry, Things, and deferred alpha adjoins use their containing/owning sector's Ambient, Extra Light, and CMP. A CMP
also has its own RGB color multipliers, but those are colormap data rather than the JKL Sector `TINT` field. OpenJKDF2's
per-sector world, Thing, and alpha-adjoin loops do not replace the global screen Tint while traversing portal sectors.

Game Preview applies that camera-sector multiplier to ordinary completed scene vertices and clamps it before
interpolation and texture/material modulation, matching the original HW diffuse-color stage. This intermediate clamp
matters when Tint boosts a channel above `1`; clamping only after sampling a dark texture would make that channel too
bright. The effect covers classic sky, ordinary world, 3DO, and sprite scene draws. Moving a Thing between sectors therefore
does not recolor it from the destination sector's Tint—the camera sector remains authoritative for the complete Game
Preview scene. If the camera is outside all sectors the Game Preview effect is neutral.

That neutral result is also correct when an out-of-world editor viewpoint can see artificially exposed sector interiors,
for example through the back or missing side of the level shell. The games normally reach visible sectors by walking
adjoins from a camera contained in a valid starting sector; an outside camera exposing internal geometry is therefore
an editor inspection case, not a valid engine Tint-parity view. To compare Game Preview with the engine, place the
camera inside a sector and inspect the scene reachable from there. Geometry seen through an adjoin still uses the one
Tint selected from the camera's current sector; the traversed or mirror sector does not replace it.

Material resolution remains independent of that choice. **Texture** geometry samples the MAT image. JKDF2 **Solid**
geometry uses the MAT cel's palette index and the active CMP light table selected by its completed scalar light level,
then applies the camera Tint. This distinction is visible in `10cargo.jkl`: `00RED.mat` uses palette index `6`, and
`10cargo.cmp` maps that index back to saturated red at every light level. Multiplying the red palette RGB by the low
surface intensity would incorrectly make the portal nearly black. A mirror sector's Tint does not modify an Editor
adjoin's MAT; that Tint becomes the camera-wide effect only after the Preview camera enters the mirror sector.

Editor mode deliberately makes the authored field inspectable: world triangles and actual 3DO/SPR Things carry their
owning sector's Tint scale, so a tinted sector remains visible when viewed from outside. Adjoin triangles retain that
same owner-sector scale and never take the mirror sector's Tint. Translucent adjoins are also drawn from either side in
Editor mode, allowing their filled side to remain visible through a Not Drawn mirror. Switching to Game Preview replaces
all local authoring Tint with the engine-style camera-sector-wide effect. Selection, grid, gizmo, and missing-resource
helper draws remain neutral. Both paths are explicitly disabled for IJIM, whose OpenJones3D renderer does not consume
`Sector::tint`.

Editor mode intentionally does not run a portal-visibility or camera-containment test to suppress this local authoring
Tint. Consequently, an exposed interior surface or Thing can appear tinted—and sometimes brighter than Game Preview—
while the editor camera is outside the level. This is useful for inspecting the value before entering the sector and is
not a prediction of what an out-of-world game camera would render. Use Game Preview from a valid containing sector when
judging engine-facing Tint composition.

Tint and translucency are independent. OpenJKDF2 queues an adjoin for the alpha pass when its face has
`RD_FF_TEX_TRANSLUCENT` or its MAT supplies alpha. The explicit face flag assigns every hardware vertex alpha `90`,
which SED reproduces as `90/255`; texture alpha then modulates that value. The alpha-adjoin pass reads depth but does
not write it. MAT-alpha adjoins without the explicit flag still use the translucent pass, while ordinary alpha-tested
walls retain their normal path. IJIM keeps its authored alpha and does not use this classic constant.

With optional Phong runtime lighting, effective Gouraud targets defer this same completed-light/pre-texture Tint
operation to the fragment shader. Diffuse/non-dynamic targets and the default Gouraud mode keep the vertex/HW-diffuse
placement. IJIM is neutral in both paths.

## GPU implementation and failure behavior

The dynamic Thing payload is a std140 uniform block at binding `3`:

```C++
64 * vec4(position.xyz, minRadius) = 1024 bytes
64 * vec4(color.rgb, maxRadius)    = 1024 bytes
total                              = 2048 bytes
```

The C++ mirror is 16-byte aligned and compile-time checked for offsets and size. Classic projects use only the first 32
entries but share the same maximum-capacity block. The UBO avoids consuming another 512 default-uniform components on
OpenGL 3.3 implementations with tight limits. Vertex attributes used by IJIM for its per-Thing Sector point light are
reused only in classic projects to carry signed face/surface Extra Light and the ambient floor; the project branch
keeps those interpretations disjoint.

The same block is declared in both shader stages. Gouraud consumes it in the vertex shader; Phong passes effective
Gouraud targets, world position/normal, and project-specific payload to the fragment shader. Model targets are chosen
from `min(face light mode, mesh light mode, viewport ceiling)`, so lowering the viewport to Diffuse cannot leave a
Gouraud runtime target active. Incremental Thing transforms update both the vertex/normal and the cached Diffuse face
anchor; committed moves therefore do not leave flat lighting attached to the old pose.

Startup verifies the shader block, reported block size, binding, buffer allocation, and upload support. There is no CPU
dynamic-light fallback. If the UBO-enabled shader cannot compile or link, SED emits one warning and relinks a fallback
shader with all runtime Thing and IJIM Sector additions disabled. If block validation, allocation, binding, or a later
map/upload fails after the UBO-enabled shader has linked, the uploaded Thing-light count remains zero; the compiled IJIM
Sector point-light path can still evaluate its mesh payload and preview overrides. Static base shading and the rest of
the viewport continue to render in either failure mode.

IJIM Sector transform previews are separate from the shared Thing UBO. Up to 16 owner-keyed preview records override
the point-light payload already stored on affected 3DO vertices. Crossing sectors or changing a Sector light updates
the relevant payload immediately without rebuilding unrelated level geometry.

Mesh vertices tag their dynamic target:

| Tag | Meaning |
| ---: | --- |
| `0` | no runtime dynamic evaluation |
| `1` | world Gouraud vertex |
| `2` | 3DO Gouraud vertex |
| `3` | 3DO Diffuse face |

## Deliberate limits and known gaps

- Runtime preview has no dynamic-light shadowing or solid-wall occlusion test.
- Its collector considers every valid authored scene emitter; it uses neither individual emitter-frustum tests nor
  portal/PVS membership.
- Membership and order above the per-project cap are editor-deterministic rather than engine-identical.
- Transient gameplay-created sources such as OpenJKDF2 saber-blade lights are not simulated by the level editor.
- Runtime illumination is preview-only; it does not dirty or save baked vertex colors.
- Phong runtime lighting is an optional OpenJones3D-inspired editor enhancement, not an original-software-renderer
  parity claim; Gouraud remains the default.
- Editor mode's local authoring Tint is intentionally independent of portal visibility and camera containment. An
  artificially exposed sector interior can therefore remain tinted when viewed from outside the level shell
- Auto bake observes only `SedLight` changes, not runtime sources or arbitrary geometry edits.
- Baked portal traversal is regression-covered for front-side rejection, cyclic graphs, `SAF_BlockLight`, and
  `LF_NoBlock`. No missing traversal branch is currently known; representative pathological maps remain useful visual
  or golden comparisons when that code changes.
- MotS support shares the classic scalar runtime source model, but unusual MotS RGB/spot-light assets need dedicated
  compatibility maps before claiming complete renderer parity. The shaded viewport does not reproduce MotS special
  shading for 3DO hierarchy nodes marked `0x2`; that flag's collision-radius preview is a separate behavior.
- Classic CMP parsing retains the palette and all 64 256-byte light-level translation tables. Solid-color JKDF2
  geometry consumes the resulting per-MAT light ramp in both ordinary and prepared-preview paths. The final table is
  still exposed for original SED `LoadCMPTable` compatibility; it is not a header Tint and is separate from JKL Sector
  `TINT`. The remaining boundary is applying a non-identity active Sector CMP palette translation consistently to
  textured pixels in every renderer and preview.

## Upstream engine source concordance

These links are evidence for runtime semantics, not dependencies of the SED build. They are pinned to exact commits so
line ranges remain stable:

- OpenJones3D: commit
  [`a675b395a5d5cff238332d091368483b849e8b5b`](https://github.com/smlu/OpenJones3D/commit/a675b395a5d5cff238332d091368483b849e8b5b)
- OpenJKDF2: commit
  [`0dbd1ea8bb2211c158f6af928b5e4fb14dfa0dac`](https://github.com/shinyquagsire23/OpenJKDF2/commit/0dbd1ea8bb2211c158f6af928b5e4fb14dfa0dac)

OpenJones3D is the primary IJIM/Jones3D reference. OpenJKDF2 is the primary JKDF2/MotS scalar-light reference. Both
are reverse-engineered reimplementations and include their own fixes/QOL branches; a linked behavior is not automatically
a claim that every build switch or later enhancement belongs in SED.

### OpenJones3D OpenGL branches / optional Phong mode

The optional mode is documented against these pinned OpenJones3D renderer revisions:

- `OpenGL` at [`0d3cfd896d67c11bd61d735531f9c543981f6c70`](https://github.com/smlu/OpenJones3D/commit/0d3cfd896d67c11bd61d735531f9c543981f6c70)
- `OpenGL-exp` at [`bc453b46a25e6448a90472a0a1aeb37933e16443`](https://github.com/smlu/OpenJones3D/commit/bc453b46a25e6448a90472a0a1aeb37933e16443)

The older experiment uses `1 - smoothstep(0.5R, R, d)` and multiplies `(C - 0.8d)` by that range and `N.L` for model
lights, while its Sector-light function retains the original two-zone linear radius split. See
[`OpenGL-exp/common.incl`](https://github.com/smlu/OpenJones3D/blob/bc453b46a25e6448a90472a0a1aeb37933e16443/Libs/std/Win95/GL/Shaders/common.incl#L50-L95)
and [`OpenGL-exp/model.frag`](https://github.com/smlu/OpenJones3D/blob/bc453b46a25e6448a90472a0a1aeb37933e16443/Libs/std/Win95/GL/Shaders/model.frag#L24-L75).

The newer `OpenGL` branch replaces that model equation with normalized Phong `N.L` and
`1 - smoothstep(0.75*maxRadius, 1.25*minRadius, d)`, applies the same helper to the owning Sector light, and changes the
world band to `0.75R..1.25R`. See
[`OpenGL/common.incl`](https://github.com/smlu/OpenJones3D/blob/0d3cfd896d67c11bd61d735531f9c543981f6c70/Libs/std/Win95/GL/Shaders/common.incl#L50-L128)
and [`OpenGL/model.frag`](https://github.com/smlu/OpenJones3D/blob/0d3cfd896d67c11bd61d735531f9c543981f6c70/Libs/std/Win95/GL/Shaders/model.frag#L25-L39).
The GL submission path sets `bRenderLights` only when `lightMode == 3`, so Diffuse is not converted into fragment
lighting ([`std3GL.c`](https://github.com/smlu/OpenJones3D/blob/0d3cfd896d67c11bd61d735531f9c543981f6c70/Libs/std/Win95/GL/std3GL.c#L1617-L1670)).
SED therefore follows the newer branch for optional Phong Gouraud targets and retains its original engine path for the
default Gouraud and all Diffuse targets.

### OpenJones3D / IJIM

| Module | Relevant original responsibility and SED use | Pinned source |
| --- | --- | --- |
| `sithRender` definitions and frame pipeline | Defines the 64-Thing-light half of the 128-camera-light pool, sets the global light mode, builds visible sectors, dynamic world lights, then Things. | [`sithRender.c` definitions/frame flow](https://github.com/smlu/OpenJones3D/blob/a675b395a5d5cff238332d091368483b849e8b5b/Libs/sith/Engine/sithRender.c#L46-L123), [`Draw` sequencing](https://github.com/smlu/OpenJones3D/blob/a675b395a5d5cff238332d091368483b849e8b5b/Libs/sith/Engine/sithRender.c#L270-L322) |
| `sithRender` light collection | Collects regular emitted Thing lights and actor/player Headlights while visiting visible sectors; applies flags, the `0.01` dynamic threshold, rotated offset, and the shared cap. | [`sithRender_BuildVisibleSector`](https://github.com/smlu/OpenJones3D/blob/a675b395a5d5cff238332d091368483b849e8b5b/Libs/sith/Engine/sithRender.c#L596-L667) |
| `sithRender` world dynamic calculation | Broad-phases lights by sector sphere, computes `color - distance*attenuationMax`, clamps after each emitter, and caches by render tick. | [`sithRender_BuildDynamicLights`](https://github.com/smlu/OpenJones3D/blob/a675b395a5d5cff238332d091368483b849e8b5b/Libs/sith/Engine/sithRender.c#L906-L966) |
| `sithRender` Thing/sector lighting | Builds sector camera ambient, appends the IJIM Sector point light for model Things, handles flat self light, and draws Things. | [`sithRender_RenderThings`](https://github.com/smlu/OpenJones3D/blob/a675b395a5d5cff238332d091368483b849e8b5b/Libs/sith/Engine/sithRender.c#L968-L1100) |
| `rdLight` | Defines light initialization, IJIM Gouraud inner/outer-radius RGB equations, Diffuse face-plane equation, enabled check, and mutable attenuation behavior. | [`rdLight.c`](https://github.com/smlu/OpenJones3D/blob/a675b395a5d5cff238332d091368483b849e8b5b/Libs/rdroid/Engine/rdLight.c#L38-L196) |
| `rdCamera` | Owns attenuation and camera light slots; derives either zero radius from average RGB divided by `0.4/0.8`. | [`rdCamera_SetAttenuation` and `rdCamera_AddLight`](https://github.com/smlu/OpenJones3D/blob/a675b395a5d5cff238332d091368483b849e8b5b/Libs/rdroid/Engine/rdCamera.c#L471-L518) |
| `sithCamera` | Installs the runtime camera attenuation used by `rdCamera` and all formulas. | [`sithCamera.c`](https://github.com/smlu/OpenJones3D/blob/a675b395a5d5cff238332d091368483b849e8b5b/Libs/sith/Engine/sithCamera.c#L230-L290), [`sithCamera.h` constants](https://github.com/smlu/OpenJones3D/blob/a675b395a5d5cff238332d091368483b849e8b5b/Libs/sith/Engine/sithCamera.h#L22-L30) |
| `rdModel3` | Caps mesh/face light modes, transforms camera lights to model space, runs Gouraud vertex or Diffuse face lighting, handles backfaces, face extra, and camera ambient. | [`rdModel3_Draw` through `rdModel3_DrawFace`](https://github.com/smlu/OpenJones3D/blob/a675b395a5d5cff238332d091368483b849e8b5b/Libs/rdroid/Primitives/rdModel3.c#L1503-L1760) |
| `rdThing` | Defines default Thing render modes, binds an `rdLight`, and dispatches model/sprite/particle/polyline drawing. | [`rdThing.c`](https://github.com/smlu/OpenJones3D/blob/a675b395a5d5cff238332d091368483b849e8b5b/Libs/rdroid/Engine/rdThing.c#L38-L220) |
| `sithThing` | Parses IJIM `light=(r/g/b/range)`, copies raw RGB/range into runtime fields, and sets the emit flag. | [`sithThing_ParseArg` light case](https://github.com/smlu/OpenJones3D/blob/a675b395a5d5cff238332d091368483b849e8b5b/Libs/sith/World/sithThing.c#L3325-L3352) |
| `sithActor` | Parses `lightoffset` and currently only RGB `lightintensity`; its source TODO calls out the missing range form, while the actor field itself is a four-component vector. | [`sithActor_ParseArg` light cases](https://github.com/smlu/OpenJones3D/blob/a675b395a5d5cff238332d091368483b849e8b5b/Libs/sith/World/sithActor.c#L925-L952) |
| `sithSector` | Reads/writes IJIM `AVERAGE LIGHT INTENSITY`, `POSITION`, and `FALLOFF`. | [`write`](https://github.com/smlu/OpenJones3D/blob/a675b395a5d5cff238332d091368483b849e8b5b/Libs/sith/World/sithSector.c#L60-L80), [`read`](https://github.com/smlu/OpenJones3D/blob/a675b395a5d5cff238332d091368483b849e8b5b/Libs/sith/World/sithSector.c#L195-L225) |
| `sithWorld` | Owns and allocates the RGB dynamic-light result and render-tick arrays indexed by world vertex. | [`SithWorld` fields](https://github.com/smlu/OpenJones3D/blob/a675b395a5d5cff238332d091368483b849e8b5b/Libs/sith/types.h#L2506-L2519), [`allocation`](https://github.com/smlu/OpenJones3D/blob/a675b395a5d5cff238332d091368483b849e8b5b/Libs/sith/World/sithWorld.c#L415-L443) |
| `rdClip` | Combines dynamic and authored world-vertex values and clamps them before raster/cache composition. | [`rdClip.c`](https://github.com/smlu/OpenJones3D/blob/a675b395a5d5cff238332d091368483b849e8b5b/Libs/rdroid/Engine/rdClip.c#L3985-L4040) |
| `rdCache` | Applies effective light mode, face extra light, Gouraud per-vertex input, and final cache/raster values. | [`rdCache.c`](https://github.com/smlu/OpenJones3D/blob/a675b395a5d5cff238332d091368483b849e8b5b/Libs/rdroid/Raster/rdCache.c#L315-L365) |
| `rdroid` types | Defines light modes, `rdLight`, the 128-slot `rdCamera` pool, face, mesh, and `rdThing` light fields. | [`types.h` modes and light records](https://github.com/smlu/OpenJones3D/blob/a675b395a5d5cff238332d091368483b849e8b5b/Libs/rdroid/types.h#L10-L40), [`rdLight`/`rdCamera`](https://github.com/smlu/OpenJones3D/blob/a675b395a5d5cff238332d091368483b849e8b5b/Libs/rdroid/types.h#L314-L355) |
| `sith` types | Defines Thing emit/visibility flags, Headlight actor flag, local offset, RGB intensity, and sector/Thing light ownership. | [`types.h` Thing flags](https://github.com/smlu/OpenJones3D/blob/a675b395a5d5cff238332d091368483b849e8b5b/Libs/sith/types.h#L54-L72), [`Headlight and actor fields`](https://github.com/smlu/OpenJones3D/blob/a675b395a5d5cff238332d091368483b849e8b5b/Libs/sith/types.h#L510-L525), [`lightOffset/headLightIntensity`](https://github.com/smlu/OpenJones3D/blob/a675b395a5d5cff238332d091368483b849e8b5b/Libs/sith/types.h#L2015-L2040) |

### OpenJKDF2 / JKDF2 and MotS

| Module | Relevant original responsibility and SED use | Pinned source |
| --- | --- | --- |
| `sithRender` definitions/modes | Defines the classic 32 collected Thing-light cap and global render modes. | [`sithRender.h`](https://github.com/shinyquagsire23/OpenJKDF2/blob/0dbd1ea8bb2211c158f6af928b5e4fb14dfa0dac/src/Engine/sithRender.h#L50-L68), [`sithRender_SetLightingMode`](https://github.com/shinyquagsire23/OpenJKDF2/blob/0dbd1ea8bb2211c158f6af928b5e4fb14dfa0dac/src/Engine/sithRender.c#L249-L284) |
| `sithRender` visible-sector collection | During portal traversal, collects positive scalar Thing lights, actor/player Headlights, and transient saber-blade lights, rotates local offsets, filters flags, and stops at `0x20`. | [`sithRender_BuildVisibleSectorList`](https://github.com/shinyquagsire23/OpenJKDF2/blob/0dbd1ea8bb2211c158f6af928b5e4fb14dfa0dac/src/Engine/sithRender.c#L620-L744) |
| `sithRender` dynamic world calculation | Builds per-visible-sector candidate lists and adds `I - distance*attenuationMax` to world vertices up to full intensity. | [`sithRender_BuildDynamicLights`](https://github.com/shinyquagsire23/OpenJKDF2/blob/0dbd1ea8bb2211c158f6af928b5e4fb14dfa0dac/src/Engine/sithRender.c#L2624-L2679) |
| `sithRender` Thing drawing | Computes sector ambient/extra input, applies per-Thing self-light behavior/QOL branches, and dispatches render Things. | [`sithRender_RenderThings`](https://github.com/shinyquagsire23/OpenJKDF2/blob/0dbd1ea8bb2211c158f6af928b5e4fb14dfa0dac/src/Engine/sithRender.c#L2681-L2968) |
| `sithRender` MotS architecture path | Routes Arch Things selected by `thingflags & 0x8` through the sector ARCHLIGHTING swap, while the ordinary Thing pass excludes that architecture route and uses its normal scalar vertex-color setup. Both paths still dispatch the common Thing renderer. | [`sithRender` architecture and ordinary Thing passes](https://github.com/shinyquagsire23/OpenJKDF2/blob/master/src/Engine/sithRender.c#L2240-L2565) |
| `sithRender` / `sithPlayer` camera Tint | Selects the camera sector once per frame and clamps its Tint into the screen color effect. | [`sithRender` camera-sector selection](https://github.com/shinyquagsire23/OpenJKDF2/blob/0dbd1ea8bb2211c158f6af928b5e4fb14dfa0dac/src/Engine/sithRender.c#L361-L364), [`sithPlayer_SetScreenTint`](https://github.com/shinyquagsire23/OpenJKDF2/blob/0dbd1ea8bb2211c158f6af928b5e4fb14dfa0dac/src/Gameplay/sithPlayer.c#L293-L307) |
| `sithRender` alpha adjoins | Detects explicit translucent faces and alpha MATs during portal traversal, defers them, then renders the queue without depth writes. | [`sithRender` alpha admission](https://github.com/shinyquagsire23/OpenJKDF2/blob/0dbd1ea8bb2211c158f6af928b5e4fb14dfa0dac/src/Engine/sithRender.c#L1811-L1833), [`sithRender_RenderAlphaAdjoins`](https://github.com/shinyquagsire23/OpenJKDF2/blob/0dbd1ea8bb2211c158f6af928b5e4fb14dfa0dac/src/Engine/sithRender.c#L3042-L3080) |
| `rdLight` | Initializes scalar lights and implements classic Gouraud `(I-d*0.4)*N.L` and independent per-light Diffuse face calculations. | [`rdLight.c`](https://github.com/shinyquagsire23/OpenJKDF2/blob/0dbd1ea8bb2211c158f6af928b5e4fb14dfa0dac/src/Engine/rdLight.c#L10-L297) |
| `rdCamera` | Stores scalar lights and derives min/max radii from camera attenuation. | [`rdCamera_SetAttenuation` and `rdCamera_AddLight`](https://github.com/shinyquagsire23/OpenJKDF2/blob/0dbd1ea8bb2211c158f6af928b5e4fb14dfa0dac/src/Engine/rdCamera.c#L487-L540) |
| `sithCamera` | Installs `0.4/0.8` attenuation on the game cameras. | [`sithCamera.c`](https://github.com/shinyquagsire23/OpenJKDF2/blob/0dbd1ea8bb2211c158f6af928b5e4fb14dfa0dac/src/Engine/sithCamera.c#L55-L100), [`constants`](https://github.com/shinyquagsire23/OpenJKDF2/blob/0dbd1ea8bb2211c158f6af928b5e4fb14dfa0dac/src/Engine/sithCamera.h#L20-L32) |
| `rdModel3` | Applies Thing/mesh/face/viewport mode caps, culls and transforms camera lights into model space, evaluates Gouraud/Diffuse, backfaces, ambient, and Extra Light. | [`rdModel3_Draw` through `rdModel3_DrawFace`](https://github.com/shinyquagsire23/OpenJKDF2/blob/0dbd1ea8bb2211c158f6af928b5e4fb14dfa0dac/src/Primitives/rdModel3.c#L1340-L1875) |
| `rdThing` | Defines default geometry/light/texture modes and draw dispatch for Thing render types. | [`rdThing.c`](https://github.com/shinyquagsire23/OpenJKDF2/blob/0dbd1ea8bb2211c158f6af928b5e4fb14dfa0dac/src/Engine/rdThing.c#L15-L205) |
| `sithThing` | Parses positive scalar `light`, stores it on the Thing, and sets the runtime emit flag. | [`sithThing_ParseThingArg`](https://github.com/shinyquagsire23/OpenJKDF2/blob/0dbd1ea8bb2211c158f6af928b5e4fb14dfa0dac/src/World/sithThing.c#L1614-L1792) |
| `sithActor` | Parses scalar `lightintensity`, local `lightoffset`, and actor type flags. | [`sithActor_ParseArg`](https://github.com/shinyquagsire23/OpenJKDF2/blob/0dbd1ea8bb2211c158f6af928b5e4fb14dfa0dac/src/World/sithActor.c#L500-L635) |
| `sithWorld` | Owns and allocates the scalar dynamic-light result and render-tick arrays indexed by world vertex. | [`SithWorld` fields](https://github.com/shinyquagsire23/OpenJKDF2/blob/0dbd1ea8bb2211c158f6af928b5e4fb14dfa0dac/src/types.h#L2366-L2375), [`allocation`](https://github.com/shinyquagsire23/OpenJKDF2/blob/0dbd1ea8bb2211c158f6af928b5e4fb14dfa0dac/src/World/sithWorld.c#L274-L310) |
| `rdPrimit3` | Combines world dynamic intensity with authored per-vertex values during face preparation, including MotS RGB paths. | [`rdPrimit3.c`](https://github.com/shinyquagsire23/OpenJKDF2/blob/0dbd1ea8bb2211c158f6af928b5e4fb14dfa0dac/src/Primitives/rdPrimit3.c#L1100-L1135) |
| `rdCache` / raster | Resolves lighting/Extra Light, maps Solid MAT palette indices through the active CMP light level, assigns explicit translucent faces alpha `90`, and applies the screen-Tint cross-channel transform to HW vertex color before texture modulation. | [`Solid light-level quantization and indexed lookup`](https://github.com/shinyquagsire23/OpenJKDF2/blob/0dbd1ea8bb2211c158f6af928b5e4fb14dfa0dac/src/Raster/rdCache.c#L1033-L1225), [`rdCache` face alpha](https://github.com/shinyquagsire23/OpenJKDF2/blob/0dbd1ea8bb2211c158f6af928b5e4fb14dfa0dac/src/Raster/rdCache.c#L404-L425), [`Tint scale and vertex clamp`](https://github.com/shinyquagsire23/OpenJKDF2/blob/0dbd1ea8bb2211c158f6af928b5e4fb14dfa0dac/src/Raster/rdCache.c#L344-L354), [`rdCache` HW vertex application](https://github.com/shinyquagsire23/OpenJKDF2/blob/0dbd1ea8bb2211c158f6af928b5e4fb14dfa0dac/src/Raster/rdCache.c#L879-L936), [`rdActive.c`](https://github.com/shinyquagsire23/OpenJKDF2/blob/0dbd1ea8bb2211c158f6af928b5e4fb14dfa0dac/src/Engine/rdActive.c#L400-L450) |
| shared types/enums | Defines `rdLight`, camera light arrays, actor light fields, and numeric Fully lit/Not lit/Diffuse/Gouraud modes. | [`types.h` light records](https://github.com/shinyquagsire23/OpenJKDF2/blob/0dbd1ea8bb2211c158f6af928b5e4fb14dfa0dac/src/types.h#L515-L585), [`actor light fields`](https://github.com/shinyquagsire23/OpenJKDF2/blob/0dbd1ea8bb2211c158f6af928b5e4fb14dfa0dac/src/types.h#L2985-L3010), [`types_enums.h` light modes](https://github.com/shinyquagsire23/OpenJKDF2/blob/0dbd1ea8bb2211c158f6af928b5e4fb14dfa0dac/src/types_enums.h#L18-L32) |

## Reference-to-SED mapping

| Reference-engine concept | SED implementation | Parity statement |
| --- | --- | --- |
| `sithThing`/`sithActor` authored fields | [`resolveRuntimeThingLight` and `resolveRuntimeHeadLight`](../src/app/src/render/lighting/dynamic_light_utils.hpp) | Payload and inherited-value interpretation; SED models parser emit-bit side effects by field presence. |
| `sithRender` visible-sector collection | [`collectRuntimeThingLights`](../src/app/src/render/lighting/dynamic_light_utils.hpp) | Same project caps and eligibility inputs; SED keeps eligible sources view-stable because it has no portal/PVS traversal, then applies its editor cap policy. |
| `rdCamera_AddLight` radius derivation | [source resolvers and `resolveRuntimeSectorPointLight`](../src/app/src/render/lighting/dynamic_light_utils.hpp) | Same `0.4/0.8` derivation and independent IJIM zero sentinels. |
| `sithRender_BuildDynamicLights` | [world target](../src/app/src/render/gl_view_render.cpp) | Gouraud has the same distance falloff and per-emitter clamp; Phong uses the pinned OpenGL smooth shader. Collector order can differ. |
| `rdModel3` + `rdLight` Gouraud | model target `2` shader/helpers | Gouraud formula parity for regular/head/Sector payloads; Phong uses the pinned OpenGL smooth model equation. |
| `rdModel3` + `rdLight` Diffuse | model target `3` shader/helpers | IJIM mutable attenuation and Sector enabled quirk preserved; classic uses independent `0.4` attenuation and saturation overshoot. |
| `rdClip`/`rdCache` final composition | [`polyCalcLitColor`](../src/app/src/render/mesh/render_lighting_utils.cpp) plus [vertex/fragment finalization](../src/app/src/render/gl_view_render.cpp) | Project-specific world/Gouraud/Diffuse clamp points intentionally separated. |
| `rdCache` Solid indexed-color path | [`CmpFile` light tables](../src/core/include/sed/assets/cmp.hpp), [CMP parser/ramp builder](../src/core/src/assets/cmp.cpp), and [viewport shader](../src/app/src/render/gl_view_render.cpp) | JKDF2 scalar Solid lighting selects one of 64 CMP rows before screen Tint; MotS RGB Gouraud and IJIM remain separate. |
| camera-sector `sithPlayer_SetScreenTint` + `rdCache` color effect | completed-light/pre-texture classic Sector Tint stage | One camera-sector effect for JKDF2/MotS; explicitly neutral for IJIM. |
| alpha-adjoin admission + `RD_FF_TEX_TRANSLUCENT` | [translucent-pass policy](../src/app/src/render/translucency/translucent_order_utils.hpp), [classic face opacity](../src/app/src/render/mesh/render_lighting_utils.cpp), and [scene submission](../src/app/src/render/gl_view_render.cpp) | Face or MAT alpha admits classic adjoins to the no-z-write blend pass; the explicit face flag uses `90/255` alpha. |

## Tests and verification map

### Core baked tests

[level_lighting_tests.cpp](../src/core/tests/level/level_lighting_tests.cpp) covers:

- old/new falloff and IJIM RGB contribution;
- alpha preservation while zeroing and accumulating;
- sector ambient averaging, baked-versus-Surface-Extra selection, normalization, JKDF2 grayscale versus MotS/IJIM RGB,
  same-sector reachability, grayscale flag, and no-ambient flag;
- clearing stale output for no valid lights, zero/negative light inputs, non-finite dirty-influence inputs, or after a
  light leaves range;
- dirty-region output equivalence to a full bake and preservation outside the dirty union;
- strict range and surface-facing rejection, portal reachability (including cyclic graphs), `SAF_BlockLight`, and
  `LF_NoBlock` bypass;
- dirty influence union filtering and cancellation at initial, between-sector, inner-loop, and final-publication checks.

JED/JKL asset tests cover editor-light and related lighting-field round trips. The committed IJIM NDY fixture also
checks RGB Headlight load materialization, retail-compatible RGB-only save, the documented lossy range reset on
reload, unchanged four-component regular Thing lights, offsets, repeated frames, flags, slash paths, and long vectors.

### Runtime and orchestration tests

[thing_utility_tests.cpp](../src/app/tests/thing_utility_tests.cpp) covers:

- raw IJIM parsing, flat threshold, self-light, dynamic classification, and GPU packing;
- effective face/mesh/viewport mode capping and runtime-target selection;
- classic scalar radii, regular owner self floor, independent Diffuse attenuation, saturation, and final composition;
- actor/player Headlights, transformed offsets, and no Headlight owner-self contribution;
- runtime-parity disabled/invisible rejection plus editor-visible disabled cutscene-actor preview;
- flat/self and exact-black source discoverability, black-source runtime-pool exclusion, and
  invalid-sector/invalid-Headlight rejection;
- camera-stable collection without individual emitter-frustum rejection, nearest-from-all cap selection, stable source
  ordering, shared/project caps, ordered IJIM world accumulation, and transform/sector overrides;
- IJIM Sector zero-radius derivation and Gouraud-versus-Diffuse participation;
- fast-transform point/normal semantics for the cached Diffuse face anchor;
- world/model contribution and final-clamp helpers;
- classic Tint input/output clamps, Editor owner-sector scope without mirror-sector borrowing, Game Preview
  camera-sector scope, classic
  `90/255` face opacity, alpha-adjoin pass admission, editor backface presentation, and IJIM neutrality;
- light-bake source comparison, dirty influences, targets, coalescing, fingerprints, and safe result transfer.

[mat_cmp_tests.cpp](../src/core/tests/assets/mat_cmp_tests.cpp) covers complete 64-row CMP retention, final-table
compatibility, Solid MAT palette-index retention, and per-level Solid-color ramp resolution.

[thing_find_lighting_menu_tests.cpp](../src/app/tests/thing_find_lighting_menu_tests.cpp) covers Thing Find template matching and picker routing, the current baked-light action label, and removal of the bulk Arch dialog and redundant menu action while retaining viewport Arch and bake actions.

[main_window_event_filter_tests.cpp](../src/app/tests/main_window_event_filter_tests.cpp) covers the user-facing mode actions, Baked/Thing/Sector family
state, persisted Auto control, source polling, transform-bake preview lifecycle, live Thing/Headlight/Sector/Baked popup
and item-editor synchronization, Baked-gizmo double-click routing, single-interaction undo, inherited-light override
materialization, the persisted Gouraud/Phong preference, and an actual OpenGL compile/link
smoke test for the UBO-enabled shader. A framebuffer/readback fixture also verifies that Gouraud leaves an interior
fragment at its authored interpolation while Phong produces a localized IJIM target-1 light pool, retains a measurable
current-OpenJones smooth transition beyond the authored radius, and reaches zero outside the expanded band. The classic
framebuffer fixture verifies that low-lit translucent and opaque JKDF2 Solid adjoins resolve their palette index through
the CMP ramp before Tint/alpha, preserve a MAT color deliberately different from the mirror-sector Tint, keep MotS RGB
Gouraud and IJIM separate, match neutral Editor and Preview scopes, and retain explicit face opacity `90/255`.

### Maintenance checklist

When changing lighting:

1. State which of the four "mode" concepts is changing.
2. State whether the change modifies saved data, baked output, runtime preview, or only UI/gizmos.
3. Keep Baked, regular Thing, Headlight, and IJIM Sector sources distinct in code and test names.
4. Test all three project types when touching payload parsing, radii, caps, or mode gating.
5. Preserve the old and new dirty influence when changing Auto/transform behavior.
6. Never publish a worker result without generation and fingerprint checks on the GUI thread.
7. Specify light order in a test for world per-emitter clamps or IJIM Diffuse mutation.
8. Treat zero IJIM Sector radii as independent sentinels.
9. Verify UBO layout and fallback behavior when changing the runtime payload.
10. If claiming engine parity, link the exact upstream function and list any collector, cap, QOL, or build-switch
    difference next to the claim.

For implementation summaries, also see:

- [Asynchronous baked lighting](algorithms.md#asynchronous-baked-lighting)
- [Runtime dynamic-light preview](algorithms.md#runtime-dynamic-light-preview)
- [Architecture](architecture.md)
- [Porting checklist](porting_guide.md#lighting)
