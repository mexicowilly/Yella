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

#if !defined(RUNTIME_LINK_H__)
#define RUNTIME_LINK_H__

#include <chucho/logger.h>

void close_shared_object(void* handle);
void* open_shared_object(const char* const file_name, chucho_logger_t* lgr);
void* shared_object_symbol(void* handle, const char* const name, chucho_logger_t* lgr);

#endif
