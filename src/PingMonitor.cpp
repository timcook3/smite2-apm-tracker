#include "PingMonitor.h"

#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <iphlpapi.h>
#include <icmpapi.h>

#include <vector>

namespace apm {
namespace {

// Resolves `host` (name or dotted IPv4) to an IPv4 address in network byte
// order. Returns 0 on failure.
unsigned long resolveIPv4(const std::string& host) {
    addrinfo hints{};
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_RAW;

    addrinfo* result = nullptr;
    if (getaddrinfo(host.c_str(), nullptr, &hints, &result) != 0 || !result) {
        return 0;
    }
    const unsigned long address =
        reinterpret_cast<sockaddr_in*>(result->ai_addr)->sin_addr.s_addr;
    freeaddrinfo(result);
    return address;
}

}  // namespace

PingMonitor::PingMonitor(std::string host, unsigned intervalMs, unsigned timeoutMs)
    : host_(std::move(host)), intervalMs_(intervalMs), timeoutMs_(timeoutMs) {}

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

    address_ = resolveIPv4(host_);
    if (address_ == 0) {
        WSACleanup();
        return false;
    }

    icmpHandle_ = IcmpCreateFile();
    if (icmpHandle_ == INVALID_HANDLE_VALUE) {
        icmpHandle_ = nullptr;
        WSACleanup();
        return false;
    }

    stopRequested_ = false;
    thread_ = std::thread(&PingMonitor::run, this);
    return true;
}

void PingMonitor::stop() {
    {
        std::lock_guard<std::mutex> lock(stopMutex_);
        if (stopRequested_ && !thread_.joinable()) {
            return;
        }
        stopRequested_ = true;
    }
    stopCv_.notify_all();
    if (thread_.joinable()) {
        thread_.join();
    }
    if (icmpHandle_) {
        IcmpCloseHandle(icmpHandle_);
        icmpHandle_ = nullptr;
        WSACleanup();
    }
}

PingMonitor::Result PingMonitor::latest() const {
    std::lock_guard<std::mutex> lock(resultMutex_);
    return result_;
}

void PingMonitor::run() {
    for (;;) {
        const bool ok = probeOnce();
        if (!ok) {
            std::lock_guard<std::mutex> lock(resultMutex_);
            result_ = Result{};
        }
        std::unique_lock<std::mutex> lock(stopMutex_);
        if (stopCv_.wait_for(lock, std::chrono::milliseconds(intervalMs_),
                             [this] { return stopRequested_; })) {
            return;
        }
    }
}

bool PingMonitor::probeOnce() {
    static const char kPayload[] = "smite2-apm-tracker";
    std::vector<unsigned char> reply(sizeof(ICMP_ECHO_REPLY) + sizeof(kPayload) + 8);

    const DWORD count = IcmpSendEcho(
        icmpHandle_, address_,
        const_cast<char*>(kPayload), static_cast<WORD>(sizeof(kPayload)),
        nullptr, reply.data(), static_cast<DWORD>(reply.size()), timeoutMs_);
    if (count == 0) {
        return false;
    }

    const auto* echo = reinterpret_cast<const ICMP_ECHO_REPLY*>(reply.data());
    if (echo->Status != IP_SUCCESS) {
        return false;
    }

    std::lock_guard<std::mutex> lock(resultMutex_);
    result_.valid = true;
    result_.latencyMs = echo->RoundTripTime;
    return true;
}

}  // namespace apm
