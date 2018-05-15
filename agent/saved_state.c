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
#include "mac_addresses.h"
#include "common/settings.h"
#include "common/file.h"
#include "common/text_util.h"
#include <chucho/log.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>

struct yella_saved_state
{
    uint32_t boot_count;
    yella_uuid* id;
    yella_mac_addresses* mac_addresses;
};

static char* ss_file_name(void)
{
    return yella_sprintf("%s%s%s", yella_settings_get_text("data-dir"), YELLA_DIR_SEP, "saved_state.flatb");
}

static void reset_ss(yella_saved_state* st)
{
    st->boot_count = 1;
    st->id = yella_create_uuid();
    st->mac_addresses = yella_get_mac_addresses();
}

void yella_destroy_saved_state(yella_saved_state* ss)
{
    yella_destroy_uuid(ss->id);
    yella_destroy_mac_addresses(ss->mac_addresses);
    free(ss);
}

yella_saved_state* yella_load_saved_state(void)
{
    char* fname;
    uint8_t* raw;
    yella_fb_saved_state_table_t tbl;
    bool is_corrupt;
    yella_saved_state* ss;
    yella_rc rc;
    flatbuffers_uint8_vec_t id_vec;
    flatbuffers_uint64_vec_t mac_addrs_vec;
    int i;

    ss = malloc(sizeof(yella_saved_state));
    is_corrupt = false;
    fname = ss_file_name();
    rc = yella_file_contents(fname, &raw);
    if (rc == YELLA_NO_ERROR)
    {
        tbl = yella_fb_saved_state_as_root(raw);
        if (yella_fb_saved_state_boot_count_is_present(tbl) &&
            yella_fb_saved_state_uuid_is_present(tbl) &&
            yella_fb_saved_state_mac_addrs_is_present(tbl))
        {
            ss->boot_count = yella_fb_saved_state_boot_count(tbl) + 1;
            id_vec = yella_fb_saved_state_uuid(tbl);
            ss->id = yella_create_uuid_from_bytes(id_vec, flatbuffers_uint8_vec_len(id_vec));
            mac_addrs_vec = yella_fb_saved_state_mac_addrs(tbl);
            ss->mac_addresses = malloc(sizeof(yella_mac_addresses));
            ss->mac_addresses->count = flatbuffers_uint64_vec_len(mac_addrs_vec);
            ss->mac_addresses->addrs = malloc(ss->mac_addresses->count * sizeof(uint64_t));
            for (i = 0; i < ss->mac_addresses->count; i++)
                ss->mac_addresses->addrs[i] = flatbuffers_uint64_vec_at(mac_addrs_vec, i);
        }
        else
        {
            is_corrupt = true;
        }
        free(raw);
    }
    else if (rc == YELLA_DOES_NOT_EXIST)
    {
        CHUCHO_C_INFO("yella",
                      "The file %s does not exist. This is first boot.",
                      fname);
        reset_ss(ss);
    }
    else
    {
        is_corrupt = true;
    }
    if (is_corrupt)
    {
        CHUCHO_C_INFO("yella",
                      "The file %s is corrupt. It is being recreated.",
                      fname);
        remove(fname);
        reset_ss(ss);
    }
    free(fname);
    return ss;
}

yella_rc yella_save_saved_state(yella_saved_state* ss)
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
    yella_fb_saved_state_boot_count_add(&bld, ss->boot_count);
    yella_fb_saved_state_uuid_create(&bld,
                                    (uint8_t*)yella_uuid_bytes(ss->id),
                                    yella_uuid_byte_count(ss->id));
    yella_fb_saved_state_mac_addrs_create(&bld,
                                          ss->mac_addresses->addrs,
                                          ss->mac_addresses->count);
    yella_fb_saved_state_end_as_root(&bld);
    raw = flatcc_builder_finalize_buffer(&bld, &size);
    flatcc_builder_clear(&bld);
    fname = ss_file_name();
    f = fopen(fname, "wb");
    if (f == NULL)
    {
        err = errno;
        CHUCHO_C_ERROR("yella",
                       "Could not open %s for writing: %s",
                       fname,
                       strerror(err));
        free(raw);
        return YELLA_WRITE_ERROR;
    }
    num_written = fwrite(raw, 1, size, f);
    fclose(f);
    free(raw);
    if (num_written != size)
    {
        CHUCHO_C_ERROR("yella",
                       "The was a problem writing to %s. The boot state cannot be saved. Subsequent boots will be considered first boot.",
                       fname);
        remove(fname);
        return YELLA_WRITE_ERROR;
    }
    free(fname);
    return YELLA_NO_ERROR;
}

uint32_t yella_saved_state_boot_count(const yella_saved_state* ss)
{
    return ss->boot_count;
}

const yella_mac_addresses* yella_saved_state_mac_addresses(const yella_saved_state* ss)
{
    return ss->mac_addresses;
}

yella_uuid* yella_saved_state_uuid(const yella_saved_state* ss)
{
    return ss->id;
}