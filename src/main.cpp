#include <windows.h>

#include <cmath>
#include <memory>
#include <string>

#include "ApmCalculator.h"
#include "Config.h"
#include "InputHook.h"
#include "OverlayWindow.h"
#include "PingMonitor.h"

namespace {

constexpr UINT_PTR kUpdateTimerId = 1;
constexpr UINT kUpdateIntervalMs = 250;

std::wstring formatApmLine(double apm) {
    return L"APM  " + std::to_wstring(static_cast<long long>(std::llround(apm)));
}

std::wstring formatPingLine(const apm::PingMonitor::Result& result) {
    if (!result.valid) {
        return L"Ping  --";
    }
    return L"Ping  " + std::to_wstring(result.latencyMs) + L" ms";
}

}  // namespace

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int) {
    const apm::Config config = apm::Config::load("config.ini");

    apm::ApmCalculator calculator{std::chrono::seconds(config.apmWindowSeconds)};

    apm::InputHook inputHook;
    if (!inputHook.install([&calculator] { calculator.recordAction(); })) {
        MessageBoxW(nullptr, L"Failed to install keyboard/mouse hooks.",
                    L"Smite 2 APM Tracker", MB_ICONERROR);
        return 1;
    }

    apm::PingMonitor pingMonitor{config.pingHost, config.pingIntervalMs, config.pingTimeoutMs};
    const bool pingAvailable = pingMonitor.start();
    if (!pingAvailable) {
        MessageBoxW(nullptr,
                    L"Ping monitor could not start (host unresolvable or ICMP unavailable).\n"
                    L"The overlay will show APM only.",
                    L"Smite 2 APM Tracker", MB_ICONWARNING);
    }

    apm::OverlayWindow overlay;
    if (!overlay.create(instance, config.overlayX, config.overlayY, config.fontSize)) {
        MessageBoxW(nullptr, L"Failed to create overlay window.",
                    L"Smite 2 APM Tracker", MB_ICONERROR);
        return 1;
    }

    SetTimer(overlay.handle(), kUpdateTimerId, kUpdateIntervalMs, nullptr);

    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
        if (msg.message == WM_TIMER && msg.wParam == kUpdateTimerId) {
            overlay.setText(formatApmLine(calculator.currentApm()),
                            formatPingLine(pingMonitor.latest()));
            // Keep the overlay above late-created topmost windows.
            SetWindowPos(overlay.handle(), HWND_TOPMOST, 0, 0, 0, 0,
                         SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
        }
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    KillTimer(overlay.handle(), kUpdateTimerId);
    pingMonitor.stop();
    inputHook.uninstall();
    return 0;
}
