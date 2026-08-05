#pragma once

#include <atomic>
#include <condition_variable>
#include <mutex>
#include <string>
#include <thread>

namespace apm {

// Periodically pings a host on a background thread and exposes the most
// recent round-trip time.
class PingMonitor {
public:
    // Latency in milliseconds, or a failure state.
    struct Result {
        bool valid = false;    // true if the last probe succeeded
        unsigned latencyMs = 0;
    };

    PingMonitor(std::string host, unsigned intervalMs, unsigned timeoutMs);
    ~PingMonitor();

    PingMonitor(const PingMonitor&) = delete;
    PingMonitor& operator=(const PingMonitor&) = delete;

    // Starts the background probing thread. Returns false if the host could
    // not be resolved or the ICMP handle could not be created.
    bool start();

    // Stops the background thread. Safe to call multiple times.
    void stop();

    // Latest probe result (thread-safe).
    Result latest() const;

private:
    void run();
    bool probeOnce();

    const std::string host_;
    const unsigned intervalMs_;
    const unsigned timeoutMs_;

    unsigned long address_ = 0;  // IPv4 address, network byte order
    void* icmpHandle_ = nullptr;

    std::thread thread_;
    std::mutex stopMutex_;
    std::condition_variable stopCv_;
    bool stopRequested_ = false;

    mutable std::mutex resultMutex_;
    Result result_;
};

}  // namespace apm
