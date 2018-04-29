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
#include "common/text_util.h"
#include "common/macro_util.h"

yella_message_header* yella_create_mhdr(void)
{
    yella_message_header* result;

    result = malloc(sizeof(yella_message_header));
    result->time = time(NULL);
    result->sender = NULL;
    result->recipient = NULL;
    result->type = NULL;
    result->cmp = YELLA_COMPRESSION_NONE;
    result->seq = NULL;
    result->grp = NULL;
    return result;
}

void yella_destroy_mhdr(yella_message_header* mhdr)
{
    free(mhdr->sender);
    free(mhdr->recipient);
    free(mhdr->type);
    free(mhdr->seq);
    if (mhdr->grp != NULL)
    {
        free(mhdr->grp->identifier);
        free(mhdr->grp);
    }
    free(mhdr);
}

uint8_t* yella_pack_mhdr(const yella_message_header* const mhdr, size_t* size)
{
    flatcc_builder_t bld;
    uint8_t* result;

    flatcc_builder_init(&bld);
    yella_fb_header_start_as_root(&bld);
    yella_fb_header_seconds_since_epoch_add(&bld, mhdr->time);
    yella_fb_header_sender_create_str(&bld, mhdr->sender);
    yella_fb_header_recipient_create_str(&bld, mhdr->recipient);
    yella_fb_header_type_create_str(&bld, mhdr->type);
    yella_fb_header_cmp_add(&bld, (mhdr->cmp == YELLA_COMPRESSION_LZ4) ? yella_fb_compression_LZ4 : yella_fb_compression_NONE);
    if (mhdr->seq != NULL)
        yella_fb_header_seq_add(&bld, yella_fb_sequence_create(&bld, mhdr->seq->major, mhdr->seq->minor));
    if (mhdr->grp != NULL)
    {
        yella_fb_header_grp_add(&bld, yella_fb_group_create(&bld,
            yella_fb_group_id_create_str(&bld, mhdr->grp->identifier),
            (mhdr->grp->disposition == YELLA_GROUP_DISPOSITION_MORE) ? yella_fb_group_disposition_MORE : yella_fb_group_disposition_END));
    }
    result = flatcc_builder_finalize_buffer(&bld, size);
    flatcc_builder_clear(&bld);
    return result;
}

yella_message_header* yella_unpack_mhdr(const uint8_t* const bytes)
{
    yella_fb_header_table_t tbl;
    yella_fb_sequence_table_t seq;
    yella_fb_group_table_t grp;
    yella_message_header* result;

    tbl = yella_fb_header_as_root(bytes);
    YELLA_REQUIRE_FLATB_FIELD(header, tbl, seconds_since_epoch, "yella.message_header", return NULL)
    YELLA_REQUIRE_FLATB_FIELD(header, tbl, sender, "yella.message_header", return NULL)
    YELLA_REQUIRE_FLATB_FIELD(header, tbl, recipient, "yella.message_header", return NULL)
    YELLA_REQUIRE_FLATB_FIELD(header, tbl, type, "yella.message_header", return NULL)
    YELLA_REQUIRE_FLATB_FIELD(header, tbl, cmp, "yella.message_header", return NULL)
    result = malloc(sizeof(yella_message_header));
    result->time = yella_fb_header_seconds_since_epoch(tbl);
    result->sender = yella_text_dup(yella_fb_header_sender(tbl));
    result->recipient = yella_text_dup(yella_fb_header_recipient(tbl));
    result->type = yella_text_dup(yella_fb_header_type(tbl));
    if (yella_fb_header_cmp(tbl) == yella_fb_compression_LZ4)
        result->cmp = YELLA_COMPRESSION_LZ4;
    else
        result->cmp = YELLA_COMPRESSION_NONE;
    result->seq = NULL;
    result->grp = NULL;
    if (yella_fb_header_seq_is_present(tbl))
    {
        seq = yella_fb_header_seq(tbl);
        YELLA_REQUIRE_FLATB_FIELD(sequence, seq, major, "yella.message_header", yella_destroy_mhdr(result);return NULL)
        YELLA_REQUIRE_FLATB_FIELD(sequence, seq, minor, "yella.message_header", yella_destroy_mhdr(result);return NULL)
        result->seq = malloc(sizeof(yella_sequence));
        result->seq->major = yella_fb_sequence_major(seq);
        result->seq->minor = yella_fb_sequence_minor(seq);
    }
    if (yella_fb_header_grp_is_present(tbl))
    {
        grp = yella_fb_header_grp(tbl);
        YELLA_REQUIRE_FLATB_FIELD(group, grp, id, "yella.message_header", yella_destroy_mhdr(result);return NULL)
        YELLA_REQUIRE_FLATB_FIELD(group, grp, disposition, "yella.message_header", yella_destroy_mhdr(result);return NULL)
        result->grp = malloc(sizeof(yella_group));
        result->grp->identifier = yella_text_dup(yella_fb_group_id(grp));
        if (yella_fb_group_disposition(grp) == yella_fb_group_disposition_MORE)
            result->grp->disposition = YELLA_GROUP_DISPOSITION_MORE;
        else
            result->grp->disposition = YELLA_GROUP_DISPOSITION_END;
    }
    return result;
}