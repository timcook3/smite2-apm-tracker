#pragma once

#include <chrono>
#include <string>

namespace apm {

// Reports whether the game client is the foreground window, so input made
// outside the game (chat apps, browser, desktop) can be excluded.
//
// The game's PID is re-resolved at most every kPidRefreshInterval, so a
// restarted client is picked up automatically without paying a process
// snapshot on every input event. Not thread-safe; call from one thread.
class GameFocus {
public:
    static constexpr std::chrono::seconds kPidRefreshInterval{2};

    explicit GameFocus(std::wstring processName);

    // True if the current foreground window belongs to the game process.
    bool isGameFocused();

private:
    const std::wstring processName_;
    unsigned long gamePid_ = 0;
    std::chrono::steady_clock::time_point lastPidRefresh_{};
};

}  // namespace apm
