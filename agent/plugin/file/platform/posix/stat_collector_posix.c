#include "plugin/file/stat_collector.h"
#include "common/text_util.h"
#include <sys/stat.h>
#include <unistd.h>
#include <pwd.h>
#include <grp.h>
#include <fcntl.h>

uint64_t get_access_time(void* stat_buf)
{
    struct stat* sb = (struct stat*)stat_buf;

    assert(stat_buf != NULL);
    return (sb->st_atim.tv_sec * 1000) + (sb->st_atim.tv_nsec / 1000000);
}

void get_group(void* stat_buf, uint64_t* id, UChar** name)
{
    struct stat* sb = (struct stat*)stat_buf;
    long sz;
    char* buf;
    struct group grp;
    struct group* result;

    assert(stat_buf != NULL);
    *id = sb->st_gid;
    sz = sysconf(_SC_GETGR_R_SIZE_MAX);
    if (sz == -1)
        sz = 1024 * 8;
    buf = malloc(sz);
    getgrgid_r(sb->st_gid, &grp, buf, sz, &result);
    if (result == NULL)
        *name = NULL;
    else
        *name = yella_from_utf8(grp.gr_name);
    free(buf);
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
        result.owner_read = true;
    if (sb->st_mode & S_IWUSR)
        result.owner_write = true;
    if (sb->st_mode & S_IXUSR)
        result.owner_execute = true;
    if (sb->st_mode & S_IRGRP)
        result.group_read = true;
    if (sb->st_mode & S_IWGRP)
        result.group_write = true;
    if (sb->st_mode & S_IXGRP)
        result.group_execute = true;
    if (sb->st_mode & S_IROTH)
        result.other_read = true;
    if (sb->st_mode & S_IWOTH)
        result.other_write = true;
    if (sb->st_mode & S_IXOTH)
        result.other_execute = true;
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

void get_user(void* stat_buf, uint64_t* id, UChar** name)
{
    struct stat* sb = (struct stat*)stat_buf;
    long sz;
    char* buf;
    struct passwd pwd;
    struct passwd* result;

    assert(stat_buf != NULL);
    *id = sb->st_uid;
    sz = sysconf(_SC_GETPW_R_SIZE_MAX);
    if (sz == -1)
        sz = 1024 * 8;
    buf = malloc(sz);
    getpwuid_r(sb->st_uid, &pwd, buf, sz, &result);
    if (result == NULL)
        *name = NULL;
    else
        *name = yella_from_utf8(pwd.pw_name);
    free(buf);
}

void reset_access_time(const UChar* fname, void* stat_buf)
{
    struct stat* sb = (struct stat*)stat_buf;
    struct timespec times[2];
    char* utf8;

    assert(stat_buf != NULL);
    times[0].tv_sec = sb->st_atim.tv_sec;
    times[0].tv_nsec = sb->st_atim.tv_nsec;
    times[1].tv_sec = 0;
    times[1].tv_nsec = UTIME_OMIT;
    utf8 = yella_to_utf8(fname);
    utimensat(0, utf8, times, AT_SYMLINK_NOFOLLOW);
    free(utf8);
}
