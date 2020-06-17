#include "common/parcel.h"
#include "common/text_util.h"
#include "common/macro_util.h"
#include "common/yaml_util.h"
#include "parcel_builder.h"
#include <unicode/udat.h>
#include <chucho/log.h>
#include <stdlib.h>
#include <assert.h>

yella_parcel* yella_create_parcel(const UChar* const recipient, const UChar* const type)
{
    yella_parcel* result;

    result = calloc(1, sizeof(yella_parcel));
    result->time = ucal_getNow();
    result->recipient = udsnew(recipient);
    result->type = udsnew(type);
    return result;
}

void yella_destroy_parcel(yella_parcel* pcl)
{
    udsfree(pcl->sender);
    udsfree(pcl->recipient);
    udsfree(pcl->type);
    if (pcl->grp != NULL)
    {
        udsfree(pcl->grp->identifier);
        free(pcl->grp);
    }
    free(pcl->payload);
    free(pcl);
}

void yella_log_parcel(const yella_parcel* const pcl, chucho_logger_t* lgr)
{
    UDateFormat* dfmt;
    UErrorCode uerr;
    UChar dbuf[17];
    int32_t urc;
    char* utf8;
    yaml_document_t doc;
    int top;
    int key;
    int value;

    if (chucho_logger_permits(lgr, CHUCHO_INFO))
    {
        yaml_document_initialize(&doc, NULL, NULL, NULL, 1, 1);
        top = yaml_document_add_mapping(&doc, NULL, YAML_FLOW_MAPPING_STYLE);
        uerr = U_ZERO_ERROR;
        dfmt = udat_open(UDAT_PATTERN, UDAT_PATTERN, NULL, u"UTC", 3, u"yyyyMMdd'T'HHmmss'Z'", -1, &uerr);
        assert(U_SUCCESS(uerr));
        uerr = U_ZERO_ERROR;
        urc = udat_format(dfmt, pcl->time, dbuf, sizeof(dbuf) / sizeof(UChar), NULL, &uerr);
        udat_close(dfmt);
        assert(urc == 16);
        yella_add_yaml_string_mapping(&doc, top, "time", dbuf);
        yella_add_yaml_string_mapping(&doc, top, "sender", (pcl->sender == NULL) ? u"null" : pcl->sender);
        yella_add_yaml_string_mapping(&doc, top, "recipient", (pcl->recipient == NULL) ? u"null" : pcl->recipient);
        yella_add_yaml_string_mapping(&doc, top, "type", pcl->type);
        yella_add_yaml_string_mapping(&doc, top, "compression", (pcl->cmp == YELLA_COMPRESSION_NONE) ? u"NONE" : u"LZ4");
        key = yaml_document_add_scalar(&doc, NULL, (yaml_char_t*)"sequence", 8, YAML_PLAIN_SCALAR_STYLE);
        value = yaml_document_add_mapping(&doc, NULL, YAML_FLOW_MAPPING_STYLE);
        yella_add_yaml_number_mapping(&doc, value, "major", pcl->seq.major);
        yella_add_yaml_number_mapping(&doc, value, "minor", pcl->seq.minor);
        yaml_document_append_mapping_pair(&doc, top, key, value);
        if (pcl->grp != NULL)
        {
            key = yaml_document_add_scalar(&doc, NULL, (yaml_char_t*)"group", 5, YAML_PLAIN_SCALAR_STYLE);
            value = yaml_document_add_mapping(&doc, NULL, YAML_FLOW_MAPPING_STYLE);
            yella_add_yaml_string_mapping(&doc, value, "identifier", pcl->grp->identifier);
            yella_add_yaml_string_mapping(&doc, value, "disposition", (pcl->grp->disposition == YELLA_GROUP_DISPOSITION_END) ? u"END" : u"MORE");
            yaml_document_append_mapping_pair(&doc, top, key, value);
        }
        yella_add_yaml_number_mapping(&doc, top, "payload_size", pcl->payload_size);
        utf8 = yella_emit_yaml(&doc);
        CHUCHO_C_INFO(lgr, utf8);
        free(utf8);
    }
}

uint8_t* yella_pack_parcel(const yella_parcel* const pcl, size_t* size)
{
    flatcc_builder_t bld;
    uint8_t* result;
    char* utf8;

    flatcc_builder_init(&bld);
    yella_fb_parcel_start_as_root(&bld);
    yella_fb_parcel_seconds_since_epoch_add(&bld, (uint64_t)pcl->time / 1000);
    utf8 = yella_to_utf8(pcl->sender);
    yella_fb_parcel_sender_create_str(&bld, utf8);
    free(utf8);
    if (pcl->recipient != NULL)
    {
        utf8 = yella_to_utf8(pcl->recipient);
        yella_fb_parcel_recipient_create_str(&bld, utf8);
        free(utf8);
    }
    utf8 = yella_to_utf8(pcl->type);
    yella_fb_parcel_type_create_str(&bld, utf8);;
    free(utf8);
    yella_fb_parcel_cmp_add(&bld, (pcl->cmp == YELLA_COMPRESSION_LZ4) ? yella_fb_compression_LZ4 : yella_fb_compression_NONE);
    yella_fb_sequence_start(&bld);
    yella_fb_sequence_major_add(&bld, pcl->seq.major);
    yella_fb_sequence_minor_add(&bld, pcl->seq.minor);
    yella_fb_parcel_seq_add(&bld, yella_fb_sequence_end(&bld));
    if (pcl->grp != NULL)
    {
        yella_fb_group_start(&bld);
        utf8 = yella_to_utf8(pcl->grp->identifier);
        yella_fb_group_id_create_str(&bld, utf8);
        free(utf8);
        yella_fb_group_disposition_add(&bld,
                                       (pcl->grp->disposition == YELLA_GROUP_DISPOSITION_MORE)
                                       ? yella_fb_group_disposition_MORE : yella_fb_group_disposition_END);
        yella_fb_parcel_grp_add(&bld, yella_fb_group_end(&bld));
    }
    if (pcl->payload_size > 0)
        yella_fb_parcel_payload_add(&bld, flatbuffers_uint8_vec_create(&bld, pcl->payload, pcl->payload_size));
    yella_fb_parcel_end_as_root(&bld);
    result = flatcc_builder_finalize_buffer(&bld, size);
    flatcc_builder_clear(&bld);
    return result;
}

yella_parcel* yella_unpack_parcel(const uint8_t* const bytes)
{
    yella_fb_parcel_table_t tbl;
    yella_fb_sequence_table_t seq;
    yella_fb_group_table_t grp;
    yella_parcel* result;
    UChar* utf16;
    flatbuffers_uint8_vec_t pld;

    tbl = yella_fb_parcel_as_root(bytes);
    YELLA_REQUIRE_FLATB_FIELD(parcel, tbl, sender, "yella.parcel", return NULL)
    YELLA_REQUIRE_FLATB_FIELD(parcel, tbl, recipient, "yella.parcel", return NULL)
    YELLA_REQUIRE_FLATB_FIELD(parcel, tbl, type, "yella.parcel", return NULL)
    YELLA_REQUIRE_FLATB_FIELD(parcel, tbl, seq, "yella.parcel", return NULL)
    result = calloc(1, sizeof(yella_parcel));
    result->time = yella_fb_parcel_seconds_since_epoch(tbl) * 1000;
    utf16 = yella_from_utf8(yella_fb_parcel_sender(tbl));
    result->sender = udsnew(utf16);
    free(utf16);
    utf16 = yella_from_utf8(yella_fb_parcel_recipient(tbl));
    result->recipient = udsnew(utf16);
    free(utf16);
    utf16 = yella_from_utf8(yella_fb_parcel_type(tbl));
    result->type = udsnew(utf16);
    free(utf16);
    if (yella_fb_parcel_cmp(tbl) == yella_fb_compression_LZ4)
        result->cmp = YELLA_COMPRESSION_LZ4;
    else
        result->cmp = YELLA_COMPRESSION_NONE;
    seq = yella_fb_parcel_seq(tbl);
    result->seq.major = yella_fb_sequence_major(seq);
    result->seq.minor = yella_fb_sequence_minor(seq);
    result->grp = NULL;
    if (yella_fb_parcel_grp_is_present(tbl))
    {
        grp = yella_fb_parcel_grp(tbl);
        result->grp = malloc(sizeof(yella_group));
        utf16 = yella_from_utf8(yella_fb_group_id(grp));
        result->grp->identifier = udsnew(utf16);
        free(utf16);
        if (yella_fb_group_disposition(grp) == yella_fb_group_disposition_MORE)
            result->grp->disposition = YELLA_GROUP_DISPOSITION_MORE;
        else
            result->grp->disposition = YELLA_GROUP_DISPOSITION_END;
    }
    if (yella_fb_parcel_payload_is_present(tbl))
    {
        pld = yella_fb_parcel_payload(tbl);
        result->payload_size = flatbuffers_uint8_vec_len(pld);
        result->payload = malloc(result->payload_size);
        memcpy(result->payload, pld, result->payload_size);
    }
    return result;
}
