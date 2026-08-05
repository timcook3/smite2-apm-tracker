#pragma once

#include <functional>

namespace apm {

// Global low-level keyboard and mouse hooks (WH_KEYBOARD_LL / WH_MOUSE_LL).
//
// Counts key-down and mouse-button-down events as "actions" and invokes the
// callback for each one with an identifier for the input (virtual-key code
// for keys, kMouseActionBase + button index for mouse buttons), so callers
// can distinguish repeated presses of the same input. Hooks are installed on the thread that calls
// install(); that thread must run a message loop for the hooks to receive
// events. Only one InputHook may be installed at a time.
class InputHook {
public:
    // Action identifiers for mouse buttons start here (keyboard actions use
    // their virtual-key code, which is < 256).
    static constexpr int kMouseActionBase = 256;

    using ActionCallback = std::function<void(int actionId)>;

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
