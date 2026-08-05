#include "OverlayWindow.h"

namespace apm {
namespace {

constexpr wchar_t kClassName[] = L"Smite2ApmOverlay";

LRESULT CALLBACK overlayWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (msg == WM_DESTROY) {
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

}  // namespace

OverlayWindow::~OverlayWindow() {
    destroy();
}

bool OverlayWindow::create(HINSTANCE instance, int x, int y, int fontSize) {
    if (hwnd_) {
        return false;
    }

    WNDCLASSW wc{};
    wc.lpfnWndProc = overlayWndProc;
    wc.hInstance = instance;
    wc.lpszClassName = kClassName;
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    if (!RegisterClassW(&wc) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) {
        return false;
    }

    // Size scales with the font so long strings (e.g. "APM 999 | 250 ms")
    // always fit.
    width_ = fontSize * 14;
    height_ = fontSize * 5;

    hwnd_ = CreateWindowExW(
        WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE,
        kClassName, L"Smite 2 APM Tracker", WS_POPUP,
        x, y, width_, height_,
        nullptr, nullptr, instance, nullptr);
    if (!hwnd_) {
        return false;
    }

    HDC screenDc = GetDC(nullptr);
    const int logicalHeight = -MulDiv(fontSize, GetDeviceCaps(screenDc, LOGPIXELSY), 72);
    ReleaseDC(nullptr, screenDc);
    font_ = CreateFontW(
        logicalHeight,
        0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Segoe UI");
    if (!font_) {
        destroy();
        return false;
    }

    ShowWindow(hwnd_, SW_SHOWNOACTIVATE);
    render();
    return true;
}

void OverlayWindow::setText(const std::wstring& apmLine, const std::wstring& pingLine) {
    if (!hwnd_) {
        return;
    }
    if (apmLine == apmLine_ && pingLine == pingLine_) {
        return;
    }
    apmLine_ = apmLine;
    pingLine_ = pingLine;
    render();
}

void OverlayWindow::destroy() {
    if (font_) {
        DeleteObject(font_);
        font_ = nullptr;
    }
    if (hwnd_) {
        DestroyWindow(hwnd_);
        hwnd_ = nullptr;
    }
}

void OverlayWindow::render() {
    HDC screenDc = GetDC(nullptr);
    HDC memDc = CreateCompatibleDC(screenDc);

    // 32-bit top-down DIB with premultiplied alpha for UpdateLayeredWindow.
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
    HGDIOBJ oldFont = SelectObject(memDc, font_);

    // Semi-transparent dark background so text is readable over the game.
    auto* pixels = static_cast<unsigned int*>(bits);
    constexpr unsigned int kBackground = 0xA0000000;  // ARGB, premultiplied
    for (int i = 0; i < width_ * height_; ++i) {
        pixels[i] = kBackground;
    }

    SetBkMode(memDc, TRANSPARENT);
    SetTextColor(memDc, RGB(255, 255, 255));

    RECT line1{8, 4, width_ - 8, height_ / 2};
    RECT line2{8, height_ / 2, width_ - 8, height_ - 4};
    DrawTextW(memDc, apmLine_.c_str(), -1, &line1, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    DrawTextW(memDc, pingLine_.c_str(), -1, &line2, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

    // GDI text rendering zeroes the alpha channel of touched pixels; restore
    // full opacity on any pixel whose color channels changed from the
    // background so the text is visible.
    for (int i = 0; i < width_ * height_; ++i) {
        if ((pixels[i] & 0x00FFFFFF) != (kBackground & 0x00FFFFFF)) {
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

    SelectObject(memDc, oldFont);
    SelectObject(memDc, oldBitmap);
    DeleteObject(bitmap);
    DeleteDC(memDc);
    ReleaseDC(nullptr, screenDc);
}

}  // namespace apm
