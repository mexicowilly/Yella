#include "agent/mac_addresses.h"
#include <chucho/log.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <ifaddrs.h>
#include <netpacket/packet.h>
#include <assert.h>

yella_mac_addresses* yella_get_mac_addresses(chucho_logger_t* lgr)
{
    struct ifaddrs* ifap;
    struct ifaddrs* cur;
    yella_mac_addresses* result;
    int i;
    struct sockaddr_ll* sll;

    result = NULL;
    if (getifaddrs(&ifap) == 0)
    {
        result = calloc(1, sizeof(yella_mac_addresses));
        for(cur = ifap; cur != NULL; cur = cur->ifa_next)
        {
            if (cur->ifa_addr->sa_family == AF_PACKET &&
                strcmp(cur->ifa_name, "lo") != 0)
            {
                ++result->count;
            }
        }
        result->addrs = malloc(result->count * sizeof(yella_mac_address));
        for(cur = ifap, i = 0; cur != NULL; cur = cur->ifa_next)
        {
            if (cur->ifa_addr->sa_family == AF_PACKET &&
                strcmp(cur->ifa_name, "lo") != 0)
            {
                sll = (struct sockaddr_ll*)(cur->ifa_addr);
                assert(sll->sll_halen == 6);
                memcpy(result->addrs[i].addr, sll->sll_addr, 6);
                snprintf(result->addrs[i].text,
                         sizeof(result->addrs[i].text),
                         "%02x:%02x:%02x:%02x:%02x:%02x",
                         sll->sll_addr[0], sll->sll_addr[1], sll->sll_addr[2], sll->sll_addr[3], sll->sll_addr[4], sll->sll_addr[5]);
                ++i;
            }
        }
        freeifaddrs(ifap);
    }
    else
    {
        CHUCHO_C_ERROR_L(lgr,
                         "Unable to query network addresses: %s",
                         strerror(errno));
    }
    return result;
}
