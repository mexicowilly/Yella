#ifndef YELLA_COMPRESSION_H__
#define YELLA_COMPRESSION_H__

#include "export.h"
#include <stdint.h>
#include <stddef.h>

YELLA_EXPORT uint8_t* yella_lz4_compress(const uint8_t* const bytes, size_t* size);
YELLA_EXPORT uint8_t* yella_lz4_decompress(const uint8_t* const bytes, size_t* size);

#endif
