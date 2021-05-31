#ifndef YELLA_POSIX_ACL_H__
#define YELLA_POSIX_ACL_H__

#include "element.h"
#include "common/ptr_vector.h"
#include <chucho/logger.h>

yella_ptr_vector* get_posix_acl(const element* elem, chucho_logger_t* lgr);

#endif

