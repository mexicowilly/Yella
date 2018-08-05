#include "agent/mac_addresses.h"
#include <chucho/log.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <net/if_dl.h>
#include <ifaddrs.h>

yella_mac_addresses* yella_get_mac_addresses(void)
{
    struct ifaddrs* ifap;
    struct ifaddrs* cur;
    yella_mac_addresses* result;
    uint8_t* addr;
    int i;

    result = NULL;
    if (getifaddrs(&ifap) == 0)
    {
        result = calloc(1, sizeof(yella_mac_addresses));
        for(cur = ifap; cur != NULL; cur = cur->ifa_next)
        {
            if (cur->ifa_addr->sa_family == AF_LINK &&
                strncmp(cur->ifa_name, "lo", 2) != 0)
            {
                ++result->count;
            }
        }
        result->addrs = malloc(result->count * sizeof(yella_mac_address));
        for(cur = ifap, i = 0; cur != NULL; cur = cur->ifa_next, i++)
        {
            if (cur->ifa_addr->sa_family == AF_LINK &&
                strncmp(cur->ifa_name, "lo", 2) != 0)
            {
                addr = (uint8_t*)LLADDR((struct sockaddr_dl*)cur->ifa_addr);
                memcpy(result->addrs[i].addr, addr, 6);
                snprintf(result->addrs[i].text,
                         sizeof(result->addrs[i].text),
                         "%02x:%02x:%02x:%02x:%02x:%02x",
                         addr[0], addr[1], addr[2], addr[3], addr[4], addr[5]);
            }
        }
        freeifaddrs(ifap);
    }
    else
    {
        CHUCHO_C_ERROR("yella.agent",
                       "Unable to query network addresses: %s",
                       strerror(errno));
    }
    return result;
}