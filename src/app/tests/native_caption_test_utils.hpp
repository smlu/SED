#pragma once

#include <QApplication>
#include <QByteArray>
#include <QCoreApplication>
#include <QDockWidget>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QPointer>
#include <QRect>
#include <QString>
#include <QWidget>
#include <QWindow>

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <limits>

#ifdef Q_OS_WIN
#  ifndef NOMINMAX
#    define NOMINMAX
#  endif
#  ifndef WIN32_LEAN_AND_MEAN
#    define WIN32_LEAN_AND_MEAN
#  endif
#  include <windows.h>
#  include <dwmapi.h>
#endif

namespace sed::app_tests::native_caption_test
{

inline bool physicalInputIsClear()
{
#ifdef Q_OS_WIN
    constexpr std::array<int, 11> kInputKeys{
        VK_LBUTTON, VK_RBUTTON, VK_MBUTTON, VK_XBUTTON1, VK_XBUTTON2,
        VK_SHIFT, VK_CONTROL, VK_MENU, VK_LWIN, VK_RWIN, VK_CANCEL};
    for ( const int key : kInputKeys )
    {
        if ( (GetAsyncKeyState(key) & 0x8000) != 0 )
        {
            return false;
        }
    }
    return QApplication::mouseButtons() == Qt::NoButton
        && GetCapture() == nullptr && !QWidget::mouseGrabber();
#else
    return QApplication::mouseButtons() == Qt::NoButton
        && !QWidget::mouseGrabber();
#endif
}

inline bool focusTestWidget(
    QWidget* const focusTarget,
    QWidget* const exactFocusWidget = nullptr,
    const int timeoutMs = 2000)
{
    if ( !focusTarget )
    {
        return false;
    }
    QWidget* const topLevel = focusTarget->window();
    if ( !topLevel )
    {
        return false;
    }

    const QPointer<QWidget> guardedTarget(focusTarget);
    const QPointer<QWidget> guardedTopLevel(topLevel);
    const QPointer<QWidget> guardedFocus(exactFocusWidget);
    const bool exactFocusRequested = exactFocusWidget != nullptr;
    const bool initialWidgetGuards = topLevel->isVisible() && topLevel->isEnabled()
        && !topLevel->isMinimized()
        && (!exactFocusRequested
            || (exactFocusWidget->isVisible() && exactFocusWidget->isEnabled()
                && exactFocusWidget->focusPolicy() != Qt::NoFocus
                && exactFocusWidget->window() == topLevel))
        && !QApplication::activeModalWidget() && !QApplication::activePopupWidget()
        && physicalInputIsClear();
#ifdef Q_OS_WIN
    const WId nativeId = topLevel->internalWinId();
    const HWND handle = reinterpret_cast<HWND>(nativeId);
    DWORD processId = 0;
    const DWORD guiThreadId = GetCurrentThreadId();
    DWORD targetThreadId = 0;

    struct NativeActivationState
    {
        BOOL setForegroundResult{};
        DWORD setForegroundError{};
        bool foregroundMatchedAfterCall{};
        HWND setActivePrevious{};
        DWORD setActiveError{};
        bool attachAttempted{};
        BOOL attachResult{};
        DWORD attachError{};
        BOOL detachResult{};
        DWORD detachError{};
        HWND setFocusPrevious{};
        DWORD setFocusError{};
        DWORD initialForegroundFlags{};
        HWND initialForegroundCapture{};
        bool initialForegroundInfoAvailable{};
    };
    NativeActivationState activation{};

    const auto logFailure = [&](const char* const stage)
    {
        const HWND foreground = GetForegroundWindow();
        DWORD foregroundProcessId = 0;
        const DWORD foregroundThreadId = foreground
            ? GetWindowThreadProcessId(foreground, &foregroundProcessId)
            : 0;
        GUITHREADINFO currentForegroundInfo{};
        currentForegroundInfo.cbSize = static_cast<DWORD>(sizeof(currentForegroundInfo));
        const bool currentForegroundInfoAvailable = foregroundThreadId
            && GetGUIThreadInfo(foregroundThreadId, &currentForegroundInfo) != FALSE;
        const auto classNameFor = [](const HWND window)
        {
            std::array<wchar_t, 256> text{};
            const int length = window && IsWindow(window)
                ? GetClassNameW(window, text.data(), static_cast<int>(text.size()))
                : 0;
            return QString::fromWCharArray(text.data(), std::max(0, length)).toUtf8();
        };
        const auto titleFor = [](const HWND window)
        {
            std::array<wchar_t, 512> text{};
            const int length = window && IsWindow(window)
                ? GetWindowTextW(window, text.data(), static_cast<int>(text.size()))
                : 0;
            return QString::fromWCharArray(text.data(), std::max(0, length)).toUtf8();
        };
        const QByteArray targetClass = classNameFor(handle);
        const QByteArray targetTitle = titleFor(handle);
        const QByteArray foregroundClass = classNameFor(foreground);
        const QByteArray foregroundTitle = titleFor(foreground);
        const auto desktopNameFor = [](const HDESK desktop)
        {
            std::array<wchar_t, 256> name{};
            DWORD returnedBytes = 0;
            if ( desktop )
            {
                (void)GetUserObjectInformationW(desktop, UOI_NAME, name.data(),
                    static_cast<DWORD>(sizeof(name)), &returnedBytes);
            }
            return QString::fromWCharArray(name.data()).toUtf8();
        };
        const HDESK targetDesktop = targetThreadId ? GetThreadDesktop(targetThreadId) : nullptr;
        const HDESK foregroundDesktop = foregroundThreadId
            ? GetThreadDesktop(foregroundThreadId)
            : nullptr;
        SetLastError(ERROR_SUCCESS);
        const HDESK inputDesktop = OpenInputDesktop(0, FALSE, DESKTOP_READOBJECTS);
        const DWORD inputDesktopError = GetLastError();
        const QByteArray targetDesktopName = desktopNameFor(targetDesktop);
        const QByteArray foregroundDesktopName = desktopNameFor(foregroundDesktop);
        const QByteArray inputDesktopName = desktopNameFor(inputDesktop);
        if ( inputDesktop )
        {
            (void)CloseDesktop(inputDesktop);
        }
        RECT clipRect{};
        POINT cursor{};
        (void)GetClipCursor(&clipRect);
        (void)GetCursorPos(&cursor);
        std::fprintf(stderr,
            "test native focus acquisition failed: stage=%s target=%p targetClass=%s targetTitle=%s targetPid=%lu targetThread=%lu guiThread=%lu live=%d visible=%d enabled=%d minimized=%d exactRequested=%d exactFocus=%p exactFocusWindow=%p exactFocusPolicy=%d exactFocusVisible=%d exactFocusEnabled=%d widgetGuards=%d activeQt=%p targetQtActive=%d foreground=%p foregroundClass=%s foregroundTitle=%s foregroundPid=%lu foregroundThread=%lu initialForegroundFlags=0x%08lx initialForegroundCapture=%p currentForegroundInfo=%d currentForegroundFlags=0x%08lx currentForegroundCapture=%p modal=%p popup=%p inputClear=%d setForeground=%d foregroundMatched=%d setForegroundLastError=%lu setActivePrevious=%p setActiveLastError=%lu attachAttempted=%d attach=%d attachLastError=%lu detach=%d detachLastError=%lu setFocusPrevious=%p setFocusLastError=%lu desktopsTarget=%s foreground=%s input=%s openInputDesktopLastError=%lu clip=%ld,%ld,%ld,%ld cursor=%ld,%ld\n",
            stage, static_cast<void*>(handle), targetClass.constData(), targetTitle.constData(),
            static_cast<unsigned long>(processId), static_cast<unsigned long>(targetThreadId),
            static_cast<unsigned long>(guiThreadId), nativeId && IsWindow(handle),
            guardedTopLevel && guardedTopLevel->isVisible(),
            guardedTopLevel && guardedTopLevel->isEnabled(),
            guardedTopLevel && guardedTopLevel->isMinimized(),
            exactFocusRequested, static_cast<void*>(guardedFocus.data()),
            static_cast<void*>(guardedFocus ? guardedFocus->window() : nullptr),
            guardedFocus ? static_cast<int>(guardedFocus->focusPolicy()) : -1,
            guardedFocus && guardedFocus->isVisible(), guardedFocus && guardedFocus->isEnabled(),
            initialWidgetGuards, static_cast<void*>(QApplication::activeWindow()),
            QApplication::activeWindow() == guardedTopLevel, static_cast<void*>(foreground),
            foregroundClass.constData(), foregroundTitle.constData(),
            static_cast<unsigned long>(foregroundProcessId),
            static_cast<unsigned long>(foregroundThreadId),
            static_cast<unsigned long>(activation.initialForegroundFlags),
            static_cast<void*>(activation.initialForegroundCapture),
            activation.initialForegroundInfoAvailable, currentForegroundInfo.flags,
            static_cast<void*>(currentForegroundInfo.hwndCapture),
            static_cast<void*>(QApplication::activeModalWidget()),
            static_cast<void*>(QApplication::activePopupWidget()), physicalInputIsClear(),
            activation.setForegroundResult, activation.foregroundMatchedAfterCall,
            static_cast<unsigned long>(activation.setForegroundError),
            static_cast<void*>(activation.setActivePrevious),
            static_cast<unsigned long>(activation.setActiveError), activation.attachAttempted,
            activation.attachResult, static_cast<unsigned long>(activation.attachError),
            activation.detachResult, static_cast<unsigned long>(activation.detachError),
            static_cast<void*>(activation.setFocusPrevious),
            static_cast<unsigned long>(activation.setFocusError), targetDesktopName.constData(),
            foregroundDesktopName.constData(), inputDesktopName.constData(),
            static_cast<unsigned long>(inputDesktopError), clipRect.left, clipRect.top,
            clipRect.right, clipRect.bottom, cursor.x, cursor.y);
    };
    const auto fail = [&logFailure](const char* const stage)
    {
        logFailure(stage);
        return false;
    };

    if ( !initialWidgetGuards )
    {
        return fail("initial-widget-modal-popup-or-input-guard");
    }
    if ( !nativeId || !IsWindow(handle) || !IsWindowVisible(handle)
        || !IsWindowEnabled(handle) || IsIconic(handle)
        || !(targetThreadId = GetWindowThreadProcessId(handle, &processId))
        || processId != GetCurrentProcessId() || targetThreadId != guiThreadId )
    {
        return fail("target-HWND-not-owned-by-current-GUI-thread");
    }

    const HWND existingForeground = GetForegroundWindow();
    const DWORD foregroundThreadId = existingForeground
        ? GetWindowThreadProcessId(existingForeground, nullptr)
        : 0;
    GUITHREADINFO foregroundInfo{};
    foregroundInfo.cbSize = static_cast<DWORD>(sizeof(foregroundInfo));
    activation.initialForegroundInfoAvailable = foregroundThreadId
        && GetGUIThreadInfo(foregroundThreadId, &foregroundInfo) != FALSE;
    if ( activation.initialForegroundInfoAvailable )
    {
        activation.initialForegroundFlags = foregroundInfo.flags;
        activation.initialForegroundCapture = foregroundInfo.hwndCapture;
    }
    const bool foregroundHasActiveInteraction = activation.initialForegroundInfoAvailable
        && (foregroundInfo.flags & (GUI_INMOVESIZE | GUI_INMENUMODE
            | GUI_SYSTEMMENUMODE | GUI_POPUPMENUMODE)) != 0;
    if ( foregroundHasActiveInteraction || foregroundInfo.hwndCapture )
    {
        return fail("initial-foreground-thread-has-menu-or-capture");
    }
#else
    const auto fail = [](const char*)
    {
        return false;
    };
    if ( !initialWidgetGuards )
    {
        return false;
    }
#endif

    const auto targetStillOwned = [&]()
    {
        if ( !guardedTarget || !guardedTopLevel
            || guardedTarget->window() != guardedTopLevel
            || !guardedTarget->isVisible() || !guardedTarget->isEnabled()
            || !guardedTopLevel->isVisible() || !guardedTopLevel->isEnabled()
            || guardedTopLevel->isMinimized()
            || ( exactFocusRequested && !guardedFocus )
            || ( exactFocusRequested
                && (!guardedFocus->isVisible() || !guardedFocus->isEnabled()
                    || guardedFocus->focusPolicy() == Qt::NoFocus
                    || guardedFocus->window() != guardedTopLevel))
            || QApplication::activeModalWidget() || QApplication::activePopupWidget()
            || !physicalInputIsClear() )
        {
            return false;
        }
#ifdef Q_OS_WIN
        if ( guardedTopLevel->internalWinId() != nativeId || !IsWindow(handle)
            || !IsWindowVisible(handle) || !IsWindowEnabled(handle) || IsIconic(handle) )
        {
            return false;
        }
        DWORD currentProcessId = 0;
        const DWORD currentThreadId = GetWindowThreadProcessId(handle, &currentProcessId);
        return currentProcessId == processId && currentThreadId == targetThreadId
            && currentThreadId == guiThreadId;
#else
        return true;
#endif
    };

    guardedTopLevel->raise();
    if ( !targetStillOwned() )
    {
        return fail("target-lost-after-raise");
    }
    guardedTopLevel->activateWindow();
    if ( !targetStillOwned() )
    {
        return fail("target-lost-after-Qt-activation");
    }
    if ( QWindow* const windowHandle = guardedTopLevel->windowHandle() )
    {
        windowHandle->requestActivate();
    }
    if ( !targetStillOwned() )
    {
        return fail("target-lost-after-QWindow-requestActivate");
    }
#ifdef Q_OS_WIN
    SetLastError(ERROR_SUCCESS);
    activation.setForegroundResult = SetForegroundWindow(handle);
    activation.setForegroundError = GetLastError();
    activation.foregroundMatchedAfterCall = GetForegroundWindow() == handle;
    SetLastError(ERROR_SUCCESS);
    activation.setActivePrevious = SetActiveWindow(handle);
    activation.setActiveError = GetLastError();
    if ( !targetStillOwned() )
    {
        return fail("target-lost-after-native-activation");
    }

    const HWND foregroundBeforeAttach = GetForegroundWindow();
    const DWORD attachForegroundThreadId = foregroundBeforeAttach
        ? GetWindowThreadProcessId(foregroundBeforeAttach, nullptr)
        : 0;
    if ( foregroundBeforeAttach != handle && attachForegroundThreadId
        && attachForegroundThreadId != guiThreadId )
    {
        class ScopedThreadInputAttachment final
        {
        public:
            ScopedThreadInputAttachment(
                const DWORD first, const DWORD second, NativeActivationState& activation)
                : first_(first), second_(second), activation_(activation)
            {
                activation_.attachAttempted = true;
                SetLastError(ERROR_SUCCESS);
                activation_.attachResult = AttachThreadInput(first_, second_, TRUE);
                activation_.attachError = GetLastError();
            }

            ~ScopedThreadInputAttachment()
            {
                if ( activation_.attachResult )
                {
                    SetLastError(ERROR_SUCCESS);
                    activation_.detachResult = AttachThreadInput(first_, second_, FALSE);
                    activation_.detachError = GetLastError();
                }
            }

            ScopedThreadInputAttachment(const ScopedThreadInputAttachment&) = delete;
            ScopedThreadInputAttachment& operator=(const ScopedThreadInputAttachment&) = delete;

            bool attached() const
            {
                return activation_.attachResult != FALSE;
            }

        private:
            DWORD first_{};
            DWORD second_{};
            NativeActivationState& activation_;
        };

        {
            const ScopedThreadInputAttachment attached(
                guiThreadId, attachForegroundThreadId, activation);
            if ( attached.attached() )
            {
                SetLastError(ERROR_SUCCESS);
                activation.setForegroundResult = SetForegroundWindow(handle);
                activation.setForegroundError = GetLastError();
                activation.foregroundMatchedAfterCall = GetForegroundWindow() == handle;
                SetLastError(ERROR_SUCCESS);
                activation.setActivePrevious = SetActiveWindow(handle);
                activation.setActiveError = GetLastError();
                SetLastError(ERROR_SUCCESS);
                activation.setFocusPrevious = SetFocus(handle);
                activation.setFocusError = GetLastError();
            }
        }
    }
    if ( GetForegroundWindow() == handle )
    {
        SetLastError(ERROR_SUCCESS);
        activation.setActivePrevious = SetActiveWindow(handle);
        activation.setActiveError = GetLastError();
        SetLastError(ERROR_SUCCESS);
        activation.setFocusPrevious = SetFocus(handle);
        activation.setFocusError = GetLastError();
    }
#endif

    QElapsedTimer deadline{};
    deadline.start();
    const auto guardsRemain = [&]()
    {
        if ( !guardedTarget || !guardedTopLevel
            || guardedTarget->window() != guardedTopLevel || !guardedTarget->isVisible()
            || !guardedTarget->isEnabled() || !guardedTopLevel->isVisible()
            || !guardedTopLevel->isEnabled() || guardedTopLevel->isMinimized()
            || ( exactFocusRequested && !guardedFocus )
            || ( exactFocusRequested
                && (!guardedFocus->isVisible() || !guardedFocus->isEnabled()
                    || guardedFocus->focusPolicy() == Qt::NoFocus
                    || guardedFocus->window() != guardedTopLevel))
            || QApplication::activeModalWidget() || QApplication::activePopupWidget()
            || !physicalInputIsClear() )
        {
            return false;
        }
#ifdef Q_OS_WIN
        if ( guardedTopLevel->internalWinId() != nativeId || !IsWindow(handle)
            || !IsWindowVisible(handle) || !IsWindowEnabled(handle) || IsIconic(handle) )
        {
            return false;
        }
        DWORD currentProcessId = 0;
        const DWORD currentThreadId = GetWindowThreadProcessId(handle, &currentProcessId);
        if ( currentProcessId != processId || currentThreadId != targetThreadId
            || currentThreadId != guiThreadId )
        {
            return false;
        }
        const HWND currentForeground = GetForegroundWindow();
        const DWORD currentForegroundThread = currentForeground
            ? GetWindowThreadProcessId(currentForeground, nullptr)
            : 0;
        GUITHREADINFO currentForegroundInfo{};
        currentForegroundInfo.cbSize = static_cast<DWORD>(sizeof(currentForegroundInfo));
        if ( currentForegroundThread
            && GetGUIThreadInfo(currentForegroundThread, &currentForegroundInfo)
            && (currentForegroundInfo.hwndCapture
                || (currentForegroundInfo.flags & (GUI_INMOVESIZE | GUI_INMENUMODE
                    | GUI_SYSTEMMENUMODE | GUI_POPUPMENUMODE)) != 0) )
        {
            return false;
        }
#endif
        return true;
    };
    const auto acquired = [&]()
    {
        if ( !guardsRemain()
            || QApplication::activeWindow() != guardedTopLevel )
        {
            return false;
        }
#ifdef Q_OS_WIN
        if ( !IsWindow(handle) || GetForegroundWindow() != handle )
        {
            return false;
        }
#endif
        if ( exactFocusRequested )
        {
            if ( !guardedFocus )
            {
                return false;
            }
            guardedFocus->setFocus(Qt::OtherFocusReason);
            return guardsRemain() && QApplication::focusWidget() == guardedFocus;
        }
        return true;
    };
    while ( !acquired() && deadline.elapsed() < timeoutMs )
    {
        if ( !guardsRemain() )
        {
            return fail("modal-popup-input-or-window-guard-changed-during-pump");
        }
        QCoreApplication::processEvents(QEventLoop::AllEvents, 10);
#ifdef Q_OS_WIN
        if ( !acquired() )
        {
            (void)MsgWaitForMultipleObjectsEx(0, nullptr, 5, QS_ALLINPUT, MWMO_INPUTAVAILABLE);
        }
#endif
    }
    if ( acquired() )
    {
        return true;
    }
#ifdef Q_OS_WIN
    const char* const failureStage = !guardsRemain()
        ? "modal-popup-input-or-window-guard-blocked-predicate"
        : QApplication::activeWindow() != guardedTopLevel
            ? "Qt-active-window-not-acquired"
            : GetForegroundWindow() != handle
                ? "native-foreground-not-acquired"
                : exactFocusRequested && QApplication::focusWidget() != guardedFocus
                    ? "exact-Qt-focus-not-acquired"
                    : "focus-predicate-not-acquired";
    return fail(failureStage);
#else
    return false;
#endif
}

class ScopedTestFocusRestore final
{
public:
    ScopedTestFocusRestore()
        : activeWindow_(QApplication::activeWindow()),
          focusWidget_(QApplication::focusWidget())
    {
#ifdef Q_OS_WIN
        foreground_ = GetForegroundWindow();
        if ( foreground_ && IsWindow(foreground_) )
        {
            foregroundThread_ = GetWindowThreadProcessId(foreground_, &foregroundProcess_);
        }
#endif
    }

    ~ScopedTestFocusRestore()
    {
        if ( activeWindow_ && activeWindow_->isVisible() && activeWindow_->isEnabled() )
        {
            activeWindow_->raise();
            activeWindow_->activateWindow();
            if ( focusWidget_ && focusWidget_->isVisible() && focusWidget_->isEnabled() )
            {
                focusWidget_->setFocus(Qt::OtherFocusReason);
            }
        }
#ifdef Q_OS_WIN
        if ( foreground_ && IsWindow(foreground_) )
        {
            DWORD processId = 0;
            const DWORD threadId = GetWindowThreadProcessId(foreground_, &processId);
            if ( processId == foregroundProcess_ && threadId == foregroundThread_ )
            {
                (void)SetForegroundWindow(foreground_);
            }
        }
#endif
    }

    ScopedTestFocusRestore(const ScopedTestFocusRestore&) = delete;
    ScopedTestFocusRestore& operator=(const ScopedTestFocusRestore&) = delete;

private:
    QPointer<QWidget> activeWindow_{};
    QPointer<QWidget> focusWidget_{};
#ifdef Q_OS_WIN
    HWND foreground_{};
    DWORD foregroundProcess_{};
    DWORD foregroundThread_{};
#endif
};

#ifdef Q_OS_WIN
inline bool windowMayObstructDock(const HWND window)
{
    if (!IsWindowVisible(window) || IsIconic(window))
    {
        return false;
    }
    DWORD cloak{};
    const HRESULT result = DwmGetWindowAttribute(
        window, DWMWA_CLOAKED, &cloak, sizeof(cloak));
    // A cloaked window retains WS_VISIBLE while DWM suppresses it. Preserve
    // the veto when presentation cannot be determined instead of assuming it.
    return FAILED(result) || cloak == 0;
}

template<typename HitTest>
inline bool resolveMaximizeButtonPoint(const RECT& reportedMax, HitTest&& hitTest, POINT& result)
{
    const std::int64_t width = static_cast<std::int64_t>(reportedMax.right) - reportedMax.left;
    const std::int64_t height = static_cast<std::int64_t>(reportedMax.bottom) - reportedMax.top;
    if ( width <= 0 || height <= 0 )
    {
        return false;
    }
    const auto accept = [&](const POINT point)
    {
        if ( hitTest(point) != HTMAXBUTTON )
        {
            return false;
        }
        result = point;
        return true;
    };
    const POINT center{
        static_cast<LONG>(reportedMax.left + width / 2),
        static_cast<LONG>(reportedMax.top + height / 2)};
    if ( accept(center) )
    {
        return true;
    }

    // Accessibility bounds can overlap neighboring caption controls. Stay
    // inside the enabled Max rectangle and require an actual native Max hit.
    constexpr int kColumns = 5;
    constexpr int kRows = 3;
    for ( int row = 0; row < kRows; ++row )
    {
        for ( int column = 0; column < kColumns; ++column )
        {
            const POINT candidate{
                static_cast<LONG>(reportedMax.left
                    + (width - 1) * (2 * column + 1) / (2 * kColumns)),
                static_cast<LONG>(reportedMax.top
                    + (height - 1) * (2 * row + 1) / (2 * kRows))};
            if ( accept(candidate) )
            {
                return true;
            }
        }
    }
    return false;
}
#endif

inline bool maximizeButtonClickRoundTrip(QDockWidget* const dock)
{
#ifdef Q_OS_WIN
    if ( !dock || !dock->isVisible() || !dock->isFloating() )
    {
        return false;
    }
    const QPointer<QDockWidget> owner(dock);
    const QByteArray dockObjectName = dock->objectName().toUtf8();
    const QRect initialDockGeometry = dock->geometry();
    const QRect initialDockFrameGeometry = dock->frameGeometry();
    const qreal initialDockDpr = dock->devicePixelRatioF();
    const WId nativeId = dock->internalWinId();
    const HWND handle = reinterpret_cast<HWND>(nativeId);
    DWORD processId = 0;
    if ( !nativeId || !IsWindow(handle) || !IsWindowVisible(handle) || IsIconic(handle)
        || IsZoomed(handle) || !GetWindowThreadProcessId(handle, &processId)
        || processId != GetCurrentProcessId() )
    {
        return false;
    }

    const LONG_PTR style = GetWindowLongPtrW(handle, GWL_STYLE);
    if ( (style & (WS_MAXIMIZEBOX | WS_SYSMENU)) != (WS_MAXIMIZEBOX | WS_SYSMENU) )
    {
        return false;
    }

    WINDOWPLACEMENT initialPlacement{};
    initialPlacement.length = static_cast<UINT>(sizeof(initialPlacement));
    RECT initialFrame{};
    POINT initialCursor{};
    if ( !GetWindowPlacement(handle, &initialPlacement)
        || !GetWindowRect(handle, &initialFrame) || !GetCursorPos(&initialCursor) )
    {
        return false;
    }
    const HWND initialForeground = GetForegroundWindow();

    MONITORINFO monitor{};
    monitor.cbSize = static_cast<DWORD>(sizeof(monitor));
    if ( !GetMonitorInfoW(MonitorFromWindow(handle, MONITOR_DEFAULTTONEAREST), &monitor) )
    {
        return false;
    }
    const auto rectInside = [](const RECT& outer, const RECT& inner)
    {
        return inner.left >= outer.left && inner.top >= outer.top
            && inner.right <= outer.right && inner.bottom <= outer.bottom;
    };
    const auto sameRect = [](const RECT& left, const RECT& right)
    {
        return left.left == right.left && left.top == right.top
            && left.right == right.right && left.bottom == right.bottom;
    };
    const RECT workArea = monitor.rcWork;
    RECT probeFrame = initialFrame;
    bool temporarilyMoved = false;
    const auto ownsLiveHandle = [&owner, nativeId, handle, processId]()
    {
        DWORD currentProcessId = 0;
        return owner && owner->internalWinId() == nativeId && IsWindow(handle)
            && GetWindowThreadProcessId(handle, &currentProcessId)
            && currentProcessId == processId;
    };
    const auto pumpUntil = [&ownsLiveHandle](const auto& predicate, const int timeoutMs)
    {
        QElapsedTimer deadline{};
        deadline.start();
        while ( ownsLiveHandle() && !predicate() && deadline.elapsed() < timeoutMs )
        {
            QCoreApplication::processEvents(QEventLoop::AllEvents, 10);
            if ( !predicate() )
            {
                (void)MsgWaitForMultipleObjectsEx(0, nullptr, 5,
                    QS_ALLINPUT, MWMO_INPUTAVAILABLE);
            }
        }
        return ownsLiveHandle() && predicate();
    };

    if ( !rectInside(workArea, probeFrame) )
    {
        const LONG width = probeFrame.right - probeFrame.left;
        const LONG height = probeFrame.bottom - probeFrame.top;
        const LONG workWidth = workArea.right - workArea.left;
        const LONG workHeight = workArea.bottom - workArea.top;
        if ( width < 1 || height < 1 || width > workWidth || height > workHeight )
        {
            return false;
        }
        const int left = workArea.left + (workWidth - width) / 2;
        const int top = workArea.top + (workHeight - height) / 2;
        if ( !SetWindowPos(handle, nullptr, left, top, 0, 0,
                SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE) )
        {
            return false;
        }
        temporarilyMoved = true;
        if ( !pumpUntil([&]()
                {
                    return GetWindowRect(handle, &probeFrame)
                        && rectInside(workArea, probeFrame);
                }, 1000) )
        {
            if ( ownsLiveHandle() )
            {
                (void)SetWindowPlacement(handle, &initialPlacement);
            }
            if ( ownsLiveHandle() )
            {
                (void)SetWindowPos(handle, nullptr, initialFrame.left, initialFrame.top,
                    initialFrame.right - initialFrame.left, initialFrame.bottom - initialFrame.top,
                    SWP_NOZORDER | SWP_NOACTIVATE);
            }
            if ( ownsLiveHandle() )
            {
                QCoreApplication::processEvents(QEventLoop::AllEvents, 10);
            }
            return false;
        }
    }
    WINDOWPLACEMENT probePlacement{};
    probePlacement.length = static_cast<UINT>(sizeof(probePlacement));
    const bool probePlacementAvailable = ownsLiveHandle()
        && GetWindowPlacement(handle, &probePlacement) != FALSE;
    const QRect probeDockGeometry = owner ? owner->geometry() : QRect{};
    const QRect probeDockFrameGeometry = owner ? owner->frameGeometry() : QRect{};
    const qreal probeDockDpr = owner ? owner->devicePixelRatioF() : 0.0;

    const bool foregroundReady = focusTestWidget(owner, nullptr, 1000);

    const auto pointOwnedByDock = [handle](const POINT point)
    {
        const HWND hit = WindowFromPoint(point);
        return hit == handle || (hit && GetAncestor(hit, GA_ROOT) == handle);
    };
    const auto firstWindowCoveringDock = [handle](const RECT& frame,
        RECT& windowFrame, RECT& overlapFrame)
    {
        for ( HWND above = GetWindow(handle, GW_HWNDPREV); above;
              above = GetWindow(above, GW_HWNDPREV) )
        {
            if ( !windowMayObstructDock(above) )
            {
                continue;
            }
            RECT aboveFrame{};
            RECT overlap{};
            if ( GetWindowRect(above, &aboveFrame) && IntersectRect(&overlap, &frame, &aboveFrame) )
            {
                windowFrame = aboveFrame;
                overlapFrame = overlap;
                return above;
            }
        }
        return static_cast<HWND>(nullptr);
    };

    TITLEBARINFOEX titleBar{};
    titleBar.cbSize = static_cast<DWORD>(sizeof(titleBar));
    if ( owner && owner->internalWinId() == nativeId && IsWindow(handle) )
    {
        (void)SendMessageW(handle, WM_GETTITLEBARINFOEX, 0,
            reinterpret_cast<LPARAM>(&titleBar));
    }
    const RECT maximizeRect = titleBar.rgrect[3];
    constexpr DWORD kUnavailableStates = STATE_SYSTEM_UNAVAILABLE
        | STATE_SYSTEM_INVISIBLE | STATE_SYSTEM_OFFSCREEN;
    RECT activeFrame{};
    const bool frameStable = GetWindowRect(handle, &activeFrame)
        && activeFrame.left == probeFrame.left && activeFrame.top == probeFrame.top
        && activeFrame.right == probeFrame.right && activeFrame.bottom == probeFrame.bottom
        && rectInside(workArea, activeFrame);
    HMENU const systemMenu = GetSystemMenu(handle, FALSE);
    const UINT maximizeMenuState = systemMenu
        ? GetMenuState(systemMenu, SC_MAXIMIZE, MF_BYCOMMAND)
        : std::numeric_limits<UINT>::max();
    const bool maximizeMenuEnabled = maximizeMenuState != std::numeric_limits<UINT>::max()
        && (maximizeMenuState & (MF_DISABLED | MF_GRAYED)) == 0;
    POINT maximizePoint{
        maximizeRect.left + (maximizeRect.right - maximizeRect.left) / 2,
        maximizeRect.top + (maximizeRect.bottom - maximizeRect.top) / 2};
    const bool maximizeRectValid = !IsRectEmpty(&maximizeRect)
        && maximizeRect.left >= probeFrame.left && maximizeRect.top >= probeFrame.top
        && maximizeRect.right <= probeFrame.right && maximizeRect.bottom <= probeFrame.bottom
        && (titleBar.rgstate[3] & kUnavailableStates) == 0;
    const auto nativeHitAt = [&](const POINT point)
    {
        return ownsLiveHandle()
            ? SendMessageW(handle, WM_NCHITTEST, 0,
                MAKELPARAM(static_cast<WORD>(point.x), static_cast<WORD>(point.y)))
            : static_cast<LRESULT>(HTNOWHERE);
    };
    const bool maximizePointResolved = maximizeRectValid
        && resolveMaximizeButtonPoint(maximizeRect, nativeHitAt, maximizePoint);
    const HWND foregroundBeforeClick = GetForegroundWindow();
    const DWORD foregroundThreadBeforeClick = foregroundBeforeClick
        ? GetWindowThreadProcessId(foregroundBeforeClick, nullptr)
        : 0;
    const DWORD targetThreadBeforeClick = GetWindowThreadProcessId(handle, nullptr);
    const bool qtActiveWindowReady = QApplication::activeWindow() == owner;
    const bool nativeInputClear = physicalInputIsClear();
    const bool mouseCaptureClear = GetCapture() == nullptr && !QWidget::mouseGrabber();
    const bool nativeForegroundReady = foregroundBeforeClick == handle;
    const HWND maximizePointHit = WindowFromPoint(maximizePoint);
    const bool pointOwned = maximizePointHit == handle
        || (maximizePointHit && GetAncestor(maximizePointHit, GA_ROOT) == handle);
    RECT coveringWindowFrame{};
    RECT coveringOverlapFrame{};
    const HWND coveringWindow = firstWindowCoveringDock(probeFrame,
        coveringWindowFrame, coveringOverlapFrame);
    const bool zOrderClear = coveringWindow == nullptr;
    const bool safeToClick = foregroundReady && nativeForegroundReady && qtActiveWindowReady
        && ownsLiveHandle() && owner->isVisible()
        && owner->isFloating() && !owner->isMaximized()
        && IsWindowVisible(handle) && !IsIconic(handle) && !IsZoomed(handle)
        && (GetAsyncKeyState(VK_LBUTTON) & 0x8000) == 0
        && nativeInputClear && mouseCaptureClear
        && frameStable && maximizeRectValid && maximizePointResolved && maximizeMenuEnabled
        && pointOwned && zOrderClear;

    const auto logCaptionHitMap = [&]()
    {
        RECT currentFrame{};
        if ( !ownsLiveHandle() || !GetWindowRect(handle, &currentFrame) )
        {
            std::fprintf(stderr, "native Max caption hit-map: dock=%s lost its native frame\n",
                dockObjectName.constData());
            return;
        }
        const QWindow* const windowHandle = owner->windowHandle();
        std::fprintf(stderr,
            "native Max caption hit-map: dock=%s target=%p owner=%p rootOwner=%p style=0x%llx exStyle=0x%llx widgetFlags=0x%llx windowFlags=0x%llx titleWidget=%p showWithoutActivating=%d frame=%ld,%ld,%ld,%ld chosen=%ld,%ld\n",
            dockObjectName.constData(), static_cast<void*>(handle),
            static_cast<void*>(GetWindow(handle, GW_OWNER)),
            static_cast<void*>(GetAncestor(handle, GA_ROOTOWNER)),
            static_cast<unsigned long long>(GetWindowLongPtrW(handle, GWL_STYLE)),
            static_cast<unsigned long long>(GetWindowLongPtrW(handle, GWL_EXSTYLE)),
            static_cast<unsigned long long>(owner->windowFlags().toInt()),
            windowHandle ? static_cast<unsigned long long>(windowHandle->flags().toInt()) : 0ULL,
            static_cast<void*>(owner->titleBarWidget()),
            owner->testAttribute(Qt::WA_ShowWithoutActivating), currentFrame.left,
            currentFrame.top, currentFrame.right, currentFrame.bottom,
            maximizePoint.x, maximizePoint.y);
        std::size_t maxHitCount = 0;
        POINT firstMaxHit{};
        const auto hitAt = [&](const POINT point)
        {
            const LRESULT hit = ownsLiveHandle()
                ? SendMessageW(handle, WM_NCHITTEST, 0,
                    MAKELPARAM(static_cast<WORD>(point.x), static_cast<WORD>(point.y)))
                : HTNOWHERE;
            if ( hit == HTMAXBUTTON )
            {
                if ( maxHitCount == 0u )
                {
                    firstMaxHit = point;
                }
                ++maxHitCount;
            }
            return hit;
        };
        for ( std::size_t index = 0;
              index < static_cast<std::size_t>(CCHILDREN_TITLEBAR + 1); ++index )
        {
            const RECT& rect = titleBar.rgrect[index];
            const POINT center{rect.left + (rect.right - rect.left) / 2,
                rect.top + (rect.bottom - rect.top) / 2};
            const bool valid = !IsRectEmpty(&rect) && rectInside(currentFrame, rect);
            const LRESULT hit = valid ? hitAt(center) : HTNOWHERE;
            std::fprintf(stderr,
                "native Max title rectangle: index=%zu state=0x%08lx rect=%ld,%ld,%ld,%ld valid=%d center=%ld,%ld hit=%lld\n",
                index, static_cast<unsigned long>(titleBar.rgstate[index]), rect.left,
                rect.top, rect.right, rect.bottom, valid, center.x, center.y,
                static_cast<long long>(hit));
        }
        RECT dwmButtons{};
        const HRESULT dwmResult = DwmGetWindowAttribute(handle, DWMWA_CAPTION_BUTTON_BOUNDS,
            &dwmButtons, static_cast<DWORD>(sizeof(dwmButtons)));
        RECT dwmScreen = dwmButtons;
        (void)OffsetRect(&dwmScreen, currentFrame.left, currentFrame.top);
        std::fprintf(stderr,
            "native Max DWM caption bounds: result=%ld relative=%ld,%ld,%ld,%ld screen=%ld,%ld,%ld,%ld titleBar=%ld,%ld,%ld,%ld\n",
            static_cast<long>(dwmResult), dwmButtons.left, dwmButtons.top,
            dwmButtons.right, dwmButtons.bottom, dwmScreen.left, dwmScreen.top,
            dwmScreen.right, dwmScreen.bottom, titleBar.rcTitleBar.left,
            titleBar.rcTitleBar.top, titleBar.rcTitleBar.right, titleBar.rcTitleBar.bottom);
        const auto sampleGrid = [&](const char* const label, const RECT& bounds,
                                    const int columns)
        {
            RECT clipped{};
            if ( IsRectEmpty(&bounds) || !IntersectRect(&clipped, &bounds, &currentFrame) )
            {
                std::fprintf(stderr, "native Max caption grid: %s has no valid in-frame bounds\n", label);
                return;
            }
            constexpr int kRows = 3;
            for ( int row = 0; row < kRows; ++row )
            {
                const LONG y = clipped.top
                    + (clipped.bottom - clipped.top - 1) * (2 * row + 1) / (2 * kRows);
                std::fprintf(stderr, "native Max caption grid: %s row=%d y=%ld hits=", label, row, y);
                for ( int column = 0; column < columns; ++column )
                {
                    const LONG x = clipped.left
                        + (clipped.right - clipped.left - 1) * (2 * column + 1) / (2 * columns);
                    std::fprintf(stderr, "%ld:%lld%s", x,
                        static_cast<long long>(hitAt(POINT{x, y})),
                        column + 1 == columns ? "" : ",");
                }
                std::fprintf(stderr, "\n");
            }
        };
        if ( SUCCEEDED(dwmResult) )
        {
            sampleGrid("DWM buttons", dwmScreen, 12);
        }
        sampleGrid("TITLEBARINFOEX Max", maximizeRect, 5);
        sampleGrid("TITLEBARINFOEX title", titleBar.rcTitleBar, 24);
        std::fprintf(stderr, "native Max caption hit-map summary: maxHits=%zu first=%ld,%ld\n",
            maxHitCount, firstMaxHit.x, firstMaxHit.y);
    };

    if (!pointOwned || !zOrderClear)
    {
        const auto logNativeWindow = [](const char* label, const HWND window)
        {
            DWORD process{};
            DWORD cloak{};
            const bool live = window && IsWindow(window);
            const DWORD thread = live ? GetWindowThreadProcessId(window, &process) : 0;
            const HRESULT cloakResult = live
                ? DwmGetWindowAttribute(window, DWMWA_CLOAKED, &cloak, sizeof(cloak))
                : E_HANDLE;
            std::fprintf(stderr,
                "native Max window diagnostic: %s=%p root=%p rootOwner=%p owner=%p pid=%lu thread=%lu visible=%d iconic=%d cloakHr=%ld cloak=%lu dpi=%u dpiContext=%p\n",
                label, static_cast<void*>(window),
                static_cast<void*>(live ? GetAncestor(window, GA_ROOT) : nullptr),
                static_cast<void*>(live ? GetAncestor(window, GA_ROOTOWNER) : nullptr),
                static_cast<void*>(live ? GetWindow(window, GW_OWNER) : nullptr),
                static_cast<unsigned long>(process), static_cast<unsigned long>(thread),
                live && IsWindowVisible(window), live && IsIconic(window),
                static_cast<long>(cloakResult), static_cast<unsigned long>(cloak),
                live ? GetDpiForWindow(window) : 0,
                static_cast<void*>(live ? GetWindowDpiAwarenessContext(window) : nullptr));
        };
        logNativeWindow("target", handle);
        logNativeWindow("hit", maximizePointHit);
        logNativeWindow("hitRoot", maximizePointHit ? GetAncestor(maximizePointHit, GA_ROOT) : nullptr);
        logNativeWindow("hitRootOwner", maximizePointHit ? GetAncestor(maximizePointHit, GA_ROOTOWNER) : nullptr);
        logNativeWindow("cover", coveringWindow);
        const LRESULT targetHit = ownsLiveHandle()
            ? SendMessageW(handle, WM_NCHITTEST, 0,
                MAKELPARAM(static_cast<WORD>(maximizePoint.x), static_cast<WORD>(maximizePoint.y)))
            : HTNOWHERE;
        std::fprintf(stderr,
            "native Max point diagnostic: point=%ld,%ld targetHit=%lld physicalHit=%p threadDpiContext=%p\n",
            maximizePoint.x, maximizePoint.y, static_cast<long long>(targetHit),
            static_cast<void*>(WindowFromPhysicalPoint(maximizePoint)),
            static_cast<void*>(GetThreadDpiAwarenessContext()));
    }

    UINT clickEventsInserted = 0;
    bool nativeMaximized = false;
    bool qtMaximized = false;
    bool restoreCommandSent = false;
    bool nativeRestored = false;
    bool qtRestored = false;
    if ( safeToClick )
    {
        const int virtualLeft = GetSystemMetrics(SM_XVIRTUALSCREEN);
        const int virtualTop = GetSystemMetrics(SM_YVIRTUALSCREEN);
        const int virtualWidth = GetSystemMetrics(SM_CXVIRTUALSCREEN);
        const int virtualHeight = GetSystemMetrics(SM_CYVIRTUALSCREEN);
        const bool pointInVirtualDesktop = virtualWidth > 0 && virtualHeight > 0
            && maximizePoint.x >= virtualLeft && maximizePoint.y >= virtualTop
            && maximizePoint.x < virtualLeft + virtualWidth
            && maximizePoint.y < virtualTop + virtualHeight;
        const auto normalizedCoordinate = [](const LONG position, const int origin, const int extent)
        {
            if ( extent <= 1 )
            {
                return LONG{};
            }
            const std::int64_t relative = static_cast<std::int64_t>(position) - origin;
            return static_cast<LONG>((relative * 65535 + (extent - 1) / 2) / (extent - 1));
        };
        const LONG inputX = normalizedCoordinate(maximizePoint.x, virtualLeft, virtualWidth);
        const LONG inputY = normalizedCoordinate(maximizePoint.y, virtualTop, virtualHeight);
        const DWORD moveFlags = MOUSEEVENTF_MOVE | MOUSEEVENTF_ABSOLUTE | MOUSEEVENTF_VIRTUALDESK;
        std::array<INPUT, 3> clickInputs{};
        for ( INPUT& input : clickInputs )
        {
            input.type = INPUT_MOUSE;
            input.mi.dx = inputX;
            input.mi.dy = inputY;
        }
        clickInputs[0].mi.dwFlags = moveFlags;
        clickInputs[1].mi.dwFlags = MOUSEEVENTF_LEFTDOWN;
        clickInputs[2].mi.dwFlags = MOUSEEVENTF_LEFTUP;
        const bool sameMaxHit = nativeHitAt(maximizePoint) == HTMAXBUTTON;
        const bool sameForeground = GetForegroundWindow() == handle;
        const bool samePointOwner = pointOwnedByDock(maximizePoint);
        if ( pointInVirtualDesktop && sameForeground && samePointOwner && sameMaxHit )
        {
            clickEventsInserted = SendInput(static_cast<UINT>(clickInputs.size()),
                clickInputs.data(), static_cast<int>(sizeof(INPUT)));
            if ( clickEventsInserted != static_cast<UINT>(clickInputs.size())
                && clickEventsInserted >= static_cast<UINT>(2) )
            {
                INPUT release{};
                release.type = INPUT_MOUSE;
                release.mi.dwFlags = MOUSEEVENTF_LEFTUP;
                (void)SendInput(1, &release, static_cast<int>(sizeof(INPUT)));
            }
            const auto maximizePublished = [&]()
            {
                return IsZoomed(handle) && owner && owner->isMaximized();
            };
            if ( clickEventsInserted >= static_cast<UINT>(2) )
            {
                (void)pumpUntil(maximizePublished, 1500);
            }
            nativeMaximized = ownsLiveHandle() && IsZoomed(handle);
            qtMaximized = owner && owner->isMaximized();
            if ( nativeMaximized )
            {
                (void)SendMessageW(handle, WM_SYSCOMMAND, SC_RESTORE, 0);
                restoreCommandSent = true;
                const auto restored = [&]()
                {
                    RECT restoredFrame{};
                    return !IsZoomed(handle) && owner && !owner->isMaximized()
                        && GetWindowRect(handle, &restoredFrame)
                        && sameRect(restoredFrame, probeFrame);
                };
                (void)pumpUntil(restored, 1500);
            }
            nativeRestored = ownsLiveHandle() && !IsZoomed(handle);
            qtRestored = owner && !owner->isMaximized();
        }
    }

    if ( !safeToClick || clickEventsInserted != static_cast<UINT>(3)
        || !nativeMaximized || !qtMaximized || !restoreCommandSent
        || !nativeRestored || !qtRestored )
    {
        logCaptionHitMap();
    }

    RECT frameAfterSystemRestore{};
    const bool probeFrameRestoredBySystem = restoreCommandSent
        && nativeRestored && qtRestored
        && ownsLiveHandle() && GetWindowRect(handle, &frameAfterSystemRestore)
        && sameRect(frameAfterSystemRestore, probeFrame);
    bool frameRestored = false;
    RECT finalFrame{};
    bool finalFrameAvailable = false;
    bool cleanupPlacementSetAttempted = false;
    bool cleanupPlacementSetSucceeded = false;
    bool cleanupPositionFallbackAttempted = false;
    bool cleanupPositionFallbackSucceeded = false;
    if ( ownsLiveHandle() )
    {
        if ( IsZoomed(handle) )
        {
            (void)SendMessageW(handle, WM_SYSCOMMAND, SC_RESTORE, 0);
            restoreCommandSent = true;
            (void)pumpUntil([&]()
                {
                    return !IsZoomed(handle) && owner && !owner->isMaximized();
                }, 1500);
        }
        if ( ownsLiveHandle() )
        {
            RECT currentFrame{};
            const bool exactFrame = GetWindowRect(handle, &currentFrame)
                && sameRect(currentFrame, initialFrame);
            if ( temporarilyMoved || !exactFrame )
            {
                cleanupPlacementSetAttempted = true;
                cleanupPlacementSetSucceeded = SetWindowPlacement(handle, &initialPlacement) != FALSE;
                if ( ownsLiveHandle() && !pumpUntil([&]()
                    {
                        RECT restoredFrame{};
                        return GetWindowRect(handle, &restoredFrame)
                            && sameRect(restoredFrame, initialFrame);
                    }, 1000) )
                {
                    if ( ownsLiveHandle() )
                    {
                        cleanupPositionFallbackAttempted = true;
                        cleanupPositionFallbackSucceeded = SetWindowPos(
                            handle, nullptr, initialFrame.left, initialFrame.top,
                            initialFrame.right - initialFrame.left, initialFrame.bottom - initialFrame.top,
                            SWP_NOZORDER | SWP_NOACTIVATE) != FALSE;
                    }
                    if ( ownsLiveHandle() )
                    {
                        (void)pumpUntil([&]()
                            {
                                RECT restoredFrame{};
                                return GetWindowRect(handle, &restoredFrame)
                                    && sameRect(restoredFrame, initialFrame);
                            }, 1000);
                    }
                }
            }
            if ( ownsLiveHandle() )
            {
                finalFrameAvailable = GetWindowRect(handle, &finalFrame) != FALSE;
                frameRestored = finalFrameAvailable && sameRect(finalFrame, initialFrame);
            }
        }
    }

    (void)SetCursorPos(initialCursor.x, initialCursor.y);
    POINT finalCursor{};
    const bool cursorRestored = GetCursorPos(&finalCursor)
        && finalCursor.x == initialCursor.x && finalCursor.y == initialCursor.y;
    if ( initialForeground && initialForeground != handle && IsWindow(initialForeground) )
    {
        (void)SetForegroundWindow(initialForeground);
        (void)pumpUntil([&]()
            {
                return GetForegroundWindow() == initialForeground;
            }, 1000);
    }
    const bool foregroundRestored = GetForegroundWindow() == initialForeground;
    WINDOWPLACEMENT finalPlacement{};
    finalPlacement.length = static_cast<UINT>(sizeof(finalPlacement));
    const bool finalPlacementAvailable = ownsLiveHandle()
        && GetWindowPlacement(handle, &finalPlacement) != FALSE;
    const bool normalPlacementRestored = finalPlacementAvailable
        && finalPlacement.rcNormalPosition.left == initialPlacement.rcNormalPosition.left
        && finalPlacement.rcNormalPosition.top == initialPlacement.rcNormalPosition.top
        && finalPlacement.rcNormalPosition.right == initialPlacement.rcNormalPosition.right
        && finalPlacement.rcNormalPosition.bottom == initialPlacement.rcNormalPosition.bottom;
    const bool placementRestored = ownsLiveHandle()
        && !owner->isMaximized() && !IsZoomed(handle) && owner->isFloating()
        && normalPlacementRestored;
    const QRect finalDockGeometry = owner ? owner->geometry() : QRect{};
    const QRect finalDockFrameGeometry = owner ? owner->frameGeometry() : QRect{};
    const qreal finalDockDpr = owner ? owner->devicePixelRatioF() : 0.0;
    const bool result = safeToClick && clickEventsInserted == static_cast<UINT>(3)
        && nativeMaximized && qtMaximized && restoreCommandSent
        && nativeRestored && qtRestored && probeFrameRestoredBySystem
        && frameRestored && placementRestored
        && cursorRestored && foregroundRestored;
    if ( !result )
    {
        std::fprintf(stderr,
            "native Max click round-trip failed: style=%llu titleState=%lu rect=%ld,%ld,%ld,%ld safe=%d foregroundReady=%d nativeForeground=%d qtActive=%d pointOwned=%d zOrderClear=%d inputClear=%d actualForeground=%p foregroundThread=%lu targetThread=%lu frameStable=%d menuState=%u sent=%u nativeMax=%d qtMax=%d restore=%d nativeNormal=%d qtNormal=%d restoreFrame=%d frame=%d placement=%d cursor=%d focus=%d\n",
            static_cast<unsigned long long>(style),
            static_cast<unsigned long>(titleBar.rgstate[3]), maximizeRect.left,
            maximizeRect.top, maximizeRect.right, maximizeRect.bottom, safeToClick,
            foregroundReady, nativeForegroundReady, qtActiveWindowReady, pointOwned,
            zOrderClear, nativeInputClear, static_cast<void*>(foregroundBeforeClick),
            static_cast<unsigned long>(foregroundThreadBeforeClick),
            static_cast<unsigned long>(targetThreadBeforeClick), frameStable,
            maximizeMenuState, clickEventsInserted, nativeMaximized, qtMaximized,
            restoreCommandSent, nativeRestored, qtRestored, probeFrameRestoredBySystem, frameRestored,
            placementRestored, cursorRestored, foregroundRestored);
        if ( !pointOwned || !zOrderClear )
        {
            const auto classNameFor = [](const HWND window)
            {
                std::array<wchar_t, 256> text{};
                const int length = window && IsWindow(window)
                    ? GetClassNameW(window, text.data(), static_cast<int>(text.size()))
                    : 0;
                return QString::fromWCharArray(text.data(), std::max(0, length)).toUtf8();
            };
            const auto titleFor = [](const HWND window)
            {
                std::array<wchar_t, 512> text{};
                const int length = window && IsWindow(window)
                    ? GetWindowTextW(window, text.data(), static_cast<int>(text.size()))
                    : 0;
                return QString::fromWCharArray(text.data(), std::max(0, length)).toUtf8();
            };
            const auto processIdFor = [](const HWND window)
            {
                DWORD id = 0;
                if ( window && IsWindow(window) )
                {
                    (void)GetWindowThreadProcessId(window, &id);
                }
                return id;
            };
            const QByteArray targetClass = classNameFor(handle);
            const QByteArray targetTitle = titleFor(handle);
            const QByteArray hitClass = classNameFor(maximizePointHit);
            const QByteArray hitTitle = titleFor(maximizePointHit);
            const QByteArray coveringClass = classNameFor(coveringWindow);
            const QByteArray coveringTitle = titleFor(coveringWindow);
            std::fprintf(stderr,
                "native Max click safety obstruction: target=%p class=%s title=%s pid=%lu frame=%ld,%ld,%ld,%ld point=%ld,%ld hit=%p class=%s title=%s pid=%lu covering=%p class=%s title=%s pid=%lu frame=%ld,%ld,%ld,%ld overlap=%ld,%ld,%ld,%ld pointOwned=%d zOrderClear=%d\n",
                static_cast<void*>(handle), targetClass.constData(), targetTitle.constData(),
                static_cast<unsigned long>(processId), probeFrame.left, probeFrame.top,
                probeFrame.right, probeFrame.bottom, maximizePoint.x, maximizePoint.y,
                static_cast<void*>(maximizePointHit), hitClass.constData(), hitTitle.constData(),
                static_cast<unsigned long>(processIdFor(maximizePointHit)),
                static_cast<void*>(coveringWindow), coveringClass.constData(),
                coveringTitle.constData(),
                static_cast<unsigned long>(processIdFor(coveringWindow)),
                coveringWindowFrame.left, coveringWindowFrame.top,
                coveringWindowFrame.right, coveringWindowFrame.bottom,
                coveringOverlapFrame.left, coveringOverlapFrame.top,
                coveringOverlapFrame.right, coveringOverlapFrame.bottom,
                pointOwned, zOrderClear);
        }
        if ( !frameRestored || !placementRestored )
        {
            std::fprintf(stderr,
                "native Max cleanup rectangles: dock=%s temporarilyMoved=%d placementSet=%d/%d fallbackSetWindowPos=%d/%d workArea=%ld,%ld,%ld,%ld initial=%ld,%ld,%ld,%ld probe=%ld,%ld,%ld,%ld systemRestoreAvailable=%d systemRestore=%ld,%ld,%ld,%ld finalAvailable=%d final=%ld,%ld,%ld,%ld\n",
                dockObjectName.constData(), temporarilyMoved, cleanupPlacementSetAttempted,
                cleanupPlacementSetSucceeded, cleanupPositionFallbackAttempted,
                cleanupPositionFallbackSucceeded,
                workArea.left, workArea.top, workArea.right, workArea.bottom,
                initialFrame.left, initialFrame.top, initialFrame.right, initialFrame.bottom,
                probeFrame.left, probeFrame.top, probeFrame.right, probeFrame.bottom,
                probeFrameRestoredBySystem, frameAfterSystemRestore.left, frameAfterSystemRestore.top,
                frameAfterSystemRestore.right, frameAfterSystemRestore.bottom,
                finalFrameAvailable, finalFrame.left, finalFrame.top, finalFrame.right, finalFrame.bottom);
            std::fprintf(stderr,
                "native Max cleanup placements: initial={flags:%u,show:%u,min:%ld,%ld,max:%ld,%ld,normal:%ld,%ld,%ld,%ld} probeAvailable=%d probe={flags:%u,show:%u,min:%ld,%ld,max:%ld,%ld,normal:%ld,%ld,%ld,%ld} finalAvailable=%d final={flags:%u,show:%u,min:%ld,%ld,max:%ld,%ld,normal:%ld,%ld,%ld,%ld}\n",
                static_cast<unsigned>(initialPlacement.flags), static_cast<unsigned>(initialPlacement.showCmd),
                initialPlacement.ptMinPosition.x, initialPlacement.ptMinPosition.y,
                initialPlacement.ptMaxPosition.x, initialPlacement.ptMaxPosition.y,
                initialPlacement.rcNormalPosition.left, initialPlacement.rcNormalPosition.top,
                initialPlacement.rcNormalPosition.right, initialPlacement.rcNormalPosition.bottom,
                probePlacementAvailable, static_cast<unsigned>(probePlacement.flags),
                static_cast<unsigned>(probePlacement.showCmd), probePlacement.ptMinPosition.x,
                probePlacement.ptMinPosition.y, probePlacement.ptMaxPosition.x, probePlacement.ptMaxPosition.y,
                probePlacement.rcNormalPosition.left, probePlacement.rcNormalPosition.top,
                probePlacement.rcNormalPosition.right, probePlacement.rcNormalPosition.bottom,
                finalPlacementAvailable, static_cast<unsigned>(finalPlacement.flags),
                static_cast<unsigned>(finalPlacement.showCmd), finalPlacement.ptMinPosition.x,
                finalPlacement.ptMinPosition.y, finalPlacement.ptMaxPosition.x, finalPlacement.ptMaxPosition.y,
                finalPlacement.rcNormalPosition.left, finalPlacement.rcNormalPosition.top,
                finalPlacement.rcNormalPosition.right, finalPlacement.rcNormalPosition.bottom);
            std::fprintf(stderr,
                "native Max cleanup Qt geometry: initialClient=%d,%d,%d,%d initialFrame=%d,%d,%d,%d initialDpr=%.3f probeClient=%d,%d,%d,%d probeFrame=%d,%d,%d,%d probeDpr=%.3f finalClient=%d,%d,%d,%d finalFrame=%d,%d,%d,%d finalDpr=%.3f\n",
                initialDockGeometry.x(), initialDockGeometry.y(), initialDockGeometry.width(),
                initialDockGeometry.height(), initialDockFrameGeometry.x(), initialDockFrameGeometry.y(),
                initialDockFrameGeometry.width(), initialDockFrameGeometry.height(),
                static_cast<double>(initialDockDpr), probeDockGeometry.x(), probeDockGeometry.y(),
                probeDockGeometry.width(), probeDockGeometry.height(), probeDockFrameGeometry.x(),
                probeDockFrameGeometry.y(), probeDockFrameGeometry.width(), probeDockFrameGeometry.height(),
                static_cast<double>(probeDockDpr), finalDockGeometry.x(), finalDockGeometry.y(),
                finalDockGeometry.width(), finalDockGeometry.height(), finalDockFrameGeometry.x(),
                finalDockFrameGeometry.y(), finalDockFrameGeometry.width(), finalDockFrameGeometry.height(),
                static_cast<double>(finalDockDpr));
        }
    }
    return result;
#else
    return dock && dock->windowFlags().testFlag(Qt::WindowMaximizeButtonHint);
#endif
}

} // namespace sed::app_tests::native_caption_test
