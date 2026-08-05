#pragma once

#include <chrono>
#include <deque>
#include <mutex>

namespace apm {

// Thread-safe sliding-window APM (actions per minute) calculator.
//
// recordAction() may be called from any thread (e.g. the input hook thread);
// currentApm() may be called concurrently from the UI thread.
class ApmCalculator {
public:
    using Clock = std::chrono::steady_clock;

    explicit ApmCalculator(std::chrono::seconds window);

    // Records one action at the current time.
    void recordAction();

    // Returns actions per minute over the configured sliding window.
    // Until a full window has elapsed since construction, the count is
    // normalized by the elapsed time instead, so early values are not
    // artificially deflated.
    double currentApm();

    // Total actions recorded since construction.
    unsigned long long totalActions();

private:
    void evict(Clock::time_point now);

    const std::chrono::seconds window_;
    const Clock::time_point start_;
    std::deque<Clock::time_point> actions_;
    unsigned long long total_ = 0;
    std::mutex mutex_;
};

}  // namespace apm
