#include "plugin/file/user_group.h"
#include "common/text_util.h"
#include <chucho/log.h>
#include <unistd.h>
#include <stdlib.h>
#include <grp.h>
#include <pwd.h>
#include <errno.h>
#include <string.h>
#include <inttypes.h>

UChar* get_group_name(uint64_t gid, chucho_logger_t* lgr)
{
    long sz;
    char* buf;
    struct group grp;
    struct group* grp_result;
    UChar* result;

    sz = sysconf(_SC_GETGR_R_SIZE_MAX);
    if (sz == -1)
        sz = 1024 * 8;
    buf = malloc(sz);
    errno = 0;
    getgrgid_r(gid, &grp, buf, sz, &grp_result);
    if (grp_result == NULL)
    {
        CHUCHO_C_ERROR(lgr, "Error getting group name for '%" PRIu64 "': %s", gid, strerror(errno));
        result = malloc(sizeof(UChar));
        result[0] = 0;
    }
    else
    {
        result = yella_from_utf8(grp.gr_name);
    }
    free(buf);
    return result;
}

UChar* get_user_name(uint64_t uid, chucho_logger_t* lgr)
{
    long sz;
    char* buf;
    struct passwd pwd;
    struct passwd* pwd_result;
    UChar* result;

    sz = sysconf(_SC_GETPW_R_SIZE_MAX);
    if (sz == -1)
        sz = 1024 * 8;
    buf = malloc(sz);
    errno = 0;
    getpwuid_r(uid, &pwd, buf, sz, &pwd_result);
    if (pwd_result == NULL)
    {
        CHUCHO_C_ERROR(lgr, "Error getting user name for '%" PRIu64 "': %s", uid, strerror(errno));
        result = malloc(sizeof(UChar));
        result[0] = 0;
    }
    else
    {
        result = yella_from_utf8(pwd.pw_name);
    }
    free(buf);
    return result;
}
