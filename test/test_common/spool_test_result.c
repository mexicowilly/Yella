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

#include "spool_test_result.h"
#include "spool_test_result_builder.h"

uint8_t* pack_spool_test_result(const spool_test_result* const str, size_t* size)
{
    flatcc_builder_t bld;
    uint8_t* result;

    flatcc_builder_init(&bld);
    yella_fb_spool_test_result_start_as_root(&bld);
    yella_fb_spool_test_result_message_count_add(&bld, str->message_count);
    yella_fb_spool_test_result_elapsed_milliseconds_add(&bld, str->elapsed_milliseconds);
    yella_fb_spool_test_result_end_as_root(&bld);
    result = flatcc_builder_finalize_buffer(&bld, size);
    flatcc_builder_clear(&bld);
    return result;
}

spool_test_result* unpack_spool_test_result(const uint8_t* const bytes)
{
    yella_fb_spool_test_result_table_t str;
    spool_test_result* result;

    result = malloc(sizeof(spool_test_result));
    str = yella_fb_spool_test_result_as_root(bytes);
    result->message_count = yella_fb_spool_test_result_message_count(str);
    result->elapsed_milliseconds = yella_fb_spool_test_result_elapsed_milliseconds(str);
    return result;
}