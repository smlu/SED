#include "ui/resources/resource_authoring_pane.hpp"
#include "ui/resources/resource_authoring_detail.hpp"
#include "ui/episode/episode_editor.hpp"

#include <QApplication>
#include <QSignalBlocker>

#include <algorithm>
#include <limits>

namespace sed::ui::resources::detail
{
namespace
{
constexpr int kHistoryCommands = 1024;
constexpr qsizetype kHistoryBytes = 16 * 1024 * 1024;

QCborArray strings(const std::array<QString, 5>& raw)
{
    QCborArray result;
    for ( const QString& value : raw )
    {
        result.append(value);
    }
    return result;
}

bool stringArray(const QCborValue& value, const int count)
{
    if ( !value.isArray() || value.toArray().size() != count )
    {
        return false;
    }
    for ( const QCborValue& cell : value.toArray() )
    {
        if ( !cell.isString() || cell.toString().size() > 4096 )
        {
            return false;
        }
    }
    return true;
}

bool rowRecordValid(const QCborValue& value, const bool sound)
{
    if ( !value.isMap() )
    {
        return false;
    }
    const QCborMap record = value.toMap();
    const QCborArray model = record.value(QStringLiteral("model")).toArray();
    if ( !stringArray(record.value(QStringLiteral("raw")), sound ? 6 : 3) ||
         !stringArray(record.value(QStringLiteral("model")), sound ? 6 : 3) ||
         (sound && !record.value(QStringLiteral("loop")).isBool()) )
    {
        return false;
    }
    for ( int index = 0; index < 2; ++index )
    {
        if ( !ui::episode::gameBytesForDisplayText(model.at(index).toString()) )
        {
            return false;
        }
    }
    if ( sound )
    {
        if ( record.value(QStringLiteral("raw")).toArray().at(0) != model.at(0) )
        {
            return false;
        }
        return parseHexFlags(model.at(2).toString()).has_value() &&
               parseFiniteFloat(model.at(3).toString()).has_value() &&
               parseFiniteFloat(model.at(4).toString()).has_value() &&
               parseFiniteFloat(model.at(5).toString()).has_value();
    }
    bool ok{};
    const qlonglong numeric = model.at(2).toString().toLongLong(&ok);
    return ok && numeric >= std::numeric_limits<std::int32_t>::min() &&
           numeric <= std::numeric_limits<std::int32_t>::max();
}

bool equivalentRow(QCborMap left, QCborMap right)
{
    // Looping is presentation, independent of a row's authored cell commands.
    left.remove(QStringLiteral("loop"));
    right.remove(QStringLiteral("loop"));
    return left == right;
}
}

QCborMap ResourceAuthoringDialog::spriteHistoryValue() const
{
    QCborArray values;
    values.append(spriteMaterial_->text());
    values.append(sprite_.type);
    values.append(floatText(sprite_.width));
    values.append(floatText(sprite_.height));
    values.append(sprite_.geometryMode);
    values.append(sprite_.lightMode);
    values.append(sprite_.textureMode);
    values.append(floatText(sprite_.extraLightRGBA[0]));
    for ( const float value : sprite_.extraLightRGBA )
    {
        values.append(floatText(value));
    }
    for ( const float value : sprite_.offsetXYZ )
    {
        values.append(floatText(value));
    }
    return {{QStringLiteral("sprite"), values}, {QStringLiteral("pending"), spritePendingNumericEdits_},
            {QStringLiteral("material"), ui::episode::displayTextForGameBytes(sprite_.material)}};
}

QCborMap ResourceAuthoringDialog::stringHistoryRow(const int row) const
{
    QCborArray raw;
    for ( int column = 0; column < 3; ++column )
    {
        const auto* item = stringTable_->item(row, column);
        raw.append(item ? item->text() : QString{});
    }
    const auto& entry = stringsFile_.entries.at(static_cast<std::size_t>(row));
    return {{QStringLiteral("raw"), raw}, {QStringLiteral("model"), QCborArray{
        ui::episode::displayTextForGameBytes(entry.key), ui::episode::displayTextForGameBytes(entry.value),
        QString::number(entry.numericValue)}}};
}

QCborMap ResourceAuthoringDialog::soundHistoryRow(const SoundRowRecord& row) const
{
    QCborArray raw;
    raw.append(ui::episode::displayTextForGameBytes(row.entry.mode));
    for ( const QCborValue& value : strings(row.raw) )
    {
        raw.append(value);
    }
    return {{QStringLiteral("raw"), raw}, {QStringLiteral("model"), QCborArray{
        ui::episode::displayTextForGameBytes(row.entry.mode), ui::episode::displayTextForGameBytes(row.entry.wav),
        QStringLiteral("0x%1").arg(row.entry.playFlags, 8, 16, QLatin1Char('0')),
        floatText(row.entry.minRadius), floatText(row.entry.maxRadius), floatText(row.entry.maxVolume)}},
        {QStringLiteral("loop"), row.previewLoop}};
}

void ResourceAuthoringDialog::resetDocumentHistory()
{
    documentHistory_.clear();
    documentHistoryBytes_ = 0;
    documentHistoryCursor_ = documentHistoryCleanCursor_ = 0;
    documentHistoryBaselineDirty_ = dirty_;
    spriteHistoryCurrent_.clear();
    spriteHistoryPendingKey_.clear();
    stringHistoryCurrent_.clear();
    if ( kind_ == ResourceAuthoringKind::sprite && spriteMaterial_ )
    {
        spriteHistoryCurrent_ = spriteHistoryValue();
    }
    else if ( kind_ == ResourceAuthoringKind::stringTable && stringTable_ )
    {
        for ( int row = 0; row < stringTable_->rowCount(); ++row )
        {
            stringHistoryCurrent_.push_back(stringHistoryRow(row));
        }
    }
    else if ( kind_ == ResourceAuthoringKind::soundClass )
    {
        soundHistoryCurrent_.clear();
        for ( int row = 0; row < static_cast<int>(soundFile_.entries.size()); ++row )
        {
            soundHistoryCurrent_.push_back(soundRowRecord(row));
        }
    }
}

void ResourceAuthoringDialog::appendDocumentHistoryCommand(QCborMap command)
{
    if ( applyingDocumentHistory_ || refreshing_ )
    {
        return;
    }
    if ( documentHistoryCleanCursor_ > documentHistoryCursor_ )
    {
        documentHistoryCleanCursor_ = -1;
    }
    while ( documentHistory_.size() > static_cast<std::size_t>(documentHistoryCursor_) )
    {
        documentHistoryBytes_ -= QCborValue(documentHistory_.back()).toCbor().size();
        documentHistory_.pop_back();
    }
    documentHistoryBytes_ += QCborValue(command).toCbor().size();
    documentHistory_.push_back(std::move(command));
    ++documentHistoryCursor_;
    while ( documentHistoryCursor_ > kHistoryCommands || documentHistoryBytes_ > kHistoryBytes / 2 )
    {
        documentHistoryBytes_ -= QCborValue(documentHistory_.front()).toCbor().size();
        documentHistory_.erase(documentHistory_.begin());
        --documentHistoryCursor_;
        documentHistoryCleanCursor_ = std::max(-1, documentHistoryCleanCursor_ - 1);
    }
}

void ResourceAuthoringDialog::recordDocumentHistory()
{
    if ( kind_ == ResourceAuthoringKind::sprite && !applyingDocumentHistory_ )
    {
        const QCborMap next = spriteHistoryValue();
        if ( next != spriteHistoryCurrent_ )
        {
            if ( !spriteHistoryPendingKey_.isEmpty() && documentHistoryCursor_ > 0 &&
                 documentHistoryCursor_ == static_cast<int>(documentHistory_.size()) &&
                 documentHistory_.back().value(QStringLiteral("pendingKey")).toString() == spriteHistoryPendingKey_ &&
                 documentHistory_.back().value(QStringLiteral("after")).toMap()
                     .value(QStringLiteral("pending")).toMap().contains(spriteHistoryPendingKey_) )
            {
                QCborMap& command = documentHistory_.back();
                documentHistoryBytes_ -= QCborValue(command).toCbor().size();
                if ( command.value(QStringLiteral("before")).toMap() == next )
                {
                    documentHistory_.pop_back();
                    --documentHistoryCursor_;
                }
                else
                {
                    command.insert(QStringLiteral("after"), next);
                    documentHistoryBytes_ += QCborValue(command).toCbor().size();
                }
            }
            else
            {
                QCborMap command{{QStringLiteral("before"), spriteHistoryCurrent_}, {QStringLiteral("after"), next}};
                if ( !spriteHistoryPendingKey_.isEmpty() )
                {
                    command.insert(QStringLiteral("pendingKey"), spriteHistoryPendingKey_);
                }
                appendDocumentHistoryCommand(command);
            }
            spriteHistoryCurrent_ = next;
        }
        if ( !spriteHistoryPendingKey_.isEmpty() && !spritePendingNumericEdits_.contains(spriteHistoryPendingKey_) )
        {
            // The final accepted value belongs to this gesture. Its next
            // text edit must start a new command, including after Save.
            if ( !documentHistory_.empty() &&
                 documentHistory_.back().value(QStringLiteral("pendingKey")).toString() == spriteHistoryPendingKey_ )
            {
                QCborMap& command = documentHistory_.back();
                documentHistoryBytes_ -= QCborValue(command).toCbor().size();
                command.remove(QStringLiteral("pendingKey"));
                documentHistoryBytes_ += QCborValue(command).toCbor().size();
            }
            spriteHistoryPendingKey_.clear();
        }
    }
}

void ResourceAuthoringDialog::recordStringHistoryRow(const int row)
{
    if ( refreshing_ || applyingDocumentHistory_ || row < 0 || row >= static_cast<int>(stringHistoryCurrent_.size()) )
    {
        return;
    }
    const QCborMap next = stringHistoryRow(row);
    QCborMap& before = stringHistoryCurrent_[static_cast<std::size_t>(row)];
    if ( before != next )
    {
        appendDocumentHistoryCommand({{QStringLiteral("row"), row}, {QStringLiteral("before"), before},
                                      {QStringLiteral("after"), next}});
        before = next;
    }
}

void ResourceAuthoringDialog::recordStringHistoryInsert(const int row)
{
    const QCborMap next = stringHistoryRow(row);
    appendDocumentHistoryCommand({{QStringLiteral("row"), row}, {QStringLiteral("before"), nullptr},
                                  {QStringLiteral("after"), next}});
    stringHistoryCurrent_.insert(stringHistoryCurrent_.begin() + row, next);
}

void ResourceAuthoringDialog::recordStringHistoryRemove(const int row)
{
    appendDocumentHistoryCommand({{QStringLiteral("row"), row},
                                  {QStringLiteral("before"), stringHistoryCurrent_.at(static_cast<std::size_t>(row))},
                                  {QStringLiteral("after"), nullptr}});
    stringHistoryCurrent_.erase(stringHistoryCurrent_.begin() + row);
}

QCborMap ResourceAuthoringDialog::captureDocumentHistory(const QCborMap& state) const
{
    const bool sound = kind_ == ResourceAuthoringKind::soundClass;
    QCborArray commands;
    if ( sound )
    {
        for ( const SoundHistoryEdit& edit : soundHistory_ )
        {
            QCborMap command{{QStringLiteral("row"), edit.row},
                             {QStringLiteral("before"), edit.before ? QCborValue(soundHistoryRow(*edit.before)) : QCborValue(nullptr)},
                             {QStringLiteral("after"), edit.after ? QCborValue(soundHistoryRow(*edit.after)) : QCborValue(nullptr)}};
            if ( !edit.presentationMode.isEmpty() )
            {
                command.insert(QStringLiteral("mode"), edit.presentationMode);
            }
            commands.append(command);
        }
    }
    else
    {
        for ( const QCborMap& command : documentHistory_ )
        {
            commands.append(command);
        }
    }
    int cursor = sound ? soundHistoryCursor_ : documentHistoryCursor_;
    int clean = sound ? soundHistoryCleanCursor_ : documentHistoryCleanCursor_;
    int cleanAuthored = soundHistoryCleanAuthoredCursor_;
    int authoredCursor = soundHistoryAuthoredCursor_;
    QCborArray models;
    const QCborArray rows = state.value(QStringLiteral("rows")).toArray();
    // Capture includes the active raw editor without committing its gesture.
    // A virtual command extends the captured journal, leaving live Undo untouched.
    if ( kind_ == ResourceAuthoringKind::sprite )
    {
        const QCborMap next = spriteHistoryValue();
        if ( next != spriteHistoryCurrent_ )
        {
            if ( clean > cursor )
            {
                clean = -1;
            }
            while ( commands.size() > cursor )
            {
                commands.removeLast();
            }
            commands.append(QCborMap{{QStringLiteral("before"), spriteHistoryCurrent_}, {QStringLiteral("after"), next}});
            ++cursor;
        }
    }
    else
    {
        for ( int row = 0; row < rows.size(); ++row )
        {
            QCborMap record = sound ? soundHistoryRow(soundRowRecord(row)) : stringHistoryRow(row);
            const QCborMap committed = sound ? soundHistoryRow(soundHistoryCurrent_.at(static_cast<std::size_t>(row)))
                                            : stringHistoryCurrent_.at(static_cast<std::size_t>(row));
            record.insert(QStringLiteral("raw"), rows.at(row));
            if ( !equivalentRow(record, committed) )
            {
                if ( clean > cursor )
                {
                    clean = -1;
                    if ( sound && cleanAuthored > authoredCursor )
                    {
                        cleanAuthored = -1;
                    }
                }
                while ( commands.size() > cursor )
                {
                    commands.removeLast();
                }
                commands.append(QCborMap{{QStringLiteral("row"), row}, {QStringLiteral("before"), committed},
                                        {QStringLiteral("after"), record}});
                ++cursor;
                ++authoredCursor;
            }
            models.append(record.value(QStringLiteral("model")));
        }
    }
    QCborMap history{{QStringLiteral("version"), 1}, {QStringLiteral("cursor"), cursor},
                     {QStringLiteral("clean"), clean},
                     {QStringLiteral("baselineDirty"), sound ? soundHistoryBaselineDirty_ : documentHistoryBaselineDirty_},
                     {QStringLiteral("commands"), commands}};
    if ( kind_ == ResourceAuthoringKind::sprite )
    {
        history.insert(QStringLiteral("material"), ui::episode::displayTextForGameBytes(sprite_.material));
    }
    else
    {
        history.insert(QStringLiteral("models"), models);
    }
    if ( sound )
    {
        history.insert(QStringLiteral("cleanAuthored"), cleanAuthored);
        QCborArray modes;
        QStringList sorted = soundPresentationModes_.values();
        sorted.sort(Qt::CaseSensitive);
        for ( const QString& mode : sorted )
        {
            modes.append(mode);
        }
        history.insert(QStringLiteral("modes"), modes);
    }
    while ( commands.size() > kHistoryCommands || QCborValue(commands).toCbor().size() > kHistoryBytes )
    {
        if ( commands.isEmpty() )
        {
            break;
        }
        // Prefer dropping the oldest undoable command. If all commands are
        // in Redo, drop its farthest future command instead.
        if ( cursor > 0 )
        {
            const bool authored = !commands.at(0).toMap().contains(QStringLiteral("mode"));
            commands.removeFirst();
            --cursor;
            clean = std::max(-1, clean - 1);
            if ( sound && authored )
            {
                history.insert(QStringLiteral("cleanAuthored"), std::max<qint64>(-1,
                    history.value(QStringLiteral("cleanAuthored")).toInteger() - 1));
            }
        }
        else
        {
            commands.removeLast();
            if ( clean > commands.size() )
            {
                clean = -1;
            }
        }
        history.insert(QStringLiteral("commands"), commands);
        history.insert(QStringLiteral("cursor"), cursor);
        history.insert(QStringLiteral("clean"), clean);
    }
    return history;
}

bool ResourceAuthoringDialog::validateDocumentHistory(const QCborMap& state)
{
    const QCborValue value = state.value(QStringLiteral("history"));
    if ( value.isUndefined() )
    {
        return true;
    }
    if ( !value.isMap() || value.toCbor().size() > 128 * 1024 * 1024 )
    {
        return false;
    }
    const QCborMap history = value.toMap();
    const QCborValue commandValue = history.value(QStringLiteral("commands"));
    const auto cursorValue = history.value(QStringLiteral("cursor"));
    const auto cleanValue = history.value(QStringLiteral("clean"));
    if ( !history.value(QStringLiteral("version")).isInteger() ||
         history.value(QStringLiteral("version")).toInteger() != 1 ||
         !cursorValue.isInteger() || !cleanValue.isInteger() ||
         !history.value(QStringLiteral("baselineDirty")).isBool() || !commandValue.isArray() )
    {
        return false;
    }
    const QCborArray commands = commandValue.toArray();
    if ( commands.size() > kHistoryCommands || commandValue.toCbor().size() > kHistoryBytes ||
         cursorValue.toInteger() < 0 || cursorValue.toInteger() > commands.size() ||
         cleanValue.toInteger() < -1 || cleanValue.toInteger() > commands.size() )
    {
        return false;
    }
    const int cursor = static_cast<int>(cursorValue.toInteger());
    const bool sprite = state.value(QStringLiteral("kind")).toInteger() == static_cast<int>(ResourceAuthoringKind::sprite);
    const bool sound = state.value(QStringLiteral("kind")).toInteger() == static_cast<int>(ResourceAuthoringKind::soundClass);
    QCborMap currentSprite;
    std::vector<QCborMap> rows;
    QSet<QString> modes;
    if ( sprite )
    {
        currentSprite = {{QStringLiteral("sprite"), state.value(QStringLiteral("sprite"))},
                         {QStringLiteral("pending"), state.value(QStringLiteral("spritePendingNumericEdits")).toMap()},
                         {QStringLiteral("material"), history.value(QStringLiteral("material"))}};
    }
    else
    {
        const QCborArray raw = state.value(QStringLiteral("rows")).toArray();
        const QCborValue models = history.value(QStringLiteral("models"));
        if ( !models.isArray() || models.toArray().size() != raw.size() )
        {
            return false;
        }
        for ( int row = 0; row < raw.size(); ++row )
        {
            QCborMap record{{QStringLiteral("raw"), raw.at(row)}, {QStringLiteral("model"), models.toArray().at(row)}};
            if ( sound )
            {
                const QCborArray loops = state.value(QStringLiteral("soundPreviewLoops")).toArray();
                record.insert(QStringLiteral("loop"), row < loops.size() && loops.at(row).toBool());
            }
            if ( !rowRecordValid(record, sound) )
            {
                return false;
            }
            rows.push_back(record);
        }
    }
    if ( sound )
    {
        if ( !history.value(QStringLiteral("modes")).isArray() || history.value(QStringLiteral("modes")).toArray().size() > 4096 ||
             !history.value(QStringLiteral("cleanAuthored")).isInteger() ||
             history.value(QStringLiteral("cleanAuthored")).toInteger() < -1 ||
             history.value(QStringLiteral("cleanAuthored")).toInteger() > commands.size() )
        {
            return false;
        }
        for ( const QCborValue& mode : history.value(QStringLiteral("modes")).toArray() )
        {
            const ProjectType project = static_cast<assets::SoundClassProfile>(state.value(QStringLiteral("profile")).toInteger()) == assets::SoundClassProfile::ijim
                ? ProjectType::IJIM : ProjectType::JKDF2;
            if ( !mode.isString() || !soundModeSuggestions(project).contains(mode.toString()) || modes.contains(mode.toString()) )
            {
                return false;
            }
            modes.insert(mode.toString());
        }
    }
    const auto validSprite = [&state](const QCborValue& record)
    {
        if ( !record.isMap() || !record.toMap().value(QStringLiteral("material")).isString() ||
             record.toMap().value(QStringLiteral("material")).toString().size() > 4096 ||
             !ui::episode::gameBytesForDisplayText(record.toMap().value(QStringLiteral("material")).toString()) )
        {
            return false;
        }
        QCborMap candidate = state;
        candidate.remove(QStringLiteral("history"));
        // The pane caret belongs to the current Sprite, not this historical record.
        candidate.remove(QStringLiteral("editorPresentation"));
        candidate.insert(QStringLiteral("sprite"), record.toMap().value(QStringLiteral("sprite")));
        candidate.insert(QStringLiteral("spritePendingNumericEdits"), record.toMap().value(QStringLiteral("pending")));
        return validateState(candidate, nullptr);
    };
    if ( sprite && !validSprite(currentSprite) )
    {
        return false;
    }
    const auto originalRows = rows;
    const auto originalSprite = currentSprite;
    const auto originalModes = modes;
    if ( sound )
    {
        QSet<QString> projectedEmpty = modes;
        for ( const QCborMap& row : rows )
        {
            projectedEmpty.remove(row.value(QStringLiteral("raw")).toArray().at(0).toString());
        }
        QSet<QString> storedEmpty;
        for ( const QCborValue& mode : state.value(QStringLiteral("soundEmptyModes")).toArray() )
        {
            storedEmpty.insert(mode.toString());
        }
        if ( projectedEmpty != storedEmpty )
        {
            return false;
        }
    }
    const auto apply = [&](const QCborValue& commandValue, const bool redo)
    {
        if ( !commandValue.isMap() )
        {
            return false;
        }
        const QCborMap command = commandValue.toMap();
        if ( sound && command.contains(QStringLiteral("mode")) )
        {
            const QCborValue mode = command.value(QStringLiteral("mode"));
            if ( !mode.isString() || mode.toString().isEmpty() || mode.toString().size() > 4096 ||
                 !command.value(QStringLiteral("row")).isInteger() || command.value(QStringLiteral("row")).toInteger() != -1 ||
                 !command.value(QStringLiteral("before")).isNull() || !command.value(QStringLiteral("after")).isNull() ||
                 modes.contains(mode.toString()) != !redo ||
                 !soundModeSuggestions(static_cast<assets::SoundClassProfile>(state.value(QStringLiteral("profile")).toInteger()) ==
                    assets::SoundClassProfile::ijim ? ProjectType::IJIM : ProjectType::JKDF2).contains(mode.toString()) )
            {
                return false;
            }
            if ( redo )
            {
                modes.insert(mode.toString());
            }
            else
            {
                modes.remove(mode.toString());
            }
            return true;
        }
        const QCborValue old = command.value(redo ? QStringLiteral("before") : QStringLiteral("after"));
        const QCborValue next = command.value(redo ? QStringLiteral("after") : QStringLiteral("before"));
        if ( sprite )
        {
            if ( !validSprite(old) || !validSprite(next) || currentSprite != old.toMap() )
            {
                return false;
            }
            currentSprite = next.toMap();
            return true;
        }
        const QCborValue rowValue = command.value(QStringLiteral("row"));
        if ( !rowValue.isInteger() || rowValue.toInteger() < 0 || rowValue.toInteger() > static_cast<qint64>(rows.size()) ||
             (!old.isNull() && !rowRecordValid(old, sound)) || (!next.isNull() && !rowRecordValid(next, sound)) ||
             (old.isNull() && next.isNull()) )
        {
            return false;
        }
        const auto row = static_cast<std::size_t>(rowValue.toInteger());
        if ( !old.isNull() && (row >= rows.size() || !equivalentRow(rows[row], old.toMap())) )
        {
            return false;
        }
        if ( old.isNull() )
        {
            rows.insert(rows.begin() + static_cast<std::ptrdiff_t>(row), next.toMap());
        }
        else if ( next.isNull() )
        {
            rows.erase(rows.begin() + static_cast<std::ptrdiff_t>(row));
        }
        else
        {
            rows[row] = next.toMap();
        }
        return rows.size() <= 4096;
    };
    for ( int index = cursor; index > 0; --index )
    {
        if ( !apply(commands.at(index - 1), false) )
        {
            return false;
        }
    }
    if ( cursor == 0 && (rows != originalRows || currentSprite != originalSprite || modes != originalModes) )
    {
        return false;
    }
    for ( int index = 0; index < commands.size(); ++index )
    {
        if ( !apply(commands.at(index), true) )
        {
            return false;
        }
        if ( index + 1 == cursor )
        {
            if ( currentSprite != originalSprite || modes != originalModes || rows.size() != originalRows.size() )
            {
                return false;
            }
            for ( std::size_t row = 0; row < rows.size(); ++row )
            {
                if ( !equivalentRow(rows[row], originalRows[row]) )
                {
                    return false;
                }
            }
        }
    }
    int authoredCursor = cursor;
    int authoredClean = static_cast<int>(cleanValue.toInteger());
    if ( sound )
    {
        int authored = 0;
        authoredCursor = 0;
        for ( int index = 0; index < commands.size(); ++index )
        {
            if ( !commands.at(index).toMap().contains(QStringLiteral("mode")) )
            {
                ++authored;
            }
            if ( index + 1 == cursor )
            {
                authoredCursor = authored;
            }
            if ( index + 1 == cleanValue.toInteger() )
            {
                authoredClean = authored;
            }
        }
        const qint64 saved = history.value(QStringLiteral("cleanAuthored")).toInteger();
        if ( saved > authored || (cleanValue.toInteger() >= 0 && saved != authoredClean) )
        {
            return false;
        }
        authoredClean = static_cast<int>(saved);
    }
    const bool modified = history.value(QStringLiteral("baselineDirty")).toBool() || authoredCursor != authoredClean;
    return modified == state.value(QStringLiteral("modified")).toBool();
}

void ResourceAuthoringDialog::restoreDocumentHistory(const QCborMap& state)
{
    resetDocumentHistory();
    const QCborValue value = state.value(QStringLiteral("history"));
    if ( !value.isMap() )
    {
        return;
    }
    const QCborMap history = value.toMap();
    const auto decodeSound = [](const QCborMap& record)
    {
        SoundRowRecord row;
        const QCborArray model = record.value(QStringLiteral("model")).toArray();
        const QCborArray raw = record.value(QStringLiteral("raw")).toArray();
        row.entry.mode = *ui::episode::gameBytesForDisplayText(model.at(0).toString());
        row.entry.wav = *ui::episode::gameBytesForDisplayText(model.at(1).toString());
        row.entry.playFlags = *parseHexFlags(model.at(2).toString());
        row.entry.minRadius = *parseFiniteFloat(model.at(3).toString());
        row.entry.maxRadius = *parseFiniteFloat(model.at(4).toString());
        row.entry.maxVolume = *parseFiniteFloat(model.at(5).toString());
        for ( int index = 0; index < 5; ++index )
        {
            row.raw[static_cast<std::size_t>(index)] = raw.at(index + 1).toString();
        }
        row.previewLoop = record.value(QStringLiteral("loop")).toBool();
        return row;
    };
    if ( kind_ == ResourceAuthoringKind::soundClass )
    {
        soundHistory_.clear();
        soundHistoryBytes_ = 0;
        soundHistoryCursor_ = static_cast<int>(history.value(QStringLiteral("cursor")).toInteger());
        soundHistoryCleanCursor_ = static_cast<int>(history.value(QStringLiteral("clean")).toInteger());
        soundHistoryCleanAuthoredCursor_ = static_cast<int>(history.value(QStringLiteral("cleanAuthored")).toInteger());
        soundHistoryBaselineDirty_ = history.value(QStringLiteral("baselineDirty")).toBool();
        soundHistoryAuthoredCursor_ = 0;
        for ( const QCborValue& value : history.value(QStringLiteral("commands")).toArray() )
        {
            const QCborMap command = value.toMap();
            SoundHistoryEdit edit;
            edit.row = static_cast<int>(command.value(QStringLiteral("row")).toInteger());
            edit.presentationMode = command.value(QStringLiteral("mode")).toString();
            if ( command.value(QStringLiteral("before")).isMap() )
            {
                edit.before = decodeSound(command.value(QStringLiteral("before")).toMap());
            }
            if ( command.value(QStringLiteral("after")).isMap() )
            {
                edit.after = decodeSound(command.value(QStringLiteral("after")).toMap());
            }
            if ( static_cast<int>(soundHistory_.size()) < soundHistoryCursor_ && edit.presentationMode.isEmpty() )
            {
                ++soundHistoryAuthoredCursor_;
            }
            soundHistory_.push_back(std::move(edit));
            soundHistoryBytes_ += QCborValue(command).toCbor().size();
        }
        soundPresentationModes_.clear();
        for ( const QCborValue& mode : history.value(QStringLiteral("modes")).toArray() )
        {
            soundPresentationModes_.insert(mode.toString());
        }
        for ( int row = 0; row < static_cast<int>(soundFile_.entries.size()); ++row )
        {
            QCborMap record = soundHistoryRow(soundRowRecord(row));
            record.insert(QStringLiteral("model"), history.value(QStringLiteral("models")).toArray().at(row));
            soundFile_.entries[static_cast<std::size_t>(row)] = decodeSound(record).entry;
        }
        soundHistoryCurrent_.clear();
        for ( int row = 0; row < static_cast<int>(soundFile_.entries.size()); ++row )
        {
            soundHistoryCurrent_.push_back(soundRowRecord(row));
        }
        refreshSoundModeGroups();
        updateSoundHistoryActions();
        return;
    }
    documentHistoryCursor_ = static_cast<int>(history.value(QStringLiteral("cursor")).toInteger());
    documentHistoryCleanCursor_ = static_cast<int>(history.value(QStringLiteral("clean")).toInteger());
    documentHistoryBaselineDirty_ = history.value(QStringLiteral("baselineDirty")).toBool();
    for ( const QCborValue& command : history.value(QStringLiteral("commands")).toArray() )
    {
        documentHistory_.push_back(command.toMap());
        documentHistoryBytes_ += QCborValue(command).toCbor().size();
    }
    if ( kind_ == ResourceAuthoringKind::sprite )
    {
        sprite_.material = *ui::episode::gameBytesForDisplayText(history.value(QStringLiteral("material")).toString());
        spriteHistoryCurrent_ = spriteHistoryValue();
    }
    else
    {
        for ( int row = 0; row < static_cast<int>(stringHistoryCurrent_.size()); ++row )
        {
            QCborMap& record = stringHistoryCurrent_[static_cast<std::size_t>(row)];
            record.insert(QStringLiteral("model"), history.value(QStringLiteral("models")).toArray().at(row));
            const QCborArray model = record.value(QStringLiteral("model")).toArray();
            auto& entry = stringsFile_.entries[static_cast<std::size_t>(row)];
            entry.key = *ui::episode::gameBytesForDisplayText(model.at(0).toString());
            entry.value = *ui::episode::gameBytesForDisplayText(model.at(1).toString());
            entry.numericValue = static_cast<std::int32_t>(model.at(2).toString().toLongLong());
        }
    }
}

void ResourceAuthoringDialog::applyDocumentHistoryCommand(const QCborMap& command, const bool redo)
{
    const QCborValue next = command.value(redo ? QStringLiteral("after") : QStringLiteral("before"));
    const QCborValue old = command.value(redo ? QStringLiteral("before") : QStringLiteral("after"));
    if ( kind_ == ResourceAuthoringKind::sprite )
    {
        const QCborMap record = next.toMap();
        const QCborArray values = record.value(QStringLiteral("sprite")).toArray();
        const bool previous = refreshing_;
        refreshing_ = true;
        spriteMaterial_->setText(values.at(0).toString());
        sprite_.material = *ui::episode::gameBytesForDisplayText(record.value(QStringLiteral("material")).toString());
        sprite_.type = static_cast<int>(values.at(1).toInteger());
        sprite_.width = *parseFiniteFloat(values.at(2).toString());
        sprite_.height = *parseFiniteFloat(values.at(3).toString());
        sprite_.geometryMode = static_cast<int>(values.at(4).toInteger());
        sprite_.lightMode = static_cast<int>(values.at(5).toInteger());
        sprite_.textureMode = static_cast<int>(values.at(6).toInteger());
        for ( std::size_t index = 0; index < 4; ++index )
        {
            sprite_.extraLightRGBA[index] = *parseFiniteFloat(values.at(static_cast<qsizetype>(8 + index)).toString());
        }
        for ( std::size_t index = 0; index < 3; ++index )
        {
            sprite_.offsetXYZ[index] = *parseFiniteFloat(values.at(static_cast<qsizetype>(12 + index)).toString());
        }
        spritePendingNumericEdits_ = record.value(QStringLiteral("pending")).toMap();
        spriteHistoryPendingKey_.clear();
        spriteNumericEditBaselines_.clear();
        spriteNumericUndoKeys_.clear();
        refreshSpriteWidgets();
        restorePendingSpriteNumericEditors();
        refreshing_ = previous;
        spriteHistoryCurrent_ = record;
        scheduleSpritePreview();
        return;
    }
    const int row = static_cast<int>(command.value(QStringLiteral("row")).toInteger());
    const QSignalBlocker blocker(stringTable_);
    if ( old.isNull() )
    {
        stringsFile_.entries.insert(stringsFile_.entries.begin() + row, assets::StringTableEntry{});
        stringHistoryCurrent_.insert(stringHistoryCurrent_.begin() + row, next.toMap());
        stringTable_->insertRow(row);
    }
    else if ( next.isNull() )
    {
        stringsFile_.entries.erase(stringsFile_.entries.begin() + row);
        stringHistoryCurrent_.erase(stringHistoryCurrent_.begin() + row);
        stringTable_->removeRow(row);
        filterStringRows();
        return;
    }
    else
    {
        stringHistoryCurrent_[static_cast<std::size_t>(row)] = next.toMap();
    }
    const QCborArray model = next.toMap().value(QStringLiteral("model")).toArray();
    const QCborArray raw = next.toMap().value(QStringLiteral("raw")).toArray();
    auto& entry = stringsFile_.entries[static_cast<std::size_t>(row)];
    entry.key = *ui::episode::gameBytesForDisplayText(model.at(0).toString());
    entry.value = *ui::episode::gameBytesForDisplayText(model.at(1).toString());
    entry.numericValue = static_cast<std::int32_t>(model.at(2).toString().toLongLong());
    for ( int column = 0; column < 3; ++column )
    {
        if ( !stringTable_->item(row, column) )
        {
            stringTable_->setItem(row, column, new QTableWidgetItem());
        }
        stringTable_->item(row, column)->setText(raw.at(column).toString());
    }
    filterStringRows();
}

bool ResourceAuthoringDialog::hasPendingDocumentEdit() const
{
    for ( const LiveTableEditor& editor : liveTableEditors_ )
    {
        if ( !editor.view || !editor.editor || !editor.index.isValid() ||
             editor.view->indexWidget(editor.index) != editor.editor.data() )
        {
            continue;
        }
        const QString current = cellTextWithPendingEditor(editor.view, editor.index, editor.index.data().toString());
        if ( current != editor.index.data().toString() )
        {
            return true;
        }
    }
    return kind_ == ResourceAuthoringKind::sprite && spriteHistoryValue() != spriteHistoryCurrent_;
}

bool ResourceAuthoringDialog::canUndoRedoDocument(const bool redo) const
{
    if ( !redo && hasPendingDocumentEdit() )
    {
        return true;
    }
    return kind_ == ResourceAuthoringKind::soundClass
        ? (redo ? soundHistoryCursor_ < static_cast<int>(soundHistory_.size()) : soundHistoryCursor_ > 0)
        : (redo ? documentHistoryCursor_ < static_cast<int>(documentHistory_.size()) : documentHistoryCursor_ > 0);
}

bool ResourceAuthoringDialog::undoRedoDocument(const bool redo, const bool respectTextFocus)
{
    QWidget* const focused = QApplication::focusWidget();
    if ( respectTextFocus && focused && isAncestorOf(focused) && qobject_cast<QLineEdit*>(focused) )
    {
        return false;
    }
    const QPointer<ResourceAuthoringDialog> self(this);
    if ( !respectTextFocus )
    {
        settleActiveTableEditors();
        if ( !self )
        {
            return false;
        }
    }
    recordDocumentHistory();
    if ( kind_ == ResourceAuthoringKind::soundClass )
    {
        if ( !canUndoRedoDocument(redo) )
        {
            return true;
        }
        const int index = redo ? soundHistoryCursor_ : soundHistoryCursor_ - 1;
        const SoundHistoryEdit edit = soundHistory_.at(static_cast<std::size_t>(index));
        applySoundHistoryEdit(edit, redo);
        cancelDeferredEditorPresentation();
        if ( edit.presentationMode.isEmpty() )
        {
            const auto& old = redo ? edit.before : edit.after;
            const auto& next = redo ? edit.after : edit.before;
            if ( old && next )
            {
                soundHistoryCurrent_[static_cast<std::size_t>(edit.row)] = soundRowRecord(edit.row);
            }
            else if ( next )
            {
                soundHistoryCurrent_.insert(soundHistoryCurrent_.begin() + edit.row, soundRowRecord(edit.row));
            }
            else
            {
                soundHistoryCurrent_.erase(soundHistoryCurrent_.begin() + edit.row);
            }
        }
        soundHistoryCursor_ += redo ? 1 : -1;
        if ( edit.presentationMode.isEmpty() )
        {
            soundHistoryAuthoredCursor_ += redo ? 1 : -1;
        }
        publishSoundHistoryChange();
        return true;
    }
    if ( !canUndoRedoDocument(redo) )
    {
        return true;
    }
    const int index = redo ? documentHistoryCursor_ : documentHistoryCursor_ - 1;
    applyingDocumentHistory_ = true;
    applyDocumentHistoryCommand(documentHistory_.at(static_cast<std::size_t>(index)), redo);
    applyingDocumentHistory_ = false;
    cancelDeferredEditorPresentation();
    documentHistoryCursor_ += redo ? 1 : -1;
    dirty_ = documentHistoryBaselineDirty_ || documentHistoryCursor_ != documentHistoryCleanCursor_;
    ++authoredRevision_;
    setStatus(dirty_ ? QStringLiteral("Unsaved changes.") : QStringLiteral("Changes reverted."));
    updateDocumentLabel();
    notifyChanged();
    return true;
}
}
