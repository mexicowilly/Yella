#ifndef YELLA_POSIX_PERMISSIONS_H
#define YELLA_POSIX_PERMISSIONS_H

#include "attribute.h"
#include <unicode/ustring.h>

/* There is no error handling in here, because on Posix we have just
 * moments earlier called lstat with proper error handling in order
 * to get the file type. */
posix_permissions get_posix_permissions(const UChar* const file);

#endif
