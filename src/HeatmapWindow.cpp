#include "HeatmapWindow.h"

#include <algorithm>
#include <string>

namespace apm {
namespace {

constexpr wchar_t kClassName[] = L"Smite2ApmHeatmap";

// One key cap in the layout. `widthUnits` is in quarter-key units so keys
// like Tab (1.5u) and Space (6.25u) can be expressed exactly.
struct KeyDef {
    const wchar_t* label;
    int vk;
    int widthUnits;  // 4 units = one standard key width
};

constexpr int kUnit = 4;

const KeyDef kRow0[] = {
    {L"ESC", VK_ESCAPE, 4}, {L"F1", VK_F1, 4}, {L"F2", VK_F2, 4},
    {L"F3", VK_F3, 4}, {L"F4", VK_F4, 4}, {L"F5", VK_F5, 4},
    {L"F6", VK_F6, 4}, {L"F7", VK_F7, 4}, {L"F8", VK_F8, 4},
    {L"F9", VK_F9, 4}, {L"F10", VK_F10, 4}, {L"F11", VK_F11, 4},
    {L"F12", VK_F12, 4},
};
const KeyDef kRow1[] = {
    {L"`", VK_OEM_3, 4}, {L"1", '1', 4}, {L"2", '2', 4}, {L"3", '3', 4},
    {L"4", '4', 4}, {L"5", '5', 4}, {L"6", '6', 4}, {L"7", '7', 4},
    {L"8", '8', 4}, {L"9", '9', 4}, {L"0", '0', 4}, {L"-", VK_OEM_MINUS, 4},
    {L"=", VK_OEM_PLUS, 4}, {L"BKSP", VK_BACK, 8},
};
const KeyDef kRow2[] = {
    {L"TAB", VK_TAB, 6}, {L"Q", 'Q', 4}, {L"W", 'W', 4}, {L"E", 'E', 4},
    {L"R", 'R', 4}, {L"T", 'T', 4}, {L"Y", 'Y', 4}, {L"U", 'U', 4},
    {L"I", 'I', 4}, {L"O", 'O', 4}, {L"P", 'P', 4}, {L"[", VK_OEM_4, 4},
    {L"]", VK_OEM_6, 4}, {L"\\", VK_OEM_5, 6},
};
const KeyDef kRow3[] = {
    {L"CAPS", VK_CAPITAL, 7}, {L"A", 'A', 4}, {L"S", 'S', 4}, {L"D", 'D', 4},
    {L"F", 'F', 4}, {L"G", 'G', 4}, {L"H", 'H', 4}, {L"J", 'J', 4},
    {L"K", 'K', 4}, {L"L", 'L', 4}, {L";", VK_OEM_1, 4}, {L"'", VK_OEM_7, 4},
    {L"ENTER", VK_RETURN, 9},
};
const KeyDef kRow4[] = {
    {L"SHIFT", VK_LSHIFT, 9}, {L"Z", 'Z', 4}, {L"X", 'X', 4}, {L"C", 'C', 4},
    {L"V", 'V', 4}, {L"B", 'B', 4}, {L"N", 'N', 4}, {L"M", 'M', 4},
    {L",", VK_OEM_COMMA, 4}, {L".", VK_OEM_PERIOD, 4}, {L"/", VK_OEM_2, 4},
    {L"SHIFT", VK_RSHIFT, 11},
};
const KeyDef kRow5[] = {
    {L"CTRL", VK_LCONTROL, 5}, {L"WIN", VK_LWIN, 5}, {L"ALT", VK_LMENU, 5},
    {L"SPACE", VK_SPACE, 25}, {L"ALT", VK_RMENU, 5}, {L"CTRL", VK_RCONTROL, 5},
    {L"\u2190", VK_LEFT, 4}, {L"\u2191\u2193", VK_UP, 4}, {L"\u2192", VK_RIGHT, 4},
};

struct RowDef {
    const KeyDef* keys;
    size_t count;
};
const RowDef kRows[] = {
    {kRow0, std::size(kRow0)}, {kRow1, std::size(kRow1)},
    {kRow2, std::size(kRow2)}, {kRow3, std::size(kRow3)},
    {kRow4, std::size(kRow4)}, {kRow5, std::size(kRow5)},
};

// Pixel width of the widest row for the given key sizing.
int maxRowPixelWidth(int unitWidth, int gap) {
    int maxWidth = 0;
    for (const RowDef& row : kRows) {
        int width = static_cast<int>(row.count - 1) * gap;
        for (size_t i = 0; i < row.count; ++i) {
            width += row.keys[i].widthUnits * unitWidth;
        }
        maxWidth = std::max(maxWidth, width);
    }
    return maxWidth;
}

// Heat color ramp: white -> yellow -> orange -> red for t in [0, 1].
COLORREF heatColor(double t) {
    struct Stop { double t; int r, g, b; };
    static const Stop stops[] = {
        {0.0, 255, 255, 255},   // white
        {1.0 / 3.0, 255, 255, 0},  // yellow
        {2.0 / 3.0, 255, 165, 0},  // orange
        {1.0, 255, 0, 0},       // red
    };
    t = std::clamp(t, 0.0, 1.0);
    for (size_t i = 1; i < std::size(stops); ++i) {
        if (t <= stops[i].t) {
            const Stop& a = stops[i - 1];
            const Stop& b = stops[i];
            const double f = (t - a.t) / (b.t - a.t);
            return RGB(static_cast<int>(a.r + (b.r - a.r) * f),
                       static_cast<int>(a.g + (b.g - a.g) * f),
                       static_cast<int>(a.b + (b.b - a.b) * f));
        }
    }
    return RGB(255, 0, 0);
}

// Count for a key, merging left/right variants and the generic VK codes so
// presses reported under either code are attributed to the drawn key.
unsigned long long keyCount(const KeyCounts& counts, int vk) {
    unsigned long long total = counts[static_cast<size_t>(vk)];
    switch (vk) {
        case VK_LSHIFT: total += counts[VK_SHIFT]; break;
        case VK_LCONTROL: total += counts[VK_CONTROL]; break;
        case VK_LMENU: total += counts[VK_MENU]; break;
        case VK_UP: total += counts[VK_DOWN]; break;  // combined arrow cap
        default: break;
    }
    return total;
}

}  // namespace

HeatmapWindow::~HeatmapWindow() {
    destroy();
}

LRESULT CALLBACK HeatmapWindow::wndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (msg == WM_NCCREATE) {
        const auto* create = reinterpret_cast<CREATESTRUCTW*>(lParam);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA,
                          reinterpret_cast<LONG_PTR>(create->lpCreateParams));
    }
    auto* self = reinterpret_cast<HeatmapWindow*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    if (self && self->hwnd_ == hwnd) {
        return self->handleMessage(msg, wParam, lParam);
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

LRESULT HeatmapWindow::handleMessage(UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_LBUTTONDOWN:
            // The whole window drags.
            ReleaseCapture();
            SendMessageW(hwnd_, WM_NCLBUTTONDOWN, HTCAPTION, 0);
            return 0;
        default:
            return DefWindowProcW(hwnd_, msg, wParam, lParam);
    }
}

bool HeatmapWindow::create(HINSTANCE instance, int x, int y, int fontSize) {
    if (hwnd_) {
        return false;
    }
    scale_ = fontSize;

    const int keySize = scale_ * 2;             // one standard key cap
    const int gap = std::max(2, scale_ / 8);    // spacing between caps
    const int margin = scale_;
    const int headerHeight = scale_ * 2;
    const int unitWidth = keySize / kUnit;

    width_ = margin * 2 + maxRowPixelWidth(unitWidth, gap);
    height_ = headerHeight + margin * 2 +
              static_cast<int>(std::size(kRows)) * (keySize + gap) - gap;

    WNDCLASSW wc{};
    wc.lpfnWndProc = wndProc;
    wc.hInstance = instance;
    wc.lpszClassName = kClassName;
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    if (!RegisterClassW(&wc) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) {
        return false;
    }

    hwnd_ = CreateWindowExW(
        WS_EX_LAYERED | WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE,
        kClassName, L"Keyboard Heatmap", WS_POPUP,
        x, y, width_, height_,
        nullptr, nullptr, instance, this);
    if (!hwnd_) {
        return false;
    }

    HDC screenDc = GetDC(nullptr);
    keyFont_ = CreateFontW(
        -MulDiv(scale_ / 2, GetDeviceCaps(screenDc, LOGPIXELSY), 72),
        0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Segoe UI");
    titleFont_ = CreateFontW(
        -MulDiv(scale_, GetDeviceCaps(screenDc, LOGPIXELSY), 72),
        0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Segoe UI Semibold");
    ReleaseDC(nullptr, screenDc);
    if (!keyFont_ || !titleFont_) {
        destroy();
        return false;
    }
    return true;
}

void HeatmapWindow::toggle() {
    if (!hwnd_) {
        return;
    }
    visible_ = !visible_;
    if (visible_) {
        render();
        ShowWindow(hwnd_, SW_SHOWNOACTIVATE);
    } else {
        ShowWindow(hwnd_, SW_HIDE);
    }
}

void HeatmapWindow::setCounts(const KeyCounts& counts) {
    if (counts == counts_) {
        return;
    }
    counts_ = counts;
    if (visible_) {
        render();
    }
}

void HeatmapWindow::destroy() {
    for (HFONT* font : {&keyFont_, &titleFont_}) {
        if (*font) {
            DeleteObject(*font);
            *font = nullptr;
        }
    }
    if (hwnd_) {
        DestroyWindow(hwnd_);
        hwnd_ = nullptr;
    }
    visible_ = false;
}

void HeatmapWindow::render() {
    HDC screenDc = GetDC(nullptr);
    HDC memDc = CreateCompatibleDC(screenDc);

    BITMAPINFO bmi{};
    bmi.bmiHeader.biSize = sizeof(bmi.bmiHeader);
    bmi.bmiHeader.biWidth = width_;
    bmi.bmiHeader.biHeight = -height_;
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    void* bits = nullptr;
    HBITMAP bitmap = CreateDIBSection(memDc, &bmi, DIB_RGB_COLORS, &bits, nullptr, 0);
    if (!bitmap) {
        DeleteDC(memDc);
        ReleaseDC(nullptr, screenDc);
        return;
    }
    HGDIOBJ oldBitmap = SelectObject(memDc, bitmap);
    auto* pixels = static_cast<unsigned int*>(bits);

    // Panel background and gold frame (matches the overlay styling).
    const unsigned int gold = 0xFFC9A24A;
    const unsigned int goldDim = 0xFF6E5A2C;
    for (int i = 0; i < width_ * height_; ++i) {
        pixels[i] = 0xE8141311;
    }
    const int headerHeight = scale_ * 2;
    for (int x = 0; x < width_; ++x) {
        pixels[x] = gold;
        pixels[(height_ - 1) * width_ + x] = gold;
        pixels[(headerHeight - 1) * width_ + x] = gold;
    }
    for (int y = 0; y < height_; ++y) {
        pixels[y * width_] = goldDim;
        pixels[y * width_ + width_ - 1] = goldDim;
    }

    SetBkMode(memDc, TRANSPARENT);
    SelectObject(memDc, titleFont_);
    SetTextColor(memDc, RGB(0xC9, 0xA2, 0x4A));
    RECT titleRect{scale_, 0, width_ - scale_, headerHeight};
    DrawTextW(memDc, L"KEYBOARD HEATMAP", -1, &titleRect,
              DT_LEFT | DT_VCENTER | DT_SINGLELINE);

    unsigned long long maxCount = 0;
    for (const RowDef& row : kRows) {
        for (size_t i = 0; i < row.count; ++i) {
            maxCount = std::max(maxCount, keyCount(counts_, row.keys[i].vk));
        }
    }

    const int keySize = scale_ * 2;
    const int gap = std::max(2, scale_ / 8);
    const int margin = scale_;
    const int unitWidth = keySize / kUnit;

    SelectObject(memDc, keyFont_);
    int top = headerHeight + margin;
    for (const RowDef& row : kRows) {
        int left = margin;
        for (size_t i = 0; i < row.count; ++i) {
            const KeyDef& key = row.keys[i];
            const int keyWidth = key.widthUnits * unitWidth;
            const unsigned long long count = keyCount(counts_, key.vk);

            COLORREF fill;
            COLORREF textColor;
            if (count == 0 || maxCount == 0) {
                fill = RGB(0x24, 0x1F, 0x17);  // unpressed: dark cap
                textColor = RGB(0x9A, 0x94, 0x86);
            } else {
                // Normalize against the most-pressed key; a single press on
                // the max key still maps to full red.
                fill = heatColor(static_cast<double>(count) /
                                 static_cast<double>(maxCount));
                textColor = RGB(0x1A, 0x16, 0x10);
            }

            RECT keyRect{left, top, left + keyWidth, top + keySize};
            HBRUSH brush = CreateSolidBrush(fill);
            FillRect(memDc, &keyRect, brush);
            DeleteObject(brush);

            // Label on the upper half, count on the lower half.
            RECT labelRect{keyRect.left, keyRect.top, keyRect.right,
                           keyRect.top + keySize / 2};
            SetTextColor(memDc, textColor);
            DrawTextW(memDc, key.label, -1, &labelRect,
                      DT_CENTER | DT_BOTTOM | DT_SINGLELINE);
            if (count > 0) {
                RECT countRect{keyRect.left, keyRect.top + keySize / 2,
                               keyRect.right, keyRect.bottom};
                const std::wstring text = std::to_wstring(count);
                DrawTextW(memDc, text.c_str(), -1, &countRect,
                          DT_CENTER | DT_TOP | DT_SINGLELINE);
            }
            left += keyWidth + gap;
        }
        top += keySize + gap;
    }

    // Restore alpha on pixels GDI touched (FillRect/DrawText zero it).
    for (int i = 0; i < width_ * height_; ++i) {
        if ((pixels[i] & 0xFF000000) == 0) {
            pixels[i] |= 0xFF000000;
        }
    }

    POINT source{0, 0};
    SIZE size{width_, height_};
    BLENDFUNCTION blend{};
    blend.BlendOp = AC_SRC_OVER;
    blend.SourceConstantAlpha = 255;
    blend.AlphaFormat = AC_SRC_ALPHA;
    UpdateLayeredWindow(hwnd_, screenDc, nullptr, &size, memDc, &source, 0, &blend, ULW_ALPHA);

    SelectObject(memDc, oldBitmap);
    DeleteObject(bitmap);
    DeleteDC(memDc);
    ReleaseDC(nullptr, screenDc);
}

}  // namespace apm
