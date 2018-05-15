/*
 * Copyright 2016 Will Mason
 *
 *    Licensed under the Apache License, Version 2.0 (the "License");
 *    you may not use this file except in compliance with the License.
 *    You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 *    Unless required by applicable law or agreed to in writing, software
 *    distributed under the License is distributed on an "AS IS" BASIS,
 *    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *    See the License for the specific language governing permissions and
 *    limitations under the License.
 */

#include "agent/mac_addresses.h"
#include <chucho/log.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <netinet/in.h>
#include <memory.h>
#include <unistd.h>
#include <errno.h>
#include <assert.h>

yella_mac_addresses* yella_get_mac_addresses(void)
{
    yella_mac_addresses* result;
    struct ifreq ifr;
    struct ifconf ifc;
    char buf[1024];
    unsigned i = 0;

    result = malloc(sizeof(yella_mac_addresses));
    memset(&result, 0, sizeof(result));
    int sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);
    if (sock == -1)
    {
        CHUCHO_C_ERROR("yella",
                       "Error opening socket to get MAC addresses: %s",
                       strerror(errno));
        return result;
    }
    ifc.ifc_len = sizeof(buf);
    ifc.ifc_buf = buf;
    if (ioctl(sock, SIOCGIFCONF, &ifc) == -1)
    {
        CHUCHO_C_ERROR("yella",
                       "Error retrieving address information: %s",
                       strerror(errno));
        return result;
    }
    struct ifreq* it = ifc.ifc_req;
    const struct ifreq* const end = it + (ifc.ifc_len / sizeof(struct ifreq));
    if (end - it > 1)
    {
        result->addrs = calloc(end - it - 1, sizeof(uint64_t));
        for (; it != end; ++it)
        {
            strcpy(ifr.ifr_name, it->ifr_name);
            if (ioctl(sock, SIOCGIFFLAGS, &ifr) == 0)
            {
                if ((ifr.ifr_flags & IFF_LOOPBACK) == 0)
                {
                    if (ioctl(sock, SIOCGIFHWADDR, &ifr) == 0)
                    {
                        memcpy(&result->addrs[i++], ifr.ifr_hwaddr.sa_data, 6);
                    }
                    else
                    {
                        CHUCHO_C_ERROR("yella",
                                       "Error retrieving socket hardware information, so this interface is being skipped: %s",
                                       strerror(errno));
                    }
                }
            }
            else
            {
                CHUCHO_C_ERROR("yella",
                               "Error retrieving socket flag information, so this interface is being skipped: %s",
                               strerror(errno));
            }
        }
    }
    else
    {
        CHUCHO_C_ERROR("yella",
                       "No interfaces with MAC addresses are present");
    }
    close(sock);
    result->count = i;
    return result;
}
