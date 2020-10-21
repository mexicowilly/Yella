#ifndef YELLA_POSIX_ACL_H__
#define YELLA_POSIX_ACL_H__

#include "attribute.h"
#include "common/ptr_vector.h"
#include <chucho/logger.h>

yella_ptr_vector* get_posix_acl(const UChar* const file_name, chucho_logger_t* lgr);

#endif

