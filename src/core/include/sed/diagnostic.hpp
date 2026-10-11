#pragma once

#include <cstdint>
#include <functional>
#include <string_view>

namespace sed
{

enum class DiagnosticSeverity : std::uint8_t
{
    verbose,
    trace,
    debug,
    info,
    warning,
    error,
    fatal
};

using DiagnosticSink = std::function<void(DiagnosticSeverity severity, std::string_view message)>;

inline void diagnosticEmit(const DiagnosticSink& sink, const DiagnosticSeverity severity, const std::string_view message)
{
    if (!sink)
    {
        return;
    }

    sink(severity, message);
}

} // namespace sed
