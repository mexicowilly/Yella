#ifndef YELLA_ATTRIBUTE_H__
#define YELLA_ATTRIBUTE_H__

#include "export.h"
#include "file_builder.h"
#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>

typedef enum
{
    ATTR_TYPE_FILE_TYPE,
    ATTR_TYPE_SHA256,
    ATTR_TYPE_POSIX_PERMISSIONS
} attribute_type;

typedef struct posix_permissions
{
    bool owner_read;
    bool owner_write;
    bool owner_execute;
    bool group_read;
    bool group_write;
    bool group_execute;
    bool other_read;
    bool other_write;
    bool other_execute;
    bool set_uid;
    bool set_gid;
    bool sticky;
} posix_permissions;

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
        posix_permissions psx_permissions;
    } value;
} attribute;

YELLA_PRIV_EXPORT uint16_t attribute_type_to_fb(attribute_type atp);
YELLA_PRIV_EXPORT int compare_attributes(const attribute* const lhs, const attribute* const rhs);
YELLA_PRIV_EXPORT attribute* create_attribute_from_table(const yella_fb_file_attr_table_t tbl);
YELLA_PRIV_EXPORT void destroy_attribute(attribute* attr);
YELLA_PRIV_EXPORT attribute_type fb_to_attribute_type(uint16_t fb);
YELLA_PRIV_EXPORT yella_fb_file_attr_ref_t pack_attribute(const attribute* const attr, flatcc_builder_t* bld);

#endif
