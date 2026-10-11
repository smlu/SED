# Binary SED project format

This overview is retained for implementation orientation. The normative byte-level specification, complete schemas,
compression tokens, validation rules, and upgrade contract are in
[formats/sed_binary_format.md](formats/sed_binary_format.md). The separate self-healing crash format is documented in
[formats/recovery_format.md](formats/recovery_format.md).

Native SED versions encode the Level UV coordinate space and MotS arch-lighting persistence. V1 is the legacy
texture-pixel format without arch-lighting fields; V2 is the normalized counterpart. V3 is normalized with the MotS
table field and optional bake-provenance trailer; V4 is the texture-pixel counterpart. The reader accepts all four
versions. The writer selects V1/V2 when the filtered snapshot retains neither ARCHLIGHTING objects nor authored bake
baselines, and V3/V4 when either remains. A provenance-only snapshot writes an empty table plus the identified `absl`
trailer. The Level's `Normalized` or `TexturePixels` tag selects the corresponding pair. All four versions use
the same section order, framing, compression, checksums, parity protection, recovery boundaries, and application
payload schemas; the native version distinguishes the coordinate space of `TexVertex.u/v` and `Surface::uScale`/`vScale`.

## Contents

- [Purpose and relationship to engine formats](#purpose-and-relationship-to-engine-formats)
- [Parity algorithms at a glance](#parity-algorithms-at-a-glance)
  - [Why native SED uses Vandermonde MDS](#why-native-sed-uses-vandermonde-mds)
  - [Why recovery metadata uses two-parity P/Q](#why-recovery-metadata-uses-two-parity-pq)
- [V1 through V4 container](#v1-through-v4-container)
  - [Primitive encoding and fixed header](#primitive-encoding-and-fixed-header)
  - [Resource catalogs and references](#resource-catalogs-and-references)
  - [Section framing](#section-framing)
  - [Required Level sections](#required-level-sections)
    - [`head`](#head)
    - [`rsrc`](#rsrc)
    - [`sect`](#sect)
    - [`thng`](#thng)
    - [`tmpl`](#tmpl)
    - [`cogs`](#cogs)
    - [`lite`](#lite)
    - [`hnod`](#hnod)
  - [Optional core import section](#optional-core-import-section)
    - [`jimp`](#jimp)
  - [Application sections](#application-sections)
    - [`edit`](#edit)
    - [`hist`](#hist)
  - [Compression](#compression)
  - [Native parity algorithm](#native-parity-algorithm)
  - [Protection boundaries and corruption model](#protection-boundaries-and-corruption-model)
  - [History storage and deltas](#history-storage-and-deltas)
  - [Loading and partial recovery](#loading-and-partial-recovery)
- [Legacy pixel-space UV admission and saving](#legacy-pixel-space-uv-admission-and-saving)
- [Recovery use](#recovery-use)
- [Implementation and tests](#implementation-and-tests)

## Purpose and relationship to engine formats

`.sed` is the complete Sith Editor document format, analogous to Blender's `.blend` rather than an engine export.
All native versions are deliberately separate from the engine text formats:

- JKDF2 and MotS export runtime levels as `.jkl`;
- IJIM exports runtime levels as `.ndy`;
- classic `.jed` remains an optional legacy editor export for JKDF2 and MotS only.

JKL/NDY importers construct the common `sed::Level` model and their exporters apply the target engine's compatibility
rules. Binary SED never applies those engine-export reductions. Its required sections preserve all four `ColorF`
channels and the authored Level fields specified by the eight required-domain schemas for all project types. A classic
project can therefore retain exact RGB(A) editor data even when a later JKL export must write a scalar intensity; IJIM
retains RGB(A), fog, sector point lights, thrust, and other NDY-capable data exactly.

Meaningful pending JKL COG import provenance is carried by the optional core-owned `jimp` V1 section and, when
HIST is present, the app-owned optional `jklCogImportV1` history map. This preserves deferred default hydration and
imported Surface ordering across native Save/Open and represented COG undo/redo states without changing the native
version or required Level schemas. A false marker plus unused Surface map is normalized away, so ordinary healthy files
keep their established bytes. Saving unresolved ambiguous Surface ordering is still rejected. Native MotS ARCH filtering
likewise follows the explicit isolated-snapshot contract in the
[normative format](formats/sed_binary_format.md#thng).

The UI decimal-separator preference changes presentation and numeric input only. Accepted numeric values keep their
existing binary encoding or canonical dot-based text in Level, resource, history, and engine formats. Optional EDIT
draft strings preserve unfinished input verbatim, including a comma decimal mark; they are never treated as accepted
engine values until validated and committed. Script source, UNI text, resource names, and other opaque strings are
not reformatted. The input locale is not part of the native draft schema; a restored numeric editor uses the current UI
policy and may require correction of an unfinished value before committing it.

Runtime JKL/NDY Export and Export As also offer independent optional editor
`Lights` and `Layers` sections plus a compact text presentation. The dialog
defaults to **Compact** enabled and both editor-only sections disabled; the
core options object remains backwards-compatible for API callers that do not
explicitly choose these flags. Compact mode writes LF, single tabs, no padded
columns, removes parser-only full-line field comments and blank lines, and
uses the shortest finite decimal/exponent spelling that round-trips to the
writer's already-selected value (including signed zero). It removes only the
two exact writer-generated field-label comments (`#Num...` and `#Name...`);
arbitrary user records beginning with `#` are retained. The copyright block
is left byte-for-byte untouched. Resource names, template/Thing names, and
quoted string contents that happen to contain decimal-looking text are never
rewritten. Unknown `name=value` Thing/template fields are treated as opaque,
and COG script names plus every COG symbol value are preserved conservatively
because their type can be unavailable at export time. Exponent signs and
leading exponent zeroes are normalized (`1e-06` becomes `1e-6`, `1e+08`
becomes `1e8`); this changes spelling only. Layer names and the optional LVIS
line are also kept as whole user strings, including spaces. Omitting an editor
section changes neither the runtime Level sections nor their declared
capacities; it only removes SED's optional editor records.

On the compact-export regression fixture (a small JKDF2/MotS/IJIM-shaped
level containing editor sections, opaque user strings, names that look
numeric, signed zero, and exponent-scale values), the JKDF2 presentation
measured 3,323 bytes in the ordinary writer and 2,640 bytes in compact mode
(20.55% smaller).  The exact ratio is content-dependent; large geometry is
dominated by numeric records, while compact mode additionally avoids repeated
padded columns and the two non-semantic writer comment lines.

## Parity algorithms at a glance

SED uses two intentionally separate forward-error-correction schemes:

| Protected data | Parity algorithm | Field and matrix | Repair budget |
|---|---|---|---|
| Native `.sed` top-level section payload | Systematic Reed–Solomon-style Vandermonde MDS erasure code | GF(256), primitive polynomial `0x11d`; adaptive `k=1..16`; the top `k` Vandermonde rows are normalized to the identity matrix | `p=4` bad slots per stripe for required Level sections, core-owned `jimp`, and `edit`; `p=2` for `hist` and future lower-priority optional sections |
| Outer `.sedrecovery` `meta` payload | Systematic two-parity P/Q erasure code | GF(256), primitive polynomial `0x11d`; P is XOR and Q uses `2^i` as each data-shard coefficient | Up to two missing or corrupt metadata shards |

CRC32C identifies erased native slots before MDS reconstruction. Outer recovery metadata instead uses IEEE CRC-32 to
identify bad P/Q shards. SHA-256 verifies the fully reconstructed semantic payload in both schemes. The native algorithm
is detailed under [Native parity algorithm](#native-parity-algorithm); the outer algorithm and its mirrored tables are
specified in [Recovery format: protected metadata payload](formats/recovery_format.md#protected-metadata-payload).

### Why native SED uses Vandermonde MDS

Native SED sections range from a few bytes to many megabytes and need different repair budgets without storing a second
copy of the document. A systematic Reed–Solomon-style Vandermonde code was chosen because it gives the maximum-distance
separable guarantee: `p` parity streams repair any `p` CRC-localized erased slots in a stripe with exactly `p` streams
of parity overhead. The systematic form leaves healthy data slots in their original byte form, while GF(256) operates
directly on bytes and the normalized Vandermonde generator works deterministically for every adaptive `k=1..16` stripe.
An XOR-only code could repair only one arbitrary erased slot, and full replication would cost much more space without
providing a better repair budget. CRC32C supplies erasure locations and SHA-256 verifies the reconstructed semantic
section, so the MDS decoder never has to guess which apparently valid slot is wrong. The implementation is in
[sed.cpp](../src/core/src/assets/sed.cpp).

### Why recovery metadata uses two-parity P/Q

The outer recovery `meta` payload is small, atomic association data rather than another large document domain. It needs
to survive two missing or corrupt shards, but it does not need the native format's configurable two-or-four-parity
matrix. Fixed P/Q was chosen because one XOR parity plus one independently weighted GF(256) parity repairs any two
CRC-localized shard erasures with a small, deterministic encoder and decoder. Mirrored checked tables describe the
shards, and SHA-256 verifies the reconstructed metadata. Applying native MDS to this tiny fixed-budget unit would add
format and matrix machinery without increasing its two-shard guarantee; protecting `docm` again would merely duplicate
the parity already owned by every native section. The implementation is in
[document_recovery_utils.cpp](../src/app/src/ui/recovery/document_recovery_utils.cpp).

## V1 through V4 container

### Primitive encoding and fixed header

All integers are little-endian. Finite `float` and `double` values use their exact IEEE-754 bit representation, including
signed zero; non-finite authored values are rejected. Strings are 32-bit-length-prefixed byte strings, and collections
are 32-bit-count-prefixed contiguous records. The reader bounds a complete file to 256 MiB, one section to 128 MiB,
all expanded sections together to 256 MiB, one decoded Level graph to 128 MiB, a string to 16 MiB, and a collection to
two million items before allocating.

The canonical fixed header shared by V1 through V4 is 28 bytes and contains:

1. the exact four-byte lowercase `sed\0` signature (the NUL is part of the magic);
2. format version `1`, `2`, `3`, or `4` immediately after the aligned signature;
3. `ProjectType` (`JKDF2`, `MOTS`, or `IJIM`);
4. section count;
5. `timestamp`, the signed 64-bit UTC Unix-epoch-millisecond creation time;
6. `checksum`, the CRC32C of the preceding fixed-header bytes.

The fourth magic byte remains the NUL discriminator rather than a one-byte version: hiding the version there would save
only four bytes and unnecessarily constrain/entangle future versioning. The creation time is an explicit signed 64-bit
value; zero means unknown. Each native Save writes the instant at which that new atomic file image was created, while
recovery/autosave uses its capture time for the native document it embeds. Its range avoids the 32-bit Unix and NTP
rollover limits. Canonical nested history and required-Level digest encodings always use zero creation time. The time
field is named `timestamp`; the integrity field is named `checksum`, with CRC32C remaining the normative algorithm
rather than part of either field's schema name.

Native file versions 1 through 4 are accepted. Every section descriptor's schema version must equal the containing
file version. V1/V2 retain the established payload layout without the MotS arch-lighting table; V3/V4 extend the
`thng` payload with the table, per-Thing arch-lighting references, and optional bake-provenance trailer. V1/V4 use
texture-pixel UV values, while V2/V3
use normalized UV values. See
[formats/sed_binary_format.md](formats/sed_binary_format.md) for byte offsets and the complete normative field layout.

### Resource catalogs and references

Required-Level resource spellings are deduplicated in 14 fixed `rsrc` categories. A fixed-schema resource field stores
only its category-local index; `-1` is the sole empty/not-set sentinel. Native indices and references use canonical
sentinel-biased ULEB128 (`wire 0 = -1`, otherwise `wire = index + 1`), including Sector/Surface/Thing links and
entity-reference COG values. This keeps an empty reference at one byte; directly converting the signed `-1` sentinel
to unsigned would instead write `UINT32_MAX` as a five-byte ULEB128 value. Flags, enums, stable IDs,
counts, and arbitrary parameter integers remain fixed-width. The `edit` and `hist` application sections use canonical
CBOR, whose integer encoding is already shortest-form and therefore does not embed a second native-index grammar.

The independent all-category test fixture quantifies the affected semantic fields: 91 occurrences and 54 unique
strings require 1,366 bytes when repeated inline, 1,304 bytes with catalogs plus fixed `u32` references, and 1,031
bytes with the actual catalogs plus ULEB128 references. That is 335 bytes (24.52%) below inline strings. The compact
references themselves use 91 rather than 364 bytes (75% less); final protected-file savings may be rounded by 4 KiB
FEC slots for small documents.

### Section framing

Each semantic section starts with a checked 100-byte `sectv1\n\0` descriptor containing its four-byte type,
section-schema version, flags, expanded/physical/logical 64-bit sizes, striped layout (`k`, `p`, 4092-byte coded payload,
stripe count, and alignment offset), one SHA-256 of the expanded payload, and descriptor CRC32C. Its payload is followed
by a checked 116-byte `sedmir1\n` trailer carrying the complete descriptor. A damaged primary descriptor can therefore be
restored exactly; corruption recovery can discard an ambiguous unit and resynchronize only at the next checked primary
descriptor/trailer pair or at a checked mirror that derives one exact section start from its stored extent and 4 KiB
phase.

### Required Level sections

All native versions require the following sections in canonical order.

#### `head`

Project/header settings, fog, export buffer extras, master CMP, PPU, layer-visibility string, and `curId`. Native
persistence retains authored allowances even if they are incompatible with runtime export: IJIM Materials Extra
must be repaired to 0 or 64 before NDY export, while native Save/Open preserves the original value.

#### `rsrc`

Fixed categorized resource/name catalogs with independent local indices, followed by ordered colormap, material, and
semantic-layer inventories that retain duplicates.

#### `sect`

All Sectors, vertices, Surfaces, adjoins, UVs, RGBA light values, point lights, thrust, and collide boxes.

#### `thng`

All Thing IDs/numbers, separate template and instance names, transforms, typed values, layers, and boxes. V3/V4 also
store each Thing's MotS arch-lighting index followed by the ordered arch-lighting object/mesh/vertex table. An optional
`absl` trailer retains authored bake baselines, including original owner/model identity and binding or unbound state.
The table may be empty when only those baselines remain; see the [normative `thng` schema](formats/sed_binary_format.md#thng).

#### `tmpl`

Authored templates, inheritance, typed values, descriptions, and boxes.

#### `cogs`

COG IDs/numbers and complete typed value metadata.

#### `lite`

Editor baked lights, including exact RGBA, intensity, range, flags, layer, and position.

#### `hnod`

3DO hierarchy nodes.

### Optional core import section

#### `jimp`

Optional core-owned JKL COG import provenance. Its ProtectedV1 payload is little-endian `u32 version` (exactly 1),
a canonical one-byte Boolean `importedJklCogRowsPresent`, `u32 referenceCount`, then that many `(i32 sector, i32
surface)` pairs in source order. The writer emits it only when the marker is true or a nonempty reference map is still
needed by raw `paramN` COG rows whose `CogType` is `unk`. A false marker with an unused map is normalized to empty and
emits no section; Levels without represented import provenance therefore retain their prior bytes. Strict loading rejects
malformed known `jimp` metadata. Permissive corruption recovery may discard a damaged or invalid `jimp`, clear only the
import marker/map, preserve the verified Level, and report the optional loss. The ordinary native version and eight
required Level sections do not change. `HistoryNested` remains exactly those eight sections and never embeds `jimp`;
HIST stores its separate per-state import metadata when needed.

### Application sections

Application-owned sections are structurally optional. Sith Editor's normal complete-document Save/Open policy attempts
to write the following sections.

The native file version does not replace the application schemas carried by these sections. In all native versions,
the `edit` root remains editor-state schema V1, its nested `window.uvState` remains UV-workspace schema V1, and `hist`
remains history/application schema V1 with its existing V1 encoding and fragment-layout names.

#### `edit`

The versioned editor/workspace CBOR used by recovery, including both cameras, projection/FOV, selection and active item,
edit/render modes, configured tools, staged Multi-cut lines, placement/brush settings, layer UI state, window/dock/tab/
splitter positions, the optional last-used Template Editor window geometry, viewport settings, the settled UV editor
zoom/pan/View Face basis, repeat/snap/grid controls, active island and face/edge/vertex selection, the last
project-aware JKL/NDY/JED export target, open COG editors including unsaved source buffers and each tab's
source-independent presentation zoom, and the optional docked resource-editor workspace. Template Editor geometry is
optional for compatibility; older files use the application's saved dialog geometry when available, otherwise the
initial default.

The optional root `surfaceSelectionContext` V1 map preserves the frozen Strict face-owner scope and remembered Face
selection separately from current component selection. Each required `strictScope`/`rememberedFaceSelection` array
contains unique valid `[sectorIndex, surfaceIndex]` pairs; writers sort them and readers accept any order. Malformed
known values reject EDIT before mutation. Unknown additional map keys are ignored and may be dropped on resave.
Present empty arrays remain intentionally empty; only an absent map infers legacy context from current selected
faces/component owners. Strict itself stays an app preference. Structural geometry reindexing clears the frozen scope
and stale mode cache, seeds remembered faces from the committed current selection, and returns to Face;
geometry-only transforms preserve the context. Cold native history checks actual Sector/Surface ownership and indexed
connectivity when dirty masks are unknown, incomplete, or contain unsupported bits; the all-domain `All` sentinel
remains known. Protected mixed jumps prepare exact adjacent-state comparisons before live changes. Pure Move and
Thing-only changes retain compatible context and committed Face selection; stored masks and HIST remain unchanged. See the
[normative contract](formats/sed_binary_format.md#workspace-schema-and-validation).

#### `hist`

The functional undo/redo timeline, with each Level state encoded through the same lossless binary codec. The V1
root may also contain optional `jklCogImportV1` metadata: `version: 1`, explicit `current: [rowsPresentBool,
[[sectorI32, surfaceI32], ...]]`, and sparse `undo`/`redo` arrays of `[stateIndex, rowsPresentBool, refs]`.
Stack indices are ascending and unique; an omitted index means the state has no import metadata, never that it inherits
from its neighbor. `current` must match the normalized metadata represented by the authoritative current Level. The
extension is emitted only when current or a stack state is represented; false-marker unused maps normalize to empty.
Per-state Surface references preserve their source order. Malformed known metadata rejects the HIST payload; older
readers may discard this optional addition while retaining the current Level. Each represented state stores its complete
ordered map again, so large pending imports can reach the existing HIST reference/CBOR/size budgets. The writer then
omits optional history with a warning; the current Level and represented `jimp` remain retained. See the
[normative HIST limits](formats/sed_binary_format.md#cbor-schema-and-base-references).

`edit.cogEditors` stores each open buffer's authored source text exactly, including quoted punctuation such as colons,
plus its modified/cursor/presentation state. COG diagnostics are derived again from that text after Open; they are not
serialized. Its workspace map retains `version: 1`, a flat `tabs` array of script records, global `active` flat-tab
index (`-1` when no host is selected), and legacy primary `dockVisible`. An optional `hosts` array stores each COG
window's stable `id`, `primary` flag, requested `visible` state, `floating` flag, saved `geometry` bytes, host-local
`active` index, and flat-tab indices in `tabs`. The primary host is first and has ID `primary`; it may be empty with
local `active: -1`. Other hosts must contain at least one script. Host IDs are unique, nonempty ASCII letters,
digits, underscores, or hyphens, with at most 64 characters. Every flat script index belongs to exactly one host,
and a selected global `active` must match that host's local selection. Admission allows at most 1,024 script tabs,
64 hosts including the primary, and 1 MiB of geometry bytes per host. The complete topology and script records are
validated before any live buffer or dock is changed. Records without `hosts` restore all scripts in the primary
window. Dynamic named docks are prepared before Qt restores the workspace layout; floating geometry is kept on an
available screen. These optional fields do not change EDIT or native SED versions.

Optional COG tab `acceptedSourceHash` is a byte string of length zero (unknown) or exactly 32 (SHA-256), recording
exact last-accepted disk bytes independently of authored text/native checkpoint state. Absent legacy values remain
unknown; malformed present values reject EDIT atomically. Ordinary loose Save checks current bytes before formatting
and offers Reload/Overwrite/Cancel on drift or an unknown source baseline. Watcher Keep does not rebase that hash;
successful Reload or committed external Save does. This additive field retains EDIT V1 and all container versions.

COG host capture records the Editor visibility requested before Game Preview hides the windows. Recovery opened in
Preview keeps every COG host hidden and read-only, then restores requested visibility when returning to Editor.
Moving a script changes host membership while retaining the same live editor and document history. Host layout is
serialized only in full native Save/recovery captures; authored-content comparisons exclude it and the histories.
Capture, transfer, and restore do not save or extract source files.

The optional `edit.resourceEditors` V1 extension stores editor-group and resource-window membership, tab
order, pinning, active tabs, source identity, per-buffer native recovery baselines, and raw authoring values. Detached resource panes use
the same draft codec as docked panes. Capture reads an active table editor's current raw text without committing the
delegate or moving focus, and retains uncommitted Sprite numeric text separately from accepted numeric values. SND/UNI
cells and pending Sprite text need not form a valid engine file for native Save or recovery; explicit resource
Save performs format validation. Native Save embeds these drafts but does not write their loose files or clear their
separate external modified baselines. Sprite FPS, paused playback, Bounds, splitter sizes, and preview camera are
presentation choices kept outside authored SPR fields and dirty state. The current MAT cel phase is derived at runtime. Resource-preview
images, parsed-model thumbnail caches, filesystem-watch state, viewport pick acceleration data, and the current pick
cycle are runtime caches and never enter `.sed`, `.sedrecovery`, or `.sedautosave`. Their implementation can therefore
be optimized or corrected without changing the application schemas or native wire layout.

The resource workspace map has `version: 1`, a global `active` tab index (`-1` when empty), legacy Sprite-group
`dockVisible`/`dockGeometry`, a flat `tabs` array, and a `hosts` array. A tab record contains its pane `page` map,
`pinned` flag, `host` index, and optional `nativeBaselineHash` (empty or 32-byte SHA-256). A host record contains
`primary`, stable `id`, `visible`, `floating`, saved `geometry`, host-local `active`, and global tab indices in `tabs`.
The optional named `groups` map contains `sprite`, `soundClass`, and `uni`, each with `visible`, `floating`, `geometry`,
and `active` (`-1` when its primary group is empty). With groups present, every occupied host has a `kind` integer
(Sprite 0, Sound Class 1, UNI 2), homogeneous files, and a valid local selection. Primary IDs are `primary`,
`primarySoundClass`, and `primaryUni`; host order does not identify kind. Empty primary groups are represented only in
`groups` and do not consume occupied-host budget. Primary host presentation agrees with its group map, and the legacy
Sprite fields agree with `groups.sprite`. Known fields are strictly validated; bounded unknown optional extensions are
ignored. Floating-window restore keeps windows on an available screen.

Older EDIT V1 records may omit `groups`, `hosts`, or a tab's `nativeBaselineHash`. Legacy mixed hosts are partitioned
into the matching editor groups while retaining raw drafts, source ownership, ordering, selection, and baselines;
missing baselines are recomputed where possible. Workspace admission is limited independently to 64 tabs and 64
occupied hosts, and each saved geometry byte array is limited to 1 MiB. These optional fields do not change the native
SED or EDIT version.

Each `page` map is versioned independently and records resource `kind`, project-derived `profile`, `sourceKind`, draft
`name`, loose `path` or `projectResource`, virtual `draftIdentity`, external `modified` marker and `diskHash`, pane
geometry, search text, and editor data. Sprite data stores the material, type, dimensions, geometry/light/texture modes,
RGBA extra light, and XYZ offsets as authored values. Sound-class and string-table data store ordered raw rows; UNI also
stores imported capacity without replacing it with the external live-row count, and SND stores collapsed-mode
presentation. Generic UNI external output uses exact key-byte identities and declares the live row count for both
engine profiles. Optional SND `soundPreviewLoops` contains one Boolean per
source row; absent data defaults to nonlooping preview and a present array must match the row count. Optional SND
`soundEmptyModes` stores a sorted array of distinct, exact engine-recognized mode names for explicitly added groups
with no entries. It admits at most 95 names in classic projects or 140 in IJIM, and none may already occur in an
authored row. Other resource kinds reject this field. Omission retains legacy behavior. These empty groups are native
presentation state: the canonical authored projection excludes them, and external SND output contains only real
entries. New, Open, and source reload clear this presentation list. Sprite
`spritePendingNumericEdits` optionally maps `width`, `height`, `extraLightScalar`, and `offsetX`/`offsetY`/`offsetZ` to
raw numeric-editor strings. Known values are bounded strings, including invalid intermediate text, while the `sprite`
array retains accepted finite values. A Sprite page may also contain the optional `presentation` map (`version: 1`)
with `bounds` (Boolean), `fps` (integer 0 through 99), optional `paused` (Boolean), optional `globalGeometryMode`
(integer 0 through 4, default 4 when absent), `splitterSizes` (two nonnegative pixel
extents), and `camera`. The camera map contains `orbit` as the flat eleven-number array
`[centerX, centerY, centerZ, panX, panY, panZ, radius, distance, yaw, pitch, roll]`, plus `projectionType`, `fovXDeg`,
`orthoScale`, and `clipEnabled`. Camera values are finite and bounded before restore. When this map is absent, the
preview uses its default Bounds/FPS/split and straight-on fitted camera. IJIM exposes `globalGeometryMode` as Preview
geometry; classic captures may carry the hidden default 4. This field never replaces the authored Geometry/Texture
tokens or changes their modified state. Restore applies the camera after the isolated
preview Level is attached and initially fitted. Current cel phase
is not stored; playback starts at the first available MAT cel with the saved paused choice (false when absent), without
changing authored resource state. These maps preserve in-progress invalid table cells and numeric text for native Save
and recovery, while explicit external Save still applies the selected
format's validation. A bounded imported MAT/WAV token can exceed its engine's external filename limit and still survive
native Save and recovery unchanged; external Save rejects it rather than truncating its identity.

Each SPR/SND/UNI page may include optional `editorPresentation` V1. It stores named line-edit cursor/anchor pairs;
UNI current cell, selection ranges, and both scroll offsets; SND current item by stable source-row ordinal or exact
mode bytes, current column, both scroll offsets, and manually sized widths; and deferred table-editor caret pairs keyed
by stable row identity and column. Restore rebuilds the model before applying current selection and layout, and applies
search/filter/collapse before scrolling. Deferred cell carets are restored only when ordinary editing begins; restore
never opens a live delegate. Capture reads raw editor text/carets without commit, focus change, resource history, dirty
state, or recovery arming. `editorPresentation` is excluded from authored-content projection. COG cursor/anchor and both
scrollbars remain separately represented and restored. The
[editor-state inventory](editor_state.md) distinguishes these presentation limits from authored-state coverage.

Resource pages and COG tab records may independently contain an optional `history` map with `version: 1`.
This document history belongs to `edit`, separately from the Level timeline in `hist`; it does not change either
section version or the native SED version. Older records without this map restore their current buffers with an
empty document timeline. A present map stores `cursor` (the number of applied commands), `clean` (the external saved
command position, or `-1` if that checkpoint is no longer retained), `baselineDirty` (Boolean), and `commands`.
A clean checkpoint may lie in the retained Redo branch. Only an edit that discards that branch invalidates it.
The page or tab's `modified` marker must agree with the history's authored position and baseline marker; a malformed
timeline is rejected before any buffer, source identity, or live Undo stack changes.

Sprite commands contain exact `before` and `after` maps with the fifteen-field `sprite` array, raw numeric `pending`
map, and accepted `material` token. The history's current `material` field retains the accepted token separately from
an invalid raw MAT draft. UNI and SND row commands contain `row` and `before`/`after` records: null denotes insertion or
removal, and a record stores both ordered `raw` cells and accepted canonical `model` cells. The history's `models`
array stores the current canonical rows so restoring an invalid pending cell cannot guess a replacement accepted
value. SND row records also store preview `loop` state. SND presentation-only mode commands instead require `row: -1`,
null `before` and `after`, and one engine-recognized `mode` name. SND history stores sorted current `modes` and a
`cleanAuthored` command count; empty-mode presentation commands can be undone without making the external SND dirty.
The current mode projection, row shapes, exact Undo/Redo preconditions, and saved authored position are all validated.

COG commands are arrays of ordered text splices with integer UTF-16 `position`, exact `removed` text, and exact
`inserted` text. Commands preserve native Qt typing coalescing and compound edit-block grouping. Splice positions and
ranges cannot split a surrogate pair, and all text must be well-formed UTF-16. Each splice must remove or insert
nonempty text. Replacing a nonempty selection with identical text retains its native command and dirty/clean boundary.
Journals require the COG editor's canonical plain-text projection: U+00A0 is captured as a regular
space, and U+2028/U+2029 as line feed. Incoming empty or noncanonical splice records reject the workspace atomically,
including records in the future Redo branch; this does not reject user input, which is already captured through Qt's
plain-text projection. Restore reconstructs the native Qt Undo/Redo stack and saved clean position while preserving the captured
current text and presentation; final availability is published only after reconstruction is complete.

Each document timeline admits at most 1,024 retained commands. Resource command arrays and complete COG history maps
are capped at 16 MiB of encoded CBOR. Current resource canonical rows remain subject to the enclosing EDIT budget,
rather than the command-array budget. Live recording and immutable capture retain a bounded recent timeline instead
of making ordinary native Save fail after prolonged editing. Older dropped commands are unavailable; the current
buffer and raw pending values remain exact. Capture can append a virtual pending row command to its immutable copy
without committing a live delegate, changing focus, or moving the live Undo cursor. History serialization runs at
native Save/recovery capture boundaries; authored-content and saved-baseline projections exclude it, and ordinary
viewport transforms reuse workspace revisions instead of traversing document timelines.

The viewport portion stores Editor and Game Preview fog visibility, Thing collision-shape and visual-bounds visibility,
classic 3DO model LOD selection, and Arch Thing gizmo visibility independently. The 3DO LOD choice is the
`thingModelLodEnabled` boolean in each viewport array's trailing named flag map; adding it does not change the native
SED version or shift positional fields. A valid EDIT payload overrides the corresponding app defaults. Fresh Editor
workspaces default both Thing overlays on; fresh Game Preview workspaces default them off. Classic 3DO LOD defaults off
in both. These choices are also remembered separately in app settings, so New and engine-format Open use the user's
latest per-pipeline choices. With no valid EDIT workspace, Editor fog defaults off and Game Preview fog defaults on;
Thing overlays use their app-setting defaults. Older EDIT records that omit the named 3DO LOD flag restore it off,
including when current app settings enable it; older overlay fields retain their app-setting fallbacks. Manual .sed Save,
autosave, and recovery share this workspace serializer. Bulk COG tab-close commands mutate only the already represented
open-tab set and require no schema field. Resource tabs use the optional `resourceEditors` extension so older EDIT V1
readers can ignore the new workspace without a native SED version change.

The existing viewport-state array's Cleave map may include the optional named `guides` map for independent angle-guide,
length-label, and line-number visibility. Units formats length labels and Add Shape dimensions without resizing
authored physical dimensions. IJIM offers Meters, IJU, and Bare; JKDF2/MotS offers Meters, JKU, and Bare. A saved unit
unsupported by the restored project normalizes to Meters. Legacy EDIT unit value `4` is accepted and normalized to
Meters with Lengths off, while explicitly saved Angles and Numbers values are preserved. When old preferences contain
only the shared measurement visibility switch, its false value initializes Lengths off while retaining the last real
unit; the obsolete preference is then canonicalized so future visibility changes can persist independently. Save,
autosave, and recovery share this compatibility behavior without changing EDIT or native SED versions.

The same Cleave map may contain `chainOrigin: [lineIndex, endpoint]` for the next chained cut. Both entries must be
integers: the line index addresses a saved staged line, and endpoint is `0` (first) or `1` (second). An absent field
uses the last staged line's second endpoint. Invalid shape, types, or bounds reject EDIT before live mutation.
The reference persists while Chain is off and leaves physical lines and their execution ranks unchanged. This optional field is shared
by Save, autosave, and recovery without changing the fixed arrays or format versions.

The Cleave map's optional `bounded` Boolean selects finite-stroke admission to descendants of earlier cuts. Present
values are authoritative, and a wrong type rejects EDIT before mutation. An older staged arrangement without the
field restores global-plane behavior; when no staged lines are present, absence retains the application's Bounded
preference, whose fresh default is on. Manual Save, autosave, and recovery share the field without a version change.

The named Cleave map at fixed viewport-array index `38` also supports these optional fields:

| Field | Contract |
| --- | --- |
| `currentFamily` | Integer `0` (Sector) or `1` (Surface); explicit even for an empty workspace. |
| `peerWorkspace` | Optional complete Cleave map for the opposite family, with inactive tool flags and no nested peer. |
| `applyOrder` | A full permutation of zero-based physical line indices, in execution-rank order. |
| `selection` | A map with `lines` as sorted unique physical indices and `active` as a selected member, or `-1` for empty selection. |
| `transformMode` | Integer `0` (Move), `1` (Rotate), or `2` (Scale). |
| `shape` | Versioned shape dimensions/counts and settled placement-armed state. |
| `history` | Retained line-only snapshots and current cursor, independently of Level `hist`. |

The outer Cleave map identifies the currently loaded workspace with optional integer `currentFamily`: `0` is Sector
and `1` is Surface. The optional `peerWorkspace` map carries the other family's complete Cleave representation,
including its own explicit `currentFamily`, options, draft, and local timeline. Peer tool-enabled flags are false;
the peer cannot contain another `peerWorkspace`. The two family identities must be opposite, and every valid current
or historical context must match its family's Sector/Surface mode. Empty workspaces remain valid with explicit family
identity and independent options. A wrong known type, conflicting identity, invalid dormant target, or malformed peer
rejects the complete EDIT workspace before either family is published.

Writers preserve both families, including empty workspaces with settled option choices. Legacy records without these
named fields infer their current family from a valid context, then from an exclusively enabled Cleave tool, otherwise
Sector. A missing peer starts empty from the application's fresh Cleave preferences; it does not inherit customized
options from the restored current family. New Project, Open, and document detachment end both draft lifetimes and
initialize independent options from those preferences before a valid EDIT overrides them.

Switching Sector/Surface or ordinarily deactivating Cleave retains both drafts, execution orders, line selections,
contexts, options, and independent local Undo/Redo cursors. Only the matching active family renders or captures draft
input/history commands. Clear resolves only the current family. Apply consumes only its family; the other family is
retained when its owning references remain valid. Coordinate, light, and UV changes retain compatible world-authored
planes. Destructive external reindexing, or unprovable structural history transitions, explicitly invalidate both
families rather than silently retargeting their indices. Apply's checked workspace memento is the family-specific
exception described below.

An unfinished Ctrl-wheel Rotate/Scale gesture checkpoints its settled pre-gesture draft and completed local history;
capture does not finalize the live gesture. Releasing Control or transferring tool/mode/family/focus ownership commits
one cumulative local command. Escape restores the baseline. Armed-shape wheel edits are future placement options,
not authored line commands, and capture retains their current option values. Neither live accumulator nor a prepared
workset is serialized, so this behavior requires no new wire field or format version.

Legacy records without order/selection use physical creation order and the scalar selected line. Without `history`, the
restored current draft begins a fresh local baseline when restoration admission permits. Present known fields have strict
types, finite ranges, bounds, and order/selection consistency; unknown optional names are ignored. A rank change moves
only an entry in the permutation, preserving physical geometry, links, colors, and origin references.

The optional `shape` map has integer `version: 1`, integer `kind` from `0` through `4` (Square, Rectangle, Regular N-gon,
Filled arch, Star), finite numeric `width`/`height` in `0.005..10000` world units, Boolean `dimensionsLinked`, integer
`segments`/`points` in `3..64`, and Boolean `placementArmed`. Square/N-gon use equal dimensions; other kinds honor Link.
Square remains a compatible wire value and restores as a linked Rectangle; it is not an offered shape choice. Missing
shape options use Rectangle with `0.1` world-unit dimensions. Stored dimensions retain their physical size when units
change. A unit unsupported by the restored project falls back to Meters without resizing the shape.
The optional named Boolean `cameraPlane` defaults to `false` when absent. It controls Sector-only camera-facing shape
placement; Surface placement continues to use its target face. A present non-Boolean rejects EDIT atomically. The
current shape plan must also validate. This records the settled tool choice, not a partially held placement gesture,
and adds no fixed-array field or EDIT/native version change.
The optional named finite numeric `rotationDegrees` defaults to `0` when absent. It stores the pending shape's
in-plane rotation; runtime options normalize complete turns to the signed interval `[-180, 180)`. A present nonnumeric
or non-finite value rejects EDIT atomically. Rotation changes the placement frame without changing stored physical
dimensions or the two-dimensional shape plan. Both families and Apply mementos share this field.

Apply mementos use the same shape map. Invalid known fields inside `workspaceHistory` follow that optional sidecar's
rejection policy rather than invalidating the otherwise valid current workspace.

The optional `history` map contains integer `version: 1`, a nonempty `snapshots` array of at most 64 states per family,
and an in-range integer `cursor`. Each snapshot contains `lines`, `context`, optional `chainOrigin`, `applyOrder`, and
`selection`, using the current draft's checked representations. All Level-relative targets are checked against the
owning Level. The cursor state's authored lines/context/origin/order must equal the separately stored current draft;
selection is restoration metadata and may differ after selection-only input. Both independent families share one
16 MiB authored draft/history admission budget, accounting for current and parked data, both retained timelines,
cursor/rollback duplicates, and decoding worksets before nested copies or reserves. Shape-planning and rendering
caches are separate from this budget.
Runtime and decoder admission use the same bounds. A malformed timeline rejects EDIT before any live publication;
a valid decoded prefix is never installed. Legacy current-only records retain their current lines when the complete
workset fits; an optional initial baseline is omitted when it cannot fit. Later edits or Apply require bounded storage
for rollback and the Apply memento before committing.

Viewport restoration checks both decoded source families, both existing runtime workspaces, and prepared candidates together
against the 16 MiB admission budget before any viewport mutation. Prepared containers publish by move after validation.
A retained timeline that cannot fit returns an explicit restoration failure with a warning; the Level remains recovered,
and neither a partial current nor parked draft/timeline is installed. A legacy current-only draft is retained when
its complete restore workset fits; its optional initial local baseline is omitted if that additional copy cannot fit.

The optional root `workspaceHistory` map is a separate app sidecar for Level-history transitions:

| Field | Contract |
| --- | --- |
| `version` | Integer `1`. |
| `undoCount`, `redoCount` | Exact saved HIST stack counts. |
| `entries` | At most 64 distinct `stack`/`index` bindings, with checked adjacent hashes and known payloads. |
| `pendingRedo` | Optional immediate matching Redo `stack`/`index` binding. |

Stack `0` means Undo and stack `1` means Redo; both zero-based indices count outward from the current cursor.
Each entry has 32-byte `beforeSha256`/`afterSha256` and one or both payloads:

- `cleave`: `workspace` using the named Cleave representation, checked `documentSelection`, the seven-item `active`
  array `[sector, surfaceSector, surface, thing, light, frameThing, frameValue]`, and the three-item `mode` array
  `[editMode, surfaceSubmode, lightSubmode]`.
- `layerVisuals`: `before` and `after` maps of `visible` Boolean rows, `locked` Boolean rows, and unsigned 32-bit RGBA
  `colors` rows, each matching its corresponding Level's layer count.

The hashes bind exact adjacent HistoryNested required-Level bytes in the existing checked HIST stream. Cleave targets,
document selection, and active items are validated against the **before-Apply** Level, including every retained local
snapshot. Layer metadata is validated against its respective before/after Level. Checked import remaps stack positions
to fresh runtime RecordIDs; branching or trimming prunes unreachable attachments. No Qt callback or runtime RecordID
is serialized. `pendingRedo` is restored only for the immediate matching transition and an unchanged restored
Apply-affecting draft in the applied family, even when that draft has a local Redo branch. Edits or successful local
Undo/Redo in that family revoke paired Level Redo priority; peer-family edits, selection, hover, and camera alone do not.
The memento carries only the applied family, identified by its optional named `currentFamily`. Its workspace forbids
a recognized `peerWorkspace`; such a field rejects the optional historical attachment transaction. Undo restores the
applied family and its document target selection/mode; Redo clears that family. Neither transition overwrites
independently edited peer work.

Workspace attachments have a separate 64-transition/16 MiB aggregate owned-storage cap, including nested timelines,
Layer vectors, and live/import/restore worksets. Admission is preflighted before candidate allocation/publication.
Malformed, mismatched, absent, or degraded HIST discards `workspaceHistory` as one optional transaction with a warning;
the current Level, current EDIT draft, and other workspace fields remain usable. These named extensions retain EDIT
schema version `1`, fixed viewport positions, HIST encoding, and native V1–V4 framing. Engine exports omit editor
workspace and history.

The optional named `edit.runtimeLighting` map stores the global viewport contribution switches: `thingLights` and
`sectorPointLights` are booleans. An absent map or field defaults to enabled; a wrong type for the map or either known
field rejects the EDIT workspace, while unknown future map keys are ignored. These choices are shared by Editor and
Game Preview and do not alter Level data or baked lighting. They live outside the legacy fixed-length `viewport` array,
whose positional layout is unchanged.

The optional named `edit.gridAppearance` map stores `minorDots` as a boolean and `minorDotSizePx` as a finite number
from `0.5` through `10`, measured in Qt logical pixels. Missing maps or fields restore Lines (`minorDots: false`) and
a `2`-pixel dot diameter, including when app preferences currently choose Points. Valid saved values override app
defaults. A wrong map/field type or invalid diameter rejects the complete EDIT workspace before application; unknown
keys are ignored. Optional `axisColors` is exactly three unsigned 32-bit RGBA integers for X/Y/Z; invalid tuples reject
EDIT before publication. Axis colors and minor-grid presentation remain independent of major-grid styling and the
snapping lattice. Native Save, autosave, and recovery share the extension without changing EDIT V1 or native versions.

The optional root `architecturalLighting` map retains settled MotS Arch workspace selection independently of Level
lighting values. Its version `1` fields are `authoringMode` (Mono/RGB enum), `targetMode` (Owner/Vertices enum),
`selectedVertices` (unique `[thing, lod, mesh, vertex]` integer identities), and Boolean `implicitOwnerExpansion`.
Identities require nonnegative int32 indices, LOD `0`, and a valid owning Thing. Encode/decode preflight a 16 MiB
owned-selection budget using a conservative 192-byte estimate per identity before uniqueness tables or nested
allocation. Known fields are strict; unknown optional names are ignored. A malformed map rejects the workspace atomically.
Model-dependent references activate only after the matching resource/mesh cache is ready. Missing resources retain the
accepted pending subset for later resolution; hover/cycle state, previews, and caches are transient. Owner-expansion
provenance is preserved rather than inferred from a coincidental selected set.

The independent optional `edit.window.uvGridAppearance` map uses the same `minorDots` boolean and finite
`minorDotSizePx` range `0.5`–`10`, with missing-field defaults of Lines and `2` logical pixels. It controls the UV
Sub-grid, separately from the world grid, UV spacing, snap state, and repeated-tile borders. Invalid known values reject
the complete EDIT workspace before mutation; unknown map keys are ignored. It uses the existing EDIT V1 schema.

Frame selection references are raw Thing/value indices paired with their exact Level. Before manual Save, the app
strictly validates live EDIT and remaps selected frames and `active[5]`/`active[6]` by logical frame ordinal into the
save-normalized candidate. The isolated HIST snapshot receives that same current selection; each historical state
remaps its own frame ordinals during frame-count normalization. Invalid live references or missing candidate frames
abort preparation before writing, without changing the live document or destination.

The ownership boundary is intentional: `head` through `hnod` are the authoritative authored Level (geometry, resources,
Things, templates, COG instances, editor lights, and 3DO hierarchy). `edit` is the persistent editor workspace,
including unsaved COG and resource-editor buffers and window/UV state; it is not a second Level copy. `hist` is optional undo/redo
history, represented by checked nested-Level byte references and deltas. It is never needed to open the current Level.

The required nested UV workspace has one shared edit representation: manual `.sed` Save, `.sedrecovery`, and periodic
autosave do not use separate UV-state codecs. `edit` is authoritative for the workspace and remains a hard Save
requirement. If only `hist` serialization or its
64 MiB validation fails, manual Save still atomically writes Level+`edit`, omits `hist`, and emits a visible Messages
warning. Allocation/logic failures and any Level/`edit`/write failure remain hard.

Consequently opening a current `.sed` applies the Level with `keepView`, then restores `edit`; it does not run the normal
camera/mode reset or deferred viewport refit. JKL, NDY, JED, ASC, and LEV continue to reset/refit because those formats do
not contain editor state. `edit` is schema V1 only and requires both explicit `tools.transformActive` and
`window.uvState`; no earlier editor-state shape is migrated. If the whole optional `edit` section is absent or rejected,
safe workspace defaults include a clean fitted UV canvas and default UV controls.

Before Qt materializes `edit`, a streaming CBOR pass bounds it to a 128 MiB decoded-graph/verification ceiling,
2,000,000 nodes, depth 64, and bounded
individual text/byte strings; it also rejects indefinite/tagged, trailing, and noncanonical encodings. Schema and
reference validation then remains all-or-nothing.

The three-value editor-mode array retains the current edit, Surface, and Light modes. Light values are `0` Baked,
`1` Thing, `2` Sector, and `3` Arch; the Arch value uses the same V1 workspace schema in native Save, autosave, and
recovery. Unknown, negative, or noninteger enum values reject the complete EDIT payload. Restore applies the normal
project rules: Sector is available in IJIM and Arch in MotS; inapplicable Light submodes fall back to Baked.
If manual Save rejects the captured EDIT payload, it preserves the previous file and dirty document and reports the
failure once in Messages plus a critical **Save failed** dialog.

`edit` is a stable workspace checkpoint, not a journal of every in-progress input transaction. The Level remains the
authoritative authored state. Staged Multi-cut lines are retained, but an active Surface Extrude preview and a partially
held brush/draw or other pointer gesture are finalized or cancelled at capture/apply boundaries rather than replayed
mid-gesture after restart. Multi-cut retains both independent drafts and local Undo/Redo timelines/cursors even when its tool
is inactive or the workspace uses Game Preview; restoring that data does not activate Cleave or change the saved edit
mode/selection. Draft input and local Undo/Redo resume only when the matching Cleave context owns the viewport, with checked
Apply/Layer attachments when HIST is usable. UV modal `G`/`S`/`R` transaction bookkeeping is not stored; only its settled view and selection
sidecar is restored. `edit` can contain the user's only unsaved COG source, so it receives core-tier physical
protection even though the Level remains valid without the application section.

### Compression

Each raw section from 4096 bytes through 64 MiB is offered exactly once to SED's bounded fast LZSS codec and retains the
result only when strictly smaller. For non-`hist` sections, smaller and larger sections remain raw; `hist` is type-specific
and rejects expanded payloads above 64 MiB.

### Native parity algorithm

The chosen logical bytes occupy stream-major 4096-byte slots: 4092 coded bytes plus a trailing CRC32C. Adaptive `k` is
at most 16. Every independent stripe uses a **systematic Reed–Solomon-style Vandermonde MDS erasure code over GF(256)**
with primitive polynomial `0x11d`. The encoder forms `V[row][column] = row^column` in GF(256), inverts the top `k` rows,
and multiplies the full Vandermonde matrix by that inverse so the first `k` generator rows are the identity. The stored
parity rows follow those systematic data rows.

There are four parity streams for the eight required Level sections, core-owned `jimp`, and `edit`, and two for
lower-priority `hist` and future optional sections. The guarantee is `p` erasures per stripe, not per whole section; corruption can span many
stripes as long as each remains within budget. More than four bad slots in one required-Level/`jimp`/`edit` stripe, or more than two in
one `hist` stripe, loses that whole semantic section. One expanded SHA-256 catches CRC-consistent corruption but cannot
localize or repair the bad slot. Native sections have no FEC tables, whole-file CRC, or duplicate raw/stored CRC fields.

`hist` deliberately uses two parity streams rather than four because it is a reconstructible, lower-priority timeline and
is safely discardable when one stripe exceeds its repair budget; the current Level and unsaved COG-bearing `edit` state
use four. This preserves faster normal Save/Open and lower history overhead while protecting the data whose loss would
change the authored level or discard irreplaceable work.
The LZSS matcher examines no more than four chained candidates per position with a bounded older-candidate comparison
budget; on the reference `test.sed` this improves canonical size by 8.79% without changing the shared native token
format.
The decoder rechecks the 64 MiB expansion bound before reserving its exact output size. Writer-side Level bounds and
finite-value validation run before categorized resource indexing, and public profile-taking codec APIs reject unknown
profile enum values instead of falling through to either nested profile.

### Protection boundaries and corruption model

The parity boundary is only the fixed-slot payload of one top-level semantic section. Fixed headers and section
descriptors use CRC32C-checked mirrors instead; alignment padding is canonical zero (strict rejects nonzero bytes while
corruption recovery normalizes/reports them); expanded SHA-256 covers decoded semantic
bytes; and `sedend\r\n` is only a marker. Native SED versions intentionally have neither a descriptor directory nor on-disk
record-group shards (the writer may use validated record boundaries for HIST pooling):
an over-budget stripe drops/defaults the entire semantic section, while later sections are recovered through mirrored
descriptors and checked anchors. After reconstruction the reader recomputes CRC-valid parity rows as a diagnostic, so a
wrong parity payload with a recomputed CRC remains observable without sacrificing systematic data that passes SHA-256.

Even a tiny required Level domain stays in its own section for salvage and failure isolation. The recovery envelope makes
a different tradeoff for its tiny identity/timestamp/digest fields: they form one atomic claim and are grouped in the
single protected `meta` unit.

Fixed-slot repair assumes checked framing still establishes the absolute 4096-byte slot phase. It can correct localized
byte/bit substitutions and treat persistently unreadable ranges inside a known original file extent as erasures within
each stripe's budget. An ordinary shorter file does not reveal how many slot bytes were removed and is not generally
repairable as an erasure; only specifically recognizable redundant trailing-framing loss has a safe exception. A byte
insertion or deletion in the middle likewise shifts all later boundaries and is outside all native versions; recovery
does not guess a new phase.

### History storage and deltas

History uses `sed-semantic-base-delta-v1`: each nearest-first undo/redo state is an explicit history-nested SED Level
split at semantic boundaries. New HIST payloads carry `fragmentLayout = sect-record-v1`; the older seven-key V1 HIST
shape without that field is explicitly decoded as `fixed-v1`, never guessed. With `sect-record-v1`, the large `sect`
frame is partitioned at writer-validated Sector-record boundaries, while all other sections use canonical fixed 8192-byte
blocks (with a shorter final block). The explicit `fixed-v1` fallback uses fixed blocks for every section. A size-changing
semantic frame may also use the
writer-only hybrid shifted-tail layout documented in the normative specification. That layout matches an exact
section-body suffix, directly reuses unchanged same-offset prefix spans, ends the changed bridge at a prior fragment
boundary, reuses shifted unchanged spans, and emits a fresh checked trailer/tail. Exact fragments are pooled across
states, same-offset, shifted, or record-reused spans reuse their prior references without copying unchanged bytes. For
same-index Sector records with the same bounded fragment layout, the writer compares bytes in place and avoids even a
temporary fragment allocation or pool hash; only changed/reordered records are materialized for pool lookup. The
consecutive grow/shrink edits retain their variable span state. The final HNOD/trailing-framing 64-byte tail remains
atomic. Across a native-version transition, each historical state's fixed header and matching HNOD mirror/footer stay
paired, so recovery never combines descriptors with another version's trailing framing.
The record-oriented base is bounded to 300,000 references per reconstructed state, including descriptor/count bridges
and large-record pieces. The bounded scanner aborts before reserving an oversized record-span vector; a valid Level that
would exceed this budget falls back to the explicit `fixed-v1` history layout before HIST is published.
The writer keeps separate undo/redo caches of the exact generated required-section frames. `DocumentUndoHistory` derives
invalidation from the concrete compact/full undo Snapshot variant and exposes it only through a noncopyable token bound
to the exact temporary Level being visited, rather than trusting `dirtyFlags`: Thing records regenerate `head` and
`thng`; COG records `head` and `cogs`; Light records `head`, `sect`, and `lite`; combined Thing/Light records add `thng`;
and Sector/Surface subsets regenerate `head` and `sect`. Full snapshots regenerate all required sections. Manual frame
normalization explicitly adds `thng` when it changes a temporary historical state. `rsrc` is always rebuilt and compared
byte-for-byte; a catalog change also regenerates every category-indexed section (`head`, `sect`, `thng`, `tmpl`, `cogs`,
and `hnod`). History-nested frames have no offset-dependent FEC alignment, so unchanged core-generated frames can be shared
without changing canonical bytes. A requested frame that regenerates to the same exact bytes keeps its previous shared
owner. HIST maps each undo/redo frame owner to its already-interned fragment references, so an unchanged owner skips
nested-file assembly, multi-megabyte copying, content-boundary scanning, hashing, and equality searches. Only changed
frames are fragmented; generated bytes are still compared before an owner is retained, so pointer identity is never an
integrity proof. The legacy last-fragment rule remains exact: the atomic final 64 bytes contain the final 16 `hnod` bytes
and the 48-byte checked header-mirror/footer. Each stack advances only after the complete entry succeeds, and failure
rolls back newly interned fragments. For an exact in-process `SectorSubset` or `SurfaceSubset` transition, the writer also
receives the Snapshot's checked sector-index list. It copies unchanged sector records from the prior checked `sect` frame
and serializes only touched records before rebuilding the canonical frame; topology changes and full/baked-light Sector
transitions use the complete encoder. This is a writer-side optimization only: the index list is never persisted or inferred
from an untrusted dirty flag, and the resulting frame remains byte-checked and canonical.
Nested history Levels carry the native version selected by their Level UV coordinate-space tag and retained
arch-lighting data: V1 for `TexturePixels` without ARCH objects or bake baselines, V2 for `Normalized` without either,
V3 for `Normalized` MotS states retaining objects or baselines, and V4 for their `TexturePixels` counterparts. They
remain required-section-only documents: exactly `head`, `rsrc`, `sect`,
`thng`, `tmpl`, `cogs`, `lite`, and `hnod` in that order, with timestamp zero and no optional, compressed, or
parity-protected section. The containing Level and its accepted history states use one coordinate space. This native
version choice does not rename the enclosing HIST CBOR schema, encoding, or fragment-layout values, which remain V1.
The canonical restriction is enforced by both ordinary nested decoding and the consuming history decoder.

On load, every reconstructed nested document still passes complete fixed-header/mirror/footer, descriptor/mirror,
history CRC32C, size, and canonical-order validation. The consuming decoder compares the complete checked
frame bytes with its own previously accepted state and skips semantic decoding only for an exact match. `rsrc` is always
decoded; if its exact frame changes, all category-indexed domains are decoded even when their frames are otherwise
unchanged. The assembled Level is then revalidated as one graph and its rebuilt resource catalog must equal the decoded
canonical first-use catalog. HIST `dirtyFlags` are checked only afterwards for compact undo representation and never
authorize section reuse. A failed transition does not advance the accepted Level or byte context.
Each stack is seeded from the current-Level reference list, so its first state uses the canonical splice delta when that
is smaller than a checkpoint. A full checkpoint is mandatory every 16 subsequent states; other states likewise use the
canonical longest-prefix/suffix splice delta only when its CBOR is smaller than a checkpoint. Nested sections keep
descriptor CRC32C, payload CRC32C, and mirrors but have no per-state SHA-256, compression, or parity; the enclosing
`hist` section retains the strong expanded SHA-256, makes the single normal compression decision only when its raw size
is 4 KiB..64 MiB, and supplies two parity streams. Its
entry manifest stores four required fields (label, dirty flags, historical-state selection, and Level reference record)
plus an optional trailing compact post-operation navigation selection. The last field is omitted when absent, preserving
byte-identical ordinary history-schema V1 timelines. It preserves exact History targets for structural
edits such as Sector/Surface cleaves even if the user changes selection before saving; it never contains a Level copy.
The decoder validates that hint against the exact adjacent post-operation Level (before advancing an Undo stream or
after reconstructing a Redo state). A dangling or incompatible hint rejects optional HIST rather than admitting a
misdirected navigation target.
bounded-scratch CBOR preflight caps the object graph at 2,000,000 nodes, depth 64, 8 KiB byte strings, and 3 MiB text
items; the semantic history codec admits 100,000 combined undo/redo entries while the live GUI retains at most 1,024,
1,000,000 pooled fragments, 64 MiB of cumulative pool bytes, and 300,000 references per state. Nested reconstruction
work is bounded independently. Each transient decoded Level remains limited to 128 MiB/two million items. The loader
immediately converts it to the live compact snapshot domain selected by its dirty flags. Its trusted adjacent-state
decoder keeps only the prior values of domains that changed; transition checks and subset discovery use those values
directly instead of retaining or copying a complete adjacent Level. Topology-preserving Sector/Surface transforms retain only the changed owning
Sectors (including adjoin/shared-vertex state); structural/unknown operations still retain a complete Level. The
optional timeline's 768 MiB/32-million-item aggregate envelope charges that compact retained representation, so
Thing/light/COG and geometry transforms do not multiply unchanged data. Reconstruction work separately charges shared
base/pool fragments once plus a bounded per-state floor.

### Loading and partial recovery

A checked 40-byte `mhdr` copy of the 28-byte fixed header precedes the final eight-byte `sedend\r\n` marker. There is no
whole-file CRC. Strict loading validates or exactly repairs checked metadata and slots and rejects over-budget sections.
The corruption-only API instead reports structured repaired/missing/unidentified sections and unreadable ranges, defaults
an unrepairable required Level domain as a whole, and omits an unrepairable optional domain. A checksum-valid required
Level schema error remains hard; application-owned optional `edit`/`hist` is validated atomically and may be omitted or
defaulted with a warning and clean-save requirement without half-applying it or vetoing an intact Level. When a primary
header is gone, recovery reuses its valid trailing mirror verbatim; if
that mirror is also gone it synthesizes checked framing and lets surviving `head` choose the project type. If both header
copies and `head` are lost, `projectType` is empty and `projectTypeTrusted=false`; the default `Level.kind` is only a
placeholder and must never be treated as JKDF2. A valid original header/mirror disagreeing with `head` is never
overridden. Strict loading requires the trailing mirror/footer; corruption recovery may probe a checked mirror when the
footer was damaged. Recovery files are admitted at 256 MiB before allocation, read into an exact-size buffer in bounded
1 MiB device transfers, and probed
for one trailing byte so concurrent growth cannot turn a checked size into an unbounded read. Genuine device failure
retries 4096-byte I/O ranges three times before zero-filling and reporting an erasure. Canonical `docm` is validated
through a non-owning view and copied only once after acceptance. Unknown future sections are exposed opaquely only after
complete validation.

Direct `.sed` Open is strict first and uses corruption recovery only when strict validation fails. Repaired-complete and
identified optional-only-loss documents remain associated but modified; missing `edit`/`hist` uses safe
workspace/history defaults, while lost `jimp` clears only its pending import marker/map. Required-Level loss or an unidentified unit is confirmed before candidate resource
settings/locator or the current document are changed, then opens detached as `Untitled*`; unidentified framing is
treated conservatively because it may conceal a required domain. Cancel has no candidate side effects. When project
metadata is unknown, one combined
dialog requires an explicit JKDF2/MotS/IJIM choice before history validation, resource setup, application, or salvage.
The damaged source is never rewritten. Background text salvage re-reads it only while size/time and normalized semantic
loss/project classification still match the accepted result, reapplies any explicit project choice, and aborts if the
source was replaced.

## Legacy pixel-space UV admission and saving

The core codec and [`loadDirectSedDocument`](../src/app/src/ui/project/direct_sed_open_utils.cpp) preserve the native
version exactly: V1/V4 publish a `TexturePixels` Level and V2/V3 publish a `Normalized` Level. The editor applies a
separate resource-backed admission policy only after the complete candidate, optional HIST, project type, and resource
locator have been validated, and before the current workspace is replaced.

**Preferences > Save & Recovery > Legacy Level UVs** offers three policies:

- **Normalize automatically when possible** is the default and converts a legacy pixel-space document to normalized
  UVs without another prompt
- **Ask before normalizing** prepares and validates the complete conversion, then offers Normalize, Keep Pixel Space,
  or Cancel
- **Keep legacy pixel space** opens V1 or V4 without conversion

Normalization divides each U coordinate and `uScale` by that surface MAT's positive first-cel width, and each V
coordinate and `vScale` by its height. It converts the current Level and every Undo/Redo state as one failure-atomic
document transaction. If a current or historical surface has a missing, unreadable, dimensionless, or invalid MAT,
	nothing is converted: the complete Level and timeline remain in legacy pixel space and the warning identifies the current or
historical Sector/Surface that blocked admission. Cancel in Ask mode likewise leaves the preceding workspace and its
resources unchanged. The conversion implementation and compact-history rebuilding contract are in
[`uv_promotion.cpp`](../src/core/src/editor/uv_promotion.cpp).

While a live document remains in texture-pixel compatibility space, an amber status-bar marker shows **UV: Legacy
pixels**. It changes to **UV: Ready to normalize** when every current and historical MAT dimension is available but the
selected policy keeps the document in pixels. Relevant loose-resource, archive, and resource-configuration changes are
generation checked and evaluated from an immutable snapshot off the GUI thread. Automatic mode then retries the whole
document transaction; Ask prompts only when the document newly becomes convertible; Keep never promotes it. A file,
policy, selection, document, or resource change during preparation or the prompt invalidates that result and forces a
fresh check instead of publishing stale dimensions. Failed retries remain entirely in pixel space and repeated watcher
notifications for the same missing MAT do not repeatedly rescan the full history.

When a normalized live document is still associated with an existing legacy pixel-space path, Save never silently
replaces that file with a different coordinate-space format. **Upgrade and Save** writes V2 for a normalized Level
without retained ARCH objects or bake baselines, or V3 when either remains. **Keep legacy pixel space and Save**
converts an isolated current-Level plus complete-history snapshot back to pixels and writes V1 without either or V4
when MotS objects or baselines remain,
while leaving the live document normalized. If reverse conversion fails, the editor identifies the current or
historical Sector/Surface and leaves both the existing file and normalized live document untouched. Cancel also writes
nothing.
The app remembers the source's legacy pixel-space identity independently from the normalized live Level so a repaired
fixed header cannot bypass this choice.

## Recovery use

The Recovery V1 writer emits exactly one raw native SED V1 through V4 document in one authoritative `docm` chunk. A
capture keeps the Level's UV coordinate-space tag and MotS arch-lighting table and bake baselines. A normalized Level
without retained objects or baselines is native V2; a normalized MotS Level retaining either is V3. The corresponding
explicitly `TexturePixels` Levels use V1 and V4. The recovery reader accepts all four native versions. The envelope
does not
serialize through JED or maintain separate outer copies of Level, `edit`, or `hist`. `docm` always contains the current
Level and `edit`, and normally contains `hist`; a history encode, size, or semantic-verification failure instead publishes
Level+`edit` with a persisted `warn`. A Level/`edit`/document validation failure publishes nothing. This avoids duplicate
authority and double compression/parity because native sections already supply independent protection and
partial-recovery boundaries.

The outer `.sedrecovery` V1 envelope supplies discovery metadata, source/session identity, a protected metadata chunk,
scanning anchors, forensic retention, and atomic `QSaveFile` autosave publication. `docm` is raw native `.sed`; its outer
CRC/SHA fields are canonical zero markers so validation is not duplicated. `meta` alone retains the outer format's compact
16+2 P/Q/table scheme and IEEE CRC-32. Its JSON requires exact lowercase 64-character hexadecimal digests and binds both
the canonical required-Level-only nested SED bytes and the complete canonical protected `docm`. After identified optional
loss the complete-document binding is explicitly unavailable, but the canonical required-Level binding is still enforced
before recovery proceeds. Mirrored meta tables are independently decoded when they disagree; only a unique fully checked
result (or identical reconstructed metadata) is accepted, and shard data must end at the exact mirrored-table boundary.

The writer emits only one `docm`. A reader ignores an identical valid duplicate with an explicit corruption warning, but
rejects conflicting valid document copies. If native damage exceeds a stripe's tier, recovery retains other verified
sections, marks the result partial/corrupt, and preserves the original binary as a uniquely named forensic copy.

Only required-Level loss or an unidentified lost unit schedules a non-authoritative JKL/NDY export plus
provenance/limitations manifest beside that forensic input (or beside the unchanged source for direct `.sed` Open). An
unidentified manifest cannot name the lost unit and warns that it may have been required. Identified optional-only
`edit`/`hist` loss does not schedule salvage. Native
text is emitted only for a checked or explicitly selected project; unknown project identity blocks JKL/NDY. It contains
only fully decoded semantic domains plus defaults for lost domains, never bytes from inside an unrecoverable section.
The published text is produced with the ordinary readable JKL/NDY writer (`compact=false`), never the optional compact
runtime-export presentation, so a person can inspect/edit the surviving records. This text is validated and atomically
published without replacing existing files, but it is only a manual diagnostic aid; the unchanged damaged binary remains
authoritative evidence and the sole source for unrecoverable bytes.

Atomic publication preserves only completed checkpoints. An unconditional hard kill can recover the last already
published snapshot, not edits made after that publication boundary.

## Implementation and tests

The portable `saveToSED(Level, path)` API serializes and validates the complete image before creating an exclusively
owned sibling staging file. Checked write, flush, and close precede same-volume replacement (`MoveFileExW` on Windows,
`rename` on POSIX); reported serialization or filesystem failures preserve the previous destination and remove the
unpublished staging file. Stream overloads cannot roll back a caller-owned stream. MainWindow continues to own its
`QSaveFile` transaction with direct-write fallback disabled. These operations do not change the binary wire format or
promise survival of every storage-device or power-loss failure.

- codec: [sed.hpp](../src/core/include/sed/assets/sed.hpp) and [sed.cpp](../src/core/src/assets/sed.cpp)
- format coverage: [sed_level_tests.cpp](../src/core/tests/assets/sed_level_tests.cpp)
- recovery integration:
  [document_recovery_utils.cpp](../src/app/src/ui/recovery/document_recovery_utils.cpp)
- full editor Save/Open integration: [file_menu.cpp](../src/app/src/main_window/menus/file_menu.cpp) and
  [startup.cpp](../src/app/src/main_window/project/startup.cpp)
- direct `.sed` strict/recovery classification:
  [direct_sed_open_utils.cpp](../src/app/src/ui/project/direct_sed_open_utils.cpp)
- recovery coverage: [document_recovery_tests.cpp](../src/app/tests/document_recovery_tests.cpp)
- checked-in native vectors and manifest:
  [sed_vectors](../src/core/tests/assets/fixtures/sed_vectors/README.md)

Tests cover deterministic byte-exact V1 pixel-space and V2 normalized round trips for JKDF2, MotS, and IJIM against
independent checked-in native SED and expected JKL/NDY files; focused MotS V3 normalized and V4 pixel-space
arch-lighting table round trips; authored bake-baseline round trips; absent-trailer compatibility; baseline semantic equality
and invalid-baseline writer rejection; bit-exact signed/multi-tile normalized UV/scale round trips; and per-Thing reference
persistence; exact checked-in partial JKL/NDY output after a checked-in five-erasure `sect` failure; all
14 resource catalogs and their category-local references across Level sections; the same normal/partial conversions
through the production autosave/recovery serializer; the fields previously lost through JED;
compression thresholds; semantic pooled history with reference-list deltas and periodic checkpoints; exhaustive
adaptive-`k` MDS erasure combinations through `p=4`;
stream-major multi-stripe and resilient-I/O repair; descriptor/header mirrors; CRC-consistent strong-hash failure;
structured partial recovery; trusted versus unknown project identity; strict-first direct Open cancellation/association/
source-preservation transactions; outer recovery/document round trips; and checked/user-selected non-authoritative
JKL/NDY salvage behavior.
