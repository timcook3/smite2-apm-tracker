#include <windows.h>

#include <chrono>
#include <cmath>
#include <string>

#include "ApmCalculator.h"
#include "Config.h"
#include "GameFocus.h"
#include "HeatmapWindow.h"
#include "InputHook.h"
#include "OverlayWindow.h"
#include "PingMonitor.h"

namespace {

constexpr UINT_PTR kUpdateTimerId = 1;
constexpr UINT kUpdateIntervalMs = 250;

std::wstring formatRate(double perMinute) {
    return std::to_wstring(static_cast<long long>(std::llround(perMinute)));
}

std::wstring formatDuration(std::chrono::seconds duration) {
    const long long total = duration.count();
    const long long hours = total / 3600;
    const long long minutes = (total % 3600) / 60;
    const long long seconds = total % 60;
    wchar_t buffer[16];
    if (hours > 0) {
        swprintf(buffer, std::size(buffer), L"%lld:%02lld:%02lld", hours, minutes, seconds);
    } else {
        swprintf(buffer, std::size(buffer), L"%02lld:%02lld", minutes, seconds);
    }
    return buffer;
}

apm::OverlayState buildState(const apm::ApmStats& stats,
                             const apm::PingMonitor::Result& ping) {
    apm::OverlayState state;
    state.tracking = stats.tracking;
    state.sessionTime = formatDuration(stats.sessionDuration);
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

std::wstring toWide(const std::string& s) {
    return std::wstring(s.begin(), s.end());
}

}  // namespace

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int) {
    const apm::Config config = apm::Config::load("config.ini");

    apm::ApmCalculator calculator{std::chrono::seconds(config.apmWindowSeconds)};
    calculator.start();  // begin tracking immediately; buttons control it

    // Low-level hooks deliver events on this thread (via the message loop),
    // so the focus filter needs no synchronization.
    apm::GameFocus gameFocus{toWide(config.gameProcessName)};
    const bool filterInput = config.onlyCountGameInput;

    apm::InputHook inputHook;
    const bool hooked = inputHook.install(
        [&calculator, &gameFocus, filterInput](int actionId, unsigned ageMs) {
            if (filterInput && !gameFocus.isGameFocused()) {
                return;
            }
            calculator.recordAction(actionId, ageMs);
        });
    if (!hooked) {
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

    apm::HeatmapWindow heatmap;
    if (!heatmap.create(instance, config.overlayX + config.fontSize * 16,
                        config.overlayY, config.fontSize)) {
        MessageBoxW(nullptr, L"Failed to create heatmap window.",
                    L"Smite 2 APM Tracker", MB_ICONERROR);
        return 1;
    }

    apm::OverlayWindow overlay;
    const bool created = overlay.create(
        instance, config.overlayX, config.overlayY, config.fontSize,
        [&calculator] { calculator.start(); },
        [&calculator] { calculator.stop(); },
        [&heatmap, &calculator] {
            heatmap.setCounts(calculator.keyCounts());
            heatmap.toggle();
        });
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
            if (heatmap.visible()) {
                heatmap.setCounts(calculator.keyCounts());
            }
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
