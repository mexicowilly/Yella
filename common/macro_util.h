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

#include <chucho/log.h>

#if !defined(MACRO_UTIL_H__)
#define MACRO_UTIL_H__

#define YELLA_VALUE_STR(s) YELLA_VALUE_STR_IMPL(s)
#define YELLA_VALUE_STR_IMPL(s) #s
#define YELLA_ARRAY_SIZE(arr) (sizeof(arr) / sizeof(arr[0]))

#define YELLA_REQUIRE_FLATB_FIELD(tbl, tbl_var, fld, lgr, fl) \
    if (!yella_fb_##tbl##_##fld##_is_present(tbl_var)) \
    { \
        CHUCHO_C_ERROR((lgr), \
                       "The " #tbl " field " #fld " is required"); \
        fl ; \
    }

#endif
