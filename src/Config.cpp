#include "Config.h"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <sstream>

namespace apm {
namespace {

std::string trim(const std::string& s) {
    const auto begin = std::find_if_not(s.begin(), s.end(), [](unsigned char c) { return std::isspace(c); });
    const auto end = std::find_if_not(s.rbegin(), s.rend(), [](unsigned char c) { return std::isspace(c); }).base();
    return (begin < end) ? std::string(begin, end) : std::string();
}

bool parseUnsigned(const std::string& value, unsigned& out) {
    try {
        const unsigned long parsed = std::stoul(value);
        if (parsed == 0 || parsed > 3600000UL) {
            return false;
        }
        out = static_cast<unsigned>(parsed);
        return true;
    } catch (...) {
        return false;
    }
}

bool parseInt(const std::string& value, int& out) {
    try {
        out = std::stoi(value);
        return true;
    } catch (...) {
        return false;
    }
}

}  // namespace

Config Config::load(const std::string& path) {
    Config config;
    std::ifstream file(path);
    if (!file.is_open()) {
        return config;
    }

    std::string line;
    while (std::getline(file, line)) {
        const auto comment = line.find('#');
        if (comment != std::string::npos) {
            line.erase(comment);
        }
        const auto eq = line.find('=');
        if (eq == std::string::npos) {
            continue;
        }
        const std::string key = trim(line.substr(0, eq));
        const std::string value = trim(line.substr(eq + 1));
        if (key.empty() || value.empty()) {
            continue;
        }

        if (key == "ping_host") {
            config.pingHost = value;
        } else if (key == "ping_interval_ms") {
            parseUnsigned(value, config.pingIntervalMs);
        } else if (key == "ping_timeout_ms") {
            parseUnsigned(value, config.pingTimeoutMs);
        } else if (key == "apm_window_seconds") {
            parseUnsigned(value, config.apmWindowSeconds);
        } else if (key == "overlay_x") {
            parseInt(value, config.overlayX);
        } else if (key == "overlay_y") {
            parseInt(value, config.overlayY);
        } else if (key == "font_size") {
            int size = 0;
            if (parseInt(value, size) && size >= 8 && size <= 72) {
                config.fontSize = size;
            }
        }
    }
    return config;
}

}  // namespace apm
