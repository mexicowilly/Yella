#include "plugin/file/stat_collector.h"
#include "plugin/file/user_group.h"
#include "common/text_util.h"
#include <chucho/log.h>
#include <sys/stat.h>
#include <unistd.h>
#include <pwd.h>
#include <grp.h>
#include <fcntl.h>
#include <errno.h>

uint64_t get_access_time(void* stat_buf)
{
    struct stat* sb = (struct stat*)stat_buf;

    assert(stat_buf != NULL);
    return (sb->st_atim.tv_sec * 1000) + (sb->st_atim.tv_nsec / 1000000);
}

void get_group(void* stat_buf, uint64_t* id, UChar** name, chucho_logger_t* lgr)
{
    struct stat* sb = (struct stat*)stat_buf;

    assert(stat_buf != NULL);
    *id = sb->st_gid;
    *name = get_group_name(*id, lgr);
}

uint64_t get_metadata_change_time(void* stat_buf)
{
    struct stat* sb = (struct stat*)stat_buf;

    assert(stat_buf != NULL);
    return (sb->st_ctim.tv_sec * 1000) + (sb->st_ctim.tv_nsec / 1000000);
}

uint64_t get_modification_time(void* stat_buf)
{
    struct stat* sb = (struct stat*)stat_buf;

    assert(stat_buf != NULL);
    return (sb->st_mtim.tv_sec * 1000) + (sb->st_mtim.tv_nsec / 1000000);
}

posix_permissions get_posix_permissions(void* stat_buf)
{
    posix_permissions result;
    struct stat* sb = (struct stat*)stat_buf;

    assert(stat_buf != NULL);
    memset(&result, 0, sizeof(result));
    if (sb->st_mode & S_IRUSR)
        result.owner.read = true;
    if (sb->st_mode & S_IWUSR)
        result.owner.write = true;
    if (sb->st_mode & S_IXUSR)
        result.owner.execute = true;
    if (sb->st_mode & S_IRGRP)
        result.group.read = true;
    if (sb->st_mode & S_IWGRP)
        result.group.write = true;
    if (sb->st_mode & S_IXGRP)
        result.group.execute = true;
    if (sb->st_mode & S_IROTH)
        result.other.read = true;
    if (sb->st_mode & S_IWOTH)
        result.other.write = true;
    if (sb->st_mode & S_IXOTH)
        result.other.execute = true;
    if (sb->st_mode & S_ISUID)
        result.set_uid = true;
    if (sb->st_mode & S_ISGID)
        result.set_gid = true;
    if (sb->st_mode & S_ISVTX)
        result.sticky = true;
    return result;
}

size_t get_size(void* stat_buf)
{
    struct stat* sb = (struct stat*)stat_buf;

    assert(stat_buf != NULL);
    return sb->st_size;
}

void get_user(void* stat_buf, uint64_t* id, UChar** name, chucho_logger_t* lgr)
{
    struct stat* sb = (struct stat*)stat_buf;

    assert(stat_buf != NULL);
    *id = sb->st_uid;
    *name = get_user_name(*id, lgr);
}

void reset_access_time(const UChar* fname, void* stat_buf, chucho_logger_t* lgr)
{
    struct stat* sb = (struct stat*)stat_buf;
    struct timespec times[2];
    char* utf8;
    int rc;

    assert(stat_buf != NULL);
    times[0].tv_sec = sb->st_atim.tv_sec;
    times[0].tv_nsec = sb->st_atim.tv_nsec;
    times[1].tv_sec = 0;
    times[1].tv_nsec = UTIME_OMIT;
    utf8 = yella_to_utf8(fname);
    if (utimensat(0, utf8, times, AT_SYMLINK_NOFOLLOW) != 0)
        CHUCHO_C_ERROR(lgr, "Error resetting access time for '%s': %s", utf8, strerror(errno));
    free(utf8);
}
