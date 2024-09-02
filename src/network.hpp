#ifndef __IG_NETWORK_HPP__
#define __IG_NETWORK_HPP__

#include <string>
#include <vector>
#include <algorithm>
#include <map>
#include <array>

#if defined(_WIN32) || defined(_WIN64)
#else
    #include <sys/socket.h>
    #include <sys/types.h>
    #include <netinet/in.h>
    #include <arpa/inet.h>
    #include <ifaddrs.h>
    #include <net/if.h>
#endif

namespace Network
{
    enum
    {
        RX_BYTES,
        RX_PACKETS,
        RX_ERRS,
        RX_DROP,
        RX_FIFO,
        RX_FRAME,
        RX_COMPRESSED,
        RX_MULTICAST,
        TX_BYTES,
        TX_PACKETS,
        TX_ERRS,
        TX_DROP,
        TX_FIFO,
        TX_COLLS,
        TX_CARRIER,
        TX_COMPRESSED
    };

    struct Interface
    {
        std::string name;
        std::string addr;
        bool isUp;
        bool isIpv6;
    };

    std::vector<Interface> GetInterfaces();
    std::map<std::string, std::array<uint32_t, 16>> NetworkDevices();
}

#endif