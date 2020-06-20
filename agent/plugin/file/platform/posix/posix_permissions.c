#include "plugin/file/posix_permissions.h"
#include "common/text_util.h"
#include <sys/stat.h>

posix_permissions get_posix_permissions(const UChar* const file)
{
    char* utf8;
    posix_permissions result;
    int rc;
    struct stat stat_buf;

    memset(&result, 0, sizeof(result));
    utf8 = yella_to_utf8(file);
    rc = lstat(utf8, &stat_buf);
    free(utf8);
    if (rc == 0)
    {
        if (stat_buf.st_mode & S_IRUSR)
            result.owner_read = true;
        if (stat_buf.st_mode & S_IWUSR)
            result.owner_write = true;
        if (stat_buf.st_mode & S_IXUSR)
            result.owner_execute = true;
        if (stat_buf.st_mode & S_IRGRP)
            result.group_read = true;
        if (stat_buf.st_mode & S_IWGRP)
            result.group_write = true;
        if (stat_buf.st_mode & S_IXGRP)
            result.group_execute = true;
        if (stat_buf.st_mode & S_IROTH)
            result.other_read = true;
        if (stat_buf.st_mode & S_IWOTH)
            result.other_write = true;
        if (stat_buf.st_mode & S_IXOTH)
            result.other_execute = true;
        if (stat_buf.st_mode & S_ISUID)
            result.set_uid = true;
        if (stat_buf.st_mode & S_ISGID)
            result.set_gid = true;
        if (stat_buf.st_mode & S_ISVTX)
            result.sticky = true;
    }
    return result;
}
