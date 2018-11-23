#ifndef YELLA_ENVELOPE_H__
#define YELLA_ENVELOPE_H__

#include "common/uds.h"

typedef struct envelope
{
    uds type;
    uint8_t* message;
    size_t len;
} envelope;

YELLA_EXPORT void destroy_envelope(envelope* env);
YELLA_EXPORT envelope* unpack_envelope(const uint8_t* const bytes);

#endif
