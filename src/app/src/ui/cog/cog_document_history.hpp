#pragma once

#include <QCborMap>
#include <QPlainTextEdit>
#include <QPointer>
#include <QString>

#include <vector>
#include <functional>

namespace sed::ui::cog
{
// Journals public QTextDocument edit boundaries without walking its Undo stack.
// Capture is const: restoring the native stack happens only during document load.
class CogDocumentHistory final : public QObject
{
public:
    explicit CogDocumentHistory(QPlainTextEdit* editor, std::function<void()> rebuilt = {});
    QCborMap capture() const;
    static bool validate(const QCborMap& history, const QString& current, bool modified);
    void restore(const QCborMap& history, const QString& current, bool modified);
    void reset(bool modified = false);
    void saved();
    void publishAvailability();

private:
    struct Splice
    {
        int position{};
        QString removed{};
        QString inserted{};
    };
    struct Command
    {
        std::vector<Splice> splices{};
        int steps{};
        qsizetype bytes{};
    };
    void changed(int position, int removed, int inserted);
    void rebuild();
    void trimAtBoundary();
    static bool apply(QString& text, const QCborMap& splice, bool redo);

    QPointer<QPlainTextEdit> editor_{};
    QString mirror_{};
    std::vector<Command> commands_{};
    int cursor_{};
    int clean_{};
    bool baselineDirty_{};
    bool replaying_{};
    bool newCommand_{};
    bool trimQueued_{};
    qsizetype bytes_{};
    std::function<void()> rebuilt_{};
};
}
