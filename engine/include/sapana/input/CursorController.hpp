#pragma once

namespace sapana
{
namespace input
{

enum class CursorMode
{
    Captured, // hidden cursor; pointer recentered each frame for free-look
    Free      // visible cursor for UI
};

/// Platform cursor hide / warp. Linux uses Xlib without XGrabPointer (grab would
/// steal motion events from Diligent's XCB input path).
class CursorController
{
public:
    CursorController();
    ~CursorController();

    CursorController(const CursorController&)            = delete;
    CursorController& operator=(const CursorController&) = delete;

    /// Open display connection (Linux). Safe to call once after window exists.
    bool Initialize();

    void SetMode(CursorMode mode);
    void Toggle();

    CursorMode GetMode() const { return m_Mode; }
    bool       IsCaptured() const { return m_Mode == CursorMode::Captured; }

    /// Warp the OS pointer to window-client coordinates (same space as Diligent PosX/PosY).
    /// Returns false if the focused window is unavailable.
    bool WarpPointer(int x, int y);

private:
    void ApplyMode();
    void Capture();
    void Release();
    bool RefreshFocusedWindow();

    CursorMode    m_Mode         = CursorMode::Free;
    bool          m_Initialized  = false;
    void*         m_Display      = nullptr; // Display* on Linux
    void*         m_BlankCursor  = nullptr; // Cursor on Linux
    unsigned long m_Window       = 0;
};

} // namespace input
} // namespace sapana
