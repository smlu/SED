#include "ui/recovery/windows_recovery_hooks.hpp"

#include <QtGlobal>

#ifdef Q_OS_WIN
#include <windows.h>
#include <dbghelp.h>

#include <array>
#include <cwchar>
#include <exception>

namespace
{

std::array<wchar_t, 32768> gCrashDumpPath{};
std::array<wchar_t, 32768> gCrashMarkerPath{};

void writeCrashMarker(const wchar_t* reason) noexcept
{
    const HANDLE file = CreateFileW(
        gCrashMarkerPath.data(), GENERIC_WRITE, FILE_SHARE_READ, nullptr,
        CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_WRITE_THROUGH, nullptr);
    if (file == INVALID_HANDLE_VALUE)
    {
        return;
    }
    SYSTEMTIME time{};
    GetSystemTime(&time);
    wchar_t message[512]{};
    const int count = swprintf_s(
        message,
        L"SED abnormal termination marker\r\nUTC %04u-%02u-%02uT%02u:%02u:%02u.%03uZ\r\nReason: %ls\r\n"
        L"The latest atomically published .sedrecovery checkpoint is authoritative.\r\n",
        time.wYear, time.wMonth, time.wDay, time.wHour, time.wMinute,
        time.wSecond, time.wMilliseconds, reason ? reason : L"unknown");
    DWORD written{};
    if (count > 0)
    {
        WriteFile(file, message, static_cast<DWORD>(count * sizeof(wchar_t)), &written, nullptr);
        FlushFileBuffers(file);
    }
    CloseHandle(file);
}

void writeCrashDump(EXCEPTION_POINTERS* exceptionPointers) noexcept
{
    const HANDLE file = CreateFileW(
        gCrashDumpPath.data(), GENERIC_WRITE, FILE_SHARE_READ, nullptr,
        CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_WRITE_THROUGH, nullptr);
    if (file != INVALID_HANDLE_VALUE)
    {
        MINIDUMP_EXCEPTION_INFORMATION exceptionInfo{};
        exceptionInfo.ThreadId = GetCurrentThreadId();
        exceptionInfo.ExceptionPointers = exceptionPointers;
        exceptionInfo.ClientPointers = FALSE;
        MiniDumpWriteDump(
            GetCurrentProcess(), GetCurrentProcessId(), file,
            static_cast<MINIDUMP_TYPE>(MiniDumpWithIndirectlyReferencedMemory
                | MiniDumpWithThreadInfo | MiniDumpWithUnloadedModules),
            exceptionPointers ? &exceptionInfo : nullptr, nullptr, nullptr);
        FlushFileBuffers(file);
        CloseHandle(file);
    }
}

LONG WINAPI unhandledExceptionFilter(EXCEPTION_POINTERS* exceptionPointers) noexcept
{
    writeCrashMarker(L"unhandled exception");
    writeCrashDump(exceptionPointers);
    return EXCEPTION_CONTINUE_SEARCH;
}

[[noreturn]] void terminateHandler() noexcept
{
    // Escaping C++ exceptions call std::terminate and are not guaranteed to
    // traverse SetUnhandledExceptionFilter. Record the same diagnostics here
    // without touching Qt or editor-owned memory.
    writeCrashMarker(L"C++ std::terminate");
    writeCrashDump(nullptr);
    TerminateProcess(GetCurrentProcess(), ERROR_UNHANDLED_EXCEPTION);
    for (;;) {}
}

DWORD WINAPI applicationRecoveryCallback(void*) noexcept
{
    BOOL canceled = FALSE;
    HRESULT status = ApplicationRecoveryInProgress(&canceled);
    if (SUCCEEDED(status) && !canceled)
    {
        // Deliberately avoid Qt, allocation, locks, OpenGL, and serialization
        // in WER's compromised-process callback. The live checkpoint has
        // already captured the editor; this marker makes the recovery path and
        // crash reason auditable.
        writeCrashMarker(L"Windows Error Reporting recovery callback");
    }
    ApplicationRecoveryFinished(!canceled);
    return 0;
}

} // namespace
#endif

namespace sed::ui::recovery
{

void installWindowsRecoveryHooks(const std::filesystem::path& recoveryDirectory)
{
#ifdef Q_OS_WIN
    std::error_code error{};
    std::filesystem::create_directories(recoveryDirectory, error);
    const std::wstring dump = (recoveryDirectory / L"sed-crash.dmp").wstring();
    const std::wstring marker = (recoveryDirectory / L"sed-crash.txt").wstring();
    wcsncpy_s(gCrashDumpPath.data(), gCrashDumpPath.size(), dump.c_str(), _TRUNCATE);
    wcsncpy_s(gCrashMarkerPath.data(), gCrashMarkerPath.size(), marker.c_str(), _TRUNCATE);
    SetUnhandledExceptionFilter(unhandledExceptionFilter);
    std::set_terminate(terminateHandler);
    (void)RegisterApplicationRecoveryCallback(
        applicationRecoveryCallback, nullptr, 5000, 0);
    (void)RegisterApplicationRestart(L"--recover", 0);
#else
    (void)recoveryDirectory;
#endif
}

} // namespace sed::ui::recovery
