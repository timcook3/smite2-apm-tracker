#pragma once

#include <windows.h>

#include "ApmCalculator.h"

namespace apm {

// Keyboard heatmap window: draws a keyboard layout where each key is
// colored by how often it was pressed this session, from white (least
// pressed) through yellow and orange to red (most pressed). Unpressed keys
// stay dark. Toggled from the overlay's HEATMAP button; draggable.
//
// All methods must be called from the thread that created the window.
class HeatmapWindow {
public:
    HeatmapWindow() = default;
    ~HeatmapWindow();

    HeatmapWindow(const HeatmapWindow&) = delete;
    HeatmapWindow& operator=(const HeatmapWindow&) = delete;

    // Creates the window (hidden) at (x, y). `fontSize` scales the layout.
    // Returns false on failure.
    bool create(HINSTANCE instance, int x, int y, int fontSize);

    // Shows or hides the window.
    void toggle();
    bool visible() const { return visible_; }

    // Updates the per-key counts and repaints if visible.
    void setCounts(const KeyCounts& counts);

    void destroy();

private:
    static LRESULT CALLBACK wndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
    LRESULT handleMessage(UINT msg, WPARAM wParam, LPARAM lParam);
    void render();

    HWND hwnd_ = nullptr;
    HFONT keyFont_ = nullptr;
    HFONT titleFont_ = nullptr;
    int width_ = 0;
    int height_ = 0;
    int scale_ = 16;
    bool visible_ = false;
    KeyCounts counts_{};
};

}  // namespace apm
