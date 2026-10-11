#include "test_common.hpp"
#include "ui/audio/audio_resource_identity_utils.hpp"
#include <QTemporaryDir>
#include <chrono>
#include <fstream>

namespace sed::app_tests
{
void runAmbientAudioIdentityTests()
{
    using namespace ui::audio;
    assets::ResourceLocator firstLocator, secondLocator;
    check(!ambientSoundTargetChanged("name", &firstLocator, "key", "name", &firstLocator, "key")
            && ambientSoundTargetChanged("name", &firstLocator, "key", "other", &firstLocator, "key")
            && ambientSoundTargetChanged("name", &firstLocator, "key", "name", &secondLocator, "key")
            && ambientSoundTargetChanged("name", &firstLocator, "key", "name", &firstLocator, "other")
            && !ambientSoundTargetChanged("", nullptr, "", "", nullptr, ""),
        "Ambient target identity tests each independently varying input and exact empty identity");
    for (const auto pending : {"A", "B"})
        for (const auto tracked : {"A", "B"})
            for (const auto live : {"A", "B"})
                check(ambientSoundResultCurrent(pending, tracked, live)
                        == (std::string_view(pending) == tracked && std::string_view(pending) == live),
                    "Ambient completion predicate covers every three-input equality combination");
    QTemporaryDir directory;
    check(directory.isValid(), "Ambient source revision fixture exists");
    if (!directory.isValid()) return;
    const auto root = std::filesystem::path(directory.path().toStdWString());
    assets::ResourceLocator resources;
    resources.addRoot(root);
    const auto missing = soundResourceRequestKey(resources, "ambient.wav");
    std::ofstream(root / "ambient.wav", std::ios::binary) << "created sound";
    const auto created = soundResourceRequestKey(resources, "ambient.wav");
    check(ambientSoundTargetChanged("ambient.wav", &resources, missing,
            "ambient.wav", &resources, created),
        "Ambient missing-resource negative cache invalidates when same target becomes available");
    check(!ambientSoundTargetChanged("ambient.wav", &resources, created,
            "ambient.wav", &resources, created),
        "Ambient unchanged source preserves current playback and avoids worker reload");
    std::ofstream(root / "ambient.wav", std::ios::binary) << "replaced sound bytes";
    std::filesystem::last_write_time(root / "ambient.wav",
        std::filesystem::file_time_type::clock::now() + std::chrono::seconds(10));
    const auto replaced = soundResourceRequestKey(resources, "ambient.wav");
    check(ambientSoundTargetChanged("ambient.wav", &resources, created,
            "ambient.wav", &resources, replaced),
        "Ambient active playback target invalidates on same-name source replacement");
    check(!ambientSoundResultCurrent(created, created, replaced),
        "Ambient completion rejects obsolete bytes before timer advances tracked generation");
    check(ambientSoundResultCurrent(replaced, replaced, replaced),
        "Ambient completion accepts exactly current resource revision");
}
}
