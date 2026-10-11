#include "test_common.hpp"

#include "sed/assets/3do.hpp"

#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <limits>
#include <optional>
#include <span>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

namespace sed::assets::detail
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

std::optional<Model3doBudgetLimitsForTests>
exchangeModel3doBudgetLimitsForTests(
    std::optional<Model3doBudgetLimitsForTests> limits) noexcept;

} // namespace sed::assets::detail

namespace sed::tests
{

namespace
{

class ScopedModel3doBudgetLimitsForTests
{
public:
    explicit ScopedModel3doBudgetLimitsForTests(
        const sed::assets::detail::Model3doBudgetLimitsForTests limits)
        : previous_(sed::assets::detail::exchangeModel3doBudgetLimitsForTests(
            limits))
    {
    }

    ScopedModel3doBudgetLimitsForTests(
        const ScopedModel3doBudgetLimitsForTests&) = delete;
    ScopedModel3doBudgetLimitsForTests& operator=(
        const ScopedModel3doBudgetLimitsForTests&) = delete;

    ~ScopedModel3doBudgetLimitsForTests()
    {
        (void)sed::assets::detail::exchangeModel3doBudgetLimitsForTests(
            std::move(previous_));
    }

private:
    std::optional<sed::assets::detail::Model3doBudgetLimitsForTests> previous_{};
};

} // namespace

void runModel3doTests()
{
    {
        // These are the camera-forward distance branches in Sith's model
        // renderer. The fourth header value is not used by 3DO geoset choice.
        constexpr std::array<double, 4> distances{ 1.0, 2.0, 3.0, 4.0 };
        struct LodCase
        {
            std::size_t geosets;
            double depth;
            std::size_t expected;
        };
        constexpr std::array cases{
            LodCase{ 0, 10.0, 0 },
            LodCase{ 1, 0.0, 0 },
            LodCase{ 1, 10.0, 0 },
            LodCase{ 2, 1.0, 0 },
            LodCase{ 2, 1.999, 0 },
            LodCase{ 2, 2.0, 1 },
            LodCase{ 2, 10.0, 1 },
            LodCase{ 3, 0.999, 0 },
            LodCase{ 3, 1.0, 1 },
            LodCase{ 3, 1.999, 1 },
            LodCase{ 3, 2.0, 2 },
            LodCase{ 4, 0.999, 0 },
            LodCase{ 4, 1.0, 1 },
            LodCase{ 4, 1.999, 1 },
            LodCase{ 4, 2.0, 2 },
            LodCase{ 4, 2.999, 2 },
            LodCase{ 4, 3.0, 3 },
            LodCase{ 5, 3.0, 3 },
        };
        for (const LodCase& item : cases)
        {
            check(sed::assets::selectModel3doLodIndex(
                    item.geosets, distances, item.depth) == item.expected,
                "3DO.LOD: engine geoset selection uses camera-forward depth and exact header thresholds");
        }
        check(sed::assets::selectModel3doLodIndex(4, distances, -2.0) == 0
                && sed::assets::selectModel3doLodIndex(
                    4, std::array<double, 4>{ 1.0, 2.0, 3.0, -100.0 }, 4.0) == 3,
            "3DO.LOD: behind-camera depth selects HQ and the fourth header threshold is ignored");
        check(sed::assets::selectModel3doLodIndex(
                4, distances, std::numeric_limits<double>::quiet_NaN()) == 0
                && sed::assets::selectModel3doLodIndex(
                    4, std::array<double, 4>{ 1.0, 2.0,
                        std::numeric_limits<double>::infinity(), 4.0 }, 10.0) == 0,
            "3DO.LOD: non-finite depth or required threshold safely selects HQ");
    }

    {
        auto makeModel = [](const std::size_t geosetCount)
        {
            std::string source =
                "SECTION: HEADER\n"
                "3DO 2.1\n"
                "SECTION: GEOMETRYDEF\n"
                "GEOSETS " + std::to_string(geosetCount) + "\n";
            for (std::size_t i = 0; i < geosetCount; ++i)
            {
                source += "GEOSET " + std::to_string(i)
                    + "\nMESHES 1\nMESH 0\nNAME lod" + std::to_string(i)
                    + "\nVERTICES 3\n0: 0 0 0\n1: 1 0 0\n2: 0 1 0\nFACES 0\n";
            }
            return source;
        };

        constexpr std::array<std::string_view, 4> expectedNames{
            "lod0", "lod1", "lod2", "lod3"
        };
        for (std::size_t geosetCount = 1; geosetCount <= 4; ++geosetCount)
        {
            const std::string source = makeModel(geosetCount);
            const auto bytes = std::as_bytes(std::span(source.data(), source.size()));
            for (std::size_t requested = 0; requested < geosetCount; ++requested)
            {
                sed::assets::Model3doLoadOptions options{};
                options.lod = static_cast<std::int32_t>(requested);
                const sed::assets::Model3do model = sed::assets::loadFrom3DO(bytes, options);
                check(model.geosetCount == geosetCount
                        && model.meshes.size() == 1
                        && model.meshes[0].name == expectedNames[requested],
                    "3DO.LOD: each requested geoset loads its own mesh and reports the declared count");
            }
            sed::assets::Model3doLoadOptions clamped{};
            clamped.lod = 99;
            const sed::assets::Model3do last = sed::assets::loadFrom3DO(bytes, clamped);
            check(last.geosetCount == geosetCount
                    && last.meshes.size() == 1
                    && last.meshes[0].name == expectedNames[geosetCount - 1],
                "3DO.LOD: requested geoset above the declared range clamps to the final geoset");
        }

        const std::string missingMeshList =
            "SECTION: HEADER\n3DO 2.1\nSECTION: GEOMETRYDEF\n"
            "GEOSETS 2\nGEOSET 0\nGEOSET 1\nMESHES 1\n"
            "MESH 0\nNAME lod1\nVERTICES 3\n"
            "0: 0 0 0\n1: 1 0 0\n2: 0 1 0\nFACES 0\n";
        const auto malformedBytes = std::as_bytes(std::span(
            missingMeshList.data(), missingMeshList.size()));
        const sed::assets::Model3do missing = sed::assets::loadFrom3DO(malformedBytes);
        check(missing.geosetCount == 2 && missing.meshes.empty(),
            "3DO.LOD: a malformed geoset cannot borrow the next geoset's mesh list");

        const std::string missingIndex =
            "SECTION: HEADER\n3DO 2.1\nSECTION: GEOMETRYDEF\n"
            "GEOSETS 2\nGEOSET 0\nGEOSET\nMESHES 1\n"
            "MESH 0\nNAME unknown\nVERTICES 3\n"
            "0: 0 0 0\n1: 1 0 0\n2: 0 1 0\nFACES 0\n";
        const auto missingIndexBytes = std::as_bytes(std::span(
            missingIndex.data(), missingIndex.size()));
        const sed::assets::Model3do malformed = sed::assets::loadFrom3DO(missingIndexBytes);
        check(malformed.geosetCount == 2 && malformed.meshes.empty(),
            "3DO.LOD: a GEOSET line missing its index still bounds the previous geoset");
    }

    {
        constexpr std::string_view malformedCountDiagnostic =
            "Cannot load 3DO: declared record count is invalid or truncated.";
        const auto expectMalformed = [malformedCountDiagnostic](
            const std::string& source,
            const char* const message)
        {
            checkThrowsWithMessage(
                [&source]()
                {
                    const auto bytes = std::as_bytes(std::span(
                        source.data(), source.size()));
                    (void)sed::assets::loadFrom3DO(bytes);
                },
                malformedCountDiagnostic,
                message);
        };
        const auto makeMeshInput = [](
            const std::string_view version,
            const std::string_view records)
        {
            std::string source = "SECTION: HEADER\n3DO ";
            source.append(version);
            source += "\nSECTION: GEOMETRYDEF\n"
                "GEOSETS 1\nGEOSET 0\nMESHES 1\nMESH 0\n";
            source.append(records);
            return source;
        };

        // Keep malformed records intentionally small. The equivalent
        // INT32_MAX fixture would try to initialize roughly 80 GiB on the
        // current parser before discovering that it has no vertex rows.
        // A count of ten with no corresponding data exercises the same
        // preflight contract without making the regression itself unsafe.
        const std::string hugeMaterialCount =
            "SECTION: HEADER\n3DO 2.1\n"
            "SECTION: MODELRESOURCE\nMATERIALS 10\n"
            "SECTION: GEOMETRYDEF\nGEOSETS 1\nGEOSET 0\nMESHES 0\n";
        expectMalformed(hugeMaterialCount,
            "3DO.budget: materials count is checked against available rows before reserve");

        const std::string vertexCountWithoutRows =
            makeMeshInput("2.1", "VERTICES 10\nFACES 0\n");
        expectMalformed(vertexCountWithoutRows,
            "3DO.budget: vertices count is checked against available rows before resize");

        const std::string textureVertexCountWithoutRows =
            makeMeshInput("2.1", "VERTICES 0\nTEXTURE VERTICES 10\nFACES 0\n");
        expectMalformed(textureVertexCountWithoutRows,
            "3DO.budget: texture vertices count is checked against available rows before resize");

        const std::string faceCountWithoutRows =
            makeMeshInput("2.3", "VERTICES 0\nFACES 10\n");
        expectMalformed(faceCountWithoutRows,
            "3DO.budget: faces count is checked against available rows before reserve");

        const std::string faceCornerCountWithoutPairs =
            makeMeshInput("2.3",
                "VERTICES 0\n"
                "FACES 1\n"
                "0: 0 0x0 4 3 3 (0/0/0/1) 10\n");
        expectMalformed(faceCornerCountWithoutPairs,
            "3DO.budget: face corner count is checked against available index pairs before reserve");

        const std::string hierarchyCountWithoutRows =
            makeMeshInput("2.1", "VERTICES 0\nFACES 0\n")
            + "SECTION: HIERARCHYDEF\nHIERARCHY NODES 10\n";
        expectMalformed(hierarchyCountWithoutRows,
            "3DO.budget: hierarchy node count is checked against available rows before reserve");

        const std::string negativeVertexCount =
            makeMeshInput("2.1", "VERTICES -1\nFACES 0\n");
        expectMalformed(negativeVertexCount,
            "3DO.budget: negative geometry counts are rejected instead of treated as empty");
    }

    {
        const char* authoredMeshRadius3do =
            R"3DO(
                SECTION: HEADER
                3DO 2.3

                SECTION: GEOMETRYDEF
                GEOSETS 1
                GEOSET 0
                MESHES 1
                MESH 0
                NAME radius_mesh
                RADIUS 4
                VERTICES 3
                0: 0 0 0
                1: 1 0 0
                2: 0 1 0
                FACES 0

                SECTION: HIERARCHYDEF
                HIERARCHY NODES 2
                0: 0x0 0x1 -1 -1 0 0 0 10 0 0 0 0 0 0 0 0 root
                1: 0x2 0x1 0 0 0 0 0 5 0 0 0 0 0 0 0 0 radius_mesh
            )3DO";

        const auto authoredRadiusBytes = std::as_bytes(std::span(
            authoredMeshRadius3do,
            std::strlen(authoredMeshRadius3do)));
        const sed::assets::Model3do transformed =
            sed::assets::loadFrom3DO(authoredRadiusBytes);
        sed::assets::Model3doLoadOptions rawOptions{};
        rawOptions.applyHierarchyTransforms = false;
        const sed::assets::Model3do raw =
            sed::assets::loadFrom3DO(authoredRadiusBytes, rawOptions);

        check(transformed.meshes.size() == 1
                && transformed.meshes[0].authoredRadius == 4.0
                && transformed.meshes[0].radius > 15.0,
            "3DO.meshRadius: hierarchy-transformed loads preserve authored radius separately from renderer radius");
        check(raw.meshes.size() == 1
                && raw.meshes[0].authoredRadius == 4.0
                && raw.meshes[0].radius == 4.0,
            "3DO.meshRadius: raw hierarchy load preserves the same authored radius and local computed radius");
        check(transformed.nodes.size() == 2
                && transformed.nodes[0].flags == 0u && transformed.nodes[0].type == 1u
                && transformed.nodes[1].flags == 2u && transformed.nodes[1].type == 1u,
            "3DO hierarchy flags and type remain distinct; MotS radius flag 0x2 is parsed exactly");
    }
    {
        const char* spanSyntax3do =
            "SECTION: HEADER\r\n"
            "3DO +2.3 # inline comment\r\n"
            "SECTION: GEOMETRYDEF\r\n"
            "RADIUS +1.25e+1\r\n"
            "INSERT OFFSET -2.5E-1 +3.0 4e-2\r\n"
            "GEOSETS 1\r\n"
            "GEOSET 0\r\n"
            "MESHES 1\r\n"
            "MESH 0\r\n"
            "NAME numeric_mesh\r\n"
            "VERTICES 3\r\n"
            "0: +1e0 0 -0.0\r\n"
            "1: 0 +2.5e0 0\r\n"
            "2: 0 0 -3.5e-1\r\n"
            "FACES 0\r\n"
            "SECTION: HIERARCHYDEF\r\n"
            "HIERARCHY NODES 1\r\n"
            "0: 0x0 0x1 0 -1 -1 -1 0 +1e0 -2e0 +3e0 0 0 0 0 0 0 numeric_mesh\r\n";
        const auto bytes = std::as_bytes(std::span(
            spanSyntax3do,
            std::strlen(spanSyntax3do)));
        const sed::assets::Model3do model = sed::assets::loadFrom3DO(bytes);

        check(close(model.version, 2.3, kRotationEpsilon)
                && close(model.radius, 12.5, kRotationEpsilon)
                && closeVec(model.insertOffset, sed::TVector{ -0.25, 3.0, 0.04 })
                && model.meshes.size() == 1
                && model.meshes[0].vertices.size() == 3
                && model.nodes.size() == 1,
            "3DO.spanSyntax: byte-span parser preserves CRLF, inline comments, signs, and exponent notation");
        if (model.meshes.size() == 1 && model.meshes[0].vertices.size() == 3)
        {
            check(close(model.meshes[0].vertices[0].x, 2.0)
                    && close(model.meshes[0].vertices[1].y, 0.5)
                    && close(model.meshes[0].vertices[2].z, 2.65),
                "3DO.spanSyntax: hierarchy transforms remain applied after direct byte parsing");
        }
    }

    {
        const char* model3do =
            R"3DO(
                # MODEL test.3do created by SED
                SECTION: HEADER

                3DO 2.3

                SECTION: MODELRESOURCE

                MATERIALS 1
                0: test.mat

                SECTION: GEOMETRYDEF

                RADIUS 1.000000
                INSERT OFFSET 0 0 0
                GEOSETS 1
                GEOSET 0
                MESHES 1

                MESH 0
                NAME mesh0
                RADIUS 1.000000
                GEOMETRYMODE 4
                LIGHTINGMODE 3
                TEXTUREMODE 3

                VERTICES 3
                0: 0 0 0 0 0 0 1
                1: 1 0 0 0 0 0 1
                2: 0 1 0 0 0 0 1

                TEXTURE VERTICES 3
                0: -0.375 0.25
                1: 1.75 -0.5
                2: 0.125 2.875

                VERTEX NORMALS
                0: 0 0 1
                1: 0 0 1
                2: 0 0 1

                FACES 1
                0: 0 0x0 0 0 0 (0.1/0.2/0.3/1.0) 3 0, 0 1, 1 2, 2

                FACE NORMALS
                0: 0 0 1

                SECTION: HIERARCHYDEF

                HIERARCHY NODES 1
                0: 0x0 0x1 0 -1 -1 -1 0 0 0 0 0 0 0 0 0 0 mesh0
            )3DO";

        std::istringstream iss(model3do);
        const sed::assets::Model3do model = sed::assets::loadFrom3DO(iss);
        const auto bytes = std::as_bytes(std::span(
            model3do,
            std::strlen(model3do)));
        const sed::assets::Model3do spanModel =
            sed::assets::loadFrom3DO(bytes);

        check(spanModel.version == model.version
                && spanModel.materials == model.materials
                && spanModel.meshes.size() == model.meshes.size()
                && spanModel.nodes.size() == model.nodes.size(),
            "3DO: in-memory byte-span parsing preserves the stream parser result without a whole-file string copy");

        check(close(model.version, 2.3, kRotationEpsilon), "3DO: version");
        check(close(model.radius, 1.0), "3DO: radius");
        check(closeVec(model.insertOffset, sed::TVector::zero), "3DO: insert offset");

        check(model.materials.size() == 1, "3DO: materials");
        check(model.materials.size() == 1 && model.materials[0] == "test.mat", "3DO: material name");

        check(model.meshes.size() == 1, "3DO: meshes");
        check(model.meshes.size() == 1 && model.meshes[0].name == "mesh0", "3DO: mesh name");

        check(model.meshes.size() == 1 && model.meshes[0].vertices.size() == 3, "3DO: vertices");
        check(model.meshes.size() == 1 && model.meshes[0].texVertices.size() == 3, "3DO: tex vertices");
        check(model.meshes.size() == 1 && model.meshes[0].vertexNormals.size() == 3, "3DO: vertex normals");
        check(model.meshes.size() == 1 && model.meshes[0].faces.size() == 1, "3DO: faces");

        if (!model.meshes.empty() && !model.meshes[0].faces.empty())
        {
            // Model3doTexVertex stays version-native. Version 2.3 uses
            // normalized wire values; a consumer that creates Level surfaces
            // must resolve MAT dimensions at that format boundary.
            check(model.meshes[0].texVertices.size() == 3
                    && close(model.meshes[0].texVertices[0].u, -0.375)
                    && close(model.meshes[0].texVertices[0].v, 0.25)
                    && close(model.meshes[0].texVertices[1].u, 1.75)
                    && close(model.meshes[0].texVertices[1].v, -0.5)
                    && close(model.meshes[0].texVertices[2].u, 0.125)
                    && close(model.meshes[0].texVertices[2].v, 2.875)
                    && spanModel.meshes.size() == 1
                    && spanModel.meshes[0].texVertices.size() == 3
                    && close(spanModel.meshes[0].texVertices[0].u, -0.375)
                    && close(spanModel.meshes[0].texVertices[0].v, 0.25)
                    && close(spanModel.meshes[0].texVertices[1].u, 1.75)
                    && close(spanModel.meshes[0].texVertices[1].v, -0.5)
                    && close(spanModel.meshes[0].texVertices[2].u, 0.125)
                    && close(spanModel.meshes[0].texVertices[2].v, 2.875),
                "3DO 2.3 UV contract: stream and byte-span parsers preserve signed and multi-tile normalized wire values without hidden pixel conversion");

            const auto& face = model.meshes[0].faces[0];
            check(face.material == 0, "3DO: face material index");
            check(face.vertices.size() == 3, "3DO: face vertex indices");
            check(face.texVertices.size() == 3, "3DO: face tex indices");
            check(close(face.extraLight.r, 0.1, kRotationEpsilon) && close(face.extraLight.g, 0.2, kRotationEpsilon) &&
                      close(face.extraLight.b, 0.3, kRotationEpsilon),
                  "3DO: face extralight");
        }

        check(model.meshes.size() == 1 && model.meshes[0].faceNormals.size() == 1, "3DO: face normals");
        check(model.nodes.size() == 1, "3DO: nodes");
        check(model.nodes.size() == 1 && model.nodes[0].name == "mesh0", "3DO: node name");
    }

    {
        const char* model3do =
            R"3DO(
                SECTION: HEADER
                3DO 2.3

                SECTION: GEOMETRYDEF
                GEOSETS 1
                GEOSET 0
                MESHES 1

                MESH 0
                NAME mesh0

                VERTICES 3
                0: 1 1 1 0 0 0 1
                1: 2 1 1 0 0 0 1
                2: 1 2 1 0 0 0 1

                FACES 0

                SECTION: HIERARCHYDEF
                HIERARCHY NODES 2
                0: 0x0 0x1 -1 -1 1 -1 1 100 0 0 0 0 0 999 0 0 root
                1: 0x0 0x1 0 0 -1 -1 0 0 10 0 0 0 0 1 2 3 mesh0
            )3DO";

        std::istringstream iss(model3do);
        const sed::assets::Model3do model = sed::assets::loadFrom3DO(iss);

        check(model.meshes.size() == 1, "3DO.offset: meshes");
        check(model.nodes.size() == 2, "3DO.offset: nodes");
        check(model.meshes.size() == 1 && model.meshes[0].vertices.size() == 3, "3DO.offset: vertices");

        if (!model.meshes.empty() && model.meshes[0].vertices.size() == 3)
        {
            check(close(model.meshes[0].vertices[0].x, 102.0), "3DO.offset: v0.x");
            check(close(model.meshes[0].vertices[0].y, 13.0), "3DO.offset: v0.y");
            check(close(model.meshes[0].vertices[0].z, 4.0), "3DO.offset: v0.z");

            check(close(model.meshes[0].vertices[1].x, 103.0), "3DO.offset: v1.x");
            check(close(model.meshes[0].vertices[1].y, 13.0), "3DO.offset: v1.y");
            check(close(model.meshes[0].vertices[1].z, 4.0), "3DO.offset: v1.z");

            check(close(model.meshes[0].vertices[2].x, 102.0), "3DO.offset: v2.x");
            check(close(model.meshes[0].vertices[2].y, 14.0), "3DO.offset: v2.y");
            check(close(model.meshes[0].vertices[2].z, 4.0), "3DO.offset: v2.z");
        }
    }

    {
        const char* model3do =
            R"3DO(
                SECTION: HEADER
                3DO 2.3

                SECTION: GEOMETRYDEF
                GEOSETS 1
                GEOSET 0
                MESHES 1

                MESH 0
                NAME mesh0

                VERTICES 3
                0: 0 0 0
                1: 1 0 0
                2: 0 1 0

                VERTEX NORMALS
                0: 0 0 1
                1: 0 2 0
                2: 1 0 0

                FACES 1
                0: 0 0x0 4 3 0 (0/0/0/1) 3 0, 0 1, 0 2, 0

                FACE NORMALS
                0: 0 0 3

                SECTION: HIERARCHYDEF
                HIERARCHY NODES 2
                0: 0x0 0x1 -1 -1 1 -1 1 10 20 30 0 90 0 4 5 6 root
                1: 0x0 0x1 0 0 -1 -1 0 40 50 60 90 0 0 7 8 9 mesh0
            )3DO";

        std::istringstream iss(model3do);
        const sed::assets::Model3do model = sed::assets::loadFrom3DO(iss);

        check(model.meshes.size() == 1, "3DO.offsetNormals: meshes");
        if (!model.meshes.empty())
        {
            const auto& mesh = model.meshes[0];

            check(mesh.vertexNormals.size() == 3, "3DO.offsetNormals: vertex normal count");
            if (mesh.vertexNormals.size() == 3)
            {
                check(closeVec(mesh.vertexNormals[0], sed::TVector{ 1.0, 0.0, 0.0 }),
                      "3DO.offsetNormals: vertex normal 0");
                check(closeVec(mesh.vertexNormals[1], sed::TVector{ 0.0, 0.0, 2.0 }),
                      "3DO.offsetNormals: vertex normal 1");
                check(closeVec(mesh.vertexNormals[2], sed::TVector{ 0.0, 1.0, 0.0 }),
                      "3DO.offsetNormals: vertex normal 2");
            }

            check(mesh.faceNormals.size() == 1, "3DO.offsetNormals: face normal count");
            if (mesh.faceNormals.size() == 1)
            {
                check(closeVec(mesh.faceNormals[0], sed::TVector{ 3.0, 0.0, 0.0 }),
                      "3DO.offsetNormals: face normal");
            }
        }
    }

    {
        const char* classicUv3do = R"3DO(
            SECTION: HEADER
            3DO 2.1
            SECTION: GEOMETRYDEF
            GEOSETS 1
            GEOSET 0
            MESHES 1
            MESH 0
            NAME classic_uv
            VERTICES 3
            0: 0 0 0 1
            1: 1 0 0 1
            2: 0 1 0 1
            TEXTURE VERTICES 3
            0: -64 16
            1: 384 -32
            2: 8 192
            FACES 1
            0: 0 0x0 4 3 3 0.0 3 0,0 1,1 2,2
        )3DO";

        std::istringstream classicUvStream(classicUv3do);
        const sed::assets::Model3do classicUvModel =
            sed::assets::loadFrom3DO(classicUvStream);
        const auto classicUvBytes = std::as_bytes(std::span(
            classicUv3do,
            std::strlen(classicUv3do)));
        const sed::assets::Model3do classicUvSpanModel =
            sed::assets::loadFrom3DO(classicUvBytes);
        check(close(classicUvModel.version, 2.1)
                && classicUvModel.meshes.size() == 1
                && classicUvModel.meshes[0].texVertices.size() == 3
                && close(classicUvModel.meshes[0].texVertices[0].u, -64.0)
                && close(classicUvModel.meshes[0].texVertices[0].v, 16.0)
                && close(classicUvModel.meshes[0].texVertices[1].u, 384.0)
                && close(classicUvModel.meshes[0].texVertices[1].v, -32.0)
                && close(classicUvModel.meshes[0].texVertices[2].u, 8.0)
                && close(classicUvModel.meshes[0].texVertices[2].v, 192.0)
                && classicUvSpanModel.meshes.size() == 1
                && classicUvSpanModel.meshes[0].texVertices.size() == 3
                && close(classicUvSpanModel.meshes[0].texVertices[0].u,
                    -64.0)
                && close(classicUvSpanModel.meshes[0].texVertices[0].v,
                    16.0)
                && close(classicUvSpanModel.meshes[0].texVertices[1].u,
                    384.0)
                && close(classicUvSpanModel.meshes[0].texVertices[1].v,
                    -32.0)
                && close(classicUvSpanModel.meshes[0].texVertices[2].u,
                    8.0)
                && close(classicUvSpanModel.meshes[0].texVertices[2].v,
                    192.0),
            "3DO 2.1 UV contract: stream and byte-span parsers preserve signed and multi-tile classic pixel-space values without hidden normalization");
    }

    {
        for (const std::string_view version : {
                 std::string_view("2.1"),
                 std::string_view("2.3") })
        {
            const std::string overflow3do =
                "SECTION: HEADER\n"
                "3DO "
                + std::string(version)
                + "\n"
                "SECTION: GEOMETRYDEF\n"
                "GEOSETS 1\n"
                "GEOSET 0\n"
                "MESHES 1\n"
                "MESH 0\n"
                "NAME overflow_uv\n"
                "VERTICES 0\n"
                "TEXTURE VERTICES 1\n"
                "0: 3.5e38 0\n"
                "FACES 0\n";

            bool streamRejected = false;
            try
            {
                std::istringstream input(overflow3do);
                (void)sed::assets::loadFrom3DO(input);
            }
            catch (const std::runtime_error& exception)
            {
                streamRejected = std::string_view(exception.what()).find(
                    "texture vertex") != std::string_view::npos;
            }

            bool spanRejected = false;
            try
            {
                const auto bytes = std::as_bytes(std::span(
                    overflow3do.data(), overflow3do.size()));
                (void)sed::assets::loadFrom3DO(bytes);
            }
            catch (const std::runtime_error& exception)
            {
                spanRejected = std::string_view(exception.what()).find(
                    "texture vertex") != std::string_view::npos;
            }

            const std::string message =
                "3DO " + std::string(version)
                + " loader rejects finite texture-coordinate doubles that overflow float storage in stream and byte-span paths";
            check(streamRejected && spanRejected, message.c_str());
        }
    }

    {
        const char* sharedMesh3do =
            R"3DO(
                SECTION: HEADER
                3DO 2.3

                SECTION: MODELRESOURCE
                MATERIALS 1
                0: shared.mat

                SECTION: GEOMETRYDEF
                GEOSETS 1
                GEOSET 0
                MESHES 1

                MESH 0
                NAME shared
                VERTICES 3
                0: 0 0 0
                1: 1 0 0
                2: 0 1 0
                TEXTURE VERTICES 3
                0: 0 0
                1: 1 0
                2: 0 1
                FACES 1
                0: 0 0x0 4 3 3 (0/0/0/1) 3 0,0 1,1 2,2

                SECTION: HIERARCHYDEF
                HIERARCHY NODES 2
                0: 0x0 0x1 0 -1 -1 -1 0 10 0 0 0 0 0 0 0 0 first
                1: 0x0 0x1 0 -1 -1 -1 0 20 0 0 0 0 0 0 0 0 second
            )3DO";

        std::istringstream transformedStream(sharedMesh3do);
        const sed::assets::Model3do transformed =
            sed::assets::loadFrom3DO(transformedStream);
        check(transformed.hierarchyTransformsApplied
                && transformed.nodes.size() == 2
                && transformed.materials == std::vector<std::string>{ "shared.mat" }
                && transformed.meshes.size() == 2
                && transformed.meshSourceIndices.size() == 2
                && transformed.meshSourceIndices[0] == 0
                && transformed.meshSourceIndices[1] == 0
                && transformed.nodes[0].mesh == 0
                && transformed.nodes[1].mesh == 1,
            "3DO.sharedMesh: default load creates independent instances while retaining each source mesh ordinal");
        if ( transformed.meshes.size() == 2 )
        {
            const auto hasCompleteGeometry = [](const sed::assets::Model3doMesh& mesh)
            {
                return mesh.vertices.size() == 3
                    && mesh.texVertices.size() == 3
                    && mesh.faces.size() == 1
                    && mesh.faces[0].vertices == std::vector<std::int32_t>{ 0, 1, 2 }
                    && mesh.faces[0].texVertices == std::vector<std::int32_t>{ 0, 1, 2 }
                    && close(mesh.texVertices[1].u, 1.0)
                    && close(mesh.texVertices[2].v, 1.0);
            };
            check(hasCompleteGeometry(transformed.meshes[0])
                    && hasCompleteGeometry(transformed.meshes[1])
                    && close(transformed.meshes[0].vertices[0].x, 10.0)
                    && close(transformed.meshes[1].vertices[0].x, 20.0),
                "3DO.sharedMesh: hierarchy expansion retains every intentional mesh instance with vertices, UVs, faces, and corners");
        }

        std::string singleNodeSharedMesh3do(sharedMesh3do);
        constexpr std::string_view nodeCountLine = "HIERARCHY NODES 2";
        constexpr std::string_view secondNodeLine =
            "1: 0x0 0x1 0 -1 -1 -1 0 20 0 0 0 0 0 0 0 0 second";
        const std::size_t nodeCountPosition =
            singleNodeSharedMesh3do.find(nodeCountLine);
        const std::size_t secondNodePosition =
            singleNodeSharedMesh3do.find(secondNodeLine);
        bool singleNodeFixtureReady =
            nodeCountPosition != std::string::npos
            && secondNodePosition != std::string::npos;
        if (singleNodeFixtureReady)
        {
            singleNodeSharedMesh3do.replace(
                nodeCountPosition, nodeCountLine.size(), "HIERARCHY NODES 1");
            const std::size_t previousNewline =
                singleNodeSharedMesh3do.rfind('\n', secondNodePosition);
            const std::size_t lineStart = previousNewline == std::string::npos
                ? 0u
                : previousNewline + 1u;
            const std::size_t lineEnd =
                singleNodeSharedMesh3do.find('\n', secondNodePosition);
            singleNodeFixtureReady = lineStart != std::string::npos
                && lineEnd != std::string::npos;
            if (singleNodeFixtureReady)
            {
                singleNodeSharedMesh3do.erase(
                    lineStart, lineEnd + 1u - lineStart);
            }
        }
        check(singleNodeFixtureReady,
            "3DO.budget: one-node reduced-budget control fixture is prepared");

        constexpr sed::assets::detail::Model3doBudgetLimitsForTests reducedBudget{
            1024u * 1024u, 30u };
        bool singleNodeBudgetAccepted = false;
        {
            ScopedModel3doBudgetLimitsForTests limits{ reducedBudget };
            try
            {
                const auto bytes = std::as_bytes(std::span(
                    singleNodeSharedMesh3do.data(), singleNodeSharedMesh3do.size()));
                const sed::assets::Model3do singleNode =
                    sed::assets::loadFrom3DO(bytes);
                singleNodeBudgetAccepted = singleNode.meshes.size() == 1
                    && singleNode.nodes.size() == 1
                    && singleNode.nodes[0].mesh == 0
                    && singleNode.meshes[0].vertices.size() == 3;
            }
            catch (const std::runtime_error&)
            {
            }
        }
        check(singleNodeBudgetAccepted,
            "3DO.budget: a single valid hierarchy instance fits the reduced budget");

        bool expansionBudgetRejected = false;
        {
            // The authored mesh fits this item limit. The second valid node
            // intentionally clones it and should exceed the aggregate limit.
            // Raw storage and its shared-source snapshot fit; the retained
            // second instance must be the charge that crosses the limit.
            ScopedModel3doBudgetLimitsForTests limits{ reducedBudget };
            try
            {
                const auto bytes = std::as_bytes(std::span(
                    sharedMesh3do, std::strlen(sharedMesh3do)));
                (void)sed::assets::loadFrom3DO(bytes);
            }
            catch (const std::runtime_error& exception)
            {
                expansionBudgetRejected = std::string_view(exception.what())
                    == "Cannot load 3DO: decoded geometry budget is exceeded.";
            }
        }
        check(expansionBudgetRejected,
            "3DO.budget: aggregate hierarchy mesh clones are charged before expansion");

        std::istringstream rawStream(sharedMesh3do);
        sed::assets::Model3doLoadOptions rawOptions{};
        rawOptions.applyHierarchyTransforms = false;
        const sed::assets::Model3do raw =
            sed::assets::loadFrom3DO(rawStream, rawOptions);
        check(!raw.hierarchyTransformsApplied
                && raw.nodes.size() == 2
                && raw.meshes.size() == 1
                && raw.meshSourceIndices.size() == 1
                && raw.meshSourceIndices[0] == 0
                && raw.meshes[0].vertices.size() == 3
                && raw.meshes[0].texVertices.size() == 3
                && raw.meshes[0].faces.size() == 1
                && raw.meshes[0].faces[0].vertices.size() == 3
                && close(raw.meshes[0].vertices[0].x, 0.0),
            "3DO.sharedMesh: raw load preserves authored mesh storage and ordinal identity for import-time instancing");
    }

    {
        constexpr std::string_view sourceBudgetDiagnostic =
            "Cannot load 3DO: source byte budget is exceeded.";
        std::string exactSource = "SECTION: HEADER\n3DO 2.1\n";
        constexpr std::size_t sourceLimit = 64u;
        check(exactSource.size() < sourceLimit,
            "3DO.inputBudget: exact source boundary fixture is smaller than its reduced cap");
        exactSource.append(sourceLimit - exactSource.size(), ' ');

        sed::assets::detail::Model3doBudgetLimitsForTests limits{};
        limits.sourceBytes = sourceLimit;
        ScopedModel3doBudgetLimitsForTests scopedLimits{ limits };

        const auto exactBytes = std::as_bytes(std::span(
            exactSource.data(), exactSource.size()));
        const sed::assets::Model3do exactSpanModel =
            sed::assets::loadFrom3DO(exactBytes);
        std::istringstream exactInput(exactSource);
        const sed::assets::Model3do exactStreamModel =
            sed::assets::loadFrom3DO(exactInput);
        check(exactSpanModel.version == 2.1
                && exactStreamModel.version == 2.1,
            "3DO.inputBudget: byte-span and stream inputs exactly at the reduced source cap are accepted");

        const std::string overSource = exactSource + " ";
        const auto overBytes = std::as_bytes(std::span(
            overSource.data(), overSource.size()));
        checkThrowsWithMessage(
            [&overBytes]()
            {
                (void)sed::assets::loadFrom3DO(overBytes);
            },
            sourceBudgetDiagnostic,
            "3DO.inputBudget: a byte-span one byte over the reduced source cap is rejected");
        checkThrowsWithMessage(
            [&overSource]()
            {
                std::istringstream input(overSource);
                (void)sed::assets::loadFrom3DO(input);
            },
            sourceBudgetDiagnostic,
            "3DO.inputBudget: a stream one byte over the reduced source cap is rejected before owned-buffer growth");
    }

    {
        constexpr std::string_view sourceBudgetDiagnostic =
            "Cannot load 3DO: source byte budget is exceeded.";
        const std::string exactCrLfSource =
            "SECTION: HEADER\r\n3DO 2.1\r\n";
        const std::string overCrLfSource = exactCrLfSource + "\r\n";
        const std::size_t sourceLimit = exactCrLfSource.size();

        struct ScopedTempModel3doFile
        {
            std::filesystem::path path{};
            ~ScopedTempModel3doFile()
            {
                std::error_code error{};
                std::filesystem::remove(path, error);
            }
        };

        const std::string unique = std::to_string(
            std::chrono::steady_clock::now().time_since_epoch().count());
        ScopedTempModel3doFile tempFile{
            std::filesystem::temp_directory_path()
                / ("sed_model3do_source_limit_crlf_" + unique + ".3do") };
        const auto writeBinary = [&tempFile](const std::string& contents)
        {
            std::ofstream output(
                tempFile.path, std::ios::binary | std::ios::trunc);
            if (!output)
            {
                return false;
            }
            output.write(
                contents.data(), static_cast<std::streamsize>(contents.size()));
            output.close();
            return !output.fail();
        };

        sed::assets::detail::Model3doBudgetLimitsForTests limits{};
        limits.sourceBytes = sourceLimit;
        ScopedModel3doBudgetLimitsForTests scopedLimits{ limits };

        const auto exactBytes = std::as_bytes(std::span(
            exactCrLfSource.data(), exactCrLfSource.size()));
        const sed::assets::Model3do exactSpanModel =
            sed::assets::loadFrom3DO(exactBytes);
        const bool wroteExactFile = writeBinary(exactCrLfSource);
        check(wroteExactFile,
            "3DO.inputBudget: CRLF exact-boundary fixture writes as raw bytes");
        if (wroteExactFile)
        {
            bool exactPathAccepted = false;
            try
            {
                exactPathAccepted =
                    sed::assets::loadFrom3DO(tempFile.path).version == 2.1;
            }
            catch (const std::exception&)
            {
            }
            check(exactSpanModel.version == 2.1 && exactPathAccepted,
                "3DO.inputBudget: span and path loads accept an exact raw CRLF source boundary");
        }

        const auto overBytes = std::as_bytes(std::span(
            overCrLfSource.data(), overCrLfSource.size()));
        checkThrowsWithMessage(
            [&overBytes]()
            {
                (void)sed::assets::loadFrom3DO(overBytes);
            },
            sourceBudgetDiagnostic,
            "3DO.inputBudget: span rejects CRLF raw bytes one line ending over its source cap");

        const bool wroteOverFile = writeBinary(overCrLfSource);
        check(wroteOverFile,
            "3DO.inputBudget: CRLF over-boundary fixture writes as raw bytes");
        if (wroteOverFile)
        {
            checkThrowsWithMessage(
                [&tempFile]()
                {
                    (void)sed::assets::loadFrom3DO(tempFile.path);
                },
                sourceBudgetDiagnostic,
                "3DO.inputBudget: path load rejects raw CRLF bytes over the cap before text newline normalization");
        }
    }

    {
        constexpr std::string_view scannerBudgetDiagnostic =
            "Cannot load 3DO: section scanner workspace budget is exceeded.";
        constexpr std::size_t initialSectionCapacity =
            (4u + 16u + 128u + 32u) * sizeof(std::string_view);
        constexpr std::size_t expandedGeometryCapacity =
            (4u + 16u + 256u + 32u) * sizeof(std::string_view);

        const std::string oneRetainedGeometryLine =
            "SECTION: HEADER\n3DO 2.1\n"
            "SECTION: GEOMETRYDEF\nIGNORED\n";
        sed::assets::detail::Model3doBudgetLimitsForTests exactInitialLimits{};
        exactInitialLimits.scannerWorkspaceBytes = initialSectionCapacity;
        {
            ScopedModel3doBudgetLimitsForTests limits{ exactInitialLimits };
            const auto bytes = std::as_bytes(std::span(
                oneRetainedGeometryLine.data(), oneRetainedGeometryLine.size()));
            const sed::assets::Model3do model =
                sed::assets::loadFrom3DO(bytes);
            check(model.version == 2.1 && model.meshes.empty(),
                "3DO.scannerBudget: initial retained section capacities fit their exact workspace boundary");
        }

        std::string oneGrowthBeyondInitialCapacity =
            "SECTION: HEADER\n3DO 2.1\nSECTION: GEOMETRYDEF\n";
        for ( std::size_t line = 0; line < 129; ++line )
        {
            oneGrowthBeyondInitialCapacity += "IGNORED ";
            oneGrowthBeyondInitialCapacity += std::to_string(line);
            oneGrowthBeyondInitialCapacity.push_back('\n');
        }

        sed::assets::detail::Model3doBudgetLimitsForTests exactGrowthLimits{};
        exactGrowthLimits.scannerWorkspaceBytes = expandedGeometryCapacity;
        {
            ScopedModel3doBudgetLimitsForTests limits{ exactGrowthLimits };
            const auto bytes = std::as_bytes(std::span(
                oneGrowthBeyondInitialCapacity.data(),
                oneGrowthBeyondInitialCapacity.size()));
            const sed::assets::Model3do model =
                sed::assets::loadFrom3DO(bytes);
            check(model.version == 2.1 && model.meshes.empty(),
                "3DO.scannerBudget: section-vector growth fits the exact aggregate capacity boundary");
        }

        sed::assets::detail::Model3doBudgetLimitsForTests oneByteShortLimits{};
        oneByteShortLimits.scannerWorkspaceBytes = expandedGeometryCapacity - 1u;
        {
            ScopedModel3doBudgetLimitsForTests limits{ oneByteShortLimits };
            const auto bytes = std::as_bytes(std::span(
                oneGrowthBeyondInitialCapacity.data(),
                oneGrowthBeyondInitialCapacity.size()));
            checkThrowsWithMessage(
                [&bytes]()
                {
                    (void)sed::assets::loadFrom3DO(bytes);
                },
                scannerBudgetDiagnostic,
                "3DO.scannerBudget: aggregate retained line-vector capacity is rejected one byte below its growth boundary");
        }
    }

    {
        constexpr std::string_view tokenBudgetDiagnostic =
            "Cannot load 3DO: line token budget is exceeded.";
        const std::string exactTokenLine =
            "SECTION: GEOMETRYDEF\nIGNORED one two three\n";
        sed::assets::detail::Model3doBudgetLimitsForTests exactTokenLimits{};
        exactTokenLimits.tokensPerLine = 4u;
        {
            ScopedModel3doBudgetLimitsForTests limits{ exactTokenLimits };
            const auto bytes = std::as_bytes(std::span(
                exactTokenLine.data(), exactTokenLine.size()));
            const sed::assets::Model3do model =
                sed::assets::loadFrom3DO(bytes);
            check(model.meshes.empty(),
                "3DO.tokenBudget: one retained line exactly at the reduced token cap is accepted");
        }

        const std::string overTokenLine =
            "SECTION: GEOMETRYDEF\nIGNORED one two three four\n";
        sed::assets::detail::Model3doBudgetLimitsForTests overTokenLimits{};
        overTokenLimits.tokensPerLine = 4u;
        {
            ScopedModel3doBudgetLimitsForTests limits{ overTokenLimits };
            const auto bytes = std::as_bytes(std::span(
                overTokenLine.data(), overTokenLine.size()));
            checkThrowsWithMessage(
                [&bytes]()
                {
                    (void)sed::assets::loadFrom3DO(bytes);
                },
                tokenBudgetDiagnostic,
                "3DO.tokenBudget: a single retained line one token over the cap is rejected");
        }

        const std::string overSectionHeader =
            "SECTION: HEADER EXTRA\n3DO 2.1\n";
        sed::assets::detail::Model3doBudgetLimitsForTests sectionTokenLimits{};
        sectionTokenLimits.tokensPerLine = 2u;
        {
            ScopedModel3doBudgetLimitsForTests limits{ sectionTokenLimits };
            const auto bytes = std::as_bytes(std::span(
                overSectionHeader.data(), overSectionHeader.size()));
            checkThrowsWithMessage(
                [&bytes]()
                {
                    (void)sed::assets::loadFrom3DO(bytes);
                },
                tokenBudgetDiagnostic,
                "3DO.tokenBudget: SECTION header token growth is checked before allocation");
        }
    }

    {
        const auto makeSharedMeshChain = [](const std::size_t nodeCount)
        {
            std::string source =
                "SECTION: HEADER\n"
                "3DO 2.1\n"
                "SECTION: GEOMETRYDEF\n"
                "GEOSETS 1\n"
                "GEOSET 0\n"
                "MESHES 1\n"
                "MESH 0\n"
                "NAME chain\n"
                "VERTICES 1\n"
                "0: 1 0 0\n"
                "FACES 0\n"
                "SECTION: HIERARCHYDEF\n"
                "HIERARCHY NODES " + std::to_string(nodeCount) + "\n";
            for ( std::size_t nodeIndex = 0; nodeIndex < nodeCount; ++nodeIndex )
            {
                const std::int32_t parent = nodeIndex + 1u < nodeCount
                    ? static_cast<std::int32_t>(nodeIndex + 1u)
                    : -1;
                source += std::to_string(nodeIndex)
                    + ": 0x0 0x1 0 " + std::to_string(parent)
                    + " -1 -1 0 1 0 0 0 0 0 0 0 0 node"
                    + std::to_string(nodeIndex) + "\n";
            }
            return source;
        };

        constexpr std::size_t chainNodeCount = 8u;
        constexpr std::size_t exactTransformVisits =
            (chainNodeCount * (chainNodeCount + 1u)) / 2u;
        const std::string chain = makeSharedMeshChain(chainNodeCount);
        sed::assets::detail::Model3doBudgetLimitsForTests exactWorkLimits{};
        exactWorkLimits.decodedBytes = 1024u * 1024u;
        exactWorkLimits.items = 1000u;
        exactWorkLimits.transformVisits = exactTransformVisits;
        {
            ScopedModel3doBudgetLimitsForTests limits{ exactWorkLimits };
            const auto bytes = std::as_bytes(std::span(chain.data(), chain.size()));
            const sed::assets::Model3do model =
                sed::assets::loadFrom3DO(bytes);
            check(model.meshes.size() == chainNodeCount
                    && close(model.meshes[0].vertices[0].x, 9.0)
                    && close(model.meshes[chainNodeCount - 1u].vertices[0].x, 2.0),
                "3DO.transformBudget: shared one-vertex chain fits the exact work cap and preserves hierarchy coordinates");
        }

        sed::assets::detail::Model3doBudgetLimitsForTests oneVisitShortLimits{};
        oneVisitShortLimits.decodedBytes = 1024u * 1024u;
        oneVisitShortLimits.items = 1000u;
        oneVisitShortLimits.transformVisits = exactTransformVisits - 1u;
        {
            ScopedModel3doBudgetLimitsForTests limits{ oneVisitShortLimits };
            const auto bytes = std::as_bytes(std::span(chain.data(), chain.size()));
            checkThrowsWithMessage(
                [&bytes]()
                {
                    (void)sed::assets::loadFrom3DO(bytes);
                },
                "Cannot load 3DO: hierarchy transform work budget is exceeded.",
                "3DO.transformBudget: aggregate shared-mesh vertex visits are rejected before hierarchy copies and transforms");
        }

        sed::assets::detail::Model3doBudgetLimitsForTests rawModeLimits{};
        rawModeLimits.decodedBytes = 1024u * 1024u;
        rawModeLimits.items = 1000u;
        rawModeLimits.transformVisits = 0u;
        {
            ScopedModel3doBudgetLimitsForTests limits{ rawModeLimits };
            const auto bytes = std::as_bytes(std::span(chain.data(), chain.size()));
            sed::assets::Model3doLoadOptions rawOptions{};
            rawOptions.applyHierarchyTransforms = false;
            const sed::assets::Model3do model =
                sed::assets::loadFrom3DO(bytes, rawOptions);
            check(!model.hierarchyTransformsApplied
                    && model.meshes.size() == 1
                    && close(model.meshes[0].vertices[0].x, 1.0),
                "3DO.transformBudget: raw hierarchy import does not pay for transform work");
        }
    }

    {
        const auto makeNormalMeshHierarchy = [](
            const std::vector<std::int32_t>& parents,
            const double nodePositionX)
        {
            std::string source =
                "SECTION: HEADER\n"
                "3DO 2.1\n"
                "SECTION: GEOMETRYDEF\n"
                "GEOSETS 1\n"
                "GEOSET 0\n"
                "MESHES 1\n"
                "MESH 0\n"
                "NAME normal-budget\n"
                "VERTICES 1\n"
                "0: 1 0 0\n"
                "VERTEX NORMALS\n"
                "0: 0 1 0\n"
                "FACES 1\n"
                "0: 0 0 4 3 3 1 3 0,0 0,0 0,0\n"
                "FACE NORMALS\n"
                "0: 0 0 1\n"
                "SECTION: HIERARCHYDEF\n"
                "HIERARCHY NODES " + std::to_string(parents.size()) + "\n";
            for (std::size_t nodeIndex = 0;
                 nodeIndex < parents.size(); ++nodeIndex)
            {
                source += std::to_string(nodeIndex)
                    + ": 0x0 0x1 0 " + std::to_string(parents[nodeIndex])
                    + " -1 -1 0 " + std::to_string(nodePositionX)
                    + " 0 0 0 0 0 0 0 0 node"
                    + std::to_string(nodeIndex) + "\n";
            }
            return source;
        };

        const auto makeLimits = [](const std::size_t transformVisits)
        {
            sed::assets::detail::Model3doBudgetLimitsForTests limits{};
            limits.decodedBytes = 1024u * 1024u;
            limits.items = 1000u;
            limits.transformVisits = transformVisits;
            return limits;
        };

        constexpr std::string_view workDiagnostic =
            "Cannot load 3DO: hierarchy transform work budget is exceeded.";

        {
            const std::string source = makeNormalMeshHierarchy({ 0 }, 1.0);
            const auto bytes = std::as_bytes(std::span(source.data(), source.size()));
            ScopedModel3doBudgetLimitsForTests limits{ makeLimits(6u) };
            const sed::assets::Model3do model =
                sed::assets::loadFrom3DO(bytes);
            check(model.meshes.size() == 1u
                    && close(model.meshes[0].vertices[0].x, 3.0)
                    && close(model.meshes[0].vertexNormals[0].y, 1.0)
                    && close(model.meshes[0].faceNormals[0].z, 1.0),
                "3DO.transformBudget: self-cycle uses the existing node-count transform limit and includes both normal arrays");
        }
        {
            const std::string source = makeNormalMeshHierarchy({ 0 }, 1.0);
            const auto bytes = std::as_bytes(std::span(source.data(), source.size()));
            ScopedModel3doBudgetLimitsForTests limits{ makeLimits(5u) };
            checkThrowsWithMessage(
                [&bytes]()
                {
                    (void)sed::assets::loadFrom3DO(bytes);
                },
                workDiagnostic,
                "3DO.transformBudget: self-cycle is rejected one vector visit below its bounded work");
        }

        {
            // Nodes 1 and 2 form a cycle. Nodes 0 and 3 are incoming tails,
            // so every node reaches a cycle and receives the full hop cap.
            const std::string source =
                makeNormalMeshHierarchy({ 1, 2, 1, 0 }, 0.0);
            const auto bytes = std::as_bytes(std::span(source.data(), source.size()));
            ScopedModel3doBudgetLimitsForTests limits{ makeLimits(60u) };
            const sed::assets::Model3do model =
                sed::assets::loadFrom3DO(bytes);
            bool allInstancesPreserved = model.meshes.size() == 4u;
            for (const sed::assets::Model3doMesh& mesh : model.meshes)
            {
                allInstancesPreserved = allInstancesPreserved
                    && close(mesh.vertices[0].x, 1.0)
                    && close(mesh.vertexNormals[0].y, 1.0)
                    && close(mesh.faceNormals[0].z, 1.0);
            }
            check(allInstancesPreserved,
                "3DO.transformBudget: a multi-node cycle and both incoming tails fit the exact bounded-work cap");
        }
        {
            const std::string source =
                makeNormalMeshHierarchy({ 1, 2, 1, 0 }, 0.0);
            const auto bytes = std::as_bytes(std::span(source.data(), source.size()));
            ScopedModel3doBudgetLimitsForTests limits{ makeLimits(59u) };
            checkThrowsWithMessage(
                [&bytes]()
                {
                    (void)sed::assets::loadFrom3DO(bytes);
                },
                workDiagnostic,
                "3DO.transformBudget: cycle-reaching tails are included in aggregate work before copies");
        }

        {
            const std::string source = makeNormalMeshHierarchy({ 77 }, 1.0);
            const auto bytes = std::as_bytes(std::span(source.data(), source.size()));
            ScopedModel3doBudgetLimitsForTests limits{ makeLimits(3u) };
            const sed::assets::Model3do model =
                sed::assets::loadFrom3DO(bytes);
            check(model.meshes.size() == 1u
                    && close(model.meshes[0].vertices[0].x, 2.0)
                    && close(model.meshes[0].vertexNormals[0].y, 1.0)
                    && close(model.meshes[0].faceNormals[0].z, 1.0),
                "3DO.transformBudget: an out-of-range parent terminates the ancestor walk and counts vertex and normal arrays");
        }
        {
            const std::string source = makeNormalMeshHierarchy({ 77 }, 1.0);
            const auto bytes = std::as_bytes(std::span(source.data(), source.size()));
            ScopedModel3doBudgetLimitsForTests limits{ makeLimits(2u) };
            checkThrowsWithMessage(
                [&bytes]()
                {
                    (void)sed::assets::loadFrom3DO(bytes);
                },
                workDiagnostic,
                "3DO.transformBudget: an invalid-parent node exceeds a cap one vector visit below its three arrays");
        }

        {
            const std::string source = makeNormalMeshHierarchy({ 1, -1 }, 1.0);
            const auto bytes = std::as_bytes(std::span(source.data(), source.size()));
            ScopedModel3doBudgetLimitsForTests limits{ makeLimits(9u) };
            const sed::assets::Model3do model =
                sed::assets::loadFrom3DO(bytes);
            check(model.meshes.size() == 2u
                    && close(model.meshes[0].vertices[0].x, 3.0)
                    && close(model.meshes[1].vertices[0].x, 2.0)
                    && close(model.meshes[0].vertexNormals[0].y, 1.0)
                    && close(model.meshes[1].vertexNormals[0].y, 1.0)
                    && close(model.meshes[0].faceNormals[0].z, 1.0)
                    && close(model.meshes[1].faceNormals[0].z, 1.0),
                "3DO.transformBudget: a normal chain fits exact aggregate work and preserves coordinates and both normal arrays");
        }
        {
            const std::string source = makeNormalMeshHierarchy({ 1, -1 }, 1.0);
            const auto bytes = std::as_bytes(std::span(source.data(), source.size()));
            ScopedModel3doBudgetLimitsForTests limits{ makeLimits(8u) };
            checkThrowsWithMessage(
                [&bytes]()
                {
                    (void)sed::assets::loadFrom3DO(bytes);
                },
                workDiagnostic,
                "3DO.transformBudget: a valid normal chain is rejected one aggregate vector visit below its work");
        }
    }
}

} // namespace sed::tests
