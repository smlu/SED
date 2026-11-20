// Synthetic JED plug-ins for the Plugins menu tests. The behaviour is chosen
// at build time with SED_TEST_JED_PLUGIN_MODE; each mode is a separate module.

#include "sed/plugins/jed_plugin_abi.hpp"

#include <cstdlib>

#if defined(_WIN32)
#define SED_TEST_PLUGIN_EXPORT extern "C" __declspec(dllexport)
#else
#define SED_TEST_PLUGIN_EXPORT extern "C" __attribute__((visibility("default")))
#endif

#define SED_TEST_PLUGIN_EDIT 1
#define SED_TEST_PLUGIN_FAIL 2
#define SED_TEST_PLUGIN_NO_ENTRY 3
#define SED_TEST_PLUGIN_SELECT 4
#define SED_TEST_PLUGIN_SED_ONLY 5
#define SED_TEST_PLUGIN_CRASH 6

#if defined(_WIN32)
#define SED_TEST_LOAD SED_JED_CALL JEDPluginLoadStdCall
#if defined(_MSC_VER) && defined(_M_IX86) && SED_TEST_JED_PLUGIN_MODE != SED_TEST_PLUGIN_NO_ENTRY && SED_TEST_JED_PLUGIN_MODE != SED_TEST_PLUGIN_SED_ONLY
#pragma comment(linker, "/EXPORT:JEDPluginLoadStdCall=_JEDPluginLoadStdCall@4")
#endif
#else
#define SED_TEST_LOAD JEDPluginLoad
#endif

namespace jed = sed::plugins::jed;

#if SED_TEST_JED_PLUGIN_MODE == SED_TEST_PLUGIN_EDIT
SED_TEST_PLUGIN_EXPORT bool SED_TEST_LOAD(jed::IJED* host)
{
    char label[] = "Test lights";
    char text[] = "test plug-in added two lights";
    host->StartUndo(label);
    jed::IJEDLevel* level = host->GetLevel();
    const int first = level->AddLight();
    const int second = level->AddLight();
    host->PanMessage(jed::msg_info, text);
    return first >= 0 && second == first + 1;
}
#elif SED_TEST_JED_PLUGIN_MODE == SED_TEST_PLUGIN_FAIL
SED_TEST_PLUGIN_EXPORT bool SED_TEST_LOAD(jed::IJED*)
{
    return false;
}
#elif SED_TEST_JED_PLUGIN_MODE == SED_TEST_PLUGIN_NO_ENTRY
SED_TEST_PLUGIN_EXPORT int sedTestNotAPlugin()
{
    return 0;
}
#elif SED_TEST_JED_PLUGIN_MODE == SED_TEST_PLUGIN_SELECT
SED_TEST_PLUGIN_EXPORT bool SED_TEST_LOAD(jed::IJED* host)
{
    host->SetMapMode(jed::MM_LT);
    return host->SelectLT(0) >= 0;
}
#elif SED_TEST_JED_PLUGIN_MODE == SED_TEST_PLUGIN_SED_ONLY
SED_TEST_PLUGIN_EXPORT bool SEDPluginLoad(void*)
{
    return true;
}
#elif SED_TEST_JED_PLUGIN_MODE == SED_TEST_PLUGIN_CRASH
// Commits one edit, then ends the process abruptly. Only the out-of-process
// bridge tests load this module.
SED_TEST_PLUGIN_EXPORT bool SED_TEST_LOAD(jed::IJED* host)
{
    char label[] = "Crash light";
    host->StartUndo(label);
    host->GetLevel()->AddLight();
    host->UpdateMap();
    std::_Exit(3);
}
#else
#error "SED_TEST_JED_PLUGIN_MODE must select a test plug-in variant"
#endif
