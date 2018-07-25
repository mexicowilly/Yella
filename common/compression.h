#ifndef YELLA_COMPRESSION_H__
#define YELLA_COMPRESSION_H__

#include <stdint.h>
#include <stddef.h>

uint8_t* yella_lz4_compress(const uint8_t* const bytes, size_t* size);
uint8_t* yella_lz4_decompress(const uint8_t* const bytes, size_t* size);

#endif
