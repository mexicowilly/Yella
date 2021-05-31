#ifndef YELLA_POSIX_PERMISSIONS_H
#define YELLA_POSIX_PERMISSIONS_H

#include "attribute.h"
#include <chucho/logger.h>

uint64_t get_access_time(void* stat_buf);
void get_group(void* stat_buf, uint64_t* id, UChar** name, chucho_logger_t* lgr);
uint64_t get_metadata_change_time(void* stat_buf);
uint64_t get_modification_time(void* stat_buf);
posix_permissions get_posix_permissions(void* stat_buf);
size_t get_size(void* stat_buf);
void get_user(void* stat_buf, uint64_t* id, UChar** name, chucho_logger_t* lgr);
void reset_access_time(const UChar* fname, void* stat_buf, chucho_logger_t* lgr);

#endif
