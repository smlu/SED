# SED crash-recovery container (`.sedrecovery`)

## Overview

`.sedrecovery` is a crash-discovery and autosave envelope around exactly one authoritative native SED V1 through V4
document. The raw `docm` payload is byte-for-byte a `.sed` containing the current lossless Level and recovery-required
`edit` workspace/COG/resource-editor state, normally with the `hist` undo/redo timeline. If `hist` encoding, size
validation, or semantic verification fails, the writer instead publishes Level+`edit` in that one `docm` and persists
the degradation in `warn`.
Recovery does not define a second document serialization.

Native `docm` version selection follows the Level's explicit UV coordinate-space tag and retained MotS arch-lighting
objects or authored bake baselines. Normalized Levels without either use native SED V2; normalized MotS Levels
retaining either use V3. Explicitly tagged `TexturePixels` Levels use V1 without either or V4 when either remains.
V3/V4 contain the table field, possibly empty, followed by the optional identified bake-provenance trailer described
in the [native `thng` schema](sed_binary_format.md#thng). Recovery readers accept all four native versions; the
outer recovery-envelope schema itself remains V1.

The native document already compresses and parity-protects every semantic section independently. The outer envelope does
not compress or parity-wrap `docm` again. Its remaining responsibilities are recovery-directory discovery, source and
session identity, capture-time metadata, scan anchors, forensic retention, and atomic autosave publication.

The outer `meta` payload uses a fixed two-parity P/Q code because it is one small, atomic association record with a
two-shard repair requirement. XOR P plus independently weighted GF(256) Q repairs any two CRC-localized shard erasures
with less format and matrix machinery than the native document's variable-tier Vandermonde code. Mirrored tables and
SHA-256 then verify the reconstructed claim. The complete rationale is under
[Why metadata uses P/Q](#why-metadata-uses-pq).

The enclosed native bytes follow the V1-through-V4 contract in
[sed_binary_format.md](sed_binary_format.md#overview). Implementations must pass the native-codec and recovery-envelope
conformance suites; passing tests does not authorize alternate native layouts.

Every structural identifier is lowercase at every container level: recovery envelope/chunk/FEC anchors, `meta`, `docm`,
and `warn`, plus every native SED anchor and semantic section inside `docm`. JSON/CBOR property names such as
`sessionId` are schema fields rather than section identifiers and retain the exact spelling documented below.

```text
  +-----------------------------------------------------------+
  | sed_recovery_v1\n                                         |
  | <metadata-json-size>\n                                    |
  | <64 lowercase SHA-256 hex characters>\n                   |
  | primary metadata JSON                                     |
  +-----------------------------------------------------------+
  | meta protected chunk | mirrored metadata                  |
  +-----------------------------------------------------------+
  | docm raw chunk       | one native SED V1 through V4 document |
  +-----------------------------------------------------------+
  | warn raw/compressed chunk (only for degraded Level+edit)  |
  +-----------------------------------------------------------+
```

## Contents

- [Parity algorithms at a glance](#parity-algorithms-at-a-glance)
- [Primary metadata envelope](#primary-metadata-envelope)
- [Live recovery and autosave filenames](#live-recovery-and-autosave-filenames)
- [Chunk envelope](#chunk-envelope)
- [Protected metadata payload](#protected-metadata-payload)
  - [P/Q parity algorithm](#pq-parity-algorithm)
    - [Why metadata uses P/Q](#why-metadata-uses-pq)
  - [Table arbitration and verification](#table-arbitration-and-verification)
- [What is stored](#what-is-stored)
  - [Authority and validation](#authority-and-validation)
    - [`docm`](#docm)
    - [`meta`](#meta)
    - [`warn`](#warn)
  - [Conformance coverage](#conformance-coverage)
- [Self-healing and excessive corruption](#self-healing-and-excessive-corruption)
  - [Native repair and section loss](#native-repair-and-section-loss)
  - [Metadata binding](#metadata-binding)
  - [Framing recovery and bounded scanning](#framing-recovery-and-bounded-scanning)
  - [Forensic retention and audit](#forensic-retention-and-audit)
- [Non-authoritative partial-Level text salvage](#non-authoritative-partial-level-text-salvage)
- [Resilient storage reads](#resilient-storage-reads)
- [Atomic publication and abnormal termination](#atomic-publication-and-abnormal-termination)
  - [Writer and generation gating](#writer-and-generation-gating)
  - [Recovery application transaction](#recovery-application-transaction)
  - [Live checkpoint, prompt eligibility, and autosave history](#live-checkpoint-prompt-eligibility-and-autosave-history)
- [Limits and implementation](#limits-and-implementation)

## Parity algorithms at a glance

The recovery file contains two different protection layers; they must not be confused or applied twice.

| Protected data | Parity algorithm | Repair budget |
|---|---|---|
| Native sections inside raw `docm` | Systematic Reed–Solomon-style Vandermonde MDS over GF(256), primitive polynomial `0x11d` | Four erasures per stripe for required Level sections and `edit`; two for `hist` |
| Outer `meta` association data | Systematic Reed–Solomon-style P/Q over GF(256), primitive polynomial `0x11d`; P is XOR and Q uses coefficient `2^i` for data shard `i` | Any two missing or corrupt metadata shards |

The outer P/Q layer protects only `meta`. It never protects or repairs `docm`, whose native sections already own their
CRC32C, SHA-256, compression, and MDS protection. The exact outer algorithm is specified under
[P/Q parity algorithm](#pq-parity-algorithm); native parity is specified in
[SED binary format](sed_binary_format.md#systematic-vandermonde-mds-parity-algorithm).

## Primary metadata envelope

The compact UTF-8 JSON object has these required V1 fields. Readers ignore
unknown additional fields so an older reader can still discover a recovery
created by a compatible extension; unknown fields do not override or weaken
validation of any required field:

| Field | JSON type | Meaning |
|---|---|---|
| `version` | number | recovery-envelope schema version, exactly `1` |
| `identity` | string | normalized source identity or untitled-session identity |
| `sessionId` | string | stable editing-session identifier |
| `sourcePath` | string | source path, or empty for an untitled document |
| `displayName` | string | user-facing document name |
| `timestamp` | decimal string | snapshot creation time as strictly positive UTC Unix epoch milliseconds |
| `sourceModifiedAtMs` | decimal string | source modification time, or `-1` when absent/unavailable |
| `undoSignature` | unsigned decimal string | captured undo-stack identity |
| `requiresUserRecovery` | Boolean | automatic recovery-offer eligibility |
| `projectType` | number | JKDF2, MotS, or IJIM project discriminator |
| `levelSha256` | string | canonical required-Level binding |
| `documentSha256` | string | complete native `docm` binding |
| `payloadFormat` | string | exactly `sed-v1-document-recovery`; this names the outer V1 envelope, not the native `docm` version |

Required fields must be present with the specified JSON types. In particular, `sourcePath` and `displayName` must be
strings; a missing, Boolean, or numeric value is not defaulted to an empty string. Empty `sourcePath` is valid only
for the matching untitled identity, and an intentionally empty string `displayName` remains admitted.

The preceding hexadecimal SHA-256 covers the exact JSON bytes. Its spelling, and both JSON digest values, must be exactly
64 lowercase hexadecimal characters; uppercase or otherwise non-canonical spellings are rejected even if they decode to
the same 32 bytes. The same JSON is repeated in the parity-protected `meta` chunk so damage to the text envelope does not
orphan an otherwise healthy document.

`identity` is `file:` plus the normalized absolute source path for a sourced document, or `untitled:` plus its stable
session ID for an unsaved document. On Windows the normalized path is case-folded. `sessionId` distinguishes concurrent
editing sessions for the same source; `sourceModifiedAtMs` lets recovery report that the source changed after capture.
Both metadata time fields are signed 64-bit Unix epoch milliseconds in UTC and therefore avoid the rollover limits of
common 32-bit timestamp representations. `timestamp` is the recovery/autosave snapshot creation time and must be
positive, while `sourceModifiedAtMs` is only the external source-file baseline and uses `-1` when no source-file time is
available. The enclosed native `docm` carries the same `timestamp`, because the recovery writer creates that native file
image at capture time. If both checked native header copies are lost, the parity-protected envelope `timestamp`
reconstructs the native timestamp without a duplicate creation-time field; a surviving native value remains primary.
Metadata helps choose and label a recovery but never overrides the checked project type or native version inside
`docm`. Its protected digests bind that association to document content: `levelSha256` covers only the canonical eight
required-Level-section `UnprotectedNested` SED bytes in the Level's native V1-through-V4 coordinate-space version; it
deliberately excludes optional core-owned `jimp`, so permitted loss of that provenance cannot invalidate a recovered
Level. `documentSha256` covers the complete canonical protected `docm`, including `jimp`, `edit`, and any emitted
`hist`.

Directory discovery is deliberately metadata-only: it reads bounded primary-envelope lines/JSON and at most a 4 MiB
protected `meta` candidate within the first bounded metadata region. It seeks past and never materializes, decompresses,
or decodes `docm`, `edit`, or `hist`. A valid primary copy remains discoverable when `meta` is missing, damaged, or
oversized; a valid `meta` remains discoverable when the primary is damaged. When both copies validate, all association
and recency/binding fields (`identity`, `sessionId`, normalized `sourcePath`, `displayName`, both timestamps,
`undoSignature`, `requiresUserRecovery`, `projectType`, `levelSha256`, and `documentSha256`) must agree. A valid disagreement is a hard safe
failure, and the chosen identity must cohere with its normalized source/session. One malformed or allocation-hostile file
is isolated and cannot abort scanning its siblings.

## Live recovery and autosave filenames

Recovery filenames are intentionally readable and collision-resistant:

```text
<sanitized-display-name>-<project-tag>-recovery-<family-token>.sedrecovery
```

The project tag is `jkdf2`, `mots`, or `ijim`. `<family-token>` is the first 128 bits of a domain-separated SHA-256
over the length-prefixed normalized source identity and stable session ID, encoded as 26 lowercase base32 characters.
This single combined token remains collision-resistant while keeping filenames short. Lowercase is required so
case-insensitive filesystems cannot equate distinct tokens. The readable basename and explicit `recovery` marker make
a live slot recognizable while browsing; identity/session decisions use the full protected META, never a filename
prefix or token alone.

## Chunk envelope

Each chunk starts with this 68-byte little-endian header:

| Relative offset | Size | Meaning |
|---:|---:|---|
| 0 | 8 | `sedchn1\n` |
| 8 | 4 | type: `meta`, `docm`, or `warn` |
| 12 | 4 | flags: bit 0 zlib-compressed, bit 1 outer parity-protected, bit 2 native SED |
| 16 | 8 | stored payload size |
| 24 | 8 | raw payload size |
| 32 | 4 | CRC-32 of stored payload |
| 36 | 32 | SHA-256 of raw payload |
| 68 | variable | stored payload |

Compressed and protected flags are mutually exclusive. Canonical flags are `meta = 2`, `docm = 4`, and `warn = 0` or
`1`. `docm` stores identical raw/stored bytes, sets the outer CRC field to zero, and fills the outer SHA field with 32
zero bytes. Those zeros are required markers, not missing validation: native SED sections already own compression,
CRC32C, SHA-256, and MDS protection. `warn`, when present, uses Qt zlib compression only when smaller. Readers can scan
for `sedchn1\n` after damaged bytes and recognize independent native `sed\0`, `sectv1\n\0`, `sedmir1\n`, `mhdr`, and
`sedend\r\n` anchors inside `docm`. There is no whole-container checksum.

The writer emits exactly one `meta`, one `docm`, and at most one `warn`. A reader admits at most two plausible
`meta`/`docm` candidates so one damaged candidate can be compared with one redundant candidate, and materializes at
most one `warn`. Byte-identical duplicate `docm` is recognized without another native decode; a nonidentical candidate
is charged to the shared native-validation work budget. Further duplicates are skipped and reported as corruption.

Outer CRC-32 fields use the reflected IEEE polynomial `0xedb88320`, initial value `0xffffffff`, and final complement.
They are intentionally not native SED's CRC32C.

## Protected metadata payload

The `meta` payload uses the outer V1 P/Q format:

```text
  u64 primaryTableSize
  primary FEC table
  u32 CRC-32(primary table)
  data shards
  P shard
  Q shard
  mirrored FEC table
  u32 CRC-32(mirrored table)
  u64 mirroredTableSize
  "fecend1\n"
```

The FEC table is:

```text
"sedfec1\n"
u32 version = 1
u32 protectedUnitCount = 1
u64 totalRawSize
byte totalRawSha256[32]
u64 rawSize
u64 compressedSize
u32 dataShardCount
u32 shardSize
byte rawSha256[32]
u32 shardCrc32[dataShardCount + 2]
```

### P/Q parity algorithm

Metadata is compressed once with `qCompress(..., 6)` before sharding, even when compression does not reduce this small
payload. Its data-shard count is `min(16, max(1, compressedSize))` and each shard is
`ceil(compressedSize / dataShardCount)` bytes. Exactly two parity shards are added. P is the bytewise XOR of the data
shards. Q is a weighted sum in GF(256), using primitive polynomial `0x11d` and a distinct nonzero coefficient for every
data shard (`coefficient[i] = 2^i` in that field). IEEE CRC-32 identifies shard erasures; P/Q reconstructs up to two; the
unit and complete-payload SHA-256 values verify the decoded metadata. The table is checked and mirrored so `meta` can
also be found and decoded without its outer chunk header. Before `qUncompress`, the reader requires Qt's four-byte
declared raw-size prefix to equal `rawSize`.

#### Why metadata uses P/Q

All association fields must be accepted or rejected together, and the payload is small enough that a fixed two-erasure
budget is sufficient. P/Q was chosen because the bytewise XOR P shard repairs any one erasure cheaply, while the
independently weighted Q shard supplies the second equation needed for any two erasures. It therefore reaches the
required two-shard guarantee without the adaptive generator-matrix and parity-tier machinery needed by large native SED
sections. A single XOR shard would not survive two losses, replication would consume more space, and wrapping `docm`
would duplicate its existing native MDS. IEEE CRC-32 localizes bad shards, the mirrored tables preserve their layout,
and per-unit plus total SHA-256 prevent a CRC-consistent but semantically wrong reconstruction. The implementation is in
[document_recovery_utils.cpp](../../src/app/src/ui/recovery/document_recovery_utils.cpp).

### Table arbitration and verification

The decoder arbitrates the primary and mirrored tables rather than silently preferring one. Equal CRC-valid tables use
the common description. If only one validates, it is used and the damaged/missing copy is reported. If two CRC-valid
tables disagree, each is decoded independently through shard CRCs, P/Q repair, expanded-unit SHA-256, and total SHA-256;
the reader accepts a uniquely valid result, or two results that reconstruct identical metadata, with a corruption
warning. It rejects the container when no unique checked result exists. For every accepted candidate, the end of shard
data must equal the computed start of the mirrored table exactly; a gap, overlap, or unexpected table/footer boundary is
not normalized or ignored.

Tiny native Level domains are not folded together merely to reduce slot overhead: each stays independently recoverable
inside `docm`. By contrast, the small envelope association fields form one atomic claim and are intentionally grouped in
the single `meta` unit so source/session identity, timestamps, project type, and both document bindings cannot be repaired
or selected independently.

This outer metadata scheme is intentionally different from native SED V1 through V4:

| Domain | Compression/integrity/recovery |
|---|---|
| native section inside `docm` | V1/V4 mean texture-pixel UVs/scales and V2/V3 mean normalized UVs/scales; V3/V4 also carry the MotS arch-lighting table field and optional bake-provenance trailer. Otherwise consider fast LZSS exactly once only for raw sizes from 4 KiB through 64 MiB and retain only if smaller; fixed 4096-byte slots with trailing CRC32C; no FEC tables; one expanded SHA-256; systematic striped Vandermonde/MDS with `p=4` for required Level sections, core-owned `jimp`, and `edit`, and `p=2` for `hist`/future lower-priority optional sections |
| outer `meta` | `qCompress` once; variable equal-sized shards with IEEE CRC-32; mirrored `sedfec1\n` tables; P/Q with two parity shards; per-unit and total SHA-256 |
| outer `docm` header | raw native bytes, native flag, equal sizes, zero outer CRC/SHA; no duplicate compression or parity |
| outer `warn` | raw or `qCompress` if smaller, with outer IEEE CRC-32 and raw SHA-256 |

Nested Levels inside `hist` use the explicit native `HistoryNested` profile and carry the native version selected by the
Level's UV coordinate-space tag. They retain checked descriptor mirrors and payload CRC32C, but no per-state SHA-256,
compression, or parity; every nested Level contains exactly the eight required sections and no `jimp`. The enclosing
native `hist` section owns the strong expanded SHA-256/compression/parity layer once. Its application payload remains
schema V1 in all native SED versions and uses the persisted `fragmentLayout` discriminator: legacy seven-key V1 payloads use fixed-offset 8192-byte fragments, while new payloads use
`sect-record-v1` for the large `sect` frame and fixed blocks for the other sections, as specified in
[sed_binary_format.md](sed_binary_format.md). The record-oriented
layout is bounded to 300,000 references per reconstructed state; the writer checks the counted SECT records before
reserving their span list and deterministically emits explicit `fixed-v1` when bridge/record fragments would exceed that
limit. `hist` is capped at 64 MiB
expanded payload bytes; before Qt materializes the object graph, bounded-scratch CBOR traversal caps it at 2,000,000
nodes, depth 64, 8 KiB byte strings, and 3 MiB text items. Pooled/reconstructed history limits are enforced before each
allocation.

Native SED `p=4`/`p=2` protection covers only the fixed-slot payload of each top-level semantic section. The 28-byte
native header and 100-byte section descriptors instead use independent CRC32C-checked mirrors; `codedOffset` alignment
padding is canonical zero (strict rejects nonzero bytes; corruption recovery normalizes/reports them);
the expanded SHA-256 covers only decoded semantic bytes; and the footer is an unchecked marker. The exact byte boundaries
are tabulated in [sed_binary_format.md](sed_binary_format.md#exact-protection-boundaries). Outer `meta` P/Q never extends
into `docm`, so it cannot spend parity to repair any native header, descriptor, section, `edit`, or `hist` byte.

If pending JKL COG import provenance is represented in the current Level or any retained undo/redo state, HIST may add
`jklCogImportV1: {version: 1, current, undo, redo}`. `current` is explicitly `[rowsPresentBool, [[sectorI32, surfaceI32], ...]]`
and must match normalized metadata in the authoritative current Level. `undo` and `redo` are sparse arrays of
`[stateIndex, rowsPresentBool, refs]`, sorted by unique ascending stack index; a missing index means empty state, never
inheritance. Each state's Surface references preserve their source order. False-marker unused maps normalize to empty,
and the extension is omitted when no represented state exists. Malformed known metadata rejects HIST as one optional
history transaction; it cannot invalidate the current Level. Older applications may drop this optional addition.

## What is stored

### Authority and validation

#### `docm`

Exactly one raw native SED V1 through V4 document emitted by the writer. A `Normalized` Level emits V2 without
retained ARCH objects or bake baselines, or V3 when either remains; an explicitly `TexturePixels` Level emits V1
without either or V4 when MotS objects or baselines remain.
It is the sole authoritative document payload and is not given a duplicate outer CRC or
SHA-256. An optional core-owned `jimp` section inside `docm` retains meaningful pending JKL COG import metadata; ordinary
Levels without represented import state omit it and retain their established bytes. Strict native decoding rejects
malformed known `jimp`; permissive corruption recovery can clear only its marker/map and warn while keeping the checked
Level. The separate app-owned HIST `jklCogImportV1` map preserves represented current and sparse undo/redo state; its
nested `HistoryNested` Levels remain exactly eight required sections and never carry `jimp`. Represented Surface maps
repeat in each affected HIST state and share its existing aggregate-reference, CBOR, and size budgets; large pending
unavailable-script imports can exceed them. The writer then omits optional HIST with a warning while retaining the
current Level, represented `jimp`, and checked EDIT. Neither extension bumps a
native or recovery-envelope version.

Recovery application first restores the coordinate-space version captured in `docm`; it does not reinterpret a legacy
pixel-space recovery as normalized. After resources and the recovered document are installed, a recovered V1 or V4 schedules the same
generation-checked automatic/Ask/Keep compatibility evaluation used by direct native `.sed` Open. When that policy
normalizes legacy pixel space, its document transaction converts the current Level and every UV-bearing HIST state
together before any new normalized recovery snapshot can be published. A failed or stale conversion therefore leaves
one complete pixel-space document and
cannot produce a mixed-version recovery timeline. See
[`uv_promotion.cpp`](../../src/core/src/editor/uv_promotion.cpp) and the
[native format overview](../sed_format.md#legacy-pixel-space-uv-admission-and-saving).

#### `meta`

One protected copy of the primary JSON metadata, including the canonical required-Level and complete-document SHA-256
bindings used for discovery, identity, anti-transplant validation, and audit context.

#### `warn`

The optional persisted explanation that `hist` encoding or size/envelope validation failed and the writer deliberately
published the authoritative Level+`edit` checkpoint without that lower-priority timeline.

The `edit` inside `docm` preserves unsaved COG source exactly, including quoted punctuation, and resource-editor draft
state, but not COG diagnostics derived from source or resource preview output. Resource-thumbnail images and parsed
preview caches, filesystem-watch registrations, viewport pick acceleration data, and the current repeated-click cycle
are transient and are rebuilt after recovery. They are not
separate recovery chunks and do not affect the V1 recovery schema.

COG tabs in EDIT may retain their optional `acceptedSourceHash` exact-byte external baseline (unknown empty byte
string or 32-byte SHA-256); malformed present values reject the workspace atomically. Restored tabs
without a source baseline use the conservative ordinary-Save conflict choice. Watcher Keep does not rebase that hash.
This is the [shared native EDIT contract](sed_binary_format.md#workspace-schema-and-validation), not new outer META.

The shared EDIT `surfaceSelectionContext` extension retains the frozen Strict owner scope and remembered Face
selection independently of current components, including intentionally empty scopes. Its checked V1 map, absent-only
legacy inference, and topology invalidation follow the
[native workspace contract](sed_binary_format.md#workspace-schema-and-validation); the outer envelope gains no
duplicate selection field or new version.

Frame workspace selections and HIST selections remain paired with their exact Level images. The production recovery
round trip preserves UI-added frame selections together with both undo and redo branches; no outer recovery field is
needed for frame identity. Manual native Save additionally maps validated live selected/active frame ordinals into its
normalized candidate before encoding EDIT and HIST, as specified in the
[native workspace contract](sed_binary_format.md#workspace-schema-and-validation).

There are no separate outer Level, editor-state, or history payloads. This prevents their copies from disagreeing and
avoids compressing or parity-protecting the same semantic bytes twice. Before opening the staging file, the writer
round-trips the generated native SED document, compares every serialized current-Level field and every optional
type/payload, requires a deterministic canonical current-Level re-encoding, and self-checks each outer chunk.
The Level comparison uses the same isolated-snapshot MotS dormant-zero ARCH filtering and retained-index remapping as
the native writer (`sedLevelNativeSerializationMatches`), rather than demanding equality with the unfiltered request.
All other represented authored fields remain checked; verification never mutates the live/request Level.
Every generated HIST state has already passed the core writer preflight and exact framing checks; its bounded canonical envelope is checked
without immediately replaying the complete optional timeline. Open/recovery performs the strict semantic HIST decode
before installation. A `hist` encode/envelope failure follows the explicit
Level+`edit`+`warn` path; failure to encode or verify Level, `edit`, `meta`, or `docm` publishes nothing. An encoder
regression therefore cannot replace the previous good checkpoint.

### Conformance coverage

Conformance coverage uses the checked-in native and text goldens listed in
[the SED vector manifest](../../src/core/tests/assets/fixtures/sed_vectors/README.md). For JKDF2, MotS, and IJIM, the
production `writeRecoverySnapshot` path wraps the checked-in native Level in a fresh autosave/recovery envelope; reopening
it must reproduce the exact checked-in normal JKL/NDY export. The test then damages five independently checked `sect`
slots inside `docm`. Recovery must report `sect` as lost, retain the independently checked Level domains, and reproduce
the exact checked-in partial JKL/NDY export through the real text-salvage writer. The all-resource-category native vector
also round-trips through this same path.

Autosave and crash recovery require no duplicate container schema. The active hard-kill checkpoint is published as
`.sedrecovery`; periodic history atomically archives an already checked prior container under the `.sedautosave`
extension. Both outer containers are decoded by the same recovery-envelope V1 reader, whose native `docm` path accepts
SED V1 through V4. `writeRecoverySnapshot` publishes the live slot and performs the
archive-first rotation when requested.

```text
checked-in *.sed --load--> DocumentState
                            |
                            +-- writeRecoverySnapshot (autosave and recovery)
                            v
                    fresh *.sedrecovery
                            |
             +--------------+----------------+
             |                               |
          reopen                         damage docm/sect
             |                               |
             v                               v
 normal JKL/NDY == golden       partial JKL/NDY == partial golden
```

The complete outer bytes are deliberately not a checked-in golden because `timestamp`, source/session identity, and
their protected bindings are input-dependent. The stable contract is tested at both boundaries: checked-in authoritative
`docm` Level semantics and checked-in engine-text output, while separate envelope tests validate metadata, chunk flags,
binding digests, parity, atomic publication, and corruption scanning.

Readers treat the first valid native document as the candidate, but duplicates never create an implicit fallback chain.
An additional valid `docm` that resolves to the identical canonical document is ignored with an explicit corruption
warning; two conflicting valid `docm` payloads reject the recovery safely. An invalid duplicate is skipped and reported.

## Self-healing and excessive corruption

### Native repair and section loss

The reader first asks the native codec to validate `docm`. Each required Level section, `jimp`, and `edit` repairs up to four
erasures in each independent stripe; `hist` and future lower-priority optional sections repair up to two. Thus many bad
slots can be repaired across a large section when no individual stripe exceeds its tier. More than four bad slots in one
core/`edit` stripe, or more than two in one `hist` stripe, makes that whole semantic section unrepairable. The expanded
SHA-256 catches CRC-consistent wrong data but cannot localize the bad slot, so it detects rather than repairs that case.
After reconstruction the native reader also recomputes every CRC-valid parity row; wrong parity with a recomputed CRC is
reported as physical corruption without discarding systematic data that still passes the semantic SHA-256.

Recovery-only loading returns structured repair/loss state and may retain independently valid sections after another
section exceeds its budget. A missing required Level section is defaulted as a whole and named; an unrepairable `edit` is
omitted and workspace restoration falls back to defaults; an unrepairable `hist` is omitted and the Level opens without
that timeline; an unrepairable `jimp` clears only the pending import marker/map. Unidentified framing loss and persistently unreadable byte ranges are reported separately. No partial
record within a lost section is decoded, no missing value is guessed, and surviving cross-section references can be
dangling. A checksum-valid required Level schema/semantic error remains a hard failure rather than permission to discard
core data. Application-owned optional payloads have their own all-or-nothing boundary: invalid `hist` is omitted with a
warning, and invalid `edit` is never partly applied but falls back to a clean/default workspace with a warning.
Native SED versions deliberately have no descriptor directory or record-group shards: their checked unit is the complete
semantic section, and a stripe beyond budget drops that unit as a whole while later checked section anchors remain
recoverable.

### Metadata binding

After `docm` decoding, protected metadata binding is mandatory whenever its canonical comparison is available. The
reader hashes a canonical eight-required-section-only `UnprotectedNested` encoding and rejects a mismatch with
`levelSha256`; optional `jimp` is not part of this digest. The authored Level state hash used by the GUI and recovery
worker, and the current-state and transition digests used to bind HIST history, conditionally append the same
domain-separated JKL import-state suffix when represented import metadata is present. The suffix is the ASCII domain
`SED-JKL-COG-IMPORT-V1`, a canonical marker byte, a little-endian u32 reference count, then ordered signed little-endian
i32 sector/surface pairs. Empty/unused import provenance appends nothing, preserving the existing authored/HIST digest
values. When all semantic sections survive, the reader also hashes the complete canonical protected document and
requires `documentSha256`. If an identified optional section was lost, a complete canonical `docm` cannot be
reconstructed, so the full binding is explicitly unavailable and reported; recovery proceeds only after the surviving
required Level still passes its independent binding. Required-Level loss can likewise make that binding unverifiable
and is never presented as a fully bound pristine document.

### Framing recovery and bounded scanning

If the outer `docm` chunk header is damaged, the reader scans for the canonical native SED anchors and validates the
recovered document from its own header/mirror, section descriptors, hashes, parity, and footer. If the primary native
fixed-header fields or CRC are damaged while the exact `sed\0` anchor remains locatable, its checked mirror supplies the
exact header. Outer header-loss scanning itself requires that native anchor; direct native `.sed` recovery has the broader
header/section resynchronization behavior documented separately: it reuses a surviving checked trailing header mirror
verbatim, or, when that mirror is also absent, synthesizes self-consistent framing around the checked section chain and
lets a surviving `head` supply the project type. Valid original header framing that disagrees with `head` is still a hard
failure. Native recovery reports project identity separately from its placeholder `Level.kind`: a surviving checked
header or `head` yields `projectType` with `projectTypeTrusted=true`; when both header copies and `head` are lost,
`projectType` is empty and `projectTypeTrusted=false`. No recovery caller may silently reinterpret that placeholder as
JKDF2. It must obtain an explicit project choice before project-dependent history/resource/export work or keep the type
unknown and block native text export.

Damaged-envelope fallback is explicitly work-bounded. The parser accepts at most 64 outer chunks, validates at most 64
outer boundary candidates after at most 8,192 marker probes, and limits each damaged-docm, native-signature, and checked
section-anchor recovery stage to bounded candidate counts and no more than twice the container byte count of cumulative
native decode work. Failed candidates are non-owning views into the staged container; only the accepted document is
copied. `meta`, `docm`, and `warn` type-specific size limits are checked before any payload copy or `qUncompress` call.
The canonical docm path follows the same rule: it validates a non-owning range view and creates its sole owned docm copy
only after native acceptance.

Full recovery admission allocates the initially checked file size once, reads it in 1 MiB blocks, and then probes exactly one byte beyond
that boundary. A short read or concurrent growth is rejected rather than treating a moving file as one snapshot. Only a
genuine device error activates the 4096-byte resilient reread; the rereader verifies the file size before and after each
range and releases the failed ordinary allocation before allocating its destination. Full parsing accepts only this
bounded immutable staging buffer, so no `readAll()` path can bypass the container ceiling.

Within `docm`, every native section has checked primary and mirrored descriptors. When both descriptors for one section
are unusable, recovery scans to the next independently checked boundary. That boundary is either a validated
`sectv1\n\0` descriptor with its matching `sedmir1\n` trailer, or a validated mirror whose embedded descriptor,
stored-size extent, and absolute 4 KiB phase derive one exact section start. This second route retains a following section
when one localized overwrite destroys the preceding trailer and overlaps that section's primary descriptor. This
resynchronization is why corrupt section framing does not force unchecked size arithmetic or discard the remainder of an
otherwise recoverable native document.

### Forensic retention and audit

Partial or repaired input is never silently treated as pristine. The audit log records metadata recovery, native slot and
outer-meta shard repairs, missing sections, unreadable ranges, warnings, retention, and republishing. On a successful
repaired/partial load, the selected damaged snapshot is copied to a unique `.corrupted-<timestamp>-<UUID>.sedrecovery`
forensic path while the original discoverable slot remains available. A separate clean detached checkpoint is then
staged, verified, and atomically published. Only after that clean publication succeeds is the redundant unmarked damaged
slot retired, preventing the same startup offer from recurring; the byte-identical marked forensic copy is not deleted or
returned by ordinary recovery discovery. If clean publication fails, the unmarked damaged slot remains discoverable.
Untrusted diagnostic text is escaped before it reaches either the audit log or partial-salvage manifest: C0/C1 and
terminal controls, Unicode line separators, bidirectional controls, and other format characters become visible
`\\uXXXX` sequences. Expansion is charged against the artifact cap, so escaping cannot create a second output-amplification
path.

## Non-authoritative partial-Level text salvage

Only structured loss of a required Level section (`head`, `rsrc`, `sect`, `thng`, `tmpl`, `cogs`, `lite`, or `hnod`) or
loss of a unit whose checked framing no longer identifies its type schedules non-authoritative JKL/NDY plus a manifest in
the background beside the retained forensic input. The latter manifest records the exact unidentified count, cannot
invent names or a required/optional mapping, and warns that any unit may have contained required Level data. Loss
confined to identified optional `jimp`/`edit`/`hist`, or a completely repaired document, does not schedule either salvage
artifact. Recovery does not wait for it.

The artifact is explicitly non-authoritative. For a checked or explicitly user-selected project, SED exports the already
recovered/defaulted Level as project-native JKL for JKDF2/MotS or NDY for IJIM, plus a plain-text manifest naming the
selected and forensic inputs, project provenance, exact missing required sections, recovery warnings, surviving-data
inventory, and limitations. If project identity is still unknown, native JKL/NDY export is intentionally blocked and the
manifest records why. The generated native text is retained as emitted by the ordinary readable JKL/NDY writer with
`compact=false`; salvage never uses the optional compact export presentation and is therefore suitable for manual
inspection/editing. It is not replaced by a parse/re-save normalization. The background worker converts only a private
copy of a normalized recovered Level when the selected legacy JKL writer requires texture-pixel UVs. That conversion
uses the immutable resource-locator snapshot captured for the accepted Open/recovery transaction, including the actual
MAT dimensions; it never mutates the recovered Level installed in the editor. If a required MAT dimension cannot be
resolved, native text publication fails atomically and the manifest records the reason. Before publication the native
text must be nonempty and at most 256 MiB, contain no NUL or line over 4 MiB, have exactly one of each required native
section anchor and valid project markers/version, parse using the checked project context, and produce a structurally
valid second semantic re-save. A MotS pixel-space SED document's otherwise ambiguous JKL project identity is accepted
only with the checked SED project context and `ARCHLIGHTING` marker.

Both artifacts use same-directory durable staging, collision-safe names, and atomic no-replace publication. Existing or
concurrently created targets are never overwritten. If native export or validation fails, the manifest is still attempted
and records that failure. Only fully decoded semantic domains are exported; an unrecoverable section contributes only
its documented default/empty domain, never guessed or copied residual bytes. JKL/NDY cannot preserve all binary SED
precision, workspace/COG buffers, or history, and defaulted collections can leave dangling references. The text and
manifest are diagnostic aids only; the unchanged binary forensic input remains the only source for bytes inside an
unrecoverable section.

Crash recovery writes beside its retained `.corrupted-*` forensic copy. Direct corrupted `.sed` Open has no need to copy
or rewrite the selected source: required or unidentified loss opens only after confirmation as detached `Untitled*`
(because an unidentified unit may conceal required Level data), and salvage is written beside that unchanged source;
unidentified-only loss also produces decoded-domain text plus the explicit
uncertainty manifest. Its background worker re-reads only after checking file size/write time and then
requires the normalized missing-required set, unidentified-loss count, and trusted/unknown project classification to
match the accepted load. A replacement or reclassification aborts salvage instead of publishing under stale warnings;
an explicit unknown-project choice is reapplied to the re-read Level before choosing JKL versus NDY.

## Resilient storage reads

The reader first attempts one ordinary complete file read. If the device reports a short/I/O read, production recovery
reopens the file and retries each independent 4096-byte range up to three times. A persistently unreadable range is
zero-filled, coalesced into structured `unreadableRanges`, and reported as an erasure so later native sections and stripes
can still be processed while checked framing preserves the absolute slot phase. Byte/bit substitutions are likewise
repairable within the per-stripe budget. An ordinary physical EOF truncation is different: the shorter file does not
retain the original extent or slot phase, so missing slot bytes cannot generally be inferred or zero-filled; only narrowly
recognized redundant trailing-framing loss is recoverable that way. A mid-section byte insertion or deletion also shifts
later slot boundaries and is outside all native versions; recovery does not guess or slide to a replacement phase. A complete
read that merely fails validation is not relabeled as an I/O error. The injectable
outer reader bounds alternatives to `512 bytes..1 MiB` and `1..16` attempts.

## Atomic publication and abnormal termination

### Writer and generation gating

Snapshots are staged through `QSaveFile` with direct-write fallback disabled. The old complete checkpoint remains
authoritative until the new file is flushed and atomically committed. A generation gate plus a short publication mutex
prevents an obsolete worker from replacing newer state after Save, document switch, or shutdown. Windows session-shutdown
requests synchronously drain a final snapshot with bounded retries.

No user-mode application can execute code after an unconditional kernel termination has already killed all its threads.
Protection against Task Manager **End task**, process crashes, power loss, and similar events therefore comes from a
durable checkpoint published *before* termination: startup/document-open schedules an initial baseline even without an
edit, and Level changes plus `edit`-only camera/tool/layout changes debounce into newer snapshots. The expensive editor
state capture runs after coalescing rather than once per viewport event. The GUI thread copies the authoritative current
Level and small EDIT values, while undo records share immutable copy-on-write snapshot payloads with the worker. Level
serialization/digests, HIST expansion, compression, FEC, verification, and publication run on a dedicated single-thread
low-priority pool, isolated from interactive resource/audio jobs. Normal
timers refresh as configured. A hard kill can recover only the last checkpoint whose atomic publication already completed; changes after
that boundary can be lost. Crash handlers may request an immediate checkpoint while the process is still alive, but they
cannot improve durability after termination and are not the only checkpoint mechanism.

Checkpoint deduplication uses the coherent undo/redo identities, editor-state digest, and captured mutation/event
revision. The worker returns the canonical Level digest after serialization so completion bookkeeping does not serialize
the Level again on the GUI thread. This distinction is intentional: undoing back to a previously published Level may
leave the window at its clean saved position, but the new redo branch is still recovery data and must cause a replacement
checkpoint.

Editor-only camera, selection, UV-panel, and dock/layout changes do not make the document dirty. While the process is
running they still replace the live crash checkpoint so an abnormal termination can restore the workspace. Such an
editor-only checkpoint has `requiresUserRecovery = false`, is ignored by automatic recovery offers, and is not rotated
into periodic `.sedautosave` history. Authored Level data, unsaved COG/resource buffers, authored pending Multi-cut
changes, or a recovered document still requiring an explicit Save set the flag true. A tool option changing a pending
cut's Apply result participates in its authored projection described below. The current Level is also compared with
the last saved canonical Level digest so an authored path without a distinct undo record cannot be mislabeled
as editor-only state. Modified COG and resource-buffer identity/content has a separate saved-baseline digest: a native
SED Save embeds those buffers in `edit`, so leaving their external editors modified after that Save does not keep the
new live baseline prompt-eligible. Further buffer changes do. Saving, reloading, or explicitly discarding one external
buffer retires only that buffer from the baseline; it cannot bless newer unsaved text in another tab.

The checkpoint preserves the authoritative Level, stable workspace/tool configuration, pending Multi-cut lines and
independent Sector/Surface local Undo/Redo timelines/cursors even while Cleave is inactive or Game Preview owns the viewport, and unsaved
COG/resource-editor buffers. Restoring a dormant draft keeps its saved edit mode and selection; matching Cleave
ownership gates draft input and local Undo/Redo. Checked Apply/Layer workspace attachments
share the native EDIT/HIST representation. An active Surface Extrude preview and a partially held brush/draw or other
pointer gesture are finalized or cancelled at snapshot/apply boundaries rather than replayed after restart.

Authored pending cuts count toward `requiresUserRecovery`, periodic archive rotation, native unsaved-work
close/replacement prompts, and event-driven async Save/replacement fingerprints even when the Level is otherwise saved.
A successful native Save checkpoints both exact captured family drafts as clean without requiring Apply. The authored projection
contains settled line geometry/frames/links, target context, execution order, chosen Chain origin, and Bounded while
lines exist. Camera, current selection, guides, future shape/angle choices, and a history cursor with unchanged authored
lines do not make the document dirty. Clearing or cancelling a saved nonempty draft changes its saved authored state to
empty and can be saved. The cached authored projection visits Sector then Surface in a stable order, so ordinary family
switching alone cannot dirty the document; a parked-family edit still affects prompts, recovery offers, and periodic archives.
Settled authored revisions invalidate that cached projection; no full-Level copy/hash is added to
mouse, title, focus, or camera paths. The quiet editor-only checkpoint rule still applies to camera/settings changes.

### Recovery application transaction

Applying a selected recovery is ordered as a replacement transaction: validation and any project choice complete before
workspace mutation; a failed choice/Level assembly leaves the selected path discoverable for retry; the recovered Level
is installed before deriving its project-tagged slot; every restore forks a fresh editing-session ID so two processes
opening the same record can never publish into one canonical live filename; and the selected/previous checkpoint is
retired only after the replacement workspace finishes successfully and its new live slot is durable. Optional invalid
`edit`/`hist` defaults with warnings and a clean
republish requirement, but does not detach or veto an intact Level. Post-install failures cannot yet roll every Qt/GPU
side effect back, so both the selected and previous durable checkpoints remain unretired until the success boundary.

Saving the source document, switching documents, or ending a session updates recovery eligibility through the generation
gate. Applying recovery never writes the source as part of the recovery operation. A complete recovery, or one with only
optional-section loss, retains its recorded source association and opens modified so a later explicit Save can update that
source. Required-Level or unidentified loss opens detached as `Untitled*`, forcing the user to choose a destination.
Source changes are reported as conflicts, not used to hide a valid checkpoint.

### Live checkpoint, prompt eligibility, and autosave history

Each editing session owns one live `.sedrecovery` path. `meta.requiresUserRecovery` is the checked automatic-prompt
discriminator: it is true for authored Level/COG/resource-buffer or pending-Cleave changes, or a recovered document
that still requires Save. Camera, selection, guides, and other editor-only changes relative to the completed native
Save/load baseline do not set it. A successful native SED Save first
gate-invalidates and removes the stale dirty live offer as soon as the authoritative atomic Level+`edit`+`hist` file is
durable, then publishes a complete clean hard-kill baseline asynchronously with the flag false. This avoids a duplicate
synchronous encode while ensuring an immediate reopen cannot offer pre-Save state. Normal close removes the live slot; a clean slot left by a hard kill is
ignored by automatic offers. It is not deleted without a reliable ownership/liveness proof because it may belong to a
concurrently running editor. Eligible dirty descriptors are prioritized within discovery's retained-candidate cap, so
clean remnants cannot starve real unsaved work. Session-qualified paths remain independent, so publishing one live
session never deletes another session.

Explicit Discard during Open/New Project retires only the selected `(identity, sessionId)` live slot. A normal
application close likewise retires only the live slot—even if the user chose Discard in the close prompt—so periodic
history remains available after restart. No replacement, source reassociation, restore, or shutdown path removes a
`.sedautosave`; only the explicit Delete action in Autosave History and post-publication retention pruning are allowed
to do so. It never removes live or historical checkpoints owned by another concurrently active session for the same
source path. After a startup
Discard, older live sessions for that same document identity are suppressed for the remainder of the current editor run,
so the startup dialog or an immediate file Open cannot repeat the prompt. Those foreign-session files remain visible in
Autosave History and are eligible again after the next process start.

Before periodically replacing an authored dirty live checkpoint, the writer captures its already checked bytes. Under the publication gate it
first atomically publishes those bytes as a `.sedautosave` without pruning, then commits the staged live `QSaveFile`,
and only after both durable outputs exist prunes the oldest history for that `(identity, sessionId)` family. Archive failure leaves
the old live point unchanged. The archive filename is deterministically bound to the 128-bit combined identity/session
family token, a compact UTC capture timestamp, and an 80-bit prefix of the authenticated old `docm` digest. The prefix remains collision-resistant for this local ten-entry history while
keeping the worst-case Windows filename component below 255 UTF-16 code units. Repeated retries after a live-commit
failure atomically refresh the same one archive plus the
unchanged old live point instead of accumulating full-size duplicates. Archive names use
`<sanitized-display-name>-<project-tag>-autosave-<utc-timestamp>-<family-token>-<docm-fingerprint>.sedautosave`;
`<utc-timestamp>` is UTC `yyyyMMddTHHmmssZ`; the 80-bit lowercase-base32 `docm-fingerprint` differentiates changed
checkpoints captured in the same second. Hyphen separators avoid the old embedded `.autosave` dotted suffix while keeping
the source name visible. At most 10 historical points are retained per
`(identity, sessionId)` family, so one editor never prunes a
concurrent session's history. Historical entries contain the
same complete native SED `docm` as live recovery, but automatic discovery never offers them. `File > Open Recent > Autosave History`
is the explicit browser for opening or permanently deleting them; deletion requires a timestamp/source confirmation.
Retention traversal is a bounded filename-digest and authenticated-metadata scan for the exact `(identity, sessionId)`
family and runs after releasing the live publication mutex. Cached UI/family descriptors are compare-before-delete
fingerprints: SED first moves the exact directory entry aside, authenticates its protected metadata, and deletes only a
match. This applies to the current live slot as well as history; invalidating a stale current-slot row re-arms publication
because the gate may have superseded a staged newer capture. A different atomic replacement is restored/preserved rather
than erased. Compare-before-delete quarantine/preservation sibling names are likewise length-bounded; quarantine uses a
non-recovery suffix so discovery cannot offer an entry during authentication.
Periodic ticks never rotate a clean or editor-only live checkpoint, even if the preceding published checkpoint was dirty
before an Undo-to-clean transition.

Opening a level with matching recovery is a two-phase replacement. The target dialog only selects Recover, Open Saved,
or Cancel. SED then resolves Save/Discard/Cancel for the current dirty workspace without mutating either recovery family.
Each COG or resource-editor answer is bound to that tab's exact authored path/name/content/modified state. If a timer,
plugin, or other nested event-loop callback changes an already resolved tab while a later prompt is open, changes the
tab set/order, or destroys a tab, replacement aborts and leaves the workspace available for a fresh decision. The
complete authorized Level, authored COG/resource buffers, and authored Cleave fingerprints are checked again before a
potentially long
Open/recovery load installs its result, so work created after the prompts cannot be overwritten by the older
authorization.
Only after recovery application or ordinary file loading succeeds are deferred Discard and Open-Saved retirements
committed. Cancellation, decode failure, project-choice cancellation, or file-load failure leaves the current workspace
and both durable checkpoint families intact. Startup collapses duplicate live slots to the newest offer per document;
older session/history points remain accessible manually. Every successful recovered/ordinary Open and New Project
gate-retires the exact superseded live paths at its commit boundary, preventing an in-flight old publisher from
recreating a prompt after the family scan. If the current-workspace choice is Save, the exact recovery selected by the
outer Open transaction is protected from Save's normal live-family retirement until recovery installation commits; this
also covers the selected snapshot and current workspace sharing one identity/session.

The native `edit` section is shared exactly by `.sed`, `.sedrecovery`, and `.sedautosave`. Its application payload
schema remains V1 inside every native SED version. Its required `window.uvState` stores UV zoom/pan, exact View Face
basis/display alignment, active UV element, and face/edge/vertex selection; repeat, snap, and grid controls remain
sibling window fields. The optional `window.templateEditorGeometry` remembers the Template Editor's last window size and
position; a valid SED value overrides the application-level fallback, while older payloads keep using that fallback.
Each recovered COG-editor tab also stores its
20%-400% presentation zoom independently from its authored text. Newer tab records may additionally carry the
two-element Symbols/Code `outlineSplitter` allocation; the field is optional so older recovery/native EDIT records
load with the default equal split. An in-progress UV G/S/R modal gesture is not
serialized. If the whole optional `edit` section is rejected during corruption recovery, UV state falls back atomically
to safe defaults together with the rest of the editor workspace.

The COG V1 workspace keeps a flat `tabs` array, global `active` flat-tab selection, and legacy primary `dockVisible`.
Its optional `hosts` array stores stable `id`, `primary`, requested `visible`, `floating`, `geometry` bytes, local
`active`, and flat-tab membership indices in `tabs`. The primary is first with ID `primary` and may be empty with
local `active: -1`; detached hosts are nonempty. IDs are unique strings of one to 64 ASCII letters, digits,
underscores, or hyphens. Every script is represented exactly once, and a selected global active tab matches its
host's local selection. Bounds are 1,024 script tabs, 64 hosts including the primary, and 1 MiB of geometry bytes
per host. The complete topology and script maps are validated before mutation; named host docks are prepared before
the saved Qt workspace layout is restored. Legacy records without `hosts` restore all scripts in the primary window,
and floating windows are kept on an available screen. Script maps and optional document histories are unchanged.

Recovery captured in Game Preview stores each COG host's requested Editor visibility rather than its forced hidden
state. Loading that workspace in Preview keeps every host hidden and read-only until Editor restores the requested
windows. A script transfer retains its live editor and local history; capture and restore never save or extract its
source. Host layout appears only in full native/recovery capture, not authored-content comparisons. The optional
host records do not change EDIT, native SED, or recovery-envelope versions.

The optional `edit.resourceEditors` V1 map stores Sprite, Sound Class, and UNI editor groups and file tabs, including
resource-window membership, order, pinning, active tabs, source metadata, per-buffer native recovery baselines, and raw
draft pane state. Docked and detached panes use the same codec, and moving a file does not create a second source owner.
The map stores `version`, global `active`, legacy Sprite `dockVisible`/`dockGeometry`, flat `tabs`, and `hosts`. Each tab
contains `page`, `pinned`, `host`, and optional 0- or 32-byte `nativeBaselineHash`. Each occupied host contains `primary`,
stable `id`, `visible`, `floating`, `geometry`, local `active`, and global tab indices. An optional `groups` map stores
`sprite`, `soundClass`, and `uni` presentation maps, each with `visible`, `floating`, `geometry`, and `active` (`-1` for
an empty primary group). With groups present, each host adds `kind` (0 Sprite, 1 Sound Class, 2 UNI), contains files only
of that kind, and uses a valid local selection. Primary IDs are `primary`, `primarySoundClass`, and `primaryUni`; their
presentation must match the corresponding group. Legacy Sprite fields must match `groups.sprite`. Empty primary groups
remain in `groups` without consuming occupied-host budget, and an entirely empty workspace has no occupied hosts.
Known fields are strictly validated; bounded unknown optional extensions are ignored. Floating windows are restored
within an available screen.

Older EDIT V1 records may omit `groups` or `hosts` and may contain mixed resource hosts. Restore partitions those files
into matching groups without discarding raw drafts or native baselines. Missing baseline hashes are recomputed where
possible. At most 64 file tabs and 64 occupied hosts are admitted independently, and each geometry byte array is limited
to 1 MiB. These optional extensions retain the existing native SED, EDIT, and recovery-envelope versions.

Each page map independently records its resource kind and project-derived profile, source kind and identity, draft name,
modified marker and external disk hash, pane geometry, search text, and raw authoring values. Sprite values include its
MAT token, type, width and height, geometry/light/texture modes, RGBA extra light, and XYZ offsets. SND and UNI pages
store ordered raw rows; UNI also retains imported capacity even though external output declares the live row count
for both profiles, and SND stores collapsed-mode presentation. Optional SND
`soundPreviewLoops` stores one Boolean per source row and must match the row count when present; absent data defaults to
nonlooping playback. Optional SND `soundEmptyModes` is a sorted array of distinct, exact engine-recognized mode names
for explicitly added groups without entries. It is limited to 95 names in classic projects or 140 in IJIM, excludes
names already present in authored rows, and is rejected for other resource kinds. Omission restores legacy behavior.
Empty groups remain native presentation state, excluded from the canonical authored projection and external SND
output; New, Open, and source reload clear the list. Active table delegates and Sprite numeric editors are captured
without committing or moving focus.
Optional Sprite `spritePendingNumericEdits` maps `width`, `height`, `extraLightScalar`, and `offsetX`/`offsetY`/`offsetZ`
to bounded raw strings; accepted finite values remain in the separate `sprite` array. Raw cells and pending numeric text
can contain intermediate invalid values and still round-trip through native Save, autosave, and recovery. Restore does
not replace a captured draft with newly
read loose-file contents. Explicit external Save validates and atomically writes the resource; archive-backed sources
leave their original archive unchanged. Normal Save of an archive-backed SPR, SND, or UNI writes a loose file under
the saved project's `misc` category directory, or the explicitly configured project resource root for Untitled.
SPR and SND use `misc/spr` and `misc/snd`; UNI uses `misc`. This Save preserves the resource filename and applies the
existing ownership, collision, and external-baseline checks without a Save As chooser. Failure retains the complete
draft and source identity; success associates the pane with the loose file. Save As remains an explicit destination
choice. Recovery capture and restore themselves never extract or write resource files.
Sprite FPS, paused playback, Bounds, IJIM Preview geometry, splitter sizes, and preview camera are
presentation state kept separate from authored SPR values and dirty state; current cel phase is derived at runtime. A Sprite page may include
the optional `presentation` map (`version: 1`) with `bounds` (Boolean), `fps` (integer 0 through 99), optional `paused`
(Boolean), optional `globalGeometryMode` (integer 0 through 4, default 4 when absent), `splitterSizes`
(two nonnegative pixel extents), and `camera`. IJIM exposes the global mode as Preview geometry; classic captures
may carry the hidden default 4. The field leaves authored Geometry/Texture tokens intact and does not mark them modified.
Its camera map stores `orbit` as the flat eleven-number array
`[centerX, centerY, centerZ, panX, panY, panZ, radius, distance, yaw, pitch, roll]`, plus `projectionType`, `fovXDeg`,
`orthoScale`, and `clipEnabled`;
finite camera values are range-checked. When no map is present, restore uses the default Bounds/FPS/split and straight-on
fitted camera. Saved framing is applied after the isolated preview Level is attached and initially fitted. Playback
starts at the first available MAT cel with the saved paused choice (false when absent) and does not change authored
resource state. Resource drafts share EDIT V1, so this extension does not bump the native SED or recovery-envelope version.


Each resource page may also carry optional `editorPresentation` V1. It retains the stable line-edit cursor/anchor pairs,
UNI current cell and selection ranges, SND current item by source ordinal or exact mode bytes plus column, table scroll
offsets, explicitly manual SND widths, and deferred cell-editor carets keyed by source-row identity and column. Restore
rebuilds the model/history before applying presentation, applies search/filter/collapse before selection/layout/scroll, and
defer-scrolls until visible. Carets for a cell editor are applied only when ordinary editing begins; recovery never opens
or commits a live delegate. Capture reads pending text/carets without changing focus, resource history, authored dirty
state, or recovery arming. `editorPresentation` is excluded from authored-content projection. The native EDIT contract
in [sed_binary_format.md](sed_binary_format.md#workspace-schema-and-validation) defines its exact map keys and validation.

Each resource page and COG tab may also store its optional, independently versioned `history` map (`version: 1`).
The shared schema is documented in [Native SED EDIT](../sed_format.md#edit). It contains the actual document command
timeline, applied `cursor`, retained external `clean` checkpoint, and `baselineDirty` marker; a saved checkpoint in
Redo survives capture until a new edit discards that branch. Resource commands retain exact raw drafts alongside
accepted canonical values. SND separates authored commands from undoable empty-mode presentation through
`cleanAuthored`. COG text splices preserve native typing and edit-block command boundaries, UTF-16 supplementary
characters, and the native saved clean position. Each COG splice must remove or insert nonempty text and use the
canonical editor projection (U+00A0 to space, U+2028/U+2029 to line feed); incoming non-reconstructible journals reject
atomically, including their future Redo commands. Existing source-text capture already uses that projection, so this
adds no user-content restriction. Identical nonempty text replacement retains the actual Qt command and clean marker.
A history-absent legacy record restores the current buffer with an
empty document timeline. A present malformed history, inconsistent modified marker, invalid splice boundary, or
unmatched command precondition rejects the workspace atomically.

These histories are shared unchanged by manual native Save, live recovery, and autosave history. Capture does not
execute live Undo/Redo or commit an active numeric/table editor: pending raw values become commands only in the
immutable captured timeline when needed. The retained command count is at most 1,024; resource command arrays and
complete COG history maps are bounded to 16 MiB, with current canonical resource metadata governed by the enclosing
EDIT budget. Recording/capture discard old history within these bounds while retaining exact current authored and
raw pending state. COG's rare native-stack reconstruction restores text, selection, scroll, dirty state, and final
Undo/Redo availability before refreshing derived editor state. It does not publish transient replay states.
Authored/modal/saved-baseline projections exclude history; ordinary Thing transforms do not serialize these journals.
A native SED Save embeds the buffer and its history but does not externally Save the resource or clear that editor's
external dirty marker.

When a table cell has an active Qt delegate, capture reads its current text
without committing the edit or changing focus, cursor, selection, or local undo.
The optional named `edit.runtimeLighting` map stores global `thingLights` and
`sectorPointLights` booleans outside the fixed viewport arrays. Both default to
enabled when absent; wrong types reject the EDIT workspace and unknown future
keys are ignored. These flags remain shared by Editor and Game Preview and do
not modify the Level.

The optional `edit.gridAppearance` map stores the minor-grid presentation outside
the fixed viewport arrays: `minorDots` is boolean and defaults to `false`
(Lines); `minorDotSizePx` is a finite number from `0.5` through `10` Qt logical
pixels and defaults to `2`. Absent maps or fields use those defaults even when
app preferences choose Points. Present valid values override app settings.
Invalid map/field types or diameter values reject the complete EDIT workspace
before mutation; unknown map keys are ignored. Major grid/axis lines and the
snap lattice remain unchanged. Native Save, autosave, and recovery share this
additive map without changing EDIT V1, native SED, or recovery-envelope versions.

The independent optional `edit.window.uvGridAppearance` map carries the UV Sub-grid presentation with the same
boolean `minorDots` and finite `minorDotSizePx` range `0.5`–`10`, defaulting to Lines and `2` logical pixels.
Missing maps or fields use those defaults independently of app settings. Invalid known values reject the complete EDIT
workspace before mutation, while unknown map keys are ignored. This map changes UV canvas appearance without changing
stored UVs, snap spacing, or the recovery-envelope version.

The shared EDIT mode array admits every current edit mode, Surface submode, and Light submode, including MotS Arch
Light mode (`3`) alongside Baked (`0`), Thing (`1`), and IJIM Sector (`2`). Negative, unknown, and noninteger enum values
still reject the entire workspace. Restore retains the normal project applicability checks: a Sector submode outside
IJIM or Arch outside MotS falls back to Baked. These values use the existing V1 editor-state schema and do not change
the native document or recovery-envelope version.

Staged Cleave points, endpoints, plane frames and cap construction data retain document doubles in their existing
three-number CBOR arrays. The EDIT V1 field layout is unchanged; older finite numeric arrays remain readable.
The existing named Cleave map may also contain `chainOrigin: [lineIndex, endpoint]`, including while Chain is off.
Both entries must be integers, the line index must address a staged line, and endpoint must be `0` or `1`.
Malformed or dangling references reject EDIT before mutation. Absence retains the last-line-second fallback;
restoration preserves physical lines and the separate execution order. Native Save, autosave, and recovery share it.

That map may also contain optional Boolean `bounded`. Present values restore the finite-stroke descendant gate;
non-Booleans reject EDIT before mutation. An absent field with pending lines preserves legacy global-plane behavior,
while absence without pending lines retains the app's Bounded preference (fresh default on). The existing saved world
endpoints define each stroke, so fixed arrays and container versions remain unchanged. Closed chains keep their
interior-descendant path. Native Save, autosave, and recovery use the same representation.

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

The Cleave map retains each family's physical lines/context/links/origin, `applyOrder`, line `selection`, `transformMode`,
shape options/placement state, and optional version `1` local `history` snapshots/cursor. Native Save, autosave, and
recovery use the same checked types, references, permutation, and at most 64 snapshots per family under one aggregate
16 MiB authored draft/history workset budget. Shape and rendering caches have separate bounded presentation storage.
The shape map's optional named Boolean `cameraPlane` defaults off when absent and affects only Sector shape placement.
A present non-Boolean rejects EDIT before mutation. It shares the native representation without changing schema or
container versions. Square's legacy enum remains readable and restores as a linked Rectangle; missing shape options
default to Rectangle with `0.1` world-unit dimensions. Project-inapplicable length units normalize to Meters without
resizing authored dimensions.
The optional named finite numeric `rotationDegrees` defaults to `0` when absent. It stores the pending shape's
in-plane rotation; runtime options normalize complete turns to the signed interval `[-180, 180)`. A present nonnumeric
or non-finite value rejects EDIT atomically. Rotation changes the placement frame without changing stored physical
dimensions or the two-dimensional shape plan. Both families and Apply mementos share this field.

Apply mementos share this map; invalid known values in their optional `workspaceHistory` attachment follow its normal
sidecar rejection policy while retaining an otherwise valid current workspace.
Legacy records without history start a fresh local baseline when restoration admission permits; malformed timelines
reject EDIT before any partial publication. Before viewport mutation, restoration checks both decoded source families,
both existing runtime workspaces, and prepared draft/history against the same aggregate budget, then publishes
prepared containers by move. An over-budget retained
timeline returns an explicit failure/warning while the Level remains recovered, without installing a partial draft or
timeline in either family. Current-only legacy lines remain retained when their complete restore workset fits; an
optional initial baseline is omitted when source, live drafts, and baseline cannot fit together.
The optional root `workspaceHistory` sidecar binds Apply workspace mementos and historical Layer visuals to exact
adjacent HIST stack positions and required-Level hashes, under a separate 64-transition/16 MiB cap. Checked import
remaps fresh runtime record IDs. Missing/degraded/invalid HIST drops optional attachments with a warning while
retaining both current family drafts and other valid workspace state. Each Cleave memento restores or clears only
its applied family and preserves independently edited peer work. Historical workspaces identify their family with
optional named `currentFamily` and reject recognized `peerWorkspace` fields rather than silently importing or ignoring
a peer. Paired Level Redo is revoked by authored changes in that applied family, not by edits to the peer. See the [native EDIT contract](../sed_format.md#edit)
for exact named fields; recovery and native section versions remain unchanged.

The optional `gridAppearance.axisColors` tuple retains three checked unsigned RGBA axis colors. Settled MotS Arch
channel/target/source-vertex selection and owner-expansion provenance use `architecturalLighting`; resource-dependent
references wait for matching meshes rather than being inferred or discarded while resources are unavailable. Hover,
cycle, previews, and caches remain transient.

The shared viewport state restores Editor and Game Preview fog, Thing collision-shape and visual-bounds visibility, and
Arch Thing gizmo visibility independently. A present named viewport-state value is authoritative over app settings. Legacy
16/17-entry viewport arrays without the optional named map inherit the app's separately stored Editor and Preview defaults;
fresh Thing-overlay defaults are on in Editor and off in Game Preview. When EDIT is absent or rejected, Editor fog defaults
off and Preview fog defaults on. Manual .sed Save, autosave, and recovery use this same EDIT codec.

Each viewport array may end with an optional named viewport-state map. Known keys are type-checked, while unknown named
keys are ignored safely. Future boolean gizmo, rendering, and overlay fields can be added to this map without shifting
positional fields or bumping the native SED version. Writers emit only fields understood by that application build, so an
older application's re-save may drop unknown future keys. COG and resource-editor tab bulk close uses the represented
tab order, pin, modified, and authored-buffer records, so it does not alter the recovery schema.

## Limits and implementation

- primary text-envelope metadata JSON: 1 MiB accepted-header limit;
- metadata-only discovery: scans a bounded cumulative amount, skips a canonical range-checked `docm` extent without
  materializing it, continues to detect conflicting later `meta`, and materializes at most a 4 MiB protected `meta`;
- raw/stored `docm`: 256 MiB maximum and must have equal raw/stored sizes;
- raw/stored `warn`: 1 MiB maximum;
- accepted `warn` chunks: one; last-resort manifest text: 2 Mi characters; recovery audit entry/log: 64 KiB/1 MiB with
  one rotated predecessor;
- whole recovery container: 256 MiB maximum;
- recovery chunks: 64 maximum while scanning;
- metadata data/parity shards: at most 16 + exactly 2.

The native `edit` payload is capped at 128 MiB and receives an allocation-bounded CBOR preflight before Qt constructs
its object graph: at most 2,000,000 nodes, depth 64, bounded text/byte strings, and a dynamic amplification allowance
with a 128 MiB absolute decoded-graph/verification ceiling. It must be one definite, tag-free, canonical CBOR value
with unique text keys and no trailing bytes. Before autosave/recovery publication or manual `.sed` replacement, the
captured immutable payload runs through the same complete non-mutating schema parser used by restore; failure leaves the
preceding checkpoint/file untouched.

The native `hist` payload is capped at 64 MiB. Its application history codec also limits CBOR to 2,000,000 nodes,
depth 64, 8 KiB byte strings, and 3 MiB text items; the persisted combined
undo/redo timeline to 100,000 entries independently of the GUI's 1,024-entry
live cap; the pool to 1,000,000 fragments and
64 MiB cumulative fragment bytes; each
state to 300,000 fragment references. Reconstruction work is bounded independently and charges unique pooled/base
fragments once plus a bounded per-state floor. Each transient decoded Level is capped at 128 MiB/two million logical
items. It is converted immediately into the live compact snapshot selected by `dirtyFlags`; topology-preserving
`levelGeometry` records retain only changed owning Sectors (including adjoin/shared-vertex state), while
structural/unknown records retain a complete Level and ordinary Thing/light/COG records retain their corresponding
domains. Nonempty MotS bake provenance can require a FullSnapshot even for a Thing-only dirty mask; admission charges
that actual complete Level before retaining it, rather than a selected-Thing proxy.
These retained compact snapshots together are capped at 768 MiB/32 million logical items. The trusted decoder keeps
only the preceding values of rematerialized semantic domains and derives transition/subset facts from those values;
ordinary compact restore neither retains nor copies a complete adjacent Level. Direct native `.sed` Open and the unknown-project
post-choice path use the same consuming decoder; the expanded portable DTO is
reserved for inspection/tests and is never installed into a live document.
For `Lights` and `Things+Lights`, the retained footprint follows the compact
baked-light graph (sector/surface records and one `ColorF` per texture vertex),
not a synthetic full `Sector`/`Surface` graph with UV-bearing `TexVertex`
objects. The recovery writer performs this same bounded semantic admission on
its worker before publishing HIST, so a stream that would be rejected on the
next open is omitted with an explicit warning rather than published as a
misleading partial autosave.
Recovery parsing retains its already native-validated Level, and the consuming
install moves that graph rather than decoding `docm` a second time. Once a
known-project HIST has become compact records its encoded buffer is released;
the raw DOCM buffer is likewise released when its decoded Level is consumed,
except when required or unidentified Level loss needs those damaged bytes for
the last-resort text-salvage worker scheduled immediately after assembly.
Writer-owned history-nested Level bytes are split through framing-only arithmetic
after serialization, avoiding one redundant full semantic decode per history
state. Adjacent generated states also reuse exact HistoryNested required-
section frames from separate undo/redo caches. The invalidation mask comes
from the actual compact/full Snapshot variant, not `dirtyFlags`; `rsrc` is
still regenerated exactly for every state, and a changed catalog invalidates
all sections containing category-local resource indices. A cache is committed
only after its complete HIST entry succeeds. Untrusted nested bytes retain the
strict validate-before-split path and never enter this generated-frame cache.

Damaged native-envelope fallbacks share one cumulative byte/candidate budget across the normal parse and redundant
retry. Live and historical discovery are separate suffix-filtered scans: each examines at most 4,096 matching
`.sedrecovery` or `.sedautosave` files, rejects symlinks, applies the bounded metadata-only parser before retention, and
keeps at most 256 valid descriptors by authenticated capture time/identity/path. Unrelated directory entries and a full
live-slot budget therefore cannot hide periodic history. Authored `requiresUserRecovery` descriptors rank ahead of a
malformed/legacy clean entry before either cap. Exact-family discard/retention uses the combined family token as a
pre-decode filter and then requires protected metadata agreement; unrelated files
cannot consume that bounded 64-entry family-maintenance scan (the supported family is one live slot plus ten history
points, with the remaining allowance reserved for failed-prune duplicates).
Malformed files, including a valid front meta followed by a conflicting protected meta after docm, cannot consume
retained slots merely by using forged future filesystem timestamps. A fixed scan cannot
guarantee discovery past the 4,096th matching entry in either scan; that boundary is the deliberate directory-walk work
cap. Accepted
document association stores 32-byte digests rather than extra
full canonical Level/document copies. Descriptor metadata is limited to 128 characters for a session ID, 512 for a
display name, 32,768 for a source path, and the source-path allowance plus a short identity prefix for the identity;
the same policy is applied before writing and before retaining a discovered descriptor.

When every meta copy is lost but docm validates, orphan-metadata synthesis reuses that decoded document's native project
trust, canonical Level digest, and deferred hist. It does not restart native decoding merely to infer those fields.

Implementation: [document_recovery_utils.cpp](../../src/app/src/ui/recovery/document_recovery_utils.cpp) and
[recovery.cpp](../../src/app/src/main_window/document/recovery.cpp). Native document implementation is
[sed.cpp](../../src/core/src/assets/sed.cpp); text-salvage publication is in
[partial_level_salvage.cpp](../../src/app/src/ui/recovery/partial_level_salvage.cpp); editor-state integration is in the
application Save/Open path. Tests are in
[document_recovery_tests.cpp](../../src/app/tests/document_recovery_tests.cpp),
[partial_level_salvage_tests.cpp](../../src/app/tests/partial_level_salvage_tests.cpp), and
[main_window_event_filter_tests.cpp](../../src/app/tests/main_window_event_filter_tests.cpp).
