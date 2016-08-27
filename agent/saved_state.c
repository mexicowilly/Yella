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

#include "saved_state.h"
#include "saved_state_builder.h"
#include "saved_state_reader.h"
#include "yella_uuid.h"
#include "common/settings.h"
#include "common/file.h"
#include "common/log.h"
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>

struct yella_saved_state
{
    uint32_t boot_count;
    yella_uuid* id;
};

static char* yella_bs_file_name(void)
{
    const char* data_dir;
    const char* bs_part;
    char* fname;

    data_dir = yella_settings_get_text("data-dir");
    bs_part = "saved_state.flatb";
    fname = malloc(strlen(data_dir) + 1 + strlen(bs_part) + 1);
    strcpy(fname, data_dir);
    strcat(fname, YELLA_DIR_SEP);
    strcat(fname, bs_part);
    return fname;
}

uint32_t yella_saved_state_count(yella_saved_state* bs)
{
    return bs->boot_count;
}

void yella_destroy_saved_state(yella_saved_state* bs)
{
    free(bs);
}

yella_saved_state* yella_load_saved_state(void)
{
    char* fname;
    uint8_t* raw;
    yella_fb_saved_state_table_t tbl;
    bool is_corrupt;
    yella_saved_state* bs;
    yella_rc rc;
    flatbuffers_uint8_vec_t id_vec;

    bs = malloc(sizeof(yella_saved_state));
    is_corrupt = false;
    fname = yella_bs_file_name();
    rc = yella_file_contents(fname, &raw);
    if (rc == YELLA_NO_ERROR)
    {
        tbl = yella_fb_saved_state_as_root(raw);
        if (yella_fb_saved_state_boot_count_is_present(tbl))
            bs->boot_count = yella_fb_saved_state_boot_count(tbl);
        else
            is_corrupt = true;
        if (yella_fb_saved_state_uuid_is_present(tbl))
        {
            id_vec = yella_fb_saved_state_uuid(tbl);
            bs->id = yella_create_uuid_from_bytes(id_vec, flatbuffers_uint8_vec_len(id_vec));
        }
        else
        {
            is_corrupt = true;
        }
    }
    else if (rc == YELLA_DOES_NOT_EXIST)
    {
        CHUCHO_C_INFO(yella_logger("yella"),
                      "The file %s does not exist. This is first boot.",
                      fname);
        bs->boot_count = 1;
        bs->id = yella_create_uuid();
    }
    else
    {
        is_corrupt = true;
    }
    if (is_corrupt)
    {
        CHUCHO_C_INFO(yella_logger("yella"),
                      "The file %s is corrupt. It is being recreated.",
                      fname);
        remove(fname);
        bs->boot_count = 1;
        bs->id = yella_create_uuid();
    }
    free(fname);
    return bs;
}

yella_rc yella_save_saved_state(yella_saved_state* bs)
{
    FILE* f;
    flatcc_builder_t bld;
    uint8_t* raw;
    size_t size;
    char* fname;
    size_t num_written;
    int err;

    flatcc_builder_init(&bld);
    yella_fb_saved_state_start_as_root(&bld);
    yella_fb_saved_state_boot_count_add(&bld, bs->boot_count);
    yella_fb_saved_state_uuid_create(&bld,
                                   (uint8_t*)yella_uuid_bytes(bs->id),
                                   yella_uuid_byte_count(bs->id));
    yella_fb_saved_state_end_as_root(&bld);
    raw = flatcc_builder_finalize_buffer(&bld, &size);
    flatcc_builder_clear(&bld);
    fname = yella_bs_file_name();
    f = fopen(fname, "w");
    if (f == NULL)
    {
        err = errno;
        CHUCHO_C_ERROR(yella_logger("yella"),
                       "Could not open %s for writing: %s",
                       fname,
                       strerror(err));
    }
    num_written = fwrite(raw, 1, size, f);
    fclose(f);
    free(raw);
    if (num_written != size)
    {
        CHUCHO_C_ERROR(yella_logger("yella"),
                       "The was a problem writing to %s. The boot state cannot be saved. Subsequent boots will be considered first boot.",
                       fname);
        remove(fname);
    }
    free(fname);
}
