#pragma once

#include <functional>
#include <string>
#include <vector>

#include <windows.h>

namespace apm {

// Data displayed by the overlay each frame.
struct OverlayState {
    bool tracking = false;
    std::wstring apm;
    std::wstring averageApm;
    std::wstring peakApm;
    std::wstring eapm;
    std::wstring ping;
    bool pingIsFallback = false;
};

// Always-on-top overlay panel styled after the Smite 2 UI (dark panel,
// gold hairlines and accents, uppercase gold labels).
//
// The panel shows live stats and has Start / Stop buttons. It is a layered
// (per-pixel alpha) window; buttons are clickable and the rest of the panel
// can be dragged to reposition it.
//
// All methods must be called from the thread that created the window (the
// thread running the message loop).
class OverlayWindow {
public:
    using ButtonCallback = std::function<void()>;

    OverlayWindow() = default;
    ~OverlayWindow();

    OverlayWindow(const OverlayWindow&) = delete;
    OverlayWindow& operator=(const OverlayWindow&) = delete;

    // Creates and shows the overlay at (x, y). `fontSize` scales the whole
    // panel. Returns false on failure.
    bool create(HINSTANCE instance, int x, int y, int fontSize,
                ButtonCallback onStart, ButtonCallback onStop);

    // Updates the displayed values and repaints if anything changed.
    void setState(const OverlayState& state);

    void destroy();

    HWND handle() const { return hwnd_; }

private:
    struct Button {
        RECT rect{};
        std::wstring label;
        bool enabled = false;
        bool hovered = false;
        ButtonCallback onClick;
    };

    static LRESULT CALLBACK wndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
    LRESULT handleMessage(UINT msg, WPARAM wParam, LPARAM lParam);
    void render();
    void layoutButtons();
    // Index of the button at client point (x, y), or -1.
    int buttonAt(int x, int y) const;

    HWND hwnd_ = nullptr;
    HFONT titleFont_ = nullptr;
    HFONT labelFont_ = nullptr;
    HFONT valueFont_ = nullptr;
    HFONT buttonFont_ = nullptr;
    int width_ = 0;
    int height_ = 0;
    int scale_ = 16;  // base font size everything is derived from
    OverlayState state_;
    std::vector<Button> buttons_;
    bool trackingMouseLeave_ = false;
};

}  // namespace apm
