#include "ui/cog/cog_document_history.hpp"

#include <QCborArray>
#include <QScrollBar>
#include <QSignalBlocker>
#include <QTextCursor>
#include <QTextDocument>
#include <QTimer>

#include <algorithm>
#include <limits>

namespace sed::ui::cog
{
namespace
{
constexpr int kCommands = 1024;
constexpr qsizetype kHistoryBytes = 16 * 1024 * 1024;
constexpr qsizetype kTextCharacters = 64 * 1024 * 1024;

bool wellFormed(const QString& text)
{
    for ( qsizetype index = 0; index < text.size(); ++index )
    {
        if ( text.at(index).isHighSurrogate() )
        {
            if ( ++index == text.size() || !text.at(index).isLowSurrogate() )
            {
                return false;
            }
        }
        else if ( text.at(index).isLowSurrogate() )
        {
            return false;
        }
    }
    return true;
}

bool canonicalPlainText(const QString& text)
{
    return wellFormed(text) && !text.contains(QChar::Nbsp) &&
        !text.contains(QChar::LineSeparator) && !text.contains(QChar::ParagraphSeparator);
}

bool boundary(const QString& text, const qsizetype position)
{
    return position == 0 || position == text.size() ||
        !(text.at(position - 1).isHighSurrogate() && text.at(position).isLowSurrogate());
}
}

CogDocumentHistory::CogDocumentHistory(QPlainTextEdit* editor, std::function<void()> rebuilt)
    : QObject(editor), editor_(editor), mirror_(editor->toPlainText()), rebuilt_(std::move(rebuilt))
{
    connect(editor->document(), &QTextDocument::undoCommandAdded, this, [this]()
    {
        if ( !replaying_ )
        {
            newCommand_ = true;
        }
    });
    connect(editor->document(), &QTextDocument::contentsChange, this,
            [this](const int position, const int removed, const int inserted)
    {
        changed(position, removed, inserted);
    });
}

void CogDocumentHistory::reset(const bool modified)
{
    commands_.clear();
    bytes_ = 0;
    cursor_ = clean_ = 0;
    baselineDirty_ = modified;
    newCommand_ = false;
    mirror_ = editor_ ? editor_->toPlainText() : QString{};
}

void CogDocumentHistory::saved()
{
    clean_ = cursor_;
    baselineDirty_ = false;
}

void CogDocumentHistory::changed(const int position, const int removed, const int inserted)
{
    if ( replaying_ || !editor_ )
    {
        return;
    }
    const int maximum = std::max(0, editor_->document()->characterCount() - 1);
    const int start = std::clamp(position, 0, maximum);
    QTextCursor text(editor_->document());
    text.setPosition(start);
    text.setPosition(start + std::min(inserted, maximum - start), QTextCursor::KeepAnchor);
    QString added = text.selectedText();
    added.replace(QChar::ParagraphSeparator, QLatin1Char('\n'));
    added.replace(QChar::LineSeparator, QLatin1Char('\n'));
    added.replace(QChar::Nbsp, QLatin1Char(' '));
    const int oldStart = std::min(position, static_cast<int>(mirror_.size()));
    const QString deleted = mirror_.mid(oldStart, std::min(removed, static_cast<int>(mirror_.size()) - oldStart));
    const int steps = editor_->document()->availableUndoSteps();
    if ( !newCommand_ )
    {
        int target = steps == 0 ? 0 : -1;
        for ( int index = 0; index < static_cast<int>(commands_.size()); ++index )
        {
            if ( commands_[static_cast<std::size_t>(index)].steps == steps )
            {
                target = index + 1;
                break;
            }
        }
        if ( target >= 0 && target != cursor_ )
        {
            // Native Undo/Redo has already changed the document. Only move
            // the journal cursor; never manufacture another edit command.
            cursor_ = target;
            mirror_.replace(oldStart, deleted.size(), added);
            return;
        }
        if ( steps == 0 )
        {
            // setPlainText / external replacement clears Qt's native stack.
            reset(editor_->document()->isModified());
            return;
        }
    }
    // Replacing a nonempty selection with identical text still advances Qt's
    // native command and clean-marker positions. Only empty events are noise.
    if ( deleted.isEmpty() && added.isEmpty() )
    {
        newCommand_ = false;
        return;
    }
    if ( newCommand_ || commands_.empty() || cursor_ == 0 )
    {
        if ( clean_ > cursor_ )
        {
            clean_ = -1;
        }
        while ( commands_.size() > static_cast<std::size_t>(cursor_) )
        {
            bytes_ -= commands_.back().bytes;
            commands_.pop_back();
        }
        commands_.push_back(Command{{}, steps, 0});
        ++cursor_;
    }
    Command& command = commands_[static_cast<std::size_t>(cursor_ - 1)];
    const bool mergeInsert = !command.splices.empty() && deleted.isEmpty() && command.splices.back().removed.isEmpty() &&
        oldStart == command.splices.back().position + command.splices.back().inserted.size();
    const bool mergeDelete = !command.splices.empty() && added.isEmpty() && command.splices.back().inserted.isEmpty() &&
        oldStart + deleted.size() == command.splices.back().position;
    const qsizetype cost = 3 * (deleted.size() + added.size()) + (mergeInsert || mergeDelete ? 0 : 128);
    command.bytes += cost;
    bytes_ += cost;
    if ( mergeInsert )
    {
        command.splices.back().inserted.append(added);
    }
    else if ( mergeDelete )
    {
        command.splices.back().position = oldStart;
        command.splices.back().removed.prepend(deleted);
    }
    else
    {
        command.splices.push_back({oldStart, deleted, added});
    }
    command.steps = steps;
    mirror_.replace(oldStart, deleted.size(), added);
    newCommand_ = false;
    if ( (commands_.size() > kCommands || bytes_ > kHistoryBytes / 2) && !trimQueued_ )
    {
        trimQueued_ = true;
        QTimer::singleShot(0, this, [this]()
        {
            trimQueued_ = false;
            trimAtBoundary();
        });
    }
}

QCborMap CogDocumentHistory::capture() const
{
    QCborArray commands;
    for ( const Command& command : commands_ )
    {
        QCborArray splices;
        for ( const Splice& splice : command.splices )
        {
            splices.append(QCborMap{{QStringLiteral("position"), splice.position},
                                   {QStringLiteral("removed"), splice.removed},
                                   {QStringLiteral("inserted"), splice.inserted}});
        }
        commands.append(splices);
    }
    int cursor = cursor_;
    int clean = clean_;
    QCborMap history{{QStringLiteral("version"), 1}, {QStringLiteral("cursor"), cursor},
                     {QStringLiteral("clean"), clean}, {QStringLiteral("baselineDirty"), baselineDirty_},
                     {QStringLiteral("commands"), commands}};
    while ( commands.size() > kCommands || QCborValue(history).toCbor().size() > kHistoryBytes )
    {
        if ( commands.isEmpty() )
        {
            break;
        }
        if ( cursor > 0 )
        {
            commands.removeFirst();
            --cursor;
            clean = std::max(-1, clean - 1);
        }
        else
        {
            commands.removeLast();
            if ( clean > commands.size() )
            {
                clean = -1;
            }
        }
        history.insert(QStringLiteral("cursor"), cursor);
        history.insert(QStringLiteral("clean"), clean);
        history.insert(QStringLiteral("commands"), commands);
    }
    return history;
}

bool CogDocumentHistory::apply(QString& text, const QCborMap& splice, const bool redo)
{
    const QCborValue position = splice.value(QStringLiteral("position"));
    const QCborValue removed = splice.value(QStringLiteral("removed"));
    const QCborValue inserted = splice.value(QStringLiteral("inserted"));
    if ( !position.isInteger() || position.toInteger() < 0 || position.toInteger() > text.size() ||
         !removed.isString() || !inserted.isString() )
    {
        return false;
    }
    const QString old = redo ? removed.toString() : inserted.toString();
    const QString next = redo ? inserted.toString() : removed.toString();
    if ( (old.isEmpty() && next.isEmpty()) || !canonicalPlainText(old) || !canonicalPlainText(next) ||
         !boundary(text, position.toInteger()) ||
         old.size() > text.size() - position.toInteger() ||
         !boundary(text, position.toInteger() + old.size()) ||
         text.mid(static_cast<int>(position.toInteger()), old.size()) != old ||
         next.size() > kTextCharacters - (text.size() - old.size()) )
    {
        return false;
    }
    text.replace(static_cast<int>(position.toInteger()), old.size(), next);
    return true;
}

bool CogDocumentHistory::validate(const QCborMap& history, const QString& current, const bool modified)
{
    const QCborValue commands = history.value(QStringLiteral("commands"));
    const QCborValue cursor = history.value(QStringLiteral("cursor"));
    const QCborValue clean = history.value(QStringLiteral("clean"));
    if ( !canonicalPlainText(current) || !history.value(QStringLiteral("version")).isInteger() || history.value(QStringLiteral("version")).toInteger() != 1 ||
         !commands.isArray() || commands.toArray().size() > kCommands ||
         !cursor.isInteger() || cursor.toInteger() < 0 || cursor.toInteger() > commands.toArray().size() ||
         !clean.isInteger() || clean.toInteger() < -1 || clean.toInteger() > commands.toArray().size() ||
         !history.value(QStringLiteral("baselineDirty")).isBool() || QCborValue(history).toCbor().size() > kHistoryBytes )
    {
        return false;
    }
    if ( modified != (history.value(QStringLiteral("baselineDirty")).toBool() || cursor.toInteger() != clean.toInteger()) )
    {
        return false;
    }
    QString text = current;
    const auto applyCommand = [&text](const QCborValue& command, const bool redo)
    {
        if ( !command.isArray() || command.toArray().isEmpty() || command.toArray().size() > 65536 )
        {
            return false;
        }
        const QCborArray splices = command.toArray();
        for ( int index = 0; index < splices.size(); ++index )
        {
            const QCborValue value = splices.at(redo ? index : static_cast<int>(splices.size()) - 1 - index);
            if ( !value.isMap() || !apply(text, value.toMap(), redo) )
            {
                return false;
            }
        }
        return true;
    };
    for ( int index = static_cast<int>(cursor.toInteger()); index > 0; --index )
    {
        if ( !applyCommand(commands.toArray().at(index - 1), false) )
        {
            return false;
        }
    }
    for ( int index = 0; index < commands.toArray().size(); ++index )
    {
        if ( !applyCommand(commands.toArray().at(index), true) || (index + 1 == cursor.toInteger() && text != current) )
        {
            return false;
        }
    }
    return true;
}

void CogDocumentHistory::rebuild()
{
    if ( !editor_ )
    {
        return;
    }
    const int position = editor_->textCursor().position();
    const int anchor = editor_->textCursor().anchor();
    const int vertical = editor_->verticalScrollBar()->value();
    const int horizontal = editor_->horizontalScrollBar()->value();
    QString base = mirror_;
    for ( int index = cursor_; index > 0; --index )
    {
        const auto& splices = commands_[static_cast<std::size_t>(index - 1)].splices;
        for ( auto it = splices.rbegin(); it != splices.rend(); ++it )
        {
            base.replace(it->position, it->inserted.size(), it->removed);
        }
    }
    replaying_ = true;
    {
        const QSignalBlocker editorBlocker(editor_);
        const QSignalBlocker documentBlocker(editor_->document());
        editor_->setPlainText(base);
        editor_->document()->setModified(baselineDirty_ || clean_ != 0);
        for ( int index = 0; index < static_cast<int>(commands_.size()); ++index )
        {
            QTextCursor edit(editor_->document());
            edit.beginEditBlock();
            for ( const Splice& splice : commands_[static_cast<std::size_t>(index)].splices )
            {
                edit.setPosition(splice.position);
                edit.setPosition(splice.position + static_cast<int>(splice.removed.size()), QTextCursor::KeepAnchor);
                edit.insertText(splice.inserted);
            }
            edit.endEditBlock();
            commands_[static_cast<std::size_t>(index)].steps = editor_->document()->availableUndoSteps();
            if ( !baselineDirty_ && index + 1 == clean_ )
            {
                editor_->document()->setModified(false);
            }
        }
        for ( int index = static_cast<int>(commands_.size()); index > cursor_; --index )
        {
            editor_->undo();
        }
        QTextCursor restored(editor_->document());
        restored.setPosition(std::min(anchor, static_cast<int>(mirror_.size())));
        restored.setPosition(std::min(position, static_cast<int>(mirror_.size())), QTextCursor::KeepAnchor);
        editor_->setTextCursor(restored);
        editor_->verticalScrollBar()->setValue(vertical);
        editor_->horizontalScrollBar()->setValue(horizontal);
    }
    replaying_ = false;
    newCommand_ = false;
    publishAvailability();
}

void CogDocumentHistory::publishAvailability()
{
    if ( !editor_ )
    {
        return;
    }
    const QPointer<CogDocumentHistory> self(this);
    const QPointer<QTextDocument> document(editor_->document());
    const bool undo = document->isUndoAvailable();
    const bool redo = document->isRedoAvailable();
    const bool modified = document->isModified();
    document->undoAvailable(undo);
    if ( !self || !document )
    {
        return;
    }
    document->redoAvailable(redo);
    if ( !self || !document )
    {
        return;
    }
    document->modificationChanged(modified);
}

void CogDocumentHistory::restore(const QCborMap& history, const QString& current, const bool modified)
{
    if ( history.isEmpty() )
    {
        replaying_ = true;
        editor_->setPlainText(current);
        editor_->document()->setModified(modified);
        replaying_ = false;
        reset(modified);
        return;
    }
    commands_.clear();
    cursor_ = static_cast<int>(history.value(QStringLiteral("cursor")).toInteger());
    bytes_ = 0;
    clean_ = static_cast<int>(history.value(QStringLiteral("clean")).toInteger());
    baselineDirty_ = history.value(QStringLiteral("baselineDirty")).toBool();
    for ( const QCborValue& value : history.value(QStringLiteral("commands")).toArray() )
    {
        Command command;
        for ( const QCborValue& value : value.toArray() )
        {
            const QCborMap splice = value.toMap();
            command.splices.push_back({static_cast<int>(splice.value(QStringLiteral("position")).toInteger()),
                                      splice.value(QStringLiteral("removed")).toString(),
                                      splice.value(QStringLiteral("inserted")).toString()});
            command.bytes += QCborValue(splice).toCbor().size();
        }
        bytes_ += command.bytes;
        commands_.push_back(std::move(command));
    }
    mirror_ = current;
    const QPointer<CogDocumentHistory> self(this);
    rebuild();
    if ( !self || !editor_ )
    {
        return;
    }
    if ( editor_->document()->isModified() != modified )
    {
        editor_->document()->setModified(modified);
    }
}

void CogDocumentHistory::trimAtBoundary()
{
    if ( (commands_.size() <= kCommands && bytes_ <= kHistoryBytes / 2) || !editor_ )
    {
        return;
    }
    // Retain a bounded recent window in both the journal and native Qt stack.
    // This rare load-style rebuild runs after a complete editing command,
    // never from capture, ordinary typing or Undo/Redo observation.
    int drop = std::min(cursor_, std::max(0, static_cast<int>(commands_.size()) - kCommands + 128));
    qsizetype retainedBytes = bytes_;
    for ( int index = 0; index < drop; ++index )
    {
        retainedBytes -= commands_[static_cast<std::size_t>(index)].bytes;
    }
    while ( retainedBytes > kHistoryBytes / 2 && drop < cursor_ )
    {
        retainedBytes -= commands_[static_cast<std::size_t>(drop++)].bytes;
    }
    if ( drop <= 0 )
    {
        return;
    }
    commands_.erase(commands_.begin(), commands_.begin() + drop);
    bytes_ = retainedBytes;
    cursor_ -= drop;
    clean_ = clean_ < drop ? -1 : clean_ - drop;
    const QPointer<CogDocumentHistory> self(this);
    const auto rebuilt = rebuilt_;
    rebuild();
    if ( self && rebuilt )
    {
        rebuilt();
    }
}
}
