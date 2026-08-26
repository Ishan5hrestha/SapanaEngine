#include "sapana/input/CursorController.hpp"

#include <cstdint>
#include <iostream>

#if PLATFORM_LINUX

// X11 headers define Bool/True/False macros that clash with Diligent.
#    include <X11/Xlib.h>
#    include <X11/Xutil.h>

#    ifdef Bool
#        undef Bool
#    endif
#    ifdef True
#        undef True
#    endif
#    ifdef False
#        undef False
#    endif
#    ifdef Status
#        undef Status
#    endif

#endif // PLATFORM_LINUX

namespace sapana
{
namespace input
{

CursorController::CursorController() = default;

CursorController::~CursorController()
{
    if (m_Initialized && m_Mode == CursorMode::Captured)
        Release();

#if PLATFORM_LINUX
    auto* display = static_cast<Display*>(m_Display);
    if (display != nullptr)
    {
        if (m_BlankCursor != nullptr)
        {
            XFreeCursor(display, static_cast<Cursor>(reinterpret_cast<std::uintptr_t>(m_BlankCursor)));
            m_BlankCursor = nullptr;
        }
        XCloseDisplay(display);
        m_Display = nullptr;
    }
#endif
}

bool CursorController::Initialize()
{
#if PLATFORM_LINUX
    if (m_Initialized)
        return true;

    Display* display = XOpenDisplay(nullptr);
    if (display == nullptr)
    {
        std::cerr << "Sapana CursorController: XOpenDisplay failed\n";
        return false;
    }
    m_Display = display;

    // 1x1 blank cursor
    const char data[1] = {0};
    Pixmap     pixmap  = XCreateBitmapFromData(display, DefaultRootWindow(display), data, 1, 1);
    XColor     black{};
    Cursor     blank = XCreatePixmapCursor(display, pixmap, pixmap, &black, &black, 0, 0);
    XFreePixmap(display, pixmap);
    m_BlankCursor = reinterpret_cast<void*>(static_cast<std::uintptr_t>(blank));

    m_Initialized = true;
    return true;
#else
    m_Initialized = true;
    return true;
#endif
}

void CursorController::SetMode(CursorMode mode)
{
    if (m_Mode == mode && m_Initialized)
    {
        if (mode == CursorMode::Captured)
            ApplyMode();
        return;
    }
    m_Mode = mode;
    ApplyMode();
}

void CursorController::Toggle()
{
    SetMode(m_Mode == CursorMode::Captured ? CursorMode::Free : CursorMode::Captured);
}

void CursorController::ApplyMode()
{
    if (!m_Initialized)
        return;

    if (m_Mode == CursorMode::Captured)
        Capture();
    else
        Release();
}

bool CursorController::RefreshFocusedWindow()
{
#if PLATFORM_LINUX
    auto* display = static_cast<Display*>(m_Display);
    if (display == nullptr)
        return false;

    Window focused = 0;
    int    revert  = 0;
    XGetInputFocus(display, &focused, &revert);
    if (focused == 0 || focused == PointerRoot || focused == None)
        return false;

    m_Window = focused;
    return true;
#else
    return false;
#endif
}

void CursorController::Capture()
{
#if PLATFORM_LINUX
    auto* display = static_cast<Display*>(m_Display);
    if (display == nullptr)
        return;

    if (!RefreshFocusedWindow())
    {
        std::cerr << "Sapana CursorController: no focused window to hide cursor on\n";
        return;
    }

    // Hide cursor only — no XGrabPointer (steals Diligent XCB motion events).
    const Cursor blank = static_cast<Cursor>(reinterpret_cast<std::uintptr_t>(m_BlankCursor));
    XDefineCursor(display, static_cast<Window>(m_Window), blank);
    XFlush(display);
#endif
}

void CursorController::Release()
{
#if PLATFORM_LINUX
    auto* display = static_cast<Display*>(m_Display);
    if (display == nullptr)
        return;

    if (m_Window != 0)
    {
        XUndefineCursor(display, static_cast<Window>(m_Window));
    }
    XFlush(display);
#endif
}

bool CursorController::WarpPointer(int x, int y)
{
#if PLATFORM_LINUX
    auto* display = static_cast<Display*>(m_Display);
    if (display == nullptr)
        return false;

    if (m_Window == 0 && !RefreshFocusedWindow())
        return false;

    // dest_window = our window; coordinates are client-relative (matches Diligent PosX/PosY).
    XWarpPointer(display, None, static_cast<Window>(m_Window), 0, 0, 0, 0, x, y);
    XFlush(display);
    return true;
#else
    (void)x;
    (void)y;
    return false;
#endif
}

} // namespace input
} // namespace sapana
