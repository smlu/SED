#include "test_common.hpp"

#include "render/overlays/thing_shape_overlay_utils.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <limits>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace sed::app_tests
{

void runThingShapeOverlayTests()
{
    using namespace render::thing_overlay;

    Level level{};
    level.kind = ProjectType::JKDF2;
    ThingTemplate templateThing{};
    templateThing.name = "overlay_test";

    TplValue collide{};
    collide.name = "collide";
    collide.type = ValueType::integer;
    collide.intValue = 1;
    TplValue size{};
    size.name = "size";
    size.type = ValueType::floating;
    size.floatValue = 2.5;
    templateThing.values = { collide, size };
    level.templates.push_back(templateThing);

    Thing thing{};
    thing.templateName = "overlay_test";
    const CollisionDescription inheritedSphere =
        resolveCollisionDescription(level, nullptr, thing);
    check(inheritedSphere.shape == CollisionShape::sphere
            && inheritedSphere.hasRadius
            && inheritedSphere.radius == 2.5,
        "Thing collision overlay resolves inherited sphere mode and size");

    level.templates[0].values[1].floatValue = 3.5;
    const CollisionDescription resizedInheritedSphere =
        resolveCollisionDescription(level, nullptr, thing);
    check(resizedInheritedSphere.shape == CollisionShape::sphere
            && resizedInheritedSphere.hasRadius
            && resizedInheritedSphere.radius == 3.5
            && !sameCollisionDescription(inheritedSphere, resizedInheritedSphere)
            && canUpdateCollisionSphereInPlace(inheritedSphere, resizedInheritedSphere),
        "inherited size changes invalidate a cached sphere radius without changing its line topology");

    TplValue localSize{};
    localSize.name = "size";
    localSize.type = ValueType::floating;
    localSize.floatValue = 4.25;
    thing.values.push_back(localSize);
    const CollisionDescription localSphere =
        resolveCollisionDescription(level, nullptr, thing);
    check(localSphere.shape == CollisionShape::sphere
            && localSphere.hasRadius
            && localSphere.radius == 4.25
            && !sameCollisionDescription(resizedInheritedSphere, localSphere)
            && canUpdateCollisionSphereInPlace(resizedInheritedSphere, localSphere),
        "local size overrides invalidate the inherited sphere cache with an in-place-compatible update");

    level.templates[0].values[1].floatValue = 5.0;
    check(sameCollisionDescription(
                localSphere, resolveCollisionDescription(level, nullptr, thing)),
        "template size changes do not invalidate the overlay cache when a local size overrides them");
    thing.values[0].floatValue = 4.75;
    const CollisionDescription resizedLocalSphere =
        resolveCollisionDescription(level, nullptr, thing);
    check(resizedLocalSphere.radius == 4.75
            && !sameCollisionDescription(localSphere, resizedLocalSphere)
            && canUpdateCollisionSphereInPlace(localSphere, resizedLocalSphere),
        "editing a local size updates the cached sphere radius independently of its inherited value");
    thing.values.clear();
    level.templates[0].values[1].floatValue = 2.5;

    TplValue localCollide{};
    localCollide.name = "collide";
    localCollide.type = ValueType::integer;
    localCollide.intValue = 3;
    thing.values.push_back(localCollide);
    const CollisionDescription localFace =
        resolveCollisionDescription(level, nullptr, thing);
    check(localFace.shape == CollisionShape::face && !localFace.hasRadius
            && !canUpdateCollisionSphereInPlace(resizedLocalSphere, localFace),
        "Thing collision mode changes invalidate sphere topology for the Things-only rebuild path");

    thing.values.clear();
    level.templates[0].values[0].intValue = 0;
    check(resolveCollisionDescription(level, nullptr, thing).shape == CollisionShape::none,
        "collide zero disables collision-shape overlay");

    level.kind = ProjectType::MOTS;
    level.templates[0].values[0].intValue = 2;
    TplValue treeSize{};
    treeSize.name = "treesize";
    treeSize.type = ValueType::floating;
    treeSize.floatValue = 3.25;
    level.templates[0].values.push_back(treeSize);
    const CollisionDescription tree = resolveCollisionDescription(level, nullptr, thing);
    check(tree.shape == CollisionShape::sphereTree && !tree.hasRadius
            && tree.broadPhaseRadius && *tree.broadPhaseRadius == 3.25,
        "MotS collide=2 keeps per-mesh radius separate from its broad-phase treesize");
    level.templates[0].values[2].floatValue = 6.5;
    const CollisionDescription resizedTree = resolveCollisionDescription(level, nullptr, thing);
    check(resizedTree.shape == CollisionShape::sphereTree
            && !resizedTree.hasRadius
            && resizedTree.broadPhaseRadius && *resizedTree.broadPhaseRadius == 6.5
            && !sameCollisionDescription(tree, resizedTree),
        "MotS treesize updates only the separately cached broad-phase guide radius");
    level.templates[0].values[2].floatValue = 3.25;
    level.kind = ProjectType::JKDF2;
    check(resolveCollisionDescription(level, nullptr, thing).shape == CollisionShape::none,
        "sphere-tree collision mode is not invented for JKDF2");
    level.kind = ProjectType::MOTS;
    level.templates[0].values[0].intValue = 2;
    level.templates[0].values[2].floatValue = -1.0;
    const CollisionDescription invalidTreeSize = resolveCollisionDescription(level, nullptr, thing);
    check(invalidTreeSize.shape == CollisionShape::sphereTree
            && !invalidTreeSize.broadPhaseRadius,
        "MotS sphere-tree overlay does not invent a shape from size when treesize is invalid");
    level.templates[0].values[1].floatValue = 0.0;
    check(resolveCollisionDescription(level, nullptr, thing).shape == CollisionShape::sphereTree,
        "MotS collide=2 keeps its model-dependent tree shape even when no broad-phase treesize is available");
    level.templates[0].values[0].intValue = 1;
    level.templates[0].values[1].floatValue = -2.0;
    check(resolveCollisionDescription(level, nullptr, thing).shape == CollisionShape::none,
        "sphere collision rejects non-positive effective size");
    level.templates[0].values[0].intValue = 4;
    check(resolveCollisionDescription(level, nullptr, thing).shape == CollisionShape::none,
        "unknown collision mode does not invent a shape");

    assets::Model3do sphereTreeModel{};
    assets::Model3doMesh rootMesh{};
    rootMesh.authoredRadius = 2.0;
    assets::Model3doMesh childMesh{};
    childMesh.authoredRadius = 4.0;
    sphereTreeModel.meshes = { rootMesh, childMesh };
    assets::Model3doNode rootNode{};
    rootNode.mesh = 0;
    rootNode.parent = -1;
    rootNode.position = TVector{ 10.0, 0.0, 0.0 };
    rootNode.rotation = TVector{ 0.0, 0.0, 90.0 };
    rootNode.pivot = TVector{ 0.0, 1.0, 0.0 };
    assets::Model3doNode childNode{};
    childNode.mesh = 1;
    childNode.parent = 0;
    childNode.position = TVector{ 0.0, 2.0, 0.0 };
    childNode.pivot = TVector{ 0.0, 0.0, 2.0 };
    sphereTreeModel.nodes = { rootNode, childNode };

    std::vector<TVector> sphereTreePoints{};
    const TMatrix43 thingTransform = TMatrix43::Create(
        TVector{ 100.0, 200.0, 300.0 }, TVector::zero);
    const std::size_t appendedSphereCount = appendSphereTreeWirePoints(
        sphereTreePoints, sphereTreeModel, thingTransform);
    check(appendedSphereCount == 2 && sphereTreePoints.size() == 288,
        "sphere-tree wire helper emits one three-ring sphere for each valid hierarchy mesh node");
    check(sphereTreePoints.size() == 288
            && std::abs(sphereTreePoints[0].x - 111.5) < 1.0e-12
            && std::abs(sphereTreePoints[0].y - 201.0) < 1.0e-12
            && std::abs(sphereTreePoints[0].z - 300.0) < 1.0e-12
            && std::abs(sphereTreePoints[144].x - 115.0) < 1.0e-12
            && std::abs(sphereTreePoints[144].y - 202.0) < 1.0e-12
            && std::abs(sphereTreePoints[144].z - 300.0) < 1.0e-12,
        "sphere-tree centers apply each node pose, nonzero node and parent pivots, parent rotation, and the default 0.75 mesh-radius factor");

    assets::Model3do flaggedSphereTreeModel = sphereTreeModel;
    flaggedSphereTreeModel.nodes[1].flags = 0x2u;
    std::vector<TVector> flaggedSphereTreePoints{};
    const std::size_t flaggedSphereCount = appendSphereTreeWirePoints(
        flaggedSphereTreePoints, flaggedSphereTreeModel, thingTransform);
    check(flaggedSphereCount == 2
            && flaggedSphereTreePoints.size() == 288
            && std::abs(flaggedSphereTreePoints[144].x - 113.5) < 1.0e-12,
        "MotS flag 0x2 child sphere inherits the nearest unflagged ancestor mesh radius");

    assets::Model3do flaggedChainModel{};
    flaggedChainModel.meshes = { rootMesh, childMesh, childMesh };
    assets::Model3doNode flaggedChainRoot{};
    flaggedChainRoot.mesh = 0;
    flaggedChainRoot.parent = -1;
    assets::Model3doNode flaggedChainMiddle{};
    flaggedChainMiddle.mesh = 1;
    flaggedChainMiddle.parent = 0;
    flaggedChainMiddle.flags = 0x2u;
    assets::Model3doNode flaggedChainLeaf{};
    flaggedChainLeaf.mesh = 2;
    flaggedChainLeaf.parent = 1;
    flaggedChainLeaf.flags = 0x2u;
    flaggedChainModel.nodes = { flaggedChainRoot, flaggedChainMiddle, flaggedChainLeaf };
    std::vector<TVector> flaggedChainPoints{};
    check(appendSphereTreeWirePoints(
                flaggedChainPoints, flaggedChainModel, TMatrix43::identity()) == 3
            && flaggedChainPoints.size() == 432
            && std::abs(flaggedChainPoints[144].x - 1.5) < 1.0e-12
            && std::abs(flaggedChainPoints[288].x - 1.5) < 1.0e-12,
        "MotS flagged descendants walk through flagged parents to the first unflagged ancestor");

    assets::Model3do flaggedRootModel{};
    flaggedRootModel.meshes = { childMesh };
    assets::Model3doNode flaggedRootNode{};
    flaggedRootNode.mesh = 0;
    flaggedRootNode.parent = -1;
    flaggedRootNode.flags = 0x2u;
    flaggedRootModel.nodes.push_back(flaggedRootNode);
    std::vector<TVector> flaggedRootPoints{};
    check(appendSphereTreeWirePoints(
                flaggedRootPoints, flaggedRootModel, TMatrix43::identity()) == 1
            && flaggedRootPoints.size() == 144
            && std::abs(flaggedRootPoints[0].x - 3.0) < 1.0e-12,
        "MotS flagged root without an ancestor keeps its own authored mesh radius");

    assets::Model3do missingAncestorMeshModel{};
    missingAncestorMeshModel.meshes = { childMesh };
    assets::Model3doNode noMeshAncestor{};
    noMeshAncestor.mesh = -1;
    noMeshAncestor.parent = -1;
    assets::Model3doNode flaggedChild{};
    flaggedChild.mesh = 0;
    flaggedChild.parent = 0;
    flaggedChild.flags = 0x2u;
    missingAncestorMeshModel.nodes = { noMeshAncestor, flaggedChild };
    std::vector<TVector> missingAncestorMeshPoints{};
    check(appendSphereTreeWirePoints(
                missingAncestorMeshPoints, missingAncestorMeshModel, TMatrix43::identity()) == 1
            && missingAncestorMeshPoints.size() == 144
            && std::abs(missingAncestorMeshPoints[0].x - 3.0) < 1.0e-12,
        "MotS flagged child falls back to its own authored radius when the unflagged ancestor has no mesh");

    std::vector<TVector> unitScaleSphereTreePoints{};
    const std::size_t unitScaleSphereCount = appendSphereTreeWirePoints(
        unitScaleSphereTreePoints, sphereTreeModel, thingTransform, 1.0);
    check(unitScaleSphereCount == 2
            && unitScaleSphereTreePoints.size() == 288
            && std::abs(unitScaleSphereTreePoints[0].x - 112.0) < 1.0e-12
            && std::abs(unitScaleSphereTreePoints[144].x - 116.0) < 1.0e-12,
        "custom MotS collision-tree preview scale changes each authored mesh radius without changing node centers");

    std::vector<TVector> invalidScaleSphereTreePoints{};
    check(appendSphereTreeWirePoints(
                invalidScaleSphereTreePoints,
                sphereTreeModel,
                thingTransform,
                std::numeric_limits<double>::infinity()) == 0
            && invalidScaleSphereTreePoints.empty(),
        "MotS collision-tree preview rejects non-finite mesh-radius scale factors");
    check(appendSphereTreeWirePoints(
                invalidScaleSphereTreePoints, sphereTreeModel, thingTransform, 0.0) == 0
            && invalidScaleSphereTreePoints.empty(),
        "MotS collision-tree preview rejects a zero radius factor");

    assets::Model3do missingSphereTreeModel{};
    std::vector<TVector> missingSpherePoints{};
    check(appendSphereTreeWirePoints(
                missingSpherePoints, missingSphereTreeModel, TMatrix43::identity()) == 0
            && missingSpherePoints.empty(),
        "sphere-tree wire helper cleanly returns no geometry when the 3DO resource or hierarchy is missing");

    assets::Model3do malformedSphereTree{};
    malformedSphereTree.meshes.push_back(rootMesh);
    assets::Model3doNode malformedMeshNode{};
    malformedMeshNode.mesh = 4;
    malformedMeshNode.parent = -1;
    malformedSphereTree.nodes.push_back(malformedMeshNode);
    std::vector<TVector> malformedSpherePoints{};
    check(appendSphereTreeWirePoints(
                malformedSpherePoints, malformedSphereTree, TMatrix43::identity()) == 0
            && malformedSpherePoints.empty(),
        "sphere-tree wire helper skips nodes with out-of-range mesh indices");

    assets::Model3do malformedParentTree{};
    malformedParentTree.meshes.push_back(rootMesh);
    assets::Model3doNode malformedParentNode{};
    malformedParentNode.mesh = 0;
    malformedParentNode.parent = 8;
    malformedParentTree.nodes.push_back(malformedParentNode);
    std::vector<TVector> malformedParentPoints{};
    check(appendSphereTreeWirePoints(
                malformedParentPoints, malformedParentTree, TMatrix43::identity()) == 0
            && malformedParentPoints.empty(),
        "sphere-tree wire helper skips nodes with invalid hierarchy parents");

    assets::Model3do cyclicSphereTree{};
    cyclicSphereTree.meshes.push_back(rootMesh);
    assets::Model3doNode firstCycleNode{};
    firstCycleNode.mesh = 0;
    firstCycleNode.parent = 1;
    assets::Model3doNode secondCycleNode{};
    secondCycleNode.mesh = 0;
    secondCycleNode.parent = 0;
    cyclicSphereTree.nodes = { firstCycleNode, secondCycleNode };
    std::vector<TVector> cyclicSpherePoints{};
    check(appendSphereTreeWirePoints(
                cyclicSpherePoints, cyclicSphereTree, TMatrix43::identity()) == 0
            && cyclicSpherePoints.empty(),
        "sphere-tree wire helper rejects cyclic hierarchy paths without partial output");

    Level nonMotsTreeLevel{};
    nonMotsTreeLevel.kind = ProjectType::IJIM;
    ThingTemplate nonMotsTreeTemplate{};
    nonMotsTreeTemplate.name = "tree_test";
    TplValue nonMotsCollide{};
    nonMotsCollide.name = "collide";
    nonMotsCollide.type = ValueType::integer;
    nonMotsCollide.intValue = 2;
    nonMotsTreeTemplate.values.push_back(nonMotsCollide);
    nonMotsTreeLevel.templates.push_back(nonMotsTreeTemplate);
    Thing nonMotsTreeThing{};
    nonMotsTreeThing.templateName = "tree_test";
    check(resolveCollisionDescription(nonMotsTreeLevel, nullptr, nonMotsTreeThing).shape
                == CollisionShape::none,
        "sphere-tree collision mode remains MotS-only for IJIM");

    const std::vector<TVector> sphere = makeSphereWirePoints(2.0, 12);
    check(sphere.size() == 72,
        "sphere wire helper emits three closed 12-segment rings");
    const std::vector<TVector> cachedSphere = makeSphereWirePoints(3.5);
    check(cachedSphere.size() == 144
            && cachedSphere[0].x == 3.5 && cachedSphere[0].y == 0.0 && cachedSphere[0].z == 0.0
            && std::abs(cachedSphere[1].x - 3.380740392011739) < 1.0e-12
            && std::abs(cachedSphere[1].y - 0.9058666578588226) < 1.0e-12
            && cachedSphere[1].z == 0.0,
        "default collision sphere radius changes preserve the exact 144-vertex line cache topology and fixed endpoint vector");
    const std::vector<TVector> sphereVector = makeSphereWirePoints(2.0, 4);
    const auto close = [](const double actual, const double expected)
    {
        return std::abs(actual - expected) < 1.0e-12;
    };
    check(sphereVector.size() == 24
            && close(sphereVector[0].x, 2.0)
            && close(sphereVector[0].y, 0.0)
            && close(sphereVector[0].z, 0.0)
            && close(sphereVector[1].x, 0.0)
            && close(sphereVector[1].y, 2.0)
            && close(sphereVector[1].z, 0.0)
            && close(sphereVector[8].x, 2.0)
            && close(sphereVector[8].z, 0.0)
            && close(sphereVector[9].x, 0.0)
            && close(sphereVector[9].z, 2.0)
            && close(sphereVector[16].y, 2.0)
            && close(sphereVector[17].z, 2.0),
        "sphere wire helper matches fixed XY, XZ, and YZ endpoint vectors");
    check(std::all_of(sphere.begin(), sphere.end(), [](const TVector& point)
        {
            return std::isfinite(point.x) && std::isfinite(point.y) && std::isfinite(point.z);
        }),
        "sphere wire helper returns only finite endpoints");
    check(makeSphereWirePoints(0.0).empty()
            && makeSphereWirePoints(std::numeric_limits<double>::infinity()).empty()
            && makeSphereWirePoints(2.0, 2).empty(),
        "sphere wire helper rejects zero, non-finite, and undersampled radii");

    const std::vector<TVector> validSegments{
        TVector{ 1.0, 2.0, 3.0 }, TVector{ 4.0, 5.0, 6.0 },
        TVector{ 7.0, 8.0, 9.0 }, TVector{ 10.0, 11.0, 12.0 },
    };
    const std::vector<TVector> finiteSegments = finiteWireSegments(validSegments);
    check(finiteSegments.size() == 4
            && finiteSegments[0].x == 1.0 && finiteSegments[0].y == 2.0 && finiteSegments[0].z == 3.0
            && finiteSegments[1].x == 4.0 && finiteSegments[1].y == 5.0 && finiteSegments[1].z == 6.0
            && finiteSegments[2].x == 7.0 && finiteSegments[2].y == 8.0 && finiteSegments[2].z == 9.0
            && finiteSegments[3].x == 10.0 && finiteSegments[3].y == 11.0 && finiteSegments[3].z == 12.0,
        "finite wire helper preserves exact valid endpoint pairs");
    const std::vector<TVector> brokenSegments{
        TVector{ std::numeric_limits<double>::quiet_NaN(), 1.0, 2.0 }, TVector{ 3.0, 4.0, 5.0 },
        TVector{ 6.0, 7.0, 8.0 }, TVector{ 9.0, 10.0, 11.0 },
    };
    const std::vector<TVector> filteredSegments = finiteWireSegments(brokenSegments);
    check(filteredSegments.size() == 2
            && filteredSegments[0].x == 6.0 && filteredSegments[0].y == 7.0 && filteredSegments[0].z == 8.0
            && filteredSegments[1].x == 9.0 && filteredSegments[1].y == 10.0 && filteredSegments[1].z == 11.0,
        "finite wire helper drops an entire invalid segment without mispairing later endpoints");
    check(finiteWireSegments({ TVector{ 1.0, 2.0, 3.0 } }).empty()
            && finiteWireSegments({
                   TVector{ 0.0, 0.0, 0.0 },
                   TVector{ std::numeric_limits<double>::max(), 0.0, 0.0 },
               }).empty(),
        "finite wire helper rejects odd endpoint counts and coordinates outside float range");

    const std::optional<double> spriteRadius = spriteBillboardConservativeRadius(
        4.0, 6.0, std::array<double, 3>{ 1.0, 2.0, 2.0 });
    check(spriteRadius && close(*spriteRadius, 3.0 + std::sqrt(13.0)),
        "sprite conservative radius combines offset length with quad half-diagonal");
    check(!spriteBillboardConservativeRadius(0.0, 1.0, std::array<double, 3>{})
            && !spriteBillboardConservativeRadius(
                1.0, 1.0, std::array<double, 3>{ 0.0, std::numeric_limits<double>::infinity(), 0.0 }),
        "sprite conservative radius rejects zero dimensions and non-finite offsets");
    const std::array<TVector, 4> fixedType2SpriteCorners{{
        TVector{ 14.0, 23.0, 27.0 },
        TVector{ 6.0, 23.0, 27.0 },
        TVector{ 6.0, 23.0, 33.0 },
        TVector{ 14.0, 23.0, 33.0 },
    }};
    const std::optional<Bounds3d> fixedSpriteBounds =
        spriteQuadWorldBounds(fixedType2SpriteCorners);
    check(fixedSpriteBounds
            && fixedSpriteBounds->min.x == 6.0
            && fixedSpriteBounds->min.y == 23.0
            && fixedSpriteBounds->min.z == 27.0
            && fixedSpriteBounds->max.x == 14.0
            && fixedSpriteBounds->max.y == 23.0
            && fixedSpriteBounds->max.z == 33.0,
        "fixed-frame sprite bounds tightly enclose the rendered quad and its world offset");
    const auto fixedSpriteBoundLines = fixedSpriteBounds
        ? makeBoundsWirePoints(*fixedSpriteBounds, TMatrix43::identity())
        : std::array<TVector, 24>{};
    check(fixedSpriteBoundLines.size() == 24
            && std::all_of(fixedSpriteBoundLines.begin(), fixedSpriteBoundLines.end(),
                [](const TVector& point)
                {
                    return point.x >= 6.0 && point.x <= 14.0
                        && point.y == 23.0
                        && point.z >= 27.0 && point.z <= 33.0;
                }),
        "fixed-frame sprite visual wires follow the card bounds without a surrounding billboard cube");
    const std::array<TVector, 4> invalidSpriteCorners{{
        TVector{ 0.0, 0.0, 0.0 },
        TVector{ 1.0, 0.0, 0.0 },
        TVector{ 1.0, std::numeric_limits<double>::infinity(), 1.0 },
        TVector{ 0.0, 0.0, 1.0 },
    }};
    check(!spriteQuadWorldBounds(invalidSpriteCorners),
        "fixed-frame sprite bounds reject incomplete non-finite quads");

    assets::Model3do model{};
    assets::Model3doMesh mesh{};
    mesh.vertices = {
        assets::Model3doVertex{ -1.0, -2.0, -3.0 },
        assets::Model3doVertex{ 4.0, 5.0, 6.0 },
        assets::Model3doVertex{ 1.0, 2.0, 3.0 },
        assets::Model3doVertex{ std::numeric_limits<double>::quiet_NaN(), 0.0, 0.0 },
    };
    assets::Model3doFace firstFace{};
    firstFace.vertices = { 0, 1, 2 };
    assets::Model3doFace duplicateFace{};
    duplicateFace.vertices = { 2, 1, 0 };
    assets::Model3doFace malformedFace{};
    malformedFace.vertices = { 0, 9, 2 };
    mesh.faces = { firstFace, duplicateFace, malformedFace };
    model.meshes.push_back(mesh);

    const std::optional<Bounds3d> bounds = modelLocalVertexBounds(model);
    check(bounds
            && bounds->min.x == -1.0 && bounds->min.y == -2.0 && bounds->min.z == -3.0
            && bounds->max.x == 4.0 && bounds->max.y == 5.0 && bounds->max.z == 6.0,
        "visual model bounds use finite local vertices across meshes");
    const TMatrix43 identity = TMatrix43::identity();
    const auto boundsLines = makeBoundsWirePoints(*bounds, identity);
    check(boundsLines.size() == 24
            && boundsLines[0].x == bounds->min.x
            && boundsLines[0].y == bounds->min.y
            && boundsLines[0].z == bounds->min.z,
        "visual bounds helper emits the transformed twelve-edge box");

    const TMatrix43 translatedAndRotated = TMatrix43::Create(
        TVector{ 10.0, 20.0, 30.0 },
        TVector{ 0.0, 90.0, 0.0 });
    const auto transformedBoundsLines = makeBoundsWirePoints(*bounds, translatedAndRotated);
    check(close(transformedBoundsLines[2].x, 12.0)
            && close(transformedBoundsLines[2].y, 24.0)
            && close(transformedBoundsLines[2].z, 27.0)
            && close(transformedBoundsLines[3].x, 5.0)
            && close(transformedBoundsLines[3].y, 24.0)
            && close(transformedBoundsLines[3].z, 27.0),
        "visual bounds follow fixed yaw and translation corner vectors");

    std::vector<TVector> faceLines{};
    appendModelFaceWirePoints(faceLines, model, identity);
    check(faceLines.size() == 6
            && faceLines[0].x == -1.0 && faceLines[0].y == -2.0 && faceLines[0].z == -3.0
            && faceLines[1].x == 4.0 && faceLines[1].y == 5.0 && faceLines[1].z == 6.0
            && faceLines[2].x == 4.0 && faceLines[2].y == 5.0 && faceLines[2].z == 6.0
            && faceLines[3].x == 1.0 && faceLines[3].y == 2.0 && faceLines[3].z == 3.0
            && faceLines[4].x == 1.0 && faceLines[4].y == 2.0 && faceLines[4].z == 3.0
            && faceLines[5].x == -1.0 && faceLines[5].y == -2.0 && faceLines[5].z == -3.0,
        "face-collision outline emits exact deduplicated edge endpoint vectors");
    assets::Model3do emptyModel{};
    check(!modelLocalVertexBounds(emptyModel),
        "visual bounds are absent when a model has no vertices");

    const ThingOverlayGeometryPlan noOverlayPlan = thingOverlayGeometryPlan(false, false);
    const ThingOverlayGeometryPlan collisionOverlayPlan = thingOverlayGeometryPlan(true, false);
    const ThingOverlayGeometryPlan visualBoundsPlan = thingOverlayGeometryPlan(false, true);
    const ThingOverlayGeometryPlan bothOverlaysPlan = thingOverlayGeometryPlan(true, true);
    check(!noOverlayPlan.collisionShapes && !noOverlayPlan.visualBounds
            && collisionOverlayPlan.collisionShapes && collisionOverlayPlan.visualBounds
            && visualBoundsPlan.collisionShapes && visualBoundsPlan.visualBounds
            && bothOverlaysPlan.collisionShapes && bothOverlaysPlan.visualBounds,
        "hidden overlays stay unbuilt, while either visible overlay toggle prepares both cached geometry classes");

    check(shouldDrawThingOverlay(true, true, true)
            && !shouldDrawThingOverlay(false, true, true)
            && !shouldDrawThingOverlay(true, false, true)
            && !shouldDrawThingOverlay(true, true, false),
        "Thing overlays obey render, layer, and effective Thing visibility gates");
    check(!shouldEmphasizeThingOverlay(false, false)
            && !shouldEmphasizeThingOverlay(false, true)
            && !shouldEmphasizeThingOverlay(true, false)
            && shouldEmphasizeThingOverlay(true, true),
        "selected Thing overlay emphasis requires both its visibility toggle and selected owner ranges");

    const std::unordered_set<std::int32_t> selectedThingIndices{ 1, 2, 99, -1 };
    const std::vector<std::uint32_t> selectedThingPickIds{
        11u,
        22u,
        0u,
        static_cast<std::uint32_t>(std::numeric_limits<int>::max()) + 1u,
    };
    const std::unordered_map<int, std::vector<render::DrawRange>> overlayRangesByPickId{
        { 11, { render::DrawRange{ 4, 2 }, render::DrawRange{ 6, 4 } } },
        { 22, { render::DrawRange{ 10, 6 }, render::DrawRange{ -1, 2 }, render::DrawRange{ 18, 0 } } },
        { 12, { render::DrawRange{ 20, 2 } } },
    };
    const std::vector<render::DrawRange> emphasizedThingRanges = selectedThingOverlayRanges(
        selectedThingIndices,
        0,
        selectedThingPickIds,
        overlayRangesByPickId,
        100);
    check(emphasizedThingRanges.size() == 3
            && emphasizedThingRanges[0].first == 104
            && emphasizedThingRanges[0].count == 2
            && emphasizedThingRanges[1].first == 106
            && emphasizedThingRanges[1].count == 4
            && emphasizedThingRanges[2].first == 110
            && emphasizedThingRanges[2].count == 6,
        "selected overlay ranges include the active Thing, all selected owners in stable order, and only valid ranges shifted into the composed VBO");

    const std::unordered_set<std::int32_t> selectionAlreadyContainsActive{ 0 };
    const std::vector<render::DrawRange> noDuplicateActiveRanges = selectedThingOverlayRanges(
        selectionAlreadyContainsActive,
        0,
        selectedThingPickIds,
        overlayRangesByPickId,
        0);
    check(noDuplicateActiveRanges.size() == 2
            && noDuplicateActiveRanges[0].first == 4
            && noDuplicateActiveRanges[0].count == 2
            && noDuplicateActiveRanges[1].first == 6
            && noDuplicateActiveRanges[1].count == 4,
        "selected overlay ranges do not draw the active Thing twice when it is already in the multi-selection");

    const std::unordered_set<std::int32_t> malformedOwner{ 3 };
    const std::unordered_set<std::int32_t> overflowingOwner{ 0 };
    const std::vector<std::uint32_t> overflowingPickIds{ 12u };
    const std::unordered_map<int, std::vector<render::DrawRange>> overflowingRanges{
        { 12, { render::DrawRange{ std::numeric_limits<int>::max(), 2 } } },
    };
    check(selectedThingOverlayRanges(
                malformedOwner,
                -1,
                selectedThingPickIds,
                overlayRangesByPickId,
                0).empty()
            && selectedThingOverlayRanges(
                {},
                99,
                selectedThingPickIds,
                overlayRangesByPickId,
                0).empty()
            && selectedThingOverlayRanges(
                selectionAlreadyContainsActive,
                0,
                selectedThingPickIds,
                overlayRangesByPickId,
                -1).empty()
            && selectedThingOverlayRanges(
                overflowingOwner,
                -1,
                overflowingPickIds,
                overflowingRanges,
                1).empty(),
        "selected overlay range collection rejects invalid pick IDs, invalid indices, absent owners, negative offsets, and composed-range overflow");
}

} // namespace sed::app_tests
