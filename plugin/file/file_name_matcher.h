#ifndef YELLA_FILE_NAME_MATCHER_H__
#define YELLA_FILE_NAME_MATCHER_H__

#include "export.h"
#include <unicode/utypes.h>
#include <stdbool.h>

YELLA_PRIV_EXPORT bool file_name_matches(const UChar* name, const UChar* pattern);

#endif
