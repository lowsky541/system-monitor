#include "network.hpp"
#include <string.h>

/**
 * Retrieve all the network interfaces whoses address family is IP (either version 4 or 6),
 * and discard all that are not.
 * The list of interfaces are sorted by interface name.
 * @return The list of interfaces from the Internet Protocol family on this machine.
 */
std::vector<Network::Interface> Network::GetInterfaces()
{
    std::vector<Network::Interface> interfaces;

    struct ifaddrs *ifap, *ifa;
    getifaddrs(&ifap);

    for (ifa = ifap; ifa; ifa = ifa->ifa_next)
    {
        if (ifa->ifa_addr)
        {
            Network::Interface interface;
            interface.name = ifa->ifa_name;
            interface.isUp = (ifa->ifa_flags & IFF_UP) != 0;

            if (ifa->ifa_addr->sa_family == AF_INET)
            {
                char addr[INET_ADDRSTRLEN];
                struct sockaddr_in *in = (struct sockaddr_in *)ifa->ifa_addr;
                inet_ntop(AF_INET, &in->sin_addr, addr, sizeof(addr));
                interface.addr = addr;
                interface.isIpv6 = false;
            }
            else if (ifa->ifa_addr->sa_family == AF_INET6)
            {
                char addr[INET6_ADDRSTRLEN];
                struct sockaddr_in6 *in6 = (struct sockaddr_in6 *)ifa->ifa_addr;
                inet_ntop(AF_INET6, &in6->sin6_addr, addr, sizeof(addr));
                interface.addr = addr;
                interface.isIpv6 = true;
            }
            else
                continue;

            interfaces.push_back(interface);
        }
    }

    std::sort(interfaces.begin(), interfaces.end(), [](const Interface &it1, const Interface &it2)
              { return it1.name < it2.name; });

    return interfaces;
}

std::map<std::string, std::array<uint32_t, 16>> Network::NetworkDevices()
{
    std::map<
        std::string,
        std::array<uint32_t, 16>>
        devices;

    char *buffer = (char *)malloc(IFNAMSIZ * sizeof(char));
    std::array<uint32_t, 16> values;

    size_t line_length = 256;
    char *line = (char *)malloc(line_length * sizeof(char));

    FILE *fp = fopen("/proc/net/dev", "r");

    getline((char **)&line, &line_length, fp);
    getline((char **)&line, &line_length, fp);

    while (getline((char **)&line, &line_length, fp) != -1)
    {
        char *ptr = strtok(line, " :");
        for (int i = 0; ptr != NULL; i++)
        {
            if (i == 0)
                strncpy(buffer, ptr, IFNAMSIZ);
            else
                values[i - 1] = strtol(ptr, NULL, 10);
            ptr = strtok(NULL, " :");
        }

        const std::string name = std::string(buffer);
        devices[name] = values;
    }

    free(line);
    free(buffer);

    fclose(fp);

    return devices;
}
