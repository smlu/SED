#pragma once

#include <QDir>
#include <QString>

#include <filesystem>

namespace sed::ui
{

/** Returns a UI path using the current operating system's preferred separator. */
inline QString nativeDisplayPath(const QString& path)
{
    return QDir::toNativeSeparators(path);
}

/** Converts a filesystem path into native, user-facing text. */
inline QString nativeDisplayPath(const std::filesystem::path& path)
{
    return nativeDisplayPath(QString::fromStdWString(path.wstring()));
}

} // namespace sed::ui
