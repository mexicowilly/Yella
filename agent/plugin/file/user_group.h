#ifndef YELLA_USER_GROUP_H__
#define YELLA_USER_GROUP_H__

#include <unicode/uchar.h>
#include <chucho/logger.h>

UChar* get_group_name(uint64_t gid, chucho_logger_t* lgr);
UChar* get_user_name(uint64_t uid, chucho_logger_t* lgr);

#endif
