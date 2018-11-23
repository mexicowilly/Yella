#include "agent/envelope.h"
#include "common/text_util.h"
#include "common/macro_util.h"
#include "envelope_reader.h"
#include <stdlib.h>
#include <string.h>

void destroy_envelope(envelope* env)
{
    udsfree(env->type);
    free(env->message);
    free(env);
}

envelope* unpack_envelope(const uint8_t* const bytes)
{
    yella_fb_envelope_table_t tbl;
    flatbuffers_uint8_vec_t msg_vec;
    envelope* result;
    UChar* utf16;

    tbl = yella_fb_envelope_as_root(bytes);
    YELLA_REQUIRE_FLATB_FIELD(envelope, tbl, type, "yella.envelope", return NULL)
    YELLA_REQUIRE_FLATB_FIELD(envelope, tbl, message, "yella.envelope", return NULL)
    result = malloc(sizeof(envelope));
    utf16 = yella_from_utf8(yella_fb_envelope_type(tbl));
    result->type = udsnew(utf16);
    free(utf16);
    msg_vec = yella_fb_envelope_message(tbl);
    result->len = flatbuffers_vec_len(msg_vec);
    result->message = malloc(result->len);
    memcpy(result->message, msg_vec, result->len);
    return result;
}
