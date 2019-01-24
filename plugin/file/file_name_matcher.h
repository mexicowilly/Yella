#ifndef YELLA_FILE_NAME_MATCHER_H__
#define YELLA_FILE_NAME_MATCHER_H__

#include "common/uds.h"
#include <unicode/utypes.h>
#include <stdbool.h>

YELLA_PRIV_EXPORT bool file_name_matches(const UChar* name, const UChar* pattern);
YELLA_PRIV_EXPORT const UChar* first_unescaped_special_char(const UChar* const pattern);
YELLA_PRIV_EXPORT uds unescape_pattern(const UChar* const pattern);

#endif
