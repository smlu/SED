#include "test_common.hpp"

#include "ui/components/preview_worker_pool.hpp"

#include <QApplication>
#include <QCoreApplication>
#include <QEvent>
#include <QEventLoop>
#include <QThreadPool>

#include <array>
#include <cstdio>
#include <string_view>
#include <vector>

namespace sed::app_tests
{

void runThingUtilityTests();
void runGeometryTransformPreviewUtilityTests();
void runAppSoundPlayerPcmDeviceTests();
void runRenderPreviewUtilityTests();
void runEditingPrecisionTests();
void runEditingHelperContractTests();
void runConsistencyGeometryContractTests();
void runScrubFloatPrecisionTests();
void runThingResourceCacheTests();
void runTemplatePrecisionEqualityTests();
void runAudioResourceIdentityTests();
void runAmbientAudioIdentityTests();
void runPreviewResourceFailureTests();
void runThingLightPrecisionTests();
void runDocumentRecoveryTests();
void runPartialLevelSalvageTests();
void runEpisodeUtilityTests();
void runLayerUtilityTests();
void runThingParameterSchemaTests();
void runThingShapeOverlayTests();
void runCleaveShapeUtilsTests();
void runCleaveMeasurementUtilsTests();
void runTemplateCollisionBoundsGuideTests();
void runThemeStorageTests();
void runViewportAaPreferenceTests();
void runViewportBannerSettingsTests();
void runToastOverlayPainterTests();
void runCleaveHudPresentationTests();
void runUndoLabelTests();

} // namespace sed::app_tests

int main(int argc, char** argv)
{
    QApplication app(argc, argv);
    struct TestGroup
    {
        std::string_view name{};
        void (*run)(){};
    };
    const std::array groups{
        TestGroup{ "app_sound_player_pcm_device",
            &sed::app_tests::runAppSoundPlayerPcmDeviceTests },
        TestGroup{ "render_preview",
            &sed::app_tests::runRenderPreviewUtilityTests },
        TestGroup{ "document_recovery",
            &sed::app_tests::runDocumentRecoveryTests },
        TestGroup{ "partial_level_salvage",
            &sed::app_tests::runPartialLevelSalvageTests },
        TestGroup{ "episode", &sed::app_tests::runEpisodeUtilityTests },
        TestGroup{ "layer", &sed::app_tests::runLayerUtilityTests },
        TestGroup{ "thing_parameter_schema",
            &sed::app_tests::runThingParameterSchemaTests },
        TestGroup{ "thing_shape_overlay",
            &sed::app_tests::runThingShapeOverlayTests },
        TestGroup{ "cleave_shape",
            &sed::app_tests::runCleaveShapeUtilsTests },
        TestGroup{ "cleave_measurements",
            &sed::app_tests::runCleaveMeasurementUtilsTests },
        TestGroup{ "template_collision_bounds_guide",
            &sed::app_tests::runTemplateCollisionBoundsGuideTests },
        TestGroup{ "theme_storage",
            &sed::app_tests::runThemeStorageTests },
        TestGroup{ "viewport_aa_preferences",
            &sed::app_tests::runViewportAaPreferenceTests },
        TestGroup{ "viewport_banner_settings",
            &sed::app_tests::runViewportBannerSettingsTests },
        TestGroup{ "toast_overlay_painter",
            &sed::app_tests::runToastOverlayPainterTests },
        TestGroup{ "cleave_hud_presentation",
            &sed::app_tests::runCleaveHudPresentationTests },
        TestGroup{ "thing_light_precision", &sed::app_tests::runThingLightPrecisionTests },
        TestGroup{ "preview_resource_failure", &sed::app_tests::runPreviewResourceFailureTests },
        TestGroup{ "ambient_audio_identity", &sed::app_tests::runAmbientAudioIdentityTests },
        TestGroup{ "audio_resource_identity", &sed::app_tests::runAudioResourceIdentityTests },
        TestGroup{ "template_precision", &sed::app_tests::runTemplatePrecisionEqualityTests },
        TestGroup{ "thing_resource_cache", &sed::app_tests::runThingResourceCacheTests },
        TestGroup{ "scrub_precision", &sed::app_tests::runScrubFloatPrecisionTests },
        TestGroup{ "editing_precision", &sed::app_tests::runEditingPrecisionTests },
        TestGroup{ "consistency_geometry_contract", &sed::app_tests::runConsistencyGeometryContractTests },
        TestGroup{ "editing_helper_contract", &sed::app_tests::runEditingHelperContractTests },
        TestGroup{ "thing", &sed::app_tests::runThingUtilityTests },
        TestGroup{ "geometry_transform_preview",
            &sed::app_tests::runGeometryTransformPreviewUtilityTests },
        TestGroup{ "undo_label", &sed::app_tests::runUndoLabelTests },
    };

    const auto printAvailableGroups = [&groups]()
    {
        std::fprintf(stderr, "Available groups:\n");
        for (const TestGroup& group : groups)
        {
            std::fprintf(stderr, "  %.*s\n",
                static_cast<int>(group.name.size()), group.name.data());
        }
    };

    std::vector<const TestGroup*> selectedGroups{};
    if (argc == 1)
    {
        selectedGroups.reserve(groups.size());
        for (const TestGroup& group : groups)
        {
            selectedGroups.push_back(&group);
        }
    }
    else
    {
        if ((argc - 1) % 2 != 0)
        {
            std::fprintf(stderr,
                "Usage: sed_app_tests.exe [--group <name> ...]\n");
            printAvailableGroups();
            return 2;
        }
        selectedGroups.reserve(static_cast<std::size_t>((argc - 1) / 2));
        for (int index = 1; index < argc; index += 2)
        {
            if (std::string_view(argv[index]) != "--group")
            {
                std::fprintf(stderr,
                    "Usage: sed_app_tests.exe [--group <name> ...]\n");
                printAvailableGroups();
                return 2;
            }

            const std::string_view requested = argv[index + 1];
            const TestGroup* selected = nullptr;
            for (const TestGroup& group : groups)
            {
                if (group.name == requested)
                {
                    selected = &group;
                    break;
                }
            }
            if (!selected)
            {
                std::fprintf(stderr, "Unknown test group: %.*s\n",
                    static_cast<int>(requested.size()), requested.data());
                printAvailableGroups();
                return 2;
            }
            selectedGroups.push_back(selected);
        }
    }

    for (const TestGroup* group : selectedGroups)
    {
        std::fprintf(stderr, "RUN %.*s\n",
            static_cast<int>(group->name.size()), group->name.data());
        std::fflush(stderr);
        group->run();
        std::fprintf(stderr, "DONE %.*s failures=%d\n",
            static_cast<int>(group->name.size()), group->name.data(),
            sed::app_tests::failures);
        std::fflush(stderr);

        // Worker-pool idleness precedes QFutureWatcher completion delivery.
        // Repeat the bounded two-stage delivery so one completion-triggered
        // replacement job also settles before the next group can reuse test
        // fixture stack addresses.
        for (int settlePass = 0; settlePass < 3; ++settlePass)
        {
            if (!QThreadPool::globalInstance()->waitForDone(30000))
            {
                sed::app_tests::check(false,
                    "app test module left a global thread-pool task running");
            }
            if (!sed::ui::detail::previewWorkerPool().waitForDone(30000))
            {
                sed::app_tests::check(false,
                    "app test module left a preview worker-pool task running");
            }
            QCoreApplication::sendPostedEvents(nullptr);
            QCoreApplication::sendPostedEvents(nullptr);
            QCoreApplication::sendPostedEvents(
                nullptr, QEvent::DeferredDelete);
            QCoreApplication::processEvents(QEventLoop::AllEvents);
        }
        const bool globalPoolSettled =
            QThreadPool::globalInstance()->waitForDone(30000);
        const bool previewPoolSettled =
            sed::ui::detail::previewWorkerPool().waitForDone(30000);
        sed::app_tests::check(globalPoolSettled && previewPoolSettled,
            "app test module queued asynchronous work throughout the between-group completion barrier");
    }
    return sed::app_tests::failures == 0 ? 0 : 1;
}
