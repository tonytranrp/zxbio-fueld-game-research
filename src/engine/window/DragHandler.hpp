#pragma once

#ifdef _WIN32
// ------------------------------------------------------------------------------
// DragHandler — Prevents Windows modal drag loop from freezing the game.
//
// When the user drags the title bar, Windows normally enters a modal message
// loop inside DefWindowProc (WM_SYSCOMMAND / SC_MOVE) that blocks the entire
// process — shaders freeze, audio stutters, everything stops.
//
// This module subclasses the window to intercept SC_MOVE and handles the drag
// manually via mouse capture + SetWindowPos, keeping the game loop alive.
//
// Usage:
//   DragHandler::install(GetWindowHandle());   // after InitWindow()
//   DragHandler::flush();                      // once per frame in update()
//   DragHandler::uninstall();                  // before CloseWindow()
// ------------------------------------------------------------------------------

// Forward-declare Win32 API to avoid <windows.h> (conflicts with raylib).
extern "C" {
    __declspec(dllimport) long long __stdcall SetWindowLongPtrW(void* hWnd, int nIndex, long long dwNewLong);
    __declspec(dllimport) long long __stdcall GetWindowLongPtrW(void* hWnd, int nIndex);
    __declspec(dllimport) int __stdcall SetWindowPos(void* hWnd, void* hWndInsertAfter, int X, int Y, int cx, int cy, unsigned int uFlags);
    __declspec(dllimport) int __stdcall GetCursorPos(void* lpPoint);
    __declspec(dllimport) void* __stdcall SetCapture(void* hWnd);
    __declspec(dllimport) int __stdcall ReleaseCapture();
    __declspec(dllimport) long long __stdcall CallWindowProcW(long long lpPrevWndFunc, void* hWnd, unsigned int Msg, unsigned long long wParam, long long lParam);
    __declspec(dllimport) long long __stdcall GetWindowRect(void* hWnd, void* lpRect);
}

namespace biofuel::engine::window {

class DragHandler final {
public:
    DragHandler() = delete;

    static void install(void* hwnd) noexcept;
    static void uninstall() noexcept;
    static void flush() noexcept;

private:
    struct Point { long x; long y; };
    struct Rect { long left; long top; long right; long bottom; };

    static long long __stdcall wndProc(void* hWnd, unsigned int msg, unsigned long long wParam, long long lParam);

    inline static constexpr int GWLP_WNDPROC    = -4;
    inline static constexpr unsigned int WM_SYSCOMMAND = 0x0112;
    inline static constexpr unsigned int WM_MOUSEMOVE  = 0x0200;
    inline static constexpr unsigned int WM_LBUTTONUP  = 0x0202;
    inline static constexpr unsigned int SC_MOVE       = 0xF010;
    inline static constexpr unsigned int SC_SIZE       = 0xF000;
    inline static constexpr unsigned int SWP_NOZORDER  = 0x0004;
    inline static constexpr unsigned int SWP_NOSIZE    = 0x0001;
    inline static constexpr unsigned int SWP_NOACTIVATE = 0x0010;

    inline static long long s_originalWndProc = 0;
    inline static void*    s_hwnd = nullptr;
    inline static bool     s_dragging = false;
    inline static int      s_startX = 0;
    inline static int      s_startY = 0;
    inline static int      s_lastX = 0;
    inline static int      s_lastY = 0;
    inline static bool     s_pending = false;
};

// ==============================================================================
// Inline implementations
// ==============================================================================

inline void DragHandler::install(void* hwnd) noexcept {
    s_hwnd = hwnd;
    if (!s_hwnd) return;
    s_originalWndProc = GetWindowLongPtrW(s_hwnd, GWLP_WNDPROC);
    SetWindowLongPtrW(s_hwnd, GWLP_WNDPROC, reinterpret_cast<long long>(&wndProc));
}

inline void DragHandler::uninstall() noexcept {
    if (s_hwnd && s_originalWndProc) {
        SetWindowLongPtrW(s_hwnd, GWLP_WNDPROC, s_originalWndProc);
        s_originalWndProc = 0;
    }
    s_hwnd = nullptr;
    s_dragging = false;
    s_pending = false;
}

inline void DragHandler::flush() noexcept {
    if (!s_pending || !s_hwnd) return;
    s_pending = false;
    SetWindowPos(s_hwnd, nullptr, s_lastX, s_lastY, 0, 0,
                 SWP_NOZORDER | SWP_NOSIZE | SWP_NOACTIVATE);
}

inline long long __stdcall DragHandler::wndProc(void* hWnd, unsigned int msg, unsigned long long wParam, long long lParam) {
    if (msg == WM_SYSCOMMAND) {
        const unsigned int cmd = wParam & 0xFFF0;
        if (cmd == SC_MOVE) {
            Point cursor;
            GetCursorPos(&cursor);
            Rect wr;
            GetWindowRect(hWnd, &wr);
            s_startX = cursor.x - wr.left;
            s_startY = cursor.y - wr.top;
            s_dragging = true;
            SetCapture(hWnd);
            return 0;
        }
        if (cmd == SC_SIZE) {
            // Resize goes through DefWindowProc — OpenGL needs proper resize path.
            return CallWindowProcW(s_originalWndProc, hWnd, msg, wParam, lParam);
        }
    }

    if (s_dragging) {
        if (msg == WM_MOUSEMOVE) {
            Point cursor;
            GetCursorPos(&cursor);
            s_lastX = cursor.x - s_startX;
            s_lastY = cursor.y - s_startY;
            s_pending = true;
            return 0;
        }
        if (msg == WM_LBUTTONUP) {
            s_dragging = false;
            s_pending = false;
            ReleaseCapture();
            return 0;
        }
    }

    return CallWindowProcW(s_originalWndProc, hWnd, msg, wParam, lParam);
}

} // namespace biofuel::engine::window
#endif // _WIN32
