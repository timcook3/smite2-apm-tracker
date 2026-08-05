#pragma once

#include <string>
#include <vector>

namespace apm {

// Discovers the remote endpoints the game process is connected to, so
// latency can be measured against the actual server instead of a static
// host.
//
// Windows only exposes remote addresses for TCP connections (the UDP table
// has no remote endpoint), so detection is based on the game's established
// TCP connections. Smite 2's realtime traffic is UDP, but its TCP
// connections terminate in the same server infrastructure, which makes
// their latency representative.
class ServerDetector {
public:
    struct Endpoint {
        unsigned long address = 0;  // IPv4, network byte order
        unsigned short port = 0;    // host byte order
    };

    // Returns the PID of the first running process whose executable name
    // matches `processName` (case-insensitive), or 0 if not found.
    static unsigned long findProcess(const std::wstring& processName);

    // Returns the remote endpoints of all established TCP connections owned
    // by `pid`, excluding loopback and private-range addresses.
    static std::vector<Endpoint> remoteEndpoints(unsigned long pid);

    static std::string toString(const Endpoint& endpoint);
};

}  // namespace apm
