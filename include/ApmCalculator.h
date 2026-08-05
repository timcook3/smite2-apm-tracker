#pragma once

#include <array>
#include <chrono>
#include <deque>
#include <mutex>

namespace apm {

// Snapshot of all tracking statistics, taken atomically.
struct ApmStats {
    bool tracking = false;
    double currentApm = 0.0;   // sliding-window APM
    double averageApm = 0.0;   // total actions over the whole session
    double peakApm = 0.0;      // highest sliding-window APM observed
    double currentEapm = 0.0;  // sliding-window effective APM
    unsigned long long totalActions = 0;
    std::chrono::seconds sessionDuration{0};
};

// Number of times each keyboard key (virtual-key code) was pressed.
using KeyCounts = std::array<unsigned long long, 256>;

// Thread-safe APM (actions per minute) tracker with session control.
//
// recordAction() may be called from any thread (e.g. the input hook thread);
// stats() and start()/stop() may be called concurrently from the UI thread.
//
// "Effective" actions (EAPM) exclude spam: a repeat of the same input within
// kEffectiveRepeatWindow does not count.
class ApmCalculator {
public:
    using Clock = std::chrono::steady_clock;

    static constexpr std::chrono::milliseconds kEffectiveRepeatWindow{500};

    explicit ApmCalculator(std::chrono::seconds window);

    // Starts a new tracking session, resetting all statistics.
    void start();

    // Stops tracking; stats are frozen and further actions are ignored
    // until start() is called again.
    void stop();

    // Records one action identified by `actionId` (virtual-key code for
    // keys) that happened `ageMs` milliseconds ago (from the input hook's
    // event timestamp, for accuracy under processing delay). Ignored while
    // stopped.
    void recordAction(int actionId, unsigned ageMs);

    // Atomically computes and returns all statistics. Peak APM is updated
    // as a side effect, so this should be called at a regular cadence.
    ApmStats stats();

    // Per-key press counts for the current session (thread-safe copy).
    KeyCounts keyCounts();

private:
    // Evicts window entries older than `now - window_`. Caller holds mutex_.
    void evict(Clock::time_point now);
    // Sliding-window rate in actions/minute. Caller holds mutex_.
    double windowRate(size_t count, Clock::time_point now) const;

    const std::chrono::seconds window_;

    std::mutex mutex_;
    bool tracking_ = false;
    Clock::time_point sessionStart_{};
    Clock::time_point sessionEnd_{};  // valid while !tracking_
    std::deque<Clock::time_point> actions_;
    std::deque<Clock::time_point> effectiveActions_;
    unsigned long long totalActions_ = 0;
    KeyCounts keyCounts_{};
    double peakApm_ = 0.0;
    int lastActionId_ = -1;
    Clock::time_point lastActionTime_{};
};

}  // namespace apm
