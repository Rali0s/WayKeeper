#include "offgrid/network.hpp"

#if defined(__APPLE__) || defined(__unix__)
#include <arpa/inet.h>
#include <ifaddrs.h>
#include <net/if.h>
#include <netinet/in.h>
#include <sys/socket.h>
#endif

namespace offgrid {

NetworkStatus inspect_network() {
    NetworkStatus result;
#if defined(__APPLE__) || defined(__unix__)
    ifaddrs* interfaces = nullptr;
    if (getifaddrs(&interfaces) != 0) return result;
    for (auto* item = interfaces; item != nullptr; item = item->ifa_next) {
        if (!item->ifa_addr || item->ifa_addr->sa_family != AF_INET) continue;
        const auto flags = item->ifa_flags;
        if ((flags & IFF_UP) == 0 || (flags & IFF_RUNNING) == 0 || (flags & IFF_LOOPBACK) != 0) {
            continue;
        }
        char address[INET_ADDRSTRLEN]{};
        const auto* ipv4 = reinterpret_cast<const sockaddr_in*>(item->ifa_addr);
        if (!inet_ntop(AF_INET, &ipv4->sin_addr, address, sizeof(address))) continue;
        result.online = true;
        result.interface_name = item->ifa_name ? item->ifa_name : "NETWORK";
        result.address = address;
        break;
    }
    freeifaddrs(interfaces);
#endif
    return result;
}

}  // namespace offgrid
