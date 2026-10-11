#include "sed/assets/3do.hpp"
#include "sed/assets/bounded_stream_reader.hpp"

#include "sed/text.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <charconv>
#include <cmath>
#include <fstream>
#include <limits>
#include <memory>
#include <numeric>
#include <optional>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace sed::assets
{

namespace detail
{

struct Model3doBudgetLimitsForTests
{
    std::size_t decodedBytes{};
    std::size_t items{};
    std::size_t sourceBytes{ 128u * 1024u * 1024u };
    std::size_t scannerWorkspaceBytes{ 64u * 1024u * 1024u };
    std::size_t tokensPerLine{ 2u * 1024u * 1024u };
    std::size_t transformVisits{ 16u * 1024u * 1024u };
};

namespace
{

thread_local std::optional<Model3doBudgetLimitsForTests>
    model3doBudgetLimitsForTests{};

} // namespace

// Link-only test seam. The override is thread-local and scoped by the parser
// tests so their deliberately small budget cannot affect unrelated loads.
std::optional<Model3doBudgetLimitsForTests>
exchangeModel3doBudgetLimitsForTests(
    std::optional<Model3doBudgetLimitsForTests> limits) noexcept
{
    std::optional<Model3doBudgetLimitsForTests> previous =
        model3doBudgetLimitsForTests;
    model3doBudgetLimitsForTests = limits;
    return previous;
}

std::optional<Model3doBudgetLimitsForTests>
model3doBudgetLimitsOverrideForTests() noexcept
{
    return model3doBudgetLimitsForTests;
}

} // namespace detail

namespace
{

using sed::intensityToColor;

using sed::text::iequalsAscii;
using sed::text::istartsWithAscii;
using sed::text::parseHex;
using sed::text::parseInt;
using sed::text::trimLeft;
using sed::text::trimView;
using sed::text::upperAscii;

constexpr std::size_t kMaxModel3doDecodedBytes = 128u * 1024u * 1024u;
constexpr std::size_t kMaxModel3doDecodedItems = 2u * 1024u * 1024u;
constexpr std::size_t kMaxModel3doSourceBytes = 128u * 1024u * 1024u;
constexpr std::size_t kMaxModel3doScannerWorkspaceBytes = 64u * 1024u * 1024u;
constexpr std::size_t kMaxModel3doTokensPerLine = 2u * 1024u * 1024u;
constexpr std::size_t kMaxModel3doTransformVisits = 16u * 1024u * 1024u;
constexpr std::string_view kInvalid3doRecordCountDiagnostic =
    "Cannot load 3DO: declared record count is invalid or truncated.";
constexpr std::string_view k3doGeometryBudgetDiagnostic =
    "Cannot load 3DO: decoded geometry budget is exceeded.";
constexpr std::string_view k3doSourceBudgetDiagnostic =
    "Cannot load 3DO: source byte budget is exceeded.";
constexpr std::string_view k3doScannerBudgetDiagnostic =
    "Cannot load 3DO: section scanner workspace budget is exceeded.";
constexpr std::string_view k3doTokenBudgetDiagnostic =
    "Cannot load 3DO: line token budget is exceeded.";
constexpr std::string_view k3doTransformBudgetDiagnostic =
    "Cannot load 3DO: hierarchy transform work budget is exceeded.";

struct Model3doParseBudget
{
    std::size_t decodedBytes{};
    std::size_t items{};
    std::size_t maxDecodedBytes{kMaxModel3doDecodedBytes};
    std::size_t maxItems{kMaxModel3doDecodedItems};
    std::size_t maxSourceBytes{kMaxModel3doSourceBytes};
    std::size_t scannerWorkspaceBytes{};
    std::size_t maxScannerWorkspaceBytes{kMaxModel3doScannerWorkspaceBytes};
    std::size_t maxTokensPerLine{kMaxModel3doTokensPerLine};
    std::size_t maxTransformVisits{kMaxModel3doTransformVisits};

    Model3doParseBudget()
    {
        if (const auto override = detail::model3doBudgetLimitsOverrideForTests())
        {
            maxDecodedBytes = override->decodedBytes;
            maxItems = override->items;
            maxSourceBytes = override->sourceBytes;
            maxScannerWorkspaceBytes = override->scannerWorkspaceBytes;
            maxTokensPerLine = override->tokensPerLine;
            maxTransformVisits = override->transformVisits;
        }
    }

    void charge(
        const std::size_t itemCount,
        const std::size_t byteCount)
    {
        if (itemCount > maxItems
            || items > maxItems - itemCount
            || byteCount > maxDecodedBytes
            || decodedBytes > maxDecodedBytes - byteCount)
        {
            throw std::runtime_error(std::string(k3doGeometryBudgetDiagnostic));
        }
        items += itemCount;
        decodedBytes += byteCount;
    }

    void chargeItems(const std::size_t count)
    {
        charge(count, 0);
    }

    void chargeBytes(const std::size_t count)
    {
        charge(0, count);
    }

    void chargeScannerWorkspaceBytes(const std::size_t count)
    {
        if (count > maxScannerWorkspaceBytes
            || scannerWorkspaceBytes > maxScannerWorkspaceBytes - count)
        {
            throw std::runtime_error(std::string(k3doScannerBudgetDiagnostic));
        }
        scannerWorkspaceBytes += count;
    }

    void chargeRecords(
        const std::size_t count,
        const std::size_t bytesPerRecord)
    {
        if (bytesPerRecord != 0
            && count > std::numeric_limits<std::size_t>::max() / bytesPerRecord)
        {
            throw std::runtime_error(std::string(k3doGeometryBudgetDiagnostic));
        }
        charge(count, count * bytesPerRecord);
    }
};

std::vector<std::string_view> split3doWords(
    const std::string_view line,
    const Model3doParseBudget& budget)
{
    std::size_t tokenCount = 0;
    std::size_t cursor = 0;
    while (cursor < line.size())
    {
        while (cursor < line.size()
            && std::isspace(static_cast<unsigned char>(line[cursor])))
        {
            ++cursor;
        }
        if (cursor == line.size())
        {
            break;
        }

        if (tokenCount == budget.maxTokensPerLine)
        {
            throw std::runtime_error(std::string(k3doTokenBudgetDiagnostic));
        }
        ++tokenCount;
        while (cursor < line.size()
            && !std::isspace(static_cast<unsigned char>(line[cursor])))
        {
            ++cursor;
        }
    }

    std::vector<std::string_view> tokens{};
    tokens.reserve(tokenCount);
    if (tokens.capacity() > budget.maxTokensPerLine)
    {
        throw std::runtime_error(std::string(k3doTokenBudgetDiagnostic));
    }

    cursor = 0;
    while (cursor < line.size())
    {
        while (cursor < line.size()
            && std::isspace(static_cast<unsigned char>(line[cursor])))
        {
            ++cursor;
        }
        if (cursor == line.size())
        {
            break;
        }

        const std::size_t tokenBegin = cursor;
        while (cursor < line.size()
            && !std::isspace(static_cast<unsigned char>(line[cursor])))
        {
            ++cursor;
        }
        tokens.emplace_back(line.substr(tokenBegin, cursor - tokenBegin));
    }
    return tokens;
}

std::size_t checked3doBudgetAdd(
    const std::size_t left,
    const std::size_t right)
{
    if (right > std::numeric_limits<std::size_t>::max() - left)
    {
        throw std::runtime_error(std::string(k3doGeometryBudgetDiagnostic));
    }
    return left + right;
}

std::size_t checked3doBudgetMultiply(
    const std::size_t left,
    const std::size_t right)
{
    if (right != 0
        && left > std::numeric_limits<std::size_t>::max() / right)
    {
        throw std::runtime_error(std::string(k3doGeometryBudgetDiagnostic));
    }
    return left * right;
}

struct Model3doMeshStorageCharge
{
    std::size_t items{};
    std::size_t bytes{};
};

Model3doMeshStorageCharge measure3doMeshStorage(
    const Model3doMesh& mesh)
{
    Model3doMeshStorageCharge charge{
        .items = 1,
        .bytes = checked3doBudgetAdd(
            sizeof(Model3doMesh),
            checked3doBudgetAdd(mesh.name.size(), 1u)),
    };

    charge.items = checked3doBudgetAdd(charge.items, mesh.vertices.size());
    charge.bytes = checked3doBudgetAdd(
        charge.bytes,
        checked3doBudgetMultiply(
            mesh.vertices.size(), sizeof(Model3doVertex)));

    charge.items = checked3doBudgetAdd(charge.items, mesh.texVertices.size());
    charge.bytes = checked3doBudgetAdd(
        charge.bytes,
        checked3doBudgetMultiply(
            mesh.texVertices.size(), sizeof(Model3doTexVertex)));

    charge.bytes = checked3doBudgetAdd(
        charge.bytes,
        checked3doBudgetMultiply(
            mesh.vertexNormals.size(), sizeof(TVector)));

    charge.items = checked3doBudgetAdd(charge.items, mesh.faces.size());
    charge.bytes = checked3doBudgetAdd(
        charge.bytes,
        checked3doBudgetMultiply(mesh.faces.size(), sizeof(Model3doFace)));

    charge.bytes = checked3doBudgetAdd(
        charge.bytes,
        checked3doBudgetMultiply(mesh.faceNormals.size(), sizeof(TVector)));

    for (const Model3doFace& face : mesh.faces)
    {
        // A face corner is one logical vertex/texture-vertex pair. The two
        // index arrays are both included in bytes because each is allocated.
        charge.items = checked3doBudgetAdd(charge.items, face.vertices.size());
        charge.bytes = checked3doBudgetAdd(
            charge.bytes,
            checked3doBudgetMultiply(
                checked3doBudgetAdd(
                    face.vertices.size(), face.texVertices.size()),
                sizeof(std::int32_t)));
    }
    return charge;
}

std::int32_t parseDeclared3doCount(
    const std::vector<std::string_view>& tokens,
    const std::size_t index)
{
    if (index >= tokens.size())
    {
        throw std::runtime_error(std::string(kInvalid3doRecordCountDiagnostic));
    }
    const std::optional<std::int32_t> count = parseInt<std::int32_t>(tokens[index]);
    if (!count || *count < 0)
    {
        throw std::runtime_error(std::string(kInvalid3doRecordCountDiagnostic));
    }
    return *count;
}

void require3doRecordRows(
    const std::size_t count,
    const std::size_t firstRow,
    const std::size_t end)
{
    if (firstRow > end || count > end - firstRow)
    {
        throw std::runtime_error(std::string(kInvalid3doRecordCountDiagnostic));
    }
}

bool is3doRecordHeader(
    const std::vector<std::string_view>& tokens,
    const bool vertexRows,
    const bool textureVertexRows)
{
    if (tokens.empty())
    {
        return false;
    }
    const std::string first = upperAscii(tokens[0]);
    if (first == "MESH" || first == "GEOSET")
    {
        return true;
    }
    if (vertexRows)
    {
        return first == "TEXTURE" || first == "VERTEX"
            || first == "FACES" || first == "FACE";
    }
    if (textureVertexRows)
    {
        return first == "VERTEX" || first == "FACES" || first == "FACE";
    }
    return first == "FACE";
}

std::size_t available3doRows(
    const std::vector<std::string_view>& lines,
    std::size_t firstRow,
    const std::size_t end,
    const bool vertexRows,
    const bool textureVertexRows,
    const Model3doParseBudget& budget)
{
    std::size_t available = 0;
    while (firstRow < end)
    {
        const auto tokens = split3doWords(lines[firstRow], budget);
        if (is3doRecordHeader(tokens, vertexRows, textureVertexRows))
        {
            break;
        }
        ++available;
        ++firstRow;
    }
    return available;
}

bool has3doCornerPairs(
    const std::vector<std::string_view>& tokens,
    std::size_t cursor,
    const std::size_t count) noexcept
{
    if (cursor > tokens.size() || count > tokens.size() - cursor)
    {
        return false;
    }
    for (std::size_t corner = 0; corner < count; ++corner)
    {
        const std::string_view pair = tokens[cursor++];
        const std::size_t comma = pair.find(',');
        if (comma == std::string_view::npos)
        {
            if (cursor >= tokens.size() || tokens[cursor].empty())
            {
                return false;
            }
            ++cursor;
            continue;
        }
        if (comma == 0)
        {
            return false;
        }
        if (comma + 1 == pair.size())
        {
            if (cursor >= tokens.size() || tokens[cursor].empty())
            {
                return false;
            }
            ++cursor;
        }
    }
    return true;
}

struct Model3doSections
{
    std::vector<std::string_view> header{};
    std::vector<std::string_view> modelResource{};
    std::vector<std::string_view> geometryDef{};
    std::vector<std::string_view> hierarchyDef{};
};

std::optional<double> parseDouble(std::string_view token) noexcept
{
    token = trimView(token);
    if (!token.empty() && token.front() == '+')
    {
        token.remove_prefix(1);
    }
    if (token.empty())
    {
        return std::nullopt;
    }

    double value{};
    const char* const first = token.data();
    const char* const last = token.data() + token.size();
    const auto result = std::from_chars(
        first,
        last,
        value,
        std::chars_format::general);
    if (result.ec != std::errc{} || result.ptr != last)
    {
        return std::nullopt;
    }
    return value;
}

float parseModelUvFloatOr(
    const std::string_view token,
    const float fallback)
{
    const std::optional<double> value = parseDouble(token);
    if (!value)
    {
        return fallback;
    }

    constexpr double maxFloat =
        static_cast<double>(std::numeric_limits<float>::max());
    if (!std::isfinite(*value) || *value < -maxFloat || *value > maxFloat)
    {
        throw std::runtime_error(
            "Cannot load 3DO texture vertex: a UV value is invalid or outside float range.");
    }
    return static_cast<float>(*value);
}

std::vector<std::string_view>* sectionLinesForName(
    Model3doSections& sections,
    const std::string_view name) noexcept
{
    if (iequalsAscii(name, "HEADER"))
    {
        return &sections.header;
    }
    if (iequalsAscii(name, "MODELRESOURCE"))
    {
        return &sections.modelResource;
    }
    if (iequalsAscii(name, "GEOMETRYDEF"))
    {
        return &sections.geometryDef;
    }
    if (iequalsAscii(name, "HIERARCHYDEF"))
    {
        return &sections.hierarchyDef;
    }
    return nullptr;
}

void reserve3doSectionLines(
    std::vector<std::string_view>& lines,
    const std::size_t requestedCapacity,
    Model3doParseBudget& budget)
{
    const std::size_t oldCapacity = lines.capacity();
    if (requestedCapacity <= oldCapacity)
    {
        return;
    }

    const std::size_t requestedSlots = requestedCapacity - oldCapacity;
    if (requestedSlots > std::numeric_limits<std::size_t>::max()
            / sizeof(std::string_view))
    {
        throw std::runtime_error(std::string(k3doScannerBudgetDiagnostic));
    }
    budget.chargeScannerWorkspaceBytes(
        requestedSlots * sizeof(std::string_view));
    lines.reserve(requestedCapacity);

    const std::size_t actualCapacity = lines.capacity();
    if (actualCapacity < requestedCapacity)
    {
        throw std::runtime_error(std::string(k3doScannerBudgetDiagnostic));
    }
    const std::size_t actualSlots = actualCapacity - oldCapacity;
    if (actualSlots > requestedSlots)
    {
        const std::size_t extraSlots = actualSlots - requestedSlots;
        if (extraSlots > std::numeric_limits<std::size_t>::max()
                / sizeof(std::string_view))
        {
            throw std::runtime_error(std::string(k3doScannerBudgetDiagnostic));
        }
        budget.chargeScannerWorkspaceBytes(
            extraSlots * sizeof(std::string_view));
    }
}

void append3doSectionLine(
    std::vector<std::string_view>& lines,
    const std::string_view line,
    Model3doParseBudget& budget)
{
    if (lines.size() == lines.capacity())
    {
        const std::size_t currentCapacity = lines.capacity();
        const std::size_t nextCapacity = currentCapacity == 0
            ? 1u
            : (currentCapacity <= std::numeric_limits<std::size_t>::max() / 2u
                ? currentCapacity * 2u
                : std::numeric_limits<std::size_t>::max());
        if (nextCapacity <= currentCapacity)
        {
            throw std::runtime_error(std::string(k3doScannerBudgetDiagnostic));
        }
        reserve3doSectionLines(lines, nextCapacity, budget);
    }
    lines.push_back(line);
}

Model3doSections scanModel3doSections(
    const std::span<const std::byte> bytes,
    Model3doParseBudget& budget)
{
    if (bytes.size() > budget.maxSourceBytes)
    {
        throw std::runtime_error(std::string(k3doSourceBudgetDiagnostic));
    }

    Model3doSections sections{};
    reserve3doSectionLines(sections.header, 4u, budget);
    reserve3doSectionLines(sections.modelResource, 16u, budget);
    reserve3doSectionLines(sections.geometryDef, 128u, budget);
    reserve3doSectionLines(sections.hierarchyDef, 32u, budget);

    const char* const data = bytes.empty()
        ? ""
        : reinterpret_cast<const char*>(bytes.data());
    const std::string_view text(data, bytes.size());
    std::vector<std::string_view>* currentSection = nullptr;
    std::size_t lineBegin = 0;
    while (lineBegin < text.size())
    {
        const std::size_t newline = text.find('\n', lineBegin);
        const std::size_t lineEnd = newline == std::string_view::npos
            ? text.size()
            : newline;
        std::string_view line = text.substr(lineBegin, lineEnd - lineBegin);
        if (const std::size_t comment = line.find('#'); comment != std::string_view::npos)
        {
            line = line.substr(0, comment);
        }
        line = trimView(line);

        if (!line.empty())
        {
            if (iequalsAscii(line, "END"))
            {
                currentSection = nullptr;
            }
            else if (istartsWithAscii(line, "SECTION:"))
            {
                const auto words = split3doWords(line, budget);
                currentSection = words.size() >= 2
                    ? sectionLinesForName(sections, words[1])
                    : nullptr;
            }
            else if (currentSection)
            {
                append3doSectionLine(*currentSection, line, budget);
            }
        }

        if (newline == std::string_view::npos)
        {
            break;
        }
        lineBegin = newline + 1;
    }
    return sections;
}

bool isCloseVersion(const double a, const double b, const double eps = 1e-6) noexcept
{
    return std::fabs(a - b) <= eps;
}

std::optional<std::int32_t> parseIndexToken(const std::string_view token) noexcept
{
    std::string_view t = token;
    if (const std::size_t colon = t.find(':'); colon != std::string_view::npos)
    {
        t = t.substr(0, colon);
    }
    return parseInt<std::int32_t>(trimView(t));
}

std::optional<ColorF> parseSlashColor(const std::string_view token)
{
    std::string_view t = trimView(token);
    if (t.size() >= 2 && t.front() == '(' && t.back() == ')')
    {
        t = t.substr(1, t.size() - 2);
    }

    std::array<float, 4> comps{};
    std::size_t count = 0;

    while (!t.empty() && count < comps.size())
    {
        std::string_view part = t;
        if (const std::size_t slash = t.find('/'); slash != std::string_view::npos)
        {
            part = t.substr(0, slash);
            t = t.substr(slash + 1);
        }
        else
        {
            t = {};
        }

        const auto v = parseDouble(trimView(part));
        if (!v)
        {
            return std::nullopt;
        }
        comps[count++] = static_cast<float>(*v);
    }

    if (count < 3)
    {
        return std::nullopt;
    }

    ColorF out{};
    out.r = comps[0];
    out.g = comps[1];
    out.b = comps[2];
    out.a = (count >= 4) ? comps[3] : 1.0f;
    return out;
}

void parseIndexPair(
    const std::vector<std::string_view>& tokens,
    std::size_t& t,
    std::int32_t& leftOut,
    std::int32_t& rightOut)
{
    leftOut = 0;
    rightOut = 0;
    if (t >= tokens.size())
    {
        return;
    }

    std::string_view pair = tokens[t++];
    std::string_view left = pair;
    std::string_view right;
    if (const std::size_t comma = pair.find(','); comma != std::string_view::npos)
    {
        left = pair.substr(0, comma);
        right = pair.substr(comma + 1);
        if (right.empty() && t < tokens.size())
        {
            right = tokens[t++];
        }
    }
    else if (t < tokens.size())
    {
        right = tokens[t++];
    }

    leftOut = parseInt<std::int32_t>(trimLeft(left)).value_or(0);
    rightOut = parseInt<std::int32_t>(trimLeft(right)).value_or(0);
}

constexpr bool isValidIndex(const std::int32_t index, const std::size_t count) noexcept
{
    return index >= 0 && static_cast<std::size_t>(index) < count;
}

void transformMeshForNode(
    Model3doMesh& mesh,
    const Model3doNode& node,
    const std::vector<Model3doNode>& nodes) noexcept
{
    const TMatrix43 rot = TMatrix43::CreateRotate(node.rotation);

    const auto rotateThroughHierarchy = [&](TVector value) noexcept
    {
        value = rot.TransformVector(value);

        std::int32_t parent = node.parent;
        std::size_t depth = 0;
        while (parent != -1 && isValidIndex(parent, nodes.size()) && depth < nodes.size())
        {
            const Model3doNode& parentNode = nodes[static_cast<std::size_t>(parent)];
            const TMatrix43 parentRot = TMatrix43::CreateRotate(parentNode.rotation);
            value = parentRot.TransformVector(value);

            parent = parentNode.parent;
            ++depth;
        }

        return value;
    };

    for (Model3doVertex& v : mesh.vertices)
    {
        TVector p{ v.x + node.pivot.x, v.y + node.pivot.y, v.z + node.pivot.z };
        p = rot.TransformVector(p);
        p = p + node.position;

        std::int32_t parent = node.parent;
        std::size_t depth = 0;
        while (parent != -1 && isValidIndex(parent, nodes.size()) && depth < nodes.size())
        {
            const Model3doNode& parentNode = nodes[static_cast<std::size_t>(parent)];
            const TMatrix43 parentRot = TMatrix43::CreateRotate(parentNode.rotation);
            p = parentRot.TransformVector(p);
            p = p + parentNode.position;

            parent = parentNode.parent;
            ++depth;
        }

        v.x = p.x;
        v.y = p.y;
        v.z = p.z;
    }

    for (TVector& normal : mesh.vertexNormals)
    {
        normal = rotateThroughHierarchy(normal);
    }
    for (TVector& normal : mesh.faceNormals)
    {
        normal = rotateThroughHierarchy(normal);
    }
}

void preflight3doHierarchyTransformWork(
    const Model3do& model,
    Model3doParseBudget& budget)
{
    const std::size_t nodeCount = model.nodes.size();
    const std::size_t scratchBytesPerNode =
        sizeof(unsigned char) + (2u * sizeof(std::size_t));
    if (scratchBytesPerNode != 0
        && nodeCount > std::numeric_limits<std::size_t>::max()
            / scratchBytesPerNode)
    {
        throw std::runtime_error(std::string(k3doGeometryBudgetDiagnostic));
    }

    // Charge the tri-color, memoized-hop and iterative path arrays before any
    // of them allocate. The traversal remains linear even for deep or cyclic
    // parent chains.
    budget.chargeBytes(nodeCount * scratchBytesPerNode);
    const std::unique_ptr<unsigned char[]> colors =
        std::make_unique<unsigned char[]>(nodeCount);
    const std::unique_ptr<std::size_t[]> hops =
        std::make_unique<std::size_t[]>(nodeCount);
    const std::unique_ptr<std::size_t[]> path =
        std::make_unique<std::size_t[]>(nodeCount);
    std::size_t pathSize = 0;

    for (std::size_t start = 0; start < nodeCount; ++start)
    {
        if (colors[start] != 0u)
        {
            continue;
        }

        pathSize = 0;
        std::size_t current = start;
        bool endedAtInvalidParent = false;
        while (colors[current] == 0u)
        {
            colors[current] = 1u;
            path[pathSize++] = current;

            const std::int32_t parent = model.nodes[current].parent;
            if (!isValidIndex(parent, nodeCount))
            {
                endedAtInvalidParent = true;
                break;
            }
            current = static_cast<std::size_t>(parent);
        }

        if (!endedAtInvalidParent && colors[current] == 1u)
        {
            // Every node on this path reaches the detected cycle, including
            // any incoming tail. The transform's existing depth guard visits
            // at most nodeCount valid parents for all such nodes.
            for (std::size_t pathIndex = 0; pathIndex < pathSize; ++pathIndex)
            {
                const std::size_t pathNode = path[pathIndex];
                hops[pathNode] = nodeCount;
                colors[pathNode] = 2u;
            }
            continue;
        }

        while (pathSize > 0u)
        {
            const std::size_t pathNode = path[--pathSize];
            const std::int32_t parent = model.nodes[pathNode].parent;
            if (isValidIndex(parent, nodeCount))
            {
                const std::size_t parentHops =
                    hops[static_cast<std::size_t>(parent)];
                hops[pathNode] = parentHops >= nodeCount
                    ? nodeCount
                    : parentHops + 1u;
            }
            colors[pathNode] = 2u;
        }
    }

    const auto addWork = [](const std::size_t left, const std::size_t right)
    {
        if (right > std::numeric_limits<std::size_t>::max() - left)
        {
            throw std::runtime_error(std::string(k3doTransformBudgetDiagnostic));
        }
        return left + right;
    };

    std::size_t totalVisits = 0;
    for (std::size_t nodeIndex = 0; nodeIndex < nodeCount; ++nodeIndex)
    {
        const std::int32_t meshIndex = model.nodes[nodeIndex].mesh;
        if (!isValidIndex(meshIndex, model.meshes.size()))
        {
            continue;
        }

        const Model3doMesh& mesh =
            model.meshes[static_cast<std::size_t>(meshIndex)];
        std::size_t vectorCount = mesh.vertices.size();
        vectorCount = addWork(vectorCount, mesh.vertexNormals.size());
        vectorCount = addWork(vectorCount, mesh.faceNormals.size());
        if (vectorCount == 0u)
        {
            continue;
        }

        const std::size_t transformDepth = addWork(hops[nodeIndex], 1u);
        if (transformDepth > budget.maxTransformVisits
            || vectorCount > budget.maxTransformVisits / transformDepth)
        {
            throw std::runtime_error(std::string(k3doTransformBudgetDiagnostic));
        }
        const std::size_t nodeVisits = vectorCount * transformDepth;
        if (nodeVisits > budget.maxTransformVisits - totalVisits)
        {
            throw std::runtime_error(std::string(k3doTransformBudgetDiagnostic));
        }
        totalVisits += nodeVisits;
    }
}

void offsetMeshes(Model3do& model, Model3doParseBudget& budget)
{
    if (model.nodes.empty() || model.meshes.empty())
    {
        return;
    }

    preflight3doHierarchyTransformWork(model, budget);

    constexpr std::size_t kNoMeshNode =
        std::numeric_limits<std::size_t>::max();
    constexpr std::size_t kNeedsSourceSnapshot = kNoMeshNode - 1u;
    struct SourceUse
    {
        std::size_t firstNode{};
        std::size_t snapshotIndex{};
    };

    const std::size_t sourceMeshCount = model.meshes.size();
    const bool needsIdentitySourceIndices =
        model.meshSourceIndices.size() != sourceMeshCount;

    // This compact table lets the preflight identify source meshes used by
    // more than one hierarchy node before any mesh/vector copy or expansion
    // reserve occurs. The selected LOD's source mesh ordinals stay stable.
    budget.chargeBytes(checked3doBudgetMultiply(
        sourceMeshCount, sizeof(SourceUse)));
    std::vector<SourceUse> sourceUses(
        sourceMeshCount, SourceUse{ kNoMeshNode, kNoMeshNode });
    std::size_t cloneCount = 0;
    for (std::size_t nodeIndex = 0; nodeIndex < model.nodes.size(); ++nodeIndex)
    {
        const std::int32_t meshIndex = model.nodes[nodeIndex].mesh;
        if (!isValidIndex(meshIndex, sourceMeshCount))
        {
            continue;
        }

        SourceUse& use = sourceUses[static_cast<std::size_t>(meshIndex)];
        if (use.firstNode == kNoMeshNode)
        {
            use.firstNode = nodeIndex;
        }
        else
        {
            cloneCount = checked3doBudgetAdd(cloneCount, 1u);
            use.snapshotIndex = kNeedsSourceSnapshot;
        }
    }

    if (sourceMeshCount > static_cast<std::size_t>(
            std::numeric_limits<std::int32_t>::max())
        || cloneCount > static_cast<std::size_t>(
            std::numeric_limits<std::int32_t>::max()) - sourceMeshCount)
    {
        throw std::runtime_error(std::string(k3doGeometryBudgetDiagnostic));
    }
    const std::size_t expandedMeshCount =
        checked3doBudgetAdd(sourceMeshCount, cloneCount);

    std::size_t sharedSourceCount = 0;
    for (std::size_t sourceIndex = 0;
         sourceIndex < sourceUses.size(); ++sourceIndex)
    {
        SourceUse& use = sourceUses[sourceIndex];
        if (use.snapshotIndex != kNeedsSourceSnapshot)
        {
            continue;
        }

        use.snapshotIndex = sharedSourceCount;
        sharedSourceCount = checked3doBudgetAdd(sharedSourceCount, 1u);

        const Model3doMeshStorageCharge copyCharge =
            measure3doMeshStorage(model.meshes[sourceIndex]);
        budget.charge(copyCharge.items, copyCharge.bytes);
    }

    // Charge every retained clone and its source-index entry before either
    // vector is reserved or any nested geometry is copied.
    for (std::size_t nodeIndex = 0; nodeIndex < model.nodes.size(); ++nodeIndex)
    {
        const std::int32_t meshIndex = model.nodes[nodeIndex].mesh;
        if (!isValidIndex(meshIndex, sourceMeshCount))
        {
            continue;
        }

        const std::size_t sourceIndex = static_cast<std::size_t>(meshIndex);
        const SourceUse& use = sourceUses[sourceIndex];
        if (nodeIndex == use.firstNode)
        {
            continue;
        }

        const Model3doMeshStorageCharge copyCharge =
            measure3doMeshStorage(model.meshes[sourceIndex]);
        budget.charge(copyCharge.items, copyCharge.bytes);
    }
    budget.chargeBytes(checked3doBudgetMultiply(
        cloneCount, sizeof(std::int32_t)));
    if (needsIdentitySourceIndices)
    {
        budget.chargeBytes(checked3doBudgetMultiply(
            sourceMeshCount, sizeof(std::int32_t)));
    }

    // All decoded and expanded storage has now passed the aggregate budget.
    // Keep only source meshes which actually need multiple node instances;
    // a full copy of every selected mesh needlessly multiplied peak memory.
    std::vector<Model3doMesh> sharedSourceMeshes;
    sharedSourceMeshes.reserve(sharedSourceCount);
    for (std::size_t sourceIndex = 0; sourceIndex < sourceMeshCount; ++sourceIndex)
    {
        const SourceUse& use = sourceUses[sourceIndex];
        if (use.snapshotIndex == kNoMeshNode)
        {
            continue;
        }
        sharedSourceMeshes.push_back(model.meshes[sourceIndex]);
    }

    model.meshes.reserve(expandedMeshCount);
    if (needsIdentitySourceIndices)
    {
        model.meshSourceIndices.resize(sourceMeshCount);
        std::iota(model.meshSourceIndices.begin(), model.meshSourceIndices.end(), 0);
    }
    model.meshSourceIndices.reserve(expandedMeshCount);

    for (std::size_t nodeIndex = 0; nodeIndex < model.nodes.size(); ++nodeIndex)
    {
        Model3doNode& node = model.nodes[nodeIndex];
        if (!isValidIndex(node.mesh, sourceMeshCount))
        {
            continue;
        }

        const std::size_t sourceIndex = static_cast<std::size_t>(node.mesh);
        const SourceUse& use = sourceUses[sourceIndex];
        if (nodeIndex == use.firstNode)
        {
            transformMeshForNode(model.meshes[sourceIndex], node, model.nodes);
            continue;
        }

        const std::size_t instanceIndex = model.meshes.size();
        model.meshes.push_back(sharedSourceMeshes[use.snapshotIndex]);
        model.meshSourceIndices.push_back(static_cast<std::int32_t>(sourceIndex));
        node.mesh = static_cast<std::int32_t>(instanceIndex);
        transformMeshForNode(model.meshes.back(), node, model.nodes);
    }
}

void computeRadii(Model3do& model) noexcept
{
    double modelRadiusSqr = model.radius * model.radius;

    for (Model3doMesh& mesh : model.meshes)
    {
        double meshRadiusSqr = mesh.radius * mesh.radius;

        for (const Model3doVertex& v : mesh.vertices)
        {
            const double r2 = (v.x * v.x) + (v.y * v.y) + (v.z * v.z);
            meshRadiusSqr = std::max(meshRadiusSqr, r2);
            modelRadiusSqr = std::max(modelRadiusSqr, r2);
        }

        mesh.radius = std::sqrt(std::max(meshRadiusSqr, 0.0));
    }

    model.radius = std::sqrt(std::max(modelRadiusSqr, 0.0));
}

Model3do parseModel3doSections(
    const Model3doSections& sections,
    const Model3doLoadOptions& options,
    Model3doParseBudget& budget)
{
    Model3do model{};
    const auto tokenize = [&budget](const std::string_view line)
    {
        return split3doWords(line, budget);
    };

    if (!sections.header.empty())
    {
        for (const std::string_view raw : sections.header)
        {
            const auto tokens = tokenize(raw);
            if (tokens.size() >= 2 && iequalsAscii(tokens[0], "3DO"))
            {
                model.version = parseDouble(tokens[1]).value_or(0.0);
                break;
            }
        }
    }

    const bool isIjim = isCloseVersion(model.version, 2.3);

    if (!sections.modelResource.empty())
    {
        const auto& lines = sections.modelResource;
        std::size_t idx = 0;

        std::int32_t count = 0;
        if (idx < lines.size())
        {
            const auto hdr = tokenize(lines[idx++]);
            if (hdr.empty() || !iequalsAscii(hdr[0], "MATERIALS"))
            {
                throw std::runtime_error(
                    std::string(kInvalid3doRecordCountDiagnostic));
            }
            count = parseDeclared3doCount(hdr, 1);
        }

        require3doRecordRows(
            static_cast<std::size_t>(count), idx, lines.size());
        budget.chargeRecords(
            static_cast<std::size_t>(count), sizeof(std::string));
        model.materials.reserve(static_cast<std::size_t>(std::max<std::int32_t>(0, count)));
        for (std::int32_t i = 0; i < count; ++i)
        {
            std::string_view name = lines[idx++];
            if (const std::size_t lastColon = name.find_last_of(':'); lastColon != std::string_view::npos)
            {
                name.remove_prefix(lastColon + 1);
            }
            name = trimView(name);
            budget.chargeBytes(name.size() + 1u);
            model.materials.emplace_back(name);
        }
    }

    if (!sections.geometryDef.empty())
    {
        const auto& lines = sections.geometryDef;
        std::int32_t geoSets = 0;

        for (const std::string_view raw : lines)
        {
            const auto tokens = tokenize(raw);
            if (tokens.empty())
            {
                continue;
            }

            const std::string k1 = upperAscii(tokens[0]);
            const std::string k2 = (tokens.size() >= 2) ? upperAscii(tokens[1]) : std::string{};

            if (k1 == "RADIUS" && tokens.size() >= 2)
            {
                model.radius = parseDouble(tokens[1]).value_or(0.0);
            }
            else if (k1 == "INSERT" && k2 == "OFFSET" && tokens.size() >= 5)
            {
                model.insertOffset.x = parseDouble(tokens[2]).value_or(0.0);
                model.insertOffset.y = parseDouble(tokens[3]).value_or(0.0);
                model.insertOffset.z = parseDouble(tokens[4]).value_or(0.0);
            }
            else if (k1 == "GEOSETS" && tokens.size() >= 2)
            {
                geoSets = parseInt<std::int32_t>(tokens[1]).value_or(0);
            }
        }

        model.geosetCount = static_cast<std::size_t>(
            std::max<std::int32_t>(0, geoSets));

        std::int32_t lod = options.lod;
        if (geoSets > 0)
        {
            lod = std::clamp<std::int32_t>(lod, 0, geoSets - 1);
        }
        else
        {
            lod = std::max<std::int32_t>(0, lod);
        }

        std::size_t idx = 0;
        while (idx < lines.size())
        {
            const auto tokens = tokenize(lines[idx]);
            if (tokens.size() >= 2 && iequalsAscii(tokens[0], "GEOSET"))
            {
                const std::int32_t geoIndex = parseInt<std::int32_t>(tokens[1]).value_or(-1);
                if (geoIndex == lod)
                {
                    ++idx;
                    break;
                }
            }
            ++idx;
        }

        // A malformed or incomplete GEOSET must not borrow MESHES from the
        // next GEOSET. Limit every mesh read to the selected geoset's section.
        std::size_t selectedGeosetEnd = lines.size();
        for (std::size_t next = idx; next < lines.size(); ++next)
        {
            const auto tokens = tokenize(lines[next]);
            if (!tokens.empty() && iequalsAscii(tokens[0], "GEOSET"))
            {
                selectedGeosetEnd = next;
                break;
            }
        }

        std::int32_t meshCount = 0;
        for (; idx < selectedGeosetEnd; ++idx)
        {
            const auto tokens = tokenize(lines[idx]);
            if (tokens.size() >= 2 && iequalsAscii(tokens[0], "MESHES"))
            {
                meshCount = parseDeclared3doCount(tokens, 1);
                ++idx;
                break;
            }
        }

        for (std::int32_t m = 0; m < meshCount && idx < selectedGeosetEnd; ++m)
        {
            while (idx < selectedGeosetEnd)
            {
                const auto tokens = tokenize(lines[idx]);
                if (!tokens.empty() && iequalsAscii(tokens[0], "MESH"))
                {
                    break;
                }
                ++idx;
            }

            if (idx >= selectedGeosetEnd)
            {
                break;
            }

            ++idx; // consume MESH header

            budget.chargeRecords(1, sizeof(Model3doMesh));
            Model3doMesh mesh{};

            if (idx < selectedGeosetEnd)
            {
                const auto tokens = tokenize(lines[idx]);
                if (tokens.size() >= 2 && iequalsAscii(tokens[0], "NAME"))
                {
                    budget.chargeBytes(tokens[1].size() + 1u);
                    mesh.name = std::string(tokens[1]);
                    ++idx;
                }
            }

            while (idx < selectedGeosetEnd)
            {
                const auto tokens = tokenize(lines[idx]);
                if (tokens.empty())
                {
                    ++idx;
                    continue;
                }

                const std::string k1 = upperAscii(tokens[0]);

                if (k1 == "VERTICES")
                {
                    break;
                }

                if (k1 == "RADIUS" && tokens.size() >= 2)
                {
                    mesh.radius = parseDouble(tokens[1]).value_or(0.0);
                    mesh.authoredRadius = mesh.radius;
                }

                if (k1 == "GEOMETRYMODE" && tokens.size() >= 2)
                {
                    mesh.geo = parseInt<std::int32_t>(tokens[1]).value_or(4);
                }

                if (k1 == "LIGHTINGMODE" && tokens.size() >= 2)
                {
                    mesh.light = parseInt<std::int32_t>(tokens[1]).value_or(3);
                }

                 if (k1 == "TEXTUREMODE" && tokens.size() >= 2)
                {
                    mesh.tex = parseInt<std::int32_t>(tokens[1]).value_or(3);
                }

                ++idx;
            }

            if (idx >= selectedGeosetEnd)
            {
                break;
            }

            {
                const auto vHdr = tokenize(lines[idx++]);
                const std::int32_t vCount =
                    parseDeclared3doCount(vHdr, 1);
                const std::size_t availableVertices = available3doRows(
                    lines, idx, selectedGeosetEnd, true, false, budget);
                require3doRecordRows(
                    static_cast<std::size_t>(vCount), 0, availableVertices);
                budget.chargeRecords(
                    static_cast<std::size_t>(vCount), sizeof(Model3doVertex));
                mesh.vertices.resize(static_cast<std::size_t>(std::max<std::int32_t>(0, vCount)));

                for (std::int32_t v = 0; v < vCount; ++v)
                {
                    const auto tokens = tokenize(lines[idx++]);
                    if (tokens.size() < 4)
                    {
                        continue;
                    }

                    const std::int32_t vi = parseIndexToken(tokens[0]).value_or(v);
                    if (vi < 0 || static_cast<std::size_t>(vi) >= mesh.vertices.size())
                    {
                        continue;
                    }

                    Model3doVertex vx{};
                    vx.x = parseDouble(tokens[1]).value_or(0.0);
                    vx.y = parseDouble(tokens[2]).value_or(0.0);
                    vx.z = parseDouble(tokens[3]).value_or(0.0);

                    if (tokens.size() >= 8)
                    {
                        vx.color.r = static_cast<float>(parseDouble(tokens[4]).value_or(0.0));
                        vx.color.g = static_cast<float>(parseDouble(tokens[5]).value_or(0.0));
                        vx.color.b = static_cast<float>(parseDouble(tokens[6]).value_or(0.0));
                        vx.color.a = static_cast<float>(parseDouble(tokens[7]).value_or(1.0));
                    }
                    else if (tokens.size() >= 5)
                    {
                        const float intensity = static_cast<float>(parseDouble(tokens[4]).value_or(0.0));
                        vx.color = intensityToColor(intensity);
                    }

                    mesh.vertices[static_cast<std::size_t>(vi)] = vx;
                }

                if (idx < selectedGeosetEnd)
                {
                    const auto hdr = tokenize(lines[idx]);
                    if (hdr.size() >= 2 && iequalsAscii(hdr[0], "TEXTURE") && iequalsAscii(hdr[1], "VERTICES"))
                    {
                        ++idx;

                        const std::int32_t tvCount =
                            parseDeclared3doCount(hdr, 2);
                        const std::size_t availableTextureVertices =
                            available3doRows(
                                lines, idx, selectedGeosetEnd, false, true, budget);
                        require3doRecordRows(
                            static_cast<std::size_t>(tvCount),
                            0,
                            availableTextureVertices);
                        budget.chargeRecords(
                            static_cast<std::size_t>(tvCount),
                            sizeof(Model3doTexVertex));
                        mesh.texVertices.resize(static_cast<std::size_t>(std::max<std::int32_t>(0, tvCount)));

                        for (std::int32_t tvi = 0; tvi < tvCount; ++tvi)
                        {
                            const auto tokens = tokenize(lines[idx++]);
                            if (tokens.size() < 3)
                            {
                                continue;
                            }
                            const std::int32_t ti = parseIndexToken(tokens[0]).value_or(tvi);
                            if (ti < 0 || static_cast<std::size_t>(ti) >= mesh.texVertices.size())
                            {
                                continue;
                            }

                            Model3doTexVertex tv{};
                            tv.u = parseModelUvFloatOr(tokens[1], 0.0f);
                            tv.v = parseModelUvFloatOr(tokens[2], 0.0f);
                            mesh.texVertices[static_cast<std::size_t>(ti)] = tv;
                        }
                    }
                }
            }

            if (idx < selectedGeosetEnd)
            {
                const auto hdr = tokenize(lines[idx]);
                if (hdr.size() >= 2 && iequalsAscii(hdr[0], "VERTEX") && iequalsAscii(hdr[1], "NORMALS"))
                {
                    ++idx;
                    budget.chargeBytes(
                        mesh.vertices.size() * sizeof(TVector));
                    mesh.vertexNormals.resize(mesh.vertices.size());
                    for (std::size_t v = 0; v < mesh.vertices.size() && idx < selectedGeosetEnd; ++v)
                    {
                        const auto tokens = tokenize(lines[idx++]);
                        if (tokens.size() < 4)
                        {
                            continue;
                        }
                        const std::int32_t vi = parseIndexToken(tokens[0]).value_or(static_cast<std::int32_t>(v));
                        if (vi < 0 || static_cast<std::size_t>(vi) >= mesh.vertexNormals.size())
                        {
                            continue;
                        }
                        mesh.vertexNormals[static_cast<std::size_t>(vi)].x = parseDouble(tokens[1]).value_or(0.0);
                        mesh.vertexNormals[static_cast<std::size_t>(vi)].y = parseDouble(tokens[2]).value_or(0.0);
                        mesh.vertexNormals[static_cast<std::size_t>(vi)].z = parseDouble(tokens[3]).value_or(0.0);
                    }
                }
            }

            while (idx < selectedGeosetEnd)
            {
                const auto hdr = tokenize(lines[idx]);
                if (!hdr.empty() && iequalsAscii(hdr[0], "FACES"))
                {
                    break;
                }
                ++idx;
            }

            if (idx >= selectedGeosetEnd)
            {
                model.meshes.push_back(std::move(mesh));
                continue;
            }

            std::int32_t faceCount = 0;
            {
                const auto fHdr = tokenize(lines[idx++]);
                faceCount = parseDeclared3doCount(fHdr, 1);
            }

            const std::size_t availableFaces = available3doRows(
                lines, idx, selectedGeosetEnd, false, false, budget);
            require3doRecordRows(
                static_cast<std::size_t>(faceCount), 0, availableFaces);
            budget.chargeRecords(
                static_cast<std::size_t>(faceCount), sizeof(Model3doFace));
            mesh.faces.reserve(static_cast<std::size_t>(std::max<std::int32_t>(0, faceCount)));
            for (std::int32_t f = 0; f < faceCount; ++f)
            {
                const auto tokens = tokenize(lines[idx++]);
                if (tokens.size() < 8)
                {
                    continue;
                }

                Model3doFace face{};
                face.material  = parseInt<std::int32_t>(tokens[1]).value_or(-1);
                face.faceFlags = parseHex(tokens[2]).value_or(0);
                face.geo       = parseInt<std::int32_t>(tokens[3]).value_or(4);
                face.light     = parseInt<std::int32_t>(tokens[4]).value_or(3);
                face.tex       = parseInt<std::int32_t>(tokens[5]).value_or(3);

                std::size_t t = 6;
                if (isIjim)
                {
                    face.extraLight = parseSlashColor(tokens[t++]).value_or(ColorWhite);
                }
                else
                {
                    const float intensity = static_cast<float>(parseDouble(tokens[t++]).value_or(0.0));
                    face.extraLight = intensityToColor(intensity);
                }

                const std::int32_t nvert =
                    parseDeclared3doCount(tokens, t++);
                if (!has3doCornerPairs(
                        tokens, t, static_cast<std::size_t>(nvert)))
                {
                    throw std::runtime_error(
                        std::string(kInvalid3doRecordCountDiagnostic));
                }
                budget.chargeRecords(
                    static_cast<std::size_t>(nvert),
                    2u * sizeof(std::int32_t));
                face.vertices.reserve(static_cast<std::size_t>(std::max<std::int32_t>(0, nvert)));
                face.texVertices.reserve(static_cast<std::size_t>(std::max<std::int32_t>(0, nvert)));

                for (std::int32_t v = 0; v < nvert; ++v)
                {
                    std::int32_t vi = 0;
                    std::int32_t tvi = 0;
                    parseIndexPair(tokens, t, vi, tvi);
                    face.vertices.push_back(vi);
                    face.texVertices.push_back(tvi);
                }

                mesh.faces.push_back(std::move(face));
            }

            if (idx < selectedGeosetEnd)
            {
                const auto hdr = tokenize(lines[idx]);
                if (hdr.size() >= 2 && iequalsAscii(hdr[0], "FACE") && iequalsAscii(hdr[1], "NORMALS"))
                {
                    ++idx;
                    budget.chargeBytes(mesh.faces.size() * sizeof(TVector));
                    mesh.faceNormals.resize(mesh.faces.size());
                    for (std::size_t f = 0; f < mesh.faces.size() && idx < selectedGeosetEnd; ++f)
                    {
                        const auto tokens = tokenize(lines[idx++]);
                        if (tokens.size() < 4)
                        {
                            continue;
                        }
                        const std::int32_t fi = parseIndexToken(tokens[0]).value_or(static_cast<std::int32_t>(f));
                        if (fi < 0 || static_cast<std::size_t>(fi) >= mesh.faceNormals.size())
                        {
                            continue;
                        }
                        mesh.faceNormals[static_cast<std::size_t>(fi)].x = parseDouble(tokens[1]).value_or(0.0);
                        mesh.faceNormals[static_cast<std::size_t>(fi)].y = parseDouble(tokens[2]).value_or(0.0);
                        mesh.faceNormals[static_cast<std::size_t>(fi)].z = parseDouble(tokens[3]).value_or(0.0);
                    }
                }
            }

            model.meshes.push_back(std::move(mesh));
        }
    }

    if (model.meshSourceIndices.size() != model.meshes.size())
    {
        budget.chargeBytes(
            model.meshes.size() * sizeof(std::int32_t));
        model.meshSourceIndices.resize(model.meshes.size());
        std::iota(model.meshSourceIndices.begin(), model.meshSourceIndices.end(), 0);
    }

    if (!sections.hierarchyDef.empty())
    {
        const auto& lines = sections.hierarchyDef;
        std::size_t idx = 0;

        std::int32_t count = 0;
        if (idx < lines.size())
        {
            const auto hdr = tokenize(lines[idx++]);
            if (hdr.size() < 2 || !iequalsAscii(hdr[0], "HIERARCHY")
                || !iequalsAscii(hdr[1], "NODES"))
            {
                throw std::runtime_error(
                    std::string(kInvalid3doRecordCountDiagnostic));
            }
            count = parseDeclared3doCount(hdr, 2);
        }

        require3doRecordRows(
            static_cast<std::size_t>(count), idx, lines.size());
        budget.chargeRecords(
            static_cast<std::size_t>(count), sizeof(Model3doNode));
        model.nodes.reserve(static_cast<std::size_t>(std::max<std::int32_t>(0, count)));
        for (std::int32_t i = 0; i < count; ++i)
        {
            const auto tokens = tokenize(lines[idx++]);
            if (tokens.size() < 18)
            {
                continue;
            }

            Model3doNode node{};
            node.id = parseIndexToken(tokens[0]).value_or(i);
            node.flags = parseHex(tokens[1]).value_or(0);
            node.type = parseHex(tokens[2]).value_or(0);
            node.mesh = parseInt<std::int32_t>(tokens[3]).value_or(-1);
            node.parent = parseInt<std::int32_t>(tokens[4]).value_or(-1);

            node.position.x = parseDouble(tokens[8]).value_or(0.0);
            node.position.y = parseDouble(tokens[9]).value_or(0.0);
            node.position.z = parseDouble(tokens[10]).value_or(0.0);

            node.rotation.x = parseDouble(tokens[11]).value_or(0.0);
            node.rotation.y = parseDouble(tokens[12]).value_or(0.0);
            node.rotation.z = parseDouble(tokens[13]).value_or(0.0);

            node.pivot.x = parseDouble(tokens[14]).value_or(0.0);
            node.pivot.y = parseDouble(tokens[15]).value_or(0.0);
            node.pivot.z = parseDouble(tokens[16]).value_or(0.0);

            budget.chargeBytes(tokens[17].size() + 1u);
            node.name = std::string(tokens[17]);
            model.nodes.push_back(std::move(node));
        }
    }

    model.hierarchyTransformsApplied = options.applyHierarchyTransforms;
    if (options.applyHierarchyTransforms)
    {
        offsetMeshes(model, budget);
    }
    computeRadii(model);
    return model;
}

Model3do loadModel3doBytes(
    const std::span<const std::byte> bytes,
    const Model3doLoadOptions& options,
    Model3doParseBudget& budget)
{
    const Model3doSections sections = scanModel3doSections(bytes, budget);
    return parseModel3doSections(sections, options, budget);
}

} // namespace

std::size_t selectModel3doLodIndex(
    const std::size_t geosetCount,
    const std::array<double, 4>& lodDistances,
    const double cameraForwardDepth) noexcept
{
    if (geosetCount <= 1 || !std::isfinite(cameraForwardDepth))
    {
        return 0;
    }

    // Sith tests the second header threshold for two-geoset models, the
    // first two for three-geoset models, and the first three for four or more.
    // Keep the original comparison order, including equality taking the
    // farther geoset. A corrupt required threshold falls back to HQ.
    if (!std::isfinite(lodDistances[1])
        || (geosetCount >= 3 && !std::isfinite(lodDistances[0]))
        || (geosetCount >= 4 && !std::isfinite(lodDistances[2])))
    {
        return 0;
    }

    if (geosetCount == 2)
    {
        return cameraForwardDepth < lodDistances[1] ? 0 : 1;
    }
    if (cameraForwardDepth < lodDistances[0])
    {
        return 0;
    }
    if (geosetCount == 3)
    {
        return cameraForwardDepth >= lodDistances[1] ? 2 : 1;
    }
    if (cameraForwardDepth < lodDistances[1])
    {
        return 1;
    }
    return cameraForwardDepth >= lodDistances[2] ? 3 : 2;
}

Model3do loadFrom3DO(std::istream& in, const Model3doLoadOptions& options)
{
    Model3doParseBudget budget{};
    bool exceededSourceBudget = false;
    std::optional<std::string> buffer =
        detail::readBinaryStreamWithinBudget<std::string>(
            in,
            budget.maxSourceBytes,
            0u,
            exceededSourceBudget,
            "Failed to read 3DO stream.");
    if (exceededSourceBudget)
    {
        throw std::runtime_error(std::string(k3doSourceBudgetDiagnostic));
    }
    if (!buffer)
    {
        throw std::runtime_error("Failed to read 3DO stream.");
    }

    return loadModel3doBytes(
        std::as_bytes(std::span(buffer->data(), buffer->size())), options, budget);
}

Model3do loadFrom3DO(const std::filesystem::path& path, const Model3doLoadOptions& options)
{
    std::ifstream file(path, std::ios::binary);
    if (!file)
    {
        throw std::runtime_error("Failed to open 3DO file.");
    }
    return loadFrom3DO(file, options);
}

Model3do loadFrom3DO(
    const std::span<const std::byte> bytes,
    const Model3doLoadOptions& options)
{
    Model3doParseBudget budget{};
    return loadModel3doBytes(bytes, options, budget);
}

} // namespace sed::assets
