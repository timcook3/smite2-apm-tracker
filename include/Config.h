#pragma once

#include <string>

namespace apm {

// Application settings, loaded from an INI-style file (key=value per line,
// '#' starts a comment). Missing file or keys fall back to defaults.
struct Config {
    // Host pinged to measure latency. Smite 2 servers cannot be discovered
    // reliably from outside the game, so this should be set to the closest
    // Hi-Rez/AWS region endpoint or any host representative of your route.
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
