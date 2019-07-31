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
    else if (rc == YELLA_LOGIC_ERROR)
        return "Logic error";
    else if (rc == YELLA_DOES_NOT_EXIST)
        return "Does not exist";
    else if (rc == YELLA_READ_ERROR)
        return "Error reading";
    else if (rc == YELLA_FILE_SYSTEM_ERROR)
        return "File system error";
    else if (rc == YELLA_NO_PERMISSION)
        return "No permission";
    else if (rc == YELLA_ALREADY_EXISTS)
        return "Already exists";
    else if (rc == YELLA_WRITE_ERROR)
        return "Write error";
    else if (rc == YELLA_TIMED_OUT)
        return "Timed out";
    return "";
}
