#ifndef YELLA_ATTRIBUTE_H__
#define YELLA_ATTRIBUTE_H__

#include "export.h"
#include "file_builder.h"
#include "common/ptr_vector.h"
#include <unicode/ustring.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>

typedef enum
{
    ATTR_TYPE_FILE_TYPE,
    ATTR_TYPE_SHA256,
    ATTR_TYPE_POSIX_PERMISSIONS,
    ATTR_TYPE_USER,
    ATTR_TYPE_GROUP,
    ATTR_TYPE_SIZE,
    ATTR_TYPE_ACCESS_TIME,
    ATTR_TYPE_METADATA_CHANGE_TIME,
    ATTR_TYPE_MODIFICATION_TIME,
    ATTR_TYPE_POSIX_ACL
} attribute_type;

typedef struct posix_permission
{
    bool read;
    bool write;
    bool execute;
} posix_permission;

typedef struct posix_permissions
{
    posix_permission owner;
    posix_permission group;
    posix_permission other;
    bool set_uid;
    bool set_gid;
    bool sticky;
} posix_permissions;

typedef enum
{
    PACL_ENTRY_TYPE_USER,
    PACL_ENTRY_TYPE_GROUP,
    PACL_ENTRY_TYPE_MASK,
    PACL_ENTRY_TYPE_USER_OBJ,
    PACL_ENTRY_TYPE_GROUP_OBJ,
    PACL_ENTRY_TYPE_OTHER
} posix_acl_entry_type;

typedef struct user_group
{
    uint64_t id;
    UChar* name;
} user_group;

typedef struct posix_acl_entry
{
    posix_acl_entry_type type;
    user_group usr_grp;
    posix_permission perm;
} posix_acl_entry;

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
        user_group usr_grp;
        size_t size;
        uint64_t millis_since_epoch;
        yella_ptr_vector* posix_acl_entries;
    } value;
} attribute;

YELLA_PRIV_EXPORT uint16_t attribute_type_to_fb(attribute_type atp);
YELLA_PRIV_EXPORT int compare_attributes(const attribute* const lhs, const attribute* const rhs);
YELLA_PRIV_EXPORT attribute* create_attribute_from_table(const yella_fb_file_attr_table_t tbl);
YELLA_PRIV_EXPORT void destroy_attribute(attribute* attr);
YELLA_PRIV_EXPORT attribute_type fb_to_attribute_type(uint16_t fb);
YELLA_PRIV_EXPORT yella_fb_file_attr_ref_t pack_attribute(const attribute* const attr, flatcc_builder_t* bld);

#endif
