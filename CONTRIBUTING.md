# Contributing to SED

Thank you for helping make SED a better level editor. Contributions of every size are welcome, whether you design
levels, write code, improve documentation, test workflows, or simply take the time to describe a problem clearly.

## 🌟 Ways to contribute

You can help SED by:

- reporting a reproducible bug or confusing level-design workflow
- proposing a new feature or a focused improvement
- testing fixes with different games, level formats, projections, and render modes
- improving the User Guide or developer documentation
- adding tests, fixing code, improving performance, or implementing features
- reviewing another contribution and sharing constructive feedback

AI-assisted code contributions are welcome. They must meet the additional requirements in
[AI-assisted code contributions](#ai-assisted-code).

## 🧭 Before you start

- Search existing [issues](https://github.com/smlu/SED/issues) and pull requests before opening a duplicate
- Open an issue before starting a large feature, architectural change, file-format change, or broad refactor. A small,
  well-understood fix or documentation correction can go directly to a pull request
- Describe one problem or proposal at a time. Keep unrelated cleanup and formatting in separate contributions
- For code changes, read the [Developer Documentation](docs/README.md) and the documents for the subsystem you intend
  to change
- Never submit credentials, private data, proprietary game assets, generated binaries, build output, recovery files,
  or material you do not have permission to distribute

## 🌿 Fork, branch, and pull request

1. Fork the repository on GitHub and clone your fork locally.
2. Add the main repository as an `upstream` remote, then fetch the current default branch.
3. Create one focused topic branch from that branch; keep unrelated cleanup in a separate branch.
4. Make and verify the change, then push the topic branch to your fork:

   ```text
   git clone https://github.com/<your-account>/SED.git
   cd SED
   git remote add upstream https://github.com/smlu/SED.git
   git fetch upstream
   git switch -c <topic-branch> upstream/<default-branch>
   git add <changed-files>
   git commit -m "<short description>"
   git push -u origin <topic-branch>
   ```

5. Open a pull request from your fork's topic branch to the repository's current default branch. Fill in the problem,
   visible result, exact verification commands and results, documentation changes, and any remaining manual checks.

Use synthetic or minimal fixtures that you have the right to distribute. Do not add copyrighted game data, proprietary
levels, private project files, or screenshots and samples whose redistribution is not permitted.

## 🐞 Reporting a bug

A useful bug report is detailed but direct. Please include:

- a short title and one-sentence description of how the problem affects level editing
- the SED version or build and your Windows version
- the game and project type, such as JK, MotS, or IJIM
- exact steps that reproduce the problem from a known starting point
- what you expected to happen and what actually happened
- whether it happens every time or only under certain conditions
- relevant edit mode, tool, projection, render mode, selection modifiers, grid settings, and preferences
- a screenshot, short recording, diagnostic text, or the smallest distributable sample that makes the issue clear

Remove unrelated history and speculation. If you have an idea about the cause, label it as an investigation note rather
than presenting it as confirmed fact.

For a crash or data-loss problem, also explain what was being edited, whether recovery was offered, and which files can
be shared legally. Do not attach copyrighted game data or private project material.

## 💡 Proposing a feature or enhancement

Start with the level designer’s need rather than an implementation. A strong proposal explains:

- the workflow or limitation that should improve
- which supported games, project types, edit modes, or tools are affected
- the desired behavior using a short, concrete example
- how the user would discover, enable, apply, or cancel the feature
- important interactions with selection, undo/redo, snapping, saving, and preferences
- concise acceptance criteria that would show the proposal is complete

Mock-ups and reference images are welcome when they clarify interaction or layout. Keep the proposal focused; unrelated
ideas are easier to discuss as separate issues.

## 🧑‍💻 Contributing code or documentation

1. Keep the change focused on one issue or coherent outcome.
2. Inspect the current implementation and existing tests before deciding how to change it.
3. Follow the project’s C++ code style and naming conventions described below, and use the
   [`.editorconfig`](.editorconfig) settings supported by your editor.
4. Create new tests or update existing tests for every behavior change; this is mandatory.
5. Preserve undo/redo, selection, dirty state, adjoins, layers, and recovery as one consistent edit transaction.
6. Update designer, architecture, algorithm, format, and changelog documentation when their contracts change.
7. Run focused tests while working, then run the complete Release test suite before submitting code.
8. Review the final diff for accidental formatting changes, generated files, machine paths, secrets, and unrelated work.

Documentation-only changes do not require an application rebuild, but links, examples, terminology, and formatting
should still be checked. For screenshot privacy and neutral synthetic capture paths, follow the [source publication
guidance](docs/README.md#source-publication-and-binary-packaging). Report those checks in the pull request and state
that application tests were not run when the change is documentation-only. For a source publication or binary handoff,
also follow the [release preparation checklist](docs/README.md#release-preparation-checklist).

## 🎨 C++ code style

Use [`.editorconfig`](.editorconfig) for shared whitespace and editor settings. In VS Code, install the EditorConfig
extension listed in the [workspace recommendations](.vscode/extensions.json); it applies supported properties such
as `trim_trailing_whitespace` and `insert_final_newline` when you save a file. Saving cleans that file, while
`.editorconfig` does not scan or enforce rules across unsaved files. The workspace also sets
`files.trimFinalNewlines` to remove extra blank lines at end of file on save. Review `git diff --check` before
submitting. Match nearby code where a convention is not configured, and do not reformat unrelated files.

- Use UTF-8, four spaces per indentation level, no tabs, no trailing whitespace, and a final newline
- Use Allman braces for namespaces, types, functions, lambdas, and control blocks. Always use braces for control-flow
  bodies, including single statements
- Put a space after control-flow keywords and inside their condition parentheses: `if ( condition )`,
  `for ( item : items )`, `while ( condition )`, and `switch ( value )`
- Do not add spaces inside ordinary function-call parentheses: `updateSurface(surface)`
- Put spaces around binary, assignment, and conditional operators and after commas
- Types and enum types use `PascalCase`
- Functions and local variables use `lowerCamelCase`
- Private data members use a trailing underscore, such as `snapRadius_`
- Compile-time constants use `kPascalCase`, such as `kDefaultSnapRadius`
- Namespaces and files use lowercase or `snake_case` following their subsystem
- Prefer small cohesive functions, explicit value types, `const`, RAII, standard containers, and clear ownership
- Avoid owning raw pointers, hidden global mutable state, broad public headers, and Qt types in `src/core` APIs

Example:

```cpp
namespace sed
{

constexpr int kMinimumVertexCount = 3;

class SurfaceValidator
{
public:
    bool hasValidVertices(const std::vector<int>& vertexIndices) const;

private:
    bool enabled_ = true;
};

bool SurfaceValidator::hasValidVertices(const std::vector<int>& vertexIndices) const
{
    if ( !enabled_ || vertexIndices.size() < kMinimumVertexCount )
    {
        return false;
    }

    for ( const int vertexIndex : vertexIndices )
    {
        if ( vertexIndex < 0 )
        {
            return false;
        }
    }

    return true;
}

} // namespace sed
```

## 🧪 Tests and verification

All coding contributions must be properly tested. A successful compilation or a manual check alone is not enough.
Every functional change follows Red–Green–Refactor: add a focused regression, confirm its expected failure, make the
smallest production change, and keep the regression passing while refining the implementation. These requirements
apply to human and AI-assisted contributions.

- Bug fixes must add a regression test that fails for the reported behavior before the fix and passes afterward
- Deterministic algorithms and helpers need focused unit tests
- User-interface changes need interaction and state coverage where practical
- File readers need valid, boundary, malformed-input, and failure-atomicity tests
- Geometry, picking, snapping, transforms, and lighting need coverage for relevant projections, modifiers, candidate
  ordering, selection states, and error paths
- Tests must not depend on personal settings, installed game files, existing build output, current time, or loose local
  fixtures
- Foreground-dependent native tests must activate only their own windows and use an idle test desktop. Test runners
  must not minimize, restore, reposition, or close unrelated applications. If the desktop is in use, run background-safe
  checks and report native checks as pending; a partial run does not satisfy the complete Release gate
- Performance changes must include repeatable before/after measurements, the fixture and toolchain used, and the
  measured cost (such as latency, allocation, resolver work, or uploads). Explain remaining measurement limits

Run the relevant test target during development, then run the complete Windows Release gate from an x64 Developer
PowerShell for Visual Studio (or PowerShell started from an x64 Native Tools Command Prompt):

```powershell
$env:QTDIR = "<Qt MSVC kit directory>"
$env:QT_QPA_PLATFORM_PLUGIN_PATH = Join-Path $env:QTDIR 'plugins/platforms'
cmake --preset msvc-ninja
cmake --build --preset msvc-ninja-release --parallel 1
ctest --preset msvc-ninja-release
```

Replace `<Qt MSVC kit directory>` with the Qt kit directory on your system, which contains `bin/qmake.exe`.
The platform-plugin path lets deployed offscreen tests load the selected kit's `qoffscreen.dll` when the test directory
contains only `qwindows.dll`; each test retains its configured Qt platform.

Report the exact commands and results in the pull request. Never weaken an assertion, skip a failing test, or add an
arbitrary delay merely to make the suite pass.

## 📬 Pull requests

A pull request should include:

- the issue it fixes or advances
- the level-designer problem and the visible result
- a concise explanation of the chosen implementation and its boundaries
- tests added and exact test commands and results
- screenshots or a short recording for visible interface changes
- remaining risks, limitations, and manual checks
- documentation updated by the change

Draft pull requests are welcome for early feedback. Reviewers may request a smaller scope, clearer reproduction steps,
additional boundary tests, original-editor evidence, performance measurements, or improved documentation. Please keep
the discussion constructive and be prepared to refine the contribution.

<a id="ai-assisted-code"></a>

## 🤖 AI-assisted code contributions

AI tools may investigate, implement, test, review, and document work. Before an AI agent changes code, it must read and
follow [`AGENTS.md`](AGENTS.md), including its progressive documentation-onboarding rules.

Test-driven development is required for AI-assisted behavior changes:

1. Express the bug or requested contract in a focused automated test.
2. Run the test and confirm that it fails for the expected reason.
3. Make the smallest implementation change that satisfies the contract.
4. Run the focused tests, add boundary and regression coverage, and then run the complete Release suite.

If a failing test genuinely cannot be written first, explain why in the pull request and add the closest reliable
automated coverage before submission. This exception is not permission to submit an untested behavior change.

The submitting contributor must also:

- understand and review every submitted line
- verify claims against the current repository or an authoritative upstream source
- disclose material AI assistance in the pull-request description
- identify tests created by the AI and tests independently run by the contributor
- resolve speculative, contradictory, or fabricated output before submission
- confirm that prompts, credentials, private data, proprietary assets, and unlicensed generated material are absent
- remain accountable for correctness, security, licensing, maintainability, and project fit

Do not use AI to bypass a failing test, weaken assertions, mass-rewrite unrelated code, invent legacy behavior, or
fabricate build and verification results. An AI-generated patch without review evidence, suitable tests, and a clear
explanation is not ready for contribution.

## ⚖️ License

By contributing, you agree that your contribution is distributed under the repository’s GNU General Public License
version 3. Submit only code, documentation, and assets that you have the right to contribute, and preserve all required
attribution and license notices.
