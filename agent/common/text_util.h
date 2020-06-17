/*
 * Copyright 2016 Will Mason
 * 
 *    Licensed under the Apache License, Version 2.0 (the "License");
 *    you may not use this file except in compliance with the License.
 *    You may obtain a copy of the License at
 * 
 *      http://www.apache.org/licenses/LICENSE-2.0
 * 
 *    Unless required by applicable law or agreed to in writing, software
 *    distributed under the License is distributed on an "AS IS" BASIS,
 *    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *    See the License for the specific language governing permissions and
 *    limitations under the License.
 */

#if !defined(TEXT_UTIL_H__)
#define TEXT_UTIL_H__

#include "export.h"
#include <unicode/ustring.h>

YELLA_EXPORT extern const UChar* YELLA_NL;

YELLA_EXPORT UChar* yella_from_utf8(const char* const str);
/* Don't use this when you need performance */
YELLA_EXPORT UChar* yella_to_string(int64_t val);
YELLA_EXPORT char* yella_to_utf8(const UChar* const str);

#endif
