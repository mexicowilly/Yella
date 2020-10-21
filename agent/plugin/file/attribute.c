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

static void acl_entry_destructor(void* elem, void* udata)
{
    posix_acl_entry* entry = (posix_acl_entry*)elem;

    free(entry->usr_grp.name);
    free(entry);
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
    case ATTR_TYPE_POSIX_ACL:
        result = yella_fb_file_attr_type_POSIX_ACL;
        break;
    default:
        assert(false);
    }
    return result;
}

int compare_attributes(const attribute* const lhs, const attribute* const rhs)
{
    int rc;
    size_t i;
    posix_acl_entry* e1;
    posix_acl_entry* e2;

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
            rc = (int64_t)lhs->value.usr_grp.id - (int64_t)rhs->value.usr_grp.id;
            if (rc == 0)
                rc = u_strcmp(lhs->value.usr_grp.name, rhs->value.usr_grp.name);
            break;
        case ATTR_TYPE_SIZE:
            rc = (int64_t)lhs->value.size - (int64_t)rhs->value.size;
            break;
        case ATTR_TYPE_ACCESS_TIME:
        case ATTR_TYPE_METADATA_CHANGE_TIME:
        case ATTR_TYPE_MODIFICATION_TIME:
            rc = (int64_t)lhs->value.millis_since_epoch - (int64_t)rhs->value.millis_since_epoch;
            break;
        case ATTR_TYPE_POSIX_ACL:
            rc = (int64_t)yella_ptr_vector_size(lhs->value.posix_acl_entries) - (int64_t)yella_ptr_vector_size(rhs->value.posix_acl_entries);
            if (rc == 0 && yella_ptr_vector_size(lhs->value.posix_acl_entries) > 0)
            {
                for (i = 0; i < yella_ptr_vector_size(lhs->value.posix_acl_entries); i++)
                {
                    e1 = yella_ptr_vector_at(lhs->value.posix_acl_entries, i);
                    e2 = yella_ptr_vector_at(rhs->value.posix_acl_entries, i);
                    rc = e1->type - e2->type;
                    if (rc == 0)
                    {
                        rc = (int64_t)e1->usr_grp.id - (int64_t)e2->usr_grp.id;
                        if (rc == 0)
                        {
                            rc = u_strcmp(e1->usr_grp.name, e2->usr_grp.name);
                            if (rc == 0)
                            {
                                rc = memcmp(&e1->perm, &e2->perm, sizeof(struct posix_permission));
                            }
                        }
                    }
                }
            }
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
    yella_fb_file_posix_permissions_table_t psx_perms;
    yella_fb_file_user_group_table_t usr_grp;
    yella_fb_file_posix_permission_table_t psx_perm;
    yella_fb_file_posix_access_control_entry_vec_t acl_entries;
    yella_fb_file_posix_access_control_entry_table_t fb_entry;
    size_t i;
    posix_acl_entry* entry;

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
        memset(&result->value.psx_permissions, 0, sizeof(posix_permissions));
        psx_perms = yella_fb_file_attr_psx_permissions(tbl);
        if (yella_fb_file_posix_permissions_owner_is_present(psx_perms))
        {
            psx_perm = yella_fb_file_posix_permissions_owner(psx_perms);
            result->value.psx_permissions.owner.read = yella_fb_file_posix_permission_read(psx_perm) ? true : false;
            result->value.psx_permissions.owner.write = yella_fb_file_posix_permission_write(psx_perm) ? true : false;
            result->value.psx_permissions.owner.execute = yella_fb_file_posix_permission_execute(psx_perm) ? true : false;
        }
        if (yella_fb_file_posix_permissions_group_is_present(psx_perms))
        {
            psx_perm = yella_fb_file_posix_permissions_group(psx_perms);
            result->value.psx_permissions.group.read = yella_fb_file_posix_permission_read(psx_perm) ? true : false;
            result->value.psx_permissions.group.write = yella_fb_file_posix_permission_write(psx_perm) ? true : false;
            result->value.psx_permissions.group.execute = yella_fb_file_posix_permission_execute(psx_perm) ? true : false;
        }
        if (yella_fb_file_posix_permissions_other_is_present(psx_perms))
        {
            psx_perm = yella_fb_file_posix_permissions_other(psx_perms);
            result->value.psx_permissions.other.read = yella_fb_file_posix_permission_read(psx_perm) ? true : false;
            result->value.psx_permissions.other.write = yella_fb_file_posix_permission_write(psx_perm) ? true : false;
            result->value.psx_permissions.other.execute = yella_fb_file_posix_permission_execute(psx_perm) ? true : false;
        }
        result->value.psx_permissions.set_uid = yella_fb_file_posix_permissions_set_uid(psx_perms) ? true : false;
        result->value.psx_permissions.set_gid = yella_fb_file_posix_permissions_set_gid(psx_perms) ? true : false;
        result->value.psx_permissions.sticky = yella_fb_file_posix_permissions_sticky(psx_perms) ? true : false;
        break;
    case yella_fb_file_attr_type_USER:
    case yella_fb_file_attr_type_GROUP:
        result->type = (yella_fb_file_attr_type(tbl) == yella_fb_file_attr_type_USER) ? ATTR_TYPE_USER : ATTR_TYPE_GROUP;
        usr_grp = yella_fb_file_attr_usr_grp(tbl);
        result->value.usr_grp.id = yella_fb_file_user_group_id(usr_grp);
        result->value.usr_grp.name = yella_from_utf8(yella_fb_file_user_group_name(usr_grp));
        break;
    case yella_fb_file_attr_type_SIZE:
        result->type = ATTR_TYPE_SIZE;
        result->value.size = yella_fb_file_attr_unsigned_int(tbl);
        break;
    case yella_fb_file_attr_type_ACCESS_TIME:
        result->type = ATTR_TYPE_ACCESS_TIME;
        result->value.millis_since_epoch = yella_fb_file_attr_milliseconds_since_epoch(tbl);
        break;
    case yella_fb_file_attr_type_METADATA_CHANGE_TIME:
        result->type = ATTR_TYPE_METADATA_CHANGE_TIME;
        result->value.millis_since_epoch = yella_fb_file_attr_milliseconds_since_epoch(tbl);
        break;
    case yella_fb_file_attr_type_MODIFICATION_TIME:
        result->type = ATTR_TYPE_MODIFICATION_TIME;
        result->value.millis_since_epoch = yella_fb_file_attr_milliseconds_since_epoch(tbl);
        break;
    case yella_fb_file_attr_type_POSIX_ACL:
        result->type = ATTR_TYPE_POSIX_ACL;
        result->value.posix_acl_entries = yella_create_ptr_vector();
        yella_set_ptr_vector_destructor(result->value.posix_acl_entries, acl_entry_destructor, NULL);
        if (yella_fb_file_attr_psx_acl_is_present(tbl))
        {
            acl_entries = yella_fb_file_attr_psx_acl(tbl);
            for (i = 0; i < yella_fb_file_posix_access_control_entry_vec_len(acl_entries); i++)
            {
                fb_entry = yella_fb_file_posix_access_control_entry_vec_at(acl_entries, i);
                entry = malloc(sizeof(posix_acl_entry));
                entry->type = yella_fb_file_posix_access_control_entry_type(fb_entry) == yella_fb_file_posix_access_control_entry_type_USER ?
                              PACL_ENTRY_TYPE_USER : PACL_ENTRY_TYPE_GROUP;
                usr_grp = yella_fb_file_posix_access_control_entry_usr_grp(fb_entry);
                entry->usr_grp.id = yella_fb_file_user_group_id(usr_grp);
                entry->usr_grp.name = yella_from_utf8(yella_fb_file_user_group_name(usr_grp));
                psx_perm = yella_fb_file_posix_access_control_entry_permission(fb_entry);
                entry->perm.read = yella_fb_file_posix_permission_read(psx_perm) ? true : false;
                entry->perm.write = yella_fb_file_posix_permission_write(psx_perm) ? true : false;
                entry->perm.execute = yella_fb_file_posix_permission_execute(psx_perm) ? true : false;
                yella_push_back_ptr_vector(result->value.posix_acl_entries, entry);
            }
        }
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
        free(attr->value.usr_grp.name);
        break;
    case ATTR_TYPE_POSIX_ACL:
        yella_destroy_ptr_vector(attr->value.posix_acl_entries);
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
    case yella_fb_file_attr_type_POSIX_ACL:
        result = ATTR_TYPE_POSIX_ACL;
        break;
    default:
        assert(false);
    }
    return result;
}

yella_fb_file_attr_ref_t pack_attribute(const attribute* const attr, flatcc_builder_t* bld)
{
    yella_fb_file_attr_type_enum_t fb_type;
    char* utf8;
    size_t i;
    posix_acl_entry* entry;

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
        if (attr->value.psx_permissions.owner.read ||
            attr->value.psx_permissions.owner.write ||
            attr->value.psx_permissions.owner.execute)
        {
            yella_fb_file_posix_permissions_owner_add(bld,
                                                      yella_fb_file_posix_permission_create(bld,
                                                                                            attr->value.psx_permissions.owner.read,
                                                                                            attr->value.psx_permissions.owner.write,
                                                                                            attr->value.psx_permissions.owner.execute));
        }
        if (attr->value.psx_permissions.group.read ||
            attr->value.psx_permissions.group.write ||
            attr->value.psx_permissions.group.execute)
        {
            yella_fb_file_posix_permissions_group_add(bld,
                                                      yella_fb_file_posix_permission_create(bld,
                                                                                            attr->value.psx_permissions.group.read,
                                                                                            attr->value.psx_permissions.group.write,
                                                                                            attr->value.psx_permissions.group.execute));
        }
        if (attr->value.psx_permissions.other.read ||
            attr->value.psx_permissions.other.write ||
            attr->value.psx_permissions.other.execute)
        {
            yella_fb_file_posix_permissions_other_add(bld,
                                                      yella_fb_file_posix_permission_create(bld,
                                                                                            attr->value.psx_permissions.other.read,
                                                                                            attr->value.psx_permissions.other.write,
                                                                                            attr->value.psx_permissions.other.execute));
        }
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
        yella_fb_file_user_group_id_add(bld, attr->value.usr_grp.id);
        utf8 = yella_to_utf8(attr->value.usr_grp.name);
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
        yella_fb_file_attr_milliseconds_since_epoch_add(bld, attr->value.millis_since_epoch);
        break;
    case ATTR_TYPE_METADATA_CHANGE_TIME:
        fb_type = yella_fb_file_attr_type_METADATA_CHANGE_TIME;
        yella_fb_file_attr_milliseconds_since_epoch_add(bld, attr->value.millis_since_epoch);
        break;
    case ATTR_TYPE_MODIFICATION_TIME:
        fb_type = yella_fb_file_attr_type_MODIFICATION_TIME;
        yella_fb_file_attr_milliseconds_since_epoch_add(bld, attr->value.millis_since_epoch);
        break;
    case ATTR_TYPE_POSIX_ACL:
        fb_type = yella_fb_file_attr_type_POSIX_ACL;
        if (yella_ptr_vector_size(attr->value.posix_acl_entries) > 0)
        {
            yella_fb_file_posix_access_control_entry_vec_start(bld);
            for (i = 0; i < yella_ptr_vector_size(attr->value.posix_acl_entries); i++)
            {
                entry = yella_ptr_vector_at(attr->value.posix_acl_entries, i);
                yella_fb_file_posix_access_control_entry_start(bld);
                yella_fb_file_posix_access_control_entry_type_add(bld, (entry->type == PACL_ENTRY_TYPE_USER) ?
                    yella_fb_file_posix_access_control_entry_type_USER : yella_fb_file_posix_access_control_entry_type_GROUP);
                utf8 = yella_to_utf8(entry->usr_grp.name);
                yella_fb_file_posix_access_control_entry_usr_grp_add(bld, yella_fb_file_user_group_create(bld, entry->usr_grp.id, flatbuffers_string_create_str(bld, utf8)));
                free(utf8);
                yella_fb_file_posix_access_control_entry_permission_add(bld, yella_fb_file_posix_permission_create(bld, entry->perm.read, entry->perm.write, entry->perm.execute));
                yella_fb_file_posix_access_control_entry_vec_push(bld, yella_fb_file_posix_access_control_entry_end(bld));
            }
            yella_fb_file_attr_psx_acl_add(bld, yella_fb_file_posix_access_control_entry_vec_end(bld));
        }
        break;
    default:
        assert(false);
    }
    yella_fb_file_attr_type_add(bld, fb_type);
    return yella_fb_file_attr_end(bld);
}
