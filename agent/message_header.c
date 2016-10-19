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

#include "message_header.h"
#include "header_reader.h"
#include "header_builder.h"
#include "common/log.h"
#include "common/text_util.h"

struct yella_message_header
{
    time_t time;
    char* sender;
    char* recipient;
    char* message_type;
    yella_sequence seq;
    yella_group grp;
    yella_compression cmp;
};

void yella_destroy_mhdr(yella_message_header* mhdr)
{
    free(mhdr->sender);
    free(mhdr->recipient);
    free(mhdr->message_type);
    free(mhdr);
}

const yella_compression yella_mhdr_get_compression(const yella_message_header const* mhdr)
{
    return mhdr->cmp;
}

const yella_group* yella_mhdr_get_group(const yella_message_header const* mhdr)
{
    return &mhdr->grp;
}

const char* yella_mhdr_get_message_type(const yella_message_header const* mhdr)
{
    return mhdr->message_type;
}

const char* yella_mhdr_get_recipient(const yella_message_header const* mhdr)
{
    return mhdr->recipient;
}

const char* yella_mhdr_get_sender(const yella_message_header const* mhdr)
{
    return mhdr->sender;
}

const yella_sequence* yella_mhdr_get_sequence(const yella_message_header const* mhdr)
{
    return &mhdr->seq;
}

time_t yella_mhdr_get_time(const yella_message_header const* mhdr)
{
    return mhdr->time;
}

uint8_t* yella_pack_mhdr(const yella_message_header const* mhdr, size_t* size)
{
    flatcc_builder_t bld;
    uint8_t* result;

    flatcc_builder_init(&bld);
    yella_fb_header_start_as_root(&bld);
    yella_fb_header_seconds_since_epoch_add(&bld, mhdr->time);
    yella_fb_header_sender_create_str(&bld, mhdr->sender);
    yella_fb_header_recipient_create_str(&bld, mhdr->recipient);
    yella_fb_header_message_type_create_str(&bld, mhdr->message_type);
    yella_fb_header_seq_add(&bld, yella_fb_sequence_create(&bld, mhdr->seq.major, mhdr->seq.minor));
    yella_fb_header_grp_add(&bld, yella_fb_group_create(&bld,
        yella_fb_group_id_create_str(&bld, mhdr->grp.identifier),
        (mhdr->grp.disposition == YELLA_GROUP_DISPOSITION_MORE) ? yella_fb_group_disposition_MORE : yella_fb_group_disposition_END));
    yella_fb_header_cmp_add(&bld, (mhdr->cmp == YELLA_COMPRESSION_LZ4) ? yella_fb_compression_LZ4 : yella_fb_compression_NONE);
    result = flatcc_builder_finalize_buffer(&bld, size);
    flatcc_builder_clear(&bld);
    return result;
}

yella_message_header* yella_unpack_mhdr(const uint8_t const* bytes)
{
    yella_fb_header_table_t tbl;
    yella_fb_sequence_table_t seq;
    yella_fb_group_table_t grp;
    yella_message_header* result;

    tbl = yella_fb_header_as_root(bytes);
    if (!yella_fb_header_seconds_since_epoch_is_present(tbl) ||
        !yella_fb_header_sender_is_present(tbl) ||
        !yella_fb_header_recipient_is_present(tbl) ||
        !yella_fb_header_message_type_is_present(tbl) ||
        !yella_fb_header_seq_is_present(tbl) ||
        !yella_fb_header_grp_is_present(tbl) ||
        !yella_fb_header_cmp_is_present(tbl))
    {
        CHUCHO_C_ERROR(yella_logger("yella.message_header"),
                "");
    }
    result = malloc(sizeof(yella_message_header));
    result->time = yella_fb_header_seconds_since_epoch(tbl);
    result->sender = yella_text_dup(yella_fb_header_sender(tbl));
    result->recipient = yella_text_dup(yella_fb_header_recipient(tbl));
    result->message_type = yella_text_dup(yella_fb_header_message_type(tbl));
    seq = yella_fb_header_seq(tbl);
    if (!yella_fb_sequence_major_is_present(seq) ||
        !yella_fb_sequence_minor_is_present(seq))
    {
        /* error */
    }
    result->seq.major = yella_fb_sequence_major(seq);
    result->seq.minor = yella_fb_sequence_minor(seq);
    grp - yella_fb_header_grp(tbl);
    if (!yella_fb_group_id_is_present(grp) ||
        !yella_fb_group_disposition_is_present(grp))
    {
        /* error */
    }
    result->grp.identifier = yella_text_dup(yella_fb_group_id(grp));
    if (yella_fb_group_disposition(grp) == yella_fb_group_disposition_MORE)
        result->grp.disposition = YELLA_GROUP_DISPOSITION_MORE;
    else
        result->grp.disposition = YELLA_GROUP_DISPOSITION_END;
    if (yella_fb_header_cmp(tbl) == yella_fb_compression_LZ4)
        result->cmp = YELLA_COMPRESSION_LZ4;
    else
        result->cmp = YELLA_COMPRESSION_NONE;
    return result;
}