#include "test_common.hpp"
#include "native_caption_test_utils.hpp"

#include <QScopeGuard>

#ifdef Q_OS_WIN
#  include <dwmapi.h>
#endif

namespace sed::app_tests
{

void runNativeCaptionVisibilityTests()
{
#ifdef Q_OS_WIN
    const RECT reportedMax{1847, 42, 1917, 86};
    const POINT reportedCenter{1882, 64};
    const auto inside = [](const RECT& rect, const POINT point)
    {
        return point.x >= rect.left && point.x < rect.right
            && point.y >= rect.top && point.y < rect.bottom;
    };
    POINT resolved{-1, -1};
    int queries = 0;
    check(native_caption_test::resolveMaximizeButtonPoint(reportedMax,
              [&](const POINT point)
              {
                  ++queries;
                  return point.x == reportedCenter.x && point.y == reportedCenter.y
                      ? HTMAXBUTTON : HTCAPTION;
              }, resolved)
            && queries == 1 && resolved.x == reportedCenter.x && resolved.y == reportedCenter.y,
        "native Max resolver retains an already correct reported center without searching");

    // Recorded native bounds overlap Minimize and the caption even though the
    // reported Max control is enabled. Its actual Max hit region starts farther
    // right and below the top resize strip.
    const auto actualCaptionHit = [](const POINT point)
    {
        if ( point.y < 56 )
        {
            return HTTOP;
        }
        if ( point.x >= 1886 && point.x < 1937 )
        {
            return HTMAXBUTTON;
        }
        if ( point.x < 1881 )
        {
            return HTMINBUTTON;
        }
        return HTCAPTION;
    };
    queries = 0;
    bool allQueriesInside = true;
    resolved = POINT{-1, -1};
    check(actualCaptionHit(reportedCenter) == HTCAPTION
            && native_caption_test::resolveMaximizeButtonPoint(reportedMax,
                [&](const POINT point)
                {
                    ++queries;
                    allQueriesInside = allQueriesInside && inside(reportedMax, point);
                    return actualCaptionHit(point);
                }, resolved)
            && inside(reportedMax, resolved) && actualCaptionHit(resolved) == HTMAXBUTTON
            && allQueriesInside && queries > 1 && queries <= 16,
        "native Max resolver replaces an inaccurate caption center with a bounded exact Max hit");

    queries = 0;
    allQueriesInside = true;
    resolved = POINT{-1, -1};
    check(!native_caption_test::resolveMaximizeButtonPoint(reportedMax,
              [&](const POINT point)
              {
                  ++queries;
                  allQueriesInside = allQueriesInside && inside(reportedMax, point);
                  return point.x < reportedCenter.x ? HTMINBUTTON
                      : point.x > reportedCenter.x ? HTCLOSE : HTCAPTION;
              }, resolved)
            && allQueriesInside && queries == 16 && resolved.x == -1 && resolved.y == -1,
        "native Max resolver rejects caption, Minimize, and Close hits without expanding its bounds");

    for ( const RECT invalid : std::array<RECT, 3>{
            RECT{10, 20, 10, 30}, RECT{20, 20, 10, 30}, RECT{10, 30, 20, 20}} )
    {
        queries = 0;
        resolved = POINT{-1, -1};
        check(!native_caption_test::resolveMaximizeButtonPoint(invalid,
                  [&](const POINT)
                  {
                      ++queries;
                      return HTMAXBUTTON;
                  }, resolved)
                && queries == 0 && resolved.x == -1 && resolved.y == -1,
            "native Max resolver rejects empty or inverted rectangles without a native query");
    }

    const RECT negativeMax{-1917, -86, -1847, -42};
    resolved = POINT{1, 1};
    check(native_caption_test::resolveMaximizeButtonPoint(negativeMax,
              [&](const POINT point)
              {
                  return inside(negativeMax, point) ? HTMAXBUTTON : HTNOWHERE;
              }, resolved)
            && resolved.x == -1882 && resolved.y == -64,
        "native Max resolver preserves negative monitor coordinates");

    const HWND window = CreateWindowExW(
        WS_EX_NOACTIVATE | WS_EX_TOOLWINDOW,
        L"STATIC", L"SED caption visibility fixture",
        WS_POPUP | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX | WS_VISIBLE,
        0, 0, 96, 64, nullptr, nullptr, GetModuleHandleW(nullptr), nullptr);
    check(window != nullptr, "native caption visibility fixture creates its own nonactivating window");
    if (!window)
    {
        return;
    }
    const auto cleanup = qScopeGuard([window]() { (void)DestroyWindow(window); });
    check(native_caption_test::windowMayObstructDock(window),
        "a real visible native window retains the dock overlap veto");

    const BOOL cloak = TRUE;
    const HRESULT cloakResult = DwmSetWindowAttribute(
        window, DWMWA_CLOAK, &cloak, sizeof(cloak));
    (void)DwmFlush();
    DWORD cloakState{};
    const HRESULT readCloakResult = DwmGetWindowAttribute(
        window, DWMWA_CLOAKED, &cloakState, sizeof(cloakState));
    const bool cloakedFixture = SUCCEEDED(cloakResult) && SUCCEEDED(readCloakResult)
        && cloakState != 0 && IsWindowVisible(window) && !IsIconic(window);
    if (!cloakedFixture)
    {
        std::fprintf(stderr,
            "Native cloak fixture diagnostic: set=%ld get=%ld cloak=%lu visible=%d iconic=%d\n",
            static_cast<long>(cloakResult), static_cast<long>(readCloakResult),
            static_cast<unsigned long>(cloakState), IsWindowVisible(window), IsIconic(window));
    }
    check(cloakedFixture,
        "native cloak fixture remains visible and nonminimized while DWM suppresses its presentation");
    if (!cloakedFixture)
    {
        return;
    }
    check(!native_caption_test::windowMayObstructDock(window),
        "a confirmed cloaked native window cannot veto the dock overlap check");

    const BOOL uncloak = FALSE;
    const HRESULT uncloakResult = DwmSetWindowAttribute(
        window, DWMWA_CLOAK, &uncloak, sizeof(uncloak));
    (void)DwmFlush();
    cloakState = 0;
    const HRESULT readUncloakResult = DwmGetWindowAttribute(
        window, DWMWA_CLOAKED, &cloakState, sizeof(cloakState));
    check(SUCCEEDED(uncloakResult) && SUCCEEDED(readUncloakResult) && cloakState == 0
            && native_caption_test::windowMayObstructDock(window),
        "uncloaking the same visible native window restores its overlap veto");

    (void)ShowWindow(window, SW_HIDE);
    check(!IsWindowVisible(window) && !native_caption_test::windowMayObstructDock(window),
        "a hidden native window cannot veto the dock overlap check");
    (void)ShowWindow(window, SW_SHOWNOACTIVATE);
    (void)ShowWindow(window, SW_MINIMIZE);
    check(IsIconic(window) && !native_caption_test::windowMayObstructDock(window),
        "a minimized native window cannot veto the dock overlap check");
    check(!native_caption_test::windowMayObstructDock(nullptr),
        "a missing native window cannot veto the dock overlap check");
#endif
}

} // namespace sed::app_tests
