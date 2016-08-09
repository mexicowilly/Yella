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

#include "return_code.h"

const char* yella_strerror(yella_rc rc)
{
    if (rc == YELLA_NO_ERROR)
        return "No error";
    else if (rc == YELLA_TOO_BIG)
        return "Too big";
    else if (rc == YELLA_INVALID_FORMAT)
        return "Invalid format";
    return "";
}
