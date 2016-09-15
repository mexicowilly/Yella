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

#if !defined(FILE_H__)
#define FILE_H__

#include "export.h"
#include "return_code.h"
#include <stdint.h>
#include <stdbool.h>

YELLA_EXPORT extern const char* YELLA_DIR_SEP;

typedef struct yella_directory_iterator yella_directory_iterator;

YELLA_EXPORT char* yella_base_name(const char* const path);
YELLA_EXPORT yella_rc yella_create_directory(const char* const name);
YELLA_EXPORT char* yella_dir_name(const char* const path);
YELLA_EXPORT yella_rc yella_ensure_dir_exists(const char* const name);
YELLA_EXPORT yella_rc yella_file_contents(const char* const name, uint8_t** contents);
YELLA_EXPORT bool yella_file_exists(const char* const name);
YELLA_EXPORT uintmax_t yella_file_size(const char* const name);

YELLA_EXPORT yella_directory_iterator* yella_create_directory_iterator(const char* const dir);
YELLA_EXPORT void yella_destroy_directory_iterator(yella_directory_iterator* itor);
YELLA_EXPORT const char* yella_directory_iterator_next(yella_directory_iterator* itor);

#endif
