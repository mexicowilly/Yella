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
#include "common/return_code.h"
#include "common/uds.h"
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

YELLA_EXPORT extern const UChar* YELLA_DIR_SEP;

typedef struct yella_directory_iterator yella_directory_iterator;

typedef enum
{
    YELLA_FILE_TYPE_BLOCK_SPECIAL,
    YELLA_FILE_TYPE_CHARACTER_SPECIAL,
    YELLA_FILE_TYPE_DIRECTORY,
    YELLA_FILE_TYPE_FIFO,
    YELLA_FILE_TYPE_SYMBOLIC_LINK,
    YELLA_FILE_TYPE_REGULAR,
    YELLA_FILE_TYPE_SOCKET,
    YELLA_FILE_TYPE_WHITEOUT
} yella_file_type;

YELLA_EXPORT yella_rc yella_apply_function_to_file_contents(const UChar* const name,
                                                            void(*func)(const uint8_t* const, size_t, void*),
                                                            void* udata);
YELLA_EXPORT uds yella_base_name(const UChar* const path);
YELLA_EXPORT yella_rc yella_create_directory(const UChar* const name);
YELLA_EXPORT uds yella_dir_name(const UChar* const path);
YELLA_EXPORT yella_rc yella_ensure_dir_exists(const UChar* const name);
YELLA_EXPORT const UChar* yella_getcwd(void);
YELLA_EXPORT yella_rc yella_get_file_type(const UChar* const name, yella_file_type* tp);
YELLA_EXPORT yella_rc yella_file_contents(const UChar* const name, uint8_t** contents);
YELLA_EXPORT bool yella_file_exists(const UChar* const name);
YELLA_EXPORT yella_rc yella_file_size(const UChar* const name, size_t* sz);
YELLA_EXPORT yella_rc yella_remove_all(const UChar* const name);

YELLA_EXPORT yella_directory_iterator* yella_create_directory_iterator(const UChar* const dir);
YELLA_EXPORT void yella_destroy_directory_iterator(yella_directory_iterator* itor);
YELLA_EXPORT const UChar* yella_directory_iterator_next(yella_directory_iterator* itor);

#endif
