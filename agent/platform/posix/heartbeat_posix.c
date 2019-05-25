#include "heartbeat_builder.h"
#include <chucho/log.h>
#include <sys/utsname.h>
#include <sys/socket.h>
#include <ifaddrs.h>
#include <netdb.h>
#include <errno.h>
#include <arpa/inet.h>

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
    uint32_t loopback4;
    struct in6_addr loopback6 = IN6ADDR_LOOPBACK_INIT;
    size_t good_count;
    void* cur_addr;

    loopback4 = htonl(INADDR_LOOPBACK);
    good_count = 0;
    if (getifaddrs(&addrs) == 0)
    {
        for (cur = addrs; cur != NULL; cur = cur->ifa_next)
        {
            if (cur->ifa_addr->sa_family == AF_INET)
                cur_addr = &((struct sockaddr_in*)cur->ifa_addr)->sin_addr;
            else if (cur->ifa_addr->sa_family == AF_INET6)
                cur_addr = &((struct sockaddr_in6*)cur->ifa_addr)->sin6_addr;
            else
                continue;
            if ((cur->ifa_addr->sa_family == AF_INET && memcmp(cur_addr, &loopback4, 4) != 0) ||
                (cur->ifa_addr->sa_family == AF_INET6 && memcmp(cur_addr, loopback6.__u6_addr.__u6_addr8, 16) != 0))
            {
                ++good_count;
            }
        }
        if (good_count > 0)
        {
            flatbuffers_string_vec_start(bld);
            for (cur = addrs; cur != NULL; cur = cur->ifa_next)
            {
                if (cur->ifa_addr->sa_family == AF_INET)
                    cur_addr = &((struct sockaddr_in*)cur->ifa_addr)->sin_addr;
                else if (cur->ifa_addr->sa_family == AF_INET6)
                    cur_addr = &((struct sockaddr_in6*)cur->ifa_addr)->sin6_addr;
                else
                    continue;
                if ((cur->ifa_addr->sa_family == AF_INET && memcmp(cur_addr, &loopback4, 4) != 0) ||
                    (cur->ifa_addr->sa_family == AF_INET6 && memcmp(cur_addr, loopback6.__u6_addr.__u6_addr8, 16) != 0))
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
