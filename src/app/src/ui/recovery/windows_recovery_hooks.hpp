#pragma once

#include <filesystem>

namespace sed::ui::recovery
{

// Installs Windows Error Reporting recovery/restart registration and an
// unhandled-exception minidump marker. Forced TerminateProcess cannot execute
// callbacks; the continuously published .sedrecovery checkpoint remains the
// authoritative editor state in that case.
void installWindowsRecoveryHooks(const std::filesystem::path& recoveryDirectory);

} // namespace sed::ui::recovery
