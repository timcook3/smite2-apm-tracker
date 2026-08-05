#pragma once

#include <functional>
#include <string>
#include <vector>

#include <windows.h>
#include <shellapi.h>

namespace apm {

// Data displayed by the overlay each frame.
struct OverlayState {
    bool tracking = false;
    bool gameFocused = true;  // false shows "NO GAME" while tracking
    std::wstring sessionTime;
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
// The panel shows live stats and has Start / Stop / Heatmap buttons, plus
// minimize / maximize / close caption buttons. Minimize hides the panel to
// the system tray (click the tray icon to restore, right-click for a menu).
// The panel can be dragged anywhere and resized from its edges/corners
// (the whole layout scales, keeping the panel's aspect ratio). Closing the
// panel quits the application.
//
// All methods must be called from the thread that created the window (the
// thread running the message loop).
class OverlayWindow {
public:
    using ButtonCallback = std::function<void()>;

    // Panel proportions, in units of scale_.
    static constexpr int kWidthUnits = 15;
    static constexpr int kHeightUnits = 18;
    static constexpr int kMinScale = 10;

    OverlayWindow() = default;
    ~OverlayWindow();

    OverlayWindow(const OverlayWindow&) = delete;
    OverlayWindow& operator=(const OverlayWindow&) = delete;

    // Creates and shows the overlay at (x, y). `fontSize` scales the whole
    // panel. Returns false on failure.
    bool create(HINSTANCE instance, int x, int y, int fontSize,
                ButtonCallback onStart, ButtonCallback onStop,
                ButtonCallback onHeatmap);

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
    // Rebuilds fonts and layout after scale_ changed.
    bool applyScale(int scale);
    // Resize hit-test code (HTLEFT..HTBOTTOMRIGHT) for a client point, or
    // 0 if the point is not on a resize edge.
    LRESULT resizeHitTest(int x, int y) const;

    void minimizeToTray();
    void restoreFromTray();
    void toggleMaximize();
    void showTrayMenu();

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

    NOTIFYICONDATAW trayIcon_{};
    bool inTray_ = false;
    bool maximized_ = false;
    int restoreScale_ = 16;   // scale before maximizing
    POINT restorePos_{};      // position before maximizing
};

}  // namespace apm
