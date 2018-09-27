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
#include <assert.h>

static bool mac_addresses_changed(yella_fb_mac_addr_vec_t old_addrs, chucho_logger_t* lgr)
{
    yella_mac_addresses* new_addrs;
    flatbuffers_uint8_vec_t bytes;
    int i;
    int j;
    bool result;

    result = true;
    new_addrs = yella_get_mac_addresses(lgr);
    for (i = 0; result == true && i < new_addrs->count; i++)
    {
        for (j = 0; i < yella_fb_mac_addr_vec_len(old_addrs); j++)
        {
            bytes = yella_fb_mac_addr_bytes(yella_fb_mac_addr_vec_at(old_addrs, i));
            assert(flatbuffers_uint8_vec_len(bytes) == sizeof(new_addrs->addrs[i].addr));
            if (memcmp(bytes, new_addrs->addrs[i].addr, sizeof(new_addrs->addrs[i].addr)) == 0)
            {
                result = false;
                break;
            }
        }
    }
    yella_destroy_mac_addresses(new_addrs);
    return result;
}

static sds ss_file_name(void)
{
    return sdscatprintf(sdsempty(), "%s%s%s", yella_settings_get_text("agent", "data-dir"), YELLA_DIR_SEP, "saved_state.flatb");
}

static void reset_ss(yella_saved_state* st, chucho_logger_t* lgr)
{
    st->id = yella_create_uuid();
    st->mac_addresses = yella_get_mac_addresses(lgr);
    st->boot_count = 0;
}

void yella_destroy_saved_state(yella_saved_state* ss)
{
    yella_destroy_uuid(ss->id);
    yella_destroy_mac_addresses(ss->mac_addresses);
    free(ss);
}

yella_saved_state* yella_load_saved_state(chucho_logger_t* lgr)
{
    sds fname;
    uint8_t* raw;
    yella_fb_saved_state_table_t tbl;
    bool is_corrupt;
    yella_saved_state* ss;
    yella_rc rc;
    flatbuffers_uint8_vec_t id_vec;
    yella_fb_mac_addr_vec_t mac_addrs_vec;
    flatbuffers_uint8_vec_t addr_bytes;
    int i;
    sds addr_text;

    ss = malloc(sizeof(yella_saved_state));
    is_corrupt = false;
    fname = ss_file_name();
    rc = yella_file_contents(fname, &raw);
    if (rc == YELLA_NO_ERROR)
    {
        if (!flatbuffers_has_identifier(raw, flatbuffers_identifier))
        {
            is_corrupt = true;
        }
        else
        {
            tbl = yella_fb_saved_state_as_root(raw);
            if (yella_fb_saved_state_uuid_is_present(tbl) &&
                yella_fb_saved_state_mac_addrs_is_present(tbl) &&
                yella_fb_saved_state_boot_count_is_present(tbl))
            {
                mac_addrs_vec = yella_fb_saved_state_mac_addrs(tbl);
                if (mac_addresses_changed(mac_addrs_vec, lgr))
                {
                    reset_ss(ss, lgr);
                    CHUCHO_C_INFO_L(lgr, "All MAC addresses have changed, so resetting the agent ID");
                }
                else
                {
                    ss->boot_count = yella_fb_saved_state_boot_count(tbl);
                    id_vec = yella_fb_saved_state_uuid(tbl);
                    ss->id = yella_create_uuid_from_bytes(id_vec);
                    ss->mac_addresses = malloc(sizeof(yella_mac_addresses));
                    ss->mac_addresses->count = yella_fb_mac_addr_vec_len(mac_addrs_vec);
                    ss->mac_addresses->addrs = malloc(ss->mac_addresses->count * sizeof(yella_mac_address));
                    for (i = 0; i < ss->mac_addresses->count; i++)
                    {
                        addr_bytes = yella_fb_mac_addr_bytes(yella_fb_mac_addr_vec_at(mac_addrs_vec, i));
                        assert(flatbuffers_uint8_vec_len(addr_bytes) == sizeof(ss->mac_addresses->addrs[i].addr));
                        memcpy(&ss->mac_addresses->addrs[i], addr_bytes, sizeof(ss->mac_addresses->addrs[i].addr));
                        snprintf(ss->mac_addresses->addrs[i].text,
                                 sizeof(ss->mac_addresses->addrs[i].text),
                                 "%02x:%02x:%02x:%02x:%02x:%02x",
                                 ss->mac_addresses->addrs[i].addr[0],
                                 ss->mac_addresses->addrs[i].addr[1],
                                 ss->mac_addresses->addrs[i].addr[2],
                                 ss->mac_addresses->addrs[i].addr[3],
                                 ss->mac_addresses->addrs[i].addr[4],
                                 ss->mac_addresses->addrs[i].addr[5]);
                    }
                }
            }
            else
            {
                is_corrupt = true;
            }
        }
        free(raw);
    }
    else if (rc == YELLA_DOES_NOT_EXIST)
    {
        CHUCHO_C_INFO_L(lgr,
                        "The file %s does not exist. This is first boot.",
                        fname);
        reset_ss(ss, lgr);
    }
    else
    {
        is_corrupt = true;
    }
    if (is_corrupt)
    {
        CHUCHO_C_INFO_L(lgr,
                        "The file %s is corrupt. It is being recreated.",
                        fname);
        remove(fname);
        reset_ss(ss, lgr);
    }
    sdsfree(fname);
    ++ss->boot_count;
    if (chucho_logger_permits(lgr, CHUCHO_INFO))
    {
        addr_text = sdsempty();
        if (ss->mac_addresses->count > 0)
        {
            for (i = 0; i < ss->mac_addresses->count; i++)
            {
                addr_text = sdscat(addr_text, ss->mac_addresses->addrs[i].text);
                addr_text = sdscat(addr_text, ",");
            }
            addr_text[strlen(addr_text) - 1] = 0;
        }
        CHUCHO_C_INFO_L(lgr,
                        "Saved state: boot_count = %u, id = %s, mac_addresses = { %s }",
                        ss->boot_count,
                        ss->id->text,
                        addr_text);
        sdsfree(addr_text);
    }
    return ss;
}

yella_rc yella_save_saved_state(yella_saved_state* ss, chucho_logger_t* lgr)
{
    FILE* f;
    flatcc_builder_t bld;
    uint8_t* raw;
    size_t size;
    char* fname;
    size_t num_written;
    int err;
    int i;

    flatcc_builder_init(&bld);
    yella_fb_saved_state_start_as_root(&bld);
    yella_fb_saved_state_boot_count_add(&bld, ss->boot_count);
    yella_fb_saved_state_uuid_create(&bld,
                                    ss->id->id,
                                    sizeof(ss->id->text));
    if (ss->mac_addresses->count > 0)
    {
        yella_fb_saved_state_mac_addrs_start(&bld);
        for (i = 0; i < ss->mac_addresses->count; i++)
        {
            yella_fb_mac_addr_start(&bld);
            yella_fb_mac_addr_bytes_add(&bld,
                                        flatbuffers_uint8_vec_create(&bld,
                                                                     ss->mac_addresses->addrs[i].addr,
                                                                     sizeof(ss->mac_addresses->addrs[i].addr)));
            yella_fb_saved_state_mac_addrs_push(&bld, yella_fb_mac_addr_end(&bld));
        }
        yella_fb_saved_state_mac_addrs_end(&bld);
    }
    yella_fb_saved_state_end_as_root(&bld);
    raw = flatcc_builder_finalize_buffer(&bld, &size);
    flatcc_builder_clear(&bld);
    fname = ss_file_name();
    f = fopen(fname, "wb");
    if (f == NULL)
    {
        err = errno;
        CHUCHO_C_ERROR_L(lgr,
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
        CHUCHO_C_ERROR_L(lgr,
                         "The was a problem writing to %s. The boot state cannot be saved. Subsequent boots will be considered first boot.",
                         fname);
        remove(fname);
        return YELLA_WRITE_ERROR;
    }
    sdsfree(fname);
    return YELLA_NO_ERROR;
}

