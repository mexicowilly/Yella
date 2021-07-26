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
#include <unicode/unumberformatter.h>
#include <chucho/log.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>

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
            CHUCHO_C_ERROR("common", "Could not convert UTF-8 to UTF-16: %s", u_errorName(ec));
            buf = NULL;
        }
    }
    return buf;
}

UChar* yella_replace_string(const UChar* const str, const UChar* const fnd, const UChar* const rep)
{
    int32_t fnd_len;
    int32_t rep_len;
    int i;
    size_t cnt;
    UChar* result;
    UChar* orig;

    cnt = 0;
    fnd_len = u_strlen(fnd);
    rep_len = u_strlen(rep);
    orig = (UChar*)str;
    for (i = 0; orig[i] != 0; i++)
    {
        if (u_strstr(&orig[i], fnd) == &orig[i])
        {
            cnt++;
            i += fnd_len - 1;
        }
    }
    result = malloc((i + (cnt * (rep_len - fnd_len)) + 1) * sizeof(UChar));
    i = 0;
    while (*orig != 0)
    {
        if (u_strstr(orig, fnd) == orig)
        {
            u_strcpy(&result[i], rep);
            i += rep_len;
            orig += fnd_len;
        }
        else
        {
            result[i++] = *orig++;
        }
    }
    result[i] = 0;
    return result;
}

UChar* yella_to_string(int64_t val)
{
    UNumberFormatter* fmt;
    UFormattedNumber* res;
    UErrorCode ec;
    UChar* str;
    int32_t len;

    ec = U_ZERO_ERROR;
    fmt = unumf_openForSkeletonAndLocale(u"precision-integer", -1, "en_US_POSIX", &ec);
    assert(U_SUCCESS(ec));
    res = unumf_openResult(&ec);
    assert(U_SUCCESS(ec));
    unumf_formatInt(fmt, val, res, &ec);
    if (U_SUCCESS(ec))
    {
        len = unumf_resultToString(res, NULL, 0, &ec);
        str = malloc((len + 1) * sizeof(UChar));
        ec = U_ZERO_ERROR;
        unumf_resultToString(res, str, len + 1, &ec);
    }
    else
    {
        CHUCHO_C_ERROR("common", "Could not convert integer to string: %s", u_errorName(ec));
        str = NULL;
    }
    unumf_closeResult(res);
    unumf_close(fmt);
    return str;
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
            CHUCHO_C_ERROR("common", "Could not convert UTF-16 to UTF-8: %s", u_errorName(ec));
            buf = NULL;
        }
    }
    return buf;
}


