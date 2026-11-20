// sed-jed-plugin-bridge: runs one legacy JED plug-in outside the editor.
//
// SED starts this helper when a plug-in DLL's architecture differs from SED's
// (for example a 32-bit JED plug-in under 64-bit SED) or when the designer asks
// for plug-ins to run in a separate process. The protocol and the IJED host
// live in sed_core (sed/plugins/jed_plugin_bridge.hpp); this file only owns the
// platform channel and module loading.

#include "sed/plugins/jed_plugin_abi.hpp"
#include "sed/plugins/jed_plugin_bridge.hpp"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <string>

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <fcntl.h>
#include <io.h>
#include <windows.h>
#else
#include <dlfcn.h>
#include <unistd.h>
#endif

namespace
{

namespace bridge = sed::plugins::bridge;
namespace jed = sed::plugins::jed;

class StdioChannel final : public bridge::BlockingChannel
{
public:
    StdioChannel(const int input, const int output)
        : input_(input), output_(output)
    {
    }

    bool write(const std::span<const std::byte> bytes) override
    {
        std::size_t done = 0;
        while ( done < bytes.size() )
        {
            const std::size_t chunk = std::min<std::size_t>(bytes.size() - done, 1u << 20);
#if defined(_WIN32)
            const int written = ::_write(output_, bytes.data() + done, static_cast<unsigned int>(chunk));
#else
            const ssize_t written = ::write(output_, bytes.data() + done, chunk);
#endif
            if ( written <= 0 )
            {
                return false;
            }
            done += static_cast<std::size_t>(written);
        }
        return true;
    }

    bool readExact(const std::span<std::byte> bytes) override
    {
        std::size_t done = 0;
        while ( done < bytes.size() )
        {
            const std::size_t chunk = std::min<std::size_t>(bytes.size() - done, 1u << 20);
#if defined(_WIN32)
            const int got = ::_read(input_, bytes.data() + done, static_cast<unsigned int>(chunk));
#else
            const ssize_t got = ::read(input_, bytes.data() + done, chunk);
#endif
            if ( got <= 0 )
            {
                return false;
            }
            done += static_cast<std::size_t>(got);
        }
        return true;
    }

private:
    int input_;
    int output_;
};

std::string fileNameOf(const std::string& path)
{
    return std::filesystem::path(path).filename().string();
}

#if defined(_WIN32)
using Module = HMODULE;

std::wstring widen(const std::string& utf8)
{
    if ( utf8.empty() )
    {
        return {};
    }
    const int size = MultiByteToWideChar(CP_UTF8, 0, utf8.data(), static_cast<int>(utf8.size()), nullptr, 0);
    std::wstring out(static_cast<std::size_t>(size > 0 ? size : 0), L'\0');
    if ( size > 0 )
    {
        MultiByteToWideChar(CP_UTF8, 0, utf8.data(), static_cast<int>(utf8.size()), out.data(), size);
    }
    return out;
}

Module openModule(const std::string& path, std::string& error)
{
    // Let the plug-in find DLLs it ships beside itself.
    const std::wstring wide = widen(path);
    Module module = LoadLibraryExW(wide.c_str(), nullptr, LOAD_WITH_ALTERED_SEARCH_PATH);
    if ( module == nullptr )
    {
        const DWORD code = GetLastError();
        char buffer[512]{};
        FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, nullptr, code, 0, buffer,
            sizeof(buffer), nullptr);
        error = buffer[0] != '\0' ? std::string(buffer) : "error " + std::to_string(code);
        while ( !error.empty() && (error.back() == '\n' || error.back() == '\r' || error.back() == ' ') )
        {
            error.pop_back();
        }
    }
    return module;
}

void* symbol(Module module, const char* name)
{
    return reinterpret_cast<void*>(GetProcAddress(module, name));
}
#else
using Module = void*;

Module openModule(const std::string& path, std::string& error)
{
    Module module = dlopen(path.c_str(), RTLD_NOW | RTLD_LOCAL);
    if ( module == nullptr )
    {
        const char* text = dlerror();
        error = text != nullptr ? text : "unknown error";
    }
    return module;
}

void* symbol(Module module, const char* name)
{
    return dlsym(module, name);
}
#endif

/** Loads and calls a plug-in with the same diagnostics as SED's in-process host. */
bridge::PluginInvocation invokePlugin(const std::string& path, jed::IJED* host)
{
    bridge::PluginInvocation invocation{};
    const std::string name = fileNameOf(path);
    std::string error;
    Module module = openModule(path, error);
    if ( module == nullptr )
    {
        invocation.error = "Couldn't load plugin " + name + ": " + error;
        return invocation;
    }
    // The module is never unloaded: plug-in windows and threads may outlive the call.
    void* stdCall = symbol(module, jed::kJedPluginLoadStdCallSymbol);
    void* plain = symbol(module, jed::kJedPluginLoadSymbol);
    if ( stdCall == nullptr && plain == nullptr )
    {
        const bool sedPlugin = symbol(module, jed::kSedPluginLoadStdCallSymbol) != nullptr
            || symbol(module, jed::kSedPluginLoadSymbol) != nullptr;
        invocation.error = sedPlugin
            ? "The DLL " + name + " is a SED (ISED) plug-in. SED supports JED (IJED) plug-ins only."
            : "The DLL " + name + " is not a SED or JED plug-in: No load function found!";
        return invocation;
    }
    if (!jed::kSupportsPlainEntry && !stdCall)
    {
        invocation.error = name + " exports only the Delphi register entry JEDPluginLoad. This x86 host requires JEDPluginLoadStdCall.";
        return invocation;
    }
    invocation.loaded = true;
    invocation.result = stdCall != nullptr ? reinterpret_cast<jed::JedPluginLoadStdCall>(stdCall)(host)
                                           : reinterpret_cast<jed::JedPluginLoad>(plain)(host);
    return invocation;
}

} // namespace

int main(int argc, char** argv)
{
    if ( argc != 2 || std::strcmp(argv[1], "--serve") != 0 )
    {
        std::fprintf(stderr,
            "sed-jed-plugin-bridge runs legacy JED plug-ins for SED.\n"
            "SED starts it automatically; it is not meant to be run by hand.\n");
        return 64;
    }

    // Keep the protocol on a private descriptor and point stdout at stderr, so a
    // plug-in that prints cannot corrupt the frames.
#if defined(_WIN32)
    _setmode(_fileno(stdin), _O_BINARY);
    const int output = _dup(_fileno(stdout));
    if ( output >= 0 )
    {
        _setmode(output, _O_BINARY);
        _dup2(_fileno(stderr), _fileno(stdout));
    }
    SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOOPENFILEERRORBOX);
    const int input = _fileno(stdin);
#else
    const int output = dup(STDOUT_FILENO);
    if ( output >= 0 )
    {
        dup2(STDERR_FILENO, STDOUT_FILENO);
    }
    const int input = STDIN_FILENO;
#endif
    if ( output < 0 )
    {
        return 70;
    }
    StdioChannel channel(input, output);
    return bridge::runBridge(channel, invokePlugin);
}
