#include <windows.h>

#include <chrono>
#include <cmath>
#include <string>

#include "ApmCalculator.h"
#include "Config.h"
#include "InputHook.h"
#include "OverlayWindow.h"
#include "PingMonitor.h"

namespace {

constexpr UINT_PTR kUpdateTimerId = 1;
constexpr UINT kUpdateIntervalMs = 250;

std::wstring formatRate(double perMinute) {
    return std::to_wstring(static_cast<long long>(std::llround(perMinute)));
}

apm::OverlayState buildState(const apm::ApmStats& stats,
                             const apm::PingMonitor::Result& ping) {
    apm::OverlayState state;
    state.tracking = stats.tracking;
    state.apm = formatRate(stats.currentApm);
    state.averageApm = formatRate(stats.averageApm);
    state.peakApm = formatRate(stats.peakApm);
    state.eapm = formatRate(stats.currentEapm);
    if (ping.valid) {
        state.ping = std::to_wstring(ping.latencyMs) + L" ms";
        state.pingIsFallback = ping.source == apm::PingMonitor::Source::FallbackHost;
    } else {
        state.ping = L"--";
        state.pingIsFallback = false;
    }
    return state;
}

}  // namespace

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int) {
    const apm::Config config = apm::Config::load("config.ini");

    apm::ApmCalculator calculator{std::chrono::seconds(config.apmWindowSeconds)};
    calculator.start();  // begin tracking immediately; buttons control it

    apm::InputHook inputHook;
    if (!inputHook.install([&calculator](int actionId) { calculator.recordAction(actionId); })) {
        MessageBoxW(nullptr, L"Failed to install keyboard/mouse hooks.",
                    L"Smite 2 APM Tracker", MB_ICONERROR);
        return 1;
    }

    apm::PingMonitor pingMonitor{config};
    if (!pingMonitor.start()) {
        MessageBoxW(nullptr,
                    L"Ping monitor could not start (networking unavailable).\n"
                    L"The overlay will show APM only.",
                    L"Smite 2 APM Tracker", MB_ICONWARNING);
    }

    apm::OverlayWindow overlay;
    const bool created = overlay.create(
        instance, config.overlayX, config.overlayY, config.fontSize,
        [&calculator] { calculator.start(); },
        [&calculator] { calculator.stop(); });
    if (!created) {
        MessageBoxW(nullptr, L"Failed to create overlay window.",
                    L"Smite 2 APM Tracker", MB_ICONERROR);
        return 1;
    }

    SetTimer(overlay.handle(), kUpdateTimerId, kUpdateIntervalMs, nullptr);

    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
        if (msg.message == WM_TIMER && msg.wParam == kUpdateTimerId) {
            overlay.setState(buildState(calculator.stats(), pingMonitor.latest()));
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
