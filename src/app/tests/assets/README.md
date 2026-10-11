# Application SED V1 HIST vectors

This directory owns the application-level HIST boundary fixture. The
sed_core fixture directory contains opaque native FEC vectors; these vectors
add the application schema that matters during recovery:

- `sed_v1_hist_document.sed` is a deterministic native SED V1 document with
  an explicitly pixel-space compatibility Level, a deterministic p=4 `edit`
  payload checked exactly at
  the native/recovery boundary, and a real serialized editor history
  containing both undo and redo entries. MainWindow EDIT-schema validation is
  covered separately by the MainWindow tests
- `sed_v1_hist_document_1_damage.sed` corrupts one CRC-protected physical
  HIST slot in every stripe
- `sed_v1_hist_document_2_damage.sed` corrupts two distinct physical slots in
  every stripe
- `sed_v1_hist_document_3_damage.sed` corrupts three distinct physical slots
  in every stripe, exceeding HIST's p=2 repair budget

The ordinary application test only reads these files. It verifies that the
first two variants reconstruct the exact Level, EDIT payload, and HIST CBOR,
then restores and walks both undo and redo branches. The third variant must
strictly fail, while permissive recovery drops only HIST and retains exact
Level and EDIT data. A generated native SED V2 companion case independently
checks the same strict normalized-Level and usable undo/redo-history boundary;
it does not replace the checked-in V1 compatibility vectors.

The vectors are regenerated only when the V1 fixture intentionally changes:

```text
set "SED_WRITE_APP_HIST_GOLDEN=1"
build\ninja\src\app\Release\sed_app_tests.exe
```

The generator writes all four files and the same test immediately compares
them with the generated canonical bytes. Regeneration is never automatic in
normal tests.

## Synthetic resource fixtures

- `synthetic_rail_track.3do` is an original, minimal two-ended rail model used
  to exercise ResourceLocator-backed connector extraction and Frame/Thing snap
  integration without depending on a game installation or proprietary assets
