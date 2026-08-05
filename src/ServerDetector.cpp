#include "ServerDetector.h"

#include <winsock2.h>
#include <windows.h>
#include <iphlpapi.h>
#include <tlhelp32.h>

#include <cwctype>
#include <vector>

namespace apm {
namespace {

bool equalsIgnoreCase(const std::wstring& a, const std::wstring& b) {
    if (a.size() != b.size()) {
        return false;
    }
    for (size_t i = 0; i < a.size(); ++i) {
        if (std::towlower(a[i]) != std::towlower(b[i])) {
            return false;
        }
    }
    return true;
}

// True for loopback, link-local, and RFC 1918 private ranges, which cannot
// be the game server.
bool isLocalAddress(unsigned long addr) {
    const unsigned char* b = reinterpret_cast<const unsigned char*>(&addr);
    if (b[0] == 127) return true;                       // 127.0.0.0/8
    if (b[0] == 10) return true;                        // 10.0.0.0/8
    if (b[0] == 172 && b[1] >= 16 && b[1] <= 31) return true;  // 172.16.0.0/12
    if (b[0] == 192 && b[1] == 168) return true;        // 192.168.0.0/16
    if (b[0] == 169 && b[1] == 254) return true;        // 169.254.0.0/16
    return false;
}

}  // namespace

unsigned long ServerDetector::findProcess(const std::wstring& processName) {
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snapshot == INVALID_HANDLE_VALUE) {
        return 0;
    }

    PROCESSENTRY32W entry{};
    entry.dwSize = sizeof(entry);
    unsigned long pid = 0;
    if (Process32FirstW(snapshot, &entry)) {
        do {
            if (equalsIgnoreCase(entry.szExeFile, processName)) {
                pid = entry.th32ProcessID;
                break;
            }
        } while (Process32NextW(snapshot, &entry));
    }
    CloseHandle(snapshot);
    return pid;
}

std::vector<ServerDetector::Endpoint> ServerDetector::remoteEndpoints(unsigned long pid) {
    std::vector<Endpoint> endpoints;
    if (pid == 0) {
        return endpoints;
    }

    ULONG size = 0;
    if (GetExtendedTcpTable(nullptr, &size, FALSE, AF_INET,
                            TCP_TABLE_OWNER_PID_CONNECTIONS, 0) != ERROR_INSUFFICIENT_BUFFER) {
        return endpoints;
    }
    std::vector<unsigned char> buffer(size);
    if (GetExtendedTcpTable(buffer.data(), &size, FALSE, AF_INET,
                            TCP_TABLE_OWNER_PID_CONNECTIONS, 0) != NO_ERROR) {
        return endpoints;
    }

    const auto* table = reinterpret_cast<const MIB_TCPTABLE_OWNER_PID*>(buffer.data());
    for (DWORD i = 0; i < table->dwNumEntries; ++i) {
        const MIB_TCPROW_OWNER_PID& row = table->table[i];
        if (row.dwOwningPid != pid || row.dwState != MIB_TCP_STATE_ESTAB) {
            continue;
        }
        if (isLocalAddress(row.dwRemoteAddr)) {
            continue;
        }
        Endpoint endpoint;
        endpoint.address = row.dwRemoteAddr;
        endpoint.port = ntohs(static_cast<unsigned short>(row.dwRemotePort));
        endpoints.push_back(endpoint);
    }
    return endpoints;
}

std::string ServerDetector::toString(const Endpoint& endpoint) {
    const unsigned char* b = reinterpret_cast<const unsigned char*>(&endpoint.address);
    return std::to_string(b[0]) + "." + std::to_string(b[1]) + "." +
           std::to_string(b[2]) + "." + std::to_string(b[3]) + ":" +
           std::to_string(endpoint.port);
}

}  // namespace apm
