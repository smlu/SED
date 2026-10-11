# AI and Engineering Guide

These rules apply to every change in this repository, whether produced by a person, an AI agent, or both. They are
requirements, not suggestions.

## Start with the project contracts

Use progressive documentation onboarding before changing code:

1. For orientation, read `README.md` and `docs/README.md`, then lightly review the documentation map and the
   designer-facing workflow related to the request. The goal of this pass is to locate the owning subsystem and its
   contracts, not to memorize every project document.
2. Once the owning subsystem is known, and before editing its code, read the relevant documents completely. Do not
   rely only on search excerpts, an old handoff, or assumptions inferred from identifiers.
3. Return to the relevant documents while implementing and reviewing the change. Resolve unclear behavior against the
   documented contract, current tests, and authoritative legacy or upstream source, then update the documentation when
   the contract changes.

Use:

- `docs/architecture.md` for ownership and dependency boundaries
- `docs/algorithms.md` for viewport and editing behavior
- `docs/lighting.md` for all authored, baked, and runtime lighting work
- `docs/user_guide.md` for the level designer's expected workflow
- `docs/formats/` and `docs/sed_format.md` for persistence and recovery contracts
- `docs/porting_guide.md` when behavior must be compared with the original Pascal editor

Before editing, confirm that the active checkout is the C++ project root (`cpp/` in the combined checkout), inspect
the current local changes, and read `CONTRIBUTING.md` when preparing a community contribution. Keep the requested
subsystem and its documentation as the unit of work; preserve unrelated changes made by other contributors or agents.

Inspect the current implementation and tests before making assumptions. If compatibility behavior is unclear, trace the
original SED/JED or game-engine source and document the evidence used.

## Architecture boundaries

- `src/core` is portable C++. It owns the level model, deterministic geometry/editing algorithms, file formats,
  resource parsing, selection/document services, and portable tests. It must not depend on Qt, OpenGL, platform UI, or
  application state
- `src/app` owns Qt widgets, OpenGL rendering, input routing, dialogs, asynchronous orchestration, preferences, and
  application lifecycle. Move reusable deterministic logic into focused helpers or `src/core` instead of embedding it
  in event handlers
- The authoritative editable state is the document model. Rendering caches, previews, and UI controls must never
  become independent authored state
- Keep Qt and OpenGL objects on their owning thread. Background work receives immutable snapshots, supports
  cancellation where practical, and uses generation/fingerprint checks before publishing results
- Preserve undo/redo, dirty flags, selection, adjoins, layers, and viewport refresh scope as one transaction. A visual
  result that cannot be undone correctly is not complete

## C++ style and naming

- Format new code consistently with the surrounding source and `.editorconfig`: UTF-8, four spaces, no tabs, trimmed
  trailing whitespace, and a final newline
- Use Allman braces for functions and control blocks
- Put spaces after control-flow keywords and inside their parentheses: `if ( condition )`, `for ( item : items )`,
  `while ( condition )`, and `switch ( value )`. Ordinary function calls remain compact: `updateSurface(surface)`
- Types and enum types use `PascalCase`; functions and local variables use `lowerCamelCase`; private data members use a
  trailing underscore; compile-time constants use the established `kPascalCase` form; namespaces and files use
  lowercase or `snake_case` matching their subsystem
- Prefer small cohesive functions, explicit value types, `const`, `std::optional`, RAII, standard containers, and
  non-owning views with clear lifetimes. Avoid owning raw pointers and hidden global mutable state
- Keep public headers narrow. Do not expose Qt types through core APIs
- Preserve existing terminology where it is part of Sith/JED formats or tested editor behavior, even when the legacy
  name is unusual. Explain compatibility names rather than silently renaming serialized concepts
- Treat warnings as defects. Do not suppress a warning, exception, failed precondition, or diagnostic without proving
  why the condition is safe

## Correctness and security

- File parsers handle untrusted input. Check sizes, counts, offsets, integer overflow, allocation budgets, graph/index
  references, and partial reads before publishing data. Parsing failure must be explicit and failure-atomic
- Geometry changes must preserve structural validity, reciprocal adjoins, stable entity identity where required, and
  deterministic results within documented tolerances
- Picking and snapping changes require coverage for perspective and orthographic projection, occlusion/X-Ray rules,
  transformed grids, multiple candidates, modifier behavior, and active/multi-selection state when applicable
- Never write editor previews into persistent level data. Commit only after validation and cancel by restoring the
  exact baseline
- Avoid broad rebuilds or full-level copies on hot paths without measurement and a documented reason. Preserve existing
  partial-update and caching contracts

## Workspace and build coordination

- Use one writer at a time for each source file and build tree. Coordinate with other contributors before editing a
  shared file, and never run concurrent Ninja writers against the same build directory. Use a separate build directory
  when genuinely parallel work is required
- Start a fresh build session by following `docs/README.md` for the Qt kit, CMake preset, focused target, and complete
  Release gate. Set `QTDIR` to one exact compatible Qt kit containing `bin/qmake.exe`, rather than a parent directory
  containing multiple kits; if Qt is unavailable, use the documented core-only configuration rather than inventing a
  replacement setup
- Keep personal game installations, proprietary level data, and unlicensed runtime assets outside the repository and
  build outputs. Use small synthetic or project-owned fixtures with clear provenance for tests
- Test runners must not minimize, restore, reposition, or close unrelated applications. Foreground-dependent native
  GUI/input tests activate their own windows and require an idle test desktop. When the desktop is in use, run
  background-safe checks and report native checks as pending; do not weaken guards or count a partial run as the full
  Release gate. Release validation remains incomplete until the complete Release CTest gate passes.

## Tests are mandatory

Every behavior change must include or update tests. A change is not finished until the relevant tests pass.

Test-driven development (TDD) is mandatory for every functional code change. Use a regression-first
Red-Green-Refactor workflow:

1. **Red:** express the reported bug or requested contract in a focused automated test before changing production
   behavior.
2. Confirm that the new test fails for the expected reason.
3. **Green:** make the smallest production change that satisfies the contract.
4. **Refactor:** improve the implementation without changing behavior while keeping the focused tests green.
5. Add relevant boundary and regression coverage, then run the complete Release suite.

If a failing test genuinely cannot be written first, record the reason and add the closest reliable automated coverage
before handoff. Documentation-only changes are exempt from application testing but still require link, example, and
format validation.

- Add focused unit tests for deterministic helpers and core algorithms
- Add a regression test that fails for the reported bug and passes with the fix
- Add UI/integration coverage for input routing, selection transitions, dialog state, settings, and undo/redo when those
  behaviors are involved
- Test error paths, boundaries, malformed input, and cancellation as well as the happy path
- Do not weaken assertions, increase arbitrary timing delays, skip tests, or replace exact behavior with a smoke test
  merely to obtain a passing run
- Isolate settings, recovery directories, temporary files, and persisted window state. Tests must be repeatable and
  must not depend on a developer's `sed.ini`, build artifacts, game installation, or loose personal fixture
- Run the smallest relevant test target during iteration, then run the complete Release CTest suite before handoff:

```powershell
$env:QTDIR = "<Qt MSVC kit directory>"
cmake --preset msvc-ninja
cmake --build --preset msvc-ninja-release --parallel 1
ctest --preset msvc-ninja-release
```

Replace `<Qt MSVC kit directory>` with the Qt kit directory on the current system, which contains `bin/qmake.exe`.

Report the exact commands and results. Never claim a build, test, visual check, or legacy comparison that was not
actually performed. If an environment prevents a required check, state the limitation plainly.

## Documentation and delivery

- Update user documentation when a level-designer workflow, label, shortcut, preference, file behavior, or limitation
  changes. Update architecture/algorithm/format docs with the code contract in the same change
- Keep living documentation date-free. Describe the current contract, not a session diary, temporary handoff, or
  point-in-time status report
- Do not commit build trees, IDE state, deployed Qt binaries, recovery files, logs, crash dumps, or personal level
  samples. Add legitimate test inputs under the owning test asset directory and explain their provenance
- Preserve unrelated local changes. Avoid destructive version-control commands and broad mechanical rewrites that hide
  semantic edits
- Handoffs must summarize behavior, files changed, tests run, remaining risks, and any manual verification still
  required

## AI-specific requirements

- AI agents are welcome and expected to follow the same engineering bar as human contributors
- Follow the progressive documentation onboarding and mandatory TDD workflow above before editing production
  code. Do not start implementation from a task description alone
- State assumptions and verify them against source, tests, or authoritative upstream references. Do not invent APIs,
  legacy behavior, test results, or user-visible guarantees
- Keep changes scoped to the requested outcome. Ask before a materially broader redesign, dependency addition, format
  change, or destructive migration
- Review generated patches line by line. The contributor remains responsible for correctness, licensing, security,
  maintainability, and test coverage regardless of who or what produced the text
- Never add secrets, private user data, proprietary game assets, generated binaries, or machine-specific paths
