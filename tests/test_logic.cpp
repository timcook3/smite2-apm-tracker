// Native logic tests for Config and ApmCalculator (buildable on any OS).
//
//   g++ -std=c++17 -Wall -Wextra -Iinclude tests/test_logic.cpp
//       src/Config.cpp src/ApmCalculator.cpp -o test_logic -pthread
// (single command line)
#include <cassert>
#include <cstdio>
#include <fstream>
#include <thread>

#include "ApmCalculator.h"
#include "Config.h"

using namespace std::chrono_literals;

static void testConfig() {
    apm::Config d;
    assert(d.autoDetectServer && d.gameProcessName == "Smite2-Win64-Shipping.exe");
    assert(d.onlyCountGameInput);
    assert(d.pingHost == "1.1.1.1" && d.apmWindowSeconds == 60);

    {
        std::ofstream f("/tmp/apm_test_config.ini");
        f << "# comment\n"
          << "auto_detect_server = false\n"
          << "only_count_game_input = 0\n"
          << "game_process_name = Custom.exe\n"
          << "ping_host = 8.8.8.8\n"
          << "apm_window_seconds = 30\n"
          << "font_size = nonsense\n";
    }
    const apm::Config c = apm::Config::load("/tmp/apm_test_config.ini");
    assert(!c.autoDetectServer && !c.onlyCountGameInput);
    assert(c.gameProcessName == "Custom.exe" && c.pingHost == "8.8.8.8");
    assert(c.apmWindowSeconds == 30);
    assert(c.fontSize == apm::Config{}.fontSize);  // malformed -> default
}

static void testApm() {
    apm::ApmCalculator calc{std::chrono::seconds(60)};

    // Actions before start() are ignored.
    calc.recordAction('A', 0);
    auto s0 = calc.stats();
    assert(!s0.tracking && s0.totalActions == 0 && s0.currentApm == 0.0);

    // Distinct actions all count toward APM and EAPM equally.
    calc.start();
    for (int i = 0; i < 10; ++i) {
        calc.recordAction(i, 0);
    }
    std::this_thread::sleep_for(50ms);
    auto s1 = calc.stats();
    assert(s1.tracking && s1.totalActions == 10);
    assert(s1.currentApm > 0.0);
    assert(s1.currentEapm == s1.currentApm);

    // Rapid repeats of the same input count toward APM but not EAPM.
    calc.start();
    for (int i = 0; i < 5; ++i) {
        calc.recordAction('Q', 0);
    }
    std::this_thread::sleep_for(50ms);
    auto s2 = calc.stats();
    assert(s2.totalActions == 5);
    assert(s2.currentEapm < s2.currentApm);

    // Stop freezes stats and ignores further actions.
    calc.stop();
    auto frozen = calc.stats();
    calc.recordAction('W', 0);
    std::this_thread::sleep_for(50ms);
    auto after = calc.stats();
    assert(!after.tracking);
    assert(after.totalActions == frozen.totalActions);
    assert(after.currentApm == frozen.currentApm);
    assert(after.peakApm == frozen.peakApm);

    // Restarting clears the previous session.
    calc.start();
    auto s3 = calc.stats();
    assert(s3.tracking && s3.totalActions == 0 && s3.peakApm == 0.0);
}

static void testAccuracy() {
    apm::ApmCalculator calc{std::chrono::seconds(60)};
    calc.start();

    // Key counts accumulate per virtual-key code; mouse (>=256) not counted.
    calc.recordAction('W', 0);
    calc.recordAction('W', 0);
    calc.recordAction('A', 0);
    calc.recordAction(300, 0);  // mouse button
    auto counts = calc.keyCounts();
    assert(counts['W'] == 2 && counts['A'] == 1);

    // ageMs backdates events.
    std::this_thread::sleep_for(300ms);
    calc.recordAction('S', 250);
    auto s = calc.stats();
    assert(s.totalActions == 5);

    // Rolling window: N distinct actions in the window read exactly N APM
    // (no extrapolation from short elapsed times).
    assert(s.currentApm == 5.0);

    // Session duration advances while tracking, freezes on stop.
    std::this_thread::sleep_for(1100ms);
    auto s1 = calc.stats();
    assert(s1.sessionDuration.count() >= 1);
    calc.stop();
    auto frozen = calc.stats();
    std::this_thread::sleep_for(200ms);
    assert(calc.stats().sessionDuration == frozen.sessionDuration);

    // A huge (bogus) age is clamped into the session instead of backdating
    // the event out of the rolling window.
    calc.start();
    calc.recordAction('Q', 4000000000u);
    std::this_thread::sleep_for(50ms);
    auto sc = calc.stats();
    assert(sc.currentApm > 0.0);

    // start() resets key counts.
    calc.start();
    assert(calc.keyCounts()['W'] == 0);
}

int main() {
    testConfig();
    testApm();
    testAccuracy();
    std::puts("all tests passed");
    return 0;
}
