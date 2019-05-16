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
#include "message_header_builder.h"
#include "common/macro_util.h"
#include "common/text_util.h"
#include <unicode/udat.h>

yella_message_header* yella_create_mhdr(void)
{
    yella_message_header* result;

    result = calloc(1, sizeof(yella_message_header));
    result->time = ucal_getNow();
    return result;
}

void yella_destroy_mhdr(yella_message_header* mhdr)
{
    udsfree(mhdr->sender);
    udsfree(mhdr->recipient);
    udsfree(mhdr->type);
    if (mhdr->grp != NULL)
    {
        udsfree(mhdr->grp->identifier);
        free(mhdr->grp);
    }
    free(mhdr);
}

void yella_log_mhdr(const yella_message_header* const mhdr, chucho_logger_t* lgr)
{
    char* timestamp;
    const UChar* cmp;
    uds group;
    const UChar* dis;
    UDateFormat* dfmt;
    UErrorCode uerr;
    UChar dbuf[17];
    int32_t urc;
    uds formatted;
    char* utf8;

    if (chucho_logger_permits(lgr, CHUCHO_INFO))
    {
        uerr = U_ZERO_ERROR;
        dfmt = udat_open(UDAT_PATTERN, UDAT_PATTERN, NULL, u"UTC", 3, u"yyyyMMdd'T'HHmmss'Z'", -1, &uerr);
        assert(U_SUCCESS(uerr));
        uerr = U_ZERO_ERROR;
        urc = udat_format(dfmt, mhdr->time, dbuf, sizeof(dbuf) / sizeof(UChar), NULL, &uerr);
        udat_close(dfmt);
        assert(urc == 16);
        cmp = (mhdr->cmp == YELLA_COMPRESSION_NONE) ? u"NONE" : u"LZ4";
        if (mhdr->grp == NULL)
        {
            group = udsempty();
        }
        else
        {
            dis = (mhdr->grp->disposition == YELLA_GROUP_DISPOSITION_END) ? u"END" : u"MORE";
            group = udscatprintf(udsempty(), u", group { identifier = %S, disposition = %S }", mhdr->grp->identifier, dis);
        }
        formatted = udscatprintf(udsempty(),
                                 u"Message header: time = %S, sender = %S, recipient = %S, type = %S, compression = %S, sequence { major = %u, minor = %u }%S",
                                 dbuf,
                                 mhdr->sender,
                                 mhdr->recipient,
                                 mhdr->type,
                                 cmp,
                                 mhdr->seq.major,
                                 mhdr->seq.minor,
                                 group);
        utf8 = yella_to_utf8(formatted);
        udsfree(group);
        udsfree(formatted);
        CHUCHO_C_INFO(lgr, utf8);
        free(utf8);
    }
}

uint8_t* yella_pack_mhdr(const yella_message_header* const mhdr, size_t* size)
{
    flatcc_builder_t bld;
    uint8_t* result;
    char* utf8;

    flatcc_builder_init(&bld);
    yella_fb_header_start_as_root(&bld);
    yella_fb_header_seconds_since_epoch_add(&bld, (uint64_t)mhdr->time);
    utf8 = yella_to_utf8(mhdr->sender);
    yella_fb_header_sender_create_str(&bld, utf8);
    free(utf8);
    if (mhdr->recipient != NULL)
    {
        utf8 = yella_to_utf8(mhdr->recipient);
        yella_fb_header_recipient_create_str(&bld, utf8);
        free(utf8);
    }
    utf8 = yella_to_utf8(mhdr->type);
    yella_fb_header_type_create_str(&bld, utf8);;
    free(utf8);
    yella_fb_header_cmp_add(&bld, (mhdr->cmp == YELLA_COMPRESSION_LZ4) ? yella_fb_compression_LZ4 : yella_fb_compression_NONE);
    yella_fb_sequence_start(&bld);
    yella_fb_sequence_major_add(&bld, mhdr->seq.major);
    yella_fb_sequence_minor_add(&bld, mhdr->seq.minor);
    yella_fb_header_seq_add(&bld, yella_fb_sequence_end(&bld));
    if (mhdr->grp != NULL)
    {
        yella_fb_group_start(&bld);
        utf8 = yella_to_utf8(mhdr->grp->identifier);
        yella_fb_group_id_create_str(&bld, utf8);
        free(utf8);
        yella_fb_group_disposition_add(&bld,
                                       (mhdr->grp->disposition == YELLA_GROUP_DISPOSITION_MORE)
                                       ? yella_fb_group_disposition_MORE : yella_fb_group_disposition_END);
        yella_fb_header_grp_add(&bld, yella_fb_group_end(&bld));
    }
    yella_fb_header_end_as_root(&bld);
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
    UChar* utf16;

    tbl = yella_fb_header_as_root(bytes);
    YELLA_REQUIRE_FLATB_FIELD(header, tbl, sender, "yella.message_header", return NULL)
    YELLA_REQUIRE_FLATB_FIELD(header, tbl, recipient, "yella.message_header", return NULL)
    YELLA_REQUIRE_FLATB_FIELD(header, tbl, type, "yella.message_header", return NULL)
    YELLA_REQUIRE_FLATB_FIELD(header, tbl, seq, "yella.message_header", return NULL)
    result = malloc(sizeof(yella_message_header));
    result->time = yella_fb_header_seconds_since_epoch(tbl);
    utf16 = yella_from_utf8(yella_fb_header_sender(tbl));
    result->sender = udsnew(utf16);
    free(utf16);
    utf16 = yella_from_utf8(yella_fb_header_recipient(tbl));
    result->recipient = udsnew(utf16);
    free(utf16);
    utf16 = yella_from_utf8(yella_fb_header_type(tbl));
    result->type = udsnew(utf16);
    free(utf16);
    if (yella_fb_header_cmp(tbl) == yella_fb_compression_LZ4)
        result->cmp = YELLA_COMPRESSION_LZ4;
    else
        result->cmp = YELLA_COMPRESSION_NONE;
    seq = yella_fb_header_seq(tbl);
    YELLA_REQUIRE_FLATB_FIELD(sequence, seq, major, "yella.message_header", yella_destroy_mhdr(result);return NULL)
    YELLA_REQUIRE_FLATB_FIELD(sequence, seq, minor, "yella.message_header", yella_destroy_mhdr(result);return NULL)
    result->seq.major = yella_fb_sequence_major(seq);
    result->seq.minor = yella_fb_sequence_minor(seq);
    result->grp = NULL;
    if (yella_fb_header_grp_is_present(tbl))
    {
        grp = yella_fb_header_grp(tbl);
        // checking the disposition presence fails, even thought it is correctly set
        result->grp = malloc(sizeof(yella_group));
        utf16 = yella_from_utf8(yella_fb_group_id(grp));
        result->grp->identifier = udsnew(utf16);
        free(utf16);
        if (yella_fb_group_disposition(grp) == yella_fb_group_disposition_MORE)
            result->grp->disposition = YELLA_GROUP_DISPOSITION_MORE;
        else
            result->grp->disposition = YELLA_GROUP_DISPOSITION_END;
    }
    return result;
}