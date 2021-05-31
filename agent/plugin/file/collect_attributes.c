#include "plugin/file/collect_attributes.h"
#include "plugin/file/stat_collector.h"
#include "plugin/file/posix_acl.h"
#include "common/file.h"
#include "common/text_util.h"
#include "attribute.h"
#include <openssl/evp.h>
#include <chucho/log.h>
#include <inttypes.h>

static void digest_callback(const uint8_t* const buf, size_t sz, void* udata)
{
    EVP_DigestUpdate((EVP_MD_CTX*)udata, buf, sz);
}

static void handle_access_time(element* elem, void* stat_buf)
{
    attribute* attr;

    attr = malloc(sizeof(attribute));
    attr->type = ATTR_TYPE_ACCESS_TIME;
    attr->value.millis_since_epoch = get_access_time(stat_buf);
    add_element_attribute(elem, attr);
}

static void handle_file_type(element* elem, yella_file_type ftype)
{
    attribute* attr;

    attr = malloc(sizeof(attribute));
    attr->type = ATTR_TYPE_FILE_TYPE;
    attr->value.integer = ftype;
    add_element_attribute(elem, attr);
}

static void handle_group(element* elem, void* stat_buf, chucho_logger_t* lgr)
{
    attribute* attr;

    attr = malloc(sizeof(attribute));
    attr->type = ATTR_TYPE_GROUP;
    get_group(stat_buf, &attr->value.usr_grp.id, &attr->value.usr_grp.name, lgr);
    add_element_attribute(elem, attr);
}

static void handle_metadata_change_time(element* elem, void* stat_buf)
{
    attribute* attr;

    attr = malloc(sizeof(attribute));
    attr->type = ATTR_TYPE_METADATA_CHANGE_TIME;
    attr->value.millis_since_epoch = get_metadata_change_time(stat_buf);
    add_element_attribute(elem, attr);
}

static void handle_modification_time(element* elem, void* stat_buf)
{
    attribute* attr;

    attr = malloc(sizeof(attribute));
    attr->type = ATTR_TYPE_MODIFICATION_TIME;
    attr->value.millis_since_epoch = get_modification_time(stat_buf);
    add_element_attribute(elem, attr);
}

static void handle_posix_acl(element* elem, chucho_logger_t* lgr)
{
    attribute* attr;
    yella_ptr_vector* acl;

    acl = get_posix_acl(elem, lgr);
    if (acl != NULL)
    {
        attr = malloc(sizeof(attribute));
        attr->type = ATTR_TYPE_POSIX_ACL;
        attr->value.posix_acl_entries = acl;
        add_element_attribute(elem, attr);
    }
}

static void handle_posix_permissions(element* elem, void* stat_buf)
{
    attribute* attr;

    attr = malloc(sizeof(attribute));
    attr->type = ATTR_TYPE_POSIX_PERMISSIONS;
    attr->value.psx_permissions = get_posix_permissions(stat_buf);
    add_element_attribute(elem, attr);
}

static bool handle_sha256(element* elem, yella_file_type ftype)
{
    EVP_MD_CTX* ctx;
    unsigned char md[EVP_MAX_MD_SIZE];
    unsigned md_len;
    yella_rc yrc;
    attribute* attr;
    bool rc = false;

    if (ftype == YELLA_FILE_TYPE_REGULAR || ftype == YELLA_FILE_TYPE_SYMBOLIC_LINK)
    {
        ctx = EVP_MD_CTX_new();
        EVP_DigestInit_ex(ctx, EVP_sha256(), NULL);
        yrc = yella_apply_function_to_file_contents(element_name(elem), digest_callback, ctx);
        if (yrc == YELLA_NO_ERROR)
        {
            EVP_DigestFinal_ex(ctx, md, &md_len);
            attr = malloc(sizeof(attribute));
            attr->type = ATTR_TYPE_SHA256;
            attr->value.byte_array.mem = malloc(md_len);
            attr->value.byte_array.sz = md_len;
            memcpy(attr->value.byte_array.mem, md, md_len);
            add_element_attribute(elem, attr);
            rc = true;
        }
        EVP_MD_CTX_free(ctx);
    }
    return rc;
}

static void handle_size(element* elem, void* stat_buf)
{
    attribute* attr;

    attr = malloc(sizeof(attribute));
    attr->type = ATTR_TYPE_SIZE;
    attr->value.size = get_size(stat_buf);
    add_element_attribute(elem, attr);
}

static void handle_user(element* elem, void* stat_buf, chucho_logger_t* lgr)
{
    attribute* attr;

    attr = malloc(sizeof(attribute));
    attr->type = ATTR_TYPE_USER;
    get_user(stat_buf, &attr->value.usr_grp.id, &attr->value.usr_grp.name, lgr);
    add_element_attribute(elem, attr);
}

element* collect_attributes(const UChar* const name,
                            const attribute_type* const attr_types,
                            size_t attr_type_count,
                            chucho_logger_t* lgr)
{
    element* result;
    yella_file_type ftype;
    size_t i;
    void* stat_buf;
    bool should_reset_access_time;
    bool should_get_access_time;

    if (yella_file_exists(name))
    {
        if (yella_get_file_type(name, &ftype, &stat_buf) != YELLA_NO_ERROR)
            return NULL;
        result = create_element(name);
        should_reset_access_time = false;
        should_get_access_time = false;
        for (i = 0; i < attr_type_count; i++)
        {
            switch (attr_types[i])
            {
            case ATTR_TYPE_FILE_TYPE:
                handle_file_type(result, ftype);
                break;
            case ATTR_TYPE_POSIX_PERMISSIONS:
                handle_posix_permissions(result, stat_buf);
                break;
            case ATTR_TYPE_USER:
                handle_user(result, stat_buf, lgr);
                break;
            case ATTR_TYPE_GROUP:
                handle_group(result, stat_buf, lgr);
                break;
            case ATTR_TYPE_SIZE:
                handle_size(result, stat_buf);
                break;
            case ATTR_TYPE_ACCESS_TIME:
                should_get_access_time = true;
                break;
            case ATTR_TYPE_METADATA_CHANGE_TIME:
                handle_metadata_change_time(result, stat_buf);
                break;
            case ATTR_TYPE_MODIFICATION_TIME:
                handle_modification_time(result, stat_buf);
                break;
            case ATTR_TYPE_POSIX_ACL:
                handle_posix_acl(result, lgr);
                break;
            case ATTR_TYPE_SHA256:
                should_reset_access_time = handle_sha256(result, ftype);
                break;
            }
        }
        if (should_reset_access_time)
            reset_access_time(name, stat_buf, lgr);
        if (should_get_access_time)
            handle_access_time(result, stat_buf);
        free(stat_buf);
    }
    else
    {
        result = NULL;
    }
    return result;
}
