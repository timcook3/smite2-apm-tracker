#include "OverlayWindow.h"

#include <windowsx.h>

#include <algorithm>
#include <iterator>

namespace apm {
namespace {

constexpr wchar_t kClassName[] = L"Smite2ApmOverlay";

constexpr UINT kTrayCallbackMsg = WM_APP + 1;
constexpr UINT kTrayIconId = 1;
constexpr UINT kMenuRestoreId = 1;
constexpr UINT kMenuExitId = 2;

// Button indices in buttons_.
constexpr size_t kStartButton = 0;
constexpr size_t kStopButton = 1;
constexpr size_t kHeatmapButton = 2;
constexpr size_t kMinimizeButton = 3;
constexpr size_t kMaximizeButton = 4;
constexpr size_t kCloseButton = 5;

// Smite 2 palette: near-black panel, gold accents, warm off-white values.
constexpr COLORREF kGold = RGB(0xC9, 0xA2, 0x4A);
constexpr COLORREF kGoldDim = RGB(0x8A, 0x71, 0x38);
constexpr COLORREF kText = RGB(0xE8, 0xE2, 0xD4);
constexpr COLORREF kTextDim = RGB(0x9A, 0x94, 0x86);
constexpr unsigned int kPanelBg = 0xD8141311;    // ARGB premultiplied-ish dark
constexpr unsigned int kHeaderBg = 0xE81E1A14;   // slightly warmer header
constexpr unsigned int kButtonBg = 0xE8241F17;
constexpr unsigned int kButtonHoverBg = 0xF0332B1C;

HFONT makeFont(int points, int weight, HDC screenDc) {
    return CreateFontW(
        -MulDiv(points, GetDeviceCaps(screenDc, LOGPIXELSY), 72),
        0, 0, 0, weight, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Segoe UI Semibold");
}

// Fills a rectangle in a 32-bit top-down DIB with an ARGB color.
void fillRect(unsigned int* pixels, int stride, const RECT& r, unsigned int argb) {
    for (int y = r.top; y < r.bottom; ++y) {
        unsigned int* row = pixels + y * stride;
        for (int x = r.left; x < r.right; ++x) {
            row[x] = argb;
        }
    }
}

}  // namespace

OverlayWindow::~OverlayWindow() {
    destroy();
}

LRESULT CALLBACK OverlayWindow::wndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (msg == WM_NCCREATE) {
        const auto* create = reinterpret_cast<CREATESTRUCTW*>(lParam);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA,
                          reinterpret_cast<LONG_PTR>(create->lpCreateParams));
    }
    auto* self = reinterpret_cast<OverlayWindow*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    if (self && self->hwnd_ == hwnd) {
        return self->handleMessage(msg, wParam, lParam);
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

LRESULT OverlayWindow::handleMessage(UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_DESTROY:
            if (inTray_) {
                Shell_NotifyIconW(NIM_DELETE, &trayIcon_);
                inTray_ = false;
            }
            PostQuitMessage(0);
            return 0;
        case WM_NCCALCSIZE:
            // Claim the whole window rect as client area: no system frame
            // (WS_THICKFRAME is present only to enable the sizing loop).
            if (wParam) {
                return 0;
            }
            break;
        case WM_NCHITTEST: {
            POINT pt{GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
            ScreenToClient(hwnd_, &pt);
            const LRESULT resize = resizeHitTest(pt.x, pt.y);
            if (resize != 0) {
                return resize;
            }
            if (buttonAt(pt.x, pt.y) >= 0) {
                return HTCLIENT;  // buttons receive normal mouse messages
            }
            return HTCAPTION;  // everywhere else drags the panel
        }
        case WM_GETMINMAXINFO: {
            auto* info = reinterpret_cast<MINMAXINFO*>(lParam);
            info->ptMinTrackSize.x = kMinScale * kWidthUnits;
            info->ptMinTrackSize.y = kMinScale * kHeightUnits;
            return 0;
        }
        case WM_SIZING: {
            // Keep the panel's aspect ratio while resizing.
            auto* rect = reinterpret_cast<RECT*>(lParam);
            int w = std::max<int>(rect->right - rect->left, kMinScale * kWidthUnits);
            w -= w % kWidthUnits;  // whole scale units keep the layout crisp
            const int h = w * kHeightUnits / kWidthUnits;
            switch (wParam) {
                case WMSZ_LEFT:
                case WMSZ_TOPLEFT:
                case WMSZ_BOTTOMLEFT:
                    rect->left = rect->right - w;
                    break;
                default:
                    rect->right = rect->left + w;
                    break;
            }
            switch (wParam) {
                case WMSZ_TOP:
                case WMSZ_TOPLEFT:
                case WMSZ_TOPRIGHT:
                    rect->top = rect->bottom - h;
                    break;
                default:
                    rect->bottom = rect->top + h;
                    break;
            }
            return TRUE;
        }
        case WM_SIZE: {
            const int newWidth = LOWORD(lParam);
            if (newWidth > 0 && newWidth / kWidthUnits != scale_) {
                if (applyScale(newWidth / kWidthUnits)) {
                    render();
                }
            }
            return 0;
        }
        case WM_LBUTTONDOWN: {
            const int index = buttonAt(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
            if (index >= 0) {
                Button& button = buttons_[static_cast<size_t>(index)];
                if (button.enabled && button.onClick) {
                    button.onClick();
                }
            }
            return 0;
        }
        case WM_MOUSEMOVE: {
            const int index = buttonAt(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
            bool changed = false;
            for (size_t i = 0; i < buttons_.size(); ++i) {
                const bool hovered = static_cast<int>(i) == index;
                if (buttons_[i].hovered != hovered) {
                    buttons_[i].hovered = hovered;
                    changed = true;
                }
            }
            if (changed) {
                render();
            }
            if (!trackingMouseLeave_) {
                TRACKMOUSEEVENT track{};
                track.cbSize = sizeof(track);
                track.dwFlags = TME_LEAVE;
                track.hwndTrack = hwnd_;
                if (TrackMouseEvent(&track)) {
                    trackingMouseLeave_ = true;
                }
            }
            return 0;
        }
        case WM_MOUSELEAVE: {
            trackingMouseLeave_ = false;
            bool changed = false;
            for (Button& button : buttons_) {
                if (button.hovered) {
                    button.hovered = false;
                    changed = true;
                }
            }
            if (changed) {
                render();
            }
            return 0;
        }
        case kTrayCallbackMsg:
            switch (LOWORD(lParam)) {
                case WM_LBUTTONUP:
                    restoreFromTray();
                    break;
                case WM_RBUTTONUP:
                    showTrayMenu();
                    break;
                default:
                    break;
            }
            return 0;
        case WM_COMMAND:
            switch (LOWORD(wParam)) {
                case kMenuRestoreId:
                    restoreFromTray();
                    break;
                case kMenuExitId:
                    DestroyWindow(hwnd_);
                    break;
                default:
                    break;
            }
            return 0;
        default:
            break;
    }
    return DefWindowProcW(hwnd_, msg, wParam, lParam);
}

bool OverlayWindow::create(HINSTANCE instance, int x, int y, int fontSize,
                           ButtonCallback onStart, ButtonCallback onStop,
                           ButtonCallback onHeatmap) {
    if (hwnd_) {
        return false;
    }

    WNDCLASSW wc{};
    wc.lpfnWndProc = wndProc;
    wc.hInstance = instance;
    wc.lpszClassName = kClassName;
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    if (!RegisterClassW(&wc) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) {
        return false;
    }

    const int initialScale = std::max(fontSize, kMinScale);
    hwnd_ = CreateWindowExW(
        WS_EX_LAYERED | WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE,
        kClassName, L"Smite 2 APM Tracker", WS_POPUP | WS_THICKFRAME,
        x, y, initialScale * kWidthUnits, initialScale * kHeightUnits,
        nullptr, nullptr, instance, this);
    if (!hwnd_) {
        return false;
    }

    buttons_.clear();
    buttons_.push_back(Button{RECT{}, L"START", true, false, std::move(onStart)});
    buttons_.push_back(Button{RECT{}, L"STOP", false, false, std::move(onStop)});
    buttons_.push_back(Button{RECT{}, L"HEATMAP", true, false, std::move(onHeatmap)});
    buttons_.push_back(Button{RECT{}, L"\u2013", true, false,
                              [this] { minimizeToTray(); }});
    buttons_.push_back(Button{RECT{}, L"\u25A1", true, false,
                              [this] { toggleMaximize(); }});
    buttons_.push_back(Button{RECT{}, L"\u2715", true, false,
                              [this] { DestroyWindow(hwnd_); }});

    if (!applyScale(initialScale)) {
        destroy();
        return false;
    }

    trayIcon_.cbSize = sizeof(trayIcon_);
    trayIcon_.hWnd = hwnd_;
    trayIcon_.uID = kTrayIconId;
    trayIcon_.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP;
    trayIcon_.uCallbackMessage = kTrayCallbackMsg;
    trayIcon_.hIcon = LoadIconW(nullptr, IDI_APPLICATION);
    lstrcpynW(trayIcon_.szTip, L"Smite 2 APM Tracker",
              static_cast<int>(std::size(trayIcon_.szTip)));

    ShowWindow(hwnd_, SW_SHOWNOACTIVATE);
    render();
    return true;
}

bool OverlayWindow::applyScale(int scale) {
    scale_ = std::max(scale, kMinScale);
    width_ = scale_ * kWidthUnits;
    height_ = scale_ * kHeightUnits;

    for (HFONT* font : {&titleFont_, &labelFont_, &valueFont_, &buttonFont_}) {
        if (*font) {
            DeleteObject(*font);
            *font = nullptr;
        }
    }
    HDC screenDc = GetDC(nullptr);
    titleFont_ = makeFont(scale_, FW_BOLD, screenDc);
    labelFont_ = makeFont(scale_ * 3 / 4, FW_SEMIBOLD, screenDc);
    valueFont_ = makeFont(scale_ * 7 / 8, FW_BOLD, screenDc);
    buttonFont_ = makeFont(scale_ * 3 / 4, FW_BOLD, screenDc);
    ReleaseDC(nullptr, screenDc);
    if (!titleFont_ || !labelFont_ || !valueFont_ || !buttonFont_) {
        return false;
    }

    layoutButtons();
    return true;
}

void OverlayWindow::layoutButtons() {
    const int margin = scale_ * 3 / 4;
    const int buttonHeight = scale_ * 2;
    const int buttonWidth = (width_ - margin * 3) / 2;

    // Start and Stop side by side, Heatmap full-width below them.
    const int bottomRowTop = height_ - margin - buttonHeight;
    const int topRowTop = bottomRowTop - margin / 2 - buttonHeight;
    buttons_[kStartButton].rect = RECT{margin, topRowTop, margin + buttonWidth,
                                       topRowTop + buttonHeight};
    buttons_[kStopButton].rect = RECT{width_ - margin - buttonWidth, topRowTop,
                                      width_ - margin, topRowTop + buttonHeight};
    buttons_[kHeatmapButton].rect = RECT{margin, bottomRowTop, width_ - margin,
                                         bottomRowTop + buttonHeight};

    // Caption buttons (minimize / maximize / close) at the header's right.
    const int headerHeight = scale_ * 2;
    const int captionSize = scale_ * 5 / 4;
    const int captionTop = (headerHeight - captionSize) / 2;
    int right = width_ - captionTop;
    for (size_t i : {kCloseButton, kMaximizeButton, kMinimizeButton}) {
        buttons_[i].rect = RECT{right - captionSize, captionTop, right,
                                captionTop + captionSize};
        right -= captionSize + captionTop;
    }
}

int OverlayWindow::buttonAt(int x, int y) const {
    const POINT pt{x, y};
    for (size_t i = 0; i < buttons_.size(); ++i) {
        if (PtInRect(&buttons_[i].rect, pt)) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

LRESULT OverlayWindow::resizeHitTest(int x, int y) const {
    if (maximized_) {
        return 0;  // resize by dragging is disabled while maximized
    }
    const int band = std::max(4, scale_ / 3);
    const bool left = x < band;
    const bool right = x >= width_ - band;
    const bool top = y < band;
    const bool bottom = y >= height_ - band;
    if (top && left) return HTTOPLEFT;
    if (top && right) return HTTOPRIGHT;
    if (bottom && left) return HTBOTTOMLEFT;
    if (bottom && right) return HTBOTTOMRIGHT;
    if (left) return HTLEFT;
    if (right) return HTRIGHT;
    if (top) return HTTOP;
    if (bottom) return HTBOTTOM;
    return 0;
}

void OverlayWindow::minimizeToTray() {
    if (inTray_) {
        return;
    }
    if (Shell_NotifyIconW(NIM_ADD, &trayIcon_)) {
        inTray_ = true;
        ShowWindow(hwnd_, SW_HIDE);
    }
}

void OverlayWindow::restoreFromTray() {
    if (!inTray_) {
        return;
    }
    Shell_NotifyIconW(NIM_DELETE, &trayIcon_);
    inTray_ = false;
    ShowWindow(hwnd_, SW_SHOWNOACTIVATE);
    render();
}

void OverlayWindow::toggleMaximize() {
    RECT workArea{};
    if (!SystemParametersInfoW(SPI_GETWORKAREA, 0, &workArea, 0)) {
        return;
    }
    if (!maximized_) {
        RECT window{};
        GetWindowRect(hwnd_, &window);
        restorePos_ = POINT{window.left, window.top};
        restoreScale_ = scale_;

        const int scaleForWidth = (workArea.right - workArea.left) / kWidthUnits;
        const int scaleForHeight = (workArea.bottom - workArea.top) / kHeightUnits;
        maximized_ = true;
        if (applyScale(std::min(scaleForWidth, scaleForHeight))) {
            const int x = workArea.left +
                          (workArea.right - workArea.left - width_) / 2;
            const int y = workArea.top +
                          (workArea.bottom - workArea.top - height_) / 2;
            SetWindowPos(hwnd_, nullptr, x, y, width_, height_,
                         SWP_NOZORDER | SWP_NOACTIVATE);
            render();
        }
    } else {
        maximized_ = false;
        if (applyScale(restoreScale_)) {
            SetWindowPos(hwnd_, nullptr, restorePos_.x, restorePos_.y,
                         width_, height_, SWP_NOZORDER | SWP_NOACTIVATE);
            render();
        }
    }
}

void OverlayWindow::showTrayMenu() {
    HMENU menu = CreatePopupMenu();
    if (!menu) {
        return;
    }
    AppendMenuW(menu, MF_STRING, kMenuRestoreId, L"Restore");
    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(menu, MF_STRING, kMenuExitId, L"Exit");

    POINT cursor{};
    GetCursorPos(&cursor);
    // Required so the menu closes when the user clicks elsewhere.
    SetForegroundWindow(hwnd_);
    TrackPopupMenu(menu, TPM_RIGHTBUTTON, cursor.x, cursor.y, 0, hwnd_, nullptr);
    DestroyMenu(menu);
}

void OverlayWindow::setState(const OverlayState& state) {
    if (!hwnd_) {
        return;
    }
    const bool trackingChanged = state.tracking != state_.tracking;
    const bool textChanged =
        state.gameFocused != state_.gameFocused ||
        state.sessionTime != state_.sessionTime ||
        state.apm != state_.apm || state.averageApm != state_.averageApm ||
        state.peakApm != state_.peakApm || state.eapm != state_.eapm ||
        state.ping != state_.ping || state.pingIsFallback != state_.pingIsFallback;
    if (!trackingChanged && !textChanged) {
        return;
    }
    state_ = state;
    buttons_[kStartButton].enabled = !state_.tracking;
    buttons_[kStopButton].enabled = state_.tracking;
    if (!inTray_) {
        render();
    }
}

void OverlayWindow::destroy() {
    for (HFONT* font : {&titleFont_, &labelFont_, &valueFont_, &buttonFont_}) {
        if (*font) {
            DeleteObject(*font);
            *font = nullptr;
        }
    }
    if (hwnd_) {
        DestroyWindow(hwnd_);
        hwnd_ = nullptr;
    }
}

void OverlayWindow::render() {
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

    const int margin = scale_ * 3 / 4;
    const int headerHeight = scale_ * 2;
    const unsigned int gold = 0xFFC9A24A;
    const unsigned int goldDim = 0xFF6E5A2C;

    // Panel background, header band, and gold hairline frame.
    fillRect(pixels, width_, RECT{0, 0, width_, height_}, kPanelBg);
    fillRect(pixels, width_, RECT{0, 0, width_, headerHeight}, kHeaderBg);
    fillRect(pixels, width_, RECT{0, 0, width_, 1}, gold);
    fillRect(pixels, width_, RECT{0, height_ - 1, width_, height_}, gold);
    fillRect(pixels, width_, RECT{0, 0, 1, height_}, goldDim);
    fillRect(pixels, width_, RECT{width_ - 1, 0, width_, height_}, goldDim);
    fillRect(pixels, width_, RECT{0, headerHeight - 1, width_, headerHeight}, gold);
    // Gold accent notch on the left of the header, echoing Smite 2 menus.
    fillRect(pixels, width_, RECT{0, 0, scale_ / 4, headerHeight}, gold);

    SetBkMode(memDc, TRANSPARENT);

    // Header title.
    SelectObject(memDc, titleFont_);
    SetTextColor(memDc, kGold);
    RECT titleRect{margin, 0, width_ - margin, headerHeight};
    DrawTextW(memDc, L"APM TRACKER", -1, &titleRect,
              DT_LEFT | DT_VCENTER | DT_SINGLELINE);

    // Tracking indicator, left of the caption buttons.
    const wchar_t* status =
        !state_.tracking ? L"PAUSED"
        : state_.gameFocused ? L"LIVE" : L"NO GAME";
    SelectObject(memDc, labelFont_);
    SetTextColor(memDc, state_.tracking && state_.gameFocused ? kGold : kTextDim);
    RECT statusRect{margin, 0,
                    buttons_[kMinimizeButton].rect.left - scale_ / 2, headerHeight};
    DrawTextW(memDc, status, -1, &statusRect,
              DT_RIGHT | DT_VCENTER | DT_SINGLELINE);

    // Stat rows: label left (gold, uppercase), value right (off-white).
    struct Row {
        const wchar_t* label;
        const std::wstring* value;
        bool dim;
    };
    const std::wstring pingDisplay =
        state_.pingIsFallback ? state_.ping + L" *" : state_.ping;
    const Row rows[] = {
        {L"TIME", &state_.sessionTime, false},
        {L"APM", &state_.apm, false},
        {L"AVERAGE APM", &state_.averageApm, false},
        {L"PEAK APM", &state_.peakApm, false},
        {L"EAPM", &state_.eapm, false},
        {L"PING", &pingDisplay, state_.pingIsFallback},
    };

    const int rowsTop = headerHeight + scale_ / 2;
    const int rowsBottom = buttons_[kStartButton].rect.top - scale_ / 2;
    const int rowHeight = (rowsBottom - rowsTop) / static_cast<int>(std::size(rows));
    for (size_t i = 0; i < std::size(rows); ++i) {
        const int top = rowsTop + static_cast<int>(i) * rowHeight;
        RECT rowRect{margin, top, width_ - margin, top + rowHeight};

        SelectObject(memDc, labelFont_);
        SetTextColor(memDc, kGoldDim);
        DrawTextW(memDc, rows[i].label, -1, &rowRect,
                  DT_LEFT | DT_VCENTER | DT_SINGLELINE);

        SelectObject(memDc, valueFont_);
        SetTextColor(memDc, rows[i].dim ? kTextDim : kText);
        DrawTextW(memDc, rows[i].value->c_str(), -1, &rowRect,
                  DT_RIGHT | DT_VCENTER | DT_SINGLELINE);

        // Hairline separator under each row except the last.
        if (i + 1 < std::size(rows)) {
            fillRect(pixels, width_,
                     RECT{margin, top + rowHeight - 1, width_ - margin, top + rowHeight},
                     0xFF2E2A20);
        }
    }

    // Panel buttons (bordered) and caption buttons (borderless glyphs).
    SelectObject(memDc, buttonFont_);
    for (size_t i = 0; i < buttons_.size(); ++i) {
        const Button& button = buttons_[i];
        const bool caption = i >= kMinimizeButton;
        const unsigned int bg = button.hovered && button.enabled
                                    ? kButtonHoverBg
                                    : (caption ? kHeaderBg : kButtonBg);
        fillRect(pixels, width_, button.rect, bg);
        if (!caption) {
            const unsigned int borderColor = button.enabled ? gold : goldDim;
            fillRect(pixels, width_,
                     RECT{button.rect.left, button.rect.top, button.rect.right,
                          button.rect.top + 1}, borderColor);
            fillRect(pixels, width_,
                     RECT{button.rect.left, button.rect.bottom - 1, button.rect.right,
                          button.rect.bottom}, borderColor);
            fillRect(pixels, width_,
                     RECT{button.rect.left, button.rect.top, button.rect.left + 1,
                          button.rect.bottom}, borderColor);
            fillRect(pixels, width_,
                     RECT{button.rect.right - 1, button.rect.top, button.rect.right,
                          button.rect.bottom}, borderColor);
        }

        SetTextColor(memDc, caption ? (button.hovered ? kText : kTextDim)
                                    : (button.enabled ? kGold : kTextDim));
        RECT labelRect = button.rect;
        DrawTextW(memDc, button.label.c_str(), -1, &labelRect,
                  DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    }

    // GDI text rendering zeroes the alpha channel of touched pixels; restore
    // full opacity on those pixels so the text is visible.
    for (int i = 0; i < width_ * height_; ++i) {
        if ((pixels[i] & 0xFF000000) == 0 && (pixels[i] & 0x00FFFFFF) != 0) {
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
