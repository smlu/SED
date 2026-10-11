# Sith Editor binary document format (`.sed`)

## Overview

`.sed` is SED's native, complete editor document, comparable in purpose to Blender's `.blend`. It is not an engine
level export. A file contains the lossless cross-project `Level`, stable workspace/editor state, unsaved COG editor
buffers, and the functional undo/redo timeline. JKDF2/MotS `.jkl`, IJIM `.ndy`, and legacy `.jed` remain interchange
formats and intentionally do not retain the workspace.

Native SED V1 through V4 are little-endian and sectioned. V1 stores `TexVertex.u/v` and `Surface::uScale`/`vScale` in
legacy texture-pixel units; V2 stores those same fields in normalized texture space. V3 and V4 add the optional MotS
`ARCHLIGHTING` table and optional authored bake-provenance trailer to the `thng` section while retaining the corresponding
normalized (V3) or pixel-space (V4) UV meaning. A Level whose serialized snapshot has neither retained ARCHLIGHTING
objects nor bake baselines continues to use V1 or V2, preserving the established bytes. The canonical live editable
`Level` is `Normalized`, so the writer emits V2 unless the MotS snapshot retains ARCHLIGHTING objects or bake
baselines, in which case it emits V3. It emits V1/V4 only for a compatibility Level explicitly tagged
`TexturePixels`; the reader accepts all four versions and preserves the corresponding tag without silently converting
coordinates. Native Save filters the table on an isolated snapshot: active Arch Thing references and their records remain,
nonzero authored records remain dormant when the Arch flag is cleared, and known off-flag/unreferenced all-zero
records and their references are omitted. When neither an instance nor a Level-local template supplies `thingflags`, the
external template database is outside the SED writer API, so a referenced all-zero record is preserved as potentially
active. The live editor `Level` is not modified by this filtering. Every top-level semantic payload is independently bounded, optionally losslessly
compressed, protected by fixed 4096-byte CRC32C-checked slots, and verified by one SHA-256 of its expanded bytes.
The eight required Level sections, core-owned `jimp`, and `edit` use four parity streams per stripe; `edit` is
structurally optional but receives the core tier because it can contain the user's only unsaved COG source. Lower-priority
`hist` and future optional sections use two parity streams. The fixed header and every section descriptor have checked mirrors. There is
deliberately no whole-file CRC, FEC table, or second pair of stored/raw payload CRCs in any native version.

The lossless claim applies to the authored fields in the eight required-domain schemas plus the optional core-owned
`jimp` import-state section. Pending `Level::importedJklCogRowsPresent` and `importedJklSurfaceRefs` are retained when
meaningful. Saving unresolved ambiguous imported Surface ordering remains rejected. Canonical-order unknown `paramN` rows
can therefore retain their pending hydration marker across native reload while raw arguments remain in `cogs`. A false
marker with an otherwise unused map is normalized away, so ordinary healthy files keep their existing bytes. The optional
app-owned HIST `jklCogImportV1` field separately retains current and affected undo/redo metadata; nested history Levels
remain exactly the eight required sections. No native file-version bump is involved.

The conformance suite covers adaptive `k=1..16` stripe packing, one-through-four-erasure decode matrices, checked
framing and mirrors, canonical padding, optional application schemas, recovery-envelope binding, partial-Level salvage,
bounded hostile-input work, all supported project exports, every `rsrc` category, legacy V1 pixel-space compatibility,
and V2 normalized-space round trips, plus MotS ARCHLIGHTING table and authored bake-baseline round trips. V2 changes only the declared meaning of
the UV and scale fields. V3/V4 additionally extend the `thng` payload with one Thing reference, the ordered table,
and the optional explicitly identified bake-provenance trailer;
all framing and protection algorithms remain unchanged. Any future wire-byte or semantic change must update this
specification and version the implementation deliberately; readers must not accept alternate layouts.

The systematic Vandermonde MDS code is used because native sections are large, independently recoverable semantic
units: it repairs any `p` CRC-localized slot erasures with exactly `p` parity streams, preserves healthy data slots
unchanged, and supports the adaptive two- and four-parity tiers with one byte-oriented GF(256) codec. The full design
rationale is under [Why this algorithm was chosen](#why-this-algorithm-was-chosen).

```text
offset 0
  +------------------------------+
  | "sed\0"                      |  4-byte file magic
  | fileVersion = 1, 2, 3, or 4  |  u32 LE
  | projectType                  |  i32 LE
  | sectionCount                 |  u32 LE
  | timestamp                    |  i64 LE; Unix milliseconds, 0 means unknown
  | checksum                     |  u32 LE; CRC32C
  +------------------------------+
  | head section                 |  required Level metadata
  | ... each section ends with   |  checked descriptor mirror
  +------------------------------+
  | rsrc section                 |  required categorized resource/name catalogs
  +------------------------------+
  | sect thng tmpl cogs          |  required Level collections
  | lite hnod                    |
  +------------------------------+
  | jimp section (optional)      |  pending JKL COG import provenance
  +------------------------------+
  | edit section (optional)      |  editor state; stable workspace + COG buffers
  +------------------------------+
  | hist section (optional)      |  undo/redo history
  +------------------------------+
  | "mhdr" header mirror         |  checked copy of fixed header
  +------------------------------+
  | "sedend\r\n"                 |  8-byte footer magic; no file CRC
  +------------------------------+
```

Every labeled semantic section in this overview is physically
`100-byte sectv1 descriptor + payload + 116-byte sedmir1 trailer`, as specified below.

## Contents

- [Version and upgrade rules](#version-and-upgrade-rules)
- [Fixed header](#fixed-header)
- [Primitive encodings](#primitive-encodings)
- [Section framing and descriptor mirror](#section-framing-and-descriptor-mirror)
  - [Exact protection boundaries](#exact-protection-boundaries)
- [Per-section integrity and striped MDS protection](#per-section-integrity-and-striped-mds-protection)
  - [Slot and stripe layout](#slot-and-stripe-layout)
  - [Systematic Vandermonde MDS parity algorithm](#systematic-vandermonde-mds-parity-algorithm)
    - [Why this algorithm was chosen](#why-this-algorithm-was-chosen)
  - [Repair guarantees and corruption limits](#repair-guarantees-and-corruption-limits)
- [Compression](#compression)
- [Required Level sections](#required-level-sections)
  - [`head`](#head)
  - [`rsrc`](#rsrc)
  - [`sect`](#sect)
  - [`thng`](#thng)
  - [`tmpl`](#tmpl)
  - [`cogs`](#cogs)
  - [`lite`](#lite)
  - [`hnod`](#hnod)
- [`jimp`](#jimp)
- [Extensibility](#extensibility)
- [Conformance vectors](#conformance-vectors)
- [Application sections](#application-sections)
  - [`edit`](#edit)
    - [Workspace schema and validation](#workspace-schema-and-validation)
    - [UV editor workspace](#uv-editor-workspace)
    - [Stable checkpoint boundary](#stable-checkpoint-boundary)
  - [`hist`](#hist)
    - [CBOR schema and base references](#cbor-schema-and-base-references)
    - [Compact snapshot restoration](#compact-snapshot-restoration)
    - [Checkpoints and deltas](#checkpoints-and-deltas)
    - [`HistoryNested` validation and frame caching](#historynested-validation-and-frame-caching)
    - [Sector-record fragmentation](#sector-record-fragmentation)
    - [Shifted-tail reuse](#shifted-tail-reuse)
    - [Canonical bounds and outer protection](#canonical-bounds-and-outer-protection)
- [Strict loading, partial recovery, and resilient I/O](#strict-loading-partial-recovery-and-resilient-io)
- [Limits and implementation](#limits-and-implementation)

## Version and upgrade rules

The four bytes immediately after the aligned `sed\0` signature are the file-format version. Readers accept exactly V1,
V2, V3, or V4 and reject any other value before interpreting sections. V1 and V4 declare the Level's surface texture
coordinates and `uScale`/`vScale` to be legacy texture-pixel units; V2 and V3 declare those fields to be normalized
texture coordinates. Signed and multi-tile values remain valid in either space; normalized does not mean clamped to
`0..1`.

| Version | UV/scale units | ARCHLIGHTING table field | Writer selection |
|---:|---|---|---|
| V1 | `TexturePixels` | absent | No retained ARCH objects or bake baselines; Level is explicitly pixel-space |
| V2 | `Normalized` | absent | No retained ARCH objects or bake baselines; Level is normalized |
| V3 | `Normalized` | present in `thng`, possibly empty | MotS snapshot retains ARCH objects or bake baselines; Level is normalized |
| V4 | `TexturePixels` | present in `thng`, possibly empty | MotS snapshot retains ARCH objects or bake baselines; Level is pixel-space |

The writer selects V1/V2 when its filtered snapshot has neither ARCHLIGHTING objects nor bake baselines and V3/V4
when either remains. A provenance-only V3/V4 file writes an empty table followed by its bake-provenance trailer.
ARCHLIGHTING objects and bake baselines are valid only for MotS, and every serialized `Thing::archLightingIndex`
must be `-1` or address an object in the table. It rejects an unknown coordinate-space tag or an invalid source
reference before writing. Reading V1/V4 publishes `TexturePixels`; reading V2/V3 publishes
`Normalized`. Loading does not need MAT dimensions and does not mutate either representation. Exact resource-backed
conversion, when requested elsewhere, converts every `TexVertex.u/v` and the matching `Surface::uScale`/`vScale` as one
failure-atomic Level operation.

That statement describes the raw codec contract. Sith Editor's MainWindow applies a higher-level legacy-pixel admission
policy after direct native validation and resource staging. The default policy attempts resource-backed normalization of
the current Level and every HIST Undo/Redo state as one transaction. Ask mode offers Normalize, Keep Pixel Space, or
Cancel; the compatibility policy keeps the pixel-space document unchanged. Any missing or invalid current/historical
MAT dimension rejects the entire conversion and retains one pixel-space document. The implementation is
[`uv_promotion.cpp`](../../src/core/src/editor/uv_promotion.cpp), and the user-facing workflow is summarized in
[`user_guide.md`](../user_guide.md#15-save-save-as-and-engine-export).

A normalized document associated with an existing legacy pixel-space path retains that source-format fact outside the
serialized Level. Overwriting the path requires an explicit choice. Upgrade writes V2, or V3 when the normalized MotS
snapshot retains ARCHLIGHTING objects or bake baselines. Keep legacy pixel space performs the inverse resource-backed
conversion on an isolated
Level-plus-HIST snapshot and serializes V1, or V4 when that pixel-space MotS snapshot retains ARCHLIGHTING objects
or bake baselines, while the live document remains normalized. Cancel and conversion failure leave the destination unchanged. This application policy
does not alter any native wire field or the raw reader's exact coordinate-space publication.

Each section descriptor also carries the containing native version and must match the fixed header. The section
identifiers, framing, checksum/hash algorithms, compression tokens, parity matrix/tiering, bounds, and recovery behavior
are identical across versions. V3/V4 change only the `thng` payload as specified below; all other required sections
retain the established field order.

The fourth magic byte remains NUL: it is part of the aligned format discriminator, not a hidden version field. Moving
the version into that byte would save only the following four-byte field while coupling identification to versioning,
limiting the simple discriminator to 255 nonzero values, and making future version inspection less explicit. The aligned `u32`
version therefore remains separate.

The fixed-header field is named `timestamp`. It stores the creation time as signed 64-bit Unix epoch milliseconds in
UTC; zero means unknown. Every native Save writes the instant at which its new atomic file image was created.
Recovery/autosave uses the capture time of its embedded native document. The value must be non-negative, and its range
avoids the 32-bit Unix and NTP rollover limits.

This timestamp intentionally makes separately timed top-level saves byte-different. Content comparison uses decoded Level
and application-section semantics or the expanded SHA-256 carried by each semantic section descriptor. The recovery
`documentSha256` instead binds the exact timestamped native bytes. Nested Levels used by history and canonical
required-Level digests always encode creation time as zero, preserving fragment reuse and semantic digest stability.

Readers accept only the exact four-byte native magic followed by version 1, 2, 3, or 4, and require every section
descriptor to match that native version. Import formats are selected through their own explicit readers, never by
weakening the native SED parser.

All wire framing and semantic section identifiers are lowercase, including the section stream nested in recovery and
autosave `docm` and the framing around protected metadata. Documented JSON/CBOR property names are typed schema fields,
not section identifiers; their exact spelling is part of the corresponding V1 application schema. Native versions do not
renumber those application payloads: `edit`, its nested `window.uvState`, and `hist` retain their documented internal
schema version `1` and V1-named encoding/fragment-layout values.

## Fixed header

The canonical V1-V4 fixed header is 28 bytes:

| Offset | Size | Type | Meaning |
|---:|---:|---|---|
| 0 | 4 | bytes | lowercase ASCII `sed` followed by NUL (`sed\0`) |
| 4 | 4 | `u32` | native file version: V1/V4 for `TexturePixels`, V2/V3 for `Normalized`; V3/V4 also carry ARCHLIGHTING |
| 8 | 4 | `i32` | `ProjectType`: JKDF2, MOTS, or IJIM |
| 12 | 4 | `u32` | number of following sections |
| 16 | 8 | `i64` | `timestamp`: creation time as UTC Unix epoch milliseconds; `0` means unknown |
| 24 | 4 | `u32` | `checksum`: CRC32C over bytes `[0, 24)` |

The project type is duplicated in `head`; strict loading requires the copies to agree.

Immediately before the footer, every version stores this 40-byte checked mirror:

| Relative offset | Size | Type | Meaning |
|---:|---:|---|---|
| 0 | 4 | bytes | lowercase ASCII `mhdr` |
| 4 | 4 | `u32` | mirrored header size, exactly `28` |
| 8 | 28 | bytes | exact fixed-header bytes from offset 0 |
| 36 | 4 | `u32` | CRC32C of the 28 mirrored bytes |

The schema field is named `checksum`; CRC32C is its normative algorithm, not part of the field name.

The primary and mirrored copies make fixed-header damage detectable and repairable. The writer always emits both. The
loader uses the mirror when the primary header is damaged and rejects two valid copies that disagree. If the primary
header remains valid, strict loading still requires the canonical trailing mirror and footer. Corruption recovery may
probe the expected trailing region for a checked `mhdr` mirror even when `sedend\r\n` was truncated or bit-flipped;
such trailing-framing damage is reported and need not discard otherwise checked semantic sections.
The footer never causes the preceding bytes to be reserved or stripped as a header mirror unless the complete
`mhdr` structure, embedded header, and mirror CRC validate.

The corruption-recovery entry point can also start at a checked section stream after the primary header is lost. When
the original checked trailing mirror/footer survives, recovery copies that mirrored 28-byte header verbatim back to the
front, preserving its project type and section count. When no valid mirror survives, recovery counts the checked section
chain and synthesizes a self-consistent primary/mirror/footer pair with a temporary JKDF2 project value; a surviving
`head` then supplies the authoritative project type in this synthesized-framing path. If `head` is also lost, the
temporary value remains only an in-memory decoder placeholder: `SedRecoveryLoadResult::projectType` is empty and
`projectTypeTrusted` is false. Callers must not label, configure resources for, decode project-dependent history for, or
export that Level as JKDF2. A valid original header or mirror without `head` is trusted; a surviving `head` without either
header is trusted; two surviving checked project identities that disagree are always a hard failure.

## Primitive encodings

- `u8`, `u32`, `i32`, `u64`, and `i64` are fixed-width little-endian integers.
- `sedIndex` is a sentinel-capable Level index or resource reference encoded as canonical unsigned LEB128, bounded to
  five bytes. Wire value `0` means the sole empty/not-set index `-1`; a non-negative index `n` uses wire value `n + 1`.
  Thus `-1` encodes as `00`, `0` as `01`, `126` as `7f`, and `127` as `80 01`. The sentinel and indices `0..126` all
  remain one byte. The bias is important: directly converting the signed `-1` sentinel to unsigned would produce
  `UINT32_MAX`, whose ULEB128 representation consumes five bytes for every empty reference. The 32-bit wire mapping has
  2^32 codes: one sentinel plus 2^32-1 actual indices (`0..UINT32_MAX-1`). The current Level model is signed, so its
  reader rejects decoded indices above `INT32_MAX`; the writer also rejects values below `-1`. This is denser for the
  actual sentinel-or-nonnegative-index domain than ZigZag, which would spend half its code space on forbidden negative
  indices.
- `bool` is one byte and must be `0` or `1`.
- `f32` and `f64` store exact IEEE-754 bits in little-endian order. Every native writer and reader require every authored
  value to be finite; finite signed zero retains its exact sign bit.
- `string` is `u32 byteCount` followed by exactly that many bytes. It is not NUL-terminated.
- `vector<T>` is `u32 itemCount` followed by packed `T` records.
- `ColorF` is:

  ```text
  f32 r
  f32 g
  f32 b
  f32 a
  ```

- `TVector` is:

  ```text
  f64 x
  f64 y
  f64 z
  ```

- `TBox` is:

  ```text
  f64 x1
  f64 y1
  f64 z1
  f64 x2
  f64 y2
  f64 z2
  ```

- Resource/name spellings are held by `rsrc`. A typed parameter's value catalog is determined by its preceding
  `adjType` or `cogType`; an unknown/untyped value uses the generic `names` catalog.
- `Value(kind, integerEncoding)` is:

  ```text
  sedIndex names[name]
  i32 valueType
  integerEncoding intValue
  f64 floatValue
  sedIndex catalog[kind][stringValue]
  ```

- `TplValue` is:

  ```text
  i32 adjType
  Value(resourceKindFor(adjType), i32) value
  ```

- `CogValue` is:

  ```text
  i32 cogType
  Value(resourceKindFor(cogType), entityIndexOrI32) value
  bool local
  sedIndex names[description]
  i32 mask
  ```

`entityIndexOrI32` is `sedIndex` only when `cogType` is Sector, Surface, or Thing and `valueType` is `ptr`; those
`intValue` fields are actual Level references. Every other parameter integer remains exact fixed-width `i32`, because
it may be an authored value, bit field, or unknown payload rather than an index.

All native checks named CRC32C use reflected Castagnoli polynomial `0x82f63b78`, initial value `0xffffffff`, and final
bitwise complement; the standard `123456789` check vector is `0xe3069283`. Hardware SSE4.2 dispatch and the portable
table implementation produce the same bytes. This is distinct from the IEEE CRC-32 retained by the outer recovery
container's `meta`/`warn` machinery.
CRC32C runtime-dispatches SSE4.2 on supported x86 processors; expanded-section SHA-256 is currently the portable scalar
implementation. Changing that digest algorithm would change the native wire contract, so any future acceleration must keep
the same SHA-256 bytes and retain a verified fallback.

## Section framing and descriptor mirror

Every section starts with this checked 100-byte primary descriptor:

| Relative offset | Size | Type | Meaning |
|---:|---:|---|---|
| 0 | 8 | bytes | `sectv1\n\0`, the section/resynchronization anchor |
| 8 | 4 | bytes | lowercase ASCII section type (`a`-`z`, `0`-`9`) |
| 12 | 4 | `u32` | section schema/native version; must equal the fixed-header version (`1`, `2`, `3`, or `4`) |
| 16 | 4 | `u32` | flags; bit 0 means compressed, bit 1 means striped parity protected |
| 20 | 8 | `u64` | expanded payload size |
| 28 | 8 | `u64` | physical stored size, including alignment and coded slots |
| 36 | 8 | `u64` | logical stored size before parity/alignment |
| 44 | 4 | `u32` | systematic data-stream count `k` |
| 48 | 4 | `u32` | parity-stream count `p` |
| 52 | 4 | `u32` | bytes of coded data per slot, exactly `4092` when protected |
| 56 | 4 | `u32` | stripe count |
| 60 | 4 | `u32` | zero-padding bytes before the first coded slot |
| 64 | 32 | bytes | SHA-256 of the expanded semantic payload |
| 96 | 4 | `u32` | CRC32C of descriptor bytes `[0, 96)` |

Exactly `storedSize` payload bytes follow. Every section then ends with this checked 116-byte trailer:

| Relative offset | Size | Type | Meaning |
|---:|---:|---|---|
| 0 | 8 | bytes | `sedmir1\n` |
| 8 | 4 | `u32` | mirrored descriptor size, exactly `100` |
| 12 | 100 | bytes | exact primary descriptor, including its CRC32C |
| 112 | 4 | `u32` | CRC32C of trailer bytes `[0, 112)` |

For a protected top-level section, `1 <= k <= 16`, `p` is `2` or `4`, `chunkPayloadSize = 4092`,
`stripeCount >= 1`, `codedOffset < 4096`, and
`storedSize = codedOffset + (k + p) * stripeCount * 4096`. `logicalStoredSize` may not exceed the capacity
`k * stripeCount * 4092`. In the explicit nested-history profile, both protection and compression flags are clear,
`rawSize = storedSize = logicalStoredSize`, and all five layout fields from `dataStreams` through `codedOffset` are zero.

The primary descriptor supplies the normal read path. If its anchor, fields, or CRC32C is damaged, a valid checked
trailer can supply its exact framing. Strict native loading requires the complete checked trailer for every section and
rejects a missing or damaged trailer even when the primary and payload validate. The corruption-recovery entry point may retain
that complete payload from its valid primary descriptor, but reports the framing damage. If both
copies validate but disagree, strict loading rejects the ambiguity and corruption recovery discards the ambiguous unit;
it never chooses one valid copy arbitrarily. If neither copy validates, strict loading rejects the document and recovery
resynchronizes only at checked framing: either another fully checked `sectv1\n\0` descriptor with its matching trailer,
or a fully checked `sedmir1\n` trailer whose embedded descriptor CRC/schema, stored-size boundary, and absolute 4 KiB
FEC phase derive one exact preceding section start. The mirror-derived path matters when one localized overwrite damages
the previous trailer and overlaps the following primary descriptor. It never trusts an unchecked size or a marker alone
to locate the next unit.

The format requires exactly one each of `head`, `rsrc`, `sect`, `thng`, `tmpl`, `cogs`, `lite`, and `hnod`. `jimp` is
an optional core-owned section; `edit` and `hist` are structurally optional application sections. Normal complete-document
Save attempts to write represented `jimp` plus both application sections. A manual or crash-recovery writer may deliberately
publish Level+`edit` without `hist` only when history encoding, size validation, or bounded canonical-envelope
verification fails; manual Save emits a Messages warning, while recovery also records the degradation in `warn`. Other
optional four-character lowercase ASCII letter/digit types may not collide with known or one another. Unknown optional
types are fully checked and returned opaquely so `sed_core` does not depend on Qt.

### Exact protection boundaries

Protection is deliberately layered rather than whole-file. The boundaries are:

| Bytes/domain | Protection and recovery boundary |
|---|---|
| primary 28-byte fixed header | `checksum` is CRC32C over bytes `[0,24)`; recoverable only from the independently checked 40-byte trailing header mirror, not from section MDS |
| 100-byte primary section descriptor | CRC32C over bytes `[0,96)`; recoverable from that section's independently checked 116-byte trailer, not from payload MDS |
| `codedOffset` alignment padding | occupies `storedSize`, carries no semantic bytes, and is excluded from slot CRC32C and expanded SHA-256; canonical bytes are all zero, strict loading rejects nonzero padding, and corruption recovery normalizes/reports it |
| logical raw/compressed section bytes | mapped into systematic 4092-byte slot payloads; zero fill to slot/stream capacity is inside those slot payloads and is canonical (strict rejects nonzero tail bytes; recovery normalizes/reports them) |
| each 4096-byte protected slot | 4092 coded bytes plus its own trailing CRC32C; an invalid CRC identifies one erasure in that stripe |
| each top-level semantic section | its own MDS code only: `p=4` for a required Level section, core-owned `jimp`, or `edit`; `p=2` for `hist` and future lower-priority optional sections; no section can spend another section's parity |
| expanded semantic bytes | one descriptor SHA-256 after optional decompression; mismatch detects but cannot localize corruption, so recovery drops/defaults the whole section |
| 116-byte descriptor trailer | its own CRC32C over bytes `[0,112)` and an embedded copy of the already-CRC-checked primary descriptor |
| 40-byte fixed-header mirror | CRC32C of its embedded 28-byte header |
| eight-byte `sedend\r\n` footer | marker only; no checksum, parity, or whole-file digest |
| nested Levels inside `hist` | canonical `HistoryNested` headers/descriptors/mirrors and payload CRC32C remain, but per-state SHA-256, section compression, and MDS are disabled; the enclosing top-level `hist` section owns expanded SHA-256, compression, and `p=2` once |

Consequently header/descriptor redundancy repairs framing, slot CRC32C plus MDS repairs localized payload erasures, and
expanded SHA-256 validates the reconstructed semantic unit. None is a whole-file checksum, and no layer protects bytes
outside its row above.

Native SED versions intentionally have no central descriptor directory and no on-disk record-group shard table within a
semantic section. The writer may use validated record boundaries for HIST pooling (described below), but each required or optional domain
is still one descriptor/payload/trailer recovery unit; checked descriptor mirrors and later
`sectv1\n\0` anchors provide resynchronization. If any stripe exceeds that section's parity tier, the expanded semantic
section is omitted/defaulted as a whole even if some of its internal records happened to survive. Partial-Level salvage
therefore contains only independently healthy semantic units plus explicit defaults, never record fragments recovered
from inside an over-budget unit. A future record-group/directory design would require a new version or schema.

## Per-section integrity and striped MDS protection

### Slot and stripe layout

The bytes selected for storage—raw or compressed—are the logical stored bytes. A protected section maps them into fixed
4096-byte slots:

```text
4092 coded payload bytes
u32 CRC32C of those 4092 bytes
```

The first slot is aligned to an absolute 4096-byte file offset. `codedOffset` records the zero padding between the
descriptor and that slot, and `storedSize` includes both the padding and every slot. Native SED versions have no inner FEC
table: all layout values and the one expanded SHA-256 live in the mirrored section descriptor, while each slot carries
its own trailing CRC32C. Alignment padding carries no semantic bytes and is not hashed, but its canonical representation
is zero: strict loading rejects nonzero padding, while corruption recovery reports and normalizes it in memory.

Let `C = max(1, ceil(logicalStoredSize / 4092))`. The encoder chooses
`stripeCount = ceil(C / 16)` and `k = ceil(C / stripeCount)`, so each stripe has between one and 16 systematic data
streams. The physical and logical order is stream-major: all stripes of data stream 0, then all stripes of data stream
1, and so on, followed by the parity streams in the same order. Unused bytes and slots are canonical zero padding:
strict native loading rejects nonzero tail padding. Corruption recovery reports it and either normalizes it when the
meaningful prefix still matches the canonical slot CRC, or treats the complete checked slot as an erasure when a recomputed CRC
authenticates the noncanonical bytes; the latter lets MDS reconstruct a simultaneously changed semantic prefix instead
of waiting for the section SHA-256 to fail.

### Systematic Vandermonde MDS parity algorithm

Native SED versions use a **systematic Reed–Solomon-style Vandermonde MDS erasure code** for each independent stripe. The
encoder constructs the code over GF(256) using primitive
polynomial `0x11d`. It multiplies the Vandermonde matrix by the inverse of its top `k` rows, making the first `k` rows the
identity while retaining the MDS property. Precisely, `V[row][column] = row^column` in GF(256), for row bases beginning
at zero and columns `0..k-1` (`x^0 = 1`, including for zero); `G = V * inverse(V[0..k-1])`. Data rows are `G[0..k-1]`
and the stored parity rows are `G[k..k+p-1]`. The parity tier is:

- `p = 4` for required Level sections `head`, `rsrc`, `sect`, `thng`, `tmpl`, `cogs`, `lite`, and `hnod`;
- `p = 4` for core-owned `jimp` and `edit`; `edit` receives the core tier because it can hold the user's only unsaved COG source;
- `p = 2` for `hist` and future lower-priority optional sections.

This tiering gives the authoritative core Level and irreplaceable unsaved editor/COG state twice the per-stripe parity
budget of undo/redo history. Even a tiny required Level domain remains its own one-stripe section instead of being
coalesced with another domain: the fixed-slot overhead buys independent salvage and failure isolation. This is separate
from the recovery envelope's tiny association fields, which must agree atomically and are intentionally grouped in its
single protected `meta` payload.

#### Why this algorithm was chosen

Native sections are variable-size semantic domains, and the core Level plus unsaved COG-bearing `edit` state need to
survive more than one damaged slot without storing a second document. A systematic Reed–Solomon-style Vandermonde code
provides the MDS guarantee: `p` parity streams repair any `p` CRC-localized erasures in a stripe with the minimum possible
`p`-stream parity overhead. The systematic generator keeps the first `k` rows as unchanged data, GF(256) maps directly
to bytes, and the normalized Vandermonde construction supplies one deterministic codec for every adaptive `k=1..16`
stripe and both parity tiers. An XOR-only scheme repairs only one arbitrary erasure, while replication costs a complete
second copy and still lacks the tunable `p=2`/`p=4` policy. CRC32C identifies erased slots before decoding and the
expanded SHA-256 validates the reconstructed semantic section. The codec is implemented in
[sed.cpp](../../src/core/src/assets/sed.cpp).

`hist` stays at `p=2` by design. It is an optional, reconstructible undo/redo timeline; if one HIST stripe exceeds two
erasures, the complete history section can be dropped while the current Level and `edit` remain usable. Raising HIST to
`p=4` would add parity bytes and normal encode/decode work to a payload that is not required to open the document. The
authoritative Level and the editor section (which may contain the only unsaved COG source) retain `p=4`.

### Repair guarantees and corruption limits

CRC32C marks bad slots as erasures. Any combination of at most `p` erased data or parity slots in each stripe is
recoverable; damage spread across many stripes remains repairable when every stripe stays within its own budget. More
than `p` erasures in any one stripe makes the entire semantic section unrepairable. Protection is independent between
sections, so a failure in one section does not consume another section's budget. Recovery reconstructs missing data only
in memory and reports every invalid checked slot, including parity-only damage; loading never rewrites or "scrubs" the
source file. After reconstructing or accepting the systematic data, the reader recomputes every CRC-valid parity row and
reports a mismatch as physical corruption. This diagnostic catches a wrong parity payload whose CRC32C was also
recomputed, without consuming repair budget or discarding data that still passes the expanded SHA-256.

After data reconstruction and optional decompression, the descriptor's SHA-256 verifies the complete expanded section.
It catches CRC-consistent corruption that reaches different expanded bytes, but it cannot identify which apparently
CRC-valid slot was wrong. Such damage therefore cannot be converted into a localized erasure; strict loading rejects it
and corruption recovery can only omit/default that semantic section as a whole.

The exact repair ceiling is four bad slots in one stripe of a required Level section, `jimp`, or `edit`, and two in one `hist` or
future lower-priority optional stripe. More than four or two respectively makes that entire semantic section
unrepairable, even if every other stripe and section is healthy.

The physical corruption model preserves the absolute 4096-byte slot phase established by checked framing. Within that
phase, CRC-localized byte/bit substitutions and persistently unreadable ranges inside a known original file extent become
erasures that each stripe can repair up to its `p` budget. An ordinary shorter file does not preserve the removed slot
count or later phase and is not generally convertible into erasures; only narrowly recognizable redundant trailing
framing has a safe special-case recovery. A byte insertion or deletion in the middle of a section likewise shifts every
later slot boundary and is outside all native versions' repair model. Recovery does not guess a new phase or slide slot
boundaries heuristically.

## Compression

Compression is lossless but intentionally optimized for editor responsiveness, bounded memory, and independent section
recovery rather than maximum possible compression. The fixed header, section descriptors/mirrors, and footer are not
compressed.
Each payload whose raw size is in the inclusive range 4096 bytes through 64 MiB is independently considered exactly once
for compression with the shared fast LZSS block codec. For non-`hist` sections, a payload smaller than 4096 bytes or
larger than 64 MiB bypasses the compressor and remains raw. `hist` has a stricter expanded-payload limit of 64 MiB and is
rejected rather than stored raw when it exceeds that limit. Within the eligible range, compressed bytes are retained
only when strictly smaller; otherwise raw bytes are stored. The upper bound caps transient encoder memory. Compression
precedes parity framing. Because output growth is monotonic, an incompressible candidate is abandoned immediately before
its next token would reach the raw size; the losing near-limit buffer is therefore released before protected-slot/FEC
allocation begins.
The decoder independently rechecks the 64 MiB expansion ceiling before it
allocates output, then reserves the already validated exact expanded size so a
valid stream does not repeatedly grow and copy its destination buffer.

The deterministic encoder uses a 65,536-entry newest-position table plus a 65,536-entry predecessor ring on the heap
(512 KiB total). At each position it examines at most four same-hash candidates. The newest candidate receives a full
comparison; older candidates share a fixed 128-byte comparison budget, and equal-length ties keep the nearest offset.
This preserves the shared native token stream contract while preventing repeated-prefix input from turning match search
into unbounded work. On the reference `test.sed`, the bounded four-candidate matcher reduced a canonical re-save from
9,920,672 to 9,048,224 bytes (8.79%) with exact decoded semantic equality.

LZSS token format:

- token `0..127`: a literal run of `token + 1` following bytes;
- token `128..255`: a match of `(token & 0x7f) + 4` bytes, followed by a 16-bit little-endian backward offset;
- valid match lengths are 4 through 131 bytes and offsets are 1 through 65,535 bytes.

Thus eligible semantic sections are considered independently and no section is expanded merely to claim it is
compressed. `hist` often compresses substantially because adjacent states are similar; already-compressed, very small,
and larger-than-64-MiB non-`hist` payloads remain raw, while an oversized `hist` is rejected by its type-specific limit.

## Required Level sections

All field names below appear in serialized order.

### `head`

```text
i32 projectType
i32 levelHeaderVersion
f64 gravity
f64 horizon.distance
f64 horizon.pixelsPerRev
f64 horizon.offsetX
f64 horizon.offsetY
f64 ceiling.height
f64 ceiling.offsetX
f64 ceiling.offsetY
f64 mipmapDistances[4]
f64 lodDistances[4]
f64 perspectiveDistance
f64 gouraudDistance
bool fog.enabled
ColorF fog.color
f64 fog.start
f64 fog.end
u32 extraBuffers[12]
sedIndex colormaps[masterCMP]
f64 ppunit
string layerVisibility
i32 curId
```

The `extraBuffers` indices, in serialized order, are `sounds`, `materials`, `aiClasses`, `models`, `sprites`,
`keyframes`, `puppets`, `soundClasses`, `cogScripts`, `cogs`, `templates`, and `things`. These are authored `u32`
allowances, preserved independently of engine-export compatibility. In particular, native SED retains an IJIM
Materials Extra outside 0 or 64; the NDY writer rejects it until the user repairs that field in Extra Buffers.
This runtime restriction does not change native field encoding or silently normalize imported values.

`head` remains the first semantic section so checked project/framing recovery does not depend on `rsrc`. Its small
payload is decoded after `rsrc` during a normal load solely to resolve `masterCMP`; if `rsrc` is unrecoverable, partial
recovery retains the rest of `head` and leaves that unresolved spelling empty.

### `rsrc`

`rsrc` begins with 14 category-local `vector<string>` catalogs in this fixed order. Every catalog contains unique,
nonempty names in first semantic-use order and owns an independent zero-based index range:

| Category ID | Catalog | Typed uses |
|---:|---|---|
| 0 | `names` | generic parameter names/values, descriptions, instance and hierarchy-node names |
| 1 | `colormaps` | CMP names |
| 2 | `materials` | MAT names |
| 3 | `sounds` | WAV names |
| 4 | `aiClasses` | AI class names |
| 5 | `models3do` | 3DO names |
| 6 | `sprites` | SPR names |
| 7 | `keyframes` | KEY names |
| 8 | `puppets` | PUP names |
| 9 | `soundClasses` | SND/sound-class names |
| 10 | `cogScripts` | COG script names |
| 11 | `templates` | Thing-template names |
| 12 | `particles` | PAR names |
| 13 | `layers` | semantic layer names |

The catalogs are followed by three ordered inventories:

```text
vector<sedIndex colormaps> level.colormaps
vector<sedIndex materials> level.materials
vector<sedIndex layers>    level.layers
```

Unlike catalogs, inventories retain order and duplicates. Fixed-schema fields know their category and store only one
`sedIndex`; for example a Surface material is an index into catalog 2, never a `(category,index)` pair. Known template and
COG parameter types similarly select the category from `AdjType`/`CogType` before reading the following index. Unknown
parameter names and arbitrary values such as `unknown_param=some_string` use catalog 0 and are preserved losslessly.
This fixed two-dimensional layout permits direct `catalogs[category][index]` lookup and lets a future `rsrc` schema add
category-specific embedded payload records after the names without mixing resource types.

The independent all-category conformance Level also measures this representation without calling the production
`ResourceIndex`. Across 91 resource/name occurrences and 54 unique nonempty spellings, repeated inline
`u32 length + bytes` strings would occupy 1,366 bytes. Categorized catalogs with fixed `u32` references occupy 1,304
bytes; the actual catalogs plus sentinel-biased ULEB128 references occupy 1,031 bytes. The complete design therefore
saves 335 bytes (24.52%) in the affected semantic fields for this fixture. Compact indices account for 273 of those
bytes: the 91 one-byte references occupy 91 bytes rather than 364 bytes, a 75% reference-byte reduction and a 20.94%
reduction relative to the same catalogs with fixed references. Common collection-count fields are excluded from all
three totals. These are pre-compression semantic measurements; final protected-file savings can be quantized by the
4 KiB slot/parity layout, especially for a tiny fixture.

### `sect`

The payload is `vector<Sector>`. Each Sector is:

```text
i32 id
sedIndex num
i32 flags
ColorF ambient
ColorF extraLight
ColorF pointLight.color
TVector pointLight.position
f64 pointLight.minRange
f64 pointLight.maxRange
sedIndex colormaps[colormap]
ColorF tint
sedIndex sounds[sound]
f64 soundVolume
TVector thrust
TBox collideBox
sedIndex layer
i32 mark
vector<Vertex> vertices
vector<Surface> surfaces
```

`Vertex` is:

```text
f64 x
f64 y
f64 z
i32 mark
```

`Surface` is:

```text
sedIndex num
i32 mark
i32 d3dId
i32 surfaceFlags
u32 faceFlags
i32 geo
i32 light
i32 tex
ColorF extraLight
i32 adjoinFlags
bool hasAdjoin
if hasAdjoin: sedIndex adjoinSector
if hasAdjoin: sedIndex adjoinSurface
sedIndex materials[material]
sedIndex nAdj
sedIndex nMat
f32 uScale
f32 vScale
TVector normal
vector<sedIndex> vertexIndices
vector<TexVertex> textureVertices
```

`TexVertex` is:

```text
f32 u
f32 v
ColorF color
```

The native file version supplies the units for both pairs of fields without adding a second per-Surface tag:

| Native version | `TexVertex.u/v` | `Surface::uScale`/`vScale` | Decoded `Level::uvCoordinateSpace` |
|---:|---|---|---|
| V1 | texture pixels | texture pixels | `TexturePixels` |
| V2 | normalized texture space | normalized texture space | `Normalized` |
| V3 | normalized texture space | normalized texture space | `Normalized` |
| V4 | texture pixels | texture pixels | `TexturePixels` |

The values are stored bit-exactly in the existing `f32` fields. V2/V3 do not clamp signed or multi-tile coordinates and
do not otherwise alter the `sect` payload. V1/V4 loading deliberately preserves pixel units rather than guessing MAT
dimensions; V2/V3 loading publishes normalized units directly. Conversely, native Save emits V2/V3 for a `Normalized`
Level and retains V1/V4 only when the whole Level is explicitly tagged `TexturePixels`.

`sect` intentionally names the sector collection, not the complete Level.
`head`, `rsrc`, `thng`, `tmpl`, `cogs`, `lite`, and `hnod` remain independent
required sections, so calling this unit `levl` would incorrectly imply that it
contained those domains. Calling it geometry would also be incomplete: every
Sector carries ambient/extra light, point-light, sound, thrust, flags, layer,
colormap, tint, and collision-box state in addition to owned geometry.

The expanded binary payload is lossless rather than normalized for minimum raw size. It retains sector-local vertices
and complete per-surface-corner `TexVertex` records (including RGBA), whereas JKL/NDY text can reference shared world
tables. Repeated spellings are removed through `rsrc`, and true indices use compact biased ULEB128 varints; the remaining
exact floating-point/color records still dominate dense geometry. The top-level section is compressed once before its strong
four-parity-stream protection is added; no second nested geometry copy is stored.

### `thng`

The payload begins with `vector<Thing>`. V1/V2 records are:

```text
i32 id
sedIndex num
sedIndex sector
sedIndex templates[templateName]
sedIndex names[instanceName]
f64 x
f64 y
f64 z
f64 pitch
f64 yaw
f64 roll
vector<TplValue> values
sedIndex layer
TBox boundingBox
```

V3/V4 insert `sedIndex archLightingIndex` immediately after `sedIndex sector` and append this table after the Thing
vector:

```text
vector<MotsArchLightObject>
  i32 wireIndex
  vector<MotsArchLightMesh>
    i32 wireIndex
    vector<MotsArchLightVertex>
      f32 mono
      f32 red
      f32 green
      f32 blue
```

The reference is `-1` when the Thing has no architectural-light object. A MotS Arch Thing (effective Thing flag `thingflags & 0x8`)
may use a virtual all-zero default while its in-memory reference is `-1`; JKL export materializes a model-sized zero record
unless the explicit omit-default option is selected. The native SED writer keeps bound active records, including all-zero
records, and nonzero authored records while their Arch flag is clear. It drops zero-valued records only when they are
unreferenced or the saved instance/Level-local template explicitly proves bit `0x8` is off; if only the external template
database can resolve the flag, it preserves the reference conservatively. Filtering clears only omitted references in the
serialized snapshot, leaving the editor Level unchanged. The table is valid only for
MotS, keeps object and mesh order separately from their wire labels, and retains all finite channel bits including signed
zero. V1/V2 readers have no table field and initialize every reference to `-1`.

When `Level::motsArchLightingBakeBaselines` is non-empty, V3/V4 append exactly one bake-provenance trailer after the
table. A file without provenance ends immediately after the table. The trailer is part of the `thng` semantic payload
and receives that section's normal compression, hash, slot checks, and parity protection:

```text
byte[4] magic = "absl"
u32 trailerVersion = 1
u32 payloadBytes
byte[payloadBytes] payload
  u32 baselineCount
  repeat baselineCount:
    i32 thingId
    string modelName
    i32 originalArchLightingIndex
    bool hasOriginalArchLighting
    if hasOriginalArchLighting:
      i32 wireIndex
      vector<MotsArchLightMesh>
        i32 wireIndex
        vector<MotsArchLightVertex>
          f32 mono
          f32 red
          f32 green
          f32 blue
```

`string` and `bool` use the primitive encodings defined above; the baseline model name is inline rather than a resource
catalog reference. Baselines retain the authored pre-bake record or the original unbound state, so repeatable baking
can restore authored values. `thingId` identifies the original owner, but need not resolve to a currently live Thing;
`originalArchLightingIndex` is provenance, not a reference into the current filtered table. It must be at least `-1`,
with `-1` if and only if the optional original object is absent. Baseline Thing IDs are unique, model names are non-empty
and at most 16 MiB, object/mesh wire labels are nonnegative, mesh labels are unique within each object, and channels
are finite. Baseline count and cumulative baseline mesh and vertex counts are each bounded to 1,000,000, separately
from the active table's mesh/vertex budgets and within the general decoded-Level storage/item limits.

Readers accept only the exact `absl` magic and trailer version `1` in V3/V4. The payload length is bounded to 128 MiB
and must consume exactly the declared bytes, with no bytes remaining after the trailer. Unknown magic, unsupported
trailer version, truncated or excess payload, extra trailing data, and a trailer in V1/V2 are errors. Absence of the
trailer restores an empty baseline collection; the writer omits an empty collection. This explicitly specified trailer
is existing V3/V4 behavior, not an open-ended trailing-byte extension mechanism.

### `tmpl`

The payload is `vector<ThingTemplate>`; each record is:

```text
sedIndex templates[name]
sedIndex templates[parent]
vector<TplValue> values
sedIndex names[description]
TBox boundingBox
```

### `cogs`

The payload is `vector<Cog>`; each record is:

```text
i32 id
sedIndex num
sedIndex cogScripts[name]
vector<CogValue> values
```

### `lite`

The payload is `vector<SedLight>`; each record is:

```text
i32 id
i32 flags
i32 mark
sedIndex layer
ColorF color
f64 intensity
f64 range
TVector position
```

### `hnod`

The payload is `vector<HNode>`; each record is:

```text
i32 id
sedIndex num
u32 type
sedIndex mesh
sedIndex parent
TVector position
TVector rotation
TVector pivot
sedIndex names[name]
```

### `jimp`

`jimp` is a known optional core-owned section protected with `p=4` in `ProtectedV1`. Its logical bytes are little-endian:

```text
u32 version = 1
u8 importedJklCogRowsPresent  // canonical 0 or 1
u32 referenceCount
repeat referenceCount:
  i32 sector
  i32 surface
```

Pairs preserve imported source order. For represented metadata, each reference must resolve against the decoded Level.
The section is meaningful when `importedJklCogRowsPresent` is true, or when the reference list is nonempty and raw COG `paramN` rows of
`CogType::unk` still need that imported ordering. False-marker unused maps normalize to empty and are not emitted. Thus
ordinary files without represented pending provenance retain their established bytes. A malformed version, Boolean, count,
out-of-range represented reference, or trailing payload is a strict-load error. Corruption recovery may treat an invalid or unrepairable `jimp`
as lost optional provenance: it clears only the marker/map, keeps the independently verified Level, and reports `jimp`
loss. This extension does not bump native V1–V4. It is never included in `HistoryNested`; the app's optional
`jklCogImportV1` HIST field stores history-state provenance separately.

## Extensibility

The fixed header supplies a checked section count and every lowercase top-level section carries the containing native
version. V1 through V4 descriptors carry the matching version `1` through `4` and must agree with the fixed header.
A future independent semantic domain should therefore use a new optional four-byte section type. Adding or reordering
fields inside an existing domain such as `sect` is not an extension point inside any supported version: it requires a
new section schema or file version. The core reader returns fully validated unknown optional sections opaquely, but the
current Sith Editor Save path preserves only sections for which an application owner is registered; merely loading an unknown section does
not promise pass-through on the next Save.

The V1 and V2 Level-section payload layouts are identical; V2 deliberately versions the UV/scale semantics described
above. V3 and V4 retain those layouts for every section except the documented `thng` arch-lighting table and optional
`absl` bake-provenance trailer. Readers require that trailer's exact marker, version, lengths, and record schema; they do
not infer unknown fields from trailing bytes or accept alternate native record layouts. The `edit` and `hist` payloads are
application-owned schemas embedded inside those native sections: their root `version` remains `1` in all native file
versions, as does the nested UV-workspace version and every documented V1 history encoding/layout name.

## Conformance vectors

The checked-in byte-level V1 pixel-space goldens and their regeneration rules are documented in the
[SED vector manifest](../../src/core/tests/assets/fixtures/sed_vectors/README.md). Each project has an input, protected
native document, normal expected export, deliberately
unrecoverable native document, and partial expected export:

| Project | Input | Native | Normal export | Beyond-p4 `sect` damage | Partial export |
|---|---|---|---|---|---|
| JKDF2 | `jkdf2_input.jkl` | `jkdf2_native.sed` | `jkdf2_expected.jkl` | `jkdf2_sect_unrecoverable.sed` | `jkdf2_expected_partial.jkl` |
| MotS | `mots_input.jkl` | `mots_native.sed` | `mots_expected.jkl` | `mots_sect_unrecoverable.sed` | `mots_expected_partial.jkl` |
| IJIM | `ijim_input.ndy` | `ijim_native.sed` | `ijim_expected.ndy` | `ijim_sect_unrecoverable.sed` | `ijim_expected_partial.ndy` |

Ordinary tests never rewrite a golden. They assert both directions independently for the V1 compatibility path:

```text
input --legacy reader--> Level --SED writer--> bytes == *_native.sed
                                               |
*_native.sed --strict SED reader--> Level --legacy writer--> *_expected.{jkl,ndy}

*_native.sed --damage five sect slots--> bytes == *_sect_unrecoverable.sed
*_sect_unrecoverable.sed --partial recovery--> surviving Level
surviving Level --legacy writer--> *_expected_partial.{jkl,ndy}
```

The normal path checks the generated protected SED bytes exactly, opens the independent checked-in SED, compares the
complete semantic Level, and byte-compares its JKL/NDY export with the checked-in text. A dynamic four-erasure case must
repair to that same complete export. The checked-in five-erasure document must be reproduced exactly, report `sect` as
lost, retain independently checked Things/templates/unknown values, and byte-match the checked-in partial export. This
prevents a symmetric writer/expected-writer regression from blessing its own output.

`all_resource_categories.sed` is the independent deterministic resource vector. It populates all 14 fixed catalogs
through direct Level fields, ordered inventories, Thing/template parameters, and COG parameters. Tests compare freshly
written protected bytes with the checked-in file, strictly open that file, verify full semantic equality, and inspect
unprotected raw `rsrc`, `thng`, and `cogs` payloads for exact catalog order and category-local `sedIndex` values. This
includes MAT, CMP, WAV, AI, 3DO, SPR, KEY, PUP, SND, COG, template, particle, layer, and generic/unknown names; the
one-byte `-1` sentinel and one-byte small indices are checked explicitly.
An independent traversal also locks the corresponding resource-size measurement to 91 occurrences, 54 unique strings,
1,366 inline-string bytes, 1,304 catalog-plus-fixed-index bytes, and 1,031 actual catalog-plus-ULEB128 bytes.

The application suite then feeds every project-native vector and `all_resource_categories.sed` through the production
autosave/recovery serializer. Reopening the resulting `.sedrecovery` must reproduce the same Level and normal expected
text. Damaging the enclosed `docm` `sect` beyond parity must reproduce the same partial expected text. Checked-in native
vectors use zero document creation time; the outer recovery container is generated during testing because its capture/source times and
session/source association are inputs. `docm` is the normative native representation and the checked-in Level/export
vectors remain stable.

Dedicated non-golden coverage constructs a `Normalized` Level with signed and multi-tile UV coordinates, verifies that
Save emits native V2 without a table, and requires exact coordinates, scales, coordinate-space tag, and canonical bytes
after Open and re-Save. A paired MotS Level with a non-empty arch-lighting table verifies V3, and its explicitly
`TexturePixels` counterpart verifies V4. A table-free `TexturePixels` Level verifies V1 and its bit-exact legacy round
trip. These tests keep the checked-in V1 corruption vectors stable while covering all native version distinctions.

## Application sections

### `edit`

#### Workspace schema and validation

`edit` is RFC 8949 CBOR. The root is a map with editor-state `version` exactly `1` and the following required domains.
There are no earlier editor-state schemas or migration paths. `tools.transformActive` is a required boolean, allowing
the saved state to distinguish an active Select transform from the neutral no-transform sentinel.

CBOR's canonical integer representation already stores selection/entity indices in their shortest one-, two-, or
multi-byte form. Applying the native Level `sedIndex` byte convention inside CBOR would add a second integer grammar and
would not make ordinary non-negative editor indices smaller; signed editor sentinels remain canonical CBOR integers.

- `selection`: checked entity-reference sets for Sectors, Surfaces, vertices, Things, Lights, COGs, and frames;
- `engineExportPath`: the remembered JKL/NDY/JED target used by repeat Export; empty until the document has an engine
  export association;
- `active`: seven integers `[sector, surfaceSector, surface, thing, light, frameThing, frameValue]`; absent active
  frames use the paired `[-1, -1]` sentinel in the last two positions;
- `edit`: edit mode, Surface submode, and Light submode;
- `viewport`: independent editor/preview cameras and projections, pipeline, clip planes, render modes, complete grid state,
  X-Ray/orbit settings, light/Thing/thrust visibility, transform state, staged Multi-cut lines, their authored cameras,
  target context, linking, and Cleave options;
- `layers`: visibility, locking, colors, color-by-layer modes, and hidden-layer Outliner policy;
- `previewOverlays`: edit, transform, and tools overlay visibility;
- `window`: main/workspace Qt geometry and dock state, active tabs, all named splitter positions, the optional
  last-used `templateEditorGeometry`, and the complete settled UV editor workspace described below;
- `tools`: placement and modification tool identity, Thing template/shape, placement layer/snapping/orientation, and Brush
  mode/shape/size/count/spacing/scatter/rotation/air settings;
- `cogEditors`: open COG tabs, normalized source paths, unsaved text, modified state, cursor/scroll state, per-tab
  presentation zoom, pane splits, and optional primary/detached window membership in `hosts`. Flat script records
  and their document histories remain independent of host layout. Zoom changes the widget font only and is not part
  of authored COG text.
- optional `resourceEditors`: Sprite, Sound Class, and UNI group/tab membership, raw authored drafts, source identity,
  native baselines, and preview presentation. Resource pages and COG tabs may include their own bounded document
  `history` map, specified in the [EDIT workspace contract](../sed_format.md#edit). These journals belong to `edit`,
  independently of the Level timeline in `hist`; they retain Undo and Redo across manual Save, autosave, and recovery.
  Adding these optional maps does not change native V1–V4 framing or the root EDIT schema version `1`.

The represented editor domains and exclusions are inventoried in [editor_state.md](../editor_state.md). SPR/SND/UNI
raw edits and local document histories remain durable, and optional page `editorPresentation` V1 retains resource-pane
caret/selection, UNI current/ranges, SND stable current item and column, table scroll, manual SND widths, and deferred
cell-editor carets. COG cursor/anchor and scrollbar positions are separately represented.

Each resource tab's `page` map may contain `editorPresentation: {version: 1, ...}`. Its `lineEdits` map uses these stable
names: `spr.material`, `spr.width`, `spr.height`, `spr.extra_light.scalar`, `spr.offset.x`, `spr.offset.y`,
`spr.offset.z`, `snd.search`, and `uni.search`; each value is `{cursor, anchor}` with both integer positions bounded by
the corresponding raw editor text in UTF-16 code units. Forward and reverse selections retain separate cursor and
anchor offsets, restored directly as selection endpoints rather than translated into grapheme-step counts.
Supplementary and combining text use those same UTF-16 offsets. `tables.uni.entries` contains `current` (null or
`{row, column}`), `selection` (an array of `{top, bottom, left, right}` ranges), `horizontalScroll`, and `verticalScroll`. `tables.snd.entries` contains
`current` (null or an identity map with integer `kind` and either `sourceOrdinal` for an alternative row or exact
`modeBytes` for a mode, add-alternative, or column-header item), a separate integer `column`, both scroll offsets, and
`manualWidths`; the six-element integer `widths` array is present only when manual sizing is explicitly true, in
Play/Loop, Sound, Flags, Min, Max, Volume order. The per-column minima are `80/160/100/56/56/230` logical pixels;
each width is an integer no greater than `16384`, and their sum is no greater than `65536`. `tableEditors` maps keys of
the form `uni.entries/<source row>/<column>` or `snd.entries/<source ordinal>/<column>` to `{cursor, anchor}` caret
pairs. Known fields are validated against the raw page data, table row/column bounds, and stable source identities
before restore. The whole map is limited to 1 MiB;
`lineEdits` has at most 64 entries, `tables` at most 16, UNI selection at most 4096 ranges, and `tableEditors` at most
4096 carets. Scroll offsets are integers from zero through `10000000`. Unknown map members are ignored and may be
dropped on resave; malformed known fields reject that EDIT resource state before restoration.

The model and document history are rebuilt before this presentation is applied. Search/filter/collapse precedes current
selection, layout, and scroll restoration; scroll waits until the table is visible and is generation-guarded. A table-cell
caret is retained for later ordinary `setEditorData`, but restore never opens, commits, or focuses a live delegate.
Capture reads pending raw text and caret positions without committing, moving focus, advancing resource history, changing
authored dirty state, or arming native recovery. The entire `editorPresentation` map is excluded from authored-content
projection; it is presentation state only. Accepted data changes, Undo/Redo, document replacement, and successful
Sound Class Save normalization discard obsolete deferred cell carets, even when canonicalization changes only raw
numeric spelling and leaves the accepted value unchanged.

The COG text byte string is the authored buffer, not a token or diagnostic cache. Quoted punctuation, including colons
inside strings, is preserved as source and reanalysed after restore. Derived COG diagnostics, resource-thumbnail images
and parsed preview caches, filesystem-watch registrations, viewport pick indexes, and point-pick cycle state are not
members of `edit` or any other native section. Readers rebuild those transient structures from the restored Level,
resource configuration, and COG text.

The optional root `surfaceSelectionContext` map stores `version: 1` and required arrays `strictScope` and
`rememberedFaceSelection`. Each member is an exact `[sectorIndex, surfaceIndex]` pair of nonnegative int32 CBOR integers
that resolves within the saved Level. Writers sort pairs; readers accept any order but reject duplicates, malformed
pairs/types/version, and missing or out-of-Level references before applying any EDIT state. Unknown additional map
keys are ignored under the enclosing CBOR limits and may be dropped on resave. A present empty array is intentional
and never triggers inference. Only an absent map uses the legacy fallback: infer both sets from valid decoded
`selection.surfaces` and the owning faces of `selection.surfaceVertices`.

The Strict Boolean remains an application preference; this map stores its document-dependent frozen owner scope and
remembered Face selection. Restore installs both after the saved selection/submode. Successful topology commits and
known history transitions carrying both Structure and LevelGeometry clear the frozen scope, stale mode-selection cache,
and selected components, seed remembered Face selection from the committed/sanitized current faces, then return the
Surface submode to Face. Cold native history checks actual owner/connectivity changes for None, partial topology masks,
and masks with bits outside LevelGeometry, Things, Lights, Cogs, and Structure. `All = 0xFFFFFFFF` is the known all-domain
sentinel, not an unsupported mask. Unsupported masks also use the local unknown-dirty UI refresh path; stored raw masks
and HIST remain unchanged. Exact topology evidence contains the Sector count, IDs/numbers and vertex/Surface counts,
Surface numbers, ordered vertex indices, and adjoin references; coordinate-only and Thing-only changes retain compatible
context and committed Face selection.

For a protected multi-step jump with a known paired topology transition before an ambiguous transition, metadata gates
a detached Document walk that prepares exact adjacent-state comparisons before live cursor mutation. The probe copies
the current Level and uses copy-on-write history storage. Required topology copies/captures finish before live swaps,
and the probe is released first. The live topology check consumes the prepared first mismatch and retains invalidation
for the rest of the jump; no new fallible topology preparation occurs in that loop. Other protected
jumps use their jump-start baseline; known paired transitions retain their existing path. Failure in this topology
preparation leaves the live cursor unchanged. Cleave draft cancellation and workspace mementos retain their separate
raw-mask policy. Topology invalidation also resets the parked Surface submode while Sector mode is active.
Geometry-only transforms preserve compatible context.
Capture filters now-invalid convenience references; it does not reinterpret a stale in-range ordinal as a different
face. Native Save, live recovery, and autosave share this additive map without changing EDIT V1 or native versions.

Frame references name raw entries in the exact owning Thing's `values` array. Manual Save first strictly validates
the live EDIT payload, then maps selected frames and the final two active indices by logical frame ordinal into the
save-normalized candidate Level. The isolated current HIST state receives the same mapped selection; historical
frame-count normalization remaps each historical state's selection independently. A source reference that is not a
frame or has no same-ordinal candidate frame aborts save preparation without changing the live document. These are
application preparation rules; the serialized references remain raw indices and the schema version remains `1`.

Each COG tab may contain optional `acceptedSourceHash`, a CBOR byte string of length zero (unknown) or exactly 32
(SHA-256). It records the exact disk bytes last accepted by Open/Reload or successful external Save, independently of
the authored text and native checkpoint. File-backed loose buffers and archive-extracted working copies can carry
this baseline; generated unsaved drafts remain unknown until their first committed external Save. It is external-source
metadata, not authored dirty state. Absence is the legacy unknown baseline. A wrong type or other length rejects
the complete EDIT workspace before mutation. Watcher Keep does not advance this hash. Ordinary loose Save checks the
current bounded source before format-on-save and offers Reload/Overwrite/Cancel for an external revision or an unknown
source baseline. These additive fields retain EDIT V1 and native/recovery container versions.

The optional COG V1 `hosts` array records stable ID, primary flag, requested visibility, floating state, bounded Qt
geometry, host-local active selection, and indices into the existing flat script array. The primary is first with
ID `primary`; every script belongs to exactly one host. Admission permits up to 1,024 script tabs, 64 hosts including
an empty primary, and 1 MiB of geometry bytes per host. The full topology is validated before named docks are prepared
for Qt workspace restoration. Host-absent records load all scripts into the primary window. Preview capture retains
the visibility requested before its transient hide. These optional fields use the existing EDIT schema V1 and native
framing; the precise membership contract is in [EDIT workspace](../sed_format.md#edit).

Each serialized viewport state includes its exact fog, Thing collision-shape and visual-bounds visibility, classic
3DO model LOD selection, and Arch Thing gizmo visibility. Restore never replaces present document values with app
defaults. The optional trailing named viewport-state map carries the Thing overlay flags and the boolean
`thingModelLodEnabled`; adding this flag does not shift positional fields or bump the native SED version. Legacy
16/17-entry arrays remain accepted. Missing overlay flags use the user's per-pipeline app defaults, while a missing
3DO LOD flag restores off even if the current app default is on. Fresh Editor defaults show both overlays and fresh
Game Preview defaults hide both; classic 3DO LOD defaults off in both. If EDIT is absent or rejected, Editor fog
defaults off and Game Preview fog defaults on. Manual Save, autosave, and recovery share this EDIT representation.

Known named fields are type-checked; unknown named keys are ignored, not round-trip-preserved, because a writer emits
only fields understood by its build. Future gizmo, rendering, and overlay booleans can be added to this map without
shifting positional fields or bumping the native SED version. An older application's re-save may drop unknown keys.
Tab bulk-close operations require no new record: surviving open buffers, order, pin state, and active tab continue to
use the existing `cogEditors` fields.

The Cleave map nested at fixed viewport-array index `38` may contain the optional named `guides` map. Writers emit
`angles`, `lengths`, and `numbers` as Booleans. When `guides`, `angles`, or `lengths` is absent, the corresponding angle
and length visibility follows the saved legacy `options[2]` measurement-visibility Boolean; absent `numbers` defaults
to `false`. A present `guides` value must be a map, and each present recognized field must be Boolean. A wrong map or
known-field type rejects the complete EDIT workspace before live mutation; unknown names are ignored. The fixed
`options[3]` unit field remains an integer from `0` through `4`; current writers use Bare (`0`), Meters (`1`), JKU (`2`),
or IJU (`3`). The legacy None token (`4`) is normalized on read to Meters with `guides.lengths=false`; any explicit
`guides.angles` and `guides.numbers` values remain authoritative. A noninteger or out-of-range unit value rejects the
complete EDIT workspace before live mutation. Manual Save, autosave, and recovery share this representation. These
compatibility rules do not change EDIT schema V1, fixed viewport-array positions, or native V1–V4 versions.

That Cleave map may also contain optional `chainOrigin: [lineIndex, endpoint]`. A present value must be an array of
exactly two CBOR integers. `lineIndex` must be nonnegative and less than the saved `lines` count; `endpoint` must be
`0` (first) or `1` (second). Invalid shape, types, or bounds reject the complete EDIT workspace before live mutation.
Writers omit the field when no explicit source exists; absence means last staged line's second endpoint, with no
source if there are no lines. The source remains saved while Chain is disabled and does not reorder `lines`.
Manual Save, autosave, and recovery share this additive field without changing fixed arrays or schema versions.

The same Cleave map may contain optional `bounded` as a CBOR Boolean. A present value is authoritative, and any other
type rejects the complete EDIT workspace before mutation. With saved staged lines, absence restores `false` to retain
the previous infinite-plane arrangement; without staged lines, absence retains the app preference (fresh default
`true`). Writers emit the current choice. The finite world endpoints already stored per line supply reach metadata;
no line-array fields, fixed viewport positions, EDIT version, or native SED version change is required. Closed chains
retain their existing interior-descendant semantics regardless of this flag.

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

The optional root `gridAppearance` map is separate from the fixed `viewport` array. Its optional `minorDots` field is
boolean (default `false`, Lines), and `minorDotSizePx` is a finite CBOR number in the inclusive range `0.5`–`10`
(default `2` Qt logical pixels). An absent map or field uses these defaults even when application preferences differ.
Present valid values are authoritative. A non-map extension, wrong known-field type, non-finite diameter, or diameter
outside that range rejects the entire EDIT workspace before mutation. Unknown map keys are ignored. Writers emit the
current minor-grid choices. Optional `axisColors` is an exact three-item X/Y/Z tuple of unsigned 32-bit RGBA integers;
wrong type/count/range rejects EDIT before mutation. Axis appearance and snap spacing remain independent. This additive map uses
EDIT version `1` and existing native V1–V4 framing, and is shared by manual Save, autosave, and recovery.

Before Qt materializes the CBOR graph, the loader performs an allocation-bounded streaming pass. It admits at most a
128 MiB payload, 2,000,000 nodes, depth 64, bounded text/byte strings, and a dynamic input-amplification allowance with
a 128 MiB absolute decoded-graph/canonical-verification ceiling. Indefinite containers/strings, tags, invalid values,
non-text or duplicate map keys, trailing bytes, and noncanonical encodings are rejected. The materialized schema then
validates numeric bounds, enum ranges, booleans, references, and
domain-specific blob limits before applying any part. A malformed `edit` never half-applies state.

The optional `window.templateEditorGeometry` is the Template Editor's last-used Qt geometry byte string. A valid SED
workspace value overrides the application-level geometry preference; older V1 payloads omit the key and continue using
the application preference.
The modal Template Editor's unaccepted working templates, search, selection, preview, and local edits are not a
checkpointed document. Only accepted Level/template state and this last-used geometry are durable.

The optional root `architecturalLighting` map retains settled MotS Arch workspace selection independently of Level
lighting values. Its version `1` fields are `authoringMode` (Mono/RGB enum), `targetMode` (Owner/Vertices enum),
`selectedVertices` (unique `[thing, lod, mesh, vertex]` integer identities), and Boolean `implicitOwnerExpansion`.
Identities require nonnegative int32 indices, LOD `0`, and a valid owning Thing. Encode/decode preflight a 16 MiB
owned-selection budget using a conservative 192-byte estimate per identity before uniqueness tables or nested
allocation. Known fields are strict; unknown optional names are ignored. A malformed map rejects the workspace atomically.
Model-dependent references activate only after the matching resource/mesh cache is ready. Missing resources retain the
accepted pending subset for later resolution; hover/cycle state, previews, and caches are transient. Owner-expansion
provenance is preserved rather than inferred from a coincidental selected set.

#### UV editor workspace

The `window` map stores the UV panel dock/expanded extent plus the stable controls `uvViewportFacing`, `uvRepeat`,
`uvSnap`, `uvGridStep`, and the optional `uvSubGrid` boolean. Older V1 payloads may omit `uvSubGrid`, which restores
the default disabled state.

The optional `window.uvGridAppearance` map controls the UV Sub-grid independently of root `gridAppearance`.
Its optional `minorDots` is boolean (default `false`, Lines), and optional `minorDotSizePx` is a finite CBOR number in
the inclusive range `0.5`–`10` (default `2` logical pixels). Missing maps or fields use those defaults regardless of app
preferences; present valid values are authoritative. A non-map extension or invalid known field rejects the complete
EDIT workspace before mutation, while unknown keys are ignored. Grid spacing, snap state, repeated-tile borders, and
authored UVs remain independent. This additive named map retains EDIT version `1` and native V1–V4 framing.

The `uvState` value is a bounded canonical-CBOR byte string with this exact nested V1 shape:

```text
{
  "version": 1,
  "view": [
    zoom,
    panX,
    panY,
    mirrorX,
    rotationRadians
  ],
  "active": [
    sectorIndex,
    surfaceIndex
  ],                                             // [-1, -1] means none
  "islands": [
    [
      sectorIndex,
      surfaceIndex,
      faceSelected,
      vertexSelectionBytes,
      edgeSelectionBytes,
      displayMirrorAxisX,
      displayOffsetX,
      displayOffsetY,
      displayMirrorX
    ],
    ...
  ]
}
```

Each selection byte string has exactly one byte per Surface corner and admits only byte values zero and one. Island
records are unique and strictly ordered by `(sectorIndex,surfaceIndex)`, must exactly match the restored Surface-only
document selection when present, and are checked against current Surface topology before any workspace mutation. The
active island must be selected in the restored face/edge/vertex submode. The display mirror/offset fields preserve the
multi-surface UV presentation without changing the authoritative stored UV coordinates. `view` preserves zoom, pan,
and the exact whole-canvas View Face basis; because the texture and grid use that same basis, it also preserves the grid's
visible position and alignment.

When `window.uvViewportFacing` is false, the nested mirror flag and rotation are canonically false/zero; an inconsistent
button/basis pair rejects the complete `edit` payload rather than restoring a visually rotated view behind an inactive
button.

```text
restore Level + document selection
              |
              v
build UV islands from authoritative Surface UVs
              |
              v
restore dock/splitters and cancel deferred Fit
              |
              v
apply exact zoom + pan + display basis + UV selection
```

`viewportUvSplitter` is dynamic: recovered UV dock position can change its orientation and child order. Its checked sizes
are therefore deferred until the UV panel sync has finalized that layout. The V1 two-pane contract contains only
the viewport and UV editor. Early V1 editor snapshots could contain a trailing zero third size because a hidden docking
preview overlay was accidentally parented as a splitter pane; readers normalize only that exact `[pane0, pane1, 0]` legacy
shape. Other pane-count mismatches are ignored as unavailable layout decoration, while malformed/out-of-range sizes and
mismatches for stable named splitters remain warnings.

The nested payload is required, limited to 16 MiB, and receives its own bounded canonical-CBOR preflight before Qt
materializes it. Missing or invalid `uvState` rejects the complete `edit` payload rather than partly applying it. If the
whole optional `edit` section is missing or rejected at its all-or-nothing boundary, the application uses a clean fitted
UV canvas with deterministic default controls as part of the safe workspace defaults. Manual `.sed` saves, recovery
snapshots, and periodic autosaves all carry this same `edit` schema; there is no separate UV-state codec for any path.

#### Stable checkpoint boundary

`edit` checkpoints stable workspace/tool configuration, staged Multi-cut lines, and unsaved COG buffers; it is not an
event journal for every live input transaction. The required Level sections remain the authoritative authored state.
Multi-cut retains both independent settled family drafts and line-only timelines/cursors while inactive or in Game Preview.
Restoration does not activate Cleave or change the saved edit mode/selection; matching Cleave ownership gates draft
input and local Undo/Redo. Checked `workspaceHistory` binds Apply
workspace mementos and historical Layer presentation to usable HIST; losing that optional sidecar does not drop the
current EDIT draft. Legacy current-only records start from a fresh local baseline when restoration admission permits;
otherwise the current draft is retained without that optional baseline.

An active Surface Extrude preview and a partially held brush/draw or other pointer gesture are finalized or cancelled
at snapshot/apply boundaries instead of being resumed mid-gesture after restart. UV `G`, `S`, or `R` modal transaction
bookkeeping is likewise omitted: only the settled canvas view and selection are captured, never a half-held operation.
Explicit Face/component UV pivot choices are also transient and are cleared when restoring the UV workspace; later
transforms use the restored selection's usual center until a new pivot is chosen. Template dialog working drafts and
resource table presentation exclusions follow the inventory above.
Because `edit` may be the sole durable
copy of unsaved COG source, its top-level native section uses the same four-parity-stream tier as required Level sections.

### `hist`

#### CBOR schema and base references

`hist` is RFC 8949 CBOR using the one accepted V1 schema:

```text
{
  "version": 1,
  "encoding": "sed-semantic-base-delta-v1",
  "fragmentLayout": "sect-record-v1",
  "baseDigest": byte-string(32),
  "baseCount": non-negative integer,
  "pool": [
    byte-string fragment,
    ...
  ],
  "undoGroups": [
    [
      entry,
      ...
    ],
    ...
  ],
  "redoGroups": [
    [
      entry,
      ...
    ],
    ...
  ]
}

entry      = [
               label,
               dirtyFlags,
               selectionMap,
               stateRecord
             ]
reference  = poolIndex | (-1 - baseFragmentIndex)
checkpoint = [
               1,
               0,
               [
                 reference,
                 ...
               ]
             ]
delta      = [
               1,
               1,
               prefixCount,
               suffixCount,
               eraseCount,
               [
                 insertedReference,
                 ...
               ]
             ]
```

When pending JKL COG import state is represented in the current or a history state, the root may also contain the
optional exact four-field `jklCogImportV1` map:

```text
{
  "version": 1,
  "current": [rowsPresentBool, [[sectorI32, surfaceI32], ...]],
  "undo": [[stateIndex, rowsPresentBool, [[sectorI32, surfaceI32], ...]], ...],
  "redo": [[stateIndex, rowsPresentBool, [[sectorI32, surfaceI32], ...]], ...]
}
```

`current` is explicit whenever the extension exists and must equal the normalized represented import metadata in the
authoritative current Level. `undo` and `redo` are sparse arrays indexed from the nearest state outward; their indices are
ascending and unique, and an omitted index means that state has no import metadata, never inheritance. Each state's
ordered Surface pairs are preserved exactly. The extension is emitted only when the current Level or at least one stack
state has represented import metadata; false-marker unused maps normalize to empty. This optional addition retains HIST
V1 and native SED V1–V4. It is separate from each nested `HistoryNested` Level, which always contains exactly the eight
required Level sections and never `jimp`.

The import extension stores a complete ordered Surface map separately for every represented current/history state;
sparse state indices do not deduplicate repeated maps. The aggregate reference limit is two million pairs across
`current`, `undo`, and `redo`, in addition to the existing HIST CBOR node and encoded-size budgets. Large JKL imports
whose scripts remain unavailable can therefore exceed those budgets after otherwise ordinary history edits. HIST is
optional: Save/recovery omits it with a warning when encoding or admission fails, while retaining the authoritative
current Level and its represented `jimp` provenance.

An older pre-V1 development payload may omit `fragmentLayout`; that exact
seven-key shape means `fixed-v1` and is decoded with the original fixed
8192-byte base partition, without `jklCogImportV1`. New writers emit the eight required root fields above, plus a
ninth `jklCogImportV1` field when needed, and an explicit fragment-layout discriminator. They prefer `sect-record-v1`
and use `fixed-v1` when the bounded Sector-record manifest would exceed the per-state reference limit. Unknown, missing,
or malformed discriminators in the explicit shape are rejected rather than guessing a base numbering.

Undo and redo entries are nearest operation first and are grouped in nonempty arrays of at most 32 entries. A state is
reconstructed by resolving its checkpoint or delta to a reference list, concatenating those pool fragments, then
strictly decoding the result as a nested Level. The save-time current Level remains outside `hist` as the authoritative
document state. `baseDigest` is SHA-256 of its canonical `HistoryNested` bytes followed, only when the current Level
has represented JKL COG import metadata, by this domain-separated suffix: ASCII `SED-JKL-COG-IMPORT-V1` without
a terminator, a canonical marker byte (`0` or `1`), a little-endian `u32` Surface-reference count, and the ordered
`i32` sector/surface pairs in little-endian form. Empty/unused import provenance appends nothing, preserving existing
base digests. This binds import provenance without adding `jimp` to the nested Level frames. With `fragmentLayout = fixed-v1`,
`baseCount` is the number of canonical fixed-offset 8192-byte base fragments (including the atomic HNOD tail when
present). With `fragmentLayout = sect-record-v1`, the large `sect` frame instead uses the checked descriptor/count
prefix, Sector-record fragments, and trailer described below; all other sections retain the fixed-block/atomic-tail
layout. A negative reference `-1 - i` resolves to base fragment `i`; a non-negative reference resolves to a `pool`
entry. The pool therefore contains only bytes that differ from every current-base fragment. The
loader must receive the exact top-level Level, verify its digest/count, and reject history if that base is unavailable
or differs. When two base fragments are byte-identical, every reference uses the first fragment's negative index; later
duplicate indices are noncanonical and rejected. Selection references are validated against each reconstructed Level
before the history is accepted. Each entry's `dirtyFlags` must also describe its exact adjacent transition under
`DocumentUndoHistory` snapshot semantics: compact Thing/light/COG masks may differ only in the domains that those
snapshots actually retain (including baked ambient/texture-vertex colors for light snapshots). A mismatched mask is rejected so
Undo/Redo cannot reconstruct a hybrid state from an otherwise checksum-valid `hist`.

#### Compact snapshot restoration

After validating one reconstructed state, the native reader converts it immediately into that compact live snapshot
representation. The nested decoder moves unchanged Level vectors through a failure-atomic transaction and retains only
the preceding values of semantic domains that were actually rematerialized. Dirty-domain validation and exact changed
Thing/Sector subset inference consume that compact transition view directly; they do not copy or materialize the complete
preceding Level. A compatibility `previousLevel()` view exists for inspection/tests and builds a full copy only on an
explicit call. This prevents a sequence of Thing-only changes from copying, retaining, or budgeting the same geometry
once per entry for ordinary subset snapshots; full structural snapshots remain fully charged and protected.
Nonempty MotS bake provenance can require a FullSnapshot even for a pure Thing transition. Retained byte/item admission
follows that actual snapshot variant and charges the complete Level, rather than treating its dirty mask as proof that
only changed Things are retained.

The retained footprint for a `Lights` or `Things+Lights` snapshot follows the
actual compact representation: baked sector/surface records and one `ColorF`
per texture vertex. It does not charge a synthetic `Sector`/`Surface` object
with complete `TexVertex` (UV plus colour) records. This keeps admission
accounting correct for large IJIM baked-light timelines instead of dropping a
valid HIST stream solely because an oversized geometry-shaped proxy was used.

Normal `.sed` Open and the post-recovery project-selection path both use this
consuming compact restore directly. The expanded history DTO is inspection
and test tooling only; using it to install a document would incorrectly charge
the unchanged Level once per entry and can reject an otherwise valid large
timeline against the 768 MiB retained-snapshot envelope.

#### Checkpoints and deltas

Undo and redo have independent delta chains seeded from the current Level reference list. The first state in each stack
is therefore a delta from the save-time current Level whenever that is smaller than a checkpoint; it never requires a
second complete Level snapshot. State indexes `16`, `32`, ... in each stack are mandatory checkpoints.
For a manual `.sed` Save, derived Thing `numframes` values are synchronized to each state's actual frame list while the
temporary history copy is walked, just as they are for the authoritative current Level. This prevents Undo after reload
from reintroducing a stale derived count without allocating a second expanded history vector. Automatic crash recovery
continues to capture the exact live document state rather than applying save/export normalization.
If normalization changes a Thing-value index, selected frames are preserved by logical frame ordinal and remapped before
that historical state is encoded; normalization is applied to an isolated Things/selection copy so it cannot contaminate
the next compact history swap. Manual Save then performs bounded canonical-CBOR and exact timeline-count validation on
the trusted writer output without re-decoding every just-serialized historical Level. Every Open, recovery read, and the
ordinary recovery writer still runs the complete semantic transition/selection/Level decoder.
For any intervening state, the encoder finds the longest non-overlapping common prefix and suffix between the previous
and new reference lists. The canonical splice keeps those ends, sets
`eraseCount = previousCount - prefixCount - suffixCount`, and inserts the new middle, so the result is
`previous[0:prefixCount] + inserted + previous[previousCount-suffixCount:]`. The encoder writes that delta only when its
canonical CBOR encoding is strictly smaller than the checkpoint record; otherwise it writes a checkpoint. The decoder
enforces the periodic checkpoints, exact record shapes, splice bounds, longest-prefix/suffix form, and the same encoded
size choice. Record version `1`, kind `0` (checkpoint), and kind `1` (delta) are the only accepted values. Earlier
pre-V1 complete-state and pool-without-delta `hist` schemas are not migrated or accepted.

#### `HistoryNested` validation and frame caching

Each history Level is first serialized with the explicit `HistoryNested` profile. Its fixed header and section
descriptors use V1 for an explicitly `TexturePixels` state without retained ARCH objects or bake baselines, V2 for a
`Normalized` state without either, V3 for a normalized MotS state retaining objects or baselines, and V4 for its
pixel-space counterpart; the profile
name does not select the coordinate-space version. It retains the canonical `sed\0` header, exactly the eight required Level
schemas, checked descriptor/header mirrors, and footer. It never contains optional `jimp`. Its sections are raw:
compression and parity fields are zero. Descriptor flag `0x4` is mandatory and selects the canonical inner checksum layout: bytes 0..3
of the 32-byte digest field contain little-endian CRC32C of the expanded payload and bytes 4..31 are zero. This avoids
repeating portable SHA-256 over the same multi-megabyte geometry for every adjacent undo state. It does not weaken the
file boundary: the complete `hist` section is still independently SHA-256 checked and p=2 protected, while the current
Level+`edit` remain separate p=4/SHA-256 sections. `HistoryNested` is rejected by the ordinary top-level and generic
`UnprotectedNested` readers and is accepted only after the enclosing `hist` section has validated.

The generated-history writer keeps independent undo and redo caches of those exact eight checked section frames. Before
each transition `DocumentUndoHistory` derives invalidation from its concrete Snapshot variant and mints a noncopyable
token bound to the exact temporary Level object being visited, never from
`dirtyFlags`: full snapshots invalidate all; Thing/subset/tail snapshots invalidate `head`+`thng`; COG snapshots
`head`+`cogs`; Light snapshots `head`+`sect`+`lite`; combined Thing/Light snapshots add `thng`; and Sector/Surface
subsets invalidate `head`+`sect`. Manual `numframes` normalization also invalidates `thng` whenever it changes the
temporary state. The writer always regenerates and byte-compares `rsrc`; if it differs, it additionally regenerates every
category-indexed section (`head`, `sect`, `thng`, `tmpl`, `cogs`, `hnod`). Because this profile has no offset-dependent
FEC alignment, unchanged core-generated frames are reusable without changing bytes. A requested semantic no-op retains
its previous frame owner only after exact byte equality. HIST maps that core-owned identity to already-interned fragment
references, so unchanged frames need no complete nested byte vector, block rescan, hash, or copy. Frames-only
generation still reports the exact full document size for bounds accounting. Undo/redo caches start from the
authoritative current Level and advance only after the complete entry succeeds; failure rolls back newly interned pool
entries. This is solely a trusted writer-side
optimization. The loader still strictly validates every reconstructed nested document and its assembled Level graph;
after complete frame integrity succeeds, it may reuse a semantic domain only when that exact checked frame matches the
decoder-owned preceding state. Candidate installation swaps only rematerialized domains and commits after the final
canonical ResourceIndex/graph checks; a late failure swaps every prepared domain back and leaves the prior Level, bytes,
statistics, and transition cache unchanged.

#### Sector-record fragmentation

The nested file is split at validated semantic section boundaries. Under
`sect-record-v1`, the large `sect` frame uses its checked prefix, bounded
Sector-record fragments, and trailer; all other sections use canonical fixed
8192-byte blocks (the final block may be shorter). Under explicit `fixed-v1`,
all sections use those fixed blocks. Fixed offsets are intentional: when a
regenerated frame has the same bytes at the same block offset as the preceding
frame, the writer reuses that preceding reference without hashing or
interning the block. Only changed blocks enter the shared pool. The final 64 bytes of the last semantic fragment are kept
exactly as the shared native atomic layout: the last 16 `hnod` bytes followed by the
48-byte checked header-mirror/footer. The HNOD body before that tail uses the
same fixed-block rule; the tail is never reused at a different offset. When a
historical Level changes native version, its fixed header and matching HNOD
mirror/footer are regenerated or reused as a pair; references from one version's
framing are never combined with another version's descriptors.

The writer applies one additional semantic partition to the large `sect`
frame. Its checked descriptor/count prefix, each self-delimiting Sector
record (split into bounded 8192-byte pieces only when an individual record is
larger), and its trailer are pooled as separate fragments. Exact byte equality
then reuses an unchanged Sector after a neighboring Sector grows, shrinks, or
is inserted, even though all following byte offsets moved. When the same-index
prior record has the same bounded fragment layout, the writer compares those
bytes in place and copies its existing references without allocating or
hashing the unchanged record; only changed or reordered records go through
the content pool. This is not a new on-disk record directory: HIST still
stores only an ordered reference list,
and the reader reconstructs the exact nested Level by concatenating those
references. If a record cannot be safely delimited by the writer's checked
codec, the writer rejects the optional HIST transaction rather than emitting
an ambiguous fragment layout; the authoritative Level remains saveable. The
record layout is also bounded to 300,000 references per reconstructed state.
The bounded SECT scanner checks the counted record total before reserving its
span vector, and the fragment builder stops as soon as bridge/record pieces
would exceed that budget. A valid Level that exceeds the record-layout budget
deterministically retries the explicit `fixed-v1` layout; it is not published
with an oversized or partially materialized `sect-record-v1` manifest. If the
fixed layout itself cannot satisfy the normal history limits, HIST is omitted
while the authoritative Level remains saveable.

#### Shifted-tail reuse

The HIST application schema V1 also permits a writer-only hybrid layout for a structural section whose expanded frame
size changes. The writer excludes that section's checked
descriptor/trailer from matching, finds the longest exact suffix (requiring at
least 64 bytes), and ends a newly emitted bridge at the first preceding-frame
fragment boundary wholly inside that suffix. The unchanged same-offset prefix
spans are also referenced directly up to the first span crossing the changed
region. Bytes before that direct prefix and between it and the shifted suffix
are emitted as canonical bridge chunks no larger than 8192 bytes. The
unchanged suffix fragments are then referenced at their shifted offsets. The current section
trailer, and for final `hnod` the outer framing, are emitted as a fresh atomic
tail no larger than 8192 bytes. The encoder stores these byte spans in each
stack's `FrameState`, so a second grow/shrink and a same-size edit after a
resynchronization reuse the variable layout rather than silently reverting to
whole-tail repooling. If the suffix, span boundary, or bridge cannot satisfy
the safety limits, the writer deterministically falls back to the fixed
layout. This does not change the wire schema or the base-fragment count.

The manifest may therefore contain any checked fragment length from 1 through
8192 bytes; readers concatenate the references exactly as listed and do not
infer writer boundaries. Nested `HistoryNested` CRC/framing validation and the
enclosing `hist` SHA-256/FEC remain mandatory, so the hybrid layout cannot
weaken failure-atomic reconstruction. Fragments byte-equal to the current-base
dictionary are represented by negative base references and never copied into
the pool. The pool hash is only an index accelerator; byte equality decides
reuse.

Untrusted nested bytes always take the strict splitter, which validates the
complete CRC32C-framed semantic document before exposing boundaries. A bad
inner checksum drops the complete optional history transaction; no prefix is
installed and the authoritative current Level+EDIT remain usable. The history writer
uses the same checked framing arithmetic directly on bytes it has just
emitted with `saveToSedBytes`; it does not decode every freshly serialized
historical Level a second time merely to rediscover those boundaries. The two
checked semantic split paths produce the same reconstructed bytes. A hybrid
writer partition may use different fragment boundaries, but concatenation is
still byte-exact. This optimization does not change the HIST application schema V1 or
weaken load/recovery validation.

The fixed-layout block boundary is format-stable and does not depend on a
rolling hash: block `n` covers byte offsets
`[n*8192, min((n+1)*8192, sectionSize))`. The hybrid path deliberately records
its shifted spans in the writer's frame state instead of asking a reader to
rediscover them; exact concatenation remains the only reader operation. This
avoids scanning every byte to rediscover variable boundaries and makes both
same-size reuse and chained resynchronization deterministic across platforms.
The only fixed-layout exception is the final HNOD/trailing-framing 64-byte
atomic tail described above.

#### Canonical bounds and outer protection

The enclosing top-level `hist` section then makes one normal compression decision only in the 4 KiB..64 MiB range,
stores one expanded SHA-256, and applies its two-parity-stream striped protection. Nested Levels never receive a second
compression/FEC layer. Before materializing the CBOR object graph, the application uses bounded-scratch string traversal
and rejects more than 2,000,000 nodes, nesting deeper than 64, byte strings over 8 KiB, or text items over 3 MiB. The
semantic decoder bounds the combined undo/redo timeline to 100,000; installing
that timeline in the current GUI retains the 1,024 entries nearest the cursor.
The decoder bounds the pool to 1,000,000 fragments and 64 MiB of cumulative fragment bytes, each state to 300,000 fragment
references and 256 MiB of expanded reconstruction work, and each label to 1 MiB of characters. Cumulative reconstruction
charges shared base/pool fragments once plus a bounded per-state floor and still obeys the dynamic budget
`min(256 MiB, max(32 MiB, encodedPayloadBytes * 256))`; independently, each transient undo/redo Level remains capped
at 128 MiB/two million items. During consuming/native restore, ordinary pure `things` transitions retain only changed
Thing values; a transition whose MotS bake provenance requires a FullSnapshot is charged as a complete Level instead.
Topology-preserving `levelGeometry` transitions retain only changed owning Sectors (including adjoin/shared
vertices); structural or otherwise ambiguous transitions retain a complete Level. The optional history timeline's
compact retained snapshots have a 768 MiB/32-million-item aggregate envelope. Pool fragments must be byte-unique, ordered by first
use, and all referenced; every non-final undo/redo group has exactly 32 entries. These canonicality rules reject
semantically redundant histories before they can spend repeated Level-reconstruction work.

## Strict loading, partial recovery, and resilient I/O

Normal `.sed` Open validates the size bounds, exact magic and supported V1-through-V4 version, matching descriptor versions,
project type, fixed-header CRC32C/copy agreement,
section count, descriptor CRC32C/copy agreement, all layout arithmetic and flags, slot CRC32Cs, MDS reconstruction,
decompression bounds, expanded SHA-256, required-section presence, duplicate types, decoded collection/string bounds,
aggregate expanded size, aggregate decoded-object/item bounds, finite numeric values,
`ValueType`/`AdjType`/`CogType` discriminants, project-type agreement,
and complete checked framing. Semantic data reconstruction is exact and observable; the original
file is not modified. Once a required Level section has passed its size and SHA-256 checks, its schema decoder runs
outside the corruption-recovery boundary: a checksum-valid required-section schema or semantic error is a hard failure,
not storage damage. Geometry/reference inconsistencies that belong to the editor consistency checker remain loadable so
unfinished or checker-invalid Levels can still be inspected and repaired. `sed_core` returns validated optional section
bytes opaquely. The application then validates `edit`
and `hist` at their own all-or-nothing boundaries. Save emits both. Direct `.sed` Open accepts a complete Level when
`edit` or `hist` is absent/unrepairable, uses a default workspace and/or empty timeline, reports the loss, and marks the
associated document modified for a clean rewrite. A present checksum-valid but semantically invalid `hist` is dropped as
one optional domain with a warning and empty timeline; it never vetoes an otherwise valid Level. A malformed known
`jimp` is rejected by strict loading; corruption recovery can drop only that optional provenance, clear its marker/map,
preserve the independently verified Level, and report the lost `jimp`. An invalid `edit` never half-applies and falls
back to default workspace state. Optional-loss cases require a clean rewrite without detaching solely for that loss.
Crash recovery follows the same Level-first optional-payload policy.

Before a manual `.sed` Save can replace its destination, the newly captured `edit` payload is run non-mutating through
the same complete versioned schema parser used by restore, including Level-relative layer and selection validation.
Autosave/recovery applies the same check before advancing its publication generation. A capture-side schema regression
therefore leaves the preceding durable file/checkpoint untouched rather than publishing an unreadable workspace.

The corruption-only recovery API returns structured evidence: repaired chunk count and section names, missing required
and optional section names, unidentified lost units when framing no longer reveals a type, whether recovery was partial,
and persistently unreadable byte ranges. An unrepairable required Level section is represented by that whole domain's
default/empty value; an unrepairable optional section is omitted. No record inside a lost section is decoded or guessed,
and references from surviving domains can consequently be dangling. The strict codec entry point rejects an
unrepairable section instead of applying these defaults.

Sith Editor's direct `.sed` Open is a strict-first transaction. It calls the strict path first and invokes
`recoverSedFile()` only after that path fails. The recovery codec catches only integrity failures proven by checked
framing, slot CRC32C/MDS, decompression, or expanded SHA-256; checksum-valid required schema/programming errors throw
again and are never masked by the application fallback. The resulting UI policy is:

- transparent repair with every required/optional domain present stays associated with the selected path but is marked
  modified; the source bytes are never scrubbed in place;
- optional `jimp`/`edit`/`hist` loss keeps the complete Level associated only when every damaged unit remains identified,
  applies safe defaults, reports exact missing units, and marks the document modified;
- loss of any required Level section, or any unit whose damaged descriptor copies no longer identify its semantic type,
  prompts before old-document teardown or candidate resource mutation. An unidentified unit may conceal required Level
  data, so it is conservatively detached even if another descriptor copy resembled `edit`/`hist`. Cancel leaves
  the current document, path, resource kind/locator, settings, snap caches, and selected damaged source untouched;
  acceptance opens a detached `Untitled*` document and Save requires a new destination;
- if both fixed headers and `head` are lost, direct Open combines partial-recovery confirmation with an explicit
  JKDF2/MotS/IJIM choice. No button is preselected, Cancel is the default, `hist` project validation and resource setup
  occur only after the choice, and synthesized JKDF2 is never treated as authoritative;
- repair count/sections, required and optional loss, unidentified units, persistent unreadable ranges, and the strict
  failure that activated recovery are written to the Messages panel.

Acceptance of named required loss or any unidentified lost unit schedules non-authoritative text salvage beside the
unchanged direct source. For unidentified units the manifest records the exact count and that no surviving checked
framing can name or map them; any one may have contained required data. The worker
re-reads the source to avoid copying a potentially large Level across the GUI thread, but it first/then requires the
captured size/write time, normalized required-loss set, unidentified-loss count, and checked project classification to
match what the user accepted. A changed/replaced source aborts salvage. When the checked project type was unavailable,
the worker reapplies only the user's explicit choice before selecting JKL versus NDY. The text artifact contains only
fully decoded semantic domains plus defaults for explicitly lost domains; it is emitted by the ordinary readable
JKL/NDY writer with `compact=false`, never the optional compact runtime-export presentation. It cannot contain or
reconstruct bytes from an unrecoverable section. The unchanged damaged binary remains the sole forensic source for those
bytes.

Direct-file loading first admits the reported size, allocates exactly once, and fills it with direct 1 MiB stream reads
plus a one-byte growth probe. Only an actual short/I/O read activates the resilient path; a
fully read but invalid file is not retried as if it were an erasure. The production fallback retries independent 4096-byte
ranges up to three times, zero-fills a persistently unreadable range, records it, and lets descriptor mirrors and striped
parity treat the zeros as checked corruption. Public recovery seams allow bounded alternative block sizes/attempt counts
for tests and other callers (`1..1 MiB` and `1..100`, respectively).

Embedded native-document discovery is also work-bounded for hostile input: it examines at most 256 checked header
candidates, accounts at most 8,192 descriptor probes, fully decodes at most eight strict candidates, and limits cumulative
strict candidate validation to twice the supplied byte count. The complete permissive native recovery operation shares
an 8,192 exact-marker ceiling, twelve input-sized marker-scan passes, and four encoded input-sized decode passes across
its fallback branches. Budget exhaustion fails closed and is not swallowed as an ordinary corrupt candidate. A normal
recovery envelope contains one authoritative native document; these larger ceilings exist only to pass damaged or stale
anchors on the way to a later complete one.

There is no whole-file checksum. `sedend\r\n` is only an eight-byte canonical footer marker, and the trailing fixed-header
mirror is independently checked. In-place loss of trailing framing can be reported while intact primary metadata still
loads; truncation or ambiguous checked metadata remains a failure unless the corruption-recovery scanner can establish a
new checked section boundary.

Normal saves use `QSaveFile` with direct-write fallback disabled, so a crash or power loss cannot replace the last good
file with a half-written one. The crash-recovery envelope and its scanning, forensic-retention, and autosave lifecycle are
specified in [recovery_format.md](recovery_format.md).

## Limits and implementation

- complete native file: 256 MiB maximum;
- section expanded/logical stored payload: 128 MiB maximum (`hist` is type-specific and capped at 64 MiB);
- aggregate expanded payload over all sections: 256 MiB maximum, accounted before decompression;
- one transient decoded Level: 128 MiB of logical owned storage and two million logical vector items maximum, admitted
  before `reserve`; retained compact history snapshots use a separate 768 MiB/32-million-item aggregate envelope;
- protected coded section: `(16 + 4) * ceil(ceil(128 MiB / 4092) / 16) * 4096 + 4095`, currently
  `168,022,015` bytes including worst-case alignment;
- string: 16 MiB maximum;
- collection: two million records maximum;
- section count: 1,024 maximum.

Writer admission applies the decoded-Level item/storage and finite-value checks
before constructing the categorized resource catalogs, so an object graph that
cannot be reopened is rejected before its resource strings are copied into the
writer index. Public profile-taking APIs accept only `ProtectedV1`, `UnprotectedNested`, and
the explicit enclosing-HIST-only `HistoryNested`; out-of-range enum values fail closed rather than silently
selecting the unprotected profile. `ProtectedV1` is the retained API name for the established top-level protection
profile; it does not force native file version 1. The Level's UV coordinate-space tag and filtered MotS arch-lighting
snapshot select native V1/V2 without retained ARCH objects or bake baselines, or V3/V4 when either remains.

The on-wire sizes remain 64-bit for future versioning, but the current V1-through-V4 implementation stages complete documents and
several checked semantic buffers in memory. The former 4 GiB admission limit therefore multiplied memory and CPU work
without providing useful project headroom; 256 MiB is the practical security boundary for `.sed`, autosave, and recovery.

`edit` additionally uses its streaming limits above. `hist` uses the application-level limits above: 2,000,000 CBOR nodes, depth 64, 8 KiB byte strings, 3 MiB
text items, 100,000 persisted combined undo/redo timeline entries (independent
of the GUI's 1,024-entry live cap), 1,000,000 pooled fragments, 64 MiB cumulative pool bytes, 300,000 references per
state, 128 MiB per transient Level and 768 MiB/32-million-item aggregate compact retained-snapshot storage. Reconstruction work remains independently bounded from the
encoded payload before any nested Level is retained.

Canonical implementation: [sed.hpp](../../src/core/include/sed/assets/sed.hpp) and
[sed.cpp](../../src/core/src/assets/sed.cpp). Application state integration:
[file_menu.cpp](../../src/app/src/main_window/menus/file_menu.cpp) and
[startup.cpp](../../src/app/src/main_window/project/startup.cpp). Strict-first direct-open classification is isolated
in [direct_sed_open_utils.cpp](../../src/app/src/ui/project/direct_sed_open_utils.cpp). Codec coverage is in
[sed_level_tests.cpp](../../src/core/tests/assets/sed_level_tests.cpp); direct
transaction/project-choice/source-preservation coverage is in
[main_window_event_filter_tests.cpp](../../src/app/tests/main_window_event_filter_tests.cpp). Configure
`-DSED_BUILD_APP=OFF -DSED_BUILD_FUZZERS=ON` with a
Clang GNU-style driver to build the ASan/UBSan/libFuzzer target `sed_file_fuzzer`; its focused target instruments the
native codec plus the minimal Level support rather than unrelated editor geometry. The harness ignores only expected
`std::runtime_error` format/integrity rejection; allocation, length, logic, sanitizer, and other unexpected failures
remain findings.
