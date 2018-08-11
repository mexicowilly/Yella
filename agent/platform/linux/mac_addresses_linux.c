///*
// * Copyright 2016 Will Mason
// *
// *    Licensed under the Apache License, Version 2.0 (the "License");
// *    you may not use this file except in compliance with the License.
// *    You may obtain a copy of the License at
// *
// *      http://www.apache.org/licenses/LICENSE-2.0
// *
// *    Unless required by applicable law or agreed to in writing, software
// *    distributed under the License is distributed on an "AS IS" BASIS,
// *    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// *    See the License for the specific language governing permissions and
// *    limitations under the License.
// */
//
//#include "agent/mac_addresses.h"
//#include <chucho/log.h>
//#include <sys/ioctl.h>
//#include <net/if.h>
//#include <netinet/in.h>
//#include <memory.h>
//#include <unistd.h>
//#include <errno.h>
//#include <assert.h>
//#include <stdio.h>
//
//yella_mac_addresses* yella_get_mac_addresses(void)
//{
//    yella_mac_addresses* result;
//    struct ifreq ifr;
//    struct ifconf ifc;
//    char buf[1024];
//    unsigned i = 0;
//    uint8_t* cur_addr;
//
//    result = malloc(sizeof(yella_mac_addresses));
//    memset(&result, 0, sizeof(result));
//    int sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);
//    if (sock == -1)
//    {
//        CHUCHO_C_ERROR("yella.agent",
//                       "Error opening socket to get MAC addresses: %s",
//                       strerror(errno));
//        return result;
//    }
//    ifc.ifc_len = sizeof(buf);
//    ifc.ifc_buf = buf;
//    if (ioctl(sock, SIOCGIFCONF, &ifc) == -1)
//    {
//        CHUCHO_C_ERROR("yella.agent",
//                       "Error retrieving address information: %s",
//                       strerror(errno));
//        return result;
//    }
//    struct ifreq* it = ifc.ifc_req;
//    const struct ifreq* const end = it + (ifc.ifc_len / sizeof(struct ifreq));
//    if (end - it > 1)
//    {
//        result->addrs = malloc((end - it - 1) * sizeof(yella_mac_address));
//        for (; it != end; ++it)
//        {
//            strcpy(ifr.ifr_name, it->ifr_name);
//            if (ioctl(sock, SIOCGIFFLAGS, &ifr) == 0)
//            {
//                if ((ifr.ifr_flags & IFF_LOOPBACK) == 0)
//                {
//                    if (ioctl(sock, SIOCGIFHWADDR, &ifr) == 0)
//                    {
//                        cur_addr = result->addrs[i].addr;
//                        memcpy(cur_addr, ifr.ifr_hwaddr.sa_data, 6);
//                        snprintf(result->addrs[i].text,
//                                 sizeof(result->addrs[i++].text),
//                                 "%02x:%02x:%02x:%02x:%02x:%02x",
//                                 cur_addr[0], cur_addr[1], cur_addr[2], cur_addr[3], cur_addr[4], cur_addr[5]);
//                    }
//                    else
//                    {
//                        CHUCHO_C_ERROR("yella.agent",
//                                       "Error retrieving socket hardware information, so this interface is being skipped: %s",
//                                       strerror(errno));
//                    }
//                }
//            }
//            else
//            {
//                CHUCHO_C_ERROR("yella.agent",
//                               "Error retrieving socket flag information, so this interface is being skipped: %s",
//                               strerror(errno));
//            }
//        }
//    }
//    else
//    {
//        CHUCHO_C_ERROR("yella.agent",
//                       "No interfaces with MAC addresses are present");
//    }
//    close(sock);
//    result->count = i;
//    return result;
//}

#include "agent/mac_addresses.h"
#include <chucho/log.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
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
