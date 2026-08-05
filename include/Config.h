#pragma once

#include <string>

namespace apm {

// Application settings, loaded from an INI-style file (key=value per line,
// '#' starts a comment). Missing file or keys fall back to defaults.
struct Config {
    // When true (default), the tracker locates the running game process,
    // reads its established server connections, and measures latency to the
    // actual server. When false, it always pings `pingHost`.
    bool autoDetectServer = true;

    // Executable name of the game process, used for server auto-detection
    // and for the game-focus input filter.
    std::string gameProcessName = "Smite2-Win64-Shipping.exe";

    // When true (default), inputs are only counted while the game window is
    // in the foreground; typing in other apps does not affect APM.
    bool onlyCountGameInput = true;

    // Fallback host pinged when the game process or its server connection
    // cannot be found (or when auto-detection is disabled).
    std::string pingHost = "1.1.1.1";

    // Interval between ping probes, in milliseconds.
    unsigned pingIntervalMs = 1000;

    // Ping timeout, in milliseconds.
    unsigned pingTimeoutMs = 1000;

    // Sliding window used to compute APM, in seconds.
    unsigned apmWindowSeconds = 60;

    // Overlay position (top-left corner), in screen pixels.
    int overlayX = 20;
    int overlayY = 20;

    // Overlay font size, in points.
    int fontSize = 16;

    // Loads configuration from `path`. Returns defaults if the file cannot
    // be opened; unknown keys are ignored, malformed values keep defaults.
    static Config load(const std::string& path);
};

}  // namespace apm
