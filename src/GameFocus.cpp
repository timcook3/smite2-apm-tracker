#include "GameFocus.h"

#include <cwctype>

namespace apm {
namespace {

std::wstring toLower(const std::wstring& s) {
    std::wstring out(s.size(), L'\0');
    for (size_t i = 0; i < s.size(); ++i) {
        out[i] = static_cast<wchar_t>(std::towlower(s[i]));
    }
    return out;
}

// The configured name may not exactly match the shipped executable across
// game updates, so accept an exact match or any process whose name
// contains "smite".
bool matchesGame(const std::wstring& processName, const std::wstring& configured) {
    const std::wstring name = toLower(processName);
    return !name.empty() &&
           (name == toLower(configured) || name.find(L"smite") != std::wstring::npos);
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
        cachedMatch_ = matchesGame(processBaseName(pid), processName_);
    }
    return cachedMatch_;
}

std::wstring GameFocus::foregroundProcessName() {
    HWND foreground = GetForegroundWindow();
    if (!foreground) {
        return {};
    }
    DWORD pid = 0;
    GetWindowThreadProcessId(foreground, &pid);
    return pid ? processBaseName(pid) : std::wstring{};
}

}  // namespace apm
