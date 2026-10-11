#include "test_common.hpp"

#include "sed/assets/sed.hpp"
#include "sed/level/build.hpp"
#include "sed/level.hpp"
#include "sed/level/transform.hpp"

#include <limits>

namespace sed::tests
{

void runLevelTests()
{
    check(static_cast<std::uint32_t>(SECF_3DO) == 0x80000000u,
        "Level flags: SECF_3DO preserves the unsigned high-bit mask");

    {
        sed::Level level{};
        check(level.uvCoordinateSpace == sed::UvCoordinateSpace::Normalized,
            "Level defaults to normalized UV coordinates");

        level.uvCoordinateSpace = sed::UvCoordinateSpace::TexturePixels;
        level.clear();
        check(level.curId == 1, "Level.clear.curId");
        check(level.sectors.empty(), "Level.clear.sectors");
        check(level.things.empty(), "Level.clear.things");
        check(level.lights.empty(), "Level.clear.lights");
        check(level.uvCoordinateSpace == sed::UvCoordinateSpace::Normalized,
            "Level.clear restores normalized UV coordinates");
    }

    {
        sed::Level level{};
        level.clear();

        const std::int32_t first = level.newId();
        const std::int32_t second = level.newId();

        check(first == 1, "Level.newId.first");
        check(second == 2, "Level.newId.second");
        check(level.curId == 3, "Level.newId.curId");
    }

    {
        constexpr std::int32_t maximumId =
            std::numeric_limits<std::int32_t>::max();
        sed::Level level{};
        level.curId = maximumId - 1;
        check(level.canAllocateIds(0)
                && level.canAllocateIds(1)
                && !level.canAllocateIds(2),
            "Level ID capacity counts INT_MAX-1 as the final allocatable identity");

        const std::int32_t lastId = level.newId();
        check(lastId == maximumId - 1 && level.curId == maximumId
                && !level.canAllocateIds(1),
            "Level.newId admits the final ID and leaves INT_MAX as exhausted sentinel");

        bool exhausted = false;
        try
        {
            (void)level.newId();
        }
        catch (const sed::IdExhaustionError&)
        {
            exhausted = true;
        }
        check(exhausted && level.curId == maximumId,
            "Level.newId repeatedly rejects exhaustion without advancing the sentinel");
    }

    {
        constexpr std::int32_t maximumId =
            std::numeric_limits<std::int32_t>::max();
        const auto allEntityContainersEmpty = [](const sed::Level& level)
        {
            return level.sectors.empty()
                && level.cogs.empty()
                && level.things.empty()
                && level.lights.empty()
                && level.h3doNodes.empty();
        };
        const auto rejectsExhaustedFactory =
            [maximumId, &allEntityContainersEmpty](
                const auto& factory,
                const char* message)
        {
            sed::Level level{};
            level.curId = maximumId;
            bool exhausted = false;
            try
            {
                factory(level);
            }
            catch (const sed::IdExhaustionError&)
            {
                exhausted = true;
            }
            check(exhausted && level.curId == maximumId
                    && allEntityContainersEmpty(level),
                message);
        };

        rejectsExhaustedFactory(
            [](sed::Level& level) { (void)level.newSector(); },
            "Level.newSector rejects exhausted ID before appending or advancing");
        rejectsExhaustedFactory(
            [](sed::Level& level) { (void)level.newCog(); },
            "Level.newCog rejects exhausted ID before appending or advancing");
        rejectsExhaustedFactory(
            [](sed::Level& level) { (void)level.newThing(); },
            "Level.newThing rejects exhausted ID before appending or advancing");
        rejectsExhaustedFactory(
            [](sed::Level& level) { (void)level.newLight(); },
            "Level.newLight rejects exhausted ID before appending or advancing");
        rejectsExhaustedFactory(
            [](sed::Level& level) { (void)level.new3doNode(); },
            "Level.new3doNode rejects exhausted ID before appending or advancing");
    }

    {
        constexpr std::int32_t maximumId =
            std::numeric_limits<std::int32_t>::max();
        const auto rejectedWithoutMutation = [](sed::Level& level,
                                                  const auto& create)
        {
            const auto before = sed::assets::saveToSedBytes(level);
            const std::int32_t nextId = level.curId;
            const std::int32_t index = create(level);
            return index < 0 && level.curId == nextId
                && sed::assets::saveToSedBytes(level) == before;
        };

        sed::Level cubeLevel{};
        cubeLevel.curId = maximumId;
        check(rejectedWithoutMutation(
                  cubeLevel,
                  [](sed::Level& level)
                  {
                      const auto created = sed::level::levelCreateCube(
                          level, 0, 0, 0, sed::TVector::up,
                          sed::TVector::right, {});
                      return created.value_or(-1);
                  }),
            "Level optional Cube creator safely rejects exhausted identity");

        const sed::Sector shape = makeTestCubeSector(0.0);
        sed::Level shapeLevel{};
        shapeLevel.curId = maximumId;
        check(rejectedWithoutMutation(
                  shapeLevel,
                  [&shape](sed::Level& level)
                  {
                      return sed::level::levelCreateSectorFromShapeBox(
                          level, shape, {-1, -1, -1}, {1, 1, 1});
                  }),
            "Level shape creator safely rejects exhausted identity before candidate staging");

        sed::Level primitiveLevel{};
        primitiveLevel.curId = maximumId;
        check(rejectedWithoutMutation(
                  primitiveLevel,
                  [](sed::Level& level)
                  {
                      return sed::level::levelCreateSectorPrimitiveBox(
                          level, sed::level::SectorPrimitive::Cube,
                          {-1, -1, -1}, {1, 1, 1});
                  }),
            "Level primitive creator safely rejects exhausted identity before candidate staging");

        sed::Level duplicateLevel{};
        sed::Sector source = makeTestCubeSector(0.0);
        source.id = 7;
        source.num = 0;
        source.mark = 91;
        duplicateLevel.sectors.push_back(std::move(source));
        duplicateLevel.curId = maximumId;
        const auto beforeDuplicate = sed::assets::saveToSedBytes(duplicateLevel);
        const std::int32_t duplicateIndex = sed::level::levelDuplicateSector(
            duplicateLevel, 0, 0, 0, 0,
            sed::TVector::right, sed::TVector::up, sed::TVector{0, 0, 1},
            0, 0, 0);
        check(duplicateIndex < 0 && duplicateLevel.curId == maximumId
                && duplicateLevel.sectors.size() == 1
                && duplicateLevel.sectors[0].mark == 91
                && sed::assets::saveToSedBytes(duplicateLevel)
                    == beforeDuplicate,
            "Level duplicate safely rejects exhausted identity before staging or publication");
    }

    {
        sed::Level level{};
        level.clear();

        auto& sector = level.newSector();
        check(level.sectors.size() == 1, "Level.newSector.size");
        check(sector.id == 1, "Level.newSector.id");
        check(sector.num == 0, "Level.newSector.num");

        const std::int32_t v0 = sector.addVertex(1.0, 2.0, 3.0);
        check(v0 == 0, "Sector.addVertex.index");
        check(sector.vertices.size() == 1, "Sector.addVertex.size");
        check(close(sector.vertices[0].x, 1.0), "Sector.addVertex.x");
        check(close(sector.vertices[0].y, 2.0), "Sector.addVertex.y");
        check(close(sector.vertices[0].z, 3.0), "Sector.addVertex.z");
        check(sector.vertices[0].mark == 0, "Sector.addVertex.mark");
    }
}

} // namespace sed::tests
