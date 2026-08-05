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

void ApmCalculator::recordAction(int actionId) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!tracking_) {
        return;
    }
    const auto now = Clock::now();
    actions_.push_back(now);
    ++totalActions_;

    const bool repeat =
        actionId == lastActionId_ && (now - lastActionTime_) < kEffectiveRepeatWindow;
    if (!repeat) {
        effectiveActions_.push_back(now);
    }
    lastActionId_ = actionId;
    lastActionTime_ = now;

    evict(now);
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
    return stats;
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

double ApmCalculator::windowRate(size_t count, Clock::time_point now) const {
    const auto elapsed =
        std::chrono::duration_cast<std::chrono::milliseconds>(now - sessionStart_);
    const auto windowMs = std::chrono::duration_cast<std::chrono::milliseconds>(window_);
    const double effectiveMs =
        static_cast<double>(elapsed < windowMs ? elapsed.count() : windowMs.count());
    if (effectiveMs <= 0.0) {
        return 0.0;
    }
    return static_cast<double>(count) * 60000.0 / effectiveMs;
}

}  // namespace apm
