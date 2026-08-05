#pragma once

#include <condition_variable>
#include <mutex>
#include <string>
#include <thread>

#include "Config.h"
#include "ServerDetector.h"

namespace apm {

// Measures latency on a background thread and exposes the most recent
// round-trip time.
//
// In "auto" mode it locates the game process, reads its established remote
// endpoints, and measures RTT to the actual server: first with a TCP
// handshake probe against the server's own port (no privileges needed),
// falling back to an ICMP echo to the server's address. If the game is not
// running, it falls back to the configured host.
//
// In "host" mode it always ICMP-pings the configured host.
class PingMonitor {
public:
    // How the latest latency value was obtained.
    enum class Source {
        None,          // no successful probe yet
        GameServer,    // measured against a detected game-server endpoint
        FallbackHost,  // measured against the configured ping_host
    };

    struct Result {
        bool valid = false;
        unsigned latencyMs = 0;
        Source source = Source::None;
    };

    explicit PingMonitor(const Config& config);
    ~PingMonitor();

    PingMonitor(const PingMonitor&) = delete;
    PingMonitor& operator=(const PingMonitor&) = delete;

    // Starts the background probing thread. Returns false if networking
    // could not be initialized.
    bool start();

    // Stops the background thread. Safe to call multiple times.
    void stop();

    // Latest probe result (thread-safe).
    Result latest() const;

private:
    void run();
    // One measurement cycle; returns the result to publish.
    Result probe();
    // TCP handshake RTT to `endpoint`; returns false on failure.
    bool tcpProbe(const ServerDetector::Endpoint& endpoint, unsigned& latencyMs) const;
    // ICMP echo RTT to `address` (IPv4, network byte order).
    bool icmpProbe(unsigned long address, unsigned& latencyMs) const;

    const Config config_;

    unsigned long fallbackAddress_ = 0;  // resolved ping_host
    void* icmpHandle_ = nullptr;
    bool wsaInitialized_ = false;

    // Cached detected server endpoint; re-detected when probing it fails.
    ServerDetector::Endpoint server_;
    bool serverKnown_ = false;

    std::thread thread_;
    std::mutex stopMutex_;
    std::condition_variable stopCv_;
    bool stopRequested_ = false;

    mutable std::mutex resultMutex_;
    Result result_;
};

}  // namespace apm
