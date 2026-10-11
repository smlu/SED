#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <istream>
#include <span>
#include <string>
#include <vector>

#include "sed/color.hpp"
#include "sed/geometry.hpp"

namespace sed::assets
{

struct Model3doVertex
{
    double x{};
    double y{};
    double z{};
    ColorF color{ ColorWhite };
};

struct Model3doTexVertex
{
    // Preserve the model's wire units: 3DO 2.1 uses texture pixels and 2.3
    // uses normalized coordinates. Consumers must inspect Model3do::version
    // before converting these values for a Level or renderer.
    float u{};
    float v{};
};

struct Model3doFace
{
    std::int32_t material{};
    std::uint32_t faceFlags{};
    std::int32_t geo{};
    std::int32_t light{};
    std::int32_t tex{};
    ColorF extraLight{ ColorWhite };

    std::vector<std::int32_t> vertices;
    std::vector<std::int32_t> texVertices;
};

struct Model3doMesh
{
    std::string name;
    // The 3DO MESH RADIUS value consumed by MotS tree collision. `radius`
    // remains the conservative renderer/bounds radius, which can expand after
    // hierarchy transforms are applied.
    double authoredRadius{};
    double radius{};
    std::int32_t geo{};
    std::int32_t light{};
    std::int32_t tex{};

    std::vector<Model3doVertex> vertices;
    std::vector<Model3doTexVertex> texVertices;
    std::vector<TVector> vertexNormals;
    std::vector<Model3doFace> faces;
    std::vector<TVector> faceNormals;
};

struct Model3doNode
{
    std::int32_t id{};
    // MotS uses hierarchy flag bit 0x2 when applying inherited mesh radii
    // during rendering; keep it distinct from the node type field.
    std::uint32_t type{};
    std::uint32_t flags{};
    std::int32_t mesh{-1};
    std::int32_t parent{-1};

    TVector position{};
    TVector rotation{};
    TVector pivot{};

    std::string name;
};

struct Model3do
{
    double version{};
    double radius{};
    // Declared GEOSETS count, retained even though one load parses only one geoset.
    std::size_t geosetCount{};
    TVector insertOffset{};

    std::vector<std::string> materials;
    std::vector<Model3doMesh> meshes;
    // Archive lighting records use authored/source mesh ordinals.  The normal
    // loader may append transformed instances when hierarchy nodes share one
    // source mesh, so this parallel table maps every published render mesh
    // back to the source ordinal consumed by ARCHLIGHTING records.
    std::vector<std::int32_t> meshSourceIndices;
    std::vector<Model3doNode> nodes;

    // Default loads preserve the historical renderer-facing contract and
    // publish hierarchy-transformed mesh vertices. Importers that need to
    // instantiate one shared mesh for multiple nodes request raw meshes and
    // apply each node transform to its own copy.
    bool hierarchyTransformsApplied{ true };
};

struct Model3doLoadOptions
{
    std::int32_t lod{ 0 };
    bool applyHierarchyTransforms{ true };
};

// Match the classic Sith renderer's geoset branches using Thing-origin depth
// along the camera forward axis. The caller applies the MotS inverse zoom scale
// to that depth. Equal thresholds choose the farther geoset; header slot 4 is
// unused. Invalid depth or a required invalid threshold safely selects geoset 0.
std::size_t selectModel3doLodIndex(
    std::size_t geosetCount,
    const std::array<double, 4>& lodDistances,
    double cameraForwardDepth) noexcept;

Model3do loadFrom3DO(std::istream& in, const Model3doLoadOptions& options = {});
Model3do loadFrom3DO(std::span<const std::byte> bytes, const Model3doLoadOptions& options = {});
Model3do loadFrom3DO(const std::filesystem::path& path, const Model3doLoadOptions& options = {});

} // namespace sed::assets
