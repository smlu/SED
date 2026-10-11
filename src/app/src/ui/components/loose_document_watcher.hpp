#pragma once

#include <QByteArray>
#include <QString>

#include <cstdint>
#include <functional>

class QWidget;

namespace sed::ui
{

struct LooseDocumentReloadResult
{
    bool loaded{};
    QByteArray acceptedFingerprint{};
    QString error{};
};

struct LooseDocumentWatchOptions
{
    QString path{};
    QByteArray acceptedFingerprint{};
    qint64 maximumFileBytes{16 * 1024 * 1024};
    std::function<bool()> stillCurrent{};
    std::function<std::uint64_t()> authoredRevision{};
    std::function<LooseDocumentReloadResult()> reload{};
};

// Watch metadata is transient. These operations never capture or replay a
// document journal, and callers supply the exact bytes' accepted SHA-256.
void watchLooseDocument(QWidget* owner, LooseDocumentWatchOptions options);
bool looseDocumentWatchMatches(const QWidget* owner, const QString& path, const QByteArray& acceptedFingerprint);
void acceptLooseDocumentSource(QWidget* owner, const QString& path, const QByteArray& acceptedFingerprint);
void requestLooseDocumentReload(QWidget* owner);
void stopLooseDocumentWatch(QWidget* owner);

} // namespace sed::ui
