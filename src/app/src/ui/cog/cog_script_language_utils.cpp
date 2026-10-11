#include "ui/cog/cog_script_language_utils.hpp"

#include <algorithm>
#include <limits>

#include <QRegularExpression>
#include <QSet>

#include "ui/cog/cog_script_assignment_utils.hpp"
#include "ui/cog/cog_script_text_utils.hpp"

namespace sed::ui::cog
{
namespace
{

enum class IdentifierReferenceSection
{
    outside,
    symbols,
    code,
};

const QRegularExpression& identifierReferenceSectionRx()
{
    static const QRegularExpression kSectionRx(
        QStringLiteral("^\\s*(symbols|code)\\b"),
        QRegularExpression::CaseInsensitiveOption
            | QRegularExpression::UseUnicodePropertiesOption);
    return kSectionRx;
}

const QRegularExpression& identifierReferenceEndRx()
{
    static const QRegularExpression kEndRx(
        QStringLiteral("^\\s*end\\b"),
        QRegularExpression::CaseInsensitiveOption
            | QRegularExpression::UseUnicodePropertiesOption);
    return kEndRx;
}

const QStringList& jkdf2MessageWords()
{
    // OpenJKDF2 sithCog_Startup: common registrations before the
    // Main_bDwCompat/Main_bMotsCompat branch. Main_bDwCompat registrations
    // are intentionally not part of a supported SED project.
    static const QStringList kWords{
        QStringLiteral("activate"),
        QStringLiteral("activated"),
        QStringLiteral("startup"),
        QStringLiteral("timer"),
        QStringLiteral("blocked"),
        QStringLiteral("entered"),
        QStringLiteral("exited"),
        QStringLiteral("crossed"),
        QStringLiteral("sighted"),
        QStringLiteral("damaged"),
        QStringLiteral("arrived"),
        QStringLiteral("killed"),
        QStringLiteral("pulse"),
        QStringLiteral("touched"),
        QStringLiteral("created"),
        QStringLiteral("loading"),
        QStringLiteral("selected"),
        QStringLiteral("deselected"),
        QStringLiteral("changed"),
        QStringLiteral("deactivated"),
        QStringLiteral("shutdown"),
        QStringLiteral("respawn"),
        QStringLiteral("removed"),
        QStringLiteral("autoselect"),
        QStringLiteral("aievent"),
        QStringLiteral("skill"),
        QStringLiteral("taken"),
        QStringLiteral("user0"),
        QStringLiteral("user1"),
        QStringLiteral("user2"),
        QStringLiteral("user3"),
        QStringLiteral("user4"),
        QStringLiteral("user5"),
        QStringLiteral("user6"),
        QStringLiteral("user7"),
        QStringLiteral("newplayer"),
        QStringLiteral("fire"),
        QStringLiteral("join"),
        QStringLiteral("leave"),
        QStringLiteral("splash"),
        QStringLiteral("trigger"),
    };
    return kWords;
}

const QStringList& motsMessageWords()
{
    static const QStringList kWords = []()
    {
        QStringList words = jkdf2MessageWords();
        words.append({
            QStringLiteral("preblock"),
            QStringLiteral("escaped"),
            QStringLiteral("attachkilled"),
            QStringLiteral("playeraction"),
        });
        return words;
    }();
    return kWords;
}

const QStringList& ijimMessageWords()
{
    // This is the established IJIM editor vocabulary. Keep it independent
    // from the classic compatibility table so JKDF2/MotS corrections cannot
    // silently remove IJIM-only handlers.
    static const QStringList kWords{
        QStringLiteral("activate"),
        QStringLiteral("activated"),
        QStringLiteral("startup"),
        QStringLiteral("timer"),
        QStringLiteral("blocked"),
        QStringLiteral("entered"),
        QStringLiteral("exited"),
        QStringLiteral("crossed"),
        QStringLiteral("sighted"),
        QStringLiteral("damaged"),
        QStringLiteral("arrived"),
        QStringLiteral("killed"),
        QStringLiteral("pulse"),
        QStringLiteral("touched"),
        QStringLiteral("created"),
        QStringLiteral("loading"),
        QStringLiteral("selected"),
        QStringLiteral("deselected"),
        QStringLiteral("changed"),
        QStringLiteral("deactivated"),
        QStringLiteral("shutdown"),
        QStringLiteral("respawn"),
        QStringLiteral("removed"),
        QStringLiteral("autoselect"),
        QStringLiteral("aim"),
        QStringLiteral("aievent"),
        QStringLiteral("callback"),
        QStringLiteral("skill"),
        QStringLiteral("taken"),
        QStringLiteral("user0"),
        QStringLiteral("user1"),
        QStringLiteral("user2"),
        QStringLiteral("user3"),
        QStringLiteral("user4"),
        QStringLiteral("user5"),
        QStringLiteral("user6"),
        QStringLiteral("user7"),
        QStringLiteral("newplayer"),
        QStringLiteral("fire"),
        QStringLiteral("join"),
        QStringLiteral("leave"),
        QStringLiteral("splash"),
        QStringLiteral("trigger"),
        QStringLiteral("statechange"),
        QStringLiteral("missed"),
        QStringLiteral("boarded"),
        QStringLiteral("unboarded"),
        QStringLiteral("arrivedwpnt"),
        QStringLiteral("initialized"),
        QStringLiteral("updatewpnts"),
    };
    return kWords;
}

} // namespace

const QStringList& cogScriptSectionWords()
{
    static const QStringList kWords{
        QStringLiteral("flags"),
        QStringLiteral("symbols"),
        QStringLiteral("code"),
        QStringLiteral("end"),
    };
    return kWords;
}

const QStringList& cogScriptKeywordWords()
{
    static const QStringList kWords{
        QStringLiteral("call"),
        QStringLiteral("return"),
        QStringLiteral("if"),
        QStringLiteral("else"),
        QStringLiteral("while"),
        QStringLiteral("for"),
        QStringLiteral("do"),
        QStringLiteral("goto"),
        QStringLiteral("continue"),
        QStringLiteral("break"),
        QStringLiteral("switch"),
        QStringLiteral("stop"),
        QStringLiteral("sleep"),
        QStringLiteral("end"),
    };
    return kWords;
}

const QStringList& cogScriptTypeWords()
{
    static const QStringList kWords{
        QStringLiteral("int"),
        QStringLiteral("thing"),
        QStringLiteral("surface"),
        QStringLiteral("sector"),
        QStringLiteral("sound"),
        QStringLiteral("template"),
        QStringLiteral("model"),
        QStringLiteral("keyframe"),
        QStringLiteral("cog"),
        QStringLiteral("material"),
        QStringLiteral("message"),
        QStringLiteral("flex"),
        QStringLiteral("float"),
        QStringLiteral("vector"),
        QStringLiteral("ai"),
    };
    return kWords;
}

const QStringList& cogScriptInsertTypeWords()
{
    static const QStringList kWords{
        QStringLiteral("int"),
        QStringLiteral("thing"),
        QStringLiteral("surface"),
        QStringLiteral("sector"),
        QStringLiteral("sound"),
        QStringLiteral("template"),
        QStringLiteral("model"),
        QStringLiteral("keyframe"),
        QStringLiteral("cog"),
        QStringLiteral("material"),
        QStringLiteral("flex"),
        QStringLiteral("float"),
        QStringLiteral("vector"),
        QStringLiteral("ai"),
    };
    return kWords;
}

const QStringList& cogScriptAttributeWords()
{
    static const QStringList kWords{
        QStringLiteral("local"),
        QStringLiteral("desc"),
        QStringLiteral("mask"),
        QStringLiteral("linkid"),
        QStringLiteral("nolink"),
    };
    return kWords;
}

const QStringList& cogScriptMessageWords()
{
    static const QStringList kWords = []()
    {
        QStringList words = motsMessageWords();
        for (const QString& word : ijimMessageWords())
        {
            if (!words.contains(word))
            {
                words.push_back(word);
            }
        }
        return words;
    }();
    return kWords;
}

QStringList cogScriptProjectMessageWords(const ProjectType kind)
{
    switch (kind)
    {
        case ProjectType::JKDF2:
            return jkdf2MessageWords();
        case ProjectType::MOTS:
            return motsMessageWords();
        case ProjectType::IJIM:
            return ijimMessageWords();
    }

    return cogScriptMessageWords();
}

const QStringList& cogScriptGlobalWords()
{
    static const QStringList kWords = []()
    {
        QStringList out{};
        out.reserve(16);
        for (int i = 0; i < 16; ++i)
        {
            out.push_back(QStringLiteral("global%1").arg(i));
        }
        return out;
    }();
    return kWords;
}

bool cogScriptTypeSupportsCodeLabel(const QString& typeName)
{
    const QString lower = typeName.trimmed().toLower();
    return lower == QStringLiteral("int")
        || lower == QStringLiteral("flex")
        || lower == QStringLiteral("float");
}

QStringList buildCogScriptCompletionWords(const QString& text, const QStringList& messageWords)
{
    QSet<QString> words{};
    auto addWords = [&words](const QStringList& list)
    {
        for (const QString& word : list)
        {
            words.insert(word);
        }
    };

    // Step 1: seed completions from the fixed COG language vocabulary.
    addWords(cogScriptSectionWords());
    addWords(cogScriptKeywordWords());
    addWords(cogScriptTypeWords());
    addWords(cogScriptAttributeWords());
    addWords(messageWords);
    addWords(cogScriptGlobalWords());

    // Step 2: scan the script for user-defined identifiers, ignoring comments and string literals.
    static const QRegularExpression kIdentifierRx(
        QStringLiteral("\\b[A-Za-z_][A-Za-z0-9_]*\\b"),
        QRegularExpression::UseUnicodePropertiesOption);
    const QStringList lines = text.split(QLatin1Char('\n'));
    for (const QString& rawLine : lines)
    {
        const QString sanitizedLine = sanitizeLineForAnalysis(rawLine);
        QRegularExpressionMatchIterator it = kIdentifierRx.globalMatch(sanitizedLine);
        while (it.hasNext())
        {
            const QRegularExpressionMatch match = it.next();
            const QString ident = match.captured(0);
            if (!ident.isEmpty())
            {
                words.insert(ident);
            }
        }
    }

    // Step 3: return a stable, case-insensitive order for completer UI and tests.
    QStringList out = words.values();
    out.sort(Qt::CaseInsensitive);
    return out;
}

QStringList buildCogScriptCallableWords(const QString& text)
{
    static const QRegularExpression kSectionRx(
        QStringLiteral("^\\s*(symbols|code)\\b"),
        QRegularExpression::CaseInsensitiveOption | QRegularExpression::UseUnicodePropertiesOption);
    static const QRegularExpression kEndRx(
        QStringLiteral("^\\s*end\\b"),
        QRegularExpression::CaseInsensitiveOption | QRegularExpression::UseUnicodePropertiesOption);
    static const QRegularExpression kSymbolRx(
        QStringLiteral("^\\s*([A-Za-z0-9_]+)\\s+([A-Za-z_][A-Za-z0-9_]*)\\b"),
        QRegularExpression::UseUnicodePropertiesOption);
    static const QRegularExpression kLabelRx(
        QStringLiteral("^\\s*([A-Za-z_][A-Za-z0-9_]*)\\s*:"),
        QRegularExpression::UseUnicodePropertiesOption);

    enum class Section
    {
        none,
        symbols,
        code,
    };

    // Step 1: walk section state so only symbols/code bodies contribute call targets.
    Section currentSection = Section::none;
    QSet<QString> names{};
    const QStringList lines = text.split(QLatin1Char('\n'));
    for (const QString& rawLine : lines)
    {
        const QString line = stripComment(rawLine).trimmed();
        if (line.isEmpty())
        {
            continue;
        }

        if (const auto match = kSectionRx.match(line); match.hasMatch())
        {
            const QString sectionName = match.captured(1).toLower();
            currentSection = (sectionName == QStringLiteral("symbols")) ? Section::symbols : Section::code;
            continue;
        }

        if (kEndRx.match(line).hasMatch())
        {
            currentSection = Section::none;
            continue;
        }

        // Step 2: code labels are callable; flex/float symbols are callable function references.
        if (currentSection == Section::code)
        {
            if (const auto match = kLabelRx.match(line); match.hasMatch())
            {
                names.insert(match.captured(1));
            }
        }
        else if (currentSection == Section::symbols)
        {
            if (const auto match = kSymbolRx.match(line); match.hasMatch())
            {
                const QString typeName = match.captured(1).trimmed().toLower();
                if (typeName == QStringLiteral("float") || typeName == QStringLiteral("flex"))
                {
                    names.insert(match.captured(2));
                }
            }
        }
    }

    QStringList out = names.values();
    out.sort(Qt::CaseInsensitive);
    return out;
}

std::vector<CogScriptIdentifierOccurrence> findCogScriptIdentifierOccurrences(
    const QString& text,
    const QStringView identifier)
{
    const auto isIdentifierStart = [](const QChar ch)
    {
        return (ch >= QLatin1Char('A') && ch <= QLatin1Char('Z'))
            || (ch >= QLatin1Char('a') && ch <= QLatin1Char('z'))
            || ch == QLatin1Char('_');
    };
    const auto isIdentifierPart = [&isIdentifierStart](const QChar ch)
    {
        return isIdentifierStart(ch)
            || (ch >= QLatin1Char('0') && ch <= QLatin1Char('9'));
    };

    if ( identifier.isEmpty() || !isIdentifierStart(identifier.front()) )
    {
        return {};
    }
    for ( const QChar ch : identifier )
    {
        if ( !isIdentifierPart(ch) )
        {
            return {};
        }
    }

    std::vector<CogScriptIdentifierOccurrence> occurrences{};
    const QStringView source(text);
    IdentifierReferenceSection section = IdentifierReferenceSection::outside;
    int sourceOffset = 0;
    const QStringList lines = text.split(QLatin1Char('\n'));
    for ( const QString& rawLine : lines )
    {
        const QString uncommented = stripComment(rawLine).trimmed();
        if ( const QRegularExpressionMatch sectionMatch =
                 identifierReferenceSectionRx().match(uncommented);
            sectionMatch.hasMatch() )
        {
            section = sectionMatch.captured(1).compare(
                    QStringLiteral("symbols"), Qt::CaseInsensitive) == 0
                ? IdentifierReferenceSection::symbols
                : IdentifierReferenceSection::code;
        }
        else if ( identifierReferenceEndRx().match(uncommented).hasMatch() )
        {
            section = IdentifierReferenceSection::outside;
        }

        QString semanticLine = sanitizeLineForAnalysis(rawLine);
        if ( section == IdentifierReferenceSection::symbols )
        {
            const int lineLength = static_cast<int>(semanticLine.size());
            const std::optional<CogScriptSymbolDeclarationNameSpan> nameSpan =
                cogScriptSymbolDeclarationNameSpan(rawLine);
            for ( int column = 0; column < lineLength; ++column )
            {
                if ( !nameSpan
                    || column < nameSpan->start
                    || column >= nameSpan->start + nameSpan->length )
                {
                    semanticLine[column] = QLatin1Char(' ');
                }
            }
        }

        const int lineLength = static_cast<int>(semanticLine.size());
        for ( int column = 0; column < lineLength; )
        {
            if ( !isIdentifierStart(semanticLine.at(column)) )
            {
                ++column;
                continue;
            }

            const int start = column++;
            while ( column < lineLength
                && isIdentifierPart(semanticLine.at(column)) )
            {
                ++column;
            }
            const int length = column - start;
            const int absoluteStart = sourceOffset + start;
            if ( length == identifier.size()
                && source.mid(absoluteStart, length).compare(
                    identifier, Qt::CaseInsensitive) == 0 )
            {
                occurrences.push_back(CogScriptIdentifierOccurrence{
                    .position = absoluteStart,
                    .length = length,
                });
            }
        }

        sourceOffset += static_cast<int>(rawLine.size()) + 1;
    }
    return occurrences;
}

QList<CogMaskThingTypeOption> cogScriptMaskThingTypeOptions(const ProjectType kind)
{
    QList<CogMaskThingTypeOption> options{
        { QStringLiteral("Camera"), 1 },
        { QStringLiteral("Actor"), 2 },
        { QStringLiteral("Weapon"), 3 },
        { QStringLiteral("Debris"), 4 },
        { QStringLiteral("Item"), 5 },
        { QStringLiteral("Explosion"), 6 },
        { QStringLiteral("COG"), 7 },
        { QStringLiteral("Ghost"), 8 },
        { QStringLiteral("Corpse"), 9 },
        { QStringLiteral("Player"), 10 },
        { QStringLiteral("Particle"), 11 },
    };

    if (kind == ProjectType::IJIM)
    {
        options.push_back({ QStringLiteral("Hint"), 12 });
        options.push_back({ QStringLiteral("Sprite"), 13 });
        options.push_back({ QStringLiteral("Polyline"), 14 });
    }

    return options;
}

std::uint32_t cogScriptSupportedMaskBits(const ProjectType kind)
{
    std::uint32_t mask = 0x1u; // system
    for (const CogMaskThingTypeOption& option : cogScriptMaskThingTypeOptions(kind))
    {
        if (option.typeNumber >= 0
            && option.typeNumber < static_cast<int>(std::numeric_limits<std::uint32_t>::digits))
        {
            mask |= (1u << static_cast<std::uint32_t>(option.typeNumber));
        }
    }
    return mask;
}

QList<CogScriptFlagOption> cogScriptFlagOptions(const ProjectType kind)
{
    // OpenJones3D's reconstructed Sith COG parser and the classic OpenJKDF2
    // engine expose the same 0x001..0x200 bit vocabulary. Do not infer new
    // IJIM-only or classic-only meanings: keep the project boundary explicit
    // while returning the shared, engine-backed table for every supported
    // project. PULSE_SET and TIMER_SET are preserved by the parser but are
    // normally controlled by SetPulse/SetTimer at runtime.
    (void)kind;
    return QList<CogScriptFlagOption>{
        {
            0x001u,
            QStringLiteral("Debug"),
            QStringLiteral("Enable extra runtime logging from the COG layer."),
            false,
        },
        {
            0x002u,
            QStringLiteral("Disabled"),
            QStringLiteral("Prevent the COG from handling messages until it is re-enabled."),
            false,
        },
        {
            0x004u,
            QStringLiteral("Pulse active"),
            QStringLiteral("Runtime state set while a periodic pulse is active; normally managed by SetPulse()."),
            true,
        },
        {
            0x008u,
            QStringLiteral("Timer active"),
            QStringLiteral("Runtime state set while a one-shot timer is active; normally managed by SetTimer()."),
            true,
        },
        {
            0x010u,
            QStringLiteral("Paused"),
            QStringLiteral("Prevent message execution while the COG is paused."),
            false,
        },
        {
            0x020u,
            QStringLiteral("Class"),
            QStringLiteral("Mark a class or template-style COG rather than a placed runtime COG instance."),
            false,
        },
        {
            0x040u,
            QStringLiteral("Local"),
            QStringLiteral("Prevent normal remote forwarding for multiplayer message sends."),
            false,
        },
        {
            0x080u,
            QStringLiteral("Server"),
            QStringLiteral("Preserve the original engine's server-oriented compatibility flag."),
            false,
        },
        {
            0x100u,
            QStringLiteral("Global"),
            QStringLiteral("Preserve the original engine's global-scope compatibility flag."),
            false,
        },
        {
            0x200u,
            QStringLiteral("No sync"),
            QStringLiteral("Suppress gameplay synchronization sends triggered by many mutating host functions."),
            false,
        },
    };
}

std::uint32_t cogScriptSupportedFlagBits(const ProjectType kind)
{
    std::uint32_t bits = 0u;
    for (const CogScriptFlagOption& option : cogScriptFlagOptions(kind))
    {
        bits |= option.bit;
    }
    return bits;
}

CogScriptFlagsValueParseResult parseCogScriptFlagsValue(const QStringView text)
{
    const QString trimmed = text.toString().trimmed();
    if (trimmed.isEmpty())
    {
        return {};
    }

    QString digits = trimmed;
    if (digits.startsWith(QStringLiteral("0x"), Qt::CaseInsensitive))
    {
        digits.remove(0, 2);
    }
    if (digits.isEmpty())
    {
        return { CogScriptFlagsValueParseState::invalid, 0u };
    }

    const auto isHexDigit = [](const QChar ch)
    {
        return (ch >= QLatin1Char('0') && ch <= QLatin1Char('9'))
            || (ch >= QLatin1Char('a') && ch <= QLatin1Char('f'))
            || (ch >= QLatin1Char('A') && ch <= QLatin1Char('F'));
    };
    if (!std::ranges::all_of(digits, isHexDigit))
    {
        return { CogScriptFlagsValueParseState::invalid, 0u };
    }

    bool ok = false;
    const qulonglong parsed = digits.toULongLong(&ok, 16);
    if (!ok || parsed > std::numeric_limits<std::uint32_t>::max())
    {
        return { CogScriptFlagsValueParseState::outOfRange, 0u };
    }

    return {
        CogScriptFlagsValueParseState::ok,
        static_cast<std::uint32_t>(parsed),
    };
}

QString formatCogScriptFlagsValue(const std::uint32_t value)
{
    return QStringLiteral("0x%1").arg(
        static_cast<qulonglong>(value), 8, 16, QLatin1Char('0'));
}

QString cogScriptAttributeHelpText(const QString& attributeName)
{
    const QString lower = attributeName.trimmed().toLower();
    if (lower == QStringLiteral("local"))
    {
        return QStringLiteral("Keep this symbol local to the COG instance instead of exposing it as an external link.");
    }
    if (lower == QStringLiteral("desc"))
    {
        return QStringLiteral("Attach a one-word description used by the editor for this symbol.");
    }
    if (lower == QStringLiteral("nolink"))
    {
        return QStringLiteral("Prevent this symbol from being linked to the COG script. "
            "No messages will be sent from the entity (sector, surface, or thing) to this script.");
    }
    if (lower == QStringLiteral("linkid"))
    {
        return QStringLiteral("Assign a sender group id readable from code through GetSenderID().");
    }
    if (lower == QStringLiteral("mask"))
    {
        return QStringLiteral("Restrict which source thing types or system events can trigger messages for this symbol.");
    }
    return {};
}

QString cogScriptFlagsHelpText()
{
    return QStringLiteral("Optional engine flags line. Use flags=<hexadecimal> before the symbols/code sections; values are hexadecimal even without a 0x prefix. Pulse active and Timer active are normally runtime-managed.");
}

QString cogScriptIdentifierAtColumn(const QString& line, int column)
{
    auto isIdentifierChar = [](const QChar ch)
    {
        return ch.isLetterOrNumber() || ch == QLatin1Char('_');
    };

    if (line.isEmpty())
    {
        return {};
    }

    column = std::clamp(column, 0, static_cast<int>(line.size()) - 1);
    if (!isIdentifierChar(line.at(column)))
    {
        if (column > 0 && isIdentifierChar(line.at(column - 1)))
        {
            --column;
        }
        else
        {
            return {};
        }
    }

    int start = column;
    while (start > 0 && isIdentifierChar(line.at(start - 1)))
    {
        --start;
    }

    int end = column + 1;
    while (end < line.size() && isIdentifierChar(line.at(end)))
    {
        ++end;
    }

    return line.mid(start, end - start);
}

QString cogScriptHoverHelpText(const QString& rawLine, const int column)
{
    const QString identifier = cogScriptIdentifierAtColumn(rawLine, column).trimmed();
    if (identifier.isEmpty())
    {
        return {};
    }

    const QString attributeHelp = cogScriptAttributeHelpText(identifier);
    if (!attributeHelp.isEmpty())
    {
        return attributeHelp;
    }

    if (identifier.compare(QStringLiteral("flags"), Qt::CaseInsensitive) == 0)
    {
        const QString trimmed = stripComment(rawLine).trimmed();
        if (trimmed.startsWith(QStringLiteral("flags"), Qt::CaseInsensitive))
        {
            return cogScriptFlagsHelpText();
        }
    }

    return {};
}

bool cogScriptTypeSupportsTriggerFilterAttributes(const QString& typeName)
{
    const QString lower = typeName.trimmed().toLower();
    return lower == QStringLiteral("message")
        || lower == QStringLiteral("thing")
        || lower == QStringLiteral("surface")
        || lower == QStringLiteral("sector");
}

} // namespace sed::ui::cog
