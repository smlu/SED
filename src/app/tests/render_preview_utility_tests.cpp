#include "test_common.hpp"

#include "render/mesh/render_lighting_utils.hpp"
#include "render/translucency/translucent_order_utils.hpp"
#include "sed/assets/3do.hpp"
#include "sed/assets/episode.hpp"
#include "sed/assets/jlh.hpp"
#include "sed/assets/tpl.hpp"
#include "ui/components/bounded_two_queue_cache.hpp"
#include "ui/components/preview_resource_disk_watcher.hpp"
#include "ui/components/preview_resource_revision_utils.hpp"
#include "ui/components/preview_thumbnail_list.hpp"
#include "ui/components/preview_worker_pool.hpp"
#include "ui/components/template_model_preview_icon.hpp"
#include "ui/project/project_startup_utils.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <limits>
#include <memory>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <unordered_set>
#include <vector>

#include <QFile>
#include <QCoreApplication>
#include <QDir>
#include <QEvent>
#include <QEventLoop>
#include <QListWidget>
#include <QPointer>
#include <QSemaphore>
#include <QTemporaryDir>

namespace sed::app_tests
{
namespace
{

bool writeFixture(const QString& path, const QByteArray& bytes)
{
    QFile file(path);
    return file.open(QIODevice::WriteOnly | QIODevice::Truncate)
        && file.write(bytes) == bytes.size();
}

bool writeFixtureWithRevision(
    const QString& path,
    const QByteArray& bytes,
    const std::filesystem::file_time_type revision)
{
    if (!writeFixture(path, bytes))
    {
        return false;
    }

    std::error_code ec{};
    std::filesystem::last_write_time(
        std::filesystem::path(path.toStdWString()), revision, ec);
    return !ec;
}

QByteArray makeDiskChangeModelFixture(
    const QByteArray& material,
    const QByteArray& revisionComment,
    const double halfWidth)
{
    QByteArray model{
        "SECTION: HEADER\n"
        "3DO 2.3\n"
        "SECTION: MODELRESOURCE\n"
        "MATERIALS 1\n"
        "0: " };
    model += material;
    model +=
        "\nSECTION: GEOMETRYDEF\n"
        "RADIUS 4\n"
        "INSERT OFFSET 0 0 0\n"
        "GEOSETS 1\n"
        "GEOSET 0\n"
        "MESHES 1\n"
        "MESH 0\n"
        "NAME disk_change_mesh\n"
        "RADIUS 4\n"
        "GEOMETRYMODE 4\n"
        "LIGHTINGMODE 3\n"
        "TEXTUREMODE 3\n"
        "VERTICES 4\n"
        "0: ";
    model += QByteArray::number(-halfWidth, 'f', 3);
    model +=
        " 0 -1 0 0 0 1\n"
        "1: ";
    model += QByteArray::number(halfWidth, 'f', 3);
    model +=
        " 0 -1 0 0 0 1\n"
        "2: ";
    model += QByteArray::number(halfWidth, 'f', 3);
    model +=
        " 0 1 0 0 0 1\n"
        "3: ";
    model += QByteArray::number(-halfWidth, 'f', 3);
    model +=
        " 0 1 0 0 0 1\n"
        "TEXTURE VERTICES 4\n"
        "0: 0 0\n"
        "1: 1 0\n"
        "2: 1 1\n"
        "3: 0 1\n"
        "VERTEX NORMALS\n"
        "0: 0 -1 0\n"
        "1: 0 -1 0\n"
        "2: 0 -1 0\n"
        "3: 0 -1 0\n"
        "FACES 1\n"
        "0: 0 0x0 4 3 3 (1/1/1/1) 4 0, 0 1, 1 2, 2 3, 3\n"
        "FACE NORMALS\n"
        "0: 0 -1 0\n"
        "SECTION: HIERARCHYDEF\n"
        "HIERARCHY NODES 1\n"
        "0: 0x0 0x1 0 -1 -1 -1 0 0 0 0 0 0 0 0 0 0 disk_change_mesh\n"
        "# ";
    model += revisionComment;
    model += '\n';
    return model;
}

QByteArray makeClassicDiskChangeModelFixture(
    const QByteArray& material,
    const QByteArray& revisionComment,
    const double halfWidth)
{
    QByteArray model = makeDiskChangeModelFixture(
        material,
        revisionComment,
        halfWidth);
    model.replace("3DO 2.3", "3DO 2.1");
    model.replace("(1/1/1/1)", "1.0");
    return model;
}

bool writeGobFixture(const QString& path, const QByteArray& entryName,
    const QByteArray& payload);

void runPreviewResourceIdentityRegressionTests()
{
    QTemporaryDir firstRoot;
    QTemporaryDir secondRoot;
    check(firstRoot.isValid() && secondRoot.isValid(),
        "preview identity: temporary resource roots exist");
    if (!firstRoot.isValid() || !secondRoot.isValid()) return;
    const QString name = QStringLiteral("same_name.3do");
    check(writeFixture(firstRoot.filePath(name), makeDiskChangeModelFixture("unused.mat", "first root", 1.0))
            && writeFixture(secondRoot.filePath(name), makeDiskChangeModelFixture("unused.mat", "second root", 3.0)),
        "preview identity: independent roots contain distinct same-name models");
    assets::ResourceLocator first;
    assets::ResourceLocator second;
    first.addRoot(std::filesystem::path(firstRoot.path().toStdWString()));
    second.addRoot(std::filesystem::path(secondRoot.path().toStdWString()));
    ui::detail::clearSharedPreviewAssetCaches();
    const auto firstModel = ui::detail::loadSharedPreviewModel(&first, "same_name.3do");
    const auto firstBox = firstModel ? ui::detail::previewBoxForModel(*firstModel) : std::nullopt;
    ui::detail::storeSharedPreviewModelBox(&first, "same_name.3do", firstBox);
    std::optional<ThingBox> secondCachedBox;
    const bool hasSecondBox = ui::detail::findSharedPreviewModelBox(&second, "same_name.3do", secondCachedBox);
    check(!hasSecondBox || (secondCachedBox && secondCachedBox->x2 == 3.0),
        "preview identity: box hint never borrows another locator's same-name model");
    const auto secondModel = ui::detail::loadSharedPreviewModel(&second, "same_name.3do");
    const auto secondBox = secondModel ? ui::detail::previewBoxForModel(*secondModel) : std::nullopt;
    check(firstBox && firstBox->x2 == 1.0 && secondBox && secondBox->x2 == 3.0,
        "preview identity: same-epoch independent locators load their own model geometry");
    const assets::ResourceLocator equivalent = first;
    const auto equivalentModel = ui::detail::loadSharedPreviewModel(&equivalent, "same_name.3do");
    check(equivalentModel == firstModel,
        "preview identity: equivalent immutable snapshots share the exact parsed model");

    // Rendered-image hints must carry the same locator scope as parsed models
    // and boxes; otherwise a warm row can display another project's pixels.
    const QSize iconSize(24, 24);
    QImage firstImage(iconSize, QImage::Format_ARGB32);
    firstImage.fill(Qt::magenta);
    const auto epoch = ui::detail::currentSharedPreviewCacheEpoch();
    check(ui::detail::storeAuthoritativeSharedModelPreviewImage(&first, "", "same_name.3do",
            iconSize, false, epoch, firstImage),
        "preview identity: first root publishes its rendered image");
    std::unordered_map<std::string, QIcon> icons;
    QIcon secondIcon;
    check(!ui::detail::findCachedSharedModelPreviewIcon(icons, &second, "", "same_name.3do",
            iconSize, false, secondIcon),
        "preview identity: rendered image hint never borrows another root's pixels");
    QIcon equivalentIcon;
    bool checkedRevision = true;
    check(ui::detail::findCachedSharedModelPreviewIcon(icons, &equivalent, "", "same_name.3do",
            iconSize, false, equivalentIcon, &checkedRevision)
            && equivalentIcon.pixmap(iconSize).toImage().pixelColor(12, 12) == QColor(Qt::magenta)
            && !checkedRevision,
        "preview identity: equivalent snapshot shares completed pixels without synchronous revision checks");
    QImage secondImage(iconSize, QImage::Format_ARGB32);
    secondImage.fill(Qt::green);
    check(ui::detail::storeAuthoritativeSharedModelPreviewImage(&second, "", "same_name.3do",
            iconSize, false, epoch, secondImage),
        "preview identity: second root publishes its own rendered image");
    QIcon firstIcon;
    check(ui::detail::findCachedSharedModelPreviewIcon(icons, &first, "", "same_name.3do",
            iconSize, false, firstIcon)
            && firstIcon.pixmap(iconSize).toImage().pixelColor(12, 12) == QColor(Qt::magenta)
            && ui::detail::findCachedSharedModelPreviewIcon(icons, &second, "", "same_name.3do",
                iconSize, false, secondIcon)
            && secondIcon.pixmap(iconSize).toImage().pixelColor(12, 12) == QColor(Qt::green),
        "preview identity: simultaneous same-name rendered hints retain independent root pixels");

    assets::ResourceLocator changed = first;
    changed.addRoot(std::filesystem::path(secondRoot.path().toStdWString()));
    const auto changedModel = ui::detail::loadSharedPreviewModel(&changed, "same_name.3do");
    const auto changedBox = changedModel ? ui::detail::previewBoxForModel(*changedModel) : std::nullopt;
    check(changedBox && changedBox->x2 == 3.0,
        "preview identity: same mutable locator honors newly added higher-priority roots");

    assets::ResourceLocator independentlyEquivalent;
    independentlyEquivalent.addRoot(std::filesystem::path(firstRoot.path().toStdWString()));
    check(ui::detail::loadSharedPreviewModel(&independentlyEquivalent, "same_name.3do") == firstModel,
        "preview identity: independently equivalent disk sources reuse the authoritative parsed owner");
    bool failedMount = false;
    try { first.mountGob(std::filesystem::path(firstRoot.filePath("absent.gob").toStdWString())); }
    catch ( const std::exception& ) { failedMount = true; }
    std::shared_ptr<const assets::Model3do> afterFailure;
    check(failedMount && ui::detail::findCachedSharedPreviewModel(&first, "same_name.3do", afterFailure)
            && afterFailure == firstModel,
        "preview identity: failed source mutation preserves the preceding warm snapshot hint");

    const QString archivePath = firstRoot.filePath("identity.gob");
    check(writeGobFixture(archivePath, QByteArray("3do\\inside.3do"),
            makeDiskChangeModelFixture("unused.mat", "mounted", 1.0)),
        "preview identity: archive fixture writes");
    assets::ResourceLocator mounted;
    mounted.mountGob(std::filesystem::path(archivePath.toStdWString()));
    const assets::ResourceLocator mountedCopy = mounted;
    const auto mountedModel = ui::detail::loadSharedPreviewModel(&mounted, "inside.3do");
    check(mountedModel && ui::detail::loadSharedPreviewModel(&mountedCopy, "inside.3do") == mountedModel,
        "preview identity: copied immutable mounted generation shares its parsed owner");
    const auto archiveRevision = std::filesystem::last_write_time(
        std::filesystem::path(archivePath.toStdWString()));
    check(writeGobFixture(archivePath, QByteArray("3do\\inside.3do"),
            makeDiskChangeModelFixture("unused.mat", "mounted", 4.0)),
        "preview identity: archive replacement fixture writes");
    std::filesystem::last_write_time(std::filesystem::path(archivePath.toStdWString()), archiveRevision);
    mounted.reloadGobs();
    const auto remountedModel = ui::detail::loadSharedPreviewModel(&mounted, "inside.3do");
    const auto remountedBox = remountedModel ? ui::detail::previewBoxForModel(*remountedModel) : std::nullopt;
    check(remountedBox && remountedBox->x2 == 4.0,
        "preview identity: explicit archive remount cannot reuse old pixels when size and timestamp match");

    assets::ResourceLocator movedFrom = first;
    const auto movedIdentity = movedFrom.previewCacheIdentity();
    assets::ResourceLocator movedTo = std::move(movedFrom);
    std::shared_ptr<const assets::Model3do> movedModel;
    check(movedTo.previewCacheIdentity() == movedIdentity
            && ui::detail::findCachedSharedPreviewModel(&movedTo, "same_name.3do", movedModel)
            && movedModel == firstModel,
        "preview identity: moved-to locator keeps its matching warm asset hints");
    std::shared_ptr<const assets::Model3do> emptiedModel;
    check(movedFrom.roots().empty() && movedFrom.gobs().empty()
            && !ui::detail::findCachedSharedPreviewModel(&movedFrom, "same_name.3do", emptiedModel),
        "preview identity: moved-from empty locator cannot borrow its transferred asset hints");

    {
        const int previousWorkerCount = ui::detail::previewWorkerCount();
        ui::detail::setPreviewWorkerCount(2);
        std::unordered_set<std::string> inFlight;
        int activeJobs = 0;
        int completions = 0;
        class ObservedOwner final : public QObject
        {
        public:
            int destructionConnections() const
            {
                return receivers(SIGNAL(destroyed(QObject*)));
            }
        } owner;
        const int initialDestructionConnections = owner.destructionConnections();
        // Fail callback capture deterministically after reservation and watcher
        // creation, without changing process-wide allocation behavior.
        struct ThrowingReady
        {
            ThrowingReady() = default;
            ThrowingReady(const ThrowingReady&) { throw std::runtime_error("callback copy failure"); }
            ThrowingReady(ThrowingReady&&) { throw std::runtime_error("callback move failure"); }
            void operator()(bool) const {}
        } throwingReady;
        inFlight.insert("existing-job");
        activeJobs = 1;
        const auto initialChildren = owner.children().size();
        for (const bool moveCallback : {false, true})
        {
            bool threw = false;
            try
            {
                const auto snapshot = std::make_shared<const assets::ResourceLocator>(first);
                if (moveCallback)
                    (void)ui::detail::queueSharedPreviewModelBoxPrep(&owner, snapshot,
                        "same_name.3do", "", iconSize, false, inFlight, activeJobs, std::move(throwingReady));
                else
                    (void)ui::detail::queueSharedPreviewModelBoxPrep(&owner, snapshot,
                        "same_name.3do", "", iconSize, false, inFlight, activeJobs, throwingReady);
            }
            catch (const std::runtime_error&) { threw = true; }
            check(threw && activeJobs == 1 && inFlight.size() == 1
                    && inFlight.contains("existing-job")
                    && owner.children().size() == initialChildren
                    && owner.destructionConnections() == initialDestructionConnections,
                "preview lifecycle: failed callback setup rolls back only its reservation and watcher");
        }
        inFlight.clear();
        activeJobs = 0;
        const auto ready = [&](bool) { ++completions; };
        const bool firstQueued = ui::detail::queueSharedPreviewModelBoxPrep(&owner,
            std::make_shared<const assets::ResourceLocator>(first), "same_name.3do", "",
            iconSize, false, inFlight, activeJobs, ready);
        const bool duplicateQueued = ui::detail::queueSharedPreviewModelBoxPrep(&owner,
            std::make_shared<const assets::ResourceLocator>(equivalent), "same_name.3do", "",
            iconSize, false, inFlight, activeJobs, ready);
        const bool secondQueued = ui::detail::queueSharedPreviewModelBoxPrep(&owner,
            std::make_shared<const assets::ResourceLocator>(second), "same_name.3do", "",
            iconSize, false, inFlight, activeJobs, ready);
        check(firstQueued && secondQueued && !duplicateQueued,
            "preview identity: one owner queues distinct locator generations and deduplicates equivalent snapshots");
        ui::detail::previewWorkerPool().waitForDone();
        QCoreApplication::sendPostedEvents();
        QCoreApplication::processEvents(QEventLoop::AllEvents);
        check(activeJobs == 0 && inFlight.empty() && completions == 2,
            "preview identity: each distinct queued locator retires and wakes its owner exactly once");
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        check(owner.destructionConnections() == initialDestructionConnections,
            "preview lifecycle: completed thumbnail jobs release owner-destruction guards and connections");
        ui::detail::setPreviewWorkerCount(previousWorkerCount);
    }

    // The locator skips directories; the revision resolver must select the
    // same regular-file fallback, including when an extension is appended.
    const QString bare = QStringLiteral("directory_shadow");
    check(QDir(firstRoot.path()).mkdir(bare)
            && writeFixture(firstRoot.filePath(bare + QStringLiteral(".3do")),
                makeDiskChangeModelFixture("unused.mat", "extension fallback", 2.0)),
        "preview revision: directory-shadow fallback fixture exists");
    const std::string revision = ui::detail::previewResourceRevisionToken(&first, "directory_shadow", ".3do");
    const std::string expected = ui::detail::previewFileRevisionToken("DISK",
        std::filesystem::path(firstRoot.filePath(bare + QStringLiteral(".3do")).toStdWString()));
    check(first.tryReadBinaryWithExt("directory_shadow", ".3do").has_value()
            && revision == expected,
        "preview revision: directory shadow resolves the file actually read by ResourceLocator");
    ui::detail::clearSharedPreviewAssetCaches();
}

void runMotsArchLightingPreviewRegressionTests()
{
    Level level{};
    level.kind = ProjectType::MOTS;

    MotsArchLightObject object{};
    object.wireIndex = 42;
    MotsArchLightMesh mesh{};
    mesh.wireIndex = 7;
    mesh.vertices = {
        MotsArchLightVertex{ 0.25f, 0.125f, 0.25f, 0.5f },
        MotsArchLightVertex{ 0.50f, 0.25f, 0.5f, 0.75f },
        MotsArchLightVertex{ 0.75f, 0.5f, 0.75f, 1.0f },
    };
    object.meshes.push_back(mesh);
    level.motsArchLighting.push_back(object);

    ThingTemplate levelGeoTemplate{};
    levelGeoTemplate.name = "+arch_fixture";
    TplValue levelGeoFlags{};
    levelGeoFlags.name = "thingflags";
    levelGeoFlags.type = ValueType::integer;
    levelGeoFlags.intValue = 0x00000008;
    levelGeoTemplate.values.push_back(levelGeoFlags);
    level.templates.push_back(levelGeoTemplate);

    Thing thing{};
    thing.archLightingIndex = 0;
    thing.templateName = "+arch_fixture";

    assets::Model3do model{};
    assets::Model3doMesh modelMesh{};
    modelMesh.vertices.resize(3);
    modelMesh.vertices[0].color = ColorF{ 0.875f, 0.75f, 0.625f, 0.625f };
    modelMesh.vertices[1].color = ColorF{ 0.75f, 0.625f, 0.5f, 0.5f };
    modelMesh.vertices[2].color = ColorF{ 0.625f, 0.5f, 0.375f, 0.375f };
    model.meshes.push_back(modelMesh);
    const auto originalColors = std::vector<ColorF>{
        model.meshes[0].vertices[0].color,
        model.meshes[0].vertices[1].color,
        model.meshes[0].vertices[2].color,
    };

    const auto preview = render::resolveMotsArchLightingPreview(
        level, thing, model);
    check(preview != nullptr
            && preview == &level.motsArchLighting[0]
            && preview->meshes.size() == 1
            && preview->meshes[0].vertices.size() == 3
            && preview->meshes[0].vertices[0].mono == 0.25f
            && preview->meshes[0].vertices[0].red == 0.125f
            && preview->meshes[0].vertices[0].green == 0.25f
            && preview->meshes[0].vertices[0].blue == 0.5f,
        "mots_archlighting preview keeps mono separate and maps archive RGB without using it as opacity");
    check(model.meshes[0].vertices[0].color == originalColors[0]
            && model.meshes[0].vertices[1].color == originalColors[1]
            && model.meshes[0].vertices[2].color == originalColors[2],
        "mots_archlighting preview never mutates cached 3DO colors or authored alpha");

    const ColorF monoColor = render::motsArchLightingVertexColor(
        model.meshes[0].vertices[0].color,
        object.meshes[0].vertices[0],
        sed::level::MotsArchLightingVertexMode::Mono);
    const ColorF rgbColor = render::motsArchLightingVertexColor(
        model.meshes[0].vertices[0].color,
        object.meshes[0].vertices[0],
        sed::level::MotsArchLightingVertexMode::Rgb);
    check(monoColor == ColorF{ 0.25f, 0.25f, 0.25f, 0.625f }
            && rgbColor == ColorF{ 0.125f, 0.25f, 0.5f, 0.625f },
        "mots_archlighting renderer maps mono/RGB through the engine path while retaining model alpha");

    assets::Templates externalTemplates{};
    ThingTemplate externalLevelGeoTemplate{};
    externalLevelGeoTemplate.name = "+external_arch_fixture";
    TplValue externalLevelGeoFlags{};
    externalLevelGeoFlags.name = "thingflags";
    externalLevelGeoFlags.type = ValueType::integer;
    externalLevelGeoFlags.intValue = 0x00000008;
    externalLevelGeoTemplate.values.push_back(externalLevelGeoFlags);
    externalTemplates.addOrUpdate(externalLevelGeoTemplate);
    Thing externalThing = thing;
    externalThing.templateName = "+external_arch_fixture";
    check(render::resolveMotsArchLightingPreview(
              level, &externalTemplates, externalThing, model)
            == &level.motsArchLighting[0],
        "mots_archlighting preview resolves the effective Level Geometry flag from external template inheritance");

    const MotsArchLightVertex originalArchiveVertex =
        level.motsArchLighting[0].meshes[0].vertices[0];
    const auto rejectsNonFiniteChannel = [&](const auto setInvalid) {
        MotsArchLightVertex& archiveVertex =
            level.motsArchLighting[0].meshes[0].vertices[0];
        setInvalid(archiveVertex);
        const bool rejected = render::resolveMotsArchLightingPreview(
                                  level, thing, model)
            == nullptr;
        archiveVertex = originalArchiveVertex;
        return rejected;
    };
    check(rejectsNonFiniteChannel([](MotsArchLightVertex& vertex) {
              vertex.mono = std::numeric_limits<float>::quiet_NaN();
          })
            && rejectsNonFiniteChannel([](MotsArchLightVertex& vertex) {
                   vertex.red = std::numeric_limits<float>::infinity();
               })
            && rejectsNonFiniteChannel([](MotsArchLightVertex& vertex) {
                   vertex.green = -std::numeric_limits<float>::infinity();
               })
            && rejectsNonFiniteChannel([](MotsArchLightVertex& vertex) {
                   vertex.blue = std::numeric_limits<float>::quiet_NaN();
               }),
        "mots_archlighting preview rejects every non-finite mono and RGB archive channel");

    const auto rejectedJk = [&]()
    {
        Level jk = level;
        jk.kind = ProjectType::JKDF2;
        return render::resolveMotsArchLightingPreview(jk, thing, model);
    }();
    Thing noArchive = thing;
    noArchive.archLightingIndex = -1;
    const auto rejectedNoArchive = render::resolveMotsArchLightingPreview(
        level, noArchive, model);
    Thing badIndex = thing;
    badIndex.archLightingIndex = 5;
    const auto rejectedBadIndex = render::resolveMotsArchLightingPreview(
        level, badIndex, model);
    check(!rejectedJk && !rejectedNoArchive && !rejectedBadIndex,
        "mots_archlighting preview is gated to MotS Things with a valid archive index");

    Thing nonLevelGeometry = thing;
    nonLevelGeometry.templateName = "+ordinary";
    ThingTemplate ordinaryTemplate{};
    ordinaryTemplate.name = "+ordinary";
    TplValue ordinaryFlags{};
    ordinaryFlags.name = "thingflags";
    ordinaryFlags.type = ValueType::integer;
    ordinaryFlags.intValue = 0;
    ordinaryTemplate.values.push_back(ordinaryFlags);
    level.templates.push_back(ordinaryTemplate);
    check(render::resolveMotsArchLightingPreview(
              level, nonLevelGeometry, model) == nullptr,
        "mots_archlighting preview skips Things without the MotS Arch flag");

    MotsArchLightObject maxLodObject = object;
    MotsArchLightMesh maxLodMesh = mesh;
    maxLodMesh.wireIndex = 8;
    maxLodMesh.vertices.push_back(MotsArchLightVertex{ 1.0f, 1.0f, 1.0f, 1.0f });
    maxLodObject.meshes.push_back(maxLodMesh);
    level.motsArchLighting[0] = maxLodObject;
    assets::Model3do lowerLodModel{};
    assets::Model3doMesh lowerLodMesh = model.meshes[0];
    lowerLodMesh.vertices.resize(2);
    lowerLodModel.meshes.push_back(lowerLodMesh);
    check(render::resolveMotsArchLightingPreview(
              level, thing, lowerLodModel) == &level.motsArchLighting[0],
        "mots_archlighting preview accepts a lower geoset prefix covered by a max-LOD archive record");
    level.motsArchLighting[0] = object;

    assets::Model3doMesh extraMesh{};
    extraMesh.vertices.resize(3);
    model.meshes.push_back(extraMesh);
    check(!render::resolveMotsArchLightingPreview(level, thing, model),
        "mots_archlighting preview rejects a mesh-count mismatch without indexing past archive data");
    model.meshes.pop_back();
    model.meshes[0].vertices.resize(4);
    check(!render::resolveMotsArchLightingPreview(level, thing, model),
        "mots_archlighting preview rejects a model vertex prefix larger than the archive record");

    // The 3DO loader expands a shared source mesh into one transformed mesh
    // instance per hierarchy node. ARCHLIGHTING retains source-mesh ordinals,
    // so an expanded clone must carry its source mapping instead of consuming
    // a later surplus archive mesh by expanded render order.
    model.meshes[0].vertices.resize(3);
    assets::Model3do sharedExpandedModel = model;
    sharedExpandedModel.meshes.push_back(sharedExpandedModel.meshes.front());
    sharedExpandedModel.hierarchyTransformsApplied = true;
    sharedExpandedModel.nodes = {
        assets::Model3doNode{ .mesh = 0, .parent = -1 },
        assets::Model3doNode{ .mesh = 1, .parent = -1 },
    };
    sharedExpandedModel.meshSourceIndices.clear();
    check(!render::resolveMotsArchLightingPreview(
              level, thing, sharedExpandedModel),
        "mots_archlighting preview rejects expanded hierarchy data without source ordinals");

    MotsArchLightObject surplusObject = object;
    MotsArchLightMesh surplusMesh = mesh;
    surplusMesh.wireIndex = 99;
    surplusMesh.vertices[0] = MotsArchLightVertex{ 0.9f, 0.02f, 0.03f, 0.04f };
    surplusObject.meshes.push_back(surplusMesh);
    MotsArchLightMesh malformedSourceMesh = mesh;
    malformedSourceMesh.wireIndex = 100;
    surplusObject.meshes.push_back(malformedSourceMesh);
    level.motsArchLighting[0] = surplusObject;
    sharedExpandedModel.meshSourceIndices = { 0, 0 };
    check(render::resolveMotsArchLightingPreview(
              level, thing, sharedExpandedModel) == &level.motsArchLighting[0]
            && render::motsArchLightingSourceMeshIndex(sharedExpandedModel, 0)
                == std::optional<std::size_t>{ 0 }
            && render::motsArchLightingSourceMeshIndex(sharedExpandedModel, 1)
                == std::optional<std::size_t>{ 0 },
        "mots_archlighting preview reuses the source ordinal for expanded clones while ignoring surplus archive meshes");

    sharedExpandedModel.meshSourceIndices = { 0 };
    check(!render::resolveMotsArchLightingPreview(
              level, thing, sharedExpandedModel),
        "mots_archlighting preview rejects a source ordinal table whose shape does not match expanded meshes");
    sharedExpandedModel.meshSourceIndices = { 2, 2 };
    check(!render::resolveMotsArchLightingPreview(
              level, thing, sharedExpandedModel)
            && !render::motsArchLightingSourceMeshIndex(sharedExpandedModel, 0),
        "mots_archlighting preview rejects a positive source ordinal outside the expanded model table even when archive slots are surplus");
    level.motsArchLighting[0] = object;
}

void runClassicSectorTintScopeTests()
{
    const auto closeScalar = [](const float a, const float b) noexcept
    {
        return std::abs(a - b) <= 1.0e-6f;
    };

    Level level{};
    level.kind = ProjectType::JKDF2;
    level.sectors.emplace_back().tint =
        ColorF{ 0.6f, 0.4f, 0.5f, 1.0f };
    level.sectors.emplace_back().tint =
        ColorF{ 0.0f, 1.0f, 0.0f, 1.0f };

    Surface ordinarySurface{};
    ordinarySurface.faceFlags = 0u;

    Surface translucentAdjoin{};
    translucentAdjoin.faceFlags = FF_Transluent;
    translucentAdjoin.adjoin = SurfaceRef{ 1, 0 };

    Surface opaqueAdjoin = translucentAdjoin;
    opaqueAdjoin.faceFlags = 0u;

    Surface invalidMirror = translucentAdjoin;
    invalidMirror.adjoin = SurfaceRef{ 99, 0 };

    check(closeQVec(
              render::classicEditorSectorTintScale(&level, 0),
              render::classicSectorTintScale(level.sectors[0].tint))
            && closeQVec(
                render::classicEditorSectorTintScale(&level, 1),
                render::classicSectorTintScale(level.sectors[1].tint)),
        "classic editor rendering resolves authored tint from each surface's owning sector instead of the camera sector");

    check(closeQVec(
              render::classicEditorSurfaceTintScale(&level, 0, ordinarySurface),
              render::classicSectorTintScale(level.sectors[0].tint))
            && closeQVec(
                render::classicEditorSurfaceTintScale(&level, 0, translucentAdjoin),
                render::classicSectorTintScale(level.sectors[0].tint))
            && closeQVec(
                render::classicEditorSurfaceTintScale(&level, 0, opaqueAdjoin),
                render::classicSectorTintScale(level.sectors[0].tint))
            && closeQVec(
                render::classicEditorSurfaceTintScale(&level, 0, invalidMirror),
                render::classicSectorTintScale(level.sectors[0].tint)),
        "classic editor surfaces retain their owning Sector Tint without borrowing an adjoin mirror Sector Tint");

    check(!render::classicBatchUsesCameraSectorTint(
              ProjectType::JKDF2, false, true, ordinarySurface.faceFlags)
            && !render::classicBatchUsesCameraSectorTint(
                ProjectType::JKDF2,
                false,
                true,
                translucentAdjoin.faceFlags | FF_RD_Adjoin)
            && render::classicBatchUsesCameraSectorTint(
                ProjectType::JKDF2, true, true, ordinarySurface.faceFlags)
            && render::classicBatchUsesCameraSectorTint(
                ProjectType::JKDF2, true, true, FF_RD_Adjoin)
            && render::classicBatchUsesCameraSectorTint(
                ProjectType::JKDF2,
                true,
                true,
                translucentAdjoin.faceFlags | FF_RD_Adjoin)
            && render::classicBatchUsesCameraSectorTint(
                ProjectType::MOTS,
                true,
                false,
                translucentAdjoin.faceFlags | FF_RD_Adjoin)
            && !render::classicBatchUsesCameraSectorTint(
                ProjectType::IJIM, true, true, ordinarySurface.faceFlags),
        "classic tint selects local surface data in Editor and the camera-sector effect for every Game Preview batch");

    check(render::classicBatchUsesLocalSectorTint(
              ProjectType::JKDF2, false, true, ordinarySurface.faceFlags)
            && !render::classicBatchUsesLocalSectorTint(
                ProjectType::JKDF2, true, true, ordinarySurface.faceFlags)
            && !render::classicBatchUsesLocalSectorTint(
                ProjectType::JKDF2,
                true,
                true,
                translucentAdjoin.faceFlags | FF_RD_Adjoin)
            && !render::classicBatchUsesLocalSectorTint(
                ProjectType::IJIM,
                false,
                true,
                translucentAdjoin.faceFlags | FF_RD_Adjoin),
        "classic tint enables local vertex data only for Editor scene batches");

    constexpr float classicTranslucentAlpha = 90.0f / 255.0f;
    check(closeScalar(
              render::classicFaceOpacity(
                  ProjectType::JKDF2, FF_Transluent, 1.0f),
              classicTranslucentAlpha)
            && closeScalar(
                render::classicFaceOpacity(
                    ProjectType::MOTS, FF_Transluent, 0.2f),
                classicTranslucentAlpha)
            && closeScalar(
                render::classicFaceOpacity(
                    ProjectType::JKDF2, 0u, 0.65f),
                0.65f)
            && closeScalar(
                render::classicFaceOpacity(
                    ProjectType::IJIM, FF_Transluent, 0.65f),
                0.65f),
        "classic face translucency uses the engine's 90/255 vertex alpha without changing IJIM or ordinary authored alpha");

    check(render::classicEditorShowsTranslucentAdjoinBackface(
              ProjectType::JKDF2, false, true, FF_RD_Adjoin | FF_Transluent, false)
            && render::classicEditorShowsTranslucentAdjoinBackface(
                ProjectType::MOTS, false, true, FF_RD_Adjoin, true)
            && !render::classicEditorShowsTranslucentAdjoinBackface(
                ProjectType::JKDF2, true, true, FF_RD_Adjoin | FF_Transluent, false)
            && !render::classicEditorShowsTranslucentAdjoinBackface(
                ProjectType::JKDF2, false, true, FF_RD_Adjoin, false)
            && !render::classicEditorShowsTranslucentAdjoinBackface(
                ProjectType::IJIM, false, true, FF_RD_Adjoin | FF_Transluent, true),
        "only classic Editor translucent adjoins expose their filled mirror-side face from both directions");

    check(closeQVec(
              render::classicEditorSurfaceTintScale(&level, 0, opaqueAdjoin),
              render::classicSectorTintScale(level.sectors[0].tint))
            && closeScalar(
                render::classicFaceOpacity(
                    ProjectType::JKDF2, opaqueAdjoin.faceFlags, 1.0f),
                1.0f)
            && !render::translucency::levelSurfaceUsesTranslucentPass(
                true, false, false, false),
        "classic Editor opaque adjoins retain owner-sector color while clearing Face Translucent restores the solid pass");

    check(closeQVec(
              render::classicCameraSectorTintScale(&level, 0),
              render::classicSectorTintScale(level.sectors[0].tint))
            && closeQVec(
                render::classicCameraSectorTintScale(&level, -1),
                QVector3D(1.0f, 1.0f, 1.0f)),
        "classic Game Preview keeps the engine-style camera-sector tint effect");

    level.kind = ProjectType::MOTS;
    check(closeQVec(
              render::classicEditorSectorTintScale(&level, 1),
              render::classicSectorTintScale(level.sectors[1].tint)),
        "MotS editor rendering uses the same owner-sector tint contract as JKDF2");

    level.kind = ProjectType::IJIM;
    check(closeQVec(
              render::classicEditorSectorTintScale(&level, 0),
              QVector3D(1.0f, 1.0f, 1.0f))
            && closeQVec(
                render::classicEditorSurfaceTintScale(&level, 0, translucentAdjoin),
                QVector3D(1.0f, 1.0f, 1.0f))
            && closeQVec(
                render::classicCameraSectorTintScale(&level, 0),
                QVector3D(1.0f, 1.0f, 1.0f)),
        "IJIM remains neutral on both classic Sector Tint paths");
}

void runThingPlacementPreviewColormapTests()
{
    Level classicLevel{};
    classicLevel.kind = ProjectType::JKDF2;
    classicLevel.sectors.emplace_back().colormap = "10Cargo.cmp";

    check(ui::detail::thingAssetPreviewColormap(classicLevel) == "10Cargo.cmp",
        "thing-placement thumbnail workers and consumers share the resolved classic sector CMP when raw masterCMP is empty");

    classicLevel.masterCMP = "explicit.cmp";
    check(ui::detail::thingAssetPreviewColormap(classicLevel) == "explicit.cmp",
        "thing-placement previews prefer an explicit classic master CMP");

    Level defaultClassicLevel{};
    defaultClassicLevel.kind = ProjectType::MOTS;
    check(ui::detail::thingAssetPreviewColormap(defaultClassicLevel) == "dflt.cmp",
        "thing-placement previews use the classic default CMP when no level or sector CMP is authored");

    Level ijimLevel{};
    ijimLevel.kind = ProjectType::IJIM;
    check(ui::detail::thingAssetPreviewColormap(ijimLevel).empty(),
        "IJIM thing-placement previews preserve an empty master CMP");
}

void runLevImportOptionDefaultTests()
{
    const assets::LevLoadOptions options =
        ui::project::defaultLevImportOptions();
    check(close(options.dfToJkFactor, 40.0)
            && options.textureMode == assets::LevTextureMode::Keep
            && options.loadObjects,
        "LEV import options default to the live Pascal scale, retained texture names, and matching object import");

    check(ui::project::isValidLevImportScale(40.0)
            && ui::project::isValidLevImportScale(0.000001)
            && !ui::project::isValidLevImportScale(0.0)
            && !ui::project::isValidLevImportScale(-1.0)
            && !ui::project::isValidLevImportScale(
                std::numeric_limits<double>::infinity())
            && !ui::project::isValidLevImportScale(
                std::numeric_limits<double>::quiet_NaN()),
        "LEV import options accept only finite positive coordinate scales");
}

bool writeGobFixture(
    const QString& path,
    const QByteArray& entryName,
    const QByteArray& payload)
{
    QByteArray bytes("GOB ", 4);
    const auto appendU32 = [&bytes](const std::uint32_t value)
    {
        bytes.append(static_cast<char>(value & 0xffu));
        bytes.append(static_cast<char>((value >> 8u) & 0xffu));
        bytes.append(static_cast<char>((value >> 16u) & 0xffu));
        bytes.append(static_cast<char>((value >> 24u) & 0xffu));
    };

    appendU32(0x14u);
    appendU32(0x0cu);
    appendU32(1u);
    appendU32(16u + 136u);
    appendU32(static_cast<std::uint32_t>(payload.size()));
    QByteArray nameBytes(128, '\0');
    const qsizetype copySize = std::min(nameBytes.size(), entryName.size());
    std::copy_n(entryName.constData(), copySize, nameBytes.data());
    bytes.append(nameBytes);
    bytes.append(payload);
    return writeFixture(path, bytes);
}

QByteArray makeRgbaMatFixture(
    const int width,
    const int height,
    const std::vector<QColor>& pixels,
    const std::uint32_t alphaBpp = 8u)
{
    QByteArray bytes("MAT ", 4);
    const auto appendU32 = [&bytes](const std::uint32_t value)
    {
        bytes.append(static_cast<char>(value & 0xffu));
        bytes.append(static_cast<char>((value >> 8u) & 0xffu));
        bytes.append(static_cast<char>((value >> 16u) & 0xffu));
        bytes.append(static_cast<char>((value >> 24u) & 0xffu));
    };

    appendU32(0x32u);
    appendU32(2u);
    appendU32(1u);
    appendU32(1u);
    appendU32(static_cast<std::uint32_t>(ColorMode::RGBA));
    appendU32(32u);
    appendU32(8u);
    appendU32(8u);
    appendU32(8u);
    appendU32(16u);
    appendU32(8u);
    appendU32(0u);
    appendU32(0u);
    appendU32(0u);
    appendU32(0u);
    appendU32(alphaBpp);
    appendU32(24u);
    appendU32(0u);
    bytes.append(QByteArray(40, '\0'));
    appendU32(static_cast<std::uint32_t>(width));
    appendU32(static_cast<std::uint32_t>(height));
    appendU32(0u);
    appendU32(0u);
    appendU32(0u);
    appendU32(1u);
    for (const QColor& pixel : pixels)
    {
        bytes.append(static_cast<char>(pixel.blue()));
        bytes.append(static_cast<char>(pixel.green()));
        bytes.append(static_cast<char>(pixel.red()));
        bytes.append(static_cast<char>(pixel.alpha()));
    }
    return bytes;
}

QByteArray makeClassicCmpFixture(
    const std::uint8_t solidColorIndex,
    const QColor& solidColor)
{
    QByteArray bytes(64 + 256 * 3 + 64 * 256, '\0');
    bytes[0] = 'C';
    bytes[1] = 'M';
    bytes[2] = 'P';
    bytes[3] = ' ';

    bytes[64 + solidColorIndex * 3 + 0] =
        static_cast<char>(solidColor.red());
    bytes[64 + solidColorIndex * 3 + 1] =
        static_cast<char>(solidColor.green());
    bytes[64 + solidColorIndex * 3 + 2] =
        static_cast<char>(solidColor.blue());

    for (int level = 0; level < 64; ++level)
    {
        for (int paletteIndex = 0; paletteIndex < 256; ++paletteIndex)
        {
            bytes[64 + 256 * 3 + level * 256 + paletteIndex] =
                static_cast<char>(paletteIndex);
        }
    }
    return bytes;
}

QByteArray makeIndexedMatFixture(const std::uint8_t solidColorIndex)
{
    QByteArray bytes("MAT ", 4);
    const auto appendU32 = [&bytes](const std::uint32_t value)
    {
        bytes.append(static_cast<char>(value & 0xffu));
        bytes.append(static_cast<char>((value >> 8u) & 0xffu));
        bytes.append(static_cast<char>((value >> 16u) & 0xffu));
        bytes.append(static_cast<char>((value >> 24u) & 0xffu));
    };

    appendU32(0x32u);
    appendU32(2u);
    appendU32(1u);
    appendU32(1u);
    appendU32(static_cast<std::uint32_t>(ColorMode::Indexed));
    appendU32(8u);
    for (int field = 0; field < 12; ++field)
    {
        appendU32(0u);
    }

    appendU32(8u);
    appendU32(solidColorIndex);
    for (int field = 0; field < 8; ++field)
    {
        appendU32(0u);
    }

    appendU32(2u);
    appendU32(2u);
    appendU32(0u);
    appendU32(0u);
    appendU32(0u);
    appendU32(1u);
    bytes.append(static_cast<char>(solidColorIndex));
    bytes.append(static_cast<char>(solidColorIndex));
    bytes.append(static_cast<char>(solidColorIndex));
    bytes.append(static_cast<char>(solidColorIndex));
    return bytes;
}

assets::Model3doMesh makePreviewQuadMesh(
    const std::string_view name,
    const double left,
    const double right,
    const std::array<assets::Model3doTexVertex, 4>& texVertices,
    const std::int32_t material,
    const std::uint32_t faceFlags = 0u)
{
    assets::Model3doMesh mesh{};
    mesh.name = std::string(name);
    mesh.geo = static_cast<std::int32_t>(TGeoMode::Texture);
    mesh.light = static_cast<std::int32_t>(TLightMode::Gouraud);
    mesh.tex = static_cast<std::int32_t>(TGeoMode::Texture);
    mesh.vertices = {
        assets::Model3doVertex{ left, 0.0, -1.0 },
        assets::Model3doVertex{ right, 0.0, -1.0 },
        assets::Model3doVertex{ right, 0.0, 1.0 },
        assets::Model3doVertex{ left, 0.0, 1.0 },
    };
    mesh.texVertices.assign(texVertices.begin(), texVertices.end());

    assets::Model3doFace face{};
    face.material = material;
    face.faceFlags = faceFlags;
    face.geo = static_cast<std::int32_t>(TGeoMode::Texture);
    face.light = static_cast<std::int32_t>(TLightMode::Gouraud);
    face.tex = static_cast<std::int32_t>(TGeoMode::Texture);
    face.vertices = { 0, 1, 2, 3 };
    face.texVertices = { 0, 1, 2, 3 };
    mesh.faces.push_back(std::move(face));
    return mesh;
}

assets::Model3doMesh makePreviewDenseGridMesh(
    const std::string_view name,
    const double left,
    const double right,
    const double bottom,
    const double top,
    const int divisions,
    const std::int32_t material)
{
    assets::Model3doMesh mesh{};
    mesh.name = std::string(name);
    mesh.geo = static_cast<std::int32_t>(TGeoMode::Texture);
    mesh.light = static_cast<std::int32_t>(TLightMode::Gouraud);
    mesh.tex = static_cast<std::int32_t>(TGeoMode::Texture);

    const int safeDivisions = std::max(1, divisions);
    const int rowWidth = safeDivisions + 1;
    mesh.vertices.reserve(static_cast<std::size_t>(rowWidth * rowWidth));
    mesh.texVertices.reserve(static_cast<std::size_t>(rowWidth * rowWidth));
    for (int y = 0; y <= safeDivisions; ++y)
    {
        const double fy = static_cast<double>(y) / safeDivisions;
        for (int x = 0; x <= safeDivisions; ++x)
        {
            const double fx = static_cast<double>(x) / safeDivisions;
            mesh.vertices.push_back(assets::Model3doVertex{
                left + ((right - left) * fx),
                0.0,
                bottom + ((top - bottom) * fy),
            });
            mesh.texVertices.push_back(assets::Model3doTexVertex{
                static_cast<float>(fx),
                static_cast<float>(fy),
            });
        }
    }

    mesh.faces.reserve(static_cast<std::size_t>(safeDivisions * safeDivisions * 2));
    const auto appendTriangle = [&](const int a, const int b, const int c)
    {
        assets::Model3doFace face{};
        face.material = material;
        face.geo = static_cast<std::int32_t>(TGeoMode::Texture);
        face.light = static_cast<std::int32_t>(TLightMode::Gouraud);
        face.tex = static_cast<std::int32_t>(TGeoMode::Texture);
        face.vertices = { a, b, c };
        face.texVertices = { a, b, c };
        mesh.faces.push_back(std::move(face));
    };
    for (int y = 0; y < safeDivisions; ++y)
    {
        for (int x = 0; x < safeDivisions; ++x)
        {
            const int topLeft = (y * rowWidth) + x;
            const int topRight = topLeft + 1;
            const int bottomLeft = topLeft + rowWidth;
            const int bottomRight = bottomLeft + 1;
            appendTriangle(topLeft, topRight, bottomLeft);
            appendTriangle(topRight, bottomRight, bottomLeft);
        }
    }
    return mesh;
}

std::size_t dominantPixelCount(
    const QImage& image,
    const QColor& dominant)
{
    std::size_t count = 0u;
    for (int y = 0; y < image.height(); ++y)
    {
        for (int x = 0; x < image.width(); ++x)
        {
            const QColor pixel = QColor::fromRgba(image.pixel(x, y));
            const int selected = dominant.red() > dominant.green()
                ? pixel.red()
                : pixel.green();
            const int competing = dominant.red() > dominant.green()
                ? pixel.green()
                : pixel.red();
            if (selected > 70 && selected > competing * 2)
            {
                ++count;
            }
        }
    }
    return count;
}

std::size_t blueDominantPixelCount(const QImage& image)
{
    std::size_t count = 0u;
    for (int y = 0; y < image.height(); ++y)
    {
        for (int x = 0; x < image.width(); ++x)
        {
            const QColor pixel = QColor::fromRgba(image.pixel(x, y));
            if (pixel.blue() > 70
                && pixel.blue() > std::max(pixel.red(), pixel.green()) * 2)
            {
                ++count;
            }
        }
    }
    return count;
}

std::uint64_t imagePixelChecksum(const QImage& image) noexcept
{
    constexpr std::uint64_t kFnvOffset = 14695981039346656037ull;
    constexpr std::uint64_t kFnvPrime = 1099511628211ull;
    std::uint64_t hash = kFnvOffset;
    const auto append = [&hash](const std::uint32_t value) noexcept
    {
        hash ^= value;
        hash *= kFnvPrime;
    };

    append(static_cast<std::uint32_t>(image.width()));
    append(static_cast<std::uint32_t>(image.height()));
    for (int y = 0; y < image.height(); ++y)
    {
        for (int x = 0; x < image.width(); ++x)
        {
            append(image.pixel(x, y));
        }
    }
    return hash;
}

bool containsWatchPath(
    const std::vector<std::filesystem::path>& paths,
    const std::filesystem::path& expected)
{
    const std::filesystem::path normalized =
        ui::detail::normalizedPreviewWatchPath(expected);
    return std::find(paths.begin(), paths.end(), normalized) != paths.end();
}

void runBoundedTwoQueueCacheBranchTests()
{
    enum class OperationKind
    {
        insertOrAssign,
        find,
        clear,
    };

    struct Operation
    {
        OperationKind kind{};
        std::string key{};
        std::string value{};
        bool expectedFound{};
        std::string expectedOutput{};
        std::size_t expectedSize{};
        std::size_t expectedWeight{};
    };

    ui::detail::BoundedTwoQueueCache<std::string> cache(
        5u,
        [](const std::string& value) -> std::size_t
        {
            return value.size();
        });
    const std::vector<Operation> operations{
        { OperationKind::insertOrAssign, "a", "aa", false, {}, 1u, 2u },
        { OperationKind::insertOrAssign, "b", "b", false, {}, 2u, 3u },
        { OperationKind::find, "a", {}, true, "aa", 2u, 3u },
        { OperationKind::insertOrAssign, "c", "ccc", false, {}, 2u, 5u },
        { OperationKind::insertOrAssign, "a", "z", false, {}, 2u, 4u },
        { OperationKind::insertOrAssign, "c", "cccc", false, {}, 2u, 5u },
        { OperationKind::insertOrAssign, "d", "dd", false, {}, 2u, 5u },
        { OperationKind::insertOrAssign, "c", "ccccc", false, {}, 1u, 5u },
        { OperationKind::find, "a", {}, false, "unchanged", 1u, 5u },
        { OperationKind::find, "c", {}, true, "ccccc", 1u, 5u },
        { OperationKind::insertOrAssign, "c", "123456", false, {}, 0u, 0u },
        { OperationKind::insertOrAssign, "after-clear", "x", false, {}, 1u, 1u },
        { OperationKind::clear, {}, {}, false, {}, 0u, 0u },
    };

    bool operationsMatch = true;
    for ( const Operation& operation : operations )
    {
        std::string output = "unchanged";
        bool found = false;
        switch ( operation.kind )
        {
        case OperationKind::insertOrAssign:
            cache.insertOrAssign(operation.key, operation.value);
            break;

        case OperationKind::find:
            found = cache.find(operation.key, output);
            operationsMatch = operationsMatch
                && found == operation.expectedFound
                && output == operation.expectedOutput;
            break;

        case OperationKind::clear:
            cache.clear();
            break;
        }

        operationsMatch = operationsMatch
            && cache.size() == operation.expectedSize
            && cache.currentWeight() == operation.expectedWeight
            && cache.currentWeight() <= cache.maximumWeight();
    }
    check(operationsMatch,
        "preview 2Q cache operation table covers promotion, probationary and protected eviction, replacement weights, misses, oversized replacement, and clear");

    std::string missingOutput = "preserved";
    check(!cache.find("missing", missingOutput)
            && missingOutput == "preserved"
            && cache.size() == 0u
            && cache.currentWeight() == 0u,
        "preview 2Q cache clear removes both queues and a miss leaves its output unchanged");

    ui::detail::BoundedTwoQueueCache<std::string> zeroBudgetCache(
        0u,
        [](const std::string& value) -> std::size_t
        {
            return value.size();
        });
    zeroBudgetCache.insertOrAssign("zero", "x");
    std::string zeroBudgetOutput = "preserved";
    check(zeroBudgetCache.maximumWeight() == 0u
            && zeroBudgetCache.size() == 0u
            && zeroBudgetCache.currentWeight() == 0u
            && !zeroBudgetCache.find("zero", zeroBudgetOutput)
            && zeroBudgetOutput == "preserved",
        "preview 2Q cache with a zero budget rejects every insertion without changing miss output");

    ui::detail::BoundedTwoQueueCache<std::string> exactBudgetCache(
        5u,
        [](const std::string& value) -> std::size_t
        {
            return value.size();
        });
    exactBudgetCache.insertOrAssign("exact", "12345");
    std::string exactBudgetOutput{};
    check(exactBudgetCache.size() == 1u
            && exactBudgetCache.currentWeight() == 5u
            && exactBudgetCache.find("exact", exactBudgetOutput)
            && exactBudgetOutput == "12345",
        "preview 2Q cache accepts and retrieves an entry whose weight exactly equals its budget");

    ui::detail::BoundedTwoQueueCache<std::uint64_t> defaultWeightCache(
        sizeof(std::uint64_t) * 2u,
        {});
    defaultWeightCache.insertOrAssign("first", 11u);
    defaultWeightCache.insertOrAssign("second", 22u);
    defaultWeightCache.insertOrAssign("third", 33u);
    std::uint64_t defaultWeightOutput = 0u;
    check(defaultWeightCache.currentWeight() == sizeof(std::uint64_t) * 2u
            && defaultWeightCache.size() == 2u
            && !defaultWeightCache.find("first", defaultWeightOutput)
            && defaultWeightCache.find("second", defaultWeightOutput)
            && defaultWeightOutput == 22u
            && defaultWeightCache.find("third", defaultWeightOutput)
            && defaultWeightOutput == 33u,
        "preview 2Q cache uses sizeof Value as its default weight and evicts the oldest probationary entry");
}

void runPreviewThumbLoadQueueBranchTests()
{
    ui::detail::PreviewThumbLoadQueue queue{};
    const std::uint64_t initialGeneration = queue.generation();
    check(queue.empty()
            && queue.pendingCount() == 0
            && !queue.hasRow(0)
            && !queue.isPending(0)
            && !queue.markPending(0)
            && !queue.finish(0)
            && queue.nextScanRow(0) == -1
            && queue.nextScanRow(-1) == -1,
        "preview thumbnail queue starts empty and rejects invalid work without changing its counters");

    queue.reset(5, { -1, 0, 0, 2, 5 });
    const std::uint64_t resetGeneration = queue.generation();
    check(!queue.empty()
            && resetGeneration == initialGeneration + 1u
            && queue.pendingCount() == 2
            && queue.hasRow(0)
            && queue.hasRow(4)
            && !queue.hasRow(-1)
            && !queue.hasRow(5)
            && queue.isPending(0)
            && queue.isPending(2)
            && !queue.isPending(1),
        "preview thumbnail queue reset deduplicates valid pending rows and ignores rows outside its generation");

    check(!queue.markPending(-1)
            && !queue.markPending(0)
            && !queue.markPending(5)
            && queue.markPending(4)
            && queue.pendingCount() == 3,
        "preview thumbnail queue marks each in-range row only once");

    check(!queue.finish(-1)
            && !queue.finish(1)
            && queue.finish(0)
            && !queue.finish(0)
            && !queue.finish(5)
            && queue.pendingCount() == 2
            && !queue.isPending(0),
        "preview thumbnail queue finish is idempotent and cannot underflow its pending count");

    const std::array<int, 7> scanRows{
        queue.nextScanRow(3),
        queue.nextScanRow(3),
        queue.nextScanRow(3),
        queue.nextScanRow(3),
        queue.nextScanRow(1),
        queue.nextScanRow(4),
        queue.nextScanRow(4),
    };
    check(scanRows == std::array<int, 7>{ 0, 1, 2, 0, 0, 1, 2 },
        "preview thumbnail queue scan cursor wraps and remains valid when the owner row count shrinks or grows");

    queue.clear();
    const std::uint64_t clearedGeneration = queue.generation();
    check(queue.empty()
            && queue.pendingCount() == 0
            && clearedGeneration == resetGeneration + 1u
            && queue.nextScanRow(3) == 0,
        "preview thumbnail queue clear removes pending state, resets scanning, and invalidates stale generation tokens");

    queue.reset(-20, { 0, 1 });
    check(queue.empty()
            && queue.pendingCount() == 0
            && queue.generation() == clearedGeneration + 1u
            && !queue.hasRow(0),
        "preview thumbnail queue clamps a negative owner row count to an empty new generation");
}

void runPreviewThumbnailPureHelperBranchTests()
{
    ui::detail::PreviewThumbnailPublicationPacer pristinePacer{};
    check(!pristinePacer.interactionActiveAt(0)
            && pristinePacer.delayUntilQuietAt(0) == 0,
        "preview thumbnail publication pacer is immediately quiet before any interaction");

    struct PacerCase
    {
        std::int64_t nowMs{};
        bool expectedActive{};
        int expectedDelayMs{};
    };
    const std::array<PacerCase, 6> pacerCases{
        PacerCase{ 999, true, ui::detail::kPreviewThumbnailInteractionQuietMs },
        PacerCase{ 1'000, true, ui::detail::kPreviewThumbnailInteractionQuietMs },
        PacerCase{ 1'001, true, ui::detail::kPreviewThumbnailInteractionQuietMs - 1 },
        PacerCase{ 1'000 + ui::detail::kPreviewThumbnailInteractionQuietMs - 1, true, 1 },
        PacerCase{ 1'000 + ui::detail::kPreviewThumbnailInteractionQuietMs, false, 0 },
        PacerCase{ 2'000, false, 0 },
    };
    ui::detail::PreviewThumbnailPublicationPacer pacer{};
    pacer.noteInteractionAt(1'000);
    bool pacerCasesMatch = true;
    for ( const PacerCase& testCase : pacerCases )
    {
        pacerCasesMatch = pacerCasesMatch
            && pacer.interactionActiveAt(testCase.nowMs)
                == testCase.expectedActive
            && pacer.delayUntilQuietAt(testCase.nowMs)
                == testCase.expectedDelayMs;
    }
    check(pacerCasesMatch,
        "preview thumbnail publication pacer covers clock rollback, zero elapsed, quiet boundary, and settled output");

    struct BatchCase
    {
        int publishedCount{};
        int configuredLimit{};
        std::int64_t elapsedNs{};
        bool expected{};
    };
    const std::array<BatchCase, 7> batchCases{
        BatchCase{ 0, 8, 50'000'000, true },
        BatchCase{ 1, 8, ui::detail::kPreviewThumbnailPublishBudgetNs - 1, true },
        BatchCase{ 1, 8, ui::detail::kPreviewThumbnailPublishBudgetNs, false },
        BatchCase{ 8, 8, 0, false },
        BatchCase{ 9, 8, 0, false },
        BatchCase{ 0, 0, 0, false },
        BatchCase{ 0, -1, 0, false },
    };
    bool batchCasesMatch = true;
    for ( const BatchCase& testCase : batchCases )
    {
        batchCasesMatch = batchCasesMatch
            && ui::detail::previewThumbnailBatchCanContinue(
                testCase.publishedCount,
                testCase.configuredLimit,
                testCase.elapsedNs) == testCase.expected;
    }
    check(batchCasesMatch,
        "preview thumbnail publication batch covers first-item progress, time and count boundaries, and disabled limits");

    check(!ui::detail::previewThumbnailQueueNeedsPolling(false, -1)
            && !ui::detail::previewThumbnailQueueNeedsPolling(false, 0)
            && !ui::detail::previewThumbnailQueueNeedsPolling(false, 1)
            && ui::detail::previewThumbnailQueueNeedsPolling(true, -1)
            && ui::detail::previewThumbnailQueueNeedsPolling(true, 0)
            && !ui::detail::previewThumbnailQueueNeedsPolling(true, 1),
        "preview thumbnail polling covers every visible-pending and active-worker boundary");

    struct PriorityCase
    {
        int rowBegin{};
        int rowEnd{};
        int currentRow{};
        std::vector<int> expected{};
    };
    const std::vector<PriorityCase> priorityCases{
        { -1, 2, 0, {} },
        { 4, 3, 4, {} },
        { 4, 4, 4, { 4 } },
        { 4, 4, 3, { 4 } },
        { 4, 8, 4, { 4, 5, 6, 7, 8 } },
        { 4, 8, 8, { 8, 4, 5, 6, 7 } },
        { 4, 8, 6, { 6, 4, 5, 7, 8 } },
        { 4, 8, 20, { 4, 5, 6, 7, 8 } },
    };
    bool priorityCasesMatch = true;
    for ( const PriorityCase& testCase : priorityCases )
    {
        priorityCasesMatch = priorityCasesMatch
            && ui::detail::previewThumbnailPriorityRows(
                testCase.rowBegin,
                testCase.rowEnd,
                testCase.currentRow) == testCase.expected;
    }
    check(priorityCasesMatch,
        "preview thumbnail priority ordering covers invalid, singleton, first, last, middle, and absent current rows");

    struct PageStepCase
    {
        int viewportExtent{};
        int rowExtent{};
        int expected{};
    };
    const std::array<PageStepCase, 9> pageStepCases{
        PageStepCase{ -5, -1, 1 },
        PageStepCase{ 0, 0, 1 },
        PageStepCase{ 0, 166, 1 },
        PageStepCase{ 120, 166, 120 },
        PageStepCase{ 166, 166, 166 },
        PageStepCase{ 167, 166, 166 },
        PageStepCase{ 331, 166, 166 },
        PageStepCase{ 332, 166, 166 },
        PageStepCase{ 840, 166, 664 },
    };
    bool pageStepCasesMatch = true;
    for ( const PageStepCase& testCase : pageStepCases )
    {
        pageStepCasesMatch = pageStepCasesMatch
            && ui::detail::previewThumbnailPageStep(
                testCase.viewportExtent,
                testCase.rowExtent) == testCase.expected;
    }
    check(pageStepCasesMatch,
        "preview thumbnail page-step helper covers invalid extents, short viewports, exact rows, partial rows, and overlap");

    check(ui::detail::assetPreviewSpinStepDegrees(-1.0f, 50) == 0.0f
            && ui::detail::assetPreviewSpinStepDegrees(0.0f, 50) == 0.0f
            && ui::detail::assetPreviewSpinStepDegrees(100.0f, -1) == 0.1f
            && ui::detail::assetPreviewSpinStepDegrees(100.0f, 0) == 0.1f
            && ui::detail::assetPreviewSpinStepDegrees(100.0f, 1) == 0.1f
            && ui::detail::assetPreviewSpinStepDegrees(100.0f, 100) == 10.0f
            && ui::detail::assetPreviewSpinStepDegrees(100.0f, 101) == 10.0f,
        "preview rotation helper clamps negative speed and elapsed-time underflow or catch-up overflow");
}

void runPreviewThumbnailWidgetBoundaryTests()
{
    int nullBegin = 8;
    int nullEnd = 9;
    int nullProbeCount = 4;
    check(!ui::detail::visiblePreviewListRowRange(
              nullptr,
              nullBegin,
              nullEnd,
              -20,
              &nullProbeCount)
            && nullBegin == -1
            && nullEnd == -1
            && nullProbeCount == 4,
        "preview visible-range helper rejects a null list and initializes its row outputs without probing");

    QListWidget emptyList{};
    emptyList.resize(320, 240);
    int emptyBegin = 8;
    int emptyEnd = 9;
    check(!ui::detail::visiblePreviewListRowRange(
              &emptyList,
              emptyBegin,
              emptyEnd)
            && emptyBegin == -1
            && emptyEnd == -1,
        "preview visible-range helper rejects an empty list and initializes its row outputs");

    QListWidget foreignOwner{};
    auto* foreignItem = new QListWidgetItem(QStringLiteral("foreign"), &foreignOwner);
    foreignItem->setData(
        ui::detail::kPreviewThumbStateRole,
        static_cast<int>(ui::detail::PreviewThumbState::none));
    QListWidget publicationTarget{};
    publicationTarget.addItem(QStringLiteral("target"));
    QImage foreignIconImage(8, 8, QImage::Format_ARGB32_Premultiplied);
    foreignIconImage.fill(QColor(200, 40, 80));
    ui::detail::publishPreviewThumbnailItem(
        nullptr,
        foreignItem,
        QIcon(QPixmap::fromImage(foreignIconImage)),
        ui::detail::kPreviewThumbStateRole,
        static_cast<int>(ui::detail::PreviewThumbState::ready));
    ui::detail::publishPreviewThumbnailItem(
        &publicationTarget,
        nullptr,
        QIcon(QPixmap::fromImage(foreignIconImage)),
        ui::detail::kPreviewThumbStateRole,
        static_cast<int>(ui::detail::PreviewThumbState::ready));
    ui::detail::publishPreviewThumbnailItem(
        &publicationTarget,
        foreignItem,
        QIcon(QPixmap::fromImage(foreignIconImage)),
        ui::detail::kPreviewThumbStateRole,
        static_cast<int>(ui::detail::PreviewThumbState::ready));
    check(foreignItem->icon().isNull()
            && foreignItem->data(ui::detail::kPreviewThumbStateRole).toInt()
                == static_cast<int>(ui::detail::PreviewThumbState::none),
        "preview thumbnail publication ignores null owners, null items, and items owned by another list");

    QListWidget pagedList{};
    pagedList.setViewMode(QListView::IconMode);
    pagedList.setFlow(QListView::LeftToRight);
    pagedList.setWrapping(true);
    pagedList.setResizeMode(QListView::Adjust);
    pagedList.setIconSize(QSize(48, 48));
    pagedList.setGridSize(QSize(80, 88));
    pagedList.resize(260, 190);
    for ( int row = 0; row < 30; ++row )
    {
        pagedList.addItem(QStringLiteral("paged_%1").arg(row));
    }
    pagedList.show();
    QCoreApplication::processEvents(QEventLoop::AllEvents);

    int firstBegin = -1;
    int firstEnd = -1;
    int firstProbeCount = 0;
    const bool firstRangeFound = ui::detail::visiblePreviewListRowRange(
        &pagedList,
        firstBegin,
        firstEnd,
        -20,
        &firstProbeCount);
    pagedList.scrollToItem(
        pagedList.item(pagedList.count() - 1),
        QAbstractItemView::PositionAtBottom);
    QCoreApplication::processEvents(QEventLoop::AllEvents);
    int finalBegin = -1;
    int finalEnd = -1;
    int finalProbeCount = 0;
    const bool finalRangeFound = ui::detail::visiblePreviewListRowRange(
        &pagedList,
        finalBegin,
        finalEnd,
        4,
        &finalProbeCount);
    check(firstRangeFound
            && firstBegin == 0
            && firstEnd >= firstBegin
            && firstEnd < pagedList.count()
            && finalRangeFound
            && finalBegin > firstBegin
            && finalEnd >= finalBegin
            && finalEnd < pagedList.count()
            && firstProbeCount <= 128
            && finalProbeCount <= 128,
        "preview visible-range helper finds bounded first and final icon pages without scanning the full model");

    int extremeInsetBegin = 8;
    int extremeInsetEnd = 9;
    int extremeInsetProbeCount = 0;
    const bool extremeInsetRangeFound =
        ui::detail::visiblePreviewListRowRange(
            &pagedList,
            extremeInsetBegin,
            extremeInsetEnd,
            10'000,
            &extremeInsetProbeCount);
    check(((extremeInsetRangeFound
                    && extremeInsetBegin >= 0
                    && extremeInsetEnd >= extremeInsetBegin
                    && extremeInsetEnd < pagedList.count())
                || (!extremeInsetRangeFound
                    && extremeInsetBegin == -1
                    && extremeInsetEnd == -1))
            && extremeInsetProbeCount <= 128,
        "preview visible-range helper handles an extreme inset with initialized, bounded outputs");
}

void runPreviewThumbnailOwnerCancellationTests()
{
    const int initialWorkerCount = ui::detail::previewWorkerCount();
    ui::detail::previewWorkerPool().waitForDone();
    ui::detail::setPreviewWorkerCount(1);

    QImage staleImage(8, 8, QImage::Format_ARGB32_Premultiplied);
    staleImage.fill(QColor(210, 80, 40));
    const QIcon staleIcon(QPixmap::fromImage(staleImage));
    const auto resources =
        std::make_shared<const assets::ResourceLocator>();

    QSemaphore destructionBlockerStarted{};
    QSemaphore releaseDestructionBlocker{};
    (void)ui::detail::runPreviewWorkerTask(
        [&destructionBlockerStarted, &releaseDestructionBlocker]()
        {
            destructionBlockerStarted.release();
            releaseDestructionBlocker.acquire();
        });
    const bool destructionBlockerReady =
        destructionBlockerStarted.tryAcquire(1, 5'000);
    check(destructionBlockerReady,
        "thumbnail owner-cancellation fixture occupies the sole preview worker before queuing owner work");

    QListWidget destructionList{};
    destructionList.addItem(QStringLiteral("original-owner-row"));
    destructionList.item(0)->setData(
        ui::detail::kPreviewThumbStateRole,
        static_cast<int>(ui::detail::PreviewThumbState::loading));
    int destructionCallbackCount = 0;
    bool destructionQueued = false;
    {
        // Hold the worker until both its QObject owner and the referenced
        // per-owner bookkeeping have left scope. This exercises the real
        // teardown boundary instead of keeping every capture alive until the
        // canceled future happens to finish.
        std::unordered_set<std::string> destructionInFlight{};
        int destructionActiveJobs = 0;
        auto owner = std::make_unique<QObject>();
        destructionQueued = destructionBlockerReady
            && ui::detail::queueSharedPreviewModelBoxPrep(
                owner.get(),
                resources,
                "owner-destroyed.3do",
                {},
                QSize(8, 8),
                false,
                destructionInFlight,
                destructionActiveJobs,
                [&](const bool current)
                {
                    if (!current)
                    {
                        return;
                    }
                    ++destructionCallbackCount;
                    ui::detail::publishPreviewThumbnailItem(
                        &destructionList,
                        destructionList.item(0),
                        staleIcon,
                        ui::detail::kPreviewThumbStateRole,
                        static_cast<int>(
                            ui::detail::PreviewThumbState::ready));
                });
        delete destructionList.takeItem(0);
        destructionList.addItem(QStringLiteral("reused-owner-row"));
        destructionList.item(0)->setData(
            ui::detail::kPreviewThumbStateRole,
            static_cast<int>(ui::detail::PreviewThumbState::loading));
        owner.reset();
        check(destructionActiveJobs == (destructionQueued ? 1 : 0)
                && destructionInFlight.size()
                    == (destructionQueued ? 1u : 0u),
            "destroyed thumbnail owner fixture retires its QObject before worker completion while its local bookkeeping is still verifiably live");
    }
    releaseDestructionBlocker.release();
    ui::detail::previewWorkerPool().waitForDone();
    QCoreApplication::sendPostedEvents(nullptr);
    QCoreApplication::sendPostedEvents(nullptr);
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    QCoreApplication::processEvents(QEventLoop::AllEvents);
    check(destructionQueued
            && destructionCallbackCount == 0
            && destructionList.item(0)
            && destructionList.item(0)->icon().isNull()
            && destructionList.item(0)->data(
                ui::detail::kPreviewThumbStateRole).toInt()
                == static_cast<int>(ui::detail::PreviewThumbState::loading),
        "destroying a thumbnail owner cancels its queued worker publication before a reused list row can receive stale output");

    QSemaphore generationBlockerStarted{};
    QSemaphore releaseGenerationBlocker{};
    (void)ui::detail::runPreviewWorkerTask(
        [&generationBlockerStarted, &releaseGenerationBlocker]()
        {
            generationBlockerStarted.release();
            releaseGenerationBlocker.acquire();
        });
    const bool generationBlockerReady =
        generationBlockerStarted.tryAcquire(1, 5'000);
    check(generationBlockerReady,
        "thumbnail generation-cancellation fixture occupies the sole preview worker before queuing stale work");

    QListWidget generationList{};
    generationList.addItem(QStringLiteral("original-generation-row"));
    generationList.item(0)->setData(
        ui::detail::kPreviewThumbStateRole,
        static_cast<int>(ui::detail::PreviewThumbState::loading));
    std::unordered_set<std::string> generationInFlight{};
    int generationActiveJobs = 0;
    int generationCallbackCount = 0;
    int generationWakeCount = 0;
    QObject generationOwner{};
    const std::uint64_t queuedGeneration =
        ui::detail::currentSharedPreviewCacheEpoch();
    const bool generationQueued = generationBlockerReady
        && ui::detail::queueSharedPreviewModelBoxPrep(
            &generationOwner,
            resources,
            "invalidated-generation.3do",
            {},
            QSize(8, 8),
            false,
            generationInFlight,
            generationActiveJobs,
            [&](const bool current)
            {
                ++generationWakeCount;
                if (!current)
                {
                    return;
                }
                ++generationCallbackCount;
                ui::detail::publishPreviewThumbnailItem(
                    &generationList,
                    generationList.item(0),
                    staleIcon,
                    ui::detail::kPreviewThumbStateRole,
                    static_cast<int>(ui::detail::PreviewThumbState::ready));
            });
    QPointer<QObject> generationWatcher = generationOwner.children().isEmpty()
        ? nullptr
        : generationOwner.children().constFirst();
    delete generationList.takeItem(0);
    generationList.addItem(QStringLiteral("reused-generation-row"));
    generationList.item(0)->setData(
        ui::detail::kPreviewThumbStateRole,
        static_cast<int>(ui::detail::PreviewThumbState::loading));
    int diskInvalidationCount = 0;
    ui::detail::PreviewResourceDiskWatcher diskInvalidationWatcher(
        nullptr,
        [&diskInvalidationCount]()
        {
            ui::detail::clearSharedPreviewAssetCaches();
            ++diskInvalidationCount;
        },
        0);
    diskInvalidationWatcher.notifyPathChangedForTest();
    diskInvalidationWatcher.flushPendingChangeForTest();
    const std::uint64_t currentGeneration =
        ui::detail::currentSharedPreviewCacheEpoch();
    releaseGenerationBlocker.release();
    ui::detail::previewWorkerPool().waitForDone();
    for ( int eventPass = 0;
          eventPass < 8 && generationActiveJobs > 0 && generationWatcher;
          ++eventPass )
    {
        // QFutureWatcher first consumes a private FutureCallOut event and may
        // only then emit finished. Target it directly so unrelated timers or
        // callbacks cannot determine whether this fixture settles.
        QCoreApplication::sendPostedEvents(generationWatcher.data());
        QCoreApplication::sendPostedEvents(generationWatcher.data());
    }
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    check(generationQueued
            && generationWatcher.isNull()
            && diskInvalidationCount == 1
            && currentGeneration != queuedGeneration
            && generationWakeCount == 1
            && generationCallbackCount == 0
            && generationActiveJobs == 0
            && generationInFlight.empty()
            && generationList.item(0)
            && generationList.item(0)->icon().isNull()
            && generationList.item(0)->data(
                ui::detail::kPreviewThumbStateRole).toInt()
                == static_cast<int>(ui::detail::PreviewThumbState::loading),
        "a picker-style disk watcher invalidation during thumbnail work wakes its live owner to requeue while retiring worker bookkeeping without publishing stale output to a reused row");

    ui::detail::setPreviewWorkerCount(initialWorkerCount);
}

void runExternalResourceFileChangeIntegrationTests()
{
    QTemporaryDir fixtureDir{};
    check(fixtureDir.isValid(),
        "resource file-change integration creates an isolated local resource store");
    if (!fixtureDir.isValid())
    {
        return;
    }

    const QString matDirectory = fixtureDir.filePath(QStringLiteral("mat"));
    const QString modelDirectory = fixtureDir.filePath(QStringLiteral("3do"));
    const QString spriteDirectory = fixtureDir.filePath(QStringLiteral("misc/spr"));
    check(QDir{}.mkpath(matDirectory)
            && QDir{}.mkpath(modelDirectory)
            && QDir{}.mkpath(spriteDirectory),
        "resource file-change integration creates MAT, 3DO, and SPR lookup directories");

    const QString modelPath = fixtureDir.filePath(
        QStringLiteral("3do/disk_change.3do"));
    const QString modelMaterialPath = fixtureDir.filePath(
        QStringLiteral("mat/disk_change.mat"));
    const QString discoveredModelMaterialPath = fixtureDir.filePath(
        QStringLiteral("mat/discovered_model.mat"));
    const QString spritePath = fixtureDir.filePath(
        QStringLiteral("misc/spr/disk_change.spr"));
    const QString firstSpriteMaterialPath = fixtureDir.filePath(
        QStringLiteral("mat/sprite_first.mat"));
    const QString secondSpriteMaterialPath = fixtureDir.filePath(
        QStringLiteral("mat/sprite_second.mat"));
    const QString classicModelPath = fixtureDir.filePath(
        QStringLiteral("3do/classic_change.3do"));
    const QString classicSpritePath = fixtureDir.filePath(
        QStringLiteral("misc/spr/classic_change.spr"));
    const QString classicMaterialPath = fixtureDir.filePath(
        QStringLiteral("mat/classic_change.mat"));
    const QString classicCmpPath = fixtureDir.filePath(
        QStringLiteral("classic_change.cmp"));
    const QString episodePath = fixtureDir.filePath(
        QStringLiteral("episode.jk"));
    const QString categoryHierarchyPath = fixtureDir.filePath(
        QStringLiteral("project_resources.jlh"));
    const QString categoryListPath = fixtureDir.filePath(
        QStringLiteral("project_resources.jll"));

    const auto baseRevision =
        std::filesystem::file_time_type::clock::now()
        - std::chrono::hours(24);
    const std::vector<QColor> redPixels(
        4u, QColor(230, 24, 18, 255));
    const std::vector<QColor> greenPixels(
        6u, QColor(18, 225, 36, 255));
    const std::vector<QColor> bluePixels(
        6u, QColor(20, 48, 232, 255));
    const std::vector<QColor> yellowPixels(
        8u, QColor(235, 205, 28, 255));

    const bool initialFilesWritten =
        writeFixtureWithRevision(
            modelPath,
            makeDiskChangeModelFixture(
                QByteArrayLiteral("disk_change.mat"),
                QByteArrayLiteral("initial"),
                1.0),
            baseRevision + std::chrono::seconds(1))
        && writeFixtureWithRevision(
            modelMaterialPath,
            makeRgbaMatFixture(2, 2, redPixels),
            baseRevision + std::chrono::seconds(2))
        && writeFixtureWithRevision(
            spritePath,
            QByteArrayLiteral(
                "sprite_first.mat 0 2.0 1.0 4 3 3 0.0 0.0 0.0 0.0\n"),
            baseRevision + std::chrono::seconds(3))
        && writeFixtureWithRevision(
            firstSpriteMaterialPath,
            makeRgbaMatFixture(2, 2, redPixels),
            baseRevision + std::chrono::seconds(4))
        && writeFixtureWithRevision(
            secondSpriteMaterialPath,
            makeRgbaMatFixture(3, 2, bluePixels),
            baseRevision + std::chrono::seconds(5))
        && writeFixtureWithRevision(
            discoveredModelMaterialPath,
            makeRgbaMatFixture(3, 2, bluePixels),
            baseRevision + std::chrono::seconds(6))
        && writeFixtureWithRevision(
            classicModelPath,
            makeClassicDiskChangeModelFixture(
                QByteArrayLiteral("classic_change.mat"),
                QByteArrayLiteral("classic indexed model"),
                1.5),
            baseRevision + std::chrono::seconds(7))
        && writeFixtureWithRevision(
            classicSpritePath,
            QByteArrayLiteral(
                "classic_change.mat 0 2.0 1.0 4 3 3 0.0 0.0 0.0 0.0\n"),
            baseRevision + std::chrono::seconds(8))
        && writeFixtureWithRevision(
            classicMaterialPath,
            makeIndexedMatFixture(6u),
            baseRevision + std::chrono::seconds(9))
        && writeFixtureWithRevision(
            classicCmpPath,
            makeClassicCmpFixture(6u, QColor(255, 0, 0)),
            baseRevision + std::chrono::seconds(10))
        && writeFixtureWithRevision(
            episodePath,
            QByteArrayLiteral(
                "\"Disk Episode One\"\n"
                "TYPE 1\n"
                "SEQ 1\n"
                "10: 0 1 LEVEL first.jkl 0 0 -1 -1\n"
                "END\n"),
            baseRevision + std::chrono::seconds(11))
        && writeFixtureWithRevision(
            categoryHierarchyPath,
            QByteArrayLiteral(
                "SUB Models\n"
                "LIST Actors Actors\n"
                "SUBEND\n"),
            baseRevision + std::chrono::seconds(12))
        && writeFixtureWithRevision(
            categoryListPath,
            QByteArrayLiteral(
                "[Actors]\n"
                "first_actor.3do\n"),
            baseRevision + std::chrono::seconds(13));
    check(initialFilesWritten,
        "resource file-change integration writes deterministic local RGBA and indexed MAT, 3DO, SPR, CMP, episode, and category fixtures");
    if (!initialFilesWritten)
    {
        return;
    }

    assets::ResourceLocator resources{};
    resources.addRoot(
        std::filesystem::path(fixtureDir.path().toStdWString()));
    ui::detail::clearSharedPreviewAssetCaches();

    const std::shared_ptr<const assets::Model3do> initialModel =
        ui::detail::loadSharedPreviewModel(&resources, "disk_change.3do");
    const std::optional<ThingBox> initialBox = initialModel
        ? ui::detail::previewBoxForModel(*initialModel)
        : std::nullopt;
    const std::optional<ui::detail::PreviewMaterialTextureSource>
        initialModelMaterial = ui::detail::loadPreviewMaterialTextureSource(
            &resources, {}, "disk_change.mat");
    const std::uint64_t initialEpoch =
        ui::detail::currentSharedPreviewCacheEpoch();
    const QSize renderSize(112, 88);
    const bool initialImageStored =
        ui::detail::renderAndStoreSharedModelPreviewImage(
            &resources,
            {},
            "disk_change.3do",
            renderSize,
            false,
            initialEpoch);
    const std::string initialImageKey =
        ui::detail::sharedModelPreviewIconCacheKey(
            &resources,
            {},
            "disk_change.3do",
            renderSize,
            false);
    const std::optional<QImage> initialModelImage =
        ui::detail::findSharedTemplateModelPreviewImage(initialImageKey);
    const std::shared_ptr<const assets::SprFile> initialSprite =
        ui::detail::loadSharedPreviewSprite(&resources, "disk_change.spr");
    const std::optional<QImage> initialSpriteImage =
        ui::detail::loadSharedPreviewSpriteImage(
            &resources, {}, "disk_change.spr");
    check(initialModel
            && initialModel->materials
                == std::vector<std::string>{ "disk_change.mat" }
            && initialBox
            && close(initialBox->x1, -1.0)
            && close(initialBox->x2, 1.0)
            && initialModelMaterial
            && initialModelMaterial->image.size() == QSize(2, 2)
            && initialModelMaterial->image.pixelColor(0, 0)
                == redPixels.front()
            && initialImageStored
            && initialModelImage
            && dominantPixelCount(*initialModelImage, QColor(255, 0, 0)) > 40u,
        "resource file-change integration publishes initial 3DO, MAT, bounds, and renderer-facing thumbnail data");
    check(initialSprite
            && initialSprite->material == "sprite_first.mat"
            && initialSpriteImage
            && initialSpriteImage->size() == QSize(2, 2)
            && initialSpriteImage->pixelColor(0, 0) == redPixels.front(),
        "resource file-change integration publishes initial SPR and referenced-MAT pixels to the preview consumer");

    int fileChangePublications = 0;
    ui::detail::PreviewResourceDiskWatcher watcher(
        nullptr,
        [&fileChangePublications]()
        {
            ui::detail::clearSharedPreviewAssetCaches();
            ++fileChangePublications;
        },
        0);
    watcher.setResourceLocator(&resources);
    watcher.watchResource("disk_change.3do", ".3do");
    watcher.watchResource("disk_change.mat", ".mat");
    watcher.watchResource("disk_change.spr", ".spr");
    watcher.watchResource("sprite_first.mat", ".mat");
    watcher.watchResource("classic_change.3do", ".3do");
    watcher.watchResource("classic_change.spr", ".spr");
    watcher.watchResource("classic_change.mat", ".mat");
    watcher.watchResource("classic_change.cmp", ".cmp");
    watcher.syncNow();
    const std::size_t initialDependencyCount =
        watcher.dependencyCountForTest();
    const auto registerDiscoveredModelDependencies =
        [&watcher, &resources](const std::string_view modelName)
    {
        watcher.watchResource(modelName, ".3do");
        std::shared_ptr<const assets::Model3do> cachedModel{};
        if (ui::detail::findCachedSharedPreviewModel(
                &resources, modelName, cachedModel)
            && cachedModel)
        {
            for (const std::string& material : cachedModel->materials)
            {
                watcher.watchResource(material, ".mat");
            }
        }
        watcher.syncNow(false);
    };
    const auto registerDiscoveredSpriteDependencies =
        [&watcher, &resources](const std::string_view spriteName)
    {
        watcher.watchResource(spriteName, ".spr");
        if (const std::shared_ptr<const assets::SprFile> sprite =
                ui::detail::loadSharedPreviewSprite(
                    &resources, spriteName))
        {
            watcher.watchResource(sprite->material, ".mat");
        }
        watcher.syncNow(false);
    };

    const std::string initialModelMaterialRevision =
        ui::detail::previewResourceRevisionToken(
            &resources, "disk_change.mat", ".mat");
    const QByteArray replacementModelMaterial =
        makeRgbaMatFixture(3, 2, greenPixels);
    check(replacementModelMaterial.size()
            != makeRgbaMatFixture(2, 2, redPixels).size()
            && writeFixtureWithRevision(
                modelMaterialPath,
                replacementModelMaterial,
                baseRevision + std::chrono::seconds(20)),
        "resource file-change integration externally replaces a MAT at the same path with changed timestamp, size, and content");
    watcher.notifyDirectoryPathChangedForTest();
    watcher.flushPendingChangeForTest();

    const std::string replacementModelMaterialRevision =
        ui::detail::previewResourceRevisionToken(
            &resources, "disk_change.mat", ".mat");
    const std::shared_ptr<const assets::Model3do> modelAfterMaterialChange =
        ui::detail::loadSharedPreviewModel(&resources, "disk_change.3do");
    const std::optional<ui::detail::PreviewMaterialTextureSource>
        replacementModelMaterialSource =
            ui::detail::loadPreviewMaterialTextureSource(
                &resources, {}, "disk_change.mat");
    const std::uint64_t materialReplacementEpoch =
        ui::detail::currentSharedPreviewCacheEpoch();
    const bool replacementModelImageStored =
        ui::detail::renderAndStoreSharedModelPreviewImage(
            &resources,
            {},
            "disk_change.3do",
            renderSize,
            false,
            materialReplacementEpoch);
    const std::string replacementModelImageKey =
        ui::detail::sharedModelPreviewIconCacheKey(
            &resources,
            {},
            "disk_change.3do",
            renderSize,
            false);
    const std::optional<QImage> replacementModelImage =
        ui::detail::findSharedTemplateModelPreviewImage(
            replacementModelImageKey);
    check(fileChangePublications == 1
            && replacementModelMaterialRevision
                != initialModelMaterialRevision
            && materialReplacementEpoch != initialEpoch
            && !ui::detail::findSharedTemplateModelPreviewImage(
                initialImageKey)
            && modelAfterMaterialChange
            && modelAfterMaterialChange != initialModel
            && initialModel
            && initialModel->materials
                == std::vector<std::string>{ "disk_change.mat" }
            && replacementModelMaterialSource
            && replacementModelMaterialSource->image.size() == QSize(3, 2)
            && replacementModelMaterialSource->image.pixelColor(0, 0)
                == greenPixels.front()
            && replacementModelImageStored
            && replacementModelImage
            && dominantPixelCount(*replacementModelImage, QColor(0, 255, 0))
                > 40u,
        "an external MAT replacement invalidates parsed dependencies and republishes changed pixels through the 3DO renderer-facing cache");

    const std::string modelRevisionBeforeReplacement =
        ui::detail::previewResourceRevisionToken(
            &resources, "disk_change.3do", ".3do");
    const QByteArray replacementModel = makeDiskChangeModelFixture(
        QByteArrayLiteral("discovered_model.mat"),
        QByteArrayLiteral("replacement model with wider bounds"),
        2.5);
    check(writeFixtureWithRevision(
              modelPath,
              replacementModel,
              baseRevision + std::chrono::seconds(30)),
        "resource file-change integration externally replaces a 3DO at the same path");
    watcher.notifyDirectoryPathChangedForTest();
    watcher.flushPendingChangeForTest();

    const std::shared_ptr<const assets::Model3do> replacementParsedModel =
        ui::detail::loadSharedPreviewModel(&resources, "disk_change.3do");
    const std::optional<ThingBox> replacementBox = replacementParsedModel
        ? ui::detail::loadSharedPreviewModelBox(
            &resources, "disk_change.3do")
        : std::nullopt;
    const std::uint64_t modelReplacementEpoch =
        ui::detail::currentSharedPreviewCacheEpoch();
    const bool modelReplacementImageStored =
        ui::detail::renderAndStoreSharedModelPreviewImage(
            &resources,
            {},
            "disk_change.3do",
            renderSize,
            false,
            modelReplacementEpoch);
    const std::string modelReplacementImageKey =
        ui::detail::sharedModelPreviewIconCacheKey(
            &resources,
            {},
            "disk_change.3do",
            renderSize,
            false);
    const std::optional<QImage> modelReplacementImage =
        ui::detail::findSharedTemplateModelPreviewImage(
            modelReplacementImageKey);
    check(fileChangePublications == 2
            && ui::detail::previewResourceRevisionToken(
                   &resources, "disk_change.3do", ".3do")
                != modelRevisionBeforeReplacement
            && replacementParsedModel
            && replacementParsedModel != modelAfterMaterialChange
            && replacementParsedModel->materials
                == std::vector<std::string>{ "discovered_model.mat" }
            && replacementBox
            && close(replacementBox->x1, -2.5)
            && close(replacementBox->x2, 2.5)
            && initialBox
            && close(initialBox->x1, -1.0)
            && close(initialBox->x2, 1.0)
            && modelReplacementImageStored
            && modelReplacementImage
            && replacementModelImage
            && imagePixelChecksum(*modelReplacementImage)
                != imagePixelChecksum(*replacementModelImage)
            && blueDominantPixelCount(*modelReplacementImage) > 40u
            && dominantPixelCount(
                   *modelReplacementImage, QColor(0, 255, 0)) == 0u,
        "an external 3DO replacement republishes new geometry, bounds, dependency pixels, and a changed renderer-facing image while existing shared owners remain stable");

    registerDiscoveredModelDependencies("disk_change.3do");
    check(watcher.dependencyCountForTest() == initialDependencyCount + 1u,
        "the production dependency-registration sequence adds a newly discovered 3DO MAT that was not pre-watched");

    check(writeFixtureWithRevision(
              discoveredModelMaterialPath,
              makeRgbaMatFixture(2, 2, redPixels),
              baseRevision + std::chrono::seconds(35)),
        "resource file-change integration externally changes the newly discovered 3DO MAT");
    watcher.notifyDirectoryPathChangedForTest();
    watcher.flushPendingChangeForTest();

    const std::shared_ptr<const assets::Model3do> modelAfterDiscoveredMaterialChange =
        ui::detail::loadSharedPreviewModel(&resources, "disk_change.3do");
    const std::optional<ui::detail::PreviewMaterialTextureSource>
        discoveredModelMaterial =
            ui::detail::loadPreviewMaterialTextureSource(
                &resources, {}, "discovered_model.mat");
    const std::uint64_t discoveredMaterialEpoch =
        ui::detail::currentSharedPreviewCacheEpoch();
    const bool discoveredMaterialImageStored =
        ui::detail::renderAndStoreSharedModelPreviewImage(
            &resources,
            {},
            "disk_change.3do",
            renderSize,
            false,
            discoveredMaterialEpoch);
    const std::string discoveredMaterialImageKey =
        ui::detail::sharedModelPreviewIconCacheKey(
            &resources,
            {},
            "disk_change.3do",
            renderSize,
            false);
    const std::optional<QImage> discoveredMaterialImage =
        ui::detail::findSharedTemplateModelPreviewImage(
            discoveredMaterialImageKey);
    check(fileChangePublications == 3
            && modelAfterDiscoveredMaterialChange
            && modelAfterDiscoveredMaterialChange != replacementParsedModel
            && discoveredModelMaterial
            && discoveredModelMaterial->image.pixelColor(0, 0)
                == redPixels.front()
            && discoveredMaterialImageStored
            && discoveredMaterialImage
            && modelReplacementImage
            && imagePixelChecksum(*discoveredMaterialImage)
                != imagePixelChecksum(*modelReplacementImage)
            && dominantPixelCount(
                   *discoveredMaterialImage, QColor(255, 0, 0)) > 40u
            && blueDominantPixelCount(*discoveredMaterialImage) == 0u,
        "a later external change to the dynamically registered 3DO MAT invalidates dependencies and republishes only the new pixels");

    const std::uint64_t staleModelEpoch =
        ui::detail::currentSharedPreviewCacheEpoch();
    const std::string modelRevisionBeforeRemoval =
        ui::detail::previewResourceRevisionToken(
            &resources, "disk_change.3do", ".3do");
    // 3DO syntax parsing intentionally accepts incomplete text as a partial
    // model. External removal exercises the deterministic supported load-
    // failure path without relying on an unsafe allocation-failure fixture.
    check(QFile::remove(modelPath) && !QFile::exists(modelPath),
        "resource file-change integration externally removes the 3DO at the same path");
    watcher.notifyDirectoryPathChangedForTest();
    watcher.flushPendingChangeForTest();

    const std::uint64_t unavailableModelEpoch =
        ui::detail::currentSharedPreviewCacheEpoch();
    const std::string unavailableModelRevision =
        ui::detail::previewResourceRevisionToken(
            &resources, "disk_change.3do", ".3do");
    check(fileChangePublications == 4
            && unavailableModelEpoch != staleModelEpoch
            && unavailableModelRevision != modelRevisionBeforeRemoval,
        "external 3DO removal advances the watcher publication, preview epoch, and resource revision");

    const std::shared_ptr<const assets::Model3do> unavailableModel =
        ui::detail::loadSharedPreviewModel(&resources, "disk_change.3do");
    const std::optional<ThingBox> unavailableBox =
        ui::detail::loadSharedPreviewModelBox(
            &resources, "disk_change.3do");
    check(!unavailableModel,
        "an unavailable changed 3DO publishes no parsed model");
    check(!unavailableBox,
        "an unavailable changed 3DO publishes no partial bounds");

    const bool unavailableImageStored =
        ui::detail::renderAndStoreSharedModelPreviewImage(
            &resources,
            {},
            "disk_change.3do",
            renderSize,
            false,
            unavailableModelEpoch);
    const std::string unavailableImageKey =
        ui::detail::sharedModelPreviewIconCacheKey(
            &resources,
            {},
            "disk_change.3do",
            renderSize,
            false);
    const std::optional<QImage> unavailableImage =
        ui::detail::findSharedTemplateModelPreviewImage(
            unavailableImageKey);
    check(!unavailableImageStored && !unavailableImage,
        "an unavailable changed 3DO publishes no stale or partial renderer-facing thumbnail");

    QImage staleModelImage(renderSize, QImage::Format_ARGB32_Premultiplied);
    staleModelImage.fill(QColor(250, 15, 180));
    const bool staleImagePublished =
        ui::detail::storeAuthoritativeSharedModelPreviewImage(
            &resources,
            {},
            "disk_change.3do",
            renderSize,
            false,
            staleModelEpoch,
            staleModelImage);
    check(!staleImagePublished
            && !ui::detail::findSharedTemplateModelPreviewImage(
                unavailableImageKey),
        "a renderer result from before external 3DO removal is rejected without publishing stale pixels");

    check(modelAfterDiscoveredMaterialChange
            && modelAfterDiscoveredMaterialChange->meshes.size() == 1u
            && replacementBox
            && close(replacementBox->x1, -2.5)
            && close(replacementBox->x2, 2.5),
        "external 3DO removal leaves the previously shared model owner and its geometry stable");

    check(writeFixtureWithRevision(
              modelPath,
              makeDiskChangeModelFixture(
                  QByteArrayLiteral("disk_change.mat"),
                  QByteArrayLiteral("restored after external removal"),
                  3.0),
              baseRevision + std::chrono::seconds(50)),
        "resource file-change integration restores the unavailable 3DO at the same path");
    watcher.notifyDirectoryPathChangedForTest();
    watcher.flushPendingChangeForTest();

    const std::shared_ptr<const assets::Model3do> repairedModel =
        ui::detail::loadSharedPreviewModel(&resources, "disk_change.3do");
    const std::optional<ThingBox> repairedBox = repairedModel
        ? ui::detail::loadSharedPreviewModelBox(
            &resources, "disk_change.3do")
        : std::nullopt;
    const bool repairedImageStored =
        ui::detail::renderAndStoreSharedModelPreviewImage(
            &resources,
            {},
            "disk_change.3do",
            renderSize,
            false,
            ui::detail::currentSharedPreviewCacheEpoch());
    const std::string repairedImageKey =
        ui::detail::sharedModelPreviewIconCacheKey(
            &resources,
            {},
            "disk_change.3do",
            renderSize,
            false);
    const std::optional<QImage> repairedImage =
        ui::detail::findSharedTemplateModelPreviewImage(repairedImageKey);
    check(fileChangePublications == 5
            && repairedModel
            && repairedBox
            && close(repairedBox->x1, -3.0)
            && close(repairedBox->x2, 3.0)
            && repairedImageStored
            && repairedImage
            && discoveredMaterialImage
            && imagePixelChecksum(*repairedImage)
                != imagePixelChecksum(*discoveredMaterialImage)
            && dominantPixelCount(*repairedImage, QColor(0, 255, 0)) > 40u
            && dominantPixelCount(
                   *repairedImage, QColor(255, 0, 0)) == 0u,
        "a restored 3DO recovers parsing, bounds, and a changed renderer-facing image without restarting the resource store");

    const std::string spriteDependenciesBeforeReplacement =
        ui::detail::sharedPreviewSpriteDependencyRevisionToken(
            &resources, {}, "disk_change.spr");
    check(writeFixtureWithRevision(
              spritePath,
              QByteArrayLiteral(
                  "sprite_second.mat 0 3.0 2.0 4 3 3 0.25 0.0 0.0 0.0\n# replacement sprite revision\n"),
              baseRevision + std::chrono::seconds(60)),
        "resource file-change integration externally replaces an SPR with a different MAT dependency");
    watcher.notifyDirectoryPathChangedForTest();
    watcher.flushPendingChangeForTest();

    const std::shared_ptr<const assets::SprFile> replacementSprite =
        ui::detail::loadSharedPreviewSprite(&resources, "disk_change.spr");
    const std::optional<QImage> replacementSpriteImage =
        ui::detail::loadSharedPreviewSpriteImage(
            &resources, {}, "disk_change.spr");
    check(fileChangePublications == 6
            && ui::detail::sharedPreviewSpriteDependencyRevisionToken(
                   &resources, {}, "disk_change.spr")
                != spriteDependenciesBeforeReplacement
            && replacementSprite
            && replacementSprite != initialSprite
            && replacementSprite->material == "sprite_second.mat"
            && replacementSpriteImage
            && replacementSpriteImage->size() == QSize(3, 2)
            && replacementSpriteImage->pixelColor(0, 0)
                == bluePixels.front()
            && initialSprite
            && initialSprite->material == "sprite_first.mat",
        "an external SPR replacement invalidates its parsed resource and republishes pixels from the new MAT dependency");

    registerDiscoveredSpriteDependencies("disk_change.spr");
    check(watcher.dependencyCountForTest() == initialDependencyCount + 2u,
        "the production dependency-registration sequence adds a newly discovered SPR MAT that was not pre-watched");

    check(writeFixtureWithRevision(
              secondSpriteMaterialPath,
              makeRgbaMatFixture(4, 2, yellowPixels),
              baseRevision + std::chrono::seconds(65)),
        "resource file-change integration externally changes the newly discovered SPR MAT");
    watcher.notifyDirectoryPathChangedForTest();
    watcher.flushPendingChangeForTest();
    const std::shared_ptr<const assets::SprFile> spriteAfterDiscoveredMaterialChange =
        ui::detail::loadSharedPreviewSprite(&resources, "disk_change.spr");
    const std::optional<QImage> discoveredSpriteMaterialImage =
        ui::detail::loadSharedPreviewSpriteImage(
            &resources, {}, "disk_change.spr");
    check(fileChangePublications == 7
            && spriteAfterDiscoveredMaterialChange
            && spriteAfterDiscoveredMaterialChange != replacementSprite
            && spriteAfterDiscoveredMaterialChange->material
                == "sprite_second.mat"
            && discoveredSpriteMaterialImage
            && replacementSpriteImage
            && imagePixelChecksum(*discoveredSpriteMaterialImage)
                != imagePixelChecksum(*replacementSpriteImage)
            && discoveredSpriteMaterialImage->size() == QSize(4, 2)
            && discoveredSpriteMaterialImage->pixelColor(0, 0)
                == yellowPixels.front()
            && discoveredSpriteMaterialImage->pixelColor(0, 0)
                != bluePixels.front(),
        "a later external change to the dynamically registered SPR MAT invalidates dependencies and republishes only the new pixels");

    check(writeFixtureWithRevision(
              secondSpriteMaterialPath,
              QByteArrayLiteral("malformed MAT replacement\n"),
              baseRevision + std::chrono::seconds(70)),
        "resource file-change integration writes a malformed MAT used by an SPR");
    watcher.notifyDirectoryPathChangedForTest();
    watcher.flushPendingChangeForTest();
    const std::shared_ptr<const assets::SprFile> spriteWithMalformedMaterial =
        ui::detail::loadSharedPreviewSprite(&resources, "disk_change.spr");
    const std::optional<QImage> malformedSpriteImage =
        ui::detail::loadSharedPreviewSpriteImage(
            &resources, {}, "disk_change.spr");
    check(fileChangePublications == 8
            && spriteWithMalformedMaterial
            && spriteWithMalformedMaterial->material == "sprite_second.mat"
            && !malformedSpriteImage,
        "a malformed changed SPR material keeps valid SPR metadata but publishes no stale or partial decoded image");

    check(writeFixtureWithRevision(
              secondSpriteMaterialPath,
              makeRgbaMatFixture(4, 2, yellowPixels),
              baseRevision + std::chrono::seconds(80)),
        "resource file-change integration repairs the changed SPR material");
    watcher.notifyDirectoryPathChangedForTest();
    watcher.flushPendingChangeForTest();
    const std::optional<QImage> repairedSpriteImage =
        ui::detail::loadSharedPreviewSpriteImage(
            &resources, {}, "disk_change.spr");
    check(fileChangePublications == 9
            && repairedSpriteImage
            && repairedSpriteImage->size() == QSize(4, 2)
            && repairedSpriteImage->pixelColor(0, 0)
                == yellowPixels.front(),
        "a corrected SPR material is re-decoded and reaches the consumer after a prior cached failure");

    check(writeFixtureWithRevision(
              spritePath,
              QByteArrayLiteral("malformed SPR replacement\n"),
              baseRevision + std::chrono::seconds(85)),
        "resource file-change integration writes a malformed SPR replacement");
    watcher.notifyDirectoryPathChangedForTest();
    watcher.flushPendingChangeForTest();
    const std::shared_ptr<const assets::SprFile> malformedSprite =
        ui::detail::loadSharedPreviewSprite(&resources, "disk_change.spr");
    const std::optional<QImage> imageFromMalformedSprite =
        ui::detail::loadSharedPreviewSpriteImage(
            &resources, {}, "disk_change.spr");
    check(fileChangePublications == 10
            && !malformedSprite
            && !imageFromMalformedSprite
            && replacementSprite
            && replacementSprite->material == "sprite_second.mat",
        "a malformed on-disk SPR replacement publishes neither stale parsed metadata nor stale decoded pixels");

    check(writeFixtureWithRevision(
              spritePath,
              QByteArrayLiteral(
                  "sprite_second.mat 0 4.0 2.0 4 3 3 0.5 0.0 0.0 0.0\n"
                  "# repaired sprite revision\n"),
              baseRevision + std::chrono::seconds(86)),
        "resource file-change integration repairs the malformed SPR at the same path");
    watcher.notifyDirectoryPathChangedForTest();
    watcher.flushPendingChangeForTest();
    const std::shared_ptr<const assets::SprFile> repairedSprite =
        ui::detail::loadSharedPreviewSprite(&resources, "disk_change.spr");
    const std::optional<QImage> imageFromRepairedSprite =
        ui::detail::loadSharedPreviewSpriteImage(
            &resources, {}, "disk_change.spr");
    check(fileChangePublications == 11
            && repairedSprite
            && repairedSprite->material == "sprite_second.mat"
            && close(repairedSprite->width, 4.0)
            && imageFromRepairedSprite
            && imageFromRepairedSprite->size() == QSize(4, 2)
            && imageFromRepairedSprite->pixelColor(0, 0)
                == yellowPixels.front(),
        "a corrected SPR replacement recovers parsed metadata and dependent MAT pixels without restarting the resource store");

    const std::uint64_t beforeExactReplacementEpoch =
        ui::detail::currentSharedPreviewCacheEpoch();
    const bool beforeExactReplacementStored =
        ui::detail::loadSharedPreviewModel(
            &resources, "disk_change.3do")
        && ui::detail::renderAndStoreSharedModelPreviewImage(
            &resources,
            {},
            "disk_change.3do",
            renderSize,
            false,
            beforeExactReplacementEpoch);
    const std::string beforeExactReplacementImageKey =
        ui::detail::sharedModelPreviewIconCacheKey(
            &resources,
            {},
            "disk_change.3do",
            renderSize,
            false);
    const std::optional<QImage> beforeExactReplacementImage =
        ui::detail::findSharedTemplateModelPreviewImage(
            beforeExactReplacementImageKey);
    check(beforeExactReplacementStored
            && beforeExactReplacementImage
            && dominantPixelCount(
                   *beforeExactReplacementImage, QColor(0, 255, 0)) > 40u,
        "resource file-change integration stages the live 3DO renderer result before an exact-metadata MAT replacement");

    const std::string exactReplacementRevision =
        ui::detail::previewResourceRevisionToken(
            &resources, "disk_change.mat", ".mat");
    std::error_code exactTimestampError{};
    const auto exactTimestamp = std::filesystem::last_write_time(
        std::filesystem::path(modelMaterialPath.toStdWString()),
        exactTimestampError);
    const QByteArray exactReplacementMat =
        makeRgbaMatFixture(3, 2, bluePixels);
    check(!exactTimestampError
            && exactReplacementMat.size() == replacementModelMaterial.size()
            && writeFixtureWithRevision(
                modelMaterialPath,
                exactReplacementMat,
                exactTimestamp)
            && ui::detail::previewResourceRevisionToken(
                   &resources, "disk_change.mat", ".mat")
                == exactReplacementRevision,
        "resource file-change integration externally replaces a MAT while deliberately retaining its path, size, and timestamp metadata");
    watcher.notifyPathChangedForTest();
    watcher.flushPendingChangeForTest();
    const std::optional<ui::detail::PreviewMaterialTextureSource>
        exactReplacementMaterialSource =
            ui::detail::loadPreviewMaterialTextureSource(
                &resources, {}, "disk_change.mat");
    const std::uint64_t exactReplacementEpoch =
        ui::detail::currentSharedPreviewCacheEpoch();
    const bool exactReplacementImageStored =
        ui::detail::loadSharedPreviewModel(
            &resources, "disk_change.3do")
        && ui::detail::renderAndStoreSharedModelPreviewImage(
            &resources,
            {},
            "disk_change.3do",
            renderSize,
            false,
            exactReplacementEpoch);
    const std::string exactReplacementImageKey =
        ui::detail::sharedModelPreviewIconCacheKey(
            &resources,
            {},
            "disk_change.3do",
            renderSize,
            false);
    const std::optional<QImage> exactReplacementImage =
        ui::detail::findSharedTemplateModelPreviewImage(
            exactReplacementImageKey);
    check(fileChangePublications == 12
            && exactReplacementEpoch != beforeExactReplacementEpoch
            && exactReplacementMaterialSource
            && exactReplacementMaterialSource->image.pixelColor(0, 0)
                == bluePixels.front()
            && !ui::detail::findSharedTemplateModelPreviewImage(
                beforeExactReplacementImageKey)
            && exactReplacementImageStored
            && exactReplacementImage
            && beforeExactReplacementImage
            && imagePixelChecksum(*exactReplacementImage)
                != imagePixelChecksum(*beforeExactReplacementImage)
            && blueDominantPixelCount(*exactReplacementImage) > 40u
            && dominantPixelCount(
                   *exactReplacementImage, QColor(0, 255, 0)) == 0u,
        "an exact-file notification advances the cache epoch and republishes changed MAT content through the 3DO renderer even when metadata revision tokens are identical");

    const std::shared_ptr<const assets::Model3do> initialClassicModel =
        ui::detail::loadSharedPreviewModel(
            &resources, "classic_change.3do");
    const std::optional<ui::detail::PreviewMaterialTextureSource>
        initialClassicMaterial =
            ui::detail::loadPreviewMaterialTextureSource(
                &resources,
                "classic_change.cmp",
                "classic_change.mat");
    const std::shared_ptr<const assets::SprFile> initialClassicSprite =
        ui::detail::loadSharedPreviewSprite(
            &resources, "classic_change.spr");
    const std::optional<QImage> initialClassicSpriteImage =
        ui::detail::loadSharedPreviewSpriteImage(
            &resources,
            "classic_change.cmp",
            "classic_change.spr");
    const std::uint64_t initialClassicEpoch =
        ui::detail::currentSharedPreviewCacheEpoch();
    const bool initialClassicModelImageStored = initialClassicModel
        && ui::detail::renderAndStoreSharedModelPreviewImage(
            &resources,
            "classic_change.cmp",
            "classic_change.3do",
            renderSize,
            false,
            initialClassicEpoch);
    const std::string initialClassicModelImageKey =
        ui::detail::sharedModelPreviewIconCacheKey(
            &resources,
            "classic_change.cmp",
            "classic_change.3do",
            renderSize,
            false);
    const std::optional<QImage> initialClassicModelImage =
        ui::detail::findSharedTemplateModelPreviewImage(
            initialClassicModelImageKey);
    check(initialClassicModel
            && close(initialClassicModel->version, 2.1)
            && initialClassicModel->materials
                == std::vector<std::string>{ "classic_change.mat" }
            && initialClassicMaterial
            && initialClassicMaterial->image.pixelColor(0, 0)
                == QColor(255, 0, 0)
            && initialClassicSprite
            && initialClassicSprite->material == "classic_change.mat"
            && initialClassicSpriteImage
            && initialClassicSpriteImage->pixelColor(0, 0)
                == QColor(255, 0, 0)
            && initialClassicModelImageStored
            && initialClassicModelImage
            && dominantPixelCount(
                   *initialClassicModelImage, QColor(255, 0, 0)) > 40u,
        "classic 3DO 2.1 and SPR previews initially decode their indexed MAT through the watched CMP");

    check(writeFixtureWithRevision(
              classicCmpPath,
              makeClassicCmpFixture(6u, QColor(0, 255, 0)),
              baseRevision + std::chrono::seconds(130)),
        "resource file-change integration externally replaces only the classic CMP palette");
    watcher.notifyDirectoryPathChangedForTest();
    watcher.flushPendingChangeForTest();

    const std::optional<ui::detail::PreviewMaterialTextureSource>
        changedClassicMaterial =
            ui::detail::loadPreviewMaterialTextureSource(
                &resources,
                "classic_change.cmp",
                "classic_change.mat");
    const std::optional<QImage> changedClassicSpriteImage =
        ui::detail::loadSharedPreviewSpriteImage(
            &resources,
            "classic_change.cmp",
            "classic_change.spr");
    const std::uint64_t changedClassicEpoch =
        ui::detail::currentSharedPreviewCacheEpoch();
    const bool changedClassicModelImageStored =
        ui::detail::loadSharedPreviewModel(
            &resources, "classic_change.3do")
        && ui::detail::renderAndStoreSharedModelPreviewImage(
            &resources,
            "classic_change.cmp",
            "classic_change.3do",
            renderSize,
            false,
            changedClassicEpoch);
    const std::string changedClassicModelImageKey =
        ui::detail::sharedModelPreviewIconCacheKey(
            &resources,
            "classic_change.cmp",
            "classic_change.3do",
            renderSize,
            false);
    const std::optional<QImage> changedClassicModelImage =
        ui::detail::findSharedTemplateModelPreviewImage(
            changedClassicModelImageKey);
    check(fileChangePublications == 13
            && changedClassicEpoch != initialClassicEpoch
            && changedClassicMaterial
            && changedClassicMaterial->image.pixelColor(0, 0)
                == QColor(0, 255, 0)
            && changedClassicSpriteImage
            && initialClassicSpriteImage
            && changedClassicSpriteImage->pixelColor(0, 0)
                == QColor(0, 255, 0)
            && changedClassicSpriteImage->pixelColor(0, 0)
                != QColor(255, 0, 0)
            && imagePixelChecksum(*changedClassicSpriteImage)
                != imagePixelChecksum(*initialClassicSpriteImage)
            && changedClassicModelImageStored
            && changedClassicModelImage
            && initialClassicModelImage
            && imagePixelChecksum(*changedClassicModelImage)
                != imagePixelChecksum(*initialClassicModelImage)
            && dominantPixelCount(
                   *changedClassicModelImage, QColor(0, 255, 0)) > 40u
            && dominantPixelCount(
                   *changedClassicModelImage, QColor(255, 0, 0)) == 0u,
        "a CMP-only external replacement republishes changed indexed pixels through both classic SPR decoding and the 3DO renderer-facing cache");

    const auto loadEpisodeFromResources = [&resources]()
    {
        const std::string text = resources.readText("episode.jk");
        std::istringstream input(text);
        return assets::loadFromEpisodeJk(input);
    };
    assets::EpisodeFile publishedEpisode = loadEpisodeFromResources();
    check(publishedEpisode.name == "Disk Episode One"
            && publishedEpisode.sequences.size() == 1u
            && publishedEpisode.sequences[0].fileName == "first.jkl",
        "the episode parser reads initial campaign data through ResourceLocator");

    check(writeFixtureWithRevision(
              episodePath,
              QByteArrayLiteral(
                  "\"Broken Disk Episode\"\n"
                  "TYPE 1\n"
                  "SEQ 2\n"
                  "10: 0 1 LEVEL broken.jkl 0 0 -1 -1\n"),
              baseRevision + std::chrono::seconds(90)),
        "resource file-change integration externally replaces episode data with a truncated file");
    bool malformedEpisodeRejected = false;
    try
    {
        assets::EpisodeFile candidate = loadEpisodeFromResources();
        publishedEpisode = std::move(candidate);
    }
    catch (const std::exception&)
    {
        malformedEpisodeRejected = true;
    }
    check(malformedEpisodeRejected
            && publishedEpisode.name == "Disk Episode One"
            && publishedEpisode.sequences.size() == 1u
            && publishedEpisode.sequences[0].fileName == "first.jkl",
        "parsing an external episode candidate before assignment leaves the caller's prior value intact when parsing fails");

    check(writeFixtureWithRevision(
              episodePath,
              QByteArrayLiteral(
                  "\"Disk Episode Two\"\n"
                  "TYPE 1\n"
                  "SEQ 1\n"
                  "20: 1 2 LEVEL second_level.jkl 12 7 -1 -1\n"
                  "END\n"
                  "# repaired and intentionally longer\n"),
              baseRevision + std::chrono::seconds(100)),
        "resource file-change integration repairs episode data at the same path");
    publishedEpisode = loadEpisodeFromResources();
    check(publishedEpisode.name == "Disk Episode Two"
            && publishedEpisode.sequences.size() == 1u
            && publishedEpisode.sequences[0].lineNumber == 20
            && publishedEpisode.sequences[0].fileName == "second_level.jkl",
        "corrected external episode data reloads completely through ResourceLocator and the parser");

    assets::JlhTree publishedCategories = assets::loadFromJLH(
        "project_resources.jlh", resources);
    check(publishedCategories.roots.size() == 1u
            && publishedCategories.roots[0].children.size() == 1u
            && publishedCategories.roots[0].children[0].list
            && publishedCategories.roots[0].children[0].list->entries
                == std::vector<std::string>{ "first_actor.3do" },
        "the JLH parser resolves the initial JLH/JLL project resource list pair");

    check(writeFixtureWithRevision(
              categoryListPath,
              QByteArrayLiteral(
                  "[Props]\n"
                  "crate.3do\n"
                  "barrel.3do\n"),
              baseRevision + std::chrono::seconds(110)),
        "resource file-change integration externally replaces only one half of a project list pair");
    const assets::JlhTree unmatchedCategoryCandidate = assets::loadFromJLH(
        "project_resources.jlh", resources);
    check(unmatchedCategoryCandidate.roots.size() == 1u
            && unmatchedCategoryCandidate.roots[0].children.empty()
            && publishedCategories.roots.size() == 1u
            && publishedCategories.roots[0].children.size() == 1u
            && publishedCategories.roots[0].children[0].list
            && publishedCategories.roots[0].children[0].list->entries
                == std::vector<std::string>{ "first_actor.3do" },
        "a half-replaced JLH/JLL pair produces an unmatched candidate value while the caller's independent prior value remains unchanged");

    check(writeFixtureWithRevision(
              categoryHierarchyPath,
              QByteArrayLiteral(
                  "SUB Project Resources\n"
                  "LIST Props Environment Props\n"
                  "SUBEND\n"
                  "# repaired matching hierarchy\n"),
              baseRevision + std::chrono::seconds(120)),
        "resource file-change integration completes the external project-list pair replacement");
    publishedCategories = assets::loadFromJLH(
        "project_resources.jlh", resources);
    check(publishedCategories.roots.size() == 1u
            && publishedCategories.roots[0].label == "Project Resources"
            && publishedCategories.roots[0].children.size() == 1u
            && publishedCategories.roots[0].children[0].label
                == "Environment Props"
            && publishedCategories.roots[0].children[0].list
            && publishedCategories.roots[0].children[0].list->entries
                == std::vector<std::string>{ "crate.3do", "barrel.3do" },
        "a completed external JLH/JLL replacement parses as the coherent project resource list");

    ui::detail::clearSharedPreviewAssetCaches();
}

void runPreviewArchiveRetryBackoffTests()
{
    int ordinaryChangeCount = 0;
    int archiveAttemptCount = 0;
    ui::detail::PreviewResourceDiskWatcher watcher(
        nullptr,
        [&ordinaryChangeCount]()
        {
            ++ordinaryChangeCount;
        },
        40);
    watcher.setArchiveChangeCallback(
        [&archiveAttemptCount]()
        {
            ++archiveAttemptCount;
            return archiveAttemptCount >= 3;
        });

    check(watcher.archiveRetryDelayMsForTest() == 40
            && watcher.changeTimerIntervalForTest() == 40,
        "preview archive watcher starts with the configured debounce and retry delay");

    watcher.notifyArchivePathChangedForTest();
    check(watcher.changeTimerActiveForTest()
            && watcher.changeTimerIntervalForTest() == 40,
        "preview archive watcher schedules a new archive event at the configured debounce");
    watcher.flushPendingChangeForTest();
    check(archiveAttemptCount == 1
            && watcher.archiveRetryPendingForTest()
            && watcher.archiveRetryAttemptCountForTest() == 1
            && watcher.changeTimerIntervalForTest() == 40
            && watcher.archiveRetryDelayMsForTest() == 80,
        "preview archive watcher retains a failed event and doubles its next retry delay");

    watcher.flushPendingChangeForTest();
    check(archiveAttemptCount == 2
            && watcher.archiveRetryPendingForTest()
            && watcher.archiveRetryAttemptCountForTest() == 2
            && watcher.changeTimerIntervalForTest() == 80
            && watcher.archiveRetryDelayMsForTest() == 160,
        "preview archive watcher applies exponential backoff to consecutive failures");

    watcher.notifyArchivePathChangedForTest();
    check(watcher.archiveRetryAttemptCountForTest() == 0
            && watcher.archiveRetryDelayMsForTest() == 40
            && watcher.changeTimerIntervalForTest() == 40,
        "a fresh archive notification resets retry state and the active timer interval");
    watcher.flushPendingChangeForTest();
    check(archiveAttemptCount == 3
            && ordinaryChangeCount == 1
            && !watcher.archiveRetryPendingForTest()
            && watcher.archiveRetryAttemptCountForTest() == 0
            && watcher.changeTimerIntervalForTest() == 40,
        "a successful archive retry clears pending state and invokes ordinary invalidation once");

    watcher.setArchiveChangeCallback(
        [&archiveAttemptCount]()
        {
            ++archiveAttemptCount;
            return false;
        });
    watcher.notifyArchivePathChangedForTest();
    for ( int failure = 0; failure < 8; ++failure )
    {
        watcher.flushPendingChangeForTest();
    }
    check(watcher.archiveRetryPendingForTest()
            && watcher.archiveRetryAttemptCountForTest() == 8
            && watcher.changeTimerIntervalForTest() == 4000
            && watcher.archiveRetryDelayMsForTest() == 4000,
        "persistent archive failures keep a pending event with a capped retry interval");

    assets::ResourceLocator firstLocator;
    assets::ResourceLocator secondLocator;
    watcher.setResourceLocator(&firstLocator);
    check(!watcher.archiveRetryPendingForTest()
            && !watcher.changeTimerActiveForTest()
            && watcher.archiveRetryAttemptCountForTest() == 0
            && watcher.changeTimerIntervalForTest() == 40,
        "changing the owner locator cancels a stale archive retry and restores normal debounce");
    watcher.notifyPathChangedForTest();
    check(watcher.changeTimerActiveForTest()
            && watcher.changeTimerIntervalForTest() == 40,
        "ordinary resource changes retain the configured debounce after archive retry cancellation");
    watcher.flushPendingChangeForTest();
    check(ordinaryChangeCount == 2 && !watcher.changeTimerActiveForTest(),
        "ordinary invalidation proceeds after an archive retry is cancelled");
    watcher.setResourceLocator(&secondLocator);
}

} // namespace

void runRenderPreviewUtilityTests()
{
    using render::translucency::OrderKey;
    using render::translucency::PrimitiveKind;

    const int groupInitialWorkerCount = ui::detail::previewWorkerCount();
    const int groupInitialPublishBatchCount =
        ui::detail::previewPublishBatchCount();
    const auto settlePreviewTestState = [&]()
    {
        // Pool idleness precedes QFutureWatcher completion delivery. Repeat
        // the bounded two-stage delivery so a stale completion may queue and
        // fully retire one replacement job before this group returns.
        for (int pass = 0; pass < 3; ++pass)
        {
            ui::detail::previewWorkerPool().waitForDone();
            QCoreApplication::sendPostedEvents(nullptr);
            QCoreApplication::sendPostedEvents(nullptr);
            QCoreApplication::sendPostedEvents(
                nullptr, QEvent::DeferredDelete);
            QCoreApplication::processEvents(QEventLoop::AllEvents);
        }
        ui::detail::previewWorkerPool().waitForDone();
        ui::detail::clearSharedPreviewAssetCaches();
        ui::detail::setPreviewWorkerCount(groupInitialWorkerCount);
        ui::detail::setPreviewPublishBatchCount(
            groupInitialPublishBatchCount);
    };

    runBoundedTwoQueueCacheBranchTests();
    runClassicSectorTintScopeTests();
    runMotsArchLightingPreviewRegressionTests();
    runThingPlacementPreviewColormapTests();
    runLevImportOptionDefaultTests();
    runPreviewThumbLoadQueueBranchTests();
    runPreviewThumbnailPureHelperBranchTests();
    runPreviewThumbnailWidgetBoundaryTests();
    runPreviewThumbnailOwnerCancellationTests();
    runPreviewResourceIdentityRegressionTests();
    runExternalResourceFileChangeIntegrationTests();
    runPreviewArchiveRetryBackoffTests();

    constexpr std::size_t totalHeavyPreviewBudget =
        ui::detail::kRenderedModelPreviewBudget
        + ui::detail::kParsedModelPreviewBudget
        + ui::detail::kMaterialPreviewBudget
        + ui::detail::kSpriteImagePreviewBudget
        + ui::detail::kParsedSpritePreviewBudget;
    check(totalHeavyPreviewBudget <= 1024u * ui::detail::kPreviewCacheMiB,
        "shared preview caches have a combined heavy-asset ceiling no greater than one GiB");

    struct TaggedOrder
    {
        char tag{};
        OrderKey key{};
    };

    std::vector<TaggedOrder> order{
        TaggedOrder{ 'f', OrderKey{ 4.0f, PrimitiveKind::thing, 9, 90 } },
        TaggedOrder{ 's', OrderKey{ 64.0f, PrimitiveKind::sprite, 3, 30 } },
        TaggedOrder{ 'l', OrderKey{ 64.0f, PrimitiveKind::level, 1, 10 } },
        TaggedOrder{ 't', OrderKey{ 100.0f, PrimitiveKind::thing, 2, 20 } },
        TaggedOrder{ 'n', OrderKey{ std::numeric_limits<float>::quiet_NaN(), PrimitiveKind::level, 0, 0 } },
    };
    std::sort(order.begin(), order.end(), [](const TaggedOrder& a, const TaggedOrder& b)
        {
            return render::translucency::backToFrontLess(a.key, b.key);
        });
    check(
        std::string{ order[0].tag, order[1].tag, order[2].tag, order[3].tag, order[4].tag } == "tlsfn",
        "translucency order globally interleaves Thing, level, and sprite draws back-to-front");

    const float orthographicFrontOffAxis = render::translucency::projectionOrderDepth(
        100.0f,
        0.0f,
        5.0f,
        0.0f,
        0.0f,
        1.0f,
        render::translucency::ProjectionKind::orthographic);
    const float orthographicBackCentered = render::translucency::projectionOrderDepth(
        0.0f,
        0.0f,
        10.0f,
        0.0f,
        0.0f,
        1.0f,
        render::translucency::ProjectionKind::orthographic);
    check(
        orthographicFrontOffAxis == 5.0f
            && orthographicBackCentered == 10.0f
            && render::translucency::backToFrontLess(
                OrderKey{ orthographicBackCentered, PrimitiveKind::thing, 1, 0 },
                OrderKey{ orthographicFrontOffAxis, PrimitiveKind::thing, 2, 0 }),
        "translucency orthographic ordering ignores lateral eye distance and draws greater view depth first");

    check(
        !render::translucency::passesAlphaDepthPrepass(1.0f, 0.5f),
        "alpha-depth prepass rejects an opaque texel with translucent material alpha");
    check(
        render::translucency::passesAlphaDepthPrepass(1.0f, 0.98f),
        "alpha-depth prepass retains a final near-opaque fragment");
    check(
        !render::translucency::passesAlphaDepthPrepass(0.90f, 1.0f),
        "alpha-depth prepass still rejects a translucent texture texel");
    check(
        render::translucency::allowsAlphaDepthPrepassForFace(false)
            && !render::translucency::allowsAlphaDepthPrepassForFace(true),
        "alpha-depth prepass honors explicit z-write-disabled faces");
    check(
        render::translucency::allowsAlphaDepthPrepassForBatch(
            PrimitiveKind::level, true, true, false, false, false)
            && render::translucency::allowsAlphaDepthPrepassForBatch(
                PrimitiveKind::thing, true, true, false, false, false)
            && !render::translucency::allowsAlphaDepthPrepassForBatch(
                PrimitiveKind::level, true, true, true, false, false)
            && !render::translucency::allowsAlphaDepthPrepassForBatch(
                PrimitiveKind::level, true, true, false, true, false)
            && !render::translucency::allowsAlphaDepthPrepassForBatch(
                PrimitiveKind::level, true, true, false, false, true)
            && !render::translucency::allowsAlphaDepthPrepassForBatch(
                PrimitiveKind::sprite, true, true, false, false, false),
        "alpha-depth prepass admits translucent solid level walls and Thing meshes but excludes adjoins, sky, sprites, and no-z-write faces");
    check(
        render::translucency::shouldSeparateLevelSurfaceForTranslucency(
            true, false, false, true, false)
            && render::translucency::shouldSeparateLevelSurfaceForTranslucency(
                true, false, false, false, true)
            && !render::translucency::shouldSeparateLevelSurfaceForTranslucency(
                true, false, false, false, false)
            && !render::translucency::shouldSeparateLevelSurfaceForTranslucency(
                true, false, true, true, false)
            && !render::translucency::shouldSeparateLevelSurfaceForTranslucency(
                false, false, false, true, false),
        "solid classic adjoins enter the translucent queue only when their Face flags request blending or no z-write");
    check(
        render::translucency::levelSurfaceUsesTranslucentPass(
            true, true, false, false)
            && render::translucency::levelSurfaceUsesTranslucentPass(
                true, false, false, true)
            && !render::translucency::levelSurfaceUsesTranslucentPass(
                true, false, false, false)
            && !render::translucency::levelSurfaceUsesTranslucentPass(
                false, false, false, true),
        "level alpha-texture surfaces join the translucent pass only when they are adjoins, matching the classic alpha-adjoin queue");


    QTemporaryDir fixtureDir;
    check(fixtureDir.isValid(), "preview revision test creates a temporary resource root");
    if (!fixtureDir.isValid())
    {
        settlePreviewTestState();
        return;
    }

    const QString modelPath = fixtureDir.filePath(QStringLiteral("preview.3do"));
    const QString materialPath = fixtureDir.filePath(QStringLiteral("preview.mat"));
    const QString cmpPath = fixtureDir.filePath(QStringLiteral("preview.cmp"));
    check(writeFixture(modelPath, QByteArray("model-v1")), "preview revision fixture writes 3DO");
    check(writeFixture(materialPath, QByteArray("mat-v1")), "preview revision fixture writes MAT");
    check(writeFixture(cmpPath, QByteArray("cmp-v1")), "preview revision fixture writes CMP");

    assets::ResourceLocator resources{};
    resources.addRoot(fixtureDir.path().toStdString());

    std::vector<QColor> patternPixels{};
    patternPixels.reserve(16u);
    for (int y = 0; y < 4; ++y)
    {
        for (int x = 0; x < 4; ++x)
        {
            patternPixels.emplace_back(
                35 + x * 60,
                30 + y * 65,
                220 - x * 35 - y * 20,
                255);
        }
    }
    check(writeFixture(
              fixtureDir.filePath(QStringLiteral("pattern.mat")),
              makeRgbaMatFixture(4, 4, patternPixels)),
        "worker thumbnail visual fixture writes a project-specific UV pattern MAT");
    check(writeFixture(
              fixtureDir.filePath(QStringLiteral("red.mat")),
              makeRgbaMatFixture(2, 2, std::vector<QColor>(4u, QColor(240, 20, 20, 255)))),
        "worker thumbnail visual fixture writes its red multi-mesh MAT");
    check(writeFixture(
              fixtureDir.filePath(QStringLiteral("green.mat")),
              makeRgbaMatFixture(2, 2, std::vector<QColor>(4u, QColor(20, 240, 20, 255)))),
        "worker thumbnail visual fixture writes its green multi-mesh MAT");
    check(writeFixture(
              fixtureDir.filePath(QStringLiteral("skin.mat")),
              makeRgbaMatFixture(2, 2, std::vector<QColor>(4u, QColor(240, 170, 120, 255)))),
        "worker thumbnail visual fixture writes its small child-mesh skin MAT");
    const std::optional<ui::detail::PreviewMaterialTextureSource> preparedPattern =
        ui::detail::loadPreviewMaterialTextureSource(
            &resources, {}, "pattern.mat");
    check(preparedPattern
            && QColor::fromRgba(preparedPattern->image.pixel(0, 0))
                == patternPixels.front()
            && QColor::fromRgba(preparedPattern->image.pixel(0, 3))
                == patternPixels[12]
            && preparedPattern->alphaBpp == 8,
        "worker-prepared MAT pixels keep the normal GlView upload orientation when primed into a selected 3DO preview");

    constexpr std::uint8_t kSolidRedIndex = 6u;
    check(writeFixture(
              fixtureDir.filePath(QStringLiteral("classic.cmp")),
              makeClassicCmpFixture(kSolidRedIndex, QColor(255, 0, 0))),
        "worker thumbnail Solid-color fixture writes a complete classic CMP");
    check(writeFixture(
              fixtureDir.filePath(QStringLiteral("classic.mat")),
              makeIndexedMatFixture(kSolidRedIndex)),
        "worker thumbnail Solid-color fixture writes an indexed classic MAT");
    const std::optional<ui::detail::PreviewMaterialTextureSource>
        preparedClassicSolid = ui::detail::loadPreviewMaterialTextureSource(
            &resources, "classic.cmp", "classic.mat");
    check(preparedClassicSolid
            && closeQVec(
                preparedClassicSolid->solidColor,
                QVector3D(1.0f, 0.0f, 0.0f))
            && preparedClassicSolid->classicSolidLightRamp
            && closeQVec(
                (*preparedClassicSolid->classicSolidLightRamp)[0],
                QVector3D(1.0f, 0.0f, 0.0f))
            && closeQVec(
                (*preparedClassicSolid->classicSolidLightRamp)[31],
                QVector3D(1.0f, 0.0f, 0.0f))
            && closeQVec(
                (*preparedClassicSolid->classicSolidLightRamp)[63],
                QVector3D(1.0f, 0.0f, 0.0f)),
        "worker-prepared MAT metadata retains every CMP-resolved Solid-color light level for picker and thumbnail GlViews");

    QByteArray paletteOnlyCmp =
        makeClassicCmpFixture(kSolidRedIndex, QColor(255, 0, 0));
    paletteOnlyCmp.truncate(64 + 256 * 3);
    check(writeFixture(
              fixtureDir.filePath(QStringLiteral("palette_only.cmp")),
              paletteOnlyCmp),
        "worker thumbnail palette-only fallback fixture writes a truncated CMP after its valid palette");
    const std::optional<ui::detail::PreviewMaterialTextureSource>
        preparedPaletteOnly = ui::detail::loadPreviewMaterialTextureSource(
            &resources, "palette_only.cmp", "classic.mat");
    check(preparedPaletteOnly
            && closeQVec(
                preparedPaletteOnly->solidColor,
                QVector3D(1.0f, 0.0f, 0.0f))
            && !preparedPaletteOnly->classicSolidLightRamp,
        "worker-prepared MAT keeps palette decoding available when a CMP has no complete classic light-table payload");

    const QString asymmetricSpritePath =
        fixtureDir.filePath(QStringLiteral("asymmetric.spr"));
    const QString asymmetricSpriteMaterialPath =
        fixtureDir.filePath(QStringLiteral("asymmetric.mat"));
    const std::vector<QColor> asymmetricSpritePixels{
        QColor(240, 20, 30, 255),
        QColor(20, 230, 40, 255),
        QColor(30, 50, 240, 255),
        QColor(235, 210, 25, 0),
    };
    check(writeFixture(
              asymmetricSpritePath,
              QByteArray("asymmetric.mat 0 2.0 1.0 4 3 3 0.0 0.0 0.0 0.0\n")),
        "sprite preview mapping fixture writes a classic SPR");
    check(writeFixture(
              asymmetricSpriteMaterialPath,
              makeRgbaMatFixture(2, 2, asymmetricSpritePixels)),
        "sprite preview mapping fixture writes an asymmetric alpha MAT");

    ui::detail::clearSharedPreviewAssetCaches();
    const std::string initialSpriteDependencies =
        ui::detail::sharedPreviewSpriteDependencyRevisionToken(
            &resources, {}, "asymmetric.spr");
    const std::optional<QImage> asymmetricSpriteImage =
        ui::detail::loadSharedPreviewSpriteImage(
            &resources, {}, "asymmetric.spr");
    check(asymmetricSpriteImage
            && asymmetricSpriteImage->size() == QSize(2, 2)
            && asymmetricSpriteImage->pixelColor(0, 0) == asymmetricSpritePixels[0]
            && asymmetricSpriteImage->pixelColor(1, 0) == asymmetricSpritePixels[1]
            && asymmetricSpriteImage->pixelColor(0, 1) == asymmetricSpritePixels[2]
            && asymmetricSpriteImage->pixelColor(1, 1) == asymmetricSpritePixels[3],
        "SPR list and selected previews preserve asymmetric MAT pixel orientation and alpha exactly");

    const std::vector<QColor> replacementSpritePixels{
        QColor(12, 22, 32, 255),
        QColor(42, 52, 62, 255),
        QColor(72, 82, 92, 255),
        QColor(102, 112, 122, 255),
        QColor(132, 142, 152, 128),
        QColor(162, 172, 182, 0),
    };
    check(writeFixture(
              asymmetricSpriteMaterialPath,
              makeRgbaMatFixture(3, 2, replacementSpritePixels)),
        "sprite preview mapping fixture replaces only the referenced MAT");
    const std::string replacedSpriteDependencies =
        ui::detail::sharedPreviewSpriteDependencyRevisionToken(
            &resources, {}, "asymmetric.spr");
    const std::optional<QImage> replacedSpriteImage =
        ui::detail::loadSharedPreviewSpriteImage(
            &resources, {}, "asymmetric.spr");
    check(replacedSpriteDependencies != initialSpriteDependencies
            && replacedSpriteImage
            && replacedSpriteImage->size() == QSize(3, 2)
            && replacedSpriteImage->pixelColor(0, 0) == replacementSpritePixels[0]
            && replacedSpriteImage->pixelColor(1, 1) == replacementSpritePixels[4]
            && replacedSpriteImage->pixelColor(2, 1) == replacementSpritePixels[5],
        "SPR preview dependency identity and decoded pixels refresh when only its MAT changes");

    const std::array normalizedUvs{
        assets::Model3doTexVertex{ 0.0f, 0.0f },
        assets::Model3doTexVertex{ 1.0f, 0.0f },
        assets::Model3doTexVertex{ 1.0f, 1.0f },
        assets::Model3doTexVertex{ 0.0f, 1.0f },
    };
    const std::array pixelUvs{
        assets::Model3doTexVertex{ 0.0f, 0.0f },
        assets::Model3doTexVertex{ 4.0f, 0.0f },
        assets::Model3doTexVertex{ 4.0f, 4.0f },
        assets::Model3doTexVertex{ 0.0f, 4.0f },
    };
    assets::Model3do ijimUvModel{};
    ijimUvModel.version = 2.3;
    ijimUvModel.materials = { "pattern.mat" };
    ijimUvModel.meshes.push_back(makePreviewQuadMesh(
        "ijim-normalized", -1.0, 1.0, normalizedUvs, 0));
    assets::Model3do classicUvModel = ijimUvModel;
    classicUvModel.version = 2.1;
    classicUvModel.meshes[0].name = "classic-pixel";
    classicUvModel.meshes[0].texVertices.assign(pixelUvs.begin(), pixelUvs.end());
    const QImage ijimUvThumbnail = ui::detail::makeTemplateModelPreviewImage(
        ijimUvModel, QSize(180, 140), &resources, {}, false);
    const QImage classicUvThumbnail = ui::detail::makeTemplateModelPreviewImage(
        classicUvModel, QSize(180, 140), &resources, {}, false);
    check(!ijimUvThumbnail.isNull()
            && classicUvThumbnail == ijimUvThumbnail,
        "worker thumbnail raster maps classic JK/MotS pixel UVs to the same texels as equivalent IJIM normalized UVs");

    const std::array tiledNormalizedUvs{
        assets::Model3doTexVertex{ -0.5f, -0.25f },
        assets::Model3doTexVertex{ 1.5f, -0.25f },
        assets::Model3doTexVertex{ 1.5f, 1.75f },
        assets::Model3doTexVertex{ -0.5f, 1.75f },
    };
    const std::array tiledPixelUvs{
        assets::Model3doTexVertex{ -2.0f, -1.0f },
        assets::Model3doTexVertex{ 6.0f, -1.0f },
        assets::Model3doTexVertex{ 6.0f, 7.0f },
        assets::Model3doTexVertex{ -2.0f, 7.0f },
    };
    assets::Model3do tiledIjimUvModel = ijimUvModel;
    tiledIjimUvModel.meshes[0].name = "ijim-tiled-normalized";
    tiledIjimUvModel.meshes[0].texVertices.assign(
        tiledNormalizedUvs.begin(), tiledNormalizedUvs.end());
    assets::Model3do tiledClassicUvModel = classicUvModel;
    tiledClassicUvModel.meshes[0].name = "classic-tiled-pixel";
    tiledClassicUvModel.meshes[0].texVertices.assign(
        tiledPixelUvs.begin(), tiledPixelUvs.end());
    const QImage tiledIjimUvThumbnail = ui::detail::makeTemplateModelPreviewImage(
        tiledIjimUvModel, QSize(180, 140), &resources, {}, false);
    const QImage tiledClassicUvThumbnail = ui::detail::makeTemplateModelPreviewImage(
        tiledClassicUvModel, QSize(180, 140), &resources, {}, false);
    check(!tiledIjimUvThumbnail.isNull()
            && tiledClassicUvThumbnail == tiledIjimUvThumbnail,
        "worker thumbnail raster maps signed multi-tile classic pixel UVs to the same texels as equivalent IJIM normalized UVs");

    const std::array warpedUvs{
        assets::Model3doTexVertex{ 0.0f, 0.0f },
        assets::Model3doTexVertex{ 1.0f, 0.0f },
        assets::Model3doTexVertex{ 0.30f, 0.65f },
        assets::Model3doTexVertex{ 0.0f, 1.0f },
    };
    assets::Model3do polygonModel{};
    polygonModel.version = 2.3;
    polygonModel.materials = { "pattern.mat" };
    polygonModel.meshes.push_back(makePreviewQuadMesh(
        "zig-zag-polygon", -1.0, 1.0, warpedUvs, 0));
    assets::Model3do explicitTrianglesModel = polygonModel;
    explicitTrianglesModel.meshes[0].name = "explicit-zig-zag";
    explicitTrianglesModel.meshes[0].faces.clear();
    for (const std::array<std::int32_t, 3> indices : {
             std::array<std::int32_t, 3>{ 0, 1, 3 },
             std::array<std::int32_t, 3>{ 1, 2, 3 } })
    {
        assets::Model3doFace triangle{};
        triangle.material = 0;
        triangle.geo = static_cast<std::int32_t>(TGeoMode::Texture);
        triangle.light = static_cast<std::int32_t>(TLightMode::Gouraud);
        triangle.tex = static_cast<std::int32_t>(TGeoMode::Texture);
        triangle.vertices.assign(indices.begin(), indices.end());
        triangle.texVertices.assign(indices.begin(), indices.end());
        explicitTrianglesModel.meshes[0].faces.push_back(std::move(triangle));
    }
    const QImage polygonThumbnail = ui::detail::makeTemplateModelPreviewImage(
        polygonModel, QSize(180, 140), &resources, {}, false);
    const QImage explicitTrianglesThumbnail = ui::detail::makeTemplateModelPreviewImage(
        explicitTrianglesModel, QSize(180, 140), &resources, {}, false);
    check(!polygonThumbnail.isNull()
            && polygonThumbnail == explicitTrianglesThumbnail,
        "worker thumbnail polygon raster uses the same zig-zag triangulation as the interactive viewport");

    QImage opaqueAlphaImage(2, 2, QImage::Format_RGBA8888);
    opaqueAlphaImage.fill(QColor(200, 120, 40, 255));
    const ui::detail::PreviewMaterialTextureSource opaqueEightBitAlpha{
        .image = opaqueAlphaImage,
        .alphaBpp = 8,
    };
    check(ui::detail::modelPreviewTransparencyMode(opaqueEightBitAlpha, 0u)
            == ui::detail::PreviewMaterialTransparency::blended
            && ui::detail::modelPreviewTransparencyMode(
                   ui::detail::PreviewMaterialTextureSource{ opaqueAlphaImage, 1 }, 0u)
                == ui::detail::PreviewMaterialTransparency::cutout
            && ui::detail::modelPreviewTransparencyMode(
                   ui::detail::PreviewMaterialTextureSource{ opaqueAlphaImage, 0 }, FF_Transluent)
                == ui::detail::PreviewMaterialTransparency::blended,
        "worker thumbnail alpha classification follows MAT alpha depth and explicit 3DO face flags rather than sampled pixels alone");

    assets::Model3do multiMeshModel{};
    multiMeshModel.version = 2.3;
    multiMeshModel.materials = { "red.mat", "green.mat" };
    multiMeshModel.meshes.push_back(makePreviewQuadMesh(
        "left", -2.0, -0.15, normalizedUvs, 0));
    multiMeshModel.meshes.push_back(makePreviewQuadMesh(
        "right", 0.15, 2.0, normalizedUvs, 1));
    const QImage multiMeshThumbnail = ui::detail::makeTemplateModelPreviewImage(
        multiMeshModel, QSize(220, 140), &resources, {}, false);
    check(dominantPixelCount(multiMeshThumbnail, QColor(255, 0, 0)) > 100u
            && dominantPixelCount(multiMeshThumbnail, QColor(0, 255, 0)) > 100u,
        "worker thumbnail raster preserves every prepared mesh instead of dropping later model parts");

    assets::Model3do smallHeadModel{};
    smallHeadModel.version = 2.3;
    smallHeadModel.materials = { "green.mat", "skin.mat" };
    smallHeadModel.meshes.push_back(makePreviewQuadMesh(
        "body", -0.55, 0.55, normalizedUvs, 0));
    assets::Model3doMesh smallHead = makePreviewQuadMesh(
        "head", -0.22, 0.22, normalizedUvs, 1);
    for (assets::Model3doVertex& vertex : smallHead.vertices)
    {
        vertex.z = (vertex.z * 0.24) + 1.24;
    }
    smallHeadModel.meshes.push_back(std::move(smallHead));
    const QImage smallHeadThumbnail = ui::detail::makeTemplateModelPreviewImage(
        smallHeadModel, QSize(96, 96), &resources, {}, false);
    std::size_t visibleSkinPixels = 0u;
    for (int y = 0; y < smallHeadThumbnail.height(); ++y)
    {
        for (int x = 0; x < smallHeadThumbnail.width(); ++x)
        {
            const QColor pixel = smallHeadThumbnail.pixelColor(x, y);
            if (pixel.red() >= 220 && pixel.green() >= 145 && pixel.blue() >= 95)
            {
                ++visibleSkinPixels;
            }
        }
    }
    check(visibleSkinPixels >= 8u,
        "worker list thumbnail keeps a separate small head mesh visibly rendered at actual icon resolution");

    assets::Model3do actorScaleHeadModel{};
    actorScaleHeadModel.version = 2.3;
    actorScaleHeadModel.materials = { "red.mat" };
    assets::Model3doMesh actorScaleHead = makePreviewQuadMesh(
        "actor-scale-head", -1.0, 1.0, normalizedUvs, 0);
    for (assets::Model3doVertex& vertex : actorScaleHead.vertices)
    {
        vertex.x *= 0.001;
        vertex.y *= 0.001;
        vertex.z *= 0.001;
    }
    actorScaleHeadModel.meshes.push_back(std::move(actorScaleHead));
    const QImage actorScaleHeadThumbnail = ui::detail::makeTemplateModelPreviewImage(
        actorScaleHeadModel, QSize(96, 96), &resources, {}, false);
    check(dominantPixelCount(actorScaleHeadThumbnail, QColor(255, 0, 0)) >= 8u,
        "worker thumbnail raster preserves valid actor-scale head triangles instead of rejecting them as degenerate");

    assets::Model3do denseActorModel{};
    denseActorModel.version = 2.3;
    denseActorModel.materials = { "green.mat", "skin.mat", "red.mat" };
    assets::Model3doMesh denseActorBody = makePreviewQuadMesh(
        "body", -0.54, 0.54, normalizedUvs, 0);
    for (assets::Model3doVertex& vertex : denseActorBody.vertices)
    {
        vertex.z = (vertex.z * 0.78) - 0.12;
    }
    denseActorModel.meshes.push_back(std::move(denseActorBody));
    denseActorModel.meshes.push_back(makePreviewDenseGridMesh(
        "dense-head", -0.18, 0.18, 0.72, 1.08, 48, 1));
    assets::Model3doMesh denseActorHat = makePreviewQuadMesh(
        "hat", -0.25, 0.25, normalizedUvs, 2);
    for (assets::Model3doVertex& vertex : denseActorHat.vertices)
    {
        vertex.z = (vertex.z * 0.045) + 1.13;
    }
    denseActorModel.meshes.push_back(std::move(denseActorHat));
    const QImage denseActorThumbnail = ui::detail::makeTemplateModelPreviewImage(
        denseActorModel, QSize(96, 96), &resources, {}, false);
    check(dominantPixelCount(denseActorThumbnail, QColor(0, 255, 0)) >= 80u
            && dominantPixelCount(denseActorThumbnail, QColor(240, 170, 120)) >= 8u
            && dominantPixelCount(denseActorThumbnail, QColor(255, 0, 0)) >= 4u,
        "worker thumbnail preserves a body, dense tiny-triangle actor head, and separate accessory mesh with their own materials");

    std::vector<QFuture<QImage>> concurrentMaterialRenders{};
    concurrentMaterialRenders.reserve(12u);
    for (int index = 0; index < 12; ++index)
    {
        const bool useRed = (index & 1) == 0;
        assets::Model3do isolatedMaterialModel{};
        isolatedMaterialModel.version = 2.3;
        isolatedMaterialModel.materials = { useRed ? "red.mat" : "green.mat" };
        isolatedMaterialModel.meshes.push_back(makePreviewQuadMesh(
            useRed ? "red-concurrent" : "green-concurrent",
            -1.0,
            1.0,
            normalizedUvs,
            0));
        concurrentMaterialRenders.push_back(ui::detail::runPreviewWorkerTask(
            [isolatedMaterialModel = std::move(isolatedMaterialModel), &resources]()
            {
                return ui::detail::makeTemplateModelPreviewImage(
                    isolatedMaterialModel, QSize(96, 96), &resources, {}, false);
            }));
    }
    ui::detail::previewWorkerPool().waitForDone();
    bool concurrentMaterialsStayedIsolated = true;
    for (std::size_t index = 0; index < concurrentMaterialRenders.size(); ++index)
    {
        const QImage image = concurrentMaterialRenders[index].result();
        const std::size_t redPixels = dominantPixelCount(image, QColor(255, 0, 0));
        const std::size_t greenPixels = dominantPixelCount(image, QColor(0, 255, 0));
        concurrentMaterialsStayedIsolated = concurrentMaterialsStayedIsolated
            && ((index & 1u) == 0u
                    ? redPixels > 100u && greenPixels == 0u
                    : greenPixels > 100u && redPixels == 0u);
    }
    check(concurrentMaterialsStayedIsolated,
        "concurrent thumbnail workers never cross-assign another model's MAT pixels or material indices");

    assets::ResourceLocator equivalentResources = resources;
    const std::string resourceRevision = ui::detail::previewResourceRevisionToken(
        &resources, "preview.3do", ".3do");
    const std::string equivalentResourceRevision =
        ui::detail::previewResourceRevisionToken(
            &equivalentResources, "preview.3do", ".3do");
    check(ui::detail::previewResourceCacheIdentity(
              &resources, "preview.3do", resourceRevision)
            == ui::detail::previewResourceCacheIdentity(
                &equivalentResources, "preview.3do", equivalentResourceRevision),
        "preview cache identity is shared by equivalent ResourceLocator snapshots owned by picker and template-editor dialogs");
    const ui::detail::SharedModelThumbLoadTuning modelThumbTuning =
        ui::detail::sharedModelThumbLoadTuning();
    check(modelThumbTuning.maxPrepJobs == ui::detail::previewWorkerCount(),
        "each thumbnail owner limits active prep jobs to the configured dedicated-worker count");
    check(modelThumbTuning.visibleBatchCount
                == ui::detail::previewPublishBatchCount()
            && modelThumbTuning.offscreenBatchCount == 0
            && modelThumbTuning.startDelayMs < ui::detail::kAssetPreviewSpinIntervalMs
            && modelThumbTuning.visibleDelayMs
                > ui::detail::kAssetPreviewSpinIntervalMs
            && modelThumbTuning.visibleDelayMs <= 50,
        "worker-rendered preview thumbnails use a small GUI publication batch paced below the interactive preview frame rate without draining off-screen rows");

    const int initialWorkerCount = ui::detail::previewWorkerCount();
    const int initialPublishBatchCount = ui::detail::previewPublishBatchCount();
    check(ui::detail::defaultPreviewWorkerCount() >= 2
            && ui::detail::defaultPreviewWorkerCount() <= 4,
        "thumbnail worker default leaves one CPU thread available while staying within the responsive 2-to-4 range");
    ui::detail::setPreviewWorkerCount(-50);
    check(ui::detail::previewWorkerCount() == 1
            && ui::detail::previewWorkerPool().maxThreadCount() == 1,
        "thumbnail worker count clamps its lower bound and updates the dedicated pool immediately");
    ui::detail::setPreviewWorkerCount(500);
    check(ui::detail::previewWorkerCount() == 16
            && ui::detail::previewWorkerPool().maxThreadCount() == 16,
        "thumbnail worker count clamps its upper bound without creating unbounded parallelism");
    ui::detail::setPreviewPublishBatchCount(-20);
    check(ui::detail::previewPublishBatchCount() == 1,
        "thumbnail GUI publication batch clamps to at least one completed image");
    ui::detail::setPreviewPublishBatchCount(80);
    check(ui::detail::previewPublishBatchCount() == 8,
        "thumbnail GUI publication batch clamps to its bounded responsive maximum");
    ui::detail::setPreviewWorkerCount(initialWorkerCount);
    ui::detail::setPreviewPublishBatchCount(initialPublishBatchCount);

    check(&ui::detail::previewWorkerPool() != QThreadPool::globalInstance()
            && ui::detail::previewWorkerPool().threadPriority()
                == QThread::LowPriority,
        "thumbnail parsing and rasterization use a dedicated low-priority pool rather than Qt's global application pool");
    const auto poolIdentityFuture = ui::detail::runPreviewWorkerTask([]()
        {
            QThread* current = QThread::currentThread();
            return std::pair{
                ui::detail::previewWorkerPool().contains(current),
                QThreadPool::globalInstance()->contains(current),
            };
        });
    ui::detail::previewWorkerPool().waitForDone();
    const std::pair<bool, bool> poolIdentity = poolIdentityFuture.result();
    check(poolIdentity.first && !poolIdentity.second,
        "thumbnail work executes on the dedicated pool's threads and never consumes a global-pool worker");

    const auto workerImageFuture = ui::detail::runPreviewWorkerTask(
        [ijimUvModel, &resources]()
        {
            return ui::detail::makeTemplateModelPreviewImage(
                ijimUvModel, QSize(180, 140), &resources, {}, false);
        });
    check(workerImageFuture.result() == ijimUvThumbnail,
        "moving thumbnail rendering to the dedicated pool preserves the exact deterministic QImage output");

    const ui::detail::PreviewWorkerPublicationGuard livePublication =
        ui::detail::makePreviewWorkerPublicationGuard(41u);
    check(ui::detail::previewWorkerMayPublish(livePublication, 41u)
            && !ui::detail::previewWorkerMayPublish(livePublication, 42u),
        "thumbnail publication accepts only the owner/resource generation captured when work was queued");
    ui::detail::cancelPreviewWorkerPublication(livePublication);
    check(!ui::detail::previewWorkerMayPublish(livePublication, 41u),
        "destroyed or canceled thumbnail owners cannot publish a late worker result");

    ui::detail::PreviewThumbnailPublicationPacer publicationPacer{};
    publicationPacer.noteInteractionAt(1'000);
    check(publicationPacer.interactionActiveAt(1'000)
            && publicationPacer.interactionActiveAt(
                1'000 + ui::detail::kPreviewThumbnailInteractionQuietMs - 1)
            && !publicationPacer.interactionActiveAt(
                1'000 + ui::detail::kPreviewThumbnailInteractionQuietMs)
            && publicationPacer.delayUntilQuietAt(1'025)
                == ui::detail::kPreviewThumbnailInteractionQuietMs - 25,
        "thumbnail GUI publication stays paused through the complete scroll/search quiet interval and resumes exactly once it expires");
    check(ui::detail::previewThumbnailBatchCanContinue(0, 8, 50'000'000)
            && ui::detail::previewThumbnailBatchCanContinue(
                1, 8, ui::detail::kPreviewThumbnailPublishBudgetNs - 1)
            && !ui::detail::previewThumbnailBatchCanContinue(
                1, 8, ui::detail::kPreviewThumbnailPublishBudgetNs)
            && !ui::detail::previewThumbnailBatchCanContinue(2, 2, 0),
        "thumbnail GUI publication always permits one icon but then obeys both its time budget and configured item limit");
    check(!ui::detail::previewThumbnailQueueNeedsPolling(true, 2)
            && !ui::detail::previewThumbnailQueueNeedsPolling(false, 0)
            && ui::detail::previewThumbnailQueueNeedsPolling(true, 0),
        "pending thumbnail rows wait for active worker completion instead of polling the GUI thread, while unqueued visible work still advances");
    check(ui::detail::previewThumbnailPriorityRows(4, 8, 6)
                == std::vector<int>{ 6, 4, 5, 7, 8 }
            && ui::detail::previewThumbnailPriorityRows(4, 8, 20)
                == std::vector<int>{ 4, 5, 6, 7, 8 },
        "thumbnail publication prioritizes the current item without duplicating or losing visible rows");
    const float ordinarySpinStep = ui::detail::assetPreviewSpinStepDegrees(72.0f, 33);
    const float recoveredSpinStep = ui::detail::assetPreviewSpinStepDegrees(72.0f, 66);
    check(recoveredSpinStep > ordinarySpinStep * 1.9f
            && recoveredSpinStep < ordinarySpinStep * 2.1f
            && ui::detail::assetPreviewSpinStepDegrees(72.0f, 500)
                == ui::detail::assetPreviewSpinStepDegrees(72.0f, 100),
        "interactive model preview rotation accounts for a skipped thumbnail-capture tick while bounding catch-up jumps");

    QListWidget visibleRangeProbe{};
    visibleRangeProbe.setViewMode(QListView::IconMode);
    visibleRangeProbe.setFlow(QListView::LeftToRight);
    visibleRangeProbe.setWrapping(true);
    visibleRangeProbe.setResizeMode(QListView::Adjust);
    visibleRangeProbe.setIconSize(QSize(96, 96));
    visibleRangeProbe.setGridSize(QSize(136, 166));
    visibleRangeProbe.resize(680, 420);
    for (int row = 0; row < 5'000; ++row)
    {
        visibleRangeProbe.addItem(QStringLiteral("model_%1.3do").arg(row));
    }
    visibleRangeProbe.show();
    QCoreApplication::processEvents(QEventLoop::AllEvents);
    int thumbnailModelChangeSignals = 0;
    QObject::connect(
        visibleRangeProbe.model(),
        &QAbstractItemModel::dataChanged,
        &visibleRangeProbe,
        [&thumbnailModelChangeSignals](
            const QModelIndex&,
            const QModelIndex&,
            const QList<int>&)
        {
            ++thumbnailModelChangeSignals;
        });
    QImage publishedTileImage(96, 96, QImage::Format_ARGB32_Premultiplied);
    publishedTileImage.fill(QColor(55, 145, 215));
    ui::detail::publishPreviewThumbnailItem(
        &visibleRangeProbe,
        visibleRangeProbe.item(0),
        QIcon(QPixmap::fromImage(publishedTileImage)),
        ui::detail::kPreviewThumbStateRole,
        static_cast<int>(ui::detail::PreviewThumbState::ready));
    check(thumbnailModelChangeSignals == 0
            && visibleRangeProbe.item(0)->data(
                ui::detail::kPreviewThumbStateRole).toInt()
                == static_cast<int>(ui::detail::PreviewThumbState::ready)
            && !visibleRangeProbe.item(0)->icon().isNull(),
        "fixed-size thumbnail publication repaints only its tile without emitting a list-model data/layout change");
    int visibleBegin = -1;
    int visibleEnd = -1;
    int visibleProbeCount = 0;
    check(
        ui::detail::visiblePreviewListRowRange(
            &visibleRangeProbe,
            visibleBegin,
            visibleEnd,
            4,
            &visibleProbeCount)
            && visibleBegin >= 0
            && visibleEnd >= visibleBegin,
        "preview thumbnail scheduler resolves the visible range for a large wrapped list");
    check(
        visibleProbeCount <= 128,
        "preview thumbnail visible-range work is bounded by the viewport instead of the full resource count");

    QImage wideFramebuffer(200, 100, QImage::Format_ARGB32_Premultiplied);
    wideFramebuffer.fill(QColor(18, 20, 28));
    for (int y = 30; y < 70; ++y)
    {
        for (int x = 80; x < 120; ++x)
        {
            wideFramebuffer.setPixelColor(x, y, QColor(240, 80, 30));
        }
    }
    const QImage fittedFramebuffer =
        ui::detail::fitAuthoritativeModelPreviewImage(
            wideFramebuffer, QSize(100, 100));
    int fittedMarkerMinX = fittedFramebuffer.width();
    int fittedMarkerMaxX = -1;
    int fittedMarkerMinY = fittedFramebuffer.height();
    int fittedMarkerMaxY = -1;
    for (int y = 0; y < fittedFramebuffer.height(); ++y)
    {
        for (int x = 0; x < fittedFramebuffer.width(); ++x)
        {
            const QColor pixel = fittedFramebuffer.pixelColor(x, y);
            if (pixel.red() > 180 && pixel.green() < 120)
            {
                fittedMarkerMinX = std::min(fittedMarkerMinX, x);
                fittedMarkerMaxX = std::max(fittedMarkerMaxX, x);
                fittedMarkerMinY = std::min(fittedMarkerMinY, y);
                fittedMarkerMaxY = std::max(fittedMarkerMaxY, y);
            }
        }
    }
    const int fittedMarkerWidth = fittedMarkerMaxX - fittedMarkerMinX + 1;
    const int fittedMarkerHeight = fittedMarkerMaxY - fittedMarkerMinY + 1;
    check(!fittedFramebuffer.isNull()
            && fittedFramebuffer.size() == QSize(100, 100)
            && std::abs(fittedMarkerWidth - fittedMarkerHeight) <= 1,
        "authoritative model thumbnail fitting preserves framebuffer aspect ratio instead of squeezing models horizontally");

    const std::uint64_t staleRenderEpoch =
        ui::detail::currentSharedPreviewCacheEpoch();
    ui::detail::clearSharedPreviewAssetCaches();
    QImage staleImage(32, 32, QImage::Format_ARGB32_Premultiplied);
    staleImage.fill(QColor(255, 0, 0));
    const bool staleImagePublished =
        ui::detail::storeAuthoritativeSharedModelPreviewImage(
            &resources,
            {},
            "preview.3do",
            QSize(32, 32),
            false,
            staleRenderEpoch,
            staleImage);
    const std::string currentImageKey =
        ui::detail::sharedModelPreviewIconCacheKey(
            &resources,
            {},
            "preview.3do",
            QSize(32, 32),
            false);
    check(!staleImagePublished
            && !ui::detail::findSharedTemplateModelPreviewImage(
                currentImageKey).has_value(),
        "a worker completing after cache invalidation cannot publish stale thumbnail pixels into the new resource epoch");

    QImage livePublishedImage(32, 32, QImage::Format_ARGB32_Premultiplied);
    livePublishedImage.fill(QColor(40, 180, 220));
    const std::uint64_t livePublishedEpoch =
        ui::detail::currentSharedPreviewCacheEpoch();
    check(ui::detail::storeAuthoritativeSharedModelPreviewImage(
              &resources,
              {},
              "preview.3do",
              QSize(32, 32),
              false,
              livePublishedEpoch,
              livePublishedImage),
        "worker thumbnail publication exposes its exact revision-keyed image to the GUI handoff");
    std::unordered_map<std::string, QIcon> liveIconCache{};
    std::shared_ptr<ui::detail::GlViewModelThumbnailRenderer>
        liveIconRenderer{};
    QIcon firstPublishedIcon{};
    bool firstPublishedLookupReadRevision = true;
    const auto firstPublishedResult =
        ui::detail::loadSharedModelPreviewIcon(
            liveIconCache,
            liveIconRenderer,
            std::make_shared<const assets::ResourceLocator>(resources),
            ProjectType::JKDF2,
            {},
            "preview.3do",
            QSize(32, 32),
            false,
            0,
            [](const std::string&) {},
            firstPublishedIcon,
            &firstPublishedLookupReadRevision);
    QIcon secondPublishedIcon{};
    bool secondPublishedLookupReadRevision = true;
    const auto secondPublishedResult =
        ui::detail::loadSharedModelPreviewIcon(
            liveIconCache,
            liveIconRenderer,
            std::make_shared<const assets::ResourceLocator>(resources),
            ProjectType::JKDF2,
            {},
            "preview.3do",
            QSize(32, 32),
            false,
            0,
            [](const std::string&) {},
            secondPublishedIcon,
            &secondPublishedLookupReadRevision);
    check(firstPublishedResult
                == ui::detail::SharedModelPreviewIconLoadResult::rendered
            && secondPublishedResult
                == ui::detail::SharedModelPreviewIconLoadResult::assigned,
        "GUI thumbnail publication distinguishes one new QImage-to-QIcon conversion from a later local icon-cache hit");
    check(!firstPublishedLookupReadRevision
            && !secondPublishedLookupReadRevision,
        "GUI thumbnail publication trusts the worker's epoch-scoped exact key instead of re-probing 3DO, CMP, and MAT revisions");
    check(!firstPublishedIcon.isNull()
            && firstPublishedIcon.cacheKey() == secondPublishedIcon.cacheKey(),
        "coalesced GUI thumbnail handoff preserves the exact icon across first publication and reuse");

    check(ui::detail::previewThumbnailPageStep(420, 166) == 166
            && ui::detail::previewThumbnailPageStep(840, 166) == 664
            && ui::detail::previewThumbnailPageStep(120, 166) == 120,
        "thumbnail page scrolling keeps one complete tile row visible as overlap without stalling a short viewport");

    const std::vector<std::string> materials{ "preview" };
    const std::string initial = ui::detail::previewModelDependencyRevisionToken(
        &resources,
        "preview",
        "preview",
        materials);

    check(writeFixture(modelPath, QByteArray("model-v2-longer")), "preview revision fixture updates 3DO");
    const std::string afterModel = ui::detail::previewModelDependencyRevisionToken(
        &resources,
        "preview",
        "preview",
        materials);
    check(afterModel != initial, "3DO revision invalidates the model preview dependency key");

    check(writeFixture(materialPath, QByteArray("mat-v2-longer")), "preview revision fixture updates MAT");
    const std::string afterMaterial = ui::detail::previewModelDependencyRevisionToken(
        &resources,
        "preview",
        "preview",
        materials);
    check(afterMaterial != afterModel, "MAT revision invalidates the model preview dependency key");

    check(writeFixture(cmpPath, QByteArray("cmp-v2-longer")), "preview revision fixture updates CMP");
    const std::string afterCmp = ui::detail::previewModelDependencyRevisionToken(
        &resources,
        "preview",
        "preview",
        materials);
    check(afterCmp != afterMaterial, "CMP revision invalidates the model preview dependency key");

    const ui::detail::PreviewResourceDiskWatchPlan directModelPlan =
        ui::detail::previewResourceDiskWatchPlan(&resources, "preview", ".3do");
    check(
        containsWatchPath(directModelPlan.files, modelPath.toStdString()),
        "preview watch plan includes a direct-root 3DO selected by ResourceLocator");
    check(
        containsWatchPath(directModelPlan.directories, fixtureDir.path().toStdString()),
        "preview watch plan includes the selected file parent for atomic replacement");

    QTemporaryDir overrideDir;
    check(overrideDir.isValid(), "preview watch-plan test creates an override root");
    if (!overrideDir.isValid())
    {
        settlePreviewTestState();
        return;
    }
    check(
        QDir(fixtureDir.path()).mkpath(QStringLiteral("3do"))
            && QDir(overrideDir.path()).mkpath(QStringLiteral("3do"))
            && QDir(overrideDir.path()).mkpath(QStringLiteral("mat"))
            && QDir(overrideDir.path()).mkpath(QStringLiteral("misc/cmp")),
        "preview watch-plan fixture creates ResourceLocator candidate folders");

    const QString lowerPriorityModel = fixtureDir.filePath(QStringLiteral("3do/priority.3do"));
    const QString higherPriorityModel = overrideDir.filePath(QStringLiteral("3do/priority.3do"));
    const QString nestedMaterial = overrideDir.filePath(QStringLiteral("mat/priority.mat"));
    const QString nestedCmp = overrideDir.filePath(QStringLiteral("misc/cmp/priority.cmp"));
    check(writeFixture(lowerPriorityModel, QByteArray("lower")), "preview watch-plan writes lower-priority 3DO");
    check(writeFixture(higherPriorityModel, QByteArray("higher")), "preview watch-plan writes higher-priority 3DO");
    check(writeFixture(nestedMaterial, QByteArray("material")), "preview watch-plan writes MAT candidate");
    check(writeFixture(nestedCmp, QByteArray("palette")), "preview watch-plan writes CMP candidate");

    resources.addRoot(overrideDir.path().toStdString());
    const ui::detail::PreviewResourceDiskWatchPlan priorityPlan =
        ui::detail::previewResourceDiskWatchPlan(&resources, "priority", ".3do");
    check(
        containsWatchPath(priorityPlan.files, higherPriorityModel.toStdString())
            && !containsWatchPath(priorityPlan.files, lowerPriorityModel.toStdString()),
        "preview watch plan selects only ResourceLocator's highest-priority 3DO");
    check(
        containsWatchPath(
            ui::detail::previewResourceDiskWatchPlan(&resources, "priority", ".mat").files,
            nestedMaterial.toStdString()),
        "preview watch plan follows the MAT candidate folders");
    check(
        containsWatchPath(
            ui::detail::previewResourceDiskWatchPlan(&resources, "priority", ".cmp").files,
            nestedCmp.toStdString()),
        "preview watch plan follows the CMP candidate folders");

    int changeNotifications = 0;
    ui::detail::PreviewResourceDiskWatcher watcher(
        nullptr,
        [&changeNotifications]()
        {
            ++changeNotifications;
        },
        0);
    watcher.setResourceLocator(&resources);
    watcher.watchResource("priority", ".3do");
    watcher.watchResource("priority", ".mat");
    watcher.watchResource("priority", ".cmp");
    const std::uint64_t initialWatchRevisionProbeCount =
        watcher.revisionProbeCountForTest();
    for (int retry = 0; retry < 100; ++retry)
    {
        watcher.watchResource("PRIORITY", ".3DO");
        watcher.watchResource("priority", ".mat");
        watcher.watchResource("Priority", ".Cmp");
    }
    check(watcher.revisionProbeCountForTest()
            == initialWatchRevisionProbeCount,
        "pending thumbnail retries do not repeat disk/archive revision probes for already watched 3DO, MAT, or CMP dependencies");
    watcher.syncNow();
    const std::uint64_t initialWatchPlanDependencyVisits =
        watcher.watchPlanDependencyVisitCountForTest();
    watcher.watchResource("new_visible_model", ".3do");
    watcher.syncNow(false);
    check(watcher.watchPlanDependencyVisitCountForTest()
            == initialWatchPlanDependencyVisits + 1u,
        "adding a later visible thumbnail extends the file-watch plan incrementally instead of rescanning every accumulated model and material");
    check(
        watcher.watchedFiles().size() == 3,
        "preview disk watcher installs exact 3DO, MAT, and CMP file watches");
    watcher.notifyPathChangedForTest();
    watcher.notifyPathChangedForTest();
    QCoreApplication::processEvents(QEventLoop::AllEvents);
    check(
        changeNotifications == 1,
        "preview disk watcher coalesces a burst into one cache-invalidating callback");

    struct MissingResourceFixture
    {
        const char* stem;
        const char* extension;
    };
    constexpr std::array missingResourceFixtures{
        MissingResourceFixture{ "missing_model", ".3do" },
        MissingResourceFixture{ "missing_sprite", ".spr" },
        MissingResourceFixture{ "missing_material", ".mat" },
        MissingResourceFixture{ "missing_colormap", ".cmp" },
        MissingResourceFixture{ "missing_sound", ".wav" },
        MissingResourceFixture{ "missing_soundclass", ".snd" },
        MissingResourceFixture{ "missing_ai", ".ai" },
        MissingResourceFixture{ "missing_key", ".key" },
        MissingResourceFixture{ "missing_puppet", ".pup" },
        MissingResourceFixture{ "missing_particle", ".par" },
        MissingResourceFixture{ "missing_per", ".per" },
        MissingResourceFixture{ "missing_cog", ".cog" },
        MissingResourceFixture{ "missing_template", ".tpl" },
        MissingResourceFixture{ "missing_strings", ".uni" },
        MissingResourceFixture{ "missing_data", ".dat" },
    };
    for (const MissingResourceFixture& fixture : missingResourceFixtures)
    {
        watcher.watchResource(fixture.stem, fixture.extension);
    }
    watcher.syncNow();
    const int notificationsBeforeUnrelatedWrite = changeNotifications;
    check(
        writeFixture(
            overrideDir.filePath(QStringLiteral("unrelated-autosave.tmp")),
            QByteArray("not-a-resource")),
        "preview watcher fixture writes an unrelated sibling file");
    watcher.notifyDirectoryPathChangedForTest();
    QCoreApplication::processEvents(QEventLoop::AllEvents);
    check(
        changeNotifications == notificationsBeforeUnrelatedWrite,
        "preview disk watcher ignores unrelated writes while every resource category is missing");

    int expectedMissingResourceNotifications = notificationsBeforeUnrelatedWrite;
    for (const MissingResourceFixture& fixture : missingResourceFixtures)
    {
        const QString createdResource = overrideDir.filePath(
            QString::fromLatin1(fixture.stem) + QString::fromLatin1(fixture.extension));
        check(
            writeFixture(createdResource, QByteArray("resource revision fixture\n")),
            "preview watcher fixture creates a previously missing resource category");
        watcher.notifyDirectoryPathChangedForTest();
        watcher.flushPendingChangeForTest();
        ++expectedMissingResourceNotifications;
        check(
            changeNotifications == expectedMissingResourceNotifications,
            "preview disk watcher invalidates exactly once when a missing resource is created");

        watcher.notifyDirectoryPathChangedForTest();
        QCoreApplication::processEvents(QEventLoop::AllEvents);
        check(
            changeNotifications == expectedMissingResourceNotifications,
            "preview disk watcher refreshes each resource category revision after invalidation");
    }

    const QString archivePath = fixtureDir.filePath(QStringLiteral("preview.gob"));
    check(
        writeGobFixture(archivePath, QByteArray("3do\\inside.3do"), QByteArray("model-one")),
        "preview archive watcher creates a mounted GOB fixture");
    try
    {
        resources.mountGob(std::filesystem::path(archivePath.toStdWString()));
        int archiveChangeNotifications = 0;
        watcher.setArchiveChangeCallback(
            [&resources, &archiveChangeNotifications]()
            {
                resources.reloadGobs();
                ++archiveChangeNotifications;
                return true;
            });
        watcher.syncNow();
        const std::filesystem::path normalizedArchivePath =
            ui::detail::normalizedPreviewWatchPath(archivePath.toStdWString());
        check(
            watcher.watchedFiles().contains(
                QString::fromStdWString(normalizedArchivePath.wstring())),
            "preview disk watcher watches mounted GOB/GOO files without a loose-resource dependency");

        check(
            writeGobFixture(archivePath, QByteArray("3do\\replacement.3do"), QByteArray("model-two")),
            "preview archive watcher replaces the mounted GOB fixture");
        watcher.notifyArchivePathChangedForTest();
        watcher.notifyArchivePathChangedForTest();
        watcher.flushPendingChangeForTest();
        const auto replacedBytes = resources.tryReadBinary("replacement.3do");
        check(
            archiveChangeNotifications == 1,
            "preview archive watcher coalesces replacement into one remount callback");
        check(
            replacedBytes.has_value(),
            "preview archive watcher publishes the replacement archive index before cache invalidation");
        check(
            !resources.tryReadBinary("inside.3do").has_value(),
            "preview archive watcher removes stale entries from the replaced archive index");
    }
    catch (const std::exception& e)
    {
        check(false, e.what());
    }

    settlePreviewTestState();
    check(ui::detail::previewWorkerPool().activeThreadCount() == 0
            && !ui::detail::sharedThumbnailRenderBusy()
            && ui::detail::previewWorkerCount() == groupInitialWorkerCount
            && ui::detail::previewPublishBatchCount()
                == groupInitialPublishBatchCount
            && ui::detail::sharedTemplateModelPreviewImageCache().size() == 0u
            && ui::detail::sharedTemplateModelPreviewImageHintCache().empty()
            && ui::detail::sharedPreviewCmpCache().empty()
            && ui::detail::sharedPreviewMaterialImageCache().size() == 0u
            && ui::detail::sharedPreviewSpriteImageCache().size() == 0u
            && ui::detail::sharedPreviewSpriteCache().size() == 0u
            && ui::detail::sharedPreviewModelCache().size() == 0u
            && ui::detail::sharedPreviewModelCacheHintCache().empty()
            && ui::detail::sharedPreviewModelBoxCache().empty()
            && ui::detail::sharedPreviewModelBoxHintCache().empty()
            && ui::detail::sharedPreviewModelRenderReadyCache().empty(),
        "render-preview group leaves no dedicated worker active, clears shared preview caches, and restores process-global tuning before the next test group");
}

} // namespace sed::app_tests
