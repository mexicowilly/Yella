#include "common/envelope.h"
#include "common/text_util.h"
#include "common/macro_util.h"
#include "envelope_reader.h"
#include "envelope_builder.h"
#include <stdlib.h>
#include <string.h>

yella_envelope* yella_create_envelope(const UChar* const sender,
                                      const UChar* const type,
                                      const uint8_t* const msg,
                                      size_t len)
{
    yella_envelope* result;

    result = malloc(sizeof(yella_envelope));
    result->time = time(NULL);
    result->sender = udsnew(sender);
    result->type = udsnew(type);
    result->message = malloc(len);
    memcpy(result->message, msg, len);
    result->len = len;
    return result;
}

void yella_destroy_envelope(yella_envelope* env)
{
    udsfree(env->sender);
    udsfree(env->type);
    free(env->message);
    free(env);
}

uint8_t* yella_pack_envelope(const yella_envelope* const env, size_t* len)
{
    flatcc_builder_t bld;
    uint8_t* result;
    char* utf8;

    flatcc_builder_init(&bld);
    yella_fb_envelope_start_as_root(&bld);
    yella_fb_envelope_seconds_since_epoch_add(&bld, env->time);
    utf8 = yella_to_utf8(env->sender);
    yella_fb_envelope_sender_create_str(&bld, utf8);
    free(utf8);
    utf8 = yella_to_utf8(env->type);
    yella_fb_envelope_type_create_str(&bld, utf8);
    free(utf8);
    yella_fb_envelope_message_create(&bld, env->message, env->len);
    yella_fb_envelope_end_as_root(&bld);
    result = flatcc_builder_finalize_buffer(&bld, len);
    flatcc_builder_clear(&bld);
    return result;
}

yella_envelope* yella_unpack_envelope(const uint8_t* const bytes)
{
    yella_fb_envelope_table_t tbl;
    flatbuffers_uint8_vec_t msg_vec;
    yella_envelope* result;
    UChar* utf16;

    tbl = yella_fb_envelope_as_root(bytes);
    YELLA_REQUIRE_FLATB_FIELD(envelope, tbl, seconds_since_epoch, "yella.envelope", return NULL)
    YELLA_REQUIRE_FLATB_FIELD(envelope, tbl, sender, "yella.envelope", return NULL)
    YELLA_REQUIRE_FLATB_FIELD(envelope, tbl, type, "yella.envelope", return NULL)
    YELLA_REQUIRE_FLATB_FIELD(envelope, tbl, message, "yella.envelope", return NULL)
    result = malloc(sizeof(yella_envelope));
    result->time = yella_fb_envelope_seconds_since_epoch(tbl);
    utf16 = yella_from_utf8(yella_fb_envelope_sender(tbl));
    result->sender = udsnew(utf16);
    free(utf16);
    utf16 = yella_from_utf8(yella_fb_envelope_type(tbl));
    result->type = udsnew(utf16);
    free(utf16);
    msg_vec = yella_fb_envelope_message(tbl);
    result->len = flatbuffers_vec_len(msg_vec);
    result->message = malloc(result->len);
    memcpy(result->message, msg_vec, result->len);
    return result;
}
