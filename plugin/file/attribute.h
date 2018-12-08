#ifndef YELLA_ATTRIUTE_H__
#define YELLA_ATTRIUTE_H__

#include "file_builder.h"
#include <stdint.h>
#include <stdlib.h>

typedef enum
{
    ATTR_TYPE_FILE_TYPE,
    ATTR_TYPE_SHA256
} attribute_type;

typedef struct attribute
{
    attribute_type type;
    union
    {
        int int_type;
        struct
        {
            uint8_t* mem;
            size_t sz;
        } byte_array;
    } value;
} attribute;

int compare_attributes(const attribute* const lhs, const attribute* const rhs);
void destroy_attribute(attribute* attr);
yella_fb_file_attr_ref_t pack_attribute(const attribute* const attr, flatcc_builder_t* bld);

#endif
