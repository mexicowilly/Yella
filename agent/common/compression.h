#ifndef YELLA_COMPRESSION_H__
#define YELLA_COMPRESSION_H__

#include "export.h"
#include <stdint.h>
#include <stddef.h>

#if defined(__cplusplus)
extern "C"
{
#endif

YELLA_EXPORT uint8_t * yella_lz4_compress(const uint8_t * const bytes, size_t * size);
YELLA_EXPORT uint8_t * yella_lz4_decompress(const uint8_t * const bytes, size_t * size);

#if defined(__cplusplus)
}
#endif

#endif
