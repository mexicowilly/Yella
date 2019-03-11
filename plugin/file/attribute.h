#ifndef YELLA_ATTRIBUTE_H__
#define YELLA_ATTRIBUTE_H__

#include "export.h"
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
        int integer;
        struct
        {
            uint8_t* mem;
            size_t sz;
        } byte_array;
    } value;
} attribute;

YELLA_PRIV_EXPORT int compare_attributes(const attribute* const lhs, const attribute* const rhs);
YELLA_PRIV_EXPORT attribute* create_attribute_from_table(const yella_fb_file_attr_table_t tbl);
YELLA_PRIV_EXPORT void destroy_attribute(attribute* attr);
YELLA_PRIV_EXPORT yella_fb_file_attr_ref_t pack_attribute(const attribute* const attr, flatcc_builder_t* bld);

#endif
