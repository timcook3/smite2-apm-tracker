#include "GameFocus.h"

#include <windows.h>

#include "ServerDetector.h"

namespace apm {

GameFocus::GameFocus(std::wstring processName) : processName_(std::move(processName)) {}

bool GameFocus::isGameFocused() {
    const auto now = std::chrono::steady_clock::now();
    if (gamePid_ == 0 || now - lastPidRefresh_ >= kPidRefreshInterval) {
        gamePid_ = ServerDetector::findProcess(processName_);
        lastPidRefresh_ = now;
    }
    if (gamePid_ == 0) {
        return false;
    }

    HWND foreground = GetForegroundWindow();
    if (!foreground) {
        return false;
    }
    DWORD foregroundPid = 0;
    GetWindowThreadProcessId(foreground, &foregroundPid);
    return foregroundPid == gamePid_;
}

}  // namespace apm
