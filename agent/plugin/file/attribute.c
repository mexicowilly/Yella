#include "attribute.h"
#include "common/file.h"
#include "common/text_util.h"
#include <string.h>

static yella_file_type fb_to_file_type(yella_fb_file_file_type_enum_t fbt)
{
    yella_file_type result;

    switch (fbt)
    {
    case yella_fb_file_file_type_BLOCK_SPECIAL:
        result = YELLA_FILE_TYPE_BLOCK_SPECIAL;
        break;
    case yella_fb_file_file_type_CHARACTER_SPECIAL:
        result = YELLA_FILE_TYPE_CHARACTER_SPECIAL;
        break;
    case yella_fb_file_file_type_DIRECTORY:
        result = YELLA_FILE_TYPE_DIRECTORY;
        break;
    case yella_fb_file_file_type_FIFO:
        result = YELLA_FILE_TYPE_FIFO;
        break;
    case yella_fb_file_file_type_SYMBOLIC_LINK:
        result = YELLA_FILE_TYPE_SYMBOLIC_LINK;
        break;
    case yella_fb_file_file_type_REGULAR:
        result = YELLA_FILE_TYPE_REGULAR;
        break;
    case yella_fb_file_file_type_SOCKET:
        result = YELLA_FILE_TYPE_SOCKET;
        break;
    case yella_fb_file_file_type_WHITEOUT:
        result = YELLA_FILE_TYPE_WHITEOUT;
        break;
    default:
        assert(false);
    }
    return result;
}

static yella_fb_file_attr_type_enum_t file_type_to_fb(yella_file_type ft)
{
    yella_fb_file_file_type_enum_t result;

    switch (ft)
    {
    case YELLA_FILE_TYPE_BLOCK_SPECIAL:
        result = yella_fb_file_file_type_BLOCK_SPECIAL;
        break;
    case YELLA_FILE_TYPE_CHARACTER_SPECIAL:
        result = yella_fb_file_file_type_CHARACTER_SPECIAL;
        break;
    case YELLA_FILE_TYPE_DIRECTORY:
        result = yella_fb_file_file_type_DIRECTORY;
        break;
    case YELLA_FILE_TYPE_FIFO:
        result = yella_fb_file_file_type_FIFO;
        break;
    case YELLA_FILE_TYPE_SYMBOLIC_LINK:
        result = yella_fb_file_file_type_SYMBOLIC_LINK;
        break;
    case YELLA_FILE_TYPE_REGULAR:
        result = yella_fb_file_file_type_REGULAR;
        break;
    case YELLA_FILE_TYPE_SOCKET:
        result = yella_fb_file_file_type_SOCKET;
        break;
    case YELLA_FILE_TYPE_WHITEOUT:
        result = yella_fb_file_file_type_WHITEOUT;
        break;
    default:
        assert(false);
    }
    return result;
}

uint16_t attribute_type_to_fb(attribute_type atp)
{
    uint16_t result;

    switch (atp)
    {
    case ATTR_TYPE_FILE_TYPE:
        result = yella_fb_file_attr_type_FILE_TYPE;
        break;
    case ATTR_TYPE_SHA256:
        result = yella_fb_file_attr_type_SHA256;
        break;
    case ATTR_TYPE_POSIX_PERMISSIONS:
        result = yella_fb_file_attr_type_POSIX_PERMISSIONS;
        break;
    case ATTR_TYPE_USER:
        result = yella_fb_file_attr_type_USER;
        break;
    case ATTR_TYPE_GROUP:
        result = yella_fb_file_attr_type_GROUP;
        break;
    case ATTR_TYPE_SIZE:
        result = yella_fb_file_attr_type_SIZE;
        break;
    case ATTR_TYPE_ACCESS_TIME:
        result = yella_fb_file_attr_type_ACCESS_TIME;
        break;
    case ATTR_TYPE_METADATA_CHANGE_TIME:
        result = yella_fb_file_attr_type_METADATA_CHANGE_TIME;
        break;
    case ATTR_TYPE_MODIFICATION_TIME:
        result = yella_fb_file_attr_type_MODIFICATION_TIME;
        break;
    default:
        assert(false);
    }
    return result;
}

int compare_attributes(const attribute* const lhs, const attribute* const rhs)
{
    int rc;

    rc = (int)lhs->type - (int)rhs->type;
    if (rc == 0)
    {
        switch (lhs->type)
        {
        case ATTR_TYPE_FILE_TYPE:
            rc = lhs->value.integer - rhs->value.integer;
            break;
        case ATTR_TYPE_SHA256:
            rc = (int64_t)lhs->value.byte_array.sz - (int64_t)rhs->value.byte_array.sz;
            if (rc == 0)
                rc = memcmp(lhs->value.byte_array.mem, rhs->value.byte_array.mem, lhs->value.byte_array.sz);
            break;
        case ATTR_TYPE_POSIX_PERMISSIONS:
            rc = memcmp(&lhs->value.psx_permissions, &rhs->value.psx_permissions, sizeof(posix_permissions));
            break;
        case ATTR_TYPE_USER:
        case ATTR_TYPE_GROUP:
            rc = (int64_t)lhs->value.user_group.id - (int64_t)rhs->value.user_group.id;
            if (rc == 0)
                rc = u_strcmp(lhs->value.user_group.name, rhs->value.user_group.name);
            break;
        case ATTR_TYPE_SIZE:
            rc = (int64_t)lhs->value.size - (int64_t)rhs->value.size;
            break;
        case ATTR_TYPE_ACCESS_TIME:
        case ATTR_TYPE_METADATA_CHANGE_TIME:
        case ATTR_TYPE_MODIFICATION_TIME:
            rc = (int64_t)lhs->value.millis_since_epoch - (int64_t)rhs->value.millis_since_epoch;
            break;
        default:
            assert(false);
        }
    }
    return rc;
}

attribute* create_attribute_from_table(const yella_fb_file_attr_table_t tbl)
{
    attribute* result;
    flatbuffers_uint8_vec_t bytes;
    yella_fb_file_posix_permissions_table_t psx_permissions;
    yella_fb_file_user_group_table_t usr_grp;

    result = malloc(sizeof(attribute));
    switch (yella_fb_file_attr_type(tbl))
    {
    case yella_fb_file_attr_type_FILE_TYPE:
        result->type = ATTR_TYPE_FILE_TYPE;
        result->value.integer = fb_to_file_type(yella_fb_file_attr_ftype(tbl));
        break;
    case yella_fb_file_attr_type_SHA256:
        result->type = ATTR_TYPE_SHA256;
        bytes = yella_fb_file_attr_bytes(tbl);
        result->value.byte_array.sz = flatbuffers_uint8_vec_len(bytes);
        result->value.byte_array.mem = malloc(result->value.byte_array.sz);
        memcpy(result->value.byte_array.mem, bytes, result->value.byte_array.sz);
        break;
    case yella_fb_file_attr_type_POSIX_PERMISSIONS:
        result->type = ATTR_TYPE_POSIX_PERMISSIONS;
        psx_permissions = yella_fb_file_attr_psx_permissions(tbl);
        result->value.psx_permissions.owner_read = yella_fb_file_posix_permissions_owner_read(psx_permissions) ? true : false;
        result->value.psx_permissions.owner_write = yella_fb_file_posix_permissions_owner_write(psx_permissions) ? true : false;
        result->value.psx_permissions.owner_execute = yella_fb_file_posix_permissions_owner_execute(psx_permissions) ? true : false;
        result->value.psx_permissions.group_read = yella_fb_file_posix_permissions_group_read(psx_permissions) ? true : false;
        result->value.psx_permissions.group_write = yella_fb_file_posix_permissions_group_write(psx_permissions) ? true : false;
        result->value.psx_permissions.group_execute = yella_fb_file_posix_permissions_group_execute(psx_permissions) ? true : false;
        result->value.psx_permissions.other_read = yella_fb_file_posix_permissions_other_read(psx_permissions) ? true : false;
        result->value.psx_permissions.other_write = yella_fb_file_posix_permissions_other_write(psx_permissions) ? true : false;
        result->value.psx_permissions.other_execute = yella_fb_file_posix_permissions_other_execute(psx_permissions) ? true : false;
        result->value.psx_permissions.set_uid = yella_fb_file_posix_permissions_set_uid(psx_permissions) ? true : false;
        result->value.psx_permissions.set_gid = yella_fb_file_posix_permissions_set_gid(psx_permissions) ? true : false;
        result->value.psx_permissions.sticky = yella_fb_file_posix_permissions_sticky(psx_permissions) ? true : false;
        break;
    case yella_fb_file_attr_type_USER:
    case yella_fb_file_attr_type_GROUP:
        result->type = (yella_fb_file_attr_type(tbl) == yella_fb_file_attr_type_USER) ? ATTR_TYPE_USER : ATTR_TYPE_GROUP;
        usr_grp = yella_fb_file_attr_usr_grp(tbl);
        result->value.user_group.id = yella_fb_file_user_group_id(usr_grp);
        result->value.user_group.name = yella_from_utf8(yella_fb_file_user_group_name(usr_grp));
        break;
    case yella_fb_file_attr_type_SIZE:
        result->type = ATTR_TYPE_SIZE;
        result->value.size = yella_fb_file_attr_unsigned_int(tbl);
        break;
    case yella_fb_file_attr_type_ACCESS_TIME:
        result->type = ATTR_TYPE_ACCESS_TIME;
        result->value.millis_since_epoch = yella_fb_file_attr_millis_since_epoch(tbl);
        break;
    case yella_fb_file_attr_type_METADATA_CHANGE_TIME:
        result->type = ATTR_TYPE_METADATA_CHANGE_TIME;
        result->value.millis_since_epoch = yella_fb_file_attr_millis_since_epoch(tbl);
        break;
    case yella_fb_file_attr_type_MODIFICATION_TIME:
        result->type = ATTR_TYPE_MODIFICATION_TIME;
        result->value.millis_since_epoch = yella_fb_file_attr_millis_since_epoch(tbl);
        break;
    default:
        assert(false);
    }
    return result;
}

void destroy_attribute(attribute* attr)
{
    switch (attr->type)
    {
    case ATTR_TYPE_SHA256:
        free(attr->value.byte_array.mem);
        break;
    case ATTR_TYPE_USER:
    case ATTR_TYPE_GROUP:
        free(attr->value.user_group.name);
        break;
    default:
        break;
    }
    free(attr);
}

attribute_type fb_to_attribute_type(uint16_t fb)
{
    attribute_type result;

    switch (fb)
    {
    case yella_fb_file_attr_type_FILE_TYPE:
        result = ATTR_TYPE_FILE_TYPE;
        break;
    case yella_fb_file_attr_type_SHA256:
        result = ATTR_TYPE_SHA256;
        break;
    case yella_fb_file_attr_type_POSIX_PERMISSIONS:
        result = ATTR_TYPE_POSIX_PERMISSIONS;
        break;
    case yella_fb_file_attr_type_USER:
        result = ATTR_TYPE_USER;
        break;
    case yella_fb_file_attr_type_GROUP:
        result = ATTR_TYPE_GROUP;
        break;
    case yella_fb_file_attr_type_SIZE:
        result = ATTR_TYPE_SIZE;
        break;
    case yella_fb_file_attr_type_ACCESS_TIME:
        result = ATTR_TYPE_ACCESS_TIME;
        break;
    case yella_fb_file_attr_type_METADATA_CHANGE_TIME:
        result = ATTR_TYPE_METADATA_CHANGE_TIME;
        break;
    case yella_fb_file_attr_type_MODIFICATION_TIME:
        result = ATTR_TYPE_MODIFICATION_TIME;
        break;
    default:
        assert(false);
    }
    return result;
}

yella_fb_file_attr_ref_t pack_attribute(const attribute* const attr, flatcc_builder_t* bld)
{
    yella_fb_file_attr_type_enum_t fb_type;
    yella_fb_file_posix_permissions_table_t psx_permissions;
    char* utf8;

    yella_fb_file_attr_start(bld);
    switch (attr->type)
    {
    case ATTR_TYPE_FILE_TYPE:
        fb_type = yella_fb_file_attr_type_FILE_TYPE;
        yella_fb_file_attr_ftype_add(bld, file_type_to_fb(attr->value.integer));
        break;
    case ATTR_TYPE_SHA256:
        fb_type = yella_fb_file_attr_type_SHA256;
        yella_fb_file_attr_bytes_add(bld,
                                     flatbuffers_uint8_vec_create(bld,
                                                                  attr->value.byte_array.mem,
                                                                  attr->value.byte_array.sz));
        break;
    case ATTR_TYPE_POSIX_PERMISSIONS:
        fb_type = yella_fb_file_attr_type_POSIX_PERMISSIONS;
        yella_fb_file_posix_permissions_start(bld);
        if (attr->value.psx_permissions.owner_read)
            yella_fb_file_posix_permissions_owner_read_add(bld, true);
        if (attr->value.psx_permissions.owner_write)
            yella_fb_file_posix_permissions_owner_write_add(bld, true);
        if (attr->value.psx_permissions.owner_execute)
            yella_fb_file_posix_permissions_owner_execute_add(bld, true);
        if (attr->value.psx_permissions.group_read)
            yella_fb_file_posix_permissions_group_read_add(bld, true);
        if (attr->value.psx_permissions.group_write)
            yella_fb_file_posix_permissions_group_write_add(bld, true);
        if (attr->value.psx_permissions.group_execute)
            yella_fb_file_posix_permissions_group_execute_add(bld, true);
        if (attr->value.psx_permissions.other_read)
            yella_fb_file_posix_permissions_other_read_add(bld, true);
        if (attr->value.psx_permissions.other_write)
            yella_fb_file_posix_permissions_other_write_add(bld, true);
        if (attr->value.psx_permissions.other_execute)
            yella_fb_file_posix_permissions_other_execute_add(bld, true);
        if (attr->value.psx_permissions.set_uid)
            yella_fb_file_posix_permissions_set_uid_add(bld, true);
        if (attr->value.psx_permissions.set_gid)
            yella_fb_file_posix_permissions_set_gid_add(bld, true);
        if (attr->value.psx_permissions.sticky)
            yella_fb_file_posix_permissions_sticky_add(bld, true);
        yella_fb_file_attr_psx_permissions_add(bld, yella_fb_file_posix_permissions_end(bld));
        break;
    case ATTR_TYPE_USER:
    case ATTR_TYPE_GROUP:
        fb_type = (attr->type == ATTR_TYPE_USER) ? yella_fb_file_attr_type_USER : yella_fb_file_attr_type_GROUP;
        yella_fb_file_user_group_start(bld);
        yella_fb_file_user_group_id_add(bld, attr->value.user_group.id);
        utf8 = yella_to_utf8(attr->value.user_group.name);
        yella_fb_file_user_group_name_create_str(bld, utf8);
        free(utf8);
        yella_fb_file_attr_usr_grp_add(bld, yella_fb_file_user_group_end(bld));
        break;
    case ATTR_TYPE_SIZE:
        fb_type = yella_fb_file_attr_type_SIZE;
        yella_fb_file_attr_unsigned_int_add(bld, attr->value.size);
        break;
    case ATTR_TYPE_ACCESS_TIME:
        fb_type = yella_fb_file_attr_type_ACCESS_TIME;
        yella_fb_file_attr_millis_since_epoch_add(bld, attr->value.millis_since_epoch);
        break;
    case ATTR_TYPE_METADATA_CHANGE_TIME:
        fb_type = yella_fb_file_attr_type_METADATA_CHANGE_TIME;
        yella_fb_file_attr_millis_since_epoch_add(bld, attr->value.millis_since_epoch);
        break;
    case ATTR_TYPE_MODIFICATION_TIME:
        fb_type = yella_fb_file_attr_type_MODIFICATION_TIME;
        yella_fb_file_attr_millis_since_epoch_add(bld, attr->value.millis_since_epoch);
        break;
    default:
        assert(false);
    }
    yella_fb_file_attr_type_add(bld, fb_type);
    return yella_fb_file_attr_end(bld);
}
