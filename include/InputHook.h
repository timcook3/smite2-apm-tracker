#pragma once

#include <functional>

namespace apm {

// Global low-level keyboard and mouse hooks (WH_KEYBOARD_LL / WH_MOUSE_LL).
//
// Counts key-down and mouse-button-down events as "actions" and invokes the
// callback for each one. Hooks are installed on the thread that calls
// install(); that thread must run a message loop for the hooks to receive
// events. Only one InputHook may be installed at a time.
class InputHook {
public:
    using ActionCallback = std::function<void()>;

    InputHook() = default;
    ~InputHook();

    InputHook(const InputHook&) = delete;
    InputHook& operator=(const InputHook&) = delete;

    // Installs the hooks. Returns false if hooks could not be installed or
    // another InputHook is already active.
    bool install(ActionCallback onAction);

    // Removes the hooks. Safe to call if not installed.
    void uninstall();

private:
    bool installed_ = false;
};

}  // namespace apm
