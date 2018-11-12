#ifndef YELLA_ENVELOPE_H__
#define YELLA_ENVELOPE_H__

#include "common/uds.h"

typedef struct yella_envelope
{
    uds sender;
    uds type;
    uint8_t* message;
    size_t len;
} yella_envelope;

YELLA_EXPORT yella_envelope* yella_create_envelope(const UChar* const sender,
                                                   const UChar* const type,
                                                   const uint8_t* const msg,
                                                   size_t len);
YELLA_EXPORT void yella_destroy_envelope(yella_envelope* env);
YELLA_EXPORT uint8_t* yella_pack_envelope(const yella_envelope* const env, size_t* len);
YELLA_EXPORT yella_envelope* yella_unpack_envelope(const uint8_t* const bytes);

#endif
