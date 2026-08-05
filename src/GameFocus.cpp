#include "GameFocus.h"

#include <cwctype>

namespace apm {
namespace {

bool equalsIgnoreCase(const std::wstring& a, const std::wstring& b) {
    if (a.size() != b.size()) {
        return false;
    }
    for (size_t i = 0; i < a.size(); ++i) {
        if (std::towlower(a[i]) != std::towlower(b[i])) {
            return false;
        }
    }
    return true;
}

// Executable base name of a process, or empty if it cannot be queried.
// PROCESS_QUERY_LIMITED_INFORMATION works even for elevated processes.
std::wstring processBaseName(DWORD pid) {
    HANDLE process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    if (!process) {
        return {};
    }
    wchar_t path[MAX_PATH];
    DWORD size = MAX_PATH;
    std::wstring result;
    if (QueryFullProcessImageNameW(process, 0, path, &size)) {
        const std::wstring full(path, size);
        const auto slash = full.find_last_of(L"\\/");
        result = slash == std::wstring::npos ? full : full.substr(slash + 1);
    }
    CloseHandle(process);
    return result;
}

}  // namespace

GameFocus::GameFocus(std::wstring processName) : processName_(std::move(processName)) {}

bool GameFocus::isGameFocused() {
    HWND foreground = GetForegroundWindow();
    if (!foreground) {
        return false;
    }
    DWORD pid = 0;
    GetWindowThreadProcessId(foreground, &pid);
    if (pid == 0) {
        return false;
    }
    if (pid != cachedPid_) {
        cachedPid_ = pid;
        cachedMatch_ = equalsIgnoreCase(processBaseName(pid), processName_);
    }
    return cachedMatch_;
}

}  // namespace apm
