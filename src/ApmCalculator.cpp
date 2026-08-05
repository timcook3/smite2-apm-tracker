#include "ApmCalculator.h"

namespace apm {

ApmCalculator::ApmCalculator(std::chrono::seconds window)
    : window_(window), start_(Clock::now()) {}

void ApmCalculator::recordAction() {
    std::lock_guard<std::mutex> lock(mutex_);
    const auto now = Clock::now();
    actions_.push_back(now);
    ++total_;
    evict(now);
}

double ApmCalculator::currentApm() {
    std::lock_guard<std::mutex> lock(mutex_);
    const auto now = Clock::now();
    evict(now);

    const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - start_);
    const auto windowMs = std::chrono::duration_cast<std::chrono::milliseconds>(window_);
    const double effectiveMs = static_cast<double>(
        elapsed < windowMs ? elapsed.count() : windowMs.count());
    if (effectiveMs <= 0.0) {
        return 0.0;
    }
    return static_cast<double>(actions_.size()) * 60000.0 / effectiveMs;
}

unsigned long long ApmCalculator::totalActions() {
    std::lock_guard<std::mutex> lock(mutex_);
    return total_;
}

void ApmCalculator::evict(Clock::time_point now) {
    const auto cutoff = now - window_;
    while (!actions_.empty() && actions_.front() < cutoff) {
        actions_.pop_front();
    }
}

}  // namespace apm
