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

#if !defined(RETURN_CODE_H__)
#define RETURN_CODE_H__

#include "export.h"

typedef enum
{
    YELLA_NO_ERROR,
    YELLA_TOO_BIG,
    YELLA_INVALID_FORMAT,
    YELLA_LOGIC_ERROR,
    YELLA_DOES_NOT_EXIST,
    YELLA_READ_ERROR,
    YELLA_FILE_SYSTEM_ERROR,
    YELLA_NO_PERMISSION,
    YELLA_ALREADY_EXISTS
} yella_rc;

YELLA_EXPORT const char* yella_strerror(yella_rc rc);

#endif
