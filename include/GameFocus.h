#pragma once

#include <string>

#include <windows.h>

namespace apm {

// Reports whether the game client is the foreground window, so input made
// outside the game (chat apps, browser, desktop) can be excluded.
//
// Cheap enough to call from the low-level hook callback: the foreground
// process's executable name is resolved only when the foreground PID
// changes and the result is cached, so the common path is just a
// GetForegroundWindow + PID comparison. Not thread-safe; the hook callback
// and the UI run on the same thread (the message loop), so no locking is
// needed.
class GameFocus {
public:
    explicit GameFocus(std::wstring processName);

    // True if the current foreground window belongs to the game process.
    bool isGameFocused();

    // Executable base name of the current foreground process (empty if it
    // cannot be determined). For diagnostics.
    static std::wstring foregroundProcessName();

private:
    const std::wstring processName_;
    DWORD cachedPid_ = 0;
    bool cachedMatch_ = false;
};

}  // namespace apm
