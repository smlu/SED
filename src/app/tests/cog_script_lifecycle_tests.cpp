#include "test_common.hpp"

#include "sed/assets/jkl.hpp"
#include "sed/assets/sed.hpp"
#include "main_window/main_window.hpp"
#include "sed/text.hpp"

#include <QAbstractButton>
#include <QApplication>
#include <QCoreApplication>
#include <QDir>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QAction>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QTabWidget>
#include <QTemporaryDir>
#include <QTimer>

#include <array>
#include <filesystem>
#include <cstddef>
#include <fstream>
#include <functional>
#include <iterator>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

namespace sed::app_tests
{
namespace
{

bool writeText(const std::filesystem::path& path, const std::string_view text)
{
    std::error_code ec{};
    std::filesystem::create_directories(path.parent_path(), ec);
    if (ec)
    {
        return false;
    }

    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    if (!out)
    {
        return false;
    }
    out.write(text.data(), static_cast<std::streamsize>(text.size()));
    return static_cast<bool>(out);
}

std::string readFileText(const std::filesystem::path& path)
{
    std::ifstream in(path, std::ios::binary);
    if (!in)
    {
        return {};
    }
    return std::string(
        std::istreambuf_iterator<char>(in),
        std::istreambuf_iterator<char>());
}

std::vector<std::string> exportedCogRowTokens(
    const std::string_view engineText,
    const std::string_view scriptName)
{
    std::istringstream input{ std::string(engineText) };
    std::string line{};
    bool inCogsSection = false;
    while (std::getline(input, line))
    {
        const std::vector<std::string_view> tokens = text::splitWords(line);
        if (tokens.size() >= 2 && tokens[0] == "Section:"
            && text::iequalsAscii(tokens[1], "cogs"))
        {
            inCogsSection = true;
            continue;
        }
        if (!inCogsSection)
        {
            continue;
        }
        if (!tokens.empty() && tokens[0] == "END")
        {
            break;
        }
        if (tokens.size() < 2 || tokens[0] != "0:" || tokens[1] != scriptName)
        {
            continue;
        }

        std::vector<std::string> result{};
        result.reserve(tokens.size());
        for (const std::string_view token : tokens)
        {
            result.emplace_back(token);
        }
        return result;
    }
    return {};
}

CogValue makeIntValue(const std::string_view name, const std::int32_t value)
{
    CogValue out{};
    out.name = std::string(name);
    out.cogType = CogType::int_;
    out.type = ValueType::integer;
    out.intValue = value;
    return out;
}

CogValue makeFloatValue(const std::string_view name, const double value)
{
    CogValue out{};
    out.name = std::string(name);
    out.cogType = CogType::float_;
    out.type = ValueType::floating;
    out.floatValue = value;
    return out;
}

CogValue makeRawJklValue(const std::size_t index, const std::string_view raw)
{
    CogValue out{};
    out.name = "param" + std::to_string(index);
    out.type = ValueType::str;
    out.cogType = CogType::unk;
    out.stringValue = std::string(raw);
    return out;
}

CogValue* findValue(Cog& cog, const std::string_view name)
{
    for (CogValue& value : cog.values)
    {
        if (text::iequalsAscii(value.name, name))
        {
            return &value;
        }
    }
    return nullptr;
}

const CogValue* findValue(const Cog& cog, const std::string_view name)
{
    for (const CogValue& value : cog.values)
    {
        if (text::iequalsAscii(value.name, name))
        {
            return &value;
        }
    }
    return nullptr;
}

bool cogValuesEqual(
    const std::vector<CogValue>& lhs,
    const std::vector<CogValue>& rhs)
{
    if (lhs.size() != rhs.size())
    {
        return false;
    }
    for (std::size_t index = 0; index < lhs.size(); ++index)
    {
        const CogValue& a = lhs[index];
        const CogValue& b = rhs[index];
        if (a.name != b.name || a.type != b.type || a.cogType != b.cogType
            || a.intValue != b.intValue || a.floatValue != b.floatValue
            || a.stringValue != b.stringValue || a.local != b.local
            || a.description != b.description || a.mask != b.mask)
        {
            return false;
        }
    }
    return true;
}

editor::DocumentState stateWithCogs(
    const std::string_view scriptName,
    const std::vector<std::vector<CogValue>>& values)
{
    editor::DocumentState state{};
    state.level.kind = ProjectType::JKDF2;
    state.level.header.version = 1;
    state.level.cogs.reserve(values.size());
    for (std::size_t index = 0; index < values.size(); ++index)
    {
        Cog cog{};
        cog.id = static_cast<std::int32_t>(index + 1);
        cog.num = static_cast<std::int32_t>(index);
        cog.name = std::string(scriptName);
        cog.values = values[index];
        state.level.cogs.push_back(std::move(cog));
    }
    return state;
}

editor::DocumentState stateWithNamedCogs(
    const std::vector<std::string>& scriptNames,
    const std::vector<std::vector<CogValue>>& values)
{
    editor::DocumentState state{};
    state.level.kind = ProjectType::JKDF2;
    state.level.header.version = 1;
    state.level.cogs.reserve(scriptNames.size());
    for (std::size_t index = 0; index < scriptNames.size(); ++index)
    {
        Cog cog{};
        cog.id = static_cast<std::int32_t>(index + 1);
        cog.num = static_cast<std::int32_t>(index);
        cog.name = scriptNames[index];
        if (index < values.size())
        {
            cog.values = values[index];
        }
        state.level.cogs.push_back(std::move(cog));
    }
    return state;
}

editor::DocumentState rawJklState(
    const std::string_view scriptName,
    const std::string_view integerToken,
    const std::string_view surfaceToken)
{
    editor::DocumentState state = stateWithCogs(
        scriptName,
        {{ makeRawJklValue(0, integerToken),
            makeRawJklValue(1, surfaceToken) }});
    state.level.sectors.resize(1);
    state.level.sectors[0].num = 0;
    state.level.sectors[0].surfaces.resize(1);
    state.level.sectors[0].surfaces[0].num = 0;
    state.level.importedJklSurfaceRefs = { SurfaceRef{ 0, 0 } };
    return state;
}

editor::DocumentState rawJklSurfaceState(
    const std::string_view scriptName,
    const std::string_view surfaceToken)
{
    editor::DocumentState state = stateWithCogs(
        scriptName,
        {{ makeRawJklValue(0, surfaceToken) }});
    state.level.sectors.resize(1);
    state.level.sectors[0].num = 0;
    state.level.sectors[0].surfaces.resize(1);
    state.level.sectors[0].surfaces[0].num = 0;
    state.level.importedJklSurfaceRefs = { SurfaceRef{ 0, 0 } };
    return state;
}

CogValue makePointerValue(
    const std::string_view name,
    const CogType cogType,
    const std::int32_t value)
{
    CogValue out{};
    out.name = std::string(name);
    out.cogType = cogType;
    out.type = ValueType::ptr;
    out.intValue = value;
    return out;
}

CogValue makeVectorValue(
    const std::string_view name,
    const std::string_view value)
{
    CogValue out{};
    out.name = std::string(name);
    out.cogType = CogType::vect;
    out.type = ValueType::vect;
    out.stringValue = std::string(value);
    return out;
}

CogValue makeResourceValue(
    const std::string_view name,
    const CogType cogType,
    const std::string_view value)
{
    CogValue out{};
    out.name = std::string(name);
    out.cogType = cogType;
    out.type = ValueType::str;
    out.stringValue = std::string(value);
    return out;
}

editor::DocumentState makeCogLifecycleState(
    const ProjectType kind,
    const std::string_view scriptName)
{
    editor::DocumentState state = stateWithCogs(
        scriptName,
        {{ makePointerValue("thingRef", CogType::thing, 0),
            makePointerValue("sectorRef", CogType::sec, 0),
            makePointerValue("surfaceRef", CogType::surface, 0),
            makeVectorValue("direction", "(1/2/3)"),
            makeResourceValue("soundRef", CogType::wav, "door_open.wav"),
            makeIntValue("count", 42),
            makeFloatValue("amount", 2.5) }});
    state.level.kind = kind;
    state.level.header.version = kind == ProjectType::IJIM ? 3 : 1;

    Sector& sector = state.level.sectors.emplace_back();
    sector.num = 0;
    sector.vertices = {
        Vertex{ 0.0, 0.0, 0.0 },
        Vertex{ 1.0, 0.0, 0.0 },
        Vertex{ 0.0, 1.0, 0.0 },
    };
    Surface surface{};
    surface.num = 0;
    surface.vertices = { 0, 1, 2 };
    surface.texVertices.resize(3);
    surface.normal = TVector{ 0.0, 0.0, 1.0 };
    sector.surfaces.push_back(std::move(surface));

    Thing& thing = state.level.things.emplace_back();
    thing.num = 0;
    thing.templateName = "lifecycle_probe";
    thing.name = "lifecycle_probe";
    thing.sector = 0;
    thing.x = 0.25;
    thing.y = 0.25;
    thing.z = 0.5;
    return state;
}

bool checkCogLifecycleValues(
    const Level& level,
    const std::string_view label)
{
    const std::string prefix = "COG lifecycle " + std::string(label) + ": ";
    const bool shape = level.cogs.size() == 1
        && level.cogs[0].values.size() == 9;
    check(shape, (prefix + "has exactly the nine public values").c_str());
    if (!shape)
    {
        return false;
    }

    const Cog& cog = level.cogs[0];
    const CogValue* thingRef = findValue(cog, "thingRef");
    const CogValue* sectorRef = findValue(cog, "sectorRef");
    const CogValue* surfaceRef = findValue(cog, "surfaceRef");
    const CogValue* direction = findValue(cog, "direction");
    const CogValue* soundRef = findValue(cog, "soundRef");
    const CogValue* count = findValue(cog, "count");
    const CogValue* amount = findValue(cog, "amount");
    const CogValue* newCount = findValue(cog, "newCount");
    const CogValue* newAmount = findValue(cog, "newAmount");

    check(thingRef != nullptr && thingRef->cogType == CogType::thing
            && thingRef->type == ValueType::ptr && thingRef->intValue == 0,
        (prefix + "thing reference is exact").c_str());
    check(sectorRef != nullptr && sectorRef->cogType == CogType::sec
            && sectorRef->type == ValueType::ptr && sectorRef->intValue == 0,
        (prefix + "sector reference is exact").c_str());
    check(surfaceRef != nullptr && surfaceRef->cogType == CogType::surface
            && surfaceRef->type == ValueType::ptr && surfaceRef->intValue == 0,
        (prefix + "surface reference is exact").c_str());
    check(direction != nullptr && direction->cogType == CogType::vect
            && direction->type == ValueType::vect
            && direction->stringValue == "(1/2/3)",
        (prefix + "vector is exact").c_str());
    check(soundRef != nullptr && soundRef->cogType == CogType::wav
            && soundRef->type == ValueType::str
            && soundRef->stringValue == "door_open.wav",
        (prefix + "resource string is exact").c_str());
    check(count != nullptr && count->cogType == CogType::int_
            && count->type == ValueType::integer && count->intValue == 42,
        (prefix + "authored integer is exact").c_str());
    check(amount != nullptr && amount->cogType == CogType::float_
            && amount->type == ValueType::floating
            && close(amount->floatValue, 2.5),
        (prefix + "authored float is exact").c_str());
    check(newCount != nullptr && newCount->cogType == CogType::int_
            && newCount->type == ValueType::integer && newCount->intValue == 17,
        (prefix + "new integer default is exact").c_str());
    check(newAmount != nullptr && newAmount->cogType == CogType::float_
            && newAmount->type == ValueType::floating
            && close(newAmount->floatValue, 4.5),
        (prefix + "new float default is exact").c_str());
    check(findValue(cog, "implementationOnly") == nullptr
            && findValue(cog, "implementationMessage") == nullptr,
        (prefix + "local and message declarations are excluded from placed values").c_str());
    return true;
}

bool processEventsUntil(const std::function<bool()>& predicate, const int timeoutMs = 1500)
{
    QElapsedTimer timer{};
    timer.start();
    while (!predicate() && timer.elapsed() < timeoutMs)
    {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 20);
    }
    return predicate();
}

bool drainCogDebounce()
{
    bool elapsed = false;
    QTimer::singleShot(350, [&elapsed]() { elapsed = true; });
    return processEventsUntil([&elapsed]() { return elapsed; }, 1000);
}

QString pathText(const std::filesystem::path& path)
{
    return QString::fromStdWString(path.wstring());
}

QString watcherNotificationPath(const std::filesystem::path& path)
{
    QString result = QDir::toNativeSeparators(pathText(path));
#ifdef Q_OS_WIN
    // QFileSystemWatcher is allowed to report a native separator spelling
    // and a different case than the resource locator. Exercise both forms
    // at the direct file-notification seam.
    result = result.toUpper();
#endif
    return result;
}

void runDuplicateCogReallocationRegression()
{
    QTemporaryDir temp{};
    check(temp.isValid(), "COG duplicate fixture creates a temporary root");
    if (!temp.isValid())
    {
        return;
    }

    const std::filesystem::path root(temp.path().toStdWString());
    const std::filesystem::path script = root / "cog" / "duplicate.cog";
    const bool written = writeText(
        script,
        "symbols\n"
        "int target\n"
        "end\n");
    check(written, "COG duplicate fixture writes its schema");
    if (!written)
    {
        return;
    }

    MainWindow window{};
    window.setAttribute(Qt::WA_DontShowOnScreen);
    window.setResourcePathsForTest({ root }, {}, root / "duplicate.sed");
    editor::DocumentState initial = stateWithCogs(
        "duplicate.cog",
        {{ makeIntValue("target", 42) },
            { makeRawJklValue(0, "unresolved-param") }});
    const std::vector<CogValue> firstValues = initial.level.cogs[0].values;
    const std::vector<CogValue> secondValues = initial.level.cogs[1].values;
    window.setDocumentStateForTest(std::move(initial));
    window.markCurrentDocumentAsSavedForTest();
    window.mutableDocumentStateForTest().selection.cogs.insert(0);
    window.mutableDocumentStateForTest().selection.cogs.insert(1);

    QAction* duplicate = window.findChild<QAction*>("edit.duplicate_cog");
    const std::size_t undoBefore = window.undoDepthForTest();
    // setDocumentStateForTest rebuilds properties but does not publish the
    // action enablement state. Refresh it before triggering the real QAction;
    // otherwise this regression would only test a disabled no-op.
    window.updateEditActionsForTest();
    if (duplicate)
    {
        duplicate->trigger();
    }

    const Level& level = window.documentStateForTest().level;
    const bool duplicated = level.cogs.size() == 4
        && cogValuesEqual(level.cogs[2].values, firstValues)
        && cogValuesEqual(level.cogs[3].values, secondValues)
        && window.documentStateForTest().selection.cogs
            == std::unordered_set<std::int32_t>{2, 3};
    check(duplicate && duplicate->isEnabled() && duplicated,
        "multi-selection COG duplication snapshots every source before vector growth and preserves authored values");
    check(window.undoDepthForTest() == undoBefore + 1,
        "multi-selection COG duplication creates one undo record");

    check(window.undoForTest() && window.documentStateForTest().level.cogs.size() == 2,
        "undo removes every reallocated COG duplicate atomically");
    check(window.redoForTest()
            && window.documentStateForTest().level.cogs.size() == 4
            && cogValuesEqual(
                window.documentStateForTest().level.cogs[2].values,
                firstValues)
            && cogValuesEqual(
                window.documentStateForTest().level.cogs[3].values,
                secondValues),
        "redo restores the exact multi-selection COG duplicates");
}

void runCogSchemaStatusAndTransactionRegression()
{
    QTemporaryDir temp{};
    check(temp.isValid(), "COG schema fixture creates a temporary root");
    if (!temp.isValid())
    {
        return;
    }

    const std::filesystem::path root(temp.path().toStdWString());
    const std::filesystem::path script = root / "cog" / "schema.cog";
    check(writeText(script, "symbols\nint target\nflex amount\nend\n"),
        "COG schema fixture writes its initial schema");

    MainWindow window{};
    window.setAttribute(Qt::WA_DontShowOnScreen);
    window.setResourcePathsForTest({ root }, {}, root / "schema.sed");
    window.setDocumentStateForTest(stateWithCogs(
        "schema.cog",
        {{ makeIntValue("target", 7), makeFloatValue("amount", 2.5) },
            { makeIntValue("target", 9), makeFloatValue("amount", 3.5) }}));
    window.markCurrentDocumentAsSavedForTest();
    window.syncCogScriptWatchesForTest();

    check(writeText(script, "symbols\nint target\nflex amount\nend\n"),
        "COG schema fixture rewrites an equivalent schema");
    const std::size_t beforeNoOp = window.undoDepthForTest();
    window.notifyCogScriptFileChangedForTest(watcherNotificationPath(script));
    check(drainCogDebounce(),
        "an equivalent external COG schema debounce is drained");
    check(window.documentStateForTest().level.cogs.size() == 2
            && window.documentStateForTest().level.cogs[0].values.size() == 2,
        "an equivalent external COG schema is processed");
    check(window.undoDepthForTest() == beforeNoOp,
        "an equivalent external COG schema does not create an undo record");

    check(writeText(script, "symbols\nint target\nflex amount\nend\n"),
        "COG schema fixture prepares a changed schema");
    // Replacing the schema with a new declaration exercises the transaction
    // path and the same-name/same-type value preservation policy.
    check(writeText(script, "symbols\nint target\nflex amount\nint added\nend\n"),
        "COG schema fixture writes a schema with one added declaration");
    const std::size_t beforeChange = window.undoDepthForTest();
    window.notifyCogScriptFileChangedForTest(pathText(script));
    check(processEventsUntil([&]()
        {
            return window.documentStateForTest().level.cogs[0].values.size() == 3;
        }),
        "a changed external COG schema updates every placed instance");
    const Level& changed = window.documentStateForTest().level;
    check(changed.cogs.size() == 2
            && changed.cogs[0].values.size() == 3
            && changed.cogs[1].values.size() == 3
            && findValue(changed.cogs[0], "target")
            && findValue(changed.cogs[0], "target")->intValue == 7
            && findValue(changed.cogs[1], "target")
            && findValue(changed.cogs[1], "target")->intValue == 9
            && findValue(changed.cogs[0], "amount")
            && close(findValue(changed.cogs[0], "amount")->floatValue, 2.5)
            && findValue(changed.cogs[0], "added")
            && findValue(changed.cogs[0], "added")->intValue == 0,
        "schema replacement preserves matching typed values and defaults new values");
    check(window.undoDepthForTest() == beforeChange + 1,
        "one schema replacement across multiple COGs creates one undo record");

    check(window.undoForTest()
            && window.documentStateForTest().level.cogs[0].values.size() == 2
            && window.documentStateForTest().level.cogs[1].values.size() == 2,
        "undo restores all COG schemas atomically");

    check(window.redoForTest()
            && window.documentStateForTest().level.cogs[0].values.size() == 3
            && window.documentStateForTest().level.cogs[1].values.size() == 3
            && findValue(window.documentStateForTest().level.cogs[0], "added")
            && findValue(window.documentStateForTest().level.cogs[1], "added"),
        "redo reapplies one schema replacement to every placed COG");
    check(window.undoForTest()
            && window.documentStateForTest().level.cogs[0].values.size() == 2
            && window.documentStateForTest().level.cogs[1].values.size() == 2,
        "undo remains available after redoing the schema transaction");

    check(writeText(script, "symbols\nend\n"),
        "COG schema fixture writes a valid empty schema");
    window.notifyCogScriptFileChangedForTest(pathText(script));
    check(processEventsUntil([&]()
        {
            return window.documentStateForTest().level.cogs[0].values.empty();
        }),
        "a valid empty schema clears old placed COG arguments");
    check(window.documentStateForTest().level.cogs[1].values.empty(),
        "a valid empty schema clears every placed instance");

    const std::size_t beforeMalformed = window.undoDepthForTest();
    const auto valuesBeforeMalformed = window.documentStateForTest().level.cogs[0].values;
    check(writeText(script, "symbols\ninte typoType\nend\n"),
        "COG schema fixture writes a malformed schema");
    window.notifyCogScriptFileChangedForTest(pathText(script));
    drainCogDebounce();
    check(window.undoDepthForTest() == beforeMalformed
            && cogValuesEqual(
                window.documentStateForTest().level.cogs[0].values,
                valuesBeforeMalformed),
        "a malformed schema leaves placed values and history unchanged");
}

void runCogDuplicateNameOccurrenceRegression()
{
    QTemporaryDir temp{};
    check(temp.isValid(), "COG duplicate-name fixture creates a temporary root");
    if (!temp.isValid())
    {
        return;
    }

    const std::filesystem::path root(temp.path().toStdWString());
    const std::filesystem::path script = root / "cog" / "duplicate_names.cog";
    check(writeText(
            script,
            "symbols\n"
            "int target\n"
            "int target\n"
            "end\n"),
        "COG duplicate-name fixture writes its initial duplicate schema");

    MainWindow window{};
    window.setAttribute(Qt::WA_DontShowOnScreen);
    window.setResourcePathsForTest(
        { root }, {}, root / "duplicate_names.sed");
    window.setDocumentStateForTest(stateWithCogs(
        "duplicate_names.cog",
        {{ makeIntValue("target", 17), makeIntValue("target", 29) }}));
    window.markCurrentDocumentAsSavedForTest();
    window.syncCogScriptWatchesForTest();

    check(writeText(
            script,
            "symbols\n"
            "int target\n"
            "int target\n"
            "int added\n"
            "end\n"),
        "COG duplicate-name fixture adds a declaration after the repeated names");
    const std::size_t before = window.undoDepthForTest();
    window.notifyCogScriptFileChangedForTest(pathText(script));
    check(processEventsUntil([&]()
        {
            return window.documentStateForTest().level.cogs[0].values.size() == 3;
        }),
        "COG duplicate-name schema update is applied");

    const auto& values = window.documentStateForTest().level.cogs[0].values;
    check(values.size() == 3
            && values[0].name == "target"
            && values[0].intValue == 17
            && values[1].name == "target"
            && values[1].intValue == 29
            && values[2].name == "added"
            && values[2].intValue == 0,
        "same-name COG values are preserved by declaration occurrence");
    check(window.undoDepthForTest() == before + 1,
        "same-name COG schema replacement creates one undo record");

    check(window.undoForTest()
            && window.documentStateForTest().level.cogs[0].values.size() == 2
            && window.documentStateForTest().level.cogs[0].values[0].intValue == 17
            && window.documentStateForTest().level.cogs[0].values[1].intValue == 29,
        "undo restores both same-name COG occurrences");
    check(window.redoForTest()
            && window.documentStateForTest().level.cogs[0].values.size() == 3
            && window.documentStateForTest().level.cogs[0].values[0].intValue == 17
            && window.documentStateForTest().level.cogs[0].values[1].intValue == 29,
        "redo restores the occurrence-aware same-name schema");
}

void runCogWatcherIdentityRegression()
{
    QTemporaryDir temp{};
    check(temp.isValid(), "COG watcher fixture creates a temporary root");
    if (!temp.isValid())
    {
        return;
    }

    const std::filesystem::path root(temp.path().toStdWString());
    const std::filesystem::path rootA = root / "project-a";
    const std::filesystem::path rootB = root / "project-b";
    const std::filesystem::path pathA = rootA / "cog" / "shared.cog";
    const std::filesystem::path pathB = rootB / "cog" / "shared.cog";
    check(writeText(pathA, "symbols\nint fromA\nend\n")
            && writeText(pathB, "symbols\nint fromB\nend\n"),
        "COG watcher fixture writes both project schemas");

    MainWindow window{};
    window.setAttribute(Qt::WA_DontShowOnScreen);
    window.setResourcePathsForTest({ rootA }, {}, rootA / "a.sed");
    window.setDocumentStateForTest(stateWithCogs(
        "shared.cog", {{ makeIntValue("fromA", 11) }}));
    window.markCurrentDocumentAsSavedForTest();
    window.syncCogScriptWatchesForTest();

    // Queue an A notification, then replace the document/resources with B
    // before its 250 ms debounce expires. The queued A work must not mutate B.
    window.notifyCogScriptFileChangedForTest(pathText(pathA));
    window.setResourcePathsForTest({ rootB }, {}, rootB / "b.sed");
    window.setDocumentStateForTest(stateWithCogs(
        "shared.cog", {{ makeIntValue("fromB", 22) }}));
    window.markCurrentDocumentAsSavedForTest();
    window.syncCogScriptWatchesForTest();
    drainCogDebounce();
    const Cog& current = window.documentStateForTest().level.cogs[0];
    check(current.values.size() == 1
            && current.name == "shared.cog"
            && findValue(current, "fromB")
            && findValue(current, "fromB")->intValue == 22
            && !findValue(current, "fromA"),
        "a debounced COG event from a replaced project is ignored");

    // Keep the resource mapping itself stable while replacing the document.
    // The old watcher captured only the path/key and would apply the queued
    // event to the new document; the document lifetime guard must reject it.
    MainWindow sameRootWindow{};
    sameRootWindow.setAttribute(Qt::WA_DontShowOnScreen);
    sameRootWindow.setResourcePathsForTest({ rootA }, {}, rootA / "a.sed");
    sameRootWindow.setDocumentStateForTest(stateWithCogs(
        "shared.cog", {{ makeIntValue("fromA", 11) }}));
    sameRootWindow.markCurrentDocumentAsSavedForTest();
    sameRootWindow.syncCogScriptWatchesForTest();
    sameRootWindow.notifyCogScriptFileChangedForTest(pathText(pathA));
    sameRootWindow.setCurrentPathForTitleForTest(rootA / "replacement.sed");
    sameRootWindow.setDocumentStateForTest(stateWithCogs(
        "shared.cog", {{ makeIntValue("replacement", 33) }}));
    sameRootWindow.markCurrentDocumentAsSavedForTest();
    sameRootWindow.syncCogScriptWatchesForTest();
    check(drainCogDebounce(),
        "same-root COG watcher fixture drains the stale replacement event");
    const Cog& replacementDocument =
        sameRootWindow.documentStateForTest().level.cogs[0];
    check(findValue(replacementDocument, "replacement")
            && !findValue(replacementDocument, "fromA"),
        "a queued COG event cannot mutate a replaced document with the same resource mapping");

    // A missing script in the replacement resource root must not fall back to
    // the old root's path. Editing the old file must therefore be ignored.
    const std::filesystem::path rootC = root / "project-c";
    const std::filesystem::path pathC = rootC / "cog" / "shared.cog";
    MainWindow missingReplacementWindow{};
    missingReplacementWindow.setAttribute(Qt::WA_DontShowOnScreen);
    missingReplacementWindow.setResourcePathsForTest(
        { rootA }, {}, rootA / "a.sed");
    missingReplacementWindow.setDocumentStateForTest(stateWithCogs(
        "shared.cog", {{ makeIntValue("fromA", 11) }}));
    missingReplacementWindow.markCurrentDocumentAsSavedForTest();
    missingReplacementWindow.syncCogScriptWatchesForTest();
    missingReplacementWindow.notifyCogScriptFileChangedForTest(pathText(pathA));
    missingReplacementWindow.setResourcePathsForTest(
        { rootC }, {}, rootC / "c.sed");
    missingReplacementWindow.setDocumentStateForTest(stateWithCogs(
        "shared.cog", {{ makeIntValue("fromC", 33) }}));
    missingReplacementWindow.markCurrentDocumentAsSavedForTest();
    missingReplacementWindow.syncCogScriptWatchesForTest();
    check(writeText(
            pathA,
            "symbols\n"
            "int editedOldRoot\n"
            "end\n"),
        "COG watcher fixture edits the old resource root after replacement");
    missingReplacementWindow.notifyCogScriptFileChangedForTest(pathText(pathA));
    check(drainCogDebounce(),
        "COG watcher fixture drains the old-root event after replacement");
    const Cog& missingReplacement =
        missingReplacementWindow.documentStateForTest().level.cogs[0];
    check(!std::filesystem::exists(pathC)
            && findValue(missingReplacement, "fromC")
            && !findValue(missingReplacement, "editedOldRoot"),
        "a missing replacement COG path never reuses the prior resource path");

    std::error_code createDirectoryError{};
    std::filesystem::create_directories(pathC.parent_path(), createDirectoryError);
    check(!createDirectoryError && !std::filesystem::exists(pathC),
        "COG watcher fixture creates the previously missing resource directory before its script");
    missingReplacementWindow.notifyCogScriptDirectoryChangedForTest(
        pathText(rootC.parent_path()));
    check(drainCogDebounce(),
        "COG watcher fixture drains the missing-directory watch migration");
    check(writeText(
            pathC,
            "symbols\n"
            "int appearedC\n"
            "end\n"),
        "COG watcher fixture creates the script after the directory watch migration");
    check(drainCogDebounce(),
        "COG watcher fixture drains the actual child-directory reappearance event");
    check(findValue(
                missingReplacementWindow.documentStateForTest().level.cogs[0],
                "appearedC"),
        "COG watcher observes a script after its missing directory is created");

    // A deleted external file must stay deleted. In-memory placed values are
    // retained while the missing resource is reported by the checker path.
    const std::filesystem::path currentPath = pathB;
    check(std::filesystem::remove(currentPath),
        "COG watcher fixture removes the active script");
    const std::size_t beforeDelete = window.undoDepthForTest();
    window.notifyCogScriptFileChangedForTest(pathText(currentPath));
    drainCogDebounce();
    check(!std::filesystem::exists(currentPath)
            && window.undoDepthForTest() == beforeDelete
            && findValue(window.documentStateForTest().level.cogs[0], "fromB")
            && findValue(window.documentStateForTest().level.cogs[0], "fromB")->intValue == 22,
        "external COG deletion preserves the in-memory instance without resurrecting the file");

    check(writeText(
            currentPath,
            "symbols\n"
            "int revived\n"
            "end\n"),
        "COG watcher fixture recreates the deleted script with a new schema");
    window.notifyCogScriptDirectoryChangedForTest(
        pathText(currentPath.parent_path()));
    check(drainCogDebounce(),
        "COG watcher fixture drains the reappearance debounce");
    check(findValue(window.documentStateForTest().level.cogs[0], "revived")
            && !findValue(window.documentStateForTest().level.cogs[0], "fromB"),
        "COG watcher applies a schema when a deleted script reappears");

    const std::filesystem::path replacement = currentPath.parent_path()
        / "shared.cog.tmp";
    check(writeText(
            replacement,
            "symbols\n"
            "int atomic\n"
            "end\n"),
        "COG watcher fixture writes an atomic replacement temporary file");
    std::error_code renameError{};
    std::filesystem::remove(currentPath, renameError);
    renameError.clear();
    std::filesystem::rename(replacement, currentPath, renameError);
    check(!renameError, "COG watcher fixture publishes the atomic replacement");
    window.notifyCogScriptDirectoryChangedForTest(
        pathText(currentPath.parent_path()));
    check(drainCogDebounce(),
        "COG watcher fixture drains the atomic replacement debounce");
    check(findValue(window.documentStateForTest().level.cogs[0], "atomic")
            && !findValue(window.documentStateForTest().level.cogs[0], "revived"),
        "COG watcher applies an atomic replacement exactly once");
}

void runCogWatcherCandidateRootsRegression()
{
    QTemporaryDir temp{};
    check(temp.isValid(), "COG candidate-root fixture creates a temporary root");
    if (!temp.isValid())
    {
        return;
    }

    const std::filesystem::path root(temp.path().toStdWString());
    const std::filesystem::path lowRoot = root / "resource-low";
    const std::filesystem::path highRoot = root / "resource-high";
    const std::filesystem::path lowScript = lowRoot / "cog" / "fallback.cog";
    const std::filesystem::path highScript = highRoot / "cog" / "fallback.cog";
    std::error_code directoryError{};
    std::filesystem::create_directories(lowScript.parent_path(), directoryError);
    directoryError.clear();
    std::filesystem::create_directories(highScript.parent_path(), directoryError);
    check(!directoryError,
        "COG candidate-root fixture creates both initially empty script directories");

    MainWindow window{};
    window.setAttribute(Qt::WA_DontShowOnScreen);
    window.setResourcePathsForTest(
        { lowRoot, highRoot }, {}, highRoot / "fallback.sed");
    window.setDocumentStateForTest(stateWithCogs(
        "fallback.cog", {{ makeIntValue("initial", 1) }}));
    window.markCurrentDocumentAsSavedForTest();
    window.syncCogScriptWatchesForTest();

    check(!std::filesystem::exists(lowScript)
            && !std::filesystem::exists(highScript),
        "COG candidate-root fixture starts with both resolver candidates missing");
    check(writeText(
            lowScript,
            "symbols\n"
            "int lowerAppeared\n"
            "end\n"),
        "COG watcher writes a script into the lower-priority root after initial sync");
    check(processEventsUntil(
              [&window]()
              {
                  return findValue(
                             window.documentStateForTest().level.cogs[0],
                             "lowerAppeared")
                      != nullptr;
              }),
        "COG watcher observes a lower-root script reappearing after an initially missing lookup");

    check(writeText(
            highScript,
            "symbols\n"
            "int highPreferred\n"
            "end\n"),
        "COG watcher writes the higher-priority candidate after lower-root recovery");
    check(processEventsUntil(
              [&window]()
              {
                  return findValue(
                             window.documentStateForTest().level.cogs[0],
                             "highPreferred")
                      != nullptr;
              }),
        "COG watcher selects the higher-priority candidate when it appears");

    check(std::filesystem::remove(highScript),
        "COG watcher removes the active higher-priority candidate");
    check(processEventsUntil(
              [&window]()
              {
                  const Cog& cog = window.documentStateForTest().level.cogs[0];
                  return findValue(cog, "lowerAppeared") != nullptr
                      && findValue(cog, "highPreferred") == nullptr;
              }),
        "COG watcher falls back to the lower candidate when the active file is deleted");
}

void runCogWatcherAliasRegression()
{
    QTemporaryDir temp{};
    check(temp.isValid(), "COG alias fixture creates a temporary root");
    if (!temp.isValid())
    {
        return;
    }

    const std::filesystem::path root(temp.path().toStdWString());
    const std::filesystem::path script = root / "cog" / "alias.cog";
    check(writeText(
            script,
            "symbols\n"
            "int authored\n"
            "end\n"),
        "COG alias fixture writes the shared script");

    MainWindow window{};
    window.setAttribute(Qt::WA_DontShowOnScreen);
    window.setResourcePathsForTest({ root }, {}, root / "alias.sed");
    window.setDocumentStateForTest(stateWithNamedCogs(
        { "cog/alias.cog", "cog\\alias.cog" },
        { { makeIntValue("authored", 7) },
            { makeIntValue("authored", 9) } }));
    window.markCurrentDocumentAsSavedForTest();
    window.syncCogScriptWatchesForTest();
    window.openCogScriptEditorForTest("alias.cog");

    auto* tabs = window.findChild<QTabWidget*>(
        QStringLiteral("cogScriptEditorTabs"));
    auto* editor = tabs && tabs->currentWidget()
        ? tabs->currentWidget()->findChild<QPlainTextEdit*>()
        : nullptr;
    const QString replacement = QStringLiteral(
        "symbols\n"
        "int authored\n"
        "int addedBySave\n"
        "end\n");
    if (editor)
    {
        editor->setPlainText(replacement);
    }

    const std::size_t beforeSave = window.undoDepthForTest();
    const bool saved = editor && window.saveCogScriptEditorForTest(0);
    check(saved, "COG alias fixture saves the shared editor buffer");
    check(readFileText(script) == replacement.toStdString(),
        "COG alias save publishes the shared script once");
    const auto& savedCogs = window.documentStateForTest().level.cogs;
    check(savedCogs.size() == 2
            && findValue(savedCogs[0], "authored")
            && findValue(savedCogs[0], "authored")->intValue == 7
            && findValue(savedCogs[0], "addedBySave")
            && findValue(savedCogs[1], "authored")
            && findValue(savedCogs[1], "authored")->intValue == 9
            && findValue(savedCogs[1], "addedBySave"),
        "slash and backslash COG aliases refresh both placed instances on an in-app save");
    check(window.undoDepthForTest() == beforeSave + 1,
        "slash and backslash COG aliases share one in-app schema undo transaction");

    check(window.undoForTest(),
        "COG alias fixture can undo the shared in-app schema transaction");
    const auto& undoneCogs = window.documentStateForTest().level.cogs;
    check(undoneCogs.size() == 2
            && findValue(undoneCogs[0], "authored")
            && findValue(undoneCogs[0], "authored")->intValue == 7
            && !findValue(undoneCogs[0], "addedBySave")
            && findValue(undoneCogs[1], "authored")
            && findValue(undoneCogs[1], "authored")->intValue == 9
            && !findValue(undoneCogs[1], "addedBySave"),
        "one COG alias undo restores both placed instances");
    check(drainCogDebounce(),
        "COG alias fixture drains duplicate notifications after the in-app save");
    check(window.redoForTest(),
        "COG alias fixture can redo the shared in-app schema transaction");

    const std::string external =
        "symbols\n"
        "int authored\n"
        "int addedExternally\n"
        "end\n";
    const std::size_t beforeExternal = window.undoDepthForTest();
    check(writeText(script, external),
        "COG alias fixture writes an external shared-script change");
    window.notifyCogScriptFileChangedForTest(watcherNotificationPath(script));
    check(drainCogDebounce(),
        "COG alias fixture drains the external shared-script notification");
    const auto& externalCogs = window.documentStateForTest().level.cogs;
    check(externalCogs.size() == 2
            && findValue(externalCogs[0], "addedExternally")
            && findValue(externalCogs[1], "addedExternally"),
        "slash and backslash COG aliases refresh both placed instances on an external change");
    check(window.undoDepthForTest() == beforeExternal + 1,
        "slash and backslash COG aliases share one external schema undo transaction");

    // A queued alias event must not cross a resource/document generation
    // boundary and mutate the replacement document.
    const std::filesystem::path replacementRoot = root / "replacement-root";
    MainWindow replacementWindow{};
    replacementWindow.setAttribute(Qt::WA_DontShowOnScreen);
    replacementWindow.setResourcePathsForTest(
        { root }, {}, root / "alias.sed");
    replacementWindow.setDocumentStateForTest(stateWithNamedCogs(
        { "cog\\alias.cog" }, {{ makeIntValue("beforeReplace", 11) }}));
    replacementWindow.markCurrentDocumentAsSavedForTest();
    replacementWindow.syncCogScriptWatchesForTest();
    replacementWindow.notifyCogScriptFileChangedForTest(pathText(script));
    replacementWindow.setResourcePathsForTest(
        { replacementRoot }, {}, replacementRoot / "replacement.sed");
    replacementWindow.setDocumentStateForTest(stateWithNamedCogs(
        { "cog/alias.cog" }, {{ makeIntValue("replacement", 33) }}));
    replacementWindow.markCurrentDocumentAsSavedForTest();
    replacementWindow.syncCogScriptWatchesForTest();
    check(drainCogDebounce(),
        "COG alias fixture drains a queued notification after replacement");
    const Cog& replacementCog =
        replacementWindow.documentStateForTest().level.cogs.front();
    check(findValue(replacementCog, "replacement")
            && !findValue(replacementCog, "addedExternally")
            && !findValue(replacementCog, "beforeReplace"),
        "a queued COG alias event cannot mutate a replacement document generation");
}

void runCogWatcherNestedPathRegression()
{
    QTemporaryDir temp{};
    check(temp.isValid(), "COG nested-path fixture creates a temporary root");
    if (!temp.isValid())
    {
        return;
    }

    const std::filesystem::path root(temp.path().toStdWString());
    const std::filesystem::path firstScript =
        root / "nested-a" / "shared.cog";
    const std::filesystem::path secondScript =
        root / "nested-b" / "shared.cog";
    check(writeText(
              firstScript,
              "symbols\n"
              "int first\n"
              "end\n")
            && writeText(
                secondScript,
                "symbols\n"
                "int second\n"
                "end\n"),
        "COG nested-path fixture writes same-basename schemas in separate directories");

    MainWindow window{};
    window.setAttribute(Qt::WA_DontShowOnScreen);
    window.setResourcePathsForTest({ root }, {}, root / "nested.sed");
    window.setDocumentStateForTest(stateWithNamedCogs(
        { "nested-a/shared.cog", "nested-b/shared.cog" },
        { { makeIntValue("first", 1) },
            { makeIntValue("second", 2) } }));
    window.markCurrentDocumentAsSavedForTest();
    window.syncCogScriptWatchesForTest();

    check(writeText(
              firstScript,
              "symbols\n"
              "int firstChanged\n"
              "end\n"),
        "COG nested-path fixture changes only the first same-basename script");
    window.notifyCogScriptFileChangedForTest(watcherNotificationPath(firstScript));
    check(drainCogDebounce(),
        "COG nested-path fixture drains the first-script notification");
    const auto& cogs = window.documentStateForTest().level.cogs;
    check(cogs.size() == 2
            && findValue(cogs[0], "firstChanged")
            && !findValue(cogs[0], "first")
            && findValue(cogs[1], "second")
            && !findValue(cogs[1], "firstChanged"),
        "same-basename COG paths refresh only the resolver-selected nested instance");
}

void runCogInternalSaveRegression()
{
    QTemporaryDir temp{};
    check(temp.isValid(), "COG save fixture creates a temporary root");
    if (!temp.isValid())
    {
        return;
    }

    const std::filesystem::path root(temp.path().toStdWString());
    const std::filesystem::path script = root / "cog" / "live.cog";
    check(writeText(
            script,
            "symbols\n"
            "int old\n"
            "end\n"),
        "COG save fixture writes the initial script");

    MainWindow window{};
    window.setAttribute(Qt::WA_DontShowOnScreen);
    window.setResourcePathsForTest({ root }, {}, root / "live.sed");
    window.setDocumentStateForTest(stateWithCogs(
        "live.cog", {{ makeIntValue("old", 7) }}));
    window.markCurrentDocumentAsSavedForTest();
    window.syncCogScriptWatchesForTest();
    window.openCogScriptEditorForTest("live.cog");

    auto* tabs = window.findChild<QTabWidget*>(
        QStringLiteral("cogScriptEditorTabs"));
    auto* editor = tabs && tabs->currentWidget()
        ? tabs->currentWidget()->findChild<QPlainTextEdit*>()
        : nullptr;
    const QString replacement = QStringLiteral(
        "symbols\n"
        "int fresh\n"
        "end\n");
    if (editor)
    {
        editor->setPlainText(replacement);
    }

    const std::size_t before = window.undoDepthForTest();
    const bool saved = editor && window.saveCogScriptEditorForTest(0);
    check(saved, "internal COG save writes the active editor buffer");
    check(readFileText(script) == replacement.toStdString(),
        "internal COG save publishes the exact fixed script vector");
    check(findValue(window.documentStateForTest().level.cogs[0], "fresh")
            && !findValue(window.documentStateForTest().level.cogs[0], "old"),
        "internal COG save refreshes the placed schema synchronously");
    check(window.undoDepthForTest() == before + 1,
        "internal COG save creates one schema undo record");

    check(window.undoForTest()
            && findValue(window.documentStateForTest().level.cogs[0], "old")
            && !findValue(window.documentStateForTest().level.cogs[0], "fresh"),
        "undo immediately removes the synchronously refreshed COG schema");
    check(drainCogDebounce(),
        "internal COG save fixture drains duplicate file notifications");
    check(findValue(window.documentStateForTest().level.cogs[0], "old")
            && !findValue(window.documentStateForTest().level.cogs[0], "fresh")
            && window.redoDepthForTest() == 1,
        "duplicate save notifications do not erase the COG schema redo");
    check(window.redoForTest()
            && findValue(window.documentStateForTest().level.cogs[0], "fresh"),
        "redo restores the synchronously refreshed COG schema");

    const std::size_t after = window.undoDepthForTest();
    check(window.saveCogScriptEditorForTest(0)
            && window.undoDepthForTest() == after,
        "re-saving an unchanged COG script does not create history");
}

void runCogRawJklHydrationRegression()
{
    QTemporaryDir temp{};
    check(temp.isValid(), "raw JKL COG fixture creates a temporary root");
    if (!temp.isValid())
    {
        return;
    }

    const std::filesystem::path root(temp.path().toStdWString());
    const std::filesystem::path script = root / "cog" / "raw.cog";
    check(writeText(
            script,
            "symbols\n"
            "int target\n"
            "surface face\n"
            "end\n"),
        "raw JKL COG fixture writes a typed schema");

    MainWindow window{};
    window.setAttribute(Qt::WA_DontShowOnScreen);
    window.setResourcePathsForTest({ root }, {}, root / "raw.sed");
    window.setDocumentStateForTest(rawJklState("raw.cog", "0x2a", "0"));
    window.markCurrentDocumentAsSavedForTest();
    window.syncCogScriptWatchesForTest();
    window.notifyCogScriptFileChangedForTest(pathText(script));
    check(drainCogDebounce(), "raw JKL COG fixture drains schema hydration");
    const Cog& hydrated = window.documentStateForTest().level.cogs[0];
    const CogValue* target = findValue(hydrated, "target");
    const CogValue* face = findValue(hydrated, "face");
    check(target != nullptr && target->type == ValueType::integer,
        "raw JKL integer argument hydrates with the script type");
    check(target != nullptr && target->intValue == 42,
        "raw JKL integer argument preserves its hexadecimal value");
    check(face != nullptr && face->type == ValueType::ptr
            && face->cogType == CogType::surface,
        "raw JKL surface argument hydrates with the script type");
    check(face != nullptr && face->intValue == 0,
        "raw JKL surface argument maps its wire index");
    check(face != nullptr && face->stringValue.empty(),
        "raw JKL surface argument clears its unresolved raw token");

    const std::filesystem::path invalidScript = root / "cog" / "invalid.cog";
    check(writeText(
            invalidScript,
            "symbols\n"
            "surface face\n"
            "end\n"),
        "raw JKL COG fixture writes an invalid surface mapping schema");
    MainWindow invalidWindow{};
    invalidWindow.setAttribute(Qt::WA_DontShowOnScreen);
    invalidWindow.setResourcePathsForTest({ root }, {}, root / "invalid.sed");
    invalidWindow.setDocumentStateForTest(
        rawJklSurfaceState("invalid.cog", "0x4"));
    invalidWindow.markCurrentDocumentAsSavedForTest();
    invalidWindow.syncCogScriptWatchesForTest();
    invalidWindow.notifyCogScriptFileChangedForTest(
        watcherNotificationPath(invalidScript));
    check(drainCogDebounce(),
        "raw JKL COG fixture drains invalid surface mapping hydration");
    const CogValue* invalidFace = findValue(
        invalidWindow.documentStateForTest().level.cogs[0], "face");
    check(invalidFace != nullptr && invalidFace->type == ValueType::ptr,
        "raw JKL out-of-range surface keeps its script type");
    check(invalidFace != nullptr && invalidFace->intValue == -1
            && invalidFace->stringValue.empty(),
        "raw JKL COG out-of-range hexadecimal surfaces become unset");
}

void runCogScriptNameSafetyRegression()
{
    MainWindow window{};
    window.setAttribute(Qt::WA_DontShowOnScreen);

    check(window.normalizedCogScriptNameForTest(QStringLiteral("  turret  "))
            == QStringLiteral("turret.cog"),
        "COG draft names receive the .cog extension after trimming");
    check(window.normalizedCogScriptNameForTest(QStringLiteral("turret.COG"))
            == QStringLiteral("turret.COG"),
        "COG draft names preserve an authored extension spelling");

    const QString parentName = QStringLiteral("../escape.cog");
    check(window.normalizedCogScriptNameForTest(parentName)
            == QStringLiteral("escape.cog")
            && window.cogScriptNameValidationErrorForTest(parentName)
                == QStringLiteral(
                    "Enter a COG script filename without directory components."),
        "COG draft validation rejects parent-directory components after basename normalization");

    const QString absoluteName = QStringLiteral("/tmp/escape.cog");
    check(window.normalizedCogScriptNameForTest(absoluteName)
            == QStringLiteral("escape.cog")
            && window.cogScriptNameValidationErrorForTest(absoluteName)
                == QStringLiteral(
                    "Enter a COG script filename without directory components."),
        "COG draft validation rejects absolute destinations");

    const QString windowsName = QStringLiteral("C:/chosen/directory/escape.cog");
    check(window.normalizedCogScriptNameForTest(windowsName)
            == QStringLiteral("escape.cog")
            && window.cogScriptNameValidationErrorForTest(windowsName)
                == QStringLiteral(
                    "Enter a COG script filename without directory components."),
        "COG draft validation rejects Windows-style absolute destinations on every host");

    const QString backslashName = QStringLiteral("C:\\chosen\\escape.cog");
    check(window.cogScriptNameValidationErrorForTest(backslashName)
            == QStringLiteral(
                "Enter a COG script filename without directory components."),
        "COG draft validation rejects backslash path components on every host");

    check(window.normalizedCogScriptNameForTest(QStringLiteral("../"))
            .isEmpty()
            && window.cogScriptNameValidationErrorForTest(QStringLiteral("../"))
                == QStringLiteral("Enter a script name."),
        "COG draft validation rejects a path with no basename");
}

void runCogScriptExtractionRegression()
{
    QTemporaryDir temp{};
    check(temp.isValid(), "COG extraction fixture creates a temporary root");
    if (!temp.isValid())
    {
        return;
    }

    const std::filesystem::path root(temp.path().toStdWString());
    const std::filesystem::path blockedDirectory = root / "cog";
    check(writeText(blockedDirectory, "directory path is intentionally blocked"),
        "COG extraction fixture creates a blocking path");

    MainWindow window{};
    window.setAttribute(Qt::WA_DontShowOnScreen);
    window.setResourcePathsForTest({ root }, {}, root / "extract.sed");
    window.setPrimaryResourceOverrideForTest(
        "archived.cog",
        []()
        {
            const std::string text =
                "symbols\n"
                "int archived\n"
                "end\n";
            std::vector<std::byte> bytes{};
            bytes.reserve(text.size());
            for (const unsigned char character : text)
            {
                bytes.push_back(static_cast<std::byte>(character));
            }
            return std::optional<std::vector<std::byte>>(std::move(bytes));
        });

    bool dismissed = false;
    QTimer::singleShot(0, [&dismissed]()
        {
            if (auto* message = qobject_cast<QMessageBox*>(
                    QApplication::activeModalWidget()))
            {
                dismissed = true;
                message->accept();
            }
        });
    window.openCogScriptEditorForTest("archived.cog");
    check(dismissed, "failed COG extraction reports a dismissible resource error");
    check(std::filesystem::is_regular_file(blockedDirectory)
            && !std::filesystem::exists(blockedDirectory / "archived.cog"),
        "failed COG extraction leaves the blocking destination unchanged without a partial file");
}

void runCogScriptReloadRaceRegression()
{
    QTemporaryDir temp{};
    check(temp.isValid(), "COG reload fixture creates a temporary root");
    if (!temp.isValid())
    {
        return;
    }

    const std::filesystem::path root(temp.path().toStdWString());
    const std::filesystem::path script = root / "cog" / "reload.cog";
    const std::string initial = "symbols\nint diskInitial\nend\n";
    const std::string local = "symbols\nint localBuffer\nend\n";
    const std::string versionA = "symbols\nint diskA\nend\n";
    const std::string versionB = "symbols\nint diskB\nend\n";
    check(writeText(script, initial), "COG reload fixture writes its initial script");

    MainWindow window{};
    window.setAttribute(Qt::WA_DontShowOnScreen);
    window.setResourcePathsForTest({ root }, {}, root / "reload.sed");
    window.openCogScriptEditorForTest("reload.cog");
    auto* tabs = window.findChild<QTabWidget*>(
        QStringLiteral("cogScriptEditorTabs"));
    auto* editor = tabs && tabs->currentWidget()
        ? tabs->currentWidget()->findChild<QPlainTextEdit*>()
        : nullptr;
    if (!editor)
    {
        check(false, "COG reload fixture finds the editor buffer");
        return;
    }
    editor->setPlainText(QString::fromStdString(local));
    editor->document()->setModified(true);

    bool sawPrompt = false;
    QTimer promptPoller{};
    QObject::connect(&promptPoller, &QTimer::timeout, &window,
        [&promptPoller, &sawPrompt, &script, &versionB]()
        {
            auto* message = qobject_cast<QMessageBox*>(
                QApplication::activeModalWidget());
            if (message
                && message->objectName() == QStringLiteral("sed.external_document_change_prompt")
                && message->windowTitle() == QStringLiteral("File changed on disk"))
            {
                sawPrompt = true;
                (void)writeText(script, versionB);
                for ( QAbstractButton* const button : message->buttons() )
                {
                    if ( button->text() == QStringLiteral("Reload") )
                    {
                        button->click();
                        break;
                    }
                }
                promptPoller.stop();
            }
            else if ( message && message->windowTitle() == QStringLiteral("Reload COG script") )
            {
                message->reject();
                promptPoller.stop();
            }
        });
    promptPoller.start(10);
    check(writeText(script, versionA),
        "COG reload fixture publishes the first external revision");

    const bool reloaded = processEventsUntil(
        [&]()
        {
            return sawPrompt
                && editor->toPlainText() == QString::fromStdString(versionB);
        },
        4000);
    promptPoller.stop();
    check(reloaded,
        "accepting an external COG Reload installs the latest bytes written while the named prompt was open");

    // Keep must leave the authored editor buffer dirty and
    // untouched while the newer disk revision remains available for a later
    // explicit reload.
    const std::string localAfterCancel =
        "symbols\nint localAfterCancel\nend\n";
    const std::string versionC = "symbols\nint diskC\nend\n";
    editor->setPlainText(QString::fromStdString(localAfterCancel));
    editor->document()->setModified(true);
    bool sawCancelPrompt = false;
    QTimer cancelPoller{};
    QObject::connect(&cancelPoller, &QTimer::timeout, &window,
        [&cancelPoller, &sawCancelPrompt, &script, &versionC]()
        {
            auto* message = qobject_cast<QMessageBox*>(
                QApplication::activeModalWidget());
            if (message
                && message->objectName() == QStringLiteral("sed.external_document_change_prompt")
                && message->windowTitle() == QStringLiteral("File changed on disk"))
            {
                sawCancelPrompt = true;
                (void)writeText(script, versionC);
                for ( QAbstractButton* const button : message->buttons() )
                {
                    if ( button->text() == QStringLiteral("Keep") )
                    {
                        button->click();
                        break;
                    }
                }
                cancelPoller.stop();
            }
            else if ( message && message->windowTitle() == QStringLiteral("Reload COG script") )
            {
                message->reject();
                cancelPoller.stop();
            }
        });
    cancelPoller.start(10);
    check(writeText(script, versionC),
        "COG reload fixture publishes a revision for the cancel path");
    window.notifyCogScriptFileChangedForTest(pathText(script));
    const bool canceled = processEventsUntil(
        [&]()
        {
            return sawCancelPrompt
                && editor->toPlainText() == QString::fromStdString(localAfterCancel);
        },
        4000);
    cancelPoller.stop();
    check(canceled && editor->document()->isModified()
            && readFileText(script) == versionC,
        "keeping an external COG change preserves the dirty editor buffer and disk revision");
}

void runCogDraftCreationRegression()
{
    QTemporaryDir temp{};
    check(temp.isValid(), "COG draft fixture creates a temporary root");
    if (!temp.isValid())
    {
        return;
    }

    const std::filesystem::path root(temp.path().toStdWString());
    const std::filesystem::path levelPath = root / "draft.sed";
    MainWindow window{};
    window.setAttribute(Qt::WA_DontShowOnScreen);
    window.setResourcePathsForTest({ root }, {}, levelPath);
    window.setCurrentPathForTitleForTest(levelPath);
    const std::size_t undoBefore = window.undoDepthForTest();

    // The pre-fix path reports a missing on-disk schema through a modal
    // warning before returning.  Close that warning in the regression so the
    // RED run remains finite while requiring the fixed path to show no modal.
    bool warningShown = false;
    QTimer warningPoller(&window);
    warningPoller.setSingleShot(true);
    QObject::connect(&warningPoller, &QTimer::timeout, &window,
        [&warningShown]()
        {
            if (auto* message = qobject_cast<QMessageBox*>(
                    QApplication::activeModalWidget()))
            {
                if (message->windowTitle() == QStringLiteral("New COG Script"))
                {
                    warningShown = true;
                    message->accept();
                }
            }
        });
    warningPoller.start(0);
    window.createNewCogScriptForTest(QStringLiteral("draft_schema_probe"));
    warningPoller.stop();

    const bool undoRemoved = window.undoForTest()
        && window.documentStateForTest().level.cogs.empty();
    const bool redoRestored = window.redoForTest()
        && window.documentStateForTest().level.cogs.size() == 1;
    check(undoRemoved && redoRestored,
        "undo and redo restore a newly-created diskless COG instance atomically");

    const Level& createdLevel = window.documentStateForTest().level;
    const bool placedDraftCreated = createdLevel.cogs.size() == 1
        && createdLevel.cogs.front().name == "draft_schema_probe.cog"
        && createdLevel.cogs.front().values.empty()
        && window.documentStateForTest().selection.cogs
            == std::unordered_set<std::int32_t>{0}
        && window.undoDepthForTest() == undoBefore + 1
        && !std::filesystem::exists(root / "cog" / "draft_schema_probe.cog");
    check(!warningShown && placedDraftCreated,
        "new COG creation adds one selected empty-schema instance without a missing-file warning or disk file");

    auto* tabs = window.findChild<QTabWidget*>(
        QStringLiteral("cogScriptEditorTabs"));
    auto* editor = tabs && tabs->currentWidget()
        ? tabs->currentWidget()->findChild<QPlainTextEdit*>()
        : nullptr;
    check(editor != nullptr,
        "a new COG draft opens an editor even when its script has no disk file");
    if (!editor)
    {
        return;
    }

    const QString schema = QStringLiteral(
        "symbols\n"
        "int draftValue\n"
        "end\n");
    editor->setPlainText(schema);
    editor->document()->setModified(true);
    check(window.saveCogScriptEditorForTest(0),
        "a new COG draft can save its first script file");
    const std::filesystem::path savedScript =
        root / "cog" / "draft_schema_probe.cog";
    check(std::filesystem::is_regular_file(savedScript)
            && readFileText(savedScript) == schema.toStdString(),
        "a new COG draft saves the exact authored schema text to its first file");

    const bool hydrated = processEventsUntil(
        [&window]()
        {
            const Level& level = window.documentStateForTest().level;
            return level.cogs.size() == 1
                && findValue(level.cogs.front(), "draftValue") != nullptr;
        });
    const Level& level = window.documentStateForTest().level;
    check(!warningShown && level.cogs.size() == 1
            && level.cogs.front().name == "draft_schema_probe.cog",
        "new COG creation adds one placed instance from the in-memory draft schema without a missing-file warning");
    check(hydrated && findValue(level.cogs.front(), "draftValue")->intValue == 0,
        "saving a new COG draft hydrates its placed instance from the saved schema with the unset integer default");
}

void runCogCrossLayerLifecycleRegression()
{
    QTemporaryDir temp{};
    check(temp.isValid(), "COG lifecycle fixture creates a temporary root");
    if (!temp.isValid())
    {
        return;
    }

    struct ProjectCase
    {
        ProjectType kind;
        const char* directory;
        const char* label;
    };
    const std::array<ProjectCase, 3> projects{
        ProjectCase{ ProjectType::JKDF2, "jkdf2", "JKDF2" },
        ProjectCase{ ProjectType::MOTS, "mots", "MotS" },
        ProjectCase{ ProjectType::IJIM, "ijim", "IJIM" },
    };

    const std::string initialSchema =
        "symbols\n"
        "thing thingRef\n"
        "sector sectorRef\n"
        "surface surfaceRef\n"
        "vector direction\n"
        "sound soundRef\n"
        "int count\n"
        "flex amount\n"
        "int implementationOnly local\n"
        "message implementationMessage\n"
        "end\n";
    const std::string changedSchema =
        "symbols\n"
        "thing thingRef\n"
        "sector sectorRef\n"
        "surface surfaceRef\n"
        "vector direction\n"
        "sound soundRef\n"
        "int count\n"
        "flex amount\n"
        "int newCount=17\n"
        "flex newAmount=4.5\n"
        "int implementationOnly local\n"
        "message implementationMessage\n"
        "end\n";

    for (const ProjectCase& project : projects)
    {
        const std::filesystem::path projectRoot =
            std::filesystem::path(temp.path().toStdWString()) / project.directory;
        const std::filesystem::path script =
            projectRoot / "cog" / "lifecycle.cog";
        check(writeText(script, initialSchema),
            (std::string("COG lifecycle ") + project.label
                + ": writes the initial synthetic schema").c_str());
        if (!std::filesystem::is_regular_file(script))
        {
            continue;
        }

        MainWindow window{};
        window.setAttribute(Qt::WA_DontShowOnScreen);
        window.setResourcePathsForTest(
            { projectRoot }, {}, projectRoot / "lifecycle.sed");
        window.setDocumentStateForTest(
            makeCogLifecycleState(project.kind, "lifecycle.cog"));
        window.markCurrentDocumentAsSavedForTest();
        window.syncCogScriptWatchesForTest();
        window.openCogScriptEditorForTest("lifecycle.cog");

        auto* tabs = window.findChild<QTabWidget*>(
            QStringLiteral("cogScriptEditorTabs"));
        auto* editor = tabs && tabs->currentWidget()
            ? tabs->currentWidget()->findChild<QPlainTextEdit*>()
            : nullptr;
        const std::string prefix = std::string("COG lifecycle ") + project.label + ": ";
        check(editor != nullptr, (prefix + "opens the editable schema").c_str());
        if (!editor)
        {
            continue;
        }

        editor->setPlainText(QString::fromStdString(changedSchema));
        editor->document()->setModified(true);
        check(window.saveCogScriptEditorForTest(0),
            (prefix + "saves the changed underlying schema").c_str());
        check(readFileText(script) == changedSchema,
            (prefix + "writes the exact changed schema text").c_str());

        const Level& edited = window.documentStateForTest().level;
        checkCogLifecycleValues(edited, project.label);
        if (edited.cogs.size() != 1 || edited.cogs[0].values.size() != 9)
        {
            continue;
        }

        // Native SED is the editor's complete authored-state checkpoint.  It
        // must retain the values materialized after the script edit before a
        // runtime text export is attempted.
        const std::vector<std::byte> nativeBytes =
            assets::saveToSedBytes(edited);
        const Level nativeRoundTrip = assets::loadFromSedBytes(nativeBytes);
        check(assets::sedLevelsEqual(edited, nativeRoundTrip),
            (prefix + "native SED save/load preserves the changed COG state").c_str());
        checkCogLifecycleValues(nativeRoundTrip, project.label);

        // The engine writer intentionally omits implementation-local and
        // message symbols.  They are present in the changed schema above but
        // absent from the nine placed values used by this Level.
        assets::ResourceLocator resources{};
        resources.addRoot(projectRoot);
        assets::JklSaveOptions saveOptions{};
        saveOptions.resources = &resources;
        std::ostringstream exported;
        assets::saveToJKL(nativeRoundTrip, exported, saveOptions);
        const std::string engineText = exported.str();
        const std::vector<std::string> expectedCogRow{
            "0:", "lifecycle.cog", "0", "0", "0", "(1/2/3)",
            "door_open.wav", "42", "2.5", "17", "4.5",
        };
        check(exportedCogRowTokens(engineText, "lifecycle.cog") == expectedCogRow,
            (prefix + "engine export writes the exact public COG row token vector").c_str());
        check(engineText.find("lifecycle.cog") != std::string::npos
                && engineText.find("implementationOnly") == std::string::npos
                && engineText.find("implementationMessage") == std::string::npos,
            (prefix + "engine export keeps the public row and excludes local/message symbols").c_str());

        assets::JklLoadOptions loadOptions{};
        loadOptions.projectType = project.kind;
        loadOptions.resources = &resources;
        std::istringstream engineInput(engineText);
        const Level engineRoundTrip = assets::loadFromJKL(engineInput, loadOptions);
        checkCogLifecycleValues(engineRoundTrip, project.label);
    }
}

} // namespace

void runCogScriptLifecycleTests()
{
    runDuplicateCogReallocationRegression();
    runCogSchemaStatusAndTransactionRegression();
    runCogDuplicateNameOccurrenceRegression();
    runCogWatcherIdentityRegression();
    runCogWatcherCandidateRootsRegression();
    runCogWatcherAliasRegression();
    runCogWatcherNestedPathRegression();
    runCogInternalSaveRegression();
    runCogRawJklHydrationRegression();
    runCogScriptNameSafetyRegression();
    runCogScriptExtractionRegression();
    runCogScriptReloadRaceRegression();
    runCogDraftCreationRegression();
    runCogCrossLayerLifecycleRegression();
}

} // namespace sed::app_tests
