/* SDSLib 2.0 -- A C dynamic strings library
 *
 * Copyright (c) 2006-2015, Salvatore Sanfilippo <antirez at gmail dot com>
 * Copyright (c) 2015, Oran Agra
 * Copyright (c) 2015, Redis Labs, Inc
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *   * Redistributions of source code must retain the above copyright notice,
 *     this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *   * Neither the name of Redis nor the names of its contributors may be used
 *     to endorse or promote products derived from this software without
 *     specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef __UDS_H
#define __UDS_H

#define UDS_MAX_PREALLOC (1024*1024)
extern const char *UDS_NOINIT;

#include "export.h"
#include <sys/types.h>
#include <stdarg.h>
#include <stdint.h>
#include <unicode/utypes.h>

typedef UChar* uds;

/* Note: sdshdr5 is never used, we just access the flags byte directly.
 * However is here to document the layout of type 5 SDS strings. */
struct __attribute__ ((packed)) udshdr5 {
    unsigned short flags; /* 3 lsb of type, and 5 msb of string length */
    UChar buf[];
};
struct __attribute__ ((packed)) udshdr8 {
    uint8_t len; /* used */
    uint8_t alloc; /* excluding the header and null terminator */
    unsigned short flags; /* 3 lsb of type, 5 unused bits */
    UChar buf[];
};
struct __attribute__ ((packed)) udshdr16 {
    uint16_t len; /* used */
    uint16_t alloc; /* excluding the header and null terminator */
    unsigned short flags; /* 3 lsb of type, 5 unused bits */
    UChar buf[];
};
struct __attribute__ ((packed)) udshdr32 {
    uint32_t len; /* used */
    uint32_t alloc; /* excluding the header and null terminator */
    unsigned short flags; /* 3 lsb of type, 5 unused bits */
    UChar buf[];
};
struct __attribute__ ((packed)) udshdr64 {
    uint64_t len; /* used */
    uint64_t alloc; /* excluding the header and null terminator */
    unsigned short flags; /* 3 lsb of type, 5 unused bits */
    UChar buf[];
};

#define UDS_TYPE_5  0
#define UDS_TYPE_8  1
#define UDS_TYPE_16 2
#define UDS_TYPE_32 3
#define UDS_TYPE_64 4
#define UDS_TYPE_MASK 7
#define UDS_TYPE_BITS 3
#define UDS_HDR_VAR(T,s) struct udshdr##T *sh = (struct udshdr##T *)(((char*)s)-(sizeof(struct udshdr##T)));
#define UDS_HDR(T,s) ((struct udshdr##T *)(((char*)s)-(sizeof(struct udshdr##T))))
#define UDS_TYPE_5_LEN(f) ((f)>>UDS_TYPE_BITS)
#define UDS_FLAGS(s) (*(((unsigned short*)s) - 1))

static inline size_t udslen(const uds s) {
    unsigned short flags = UDS_FLAGS(s);
    switch(flags&UDS_TYPE_MASK) {
        case UDS_TYPE_5:
            return UDS_TYPE_5_LEN(flags);
        case UDS_TYPE_8:
            return UDS_HDR(8,s)->len;
        case UDS_TYPE_16:
            return UDS_HDR(16,s)->len;
        case UDS_TYPE_32:
            return UDS_HDR(32,s)->len;
        case UDS_TYPE_64:
            return UDS_HDR(64,s)->len;
    }
    return 0;
}

static inline size_t udsavail(const uds s) {
    unsigned short flags = UDS_FLAGS(s);
    switch(flags&UDS_TYPE_MASK) {
        case UDS_TYPE_5: {
            return 0;
        }
        case UDS_TYPE_8: {
            UDS_HDR_VAR(8,s);
            return sh->alloc - sh->len;
        }
        case UDS_TYPE_16: {
            UDS_HDR_VAR(16,s);
            return sh->alloc - sh->len;
        }
        case UDS_TYPE_32: {
            UDS_HDR_VAR(32,s);
            return sh->alloc - sh->len;
        }
        case UDS_TYPE_64: {
            UDS_HDR_VAR(64,s);
            return sh->alloc - sh->len;
        }
    }
    return 0;
}

static inline void udssetlen(uds s, size_t newlen) {
    unsigned short flags = UDS_FLAGS(s);
    switch(flags&UDS_TYPE_MASK) {
        case UDS_TYPE_5:
            {
                unsigned short *fp = ((unsigned short*)s)-1;
                *fp = UDS_TYPE_5 | (newlen << UDS_TYPE_BITS);
            }
            break;
        case UDS_TYPE_8:
            UDS_HDR(8,s)->len = newlen;
            break;
        case UDS_TYPE_16:
            UDS_HDR(16,s)->len = newlen;
            break;
        case UDS_TYPE_32:
            UDS_HDR(32,s)->len = newlen;
            break;
        case UDS_TYPE_64:
            UDS_HDR(64,s)->len = newlen;
            break;
    }
}

static inline void udsinclen(uds s, size_t inc) {
    unsigned short flags = UDS_FLAGS(s);
    switch(flags&UDS_TYPE_MASK) {
        case UDS_TYPE_5:
            {
                unsigned short *fp = ((unsigned short*)s)-1;
                unsigned char newlen = UDS_TYPE_5_LEN(flags)+inc;
                *fp = UDS_TYPE_5 | (newlen << UDS_TYPE_BITS);
            }
            break;
        case UDS_TYPE_8:
            UDS_HDR(8,s)->len += inc;
            break;
        case UDS_TYPE_16:
            UDS_HDR(16,s)->len += inc;
            break;
        case UDS_TYPE_32:
            UDS_HDR(32,s)->len += inc;
            break;
        case UDS_TYPE_64:
            UDS_HDR(64,s)->len += inc;
            break;
    }
}

/* sdsalloc() = sdsavail() + sdslen() */
static inline size_t udsalloc(const uds s) {
    unsigned short flags = UDS_FLAGS(s);
    switch(flags&UDS_TYPE_MASK) {
        case UDS_TYPE_5:
            return UDS_TYPE_5_LEN(flags);
        case UDS_TYPE_8:
            return UDS_HDR(8,s)->alloc;
        case UDS_TYPE_16:
            return UDS_HDR(16,s)->alloc;
        case UDS_TYPE_32:
            return UDS_HDR(32,s)->alloc;
        case UDS_TYPE_64:
            return UDS_HDR(64,s)->alloc;
    }
    return 0;
}

static inline void udssetalloc(uds s, size_t newlen) {
    unsigned short flags = UDS_FLAGS(s);
    switch(flags&UDS_TYPE_MASK) {
        case UDS_TYPE_5:
            /* Nothing to do, this type has no total allocation info. */
            break;
        case UDS_TYPE_8:
            UDS_HDR(8,s)->alloc = newlen;
            break;
        case UDS_TYPE_16:
            UDS_HDR(16,s)->alloc = newlen;
            break;
        case UDS_TYPE_32:
            UDS_HDR(32,s)->alloc = newlen;
            break;
        case UDS_TYPE_64:
            UDS_HDR(64,s)->alloc = newlen;
            break;
    }
}

YELLA_EXPORT uds udsnewlen(const void *init, size_t initlen);
YELLA_EXPORT uds udsnew(const UChar *init);
YELLA_EXPORT uds udsempty(void);
YELLA_EXPORT uds udsdup(const uds s);
YELLA_EXPORT void udsfree(uds s);
YELLA_EXPORT uds udsgrowzero(uds s, size_t len);
YELLA_EXPORT uds udscatlen(uds s, const void *t, size_t len);
YELLA_EXPORT uds udscat(uds s, const UChar *t);
YELLA_EXPORT uds udscatuds(uds s, const uds t);
YELLA_EXPORT uds udscpylen(uds s, const UChar *t, size_t len);
YELLA_EXPORT uds udscpy(uds s, const UChar *t);

YELLA_EXPORT uds udscatvprintf(uds s, const UChar *fmt, va_list ap);
#ifdef __GNUC__
YELLA_EXPORT uds udscatprintf(uds s, const UChar *fmt, ...);
#else
YELLA_EXPORT uds udscatprintf(uds s, const char *fmt, ...);
#endif

/* YELLA_EXPORT uds udscatfmt(uds s, char const *fmt, ...); */
YELLA_EXPORT uds udstrim(uds s, const UChar *cset);
YELLA_EXPORT void udsrange(uds s, ssize_t start, ssize_t end);
YELLA_EXPORT void udsupdatelen(uds s);
YELLA_EXPORT void udsclear(uds s);
//YELLA_EXPORT int udscmp(const uds s1, const uds s2);
//YELLA_EXPORT uds *udssplitlen(const char *s, ssize_t len, const char *sep, int seplen, int *count);
//YELLA_EXPORT void udsfreesplitres(uds *tokens, int count);
//YELLA_EXPORT void udstolower(uds s);
//YELLA_EXPORT void udstoupper(uds s);
//YELLA_EXPORT uds udsfromlonglong(long long value);
//YELLA_EXPORT uds udscatrepr(uds s, const char *p, size_t len);
//YELLA_EXPORT uds *udssplitargs(const char *line, int *argc);
//YELLA_EXPORT uds udsmapchars(uds s, const char *from, const char *to, size_t setlen);
YELLA_EXPORT uds udsjoin(UChar **argv, int argc, UChar *sep);
YELLA_EXPORT uds udsjoinuds(uds *argv, int argc, const UChar *sep, size_t seplen);

/* Low level functions exposed to the user API */
YELLA_EXPORT uds udsMakeRoomFor(uds s, size_t addlen);
YELLA_EXPORT void udsIncrLen(uds s, ssize_t incr);
YELLA_EXPORT uds udsRemoveFreeSpace(uds s);
YELLA_EXPORT size_t udsAllocSize(uds s);
YELLA_EXPORT void *udsAllocPtr(uds s);

/* Export the allocator used by SDS to the program using SDS.
 * Sometimes the program SDS is linked to, may use a different set of
 * allocators, but may want to allocate or free things that SDS will
 * respectively free or allocate. */
YELLA_EXPORT void *uds_malloc(size_t size);
YELLA_EXPORT void *uds_realloc(void *ptr, size_t size);
YELLA_EXPORT void uds_free(void *ptr);

#ifdef REDIS_TEST
YELLA_EXPORT int sdsTest(int argc, char *argv[]);
#endif

#endif
