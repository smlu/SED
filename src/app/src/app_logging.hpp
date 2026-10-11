#pragma once

#include <cstdint>
#include <filesystem>

#include <QLoggingCategory>

class QApplication;

namespace sed
{

enum class ApplicationLogLevel : std::uint8_t
{
    debug,
    info,
    warning,
    error,
};

struct ApplicationLogSettings
{
    bool enabled{ true };
    ApplicationLogLevel minimumLevel{ ApplicationLogLevel::warning };
    std::uint32_t maximumFileMiB{ 10 };
    bool applicationEnabled{ true };
    bool guiEnabled{ true };
    bool coreEnabled{ true };
    bool resourcesEnabled{ true };
};

Q_DECLARE_LOGGING_CATEGORY(logApplication)
Q_DECLARE_LOGGING_CATEGORY(logGui)
Q_DECLARE_LOGGING_CATEGORY(logCore)
Q_DECLARE_LOGGING_CATEGORY(logResources)

/** Skips automatic notification and resolved-confirmation file records. */
inline constexpr auto kMessageBoxLoggingSkipProperty = "sed.logging.skip";

/** Adds useful object/path identity to a resolved confirmation record. */
inline constexpr auto kMessageBoxLoggingContextProperty =
    "sed.logging.context";

/** Logs an affirmative Yes, OK, or Apply result as a warning. */
inline constexpr auto kMessageBoxLoggingDestructiveProperty =
    "sed.logging.destructive";

std::filesystem::path applicationLogPath();
ApplicationLogSettings loadApplicationLogSettings();

/** Installs categorized file logging and message-box outcome reporting. */
void installApplicationLogging(QApplication& application);

/** Applies logging preferences changed while the application is running. */
void reloadApplicationLogSettings();

#ifdef SED_APP_MAIN_WINDOW_TEST_ACCESS
/** Overrides the rotation threshold in tests; zero restores the MiB setting. */
void setApplicationLogMaximumBytesForTest(std::uint64_t maximumBytes);
#endif

} // namespace sed
