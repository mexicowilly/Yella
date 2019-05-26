#include "heartbeat_builder.h"
#include <chucho/log.h>
#include <sys/utsname.h>
#include <sys/socket.h>
#include <ifaddrs.h>
#include <netdb.h>
#include <errno.h>
#include <arpa/inet.h>
#include <stdbool.h>

/* Returns NULL if address is not accetable, or struct sockaddr_in.sin_addr for
 * IPv4, or struct sockaddr_in6.sin6_addr for IPv6. */
static void* acceptable_addr(const struct sockaddr* addr)
{
    void* result;
    uint32_t loopback4;
    struct in6_addr loopback6 = IN6ADDR_LOOPBACK_INIT;
    struct sockaddr_in* v4;
    struct sockaddr_in6* v6;
    uint32_t hv4_addr;
    uint32_t llv4_start;
    uint32_t llv4_end;
    uint16_t llv6_start;
    uint16_t llv6_end;
    uint16_t hv6_top;

    result = NULL;
    if (addr->sa_family == AF_INET)
    {
        v4 = (struct sockaddr_in*)addr;
        loopback4 = htonl(INADDR_LOOPBACK);
        if (memcmp(&v4->sin_addr, &loopback4, 4) != 0)
        {
            hv4_addr = ntohl(v4->sin_addr.s_addr);
            llv4_start = ntohs(0xa9fe) << 16;
            llv4_end = ntohl(0xa9feffff);
            if (hv4_addr < llv4_start || hv4_addr > llv4_end)
                result = &v4->sin_addr;
        }
    }
    else if (addr->sa_family == AF_INET6)
    {
        v6 = (struct sockaddr_in6*)addr;
        if (memcmp(&v6->sin6_addr, loopback6.s6_addr, 16) != 0)
        {
            hv6_top = *((uint16_t*)(v6->sin6_addr.s6_addr));
            llv6_start = ntohs(0xfe80);
            llv6_end = ntohs(0xfec0);
            if (hv6_top < llv6_start || hv6_top >= llv6_end)
                result = &v6->sin6_addr;
        }
    }
    return result;
}

static void set_host_name(flatcc_builder_t* bld, const char* const node_name)
{
    struct addrinfo hints;
    struct addrinfo* info;
    int rc;

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = PF_UNSPEC;
    hints.ai_flags = AI_CANONNAME;
    rc = getaddrinfo(node_name, NULL, &hints, &info);
    if (rc == 0)
    {
        yella_fb_heartbeat_host_create_str(bld, info->ai_canonname);
        freeaddrinfo(info);
    }
    else
    {
        CHUCHO_C_INFO("yella.agent", "Cound not get fully qualified host name: %s", gai_strerror(rc));
    }
}

static void set_ip_addresses(flatcc_builder_t* bld)
{
    struct ifaddrs* addrs;
    struct ifaddrs* cur;
    char text[NI_MAXHOST];
    bool got_one;
    void* cur_addr;

    if (getifaddrs(&addrs) == 0)
    {
        got_one = false;
        for (cur = addrs; cur != NULL; cur = cur->ifa_next)
        {
            if (acceptable_addr(cur->ifa_addr) != NULL)
            {
                got_one = true;
                break;
            }
        }
        if (got_one)
        {
            flatbuffers_string_vec_start(bld);
            for (cur = addrs; cur != NULL; cur = cur->ifa_next)
            {
                cur_addr = acceptable_addr(cur->ifa_addr);
                if (cur_addr != NULL)
                {
                    inet_ntop(cur->ifa_addr->sa_family, cur_addr, text, INET6_ADDRSTRLEN);
                    flatbuffers_string_vec_push(bld, flatbuffers_string_create_str(bld, text));
                }
            }
            yella_fb_heartbeat_ip_addresses_add(bld, flatbuffers_string_vec_end(bld));
        }
        freeifaddrs(addrs);
    }
    else
    {
        CHUCHO_C_INFO("yella.agent", "Unable to get address information: %s", strerror(errno));
    }
}

void set_host(flatcc_builder_t* bld)
{
    struct utsname info;

    uname(&info);
    set_host_name(bld, info.nodename);
    set_ip_addresses(bld);
    yella_fb_operating_system_start(bld);
    yella_fb_operating_system_machine_create_str(bld, info.machine);
    yella_fb_operating_system_system_create_str(bld, info.sysname);
    yella_fb_operating_system_version_create_str(bld, info.version);
    yella_fb_operating_system_release_create_str(bld, info.release);
    yella_fb_heartbeat_os_add(bld, yella_fb_operating_system_end(bld));
}
