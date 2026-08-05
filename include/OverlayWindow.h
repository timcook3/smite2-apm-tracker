#pragma once

#include <string>

#include <windows.h>

namespace apm {

// Transparent, always-on-top, click-through overlay window that displays
// two lines of text (APM and ping). Rendered with GDI via UpdateLayeredWindow
// so it stays visible over borderless-windowed games.
//
// All methods must be called from the thread that created the window (the
// thread running the message loop).
class OverlayWindow {
public:
    OverlayWindow() = default;
    ~OverlayWindow();

    OverlayWindow(const OverlayWindow&) = delete;
    OverlayWindow& operator=(const OverlayWindow&) = delete;

    // Creates and shows the overlay at (x, y). Returns false on failure.
    bool create(HINSTANCE instance, int x, int y, int fontSize);

    // Updates the displayed text and repaints.
    void setText(const std::wstring& apmLine, const std::wstring& pingLine);

    void destroy();

    HWND handle() const { return hwnd_; }

private:
    void render();

    HWND hwnd_ = nullptr;
    HFONT font_ = nullptr;
    int width_ = 0;
    int height_ = 0;
    std::wstring apmLine_;
    std::wstring pingLine_;
};

}  // namespace apm
