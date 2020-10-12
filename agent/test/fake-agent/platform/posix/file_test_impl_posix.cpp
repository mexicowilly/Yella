#include "file_test_impl.hpp"
#include <sys/stat.h>
#include <string.h>

namespace yella
{

namespace test
{

file_test_impl::file_type_attribute::file_type_attribute(const std::filesystem::path& file_name)
    : attribute(type::FILE_TYPE)
{
    struct stat stat_buf;
    if (lstat(file_name.c_str(), &stat_buf) != 0)
        throw std::runtime_error(std::string("Error getting file type for '") + file_name.string() + "':" + strerror(errno));
    if (S_ISREG(stat_buf.st_mode))
        ftype_ = file_type::REGULAR;
    else if (S_ISDIR(stat_buf.st_mode))
        ftype_ = file_type::DIRECTORY;
    else if (S_ISLNK(stat_buf.st_mode))
        ftype_ = file_type::SYMBOLIC_LINK;
    else if (S_ISSOCK(stat_buf.st_mode))
        ftype_ = file_type::SOCKET;
    else if (S_ISFIFO(stat_buf.st_mode))
        ftype_ = file_type::FIFO;
    else if (S_ISCHR(stat_buf.st_mode))
        ftype_ = file_type::CHARACTER_SPECIAL;
    else if (S_ISBLK(stat_buf.st_mode))
        ftype_ = file_type::BLOCK_SPECIAL;
#if defined(S_ISWHT)
    else if (S_ISWHT(stat_buf.st_mode))
        ftype_ = file_type::WHITEOUT;
#endif
}

file_test_impl::posix_permissions_attribute::posix_permissions_attribute(const std::filesystem::path& file_name)
    : attribute(type::POSIX_PERMISSIONS)
{
    struct stat stat_buf;
    if (lstat(file_name.c_str(), &stat_buf) != 0)
        throw std::runtime_error(std::string("Error getting POSIX permisions for '") + file_name.string() + "':" + strerror(errno));
    if (stat_buf.st_mode & S_IRUSR)
        bits_.set(static_cast<std::size_t>(perm::OWNER_READ));
    if (stat_buf.st_mode & S_IWUSR)
        bits_.set(static_cast<std::size_t>(perm::OWNER_WRITE));
    if (stat_buf.st_mode & S_IXUSR)
        bits_.set(static_cast<std::size_t>(perm::OWNER_EXECUTE));
    if (stat_buf.st_mode & S_IRGRP)
        bits_.set(static_cast<std::size_t>(perm::GROUP_READ));
    if (stat_buf.st_mode & S_IWGRP)
        bits_.set(static_cast<std::size_t>(perm::GROUP_WRITE));
    if (stat_buf.st_mode & S_IXGRP)
        bits_.set(static_cast<std::size_t>(perm::GROUP_EXECUTE));
    if (stat_buf.st_mode & S_IROTH)
        bits_.set(static_cast<std::size_t>(perm::OTHER_READ));
    if (stat_buf.st_mode & S_IWOTH)
        bits_.set(static_cast<std::size_t>(perm::OTHER_WRITE));
    if (stat_buf.st_mode & S_IXOTH)
        bits_.set(static_cast<std::size_t>(perm::OTHER_EXECUTE));
    if (stat_buf.st_mode & S_ISUID)
        bits_.set(static_cast<std::size_t>(perm::SET_UID));
    if (stat_buf.st_mode & S_ISGID)
        bits_.set(static_cast<std::size_t>(perm::SET_GID));
    if (stat_buf.st_mode & S_ISVTX)
        bits_.set(static_cast<std::size_t>(perm::STICKY));
}

}

}