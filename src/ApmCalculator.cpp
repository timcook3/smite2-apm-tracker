#include "ApmCalculator.h"

#include <algorithm>

namespace apm {

ApmCalculator::ApmCalculator(std::chrono::seconds window) : window_(window) {}

void ApmCalculator::start() {
    std::lock_guard<std::mutex> lock(mutex_);
    tracking_ = true;
    sessionStart_ = Clock::now();
    sessionEnd_ = sessionStart_;
    actions_.clear();
    effectiveActions_.clear();
    totalActions_ = 0;
    keyCounts_.fill(0);
    peakApm_ = 0.0;
    lastActionId_ = -1;
    lastActionTime_ = Clock::time_point{};
}

void ApmCalculator::stop() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (tracking_) {
        tracking_ = false;
        sessionEnd_ = Clock::now();
    }
}

void ApmCalculator::recordAction(int actionId, unsigned ageMs) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!tracking_) {
        return;
    }
    // Reconstruct when the event physically happened, clamped into the
    // session and kept monotonic so the deques stay sorted no matter what
    // the reported age is.
    auto eventTime = Clock::now() - std::chrono::milliseconds(ageMs);
    if (eventTime < sessionStart_) {
        eventTime = sessionStart_;
    }
    if (!actions_.empty() && eventTime < actions_.back()) {
        eventTime = actions_.back();
    }
    actions_.push_back(eventTime);
    ++totalActions_;
    if (actionId >= 0 && actionId < static_cast<int>(keyCounts_.size())) {
        ++keyCounts_[static_cast<size_t>(actionId)];
    }

    const bool repeat = actionId == lastActionId_ &&
                        (eventTime - lastActionTime_) < kEffectiveRepeatWindow;
    if (!repeat) {
        effectiveActions_.push_back(eventTime);
    }
    lastActionId_ = actionId;
    lastActionTime_ = eventTime;

    evict(Clock::now());
}

ApmStats ApmCalculator::stats() {
    std::lock_guard<std::mutex> lock(mutex_);
    ApmStats stats;
    stats.tracking = tracking_;
    stats.totalActions = totalActions_;

    if (sessionStart_ == Clock::time_point{}) {
        return stats;  // never started
    }

    const auto now = tracking_ ? Clock::now() : sessionEnd_;
    if (tracking_) {
        evict(now);
    }

    stats.currentApm = windowRate(actions_.size(), now);
    stats.currentEapm = windowRate(effectiveActions_.size(), now);

    const auto sessionMs =
        std::chrono::duration_cast<std::chrono::milliseconds>(now - sessionStart_).count();
    if (sessionMs > 0) {
        stats.averageApm = static_cast<double>(totalActions_) * 60000.0 /
                           static_cast<double>(sessionMs);
    }

    if (tracking_) {
        peakApm_ = std::max(peakApm_, stats.currentApm);
    }
    stats.peakApm = peakApm_;
    stats.sessionDuration =
        std::chrono::duration_cast<std::chrono::seconds>(now - sessionStart_);
    return stats;
}

KeyCounts ApmCalculator::keyCounts() {
    std::lock_guard<std::mutex> lock(mutex_);
    return keyCounts_;
}

void ApmCalculator::evict(Clock::time_point now) {
    const auto cutoff = now - window_;
    while (!actions_.empty() && actions_.front() < cutoff) {
        actions_.pop_front();
    }
    while (!effectiveActions_.empty() && effectiveActions_.front() < cutoff) {
        effectiveActions_.pop_front();
    }
}

double ApmCalculator::windowRate(size_t count, Clock::time_point) const {
    // True rolling-window rate: actions in the last `window_`, normalized to
    // one minute. No extrapolation from short elapsed times — extrapolating
    // (count / elapsed) early in a session produces wild spikes (5 actions in
    // 2 s would read 150 APM) and pollutes peak APM.
    const auto windowMs = std::chrono::duration_cast<std::chrono::milliseconds>(window_);
    if (windowMs.count() <= 0) {
        return 0.0;
    }
    return static_cast<double>(count) * 60000.0 /
           static_cast<double>(windowMs.count());
}

}  // namespace apm
