#include "PingMonitor.h"

#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <iphlpapi.h>
#include <icmpapi.h>

#include <chrono>
#include <vector>

namespace apm {
namespace {

// Resolves `host` (name or dotted IPv4) to an IPv4 address in network byte
// order. Returns 0 on failure.
unsigned long resolveIPv4(const std::string& host) {
    addrinfo hints{};
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    addrinfo* result = nullptr;
    if (getaddrinfo(host.c_str(), nullptr, &hints, &result) != 0 || !result) {
        return 0;
    }
    const unsigned long address =
        reinterpret_cast<sockaddr_in*>(result->ai_addr)->sin_addr.s_addr;
    freeaddrinfo(result);
    return address;
}

std::wstring toWide(const std::string& s) {
    return std::wstring(s.begin(), s.end());
}

}  // namespace

PingMonitor::PingMonitor(const Config& config) : config_(config) {}

PingMonitor::~PingMonitor() {
    stop();
}

bool PingMonitor::start() {
    if (thread_.joinable()) {
        return false;
    }

    WSADATA wsaData{};
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        return false;
    }
    wsaInitialized_ = true;

    icmpHandle_ = IcmpCreateFile();
    if (icmpHandle_ == INVALID_HANDLE_VALUE) {
        icmpHandle_ = nullptr;
    }

    fallbackAddress_ = resolveIPv4(config_.pingHost);
    if (fallbackAddress_ == 0 && !config_.autoDetectServer) {
        stop();
        return false;
    }

    stopRequested_ = false;
    thread_ = std::thread(&PingMonitor::run, this);
    return true;
}

void PingMonitor::stop() {
    {
        std::lock_guard<std::mutex> lock(stopMutex_);
        stopRequested_ = true;
    }
    stopCv_.notify_all();
    if (thread_.joinable()) {
        thread_.join();
    }
    if (icmpHandle_) {
        IcmpCloseHandle(icmpHandle_);
        icmpHandle_ = nullptr;
    }
    if (wsaInitialized_) {
        WSACleanup();
        wsaInitialized_ = false;
    }
}

PingMonitor::Result PingMonitor::latest() const {
    std::lock_guard<std::mutex> lock(resultMutex_);
    return result_;
}

void PingMonitor::run() {
    for (;;) {
        const Result probed = probe();
        {
            std::lock_guard<std::mutex> lock(resultMutex_);
            result_ = probed;
        }
        std::unique_lock<std::mutex> lock(stopMutex_);
        if (stopCv_.wait_for(lock, std::chrono::milliseconds(config_.pingIntervalMs),
                             [this] { return stopRequested_; })) {
            return;
        }
    }
}

PingMonitor::Result PingMonitor::probe() {
    Result result;

    if (config_.autoDetectServer) {
        // Re-detect the server endpoint if we do not have one yet.
        if (!serverKnown_) {
            const unsigned long pid =
                ServerDetector::findProcess(toWide(config_.gameProcessName));
            const auto endpoints = ServerDetector::remoteEndpoints(pid);
            if (!endpoints.empty()) {
                server_ = endpoints.front();
                serverKnown_ = true;
            }
        }

        if (serverKnown_) {
            unsigned latency = 0;
            if (tcpProbe(server_, latency) || icmpProbe(server_.address, latency)) {
                result.valid = true;
                result.latencyMs = latency;
                result.source = Source::GameServer;
                return result;
            }
            // Endpoint went away (match ended, reconnect); re-detect next cycle.
            serverKnown_ = false;
        }
    }

    if (fallbackAddress_ != 0) {
        unsigned latency = 0;
        if (icmpProbe(fallbackAddress_, latency)) {
            result.valid = true;
            result.latencyMs = latency;
            result.source = Source::FallbackHost;
        }
    }
    return result;
}

bool PingMonitor::tcpProbe(const ServerDetector::Endpoint& endpoint, unsigned& latencyMs) const {
    SOCKET sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sock == INVALID_SOCKET) {
        return false;
    }

    u_long nonBlocking = 1;
    ioctlsocket(sock, FIONBIO, &nonBlocking);

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = endpoint.address;
    addr.sin_port = htons(endpoint.port);

    const auto start = std::chrono::steady_clock::now();
    connect(sock, reinterpret_cast<sockaddr*>(&addr), sizeof(addr));

    // Wait until the handshake completes (writable) or fails.
    fd_set writeSet;
    fd_set errorSet;
    FD_ZERO(&writeSet);
    FD_ZERO(&errorSet);
    FD_SET(sock, &writeSet);
    FD_SET(sock, &errorSet);
    timeval timeout{};
    timeout.tv_sec = static_cast<long>(config_.pingTimeoutMs / 1000);
    timeout.tv_usec = static_cast<long>((config_.pingTimeoutMs % 1000) * 1000);

    const int ready = select(0, nullptr, &writeSet, &errorSet, &timeout);
    const bool connected = ready > 0 && FD_ISSET(sock, &writeSet);
    const auto elapsed = std::chrono::steady_clock::now() - start;
    closesocket(sock);

    if (!connected) {
        return false;
    }
    latencyMs = static_cast<unsigned>(
        std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count());
    return true;
}

bool PingMonitor::icmpProbe(unsigned long address, unsigned& latencyMs) const {
    if (!icmpHandle_ || address == 0) {
        return false;
    }
    static const char kPayload[] = "smite2-apm-tracker";
    std::vector<unsigned char> reply(sizeof(ICMP_ECHO_REPLY) + sizeof(kPayload) + 8);

    const DWORD count = IcmpSendEcho(
        icmpHandle_, address,
        const_cast<char*>(kPayload), static_cast<WORD>(sizeof(kPayload)),
        nullptr, reply.data(), static_cast<DWORD>(reply.size()), config_.pingTimeoutMs);
    if (count == 0) {
        return false;
    }

    const auto* echo = reinterpret_cast<const ICMP_ECHO_REPLY*>(reply.data());
    if (echo->Status != IP_SUCCESS) {
        return false;
    }
    latencyMs = echo->RoundTripTime;
    return true;
}

}  // namespace apm
