#include "app_logging.hpp"

#include "app_settings.hpp"

#include <algorithm>
#include <array>
#include <atomic>
#include <fstream>
#include <mutex>
#include <string>
#include <string_view>

#include <QAbstractButton>
#include <QApplication>
#include <QDateTime>
#include <QEvent>
#include <QMessageBox>
#include <QPointer>
#include <QSettings>
#include <QString>
#include <QStringView>

namespace sed
{

Q_LOGGING_CATEGORY(logApplication, "sed.application")
Q_LOGGING_CATEGORY(logGui, "sed.gui")
Q_LOGGING_CATEGORY(logCore, "sed.core")
Q_LOGGING_CATEGORY(logResources, "sed.resources")

namespace
{

constexpr std::uint32_t kMinimumLogMiB = 1;
constexpr std::uint32_t kMaximumLogMiB = 100;
constexpr auto kConfirmationConnectionProperty =
    "sed.logging.confirmation_connected";

enum class RuntimeLogCategory : std::uint8_t
{
    application,
    gui,
    core,
    resources,
};

std::atomic_bool loggingEnabled{ true };
std::atomic<ApplicationLogLevel> minimumLogLevel{
    ApplicationLogLevel::warning };
std::atomic_uint32_t maximumLogFileMiB{ 10 };
std::atomic_bool applicationCategoryEnabled{ true };
std::atomic_bool guiCategoryEnabled{ true };
std::atomic_bool coreCategoryEnabled{ true };
std::atomic_bool resourcesCategoryEnabled{ true };
std::atomic_uint64_t maximumLogFileBytesForTest{};
std::atomic<QtMessageHandler> priorMessageHandler{};

std::mutex logMutex{};

ApplicationLogLevel messageLevel(const QtMsgType type) noexcept
{
    switch (type)
    {
        case QtDebugMsg:
            return ApplicationLogLevel::debug;
        case QtInfoMsg:
            return ApplicationLogLevel::info;
        case QtWarningMsg:
            return ApplicationLogLevel::warning;
        case QtCriticalMsg:
        case QtFatalMsg:
            return ApplicationLogLevel::error;
    }
    return ApplicationLogLevel::info;
}

const char* messageTypeName(const QtMsgType type) noexcept
{
    switch (type)
    {
        case QtDebugMsg:
            return "Debug";
        case QtInfoMsg:
            return "Info";
        case QtWarningMsg:
            return "Warning";
        case QtCriticalMsg:
            return "Error";
        case QtFatalMsg:
            return "Fatal";
    }
    return "Message";
}

bool isQtGuiSourceContext(const QMessageLogContext& context) noexcept
{
    const std::string_view file(context.file ? context.file : "");
    constexpr std::array<std::string_view, 6> guiSourceRoots{
        "/qtbase/src/gui/",
        "/qtbase/src/plugins/platforms/",
        "/qtbase/src/widgets/",
        "\\qtbase\\src\\gui\\",
        "\\qtbase\\src\\plugins\\platforms\\",
        "\\qtbase\\src\\widgets\\"
    };
    for (const std::string_view root : guiSourceRoots)
    {
        if (file.find(root) != std::string_view::npos)
        {
            return true;
        }
    }

    const std::string_view function(
        context.function ? context.function : "");
    constexpr std::array<std::string_view, 12> guiClassScopes{
        "QApplication::",
        "QBackingStore::",
        "QGuiApplication::",
        "QLayout::",
        "QOpenGLWidget::",
        "QPainter::",
        "QPlatformWindow::",
        "QWidget::",
        "QWidgetPrivate::",
        "QWidgetWindow::",
        "QWindowsWindow::",
        "QWindow::"
    };
    for (const std::string_view scope : guiClassScopes)
    {
        if (function.find(scope) != std::string_view::npos)
        {
            return true;
        }
    }
    return false;
}

bool isUncategorizedQtGuiDiagnostic(const QStringView message) noexcept
{
    constexpr std::array<QStringView, 12> guiDiagnosticPrefixes{
        u"External WM_DESTROY received for QWidgetWindow(",
        u"QApplication::",
        u"QBackingStore::",
        u"QGuiApplication::",
        u"QLayout::",
        u"QOpenGLWidget::",
        u"QPainter::",
        u"QPlatformWindow::",
        u"QWidget::",
        u"QWidgetWindow::",
        u"QWindowsWindow::",
        u"QWindow::"
    };
    return std::ranges::any_of(guiDiagnosticPrefixes,
        [message](const QStringView prefix)
        {
            return message.startsWith(prefix);
        });
}

RuntimeLogCategory runtimeCategory(
    const QMessageLogContext& context,
    const QStringView message) noexcept
{
    const QByteArrayView name(context.category ? context.category : "");
    if (name == QByteArrayView("sed.gui")
        || name.startsWith(QByteArrayView("qt.")))
    {
        return RuntimeLogCategory::gui;
    }
    if (name == QByteArrayView("sed.core"))
    {
        return RuntimeLogCategory::core;
    }
    if (name == QByteArrayView("sed.resources"))
    {
        return RuntimeLogCategory::resources;
    }
    if (name == QByteArrayView("sed.application"))
    {
        return RuntimeLogCategory::application;
    }
    if ((name.isEmpty() || name == QByteArrayView("default"))
        && (isQtGuiSourceContext(context)
            || isUncategorizedQtGuiDiagnostic(message)))
    {
        return RuntimeLogCategory::gui;
    }
    return RuntimeLogCategory::application;
}

const char* runtimeCategoryName(const RuntimeLogCategory category) noexcept
{
    switch (category)
    {
        case RuntimeLogCategory::application:
            return "App";
        case RuntimeLogCategory::gui:
            return "GUI";
        case RuntimeLogCategory::core:
            return "Core";
        case RuntimeLogCategory::resources:
            return "Resources";
    }
    return "App";
}

bool runtimeCategoryIsEnabled(
    const RuntimeLogCategory category) noexcept
{
    switch (category)
    {
        case RuntimeLogCategory::application:
            return applicationCategoryEnabled.load();
        case RuntimeLogCategory::gui:
            return guiCategoryEnabled.load();
        case RuntimeLogCategory::core:
            return coreCategoryEnabled.load();
        case RuntimeLogCategory::resources:
            return resourcesCategoryEnabled.load();
    }
    return true;
}

void rotateLogIfNeeded(
    const std::filesystem::path& path,
    const std::uint64_t incomingBytes)
{
    std::error_code error{};
    const std::uint64_t testMaximum = maximumLogFileBytesForTest.load();
    const std::uint64_t maximumBytes = testMaximum > 0
        ? testMaximum
        : static_cast<std::uint64_t>(maximumLogFileMiB.load())
            * 1024u * 1024u;
    const std::uint64_t currentBytes = std::filesystem::exists(path, error)
        ? std::filesystem::file_size(path, error)
        : 0;
    if (error || currentBytes <= maximumBytes
            - std::min(maximumBytes, incomingBytes))
    {
        return;
    }

    const std::filesystem::path previous = path.string() + ".1";
    std::filesystem::remove(previous, error);
    error.clear();
    std::filesystem::rename(path, previous, error);
}

bool writeLogRecord(
    const QtMsgType type,
    const RuntimeLogCategory category,
    const QString& message,
    const QMessageLogContext* context)
{
    if (!loggingEnabled.load()
        || messageLevel(type) < minimumLogLevel.load()
        || !runtimeCategoryIsEnabled(category))
    {
        return false;
    }

    std::lock_guard<std::mutex> lock(logMutex);
    try
    {
        std::string line =
            QDateTime::currentDateTime().toString(Qt::ISODateWithMs).toStdString()
            + " [" + messageTypeName(type) + "] ["
            + runtimeCategoryName(category) + "] "
            + message.toStdString();
        if (context && context->file && context->line > 0)
        {
            line += " (" + std::string(context->file) + ':'
                + std::to_string(context->line) + ')';
        }
        line.push_back('\n');

        const std::filesystem::path path = applicationLogPath();
        rotateLogIfNeeded(path, line.size());
        std::ofstream output(path, std::ios::binary | std::ios::app);
        if (output)
        {
            output.write(line.data(), static_cast<std::streamsize>(line.size()));
            output.flush();
            return static_cast<bool>(output);
        }
    }
    catch (...)
    {
        // Logging must never make an editor operation fail.
    }
    return false;
}

void applicationMessageHandler(
    const QtMsgType type,
    const QMessageLogContext& context,
    const QString& message)
{
    (void)writeLogRecord(
        type, runtimeCategory(context, message), message, &context);
    const QtMessageHandler prior = priorMessageHandler.load();
    if (prior && prior != applicationMessageHandler)
    {
        prior(type, context, message);
    }
}

QString standardButtonName(
    const QMessageBox::StandardButton button,
    const QMessageBox& messageBox)
{
    switch (button)
    {
        case QMessageBox::Ok:
            return QStringLiteral("OK");
        case QMessageBox::Save:
            return QStringLiteral("Save");
        case QMessageBox::SaveAll:
            return QStringLiteral("Save All");
        case QMessageBox::Open:
            return QStringLiteral("Open");
        case QMessageBox::Yes:
            return QStringLiteral("Yes");
        case QMessageBox::YesToAll:
            return QStringLiteral("Yes to All");
        case QMessageBox::No:
            return QStringLiteral("No");
        case QMessageBox::NoToAll:
            return QStringLiteral("No to All");
        case QMessageBox::Abort:
            return QStringLiteral("Abort");
        case QMessageBox::Retry:
            return QStringLiteral("Retry");
        case QMessageBox::Ignore:
            return QStringLiteral("Ignore");
        case QMessageBox::Close:
            return QStringLiteral("Close");
        case QMessageBox::Cancel:
            return QStringLiteral("Cancel");
        case QMessageBox::Discard:
            return QStringLiteral("Discard");
        case QMessageBox::Help:
            return QStringLiteral("Help");
        case QMessageBox::Apply:
            return QStringLiteral("Apply");
        case QMessageBox::Reset:
            return QStringLiteral("Reset");
        case QMessageBox::RestoreDefaults:
            return QStringLiteral("Restore Defaults");
        default:
            break;
    }

    if (const QAbstractButton* clicked = messageBox.clickedButton())
    {
        QString text = clicked->text();
        text.remove(QLatin1Char('&'));
        if (!text.trimmed().isEmpty())
        {
            return text.trimmed();
        }
    }
    return QStringLiteral("Closed");
}

QtMsgType confirmationMessageType(
    const QMessageBox::StandardButton button,
    const QMessageBox& messageBox) noexcept
{
    if (button == QMessageBox::Discard || button == QMessageBox::Abort)
    {
        return QtWarningMsg;
    }
    if (QAbstractButton* clicked = messageBox.clickedButton();
        clicked)
    {
        const QMessageBox::ButtonRole role = messageBox.buttonRole(clicked);
        if (role == QMessageBox::DestructiveRole)
        {
            return QtWarningMsg;
        }
        if (role == QMessageBox::RejectRole
            || role == QMessageBox::NoRole)
        {
            return QtDebugMsg;
        }
        if (messageBox.property(
                kMessageBoxLoggingDestructiveProperty).toBool()
            && (role == QMessageBox::YesRole
                || role == QMessageBox::AcceptRole
                || role == QMessageBox::ApplyRole))
        {
            return QtWarningMsg;
        }
    }
    if (button == QMessageBox::Cancel || button == QMessageBox::No
        || button == QMessageBox::NoToAll || button == QMessageBox::Close
        || button == QMessageBox::NoButton)
    {
        return QtDebugMsg;
    }
    if (messageBox.property(
            kMessageBoxLoggingDestructiveProperty).toBool()
        && (button == QMessageBox::Yes
            || button == QMessageBox::YesToAll
            || button == QMessageBox::Ok
            || button == QMessageBox::Apply))
    {
        return QtWarningMsg;
    }
    return QtInfoMsg;
}

bool isConfirmation(const QMessageBox& messageBox)
{
    if (messageBox.icon() == QMessageBox::Question)
    {
        return true;
    }

    const QMessageBox::StandardButtons buttons = messageBox.standardButtons();
    const QMessageBox::StandardButtons notificationButtons =
        QMessageBox::Ok | QMessageBox::Close;
    if ((buttons & ~notificationButtons) != QMessageBox::NoButton)
    {
        return true;
    }
    return messageBox.buttons().size() > 1;
}

void reportConfirmationResult(QMessageBox& messageBox, const int result)
{
    QMessageBox::StandardButton button = messageBox.standardButton(
        messageBox.clickedButton());
    if (button == QMessageBox::NoButton)
    {
        button = static_cast<QMessageBox::StandardButton>(result);
    }

    const QString title = messageBox.windowTitle().trimmed().isEmpty()
        ? QStringLiteral("Confirmation")
        : messageBox.windowTitle().trimmed();
    QString record = QStringLiteral("Confirmation '%1' resolved: %2")
        .arg(title, standardButtonName(button, messageBox));
    const QString context = messageBox.property(
        kMessageBoxLoggingContextProperty).toString().trimmed();
    if (!context.isEmpty())
    {
        record += QStringLiteral(". %1").arg(context);
    }
    const QString prompt = messageBox.text().trimmed();
    if (!prompt.isEmpty())
    {
        record += QStringLiteral(". Prompt: %1").arg(prompt);
    }
    writeLogRecord(confirmationMessageType(button, messageBox),
        RuntimeLogCategory::gui, record, nullptr);
}

class MessageBoxLogFilter final : public QObject
{
public:
    using QObject::QObject;

protected:
    bool eventFilter(QObject* watched, QEvent* event) override
    {
        if (!event || event->type() != QEvent::Show)
        {
            return QObject::eventFilter(watched, event);
        }

        auto* messageBox = qobject_cast<QMessageBox*>(watched);
        if (!messageBox || messageBox->property(
                kMessageBoxLoggingSkipProperty).toBool())
        {
            return QObject::eventFilter(watched, event);
        }

        if (isConfirmation(*messageBox))
        {
            if (!messageBox->property(
                    kConfirmationConnectionProperty).toBool())
            {
                messageBox->setProperty(
                    kConfirmationConnectionProperty, true);
                const QPointer<QMessageBox> guarded(messageBox);
                connect(messageBox, &QDialog::finished, this,
                    [guarded](const int result)
                    {
                        if (guarded && !guarded->property(
                                kMessageBoxLoggingSkipProperty).toBool())
                        {
                            reportConfirmationResult(*guarded, result);
                        }
                    });
            }
            return QObject::eventFilter(watched, event);
        }

        const QMessageBox::Icon icon = messageBox->icon();
        if (icon != QMessageBox::Warning
            && icon != QMessageBox::Critical)
        {
            return QObject::eventFilter(watched, event);
        }

        const QString message = messageBox->text().trimmed();
        const QtMsgType type = icon == QMessageBox::Critical
            ? QtCriticalMsg : QtWarningMsg;
        const QString title = messageBox->windowTitle().trimmed();
        const QString details = messageBox->detailedText().trimmed();
        QString record = title.isEmpty()
            ? message
            : QStringLiteral("%1: %2").arg(title, message);
        if (!details.isEmpty())
        {
            record += QStringLiteral(" Details: %1").arg(details);
        }
        writeLogRecord(type, RuntimeLogCategory::gui, record, nullptr);
        return QObject::eventFilter(watched, event);
    }
};

} // namespace

std::filesystem::path applicationLogPath()
{
    return appSettingsIniPath().parent_path() / "sed.log";
}

ApplicationLogSettings loadApplicationLogSettings()
{
    QSettings settings = settingsStore();
    ApplicationLogSettings result{};
    result.enabled = settings.value(
        QStringLiteral("logging/enabled"), true).toBool();
    const int configuredLevel = settings.value(
        QStringLiteral("logging/minimum_level"),
        static_cast<int>(ApplicationLogLevel::warning)).toInt();
    result.minimumLevel = static_cast<ApplicationLogLevel>(std::clamp(
        configuredLevel,
        static_cast<int>(ApplicationLogLevel::debug),
        static_cast<int>(ApplicationLogLevel::error)));
    result.maximumFileMiB = static_cast<std::uint32_t>(std::clamp(
        settings.value(QStringLiteral("logging/maximum_file_mib"), 10).toInt(),
        static_cast<int>(kMinimumLogMiB),
        static_cast<int>(kMaximumLogMiB)));
    result.applicationEnabled = settings.value(
        QStringLiteral("logging/category_application"), true).toBool();
    result.guiEnabled = settings.value(
        QStringLiteral("logging/category_gui"), true).toBool();
    result.coreEnabled = settings.value(
        QStringLiteral("logging/category_core"), true).toBool();
    result.resourcesEnabled = settings.value(
        QStringLiteral("logging/category_resources"), true).toBool();
    return result;
}

void reloadApplicationLogSettings()
{
    const ApplicationLogSettings settings = loadApplicationLogSettings();
    loggingEnabled.store(settings.enabled);
    minimumLogLevel.store(settings.minimumLevel);
    maximumLogFileMiB.store(settings.maximumFileMiB);
    applicationCategoryEnabled.store(settings.applicationEnabled);
    guiCategoryEnabled.store(settings.guiEnabled);
    coreCategoryEnabled.store(settings.coreEnabled);
    resourcesCategoryEnabled.store(settings.resourcesEnabled);
}

void installApplicationLogging(QApplication& application)
{
    reloadApplicationLogSettings();
    const QtMessageHandler previous =
        qInstallMessageHandler(applicationMessageHandler);
    if (previous != applicationMessageHandler)
    {
        priorMessageHandler.store(previous);
    }
    auto* filter = new MessageBoxLogFilter(&application);
    filter->setObjectName(QStringLiteral("applicationMessageBoxLogFilter"));
    application.installEventFilter(filter);
}

#ifdef SED_APP_MAIN_WINDOW_TEST_ACCESS
void setApplicationLogMaximumBytesForTest(
    const std::uint64_t maximumBytes)
{
    maximumLogFileBytesForTest.store(maximumBytes);
}
#endif

} // namespace sed
