#if !defined(HEARTBEAT_H__)
#define HEARTBEAT_H__

#include "common/ptr_vector.h"
#include <unicode/utypes.h>
#include <stdint.h>

uint8_t* create_heartbeat(const UChar* id, const yella_ptr_vector* plugins, size_t* sz);

#endif
