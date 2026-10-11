#pragma once

#include <algorithm>
#include <cstdint>
#include <limits>
#include <optional>
#include <string_view>
#include <unordered_set>

#include <QByteArray>
#include <QCborArray>
#include <QCborMap>
#include <QCborParserError>
#include <QCborValue>
#include <QString>

#include "sed/editor/selection.hpp"
#include "sed/level.hpp"
#include "sed/level/transform.hpp"
#include "sed/text.hpp"
#include "sed/values.hpp"
#include "ui/recovery/document_recovery_utils.hpp"

namespace sed::detail
{

// Save-time Level normalization may insert `numframes` before existing frame
// values. Keep EDIT's selected/active frame references paired with the exact
// candidate Level image without changing the live document or any other EDIT
// navigation state. Call only after strict live-EDIT validation.
inline std::optional<QByteArray> remapRecoveryEditorFrameRefsForCandidateLevel(
    const QByteArray& validatedPayload,
    const Level& authoredLevel,
    const Level& candidateLevel,
    QString* error = nullptr)
{
    const auto fail = [error](const QString& message)
        -> std::optional<QByteArray>
    {
        if ( error )
        {
            *error = message;
        }
        return std::nullopt;
    };
    if ( error )
    {
        error->clear();
    }

    QCborParserError parseError{};
    const QCborValue decoded = QCborValue::fromCbor(
        validatedPayload, &parseError);
    if ( parseError.error != QCborError::NoError || !decoded.isMap() )
    {
        return fail(QStringLiteral(
            "the captured EDIT payload is not a CBOR map"));
    }
    QCborMap root = decoded.toMap();
    const QCborValue selectionValue = root.value(QStringLiteral("selection"));
    QString selectionError{};
    std::optional<editor::Selection> selection = selectionValue.isByteArray()
        ? ui::recovery::deserializeSelection(
            selectionValue.toByteArray(), &selectionError)
        : std::nullopt;
    const QCborValue activeValue = root.value(QStringLiteral("active"));
    if ( !selection || !activeValue.isArray() )
    {
        return fail(selectionError.isEmpty()
            ? QStringLiteral("the captured EDIT selection or active state is malformed")
            : selectionError);
    }
    QCborArray active = activeValue.toArray();
    if ( active.size() != 7 || !active.at(5).isInteger()
        || !active.at(6).isInteger() )
    {
        return fail(QStringLiteral(
            "the captured EDIT active frame reference is malformed"));
    }

    const auto isFrame = [](const TplValue& value)
    {
        return value.type == ValueType::frame
            && text::iequalsAscii(value.name, "frame");
    };
    const auto remap = [&](const level::FrameRef ref)
        -> std::optional<level::FrameRef>
    {
        if ( ref.thing < 0
            || static_cast<std::size_t>(ref.thing) >= authoredLevel.things.size()
            || static_cast<std::size_t>(ref.thing) >= candidateLevel.things.size() )
        {
            return std::nullopt;
        }
        const Thing& authoredThing = authoredLevel.things[
            static_cast<std::size_t>(ref.thing)];
        if ( ref.value < 0
            || static_cast<std::size_t>(ref.value) >= authoredThing.values.size() )
        {
            return std::nullopt;
        }
        const auto authoredFrame = authoredThing.values.begin() + ref.value;
        if ( !isFrame(*authoredFrame) )
        {
            return std::nullopt;
        }
        const std::size_t ordinal = static_cast<std::size_t>(std::count_if(
            authoredThing.values.begin(), authoredFrame, isFrame));

        const Thing& candidateThing = candidateLevel.things[
            static_cast<std::size_t>(ref.thing)];
        std::size_t candidateOrdinal = 0;
        for ( std::size_t valueIndex = 0;
            valueIndex < candidateThing.values.size(); ++valueIndex )
        {
            if ( !isFrame(candidateThing.values[valueIndex]) )
            {
                continue;
            }
            if ( candidateOrdinal == ordinal )
            {
                if ( valueIndex > static_cast<std::size_t>(
                        std::numeric_limits<std::int32_t>::max()) )
                {
                    return std::nullopt;
                }
                return level::FrameRef{
                    ref.thing, static_cast<std::int32_t>(valueIndex) };
            }
            ++candidateOrdinal;
        }
        return std::nullopt;
    };

    std::unordered_set<level::FrameRef, editor::FrameRefHash, editor::FrameRefEq>
        candidateFrames{};
    candidateFrames.reserve(selection->frames.size());
    for ( const level::FrameRef authoredRef : selection->frames )
    {
        const std::optional<level::FrameRef> candidateRef = remap(authoredRef);
        if ( !candidateRef )
        {
            return fail(QStringLiteral(
                "a selected EDIT frame does not have a same-ordinal frame in the candidate Level"));
        }
        const Thing& thing = candidateLevel.things[
            static_cast<std::size_t>(candidateRef->thing)];
        if ( candidateRef->value < 0
            || static_cast<std::size_t>(candidateRef->value) >= thing.values.size()
            || !isFrame(thing.values[static_cast<std::size_t>(candidateRef->value)]) )
        {
            return fail(QStringLiteral(
                "a remapped EDIT frame reference does not name a candidate frame value"));
        }
        candidateFrames.insert(*candidateRef);
    }
    if ( candidateFrames.size() != selection->frames.size() )
    {
        return fail(QStringLiteral(
            "remapped EDIT frame references are not unique"));
    }
    selection->frames = std::move(candidateFrames);
    root.insert(QStringLiteral("selection"),
        ui::recovery::serializeSelection(*selection));

    const std::int64_t activeFrameThing = active.at(5).toInteger();
    const std::int64_t activeFrameValue = active.at(6).toInteger();
    if ( activeFrameThing == -1 && activeFrameValue == -1 )
    {
        // The canonical no-primary-frame sentinel remains unchanged.
    }
    else if ( activeFrameThing < 0 || activeFrameValue < 0
        || activeFrameThing > std::numeric_limits<std::int32_t>::max()
        || activeFrameValue > std::numeric_limits<std::int32_t>::max() )
    {
        return fail(QStringLiteral(
            "the captured EDIT active frame reference is out of range"));
    }
    else
    {
        const std::optional<level::FrameRef> candidateActive = remap(
            level::FrameRef{
                static_cast<std::int32_t>(activeFrameThing),
                static_cast<std::int32_t>(activeFrameValue) });
        if ( !candidateActive || !selection->frames.contains(*candidateActive) )
        {
            return fail(QStringLiteral(
                "the active EDIT frame is not selected in the candidate Level"));
        }
        active[5] = QCborValue(static_cast<qint64>(candidateActive->thing));
        active[6] = QCborValue(static_cast<qint64>(candidateActive->value));
        root.insert(QStringLiteral("active"), active);
    }

    return ui::recovery::serializeCanonicalCbor(QCborValue(root));
}

} // namespace sed::detail
