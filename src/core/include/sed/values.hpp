#pragma once

#include <cstdint>
#include <initializer_list>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace sed
{

struct Level;

enum class ValueType : std::int32_t
{
    str,
    integer,
    floating,
    flag,
    ptr,
    vect,
    frame,
    unknown,
};

enum class CogType : std::int32_t
{
    unk,
    ai,
    cog,
    key,
    mat,
    msg,
    model3do,
    sec,
    wav,
    surface,
    template_,
    thing,
    int_,
    float_,
    vect,
};

enum class AdjType : std::int32_t
{
    unk,
    mat,
    cog,
    snd,
    pup,
    spr,
    par,
    model3do,
    ai,
    tpl,
    frame,
};

struct Value
{
    std::string name{};
    ValueType type{ ValueType::unknown };

    std::int32_t intValue{};
    double floatValue{};
    std::string stringValue{};

    std::string asString() const;
    std::optional<Value> clone() const
    {
        return *this;
    }

    // Parses and assigns from a string representation used by the original editor.
    bool parse(const std::string_view text);
};

struct CogValue : Value
{
    CogType cogType{ CogType::unk };
    bool local{ false };
    std::string description{};
    std::int32_t mask{};

    std::string asJedString() const;
    std::string asJedString(const Level& level) const;
    bool parseJed(const std::string_view text);
    bool parseJed(const Level& level, const std::string_view text);
};

struct TplValue : Value
{
    AdjType adjType{ AdjType::unk };

    // Frame is stored as a single token in JKL/JED, e.g. "(x/y/z:pch/yaw/rol)" or "(x/y/z)".
    bool tryGetFrame(double& x, double& y, double& z, double& pch, double& yaw, double& rol) const;
    void getFrame(double& x, double& y, double& z, double& pch, double& yaw, double& rol) const;
    void setFrame(double x, double y, double z, double pch, double yaw, double rol);
};

/** Format a numeric Thing/template tuple as one engine-safe token.
 *
 * The text Thing parsers in JK, MotS, and IJIM require vector-shaped values to
 * be enclosed in parentheses. The default shortest-roundtrip formatting
 * preserves each authored double exactly.
 */
std::string formatTplTuple(std::initializer_list<double> components);

// Explicit precision is for presentation; authored values use the overload above.
std::string formatTplTuple(
    std::initializer_list<double> components,
    int significantDigits);

/** Deserialize one `name=value` Thing/template parameter.
 *
 * Known tuple parameters accept both canonical parenthesized input and legacy
 * bare slash input.  Valid tuples are normalized to a parenthesized token;
 * classic scalar `light`/`lightintensity` values and unrelated strings remain
 * unchanged.
 */
TplValue deserializeTplValue(std::string_view name, std::string_view text);

/** Serialize one Thing/template parameter value in engine-compatible syntax. */
std::string serializeTplValue(const TplValue& value);

/** Deserialize one IJIM NDY Thing/template parameter.
 *
 * IJIM's text parser accepts only RGB for actor `lightintensity`, while SED's
 * editor model keeps a fourth range component.  RGB-only input is therefore
 * materialized with the editor default range of 1.  Other parameters retain
 * the generic Thing/template codec semantics.
 */
TplValue deserializeIjimNdyTplValue(std::string_view name, std::string_view text);

/** Serialize one IJIM NDY Thing/template parameter for the retail text parser.
 *
 * A valid four-component actor `lightintensity` is intentionally written as
 * RGB only.  Regular Thing `light`, classic scalar values, and unrelated
 * vector-shaped parameters are not changed by this compatibility rule.
 */
std::string serializeIjimNdyTplValue(const TplValue& value);

std::string_view toString(ValueType type) noexcept;
std::string_view toString(CogType type) noexcept;
std::string_view toString(AdjType type) noexcept;

ValueType tplValueTypeForName(const std::string_view name) noexcept;
AdjType tplAdjTypeForName(const std::string_view name) noexcept;

} // namespace sed
