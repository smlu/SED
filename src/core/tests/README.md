# Core test guide

`sed_core_tests` covers the portable level model, file formats, editing algorithms, and disk/resource boundaries without
Qt. CTest labels the suite as `unit`, `module`, `domain`, and `integration`.

## Test layers

| Layer | Purpose | Representative coverage |
|---|---|---|
| Unit | One deterministic helper or value contract | [binary_reader_tests.cpp](binary_reader_tests.cpp), [geometry_math_tests.cpp](geometry/geometry_math_tests.cpp), [text_tests.cpp](text/text_tests.cpp) |
| Module | One parser, serializer, or self-contained subsystem | [assets](assets), [editor_tests.cpp](editor/editor_tests.cpp) |
| Domain | Level-designer operations over the real Level model | [level](level) |
| Integration | Multiple modules plus streams, files, resources, or save/reopen boundaries | [resource_locator_disk_tests.cpp](assets/resource_locator_disk_tests.cpp), [jkl_save_tests.cpp](assets/jkl_save_tests.cpp), [sed_level_tests.cpp](assets/sed_level_tests.cpp) |

The categories overlap intentionally. A native SED round trip, for example, is both file-format module coverage and a
Level-domain integration test

## Geometry diagnostic precision

`level/level_geometry_tests.cpp` distinguishes diagnostic compatibility from strict editing predicates with analytic
warped cubes, translated coordinates, threshold boundaries, extreme normal scales, malformed inputs, and reversed
portal loops with redundant edge subdivisions. Genuine gaps and crossed loops remain mismatches. The
`diagnostic-boundary` mode of `sed_cleave_allocation_tests` injects each allocation failure and verifies the boolean
result and complete authored/runtime input preservation, then requires a successful no-fault comparison.

## Bounded Multi-cut coverage

Ordered Sector Multi-cut coverage in [level_cleave_tests.cpp](level/level_cleave_tests.cpp) also checks Bounded
stroke reach through successful split ancestry, global mode, closed-chain bypass, failure-atomic invalid metadata,
crossing and endpoint-only contact, and short translated strokes with real gaps. These cases exercise the portable
interval helper used by both the Surface and Sector workflows.

Independent Sector arrangements cover non-splitting planes first, in the middle, and last; all-no-op rejection;
bounded ancestry that reaches only a non-splitting child; and an oblique camera-plane Rectangle extending beyond a
synthetic Sector. Closed-chain cases preserve a non-empty interior through no-op planes and reject an empty interior.
Successful results match equivalent effective-plane batches, including UV tuples, identity, Things, and reciprocal
adjoins; malformed later stages preserve the complete Level.


## Entity-ID capacity and import provenance

`level/level_tests.cpp` pins the next-ID invariant: `INT32_MAX - 1` is the final assignable ID, `INT32_MAX` is the
exhausted sentinel, and a widened constant-time capacity check admits exact fits while refusing over-budget batches.
The same suite covers failure-before-append behavior in Level factories. `level/level_clipboard_tests.cpp` checks
atomic rejection and exact-capacity Sector/Light imports; `level/level_cleave_tests.cpp` checks private split
exhaustion without source mutation; and `editor/editor_tests.cpp` verifies document rollback retains Level identity,
selection, and the complete Undo/Redo branch.

`assets/sed_level_tests.cpp` covers the optional protected `jimp` V1 section, p4 repair, represented-state emission and
unused-map normalization, malformed version/Boolean/count/reference/trailing data, and permissive loss that clears only
import metadata while preserving the verified Level. Ordinary COG data without represented pending import provenance
keeps the prior bytes.

## UV contract coverage

- Native SED V2/V3 preserve normalized `TexVertex` UVs, per-axis scales, and colours byte-exactly; V1/V4 preserve explicitly tagged legacy pixel values, with V3/V4 also covering MotS ARCHLIGHTING persistence
- Native SED preserves authored ARCH bake baselines (owner ID, model identity, original binding, and optional original
  object), distinguishes baseline changes in semantic equality, reads files without the optional trailer, and rejects
  duplicate baseline IDs or an inconsistent original binding
- JKDF2/MotS JKL and legacy JED/ASC/LEV normalize through real anisotropic MAT dimensions, retain signed and multi-tile values, and reject unresolved conversion atomically
- JKL/JED export expands normalized UVs and scales to pixel wire units, while IJIM NDY preserves normalized wire values independently of MAT-size changes
- JKL/NDY loading derives the wire UV interpretation from the parsed header version, while export rejects a project/header-version contradiction instead of writing ambiguous units
- 3DO 2.1 preserves pixel UVs while 3DO 2.3 preserves normalized UVs, with stream and byte-span parsing kept equivalent
- Sector, Surface, Edge, and Vertex transforms preserve authored normalized UVs; cleaves retain original corners and interpolate only new corners
- Surface, Edge, and Sector cleaves reject incomplete, non-finite, and overflowing input without partial mutation and produce equivalent geometry from anisotropic pixel and normalized fixtures
- Extrude and merge preserve normalized coordinate-space ownership and per-axis scales while producing finite generated coordinates; merge rejects malformed survivor, donor, and mirror data rather than padding or regenerating UVs
- Surface merge uses analytic regressions for tiny leading edges, float-rounded UV deltas, transformed faces, and
  anisotropic pixel coordinates. Single and fixed-point batches preserve exact source tuples, independent portal
  frames, scales, and document Undo/Redo. Run `sed_core_tests --surface-merge-uv` for this focused coverage.
- Same-space conversion is still a validating boundary: invalid tags, missing or excess corner coordinates, non-finite coordinates/scales, and float overflow fail without unnecessary resolver calls or partial mutation
- Raw Sector-shape and built-in primitive admission converts complete candidates to the destination Level space exactly once; malformed saved-shape rows and malformed in-memory shape topology are rejected without publishing a partial Sector
- Safe and forced Vertex deletion preserve the exact surviving UV tuple, corner colour, signed per-axis scales, and Level tag; incomplete or excess texture topology is rejected atomically
- Cross-space clipboard paste converts complete Sector payloads exactly once, while same-space and cross-space payloads reject malformed topology, non-finite values, or unavailable dimensions without partial publication
- Public UV basis and scale-recovery helpers reject malformed geometry, non-finite coordinates, invalid pixels-per-unit values, and finite values that overflow when narrowed to stored floats without publishing partial output
- Scale recovery handles finite endpoint differences beyond float range in double precision, rejects projected-delta overflow, cleave interpolation retains finite extreme endpoints, texture winding avoids float-product overflow, and rejected Surface Connect operations preserve the complete Level atomically
- Texture-density scaling commits both finite per-axis scale values together or preserves both when either result is unrepresentable
- Connected-surface UV preservation covers small valid faces in normalized and compatibility pixel space, including analytic texture-density expectations and atomic document Undo/Redo
- Expanded Extrude's contained-ring and partition routes preserve fixed source UV/RGBA tuples, exact source corners and
  marks, reciprocal subdivided portals, and COG ownership; route tests reject a single-portal candidate before
  accepting multipartition coverage and keep fixed affine, piecewise, and cancellation vectors independent of the
  production interpolation code
- Node offset/inverse transforms cover malformed ancestry and bounded cycle rejection without partial coordinate publication, alongside valid hierarchy round trips

The format-boundary cases live in [jkl_save_tests.cpp](assets/jkl_save_tests.cpp),
[jed_level_tests.cpp](assets/jed_level_tests.cpp), [asc_tests.cpp](assets/asc_tests.cpp),
[lev_tests.cpp](assets/lev_tests.cpp), [model3do_tests.cpp](assets/model3do_tests.cpp), and
[sed_level_tests.cpp](assets/sed_level_tests.cpp). Geometry behavior is owned by
[level_uv_tests.cpp](level/level_uv_tests.cpp), [level_transform_tests.cpp](level/level_transform_tests.cpp),
[level_cleave_tests.cpp](level/level_cleave_tests.cpp), [level_connect_contract_tests.cpp](level/level_connect_contract_tests.cpp),
[level_extrude_tests.cpp](level/level_extrude_tests.cpp),
[extrude_arrangement_registry_tests.cpp](level/extrude_arrangement_registry_tests.cpp),
[level_cog_reference_tests.cpp](level/level_cog_reference_tests.cpp),
[level_merge_tests.cpp](level/level_merge_tests.cpp),
[level_surface_merge_uv_tests.cpp](level/level_surface_merge_uv_tests.cpp),
[connected_surface_uv_tests.cpp](level/connected_surface_uv_tests.cpp),
and [level_clipboard_tests.cpp](level/level_clipboard_tests.cpp)

## JKL/NDY input and path-save boundaries

[jkl_level_tests.cpp](assets/jkl_level_tests.cpp) exercises stream device errors and ordinary EOF without a final
newline; malformed, negative, overflowing, incomplete, and out-of-range Sector `SURFACES` spans; and the valid empty
span at the end of the global surface array. Float-field cases cover fog, Sector lighting and tint, Surface corner
channels, editor light RGB, and classic scalar/MotS RGBA layouts, rejecting overflow while accepting signed HDR and
the writer's rounded finite maximum-float spelling. Sector-record cases reject complete records beyond the declared
WORLD count, including a zero count, while retaining comments, unknown trailing fields, and later sections. Failed input
retains the caller's existing Level.

[jkl_save_tests.cpp](assets/jkl_save_tests.cpp) compares successful path output against the stream serializer for all
projects. Injected write, flush, close, and publish errors preserve the complete previous destination, remove owned
staging files, and restore the thread-local fault scope. The core path API stages a complete validated sibling file
before atomic replacement; a caller-owned output stream cannot provide the same replacement guarantee.
IJIM Materials capacity cases reject Extra outside 0 or 64 at runtime export while retaining incompatible authored
values in imports, native round trips, and report-only measurement; zero capacity remains a valid tested boundary.

## Resource and 3DO admission

[resource_locator_bounded_read_tests.cpp](assets/resource_locator_bounded_read_tests.cpp) checks complete bounded
stream capture with stale low/high size hints, exact fits, growth beyond the cap, I/O failure, cleared source-path
outputs on refusal, winning loose/GOB precedence, trusted primary overrides, and no fallback after an oversized winner.
Selected-loose metadata cases preserve that precedence without reading payloads or materializing a primary override.
The thread-local reduced budgets in [model3do_tests.cpp](assets/model3do_tests.cpp) exercise exact and one-over source,
section-view capacity, token-view, decoded-model, and hierarchy-work boundaries without allocating production-sized
fixtures. Byte-span, stream, and binary-path inputs share source admission, including literal CRLF byte counts.
Hierarchy controls cover repeated meshes, both normal arrays, valid chains, cycles and incoming tails, invalid parents,
and raw authored-mesh mode, with preflight before clones/transforms. These tests establish the bounded synthetic cases;
they do not claim admission of every retail model or a total process-memory ceiling.

## Checked-in vectors

The native-format vector contract, project matrix, resource-catalog fixture, external SHA-256 and CRC32C vectors, and
regeneration rules are documented in [SED V1 conformance vectors](assets/fixtures/sed_vectors/README.md)

Disk-resource regressions also cover non-regular loose candidates, exact-root GOB precedence over loose kind-folder
fallbacks, hostile GOB v1/v2 directory counts before allocation, and retained immutable loose-root/GOB-mount
configuration snapshots while a publisher installs or clears a replacement configuration and after that publisher is destroyed
Previously returned loose-file and archive byte values remain stable after same-path replacement or removal while a fresh read observes the current resource

Ordinary tests only read checked-in vectors. They must not regenerate expected bytes with the writer under test.
All three project inputs assert one Sector and one triangle before comparing their goldens. The deliberate
`--write-sed-ijim-vector` maintenance selector rewrites only the four derived IJIM vectors after an input correction;
see the vector manifest for its exact scope.

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

- Put narrow helper tests next to the owning module
- Exercise successful, empty, malformed, truncated, oversized, and unavailable-input paths that the public contract permits
- Assert state and every meaningful output, including failure text when it is part of a user-facing or diagnostic contract
- Use checked-in byte goldens when exact serialization matters
- Use synthetic original fixtures instead of depending on a game installation or personal files
- Keep temporary paths isolated through the CTest-provided test temporary directory
- Register every new test source in [core CMakeLists.txt](../CMakeLists.txt)

Run the focused executable while iterating, then run the complete Release CTest gate described in the
[developer documentation](../../../docs/README.md)

## Allocation failure boundaries

`sed_geometry_transform_allocation_tests`, implemented in
[geometry_transform_allocation_tests.cpp](level/geometry_transform_allocation_tests.cpp), runs 13 isolated modes:
Sector recalculation; Sector/Surface/Edge/Vertex translation; Sector/Surface/Edge rotation; Sector/Surface scaling
with texture metadata; axis and plane Sector flips; and Sector duplication. Every allocation position is injected
before a no-fault success, with complete authored and runtime state checked on rejection, including marks, normals,
bounds, and reciprocal-adjoin refresh. The `Checked` transform APIs return preparation failure without mutation;
application callers use that status to avoid publishing a partial edit. Existing void wrappers remain available.

`sed_document_allocation_tests` is a separate CTest executable. Thread-local fault injection is armed only around the
operation under test; fixture construction, serialization, and assertions run with injection disabled. The suite sweeps
Undo/Redo and history publication variants, retained recovery copies, capped-history trimming, recovery import, and
ResourceLocator replacement. It compares complete native state and history metadata rather than only stack depth.

The isolated Frame transform target additionally sweeps parsing, replacement-token
staging and selected-pose publication. Track whether the injector actually fired:
a swallowed allocation failure must not be mistaken for a clean successful run.
No-op and overflow rejection must preserve original authored tokens as well as
numeric poses. Separate parser/lookup modes cover exception specifications at
public and forwarding boundaries.

`sed_mat_allocation_tests`, implemented in [mat_allocation_tests.cpp](assets/mat_allocation_tests.cpp), arms an
allocation-size guard only around tiny MAT inputs with impossible cel or mipmap counts. The isolated CTest cases
`sed_mat_allocation_cel_count` and `sed_mat_allocation_mipmap_count` require ordinary parser rejection before a large
metadata reserve. Valid controls compare exact animated, full mip-chain, and nonsquare indexed texture pixels.

`sed_cleave_allocation_tests` covers Surface, Edge, Sector and multi-plane Cleave,
including the ancestry allocations used by bounded batches,
individual Join/Knife, auto-adjoin, mirrored Surface Merge and material metadata copy,
including reciprocal portals. It verifies every allocation position until a clean
success, complete rejection rollback, all batch diagnostic fields and independently
computed volume, cut coordinates and boundary incidence.

The multi-target Join regression separately preserves the documented partial-success
policy: the bool result must match the exact connections committed before failure.
Runtime fields omitted from native SED serialization (marks, renderer IDs, cached
material/adjoin indices and normals) are compared independently on rejection.

`sed_extrude_allocation_tests` runs wire, solid, bounds, source-cut, plain commit,
expanded commit, inset commit and a rounded footprint partition in isolated processes. Each mode injects every
allocation failure until a complete successful call, checks the return value and
both report fields, and compares authored bytes plus runtime fields on rejection.
Preview success also preserves the input. Fixed coordinate vectors, analytic
bounds and areas, directed edge incidence, and reverse cyclic portal boundaries
provide independent success oracles.

The expanded connection owns at most three private trials: direct, contained-ring, and source-fragment partition.
False candidate validation restores the pristine input before the next trial; an allocation exception propagates
without a fallback retry. The contained-ring kernel has direct unit coverage for fixed annulus areas, stable corner
identities and winding, simple rounded/concave contours, forward-collinear subdivisions, malformed or backtracking
edges, missing or duplicated triangles, invalid references, and bridge visibility. Its deterministic bridge ordering
uses the non-allocating sort path covered by the rounded allocation sweep.

Rounded-source editing regressions use synthetic closed solids with fixed coordinates, independent volume/edge/portal
checks, retained UV/RGBA tuples, and full failure rollback. They cover repeated Cleave, both Join modes, Adjoin, both
Merge operations, plain/signed Extrude preview parity, tiny edge interpolation and normal-cache handling. Original
commercial game levels are local audit inputs and are not required by these tests.

`level_extrude_tests.cpp` also keeps a fixed 72-case quantized-prism matrix: slopes `0.09123456`, `0.13765432`, and
`0.27182818`, three translated origins, four inherited-bend magnitudes, and both signed nonzero Expand values. Every committed case checks the requested
report, authored source corner/UV retention, reciprocal portal boundaries, finite generated geometry, COG pair text,
and the independently measured `0.9465` XZ source footprint (positive Expand portal coverage and negative Expand
source-cut plus portal conservation). The closure oracle counts each undirected edge twice with opposite winding and
requires two distinct faces; strict planar/convex checks cover the generated sides and outer cap, while inherited cap
fragments are checked by selected-face identity, source-envelope correspondence, and reciprocal portals. Representative
members exercise wire, solid, bounds, source-cut, and commit entry points against the same fixture family.

The Extrude intersection registry has independent fixed vectors for repeated and reversed line queries, original
corner identity, translated oblique coordinates, parallel/coincident lines, conflicting incidence, invalid identifiers,
and non-finite input. Portal test oracles accept redundant forward-collinear subdivisions while rejecting real gaps,
backtracking, and same-facing loops. The inherited source-cap identity proof is restricted to generated face zero and
an exact reverse cyclic boundary; fixed tests reject changed coordinates, one-ULP points, repeated points, invalid
indices, non-finite values, same-facing order, and wrong cyclic order without mutating the source. COG reference
regressions check every value field, fixed serialized reference strings, the geometry of the referenced face, and
native persistence plus Undo/Redo after structural edits. The checked Extrude connection has callback controls for
rejected direct/contained/partition trials and propagated allocation failure; each verifies pristine retry input,
exact callback counts, and no leaked metadata or geometry in the published Level.
