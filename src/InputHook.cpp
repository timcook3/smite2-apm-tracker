#include "InputHook.h"

#include <windows.h>

namespace apm {
namespace {

// Low-level hook procedures cannot carry user data, so the active hook's
// state lives in file-scope globals. Guarded by the single-instance check
// in InputHook::install().
HHOOK g_keyboardHook = nullptr;
HHOOK g_mouseHook = nullptr;
InputHook::ActionCallback g_callback;

void uninstallGlobals() {
    if (g_keyboardHook) {
        UnhookWindowsHookEx(g_keyboardHook);
        g_keyboardHook = nullptr;
    }
    if (g_mouseHook) {
        UnhookWindowsHookEx(g_mouseHook);
        g_mouseHook = nullptr;
    }
    g_callback = nullptr;
}

LRESULT CALLBACK keyboardProc(int code, WPARAM wParam, LPARAM lParam) {
    if (code == HC_ACTION && (wParam == WM_KEYDOWN || wParam == WM_SYSKEYDOWN)) {
        const auto* info = reinterpret_cast<const KBDLLHOOKSTRUCT*>(lParam);
        // Ignore injected events (e.g. from macro software).
        if (!(info->flags & LLKHF_INJECTED) && g_callback) {
            g_callback(static_cast<int>(info->vkCode));
        }
    }
    return CallNextHookEx(nullptr, code, wParam, lParam);
}

LRESULT CALLBACK mouseProc(int code, WPARAM wParam, LPARAM lParam) {
    if (code == HC_ACTION) {
        switch (wParam) {
            case WM_LBUTTONDOWN:
            case WM_RBUTTONDOWN:
            case WM_MBUTTONDOWN:
            case WM_XBUTTONDOWN: {
                const auto* info = reinterpret_cast<const MSLLHOOKSTRUCT*>(lParam);
                if (!(info->flags & LLMHF_INJECTED) && g_callback) {
                    int button = 0;
                    switch (wParam) {
                        case WM_LBUTTONDOWN: button = 0; break;
                        case WM_RBUTTONDOWN: button = 1; break;
                        case WM_MBUTTONDOWN: button = 2; break;
                        default:  // WM_XBUTTONDOWN: XBUTTON1/2 in high word
                            button = 2 + HIWORD(info->mouseData);
                            break;
                    }
                    g_callback(InputHook::kMouseActionBase + button);
                }
                break;
            }
            default:
                break;
        }
    }
    return CallNextHookEx(nullptr, code, wParam, lParam);
}

}  // namespace

InputHook::~InputHook() {
    uninstall();
}

bool InputHook::install(ActionCallback onAction) {
    if (installed_ || g_keyboardHook || g_mouseHook) {
        return false;
    }
    g_callback = std::move(onAction);
    g_keyboardHook = SetWindowsHookExW(WH_KEYBOARD_LL, keyboardProc, nullptr, 0);
    g_mouseHook = SetWindowsHookExW(WH_MOUSE_LL, mouseProc, nullptr, 0);
    if (!g_keyboardHook || !g_mouseHook) {
        uninstallGlobals();
        return false;
    }
    installed_ = true;
    return true;
}

void InputHook::uninstall() {
    if (!installed_) {
        return;
    }
    uninstallGlobals();
    installed_ = false;
}

}  // namespace apm
