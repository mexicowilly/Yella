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

#include "common/text_util.h"
#include <chucho/log.h>

UChar* yella_from_utf8(const char* const str)
{
    int32_t len;
    UChar* buf;
    int32_t dest_len;
    UErrorCode ec;

    len = strlen(str);
    buf = malloc((len + 1) * sizeof(UChar));
    ec = U_ZERO_ERROR;
    u_strFromUTF8(buf, len + 1, &dest_len, str, len, &ec);
    if (ec != U_ZERO_ERROR)
    {
        buf = realloc(buf, dest_len * sizeof(UChar));
        ec = U_ZERO_ERROR;
        u_strFromUTF8(buf, dest_len, &dest_len, str, len, &ec);
        if (ec != U_ZERO_ERROR)
        {
            free(buf);
            CHUCHO_C_ERROR("yella.common", "Could not convert UTF-8 to UTF-16: %s", u_errorName(ec));
            buf = NULL;
        }
    }
    return buf;
}

char* yella_to_utf8(const UChar* const str)
{
    int32_t len;
    char* buf;
    int32_t dest_len;
    UErrorCode ec;

    len = u_strlen(str);
    buf = malloc(len + 1);
    ec = U_ZERO_ERROR;
    u_strToUTF8(buf, len + 1, &dest_len, str, len, &ec);
    if (ec != U_ZERO_ERROR)
    {
        buf = realloc(buf, dest_len);
        ec = U_ZERO_ERROR;
        u_strToUTF8(buf, dest_len, &dest_len, str, len, &ec);
        if (ec != U_ZERO_ERROR)
        {
            free(buf);
            CHUCHO_C_ERROR("yella.common", "Could not convert UTF-16 to UTF-8: %s", u_errorName(ec));
            buf = NULL;
        }
    }
    return buf;
}


