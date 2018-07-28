#include "heartbeat_builder.h"
#include <sys/utsname.h>

void set_host(flatcc_builder_t* bld)
{
    struct utsname info;

    uname(&info);
    yella_fb_operating_system_start(bld);
    yella_fb_operating_system_machine_create_str(bld, info.machine);
    yella_fb_operating_system_system_create_str(bld, info.sysname);
    yella_fb_operating_system_version_create_str(bld, info.version);
    yella_fb_operating_system_release_create_str(bld, info.release);
    yella_fb_heartbeat_host_add(bld, yella_fb_operating_system_end(bld));
}
