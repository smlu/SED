#include "app_icon.hpp"

#include <QColor>
#include <QFile>
#include <QSize>
#include <QWidget>

#include <array>

#if defined(Q_OS_WIN)
#include <qt_windows.h>
#endif

namespace
{

QIcon loadEmbeddedIcon(const QString& resourceStem)
{
    constexpr std::array iconSizes{16, 24, 32, 48, 64, 128, 256};

    QIcon icon{};
    for (const int size : iconSizes)
    {
        const QString path = QStringLiteral(":/sed/icons/%1_%2.png").arg(resourceStem).arg(size);
        if (QFile::exists(path))
        {
            icon.addFile(path, QSize(size, size));
        }
    }
    return icon;
}

} // namespace

namespace sed
{

QIcon loadApplicationIcon()
{
    // Embedded PNGs use Qt's built-in decoder and remain available even when
    // optional ICO image-format plugins are not deployed beside the app.
    return loadEmbeddedIcon(QStringLiteral("app_window"));
}

QIcon loadApplicationShellIcon()
{
    return loadEmbeddedIcon(QStringLiteral("app_shell"));
}

bool applyWindowsTaskbarIcon(QWidget& window)
{
#if defined(Q_OS_WIN)
    // Resource ID 1 is the ICO embedded by sed_app.rc. Assign only the large
    // slot so Windows uses it for the taskbar and Alt+Tab while Qt's small
    // borderless icon remains visible in the title bar.
    const HINSTANCE module = GetModuleHandleW(nullptr);
    const HICON taskbarIcon = static_cast<HICON>(LoadImageW(
        module,
        MAKEINTRESOURCEW(1),
        IMAGE_ICON,
        GetSystemMetrics(SM_CXICON),
        GetSystemMetrics(SM_CYICON),
        LR_DEFAULTCOLOR | LR_SHARED));
    if (taskbarIcon == nullptr)
    {
        return false;
    }

    const HWND nativeWindow = reinterpret_cast<HWND>(window.winId());
    if (nativeWindow == nullptr)
    {
        return false;
    }

    SendMessageW(
        nativeWindow,
        WM_SETICON,
        ICON_BIG,
        reinterpret_cast<LPARAM>(taskbarIcon));
    const HICON appliedIcon = reinterpret_cast<HICON>(SendMessageW(
        nativeWindow,
        WM_GETICON,
        ICON_BIG,
        0));
    return appliedIcon == taskbarIcon;
#else
    Q_UNUSED(window);
    return false;
#endif
}

bool applyWindowsTitleBarColor(QWidget& window, const QColor& color)
{
#if defined(Q_OS_WIN)
    // WinIdChange also reports native handle removal during destruction.
    // Decorating chrome must never create a replacement native window there.
    const HWND nativeWindow = reinterpret_cast<HWND>(window.internalWinId());
    if ( !nativeWindow )
    {
        return false;
    }

    using DwmSetWindowAttributeFn = HRESULT(WINAPI*)(HWND, DWORD, LPCVOID, DWORD);
    constexpr DWORD kUseImmersiveDarkMode = 20;
    constexpr DWORD kBorderColor = 34;
    constexpr DWORD kCaptionColor = 35;
    constexpr DWORD kTextColor = 36;

    const HMODULE dwmApi = LoadLibraryW(L"dwmapi.dll");
    if (!dwmApi)
    {
        return false;
    }

    const auto setWindowAttribute = reinterpret_cast<DwmSetWindowAttributeFn>(
        GetProcAddress(dwmApi, "DwmSetWindowAttribute"));
    if ( !setWindowAttribute )
    {
        FreeLibrary(dwmApi);
        return false;
    }

    const BOOL useDarkMode = TRUE;
    setWindowAttribute(
        nativeWindow,
        kUseImmersiveDarkMode,
        &useDarkMode,
        sizeof(useDarkMode));

    const COLORREF captionColor = RGB(color.red(), color.green(), color.blue());
    const COLORREF textColor = RGB(242, 242, 242);
    const HRESULT captionResult = setWindowAttribute(
        nativeWindow,
        kCaptionColor,
        &captionColor,
        sizeof(captionColor));
    setWindowAttribute(nativeWindow, kBorderColor, &captionColor, sizeof(captionColor));
    setWindowAttribute(nativeWindow, kTextColor, &textColor, sizeof(textColor));

    FreeLibrary(dwmApi);
    return SUCCEEDED(captionResult);
#else
    Q_UNUSED(window);
    Q_UNUSED(color);
    return false;
#endif
}

bool refreshWindowsNativeWindowFrame(QWidget& window)
{
#if defined(Q_OS_WIN)
    const HWND nativeWindow = reinterpret_cast<HWND>(window.internalWinId());
    if ( !nativeWindow )
    {
        return false;
    }
    // Qt 6.10 changes native styles before publishing QWindow's new flags.
    // Recalculate once those flags are settled so WM_NCCALCSIZE no longer
    // sees the previous frameless role. Never create or reposition a window.
    return SetWindowPos(nativeWindow, nullptr, 0, 0, 0, 0,
        SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED) != FALSE;
#else
    Q_UNUSED(window);
    return false;
#endif
}

} // namespace sed
