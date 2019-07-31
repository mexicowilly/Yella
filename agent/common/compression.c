#include "common/compression.h"
#include <chucho/log.h>
#include <lz4.h>
#include <stdlib.h>

uint8_t* yella_lz4_compress(const uint8_t* const bytes, size_t* size)
{
    int cap;
    uint8_t* cmp;

    cap = LZ4_compressBound(*size);
    cmp = malloc(cap);
    *size = LZ4_compress_default((const char*)bytes, (char*)cmp, *size, cap);
    cmp = realloc(cmp, *size);
    return cmp;
}

uint8_t* yella_lz4_decompress(const uint8_t* const bytes, size_t* size)
{
    uint8_t* decmp = NULL;
    int buf_size = (*size < 10) ? 10 : *size;
    int rc;

    buf_size = (buf_size << 8) - buf_size - 2526;
    decmp = malloc(buf_size);
    rc = LZ4_decompress_safe((const char*)bytes, (char*)decmp, *size, buf_size);
    if (rc < 0)
    {
        buf_size *= 2;
        decmp = realloc(decmp, buf_size);
        rc = LZ4_decompress_safe((const char*)bytes, (char*)decmp, *size, buf_size);
    }
    if (rc < 0)
    {
        CHUCHO_C_ERROR("yella.common", "Unable to decompress with LZ4");
        free(decmp);
        *size = 0;
        return NULL;
    }
    decmp = realloc(decmp, rc);
    *size = rc;
    return decmp;
}

