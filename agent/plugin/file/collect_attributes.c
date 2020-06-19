#include "plugin/file/collect_attributes.h"
#include "plugin/file/posix_permissions.h"
#include "common/file.h"
#include "common/text_util.h"
#include "attribute.h"
#include <openssl/evp.h>

static void digest_callback(const uint8_t* const buf, size_t sz, void* udata)
{
    EVP_DigestUpdate((EVP_MD_CTX*)udata, buf, sz);
}

static void handle_file_type(element* elem, yella_file_type ftype)
{
    attribute* attr;

    attr = malloc(sizeof(attribute));
    attr->type = ATTR_TYPE_FILE_TYPE;
    attr->value.integer = ftype;
    add_element_attribute(elem, attr);
}

static void handle_posix_permissions(element* elem, const UChar* const name)
{
    attribute* attr;

    attr = malloc(sizeof(attribute));
    attr->type = ATTR_TYPE_POSIX_PERMISSIONS;
    attr->value.psx_permissions = get_posix_permissions(name);
    add_element_attribute(elem, attr);
}

static void handle_sha256(element* elem, const UChar* const name, yella_file_type ftype)
{
    EVP_MD_CTX* ctx;
    unsigned char md[EVP_MAX_MD_SIZE];
    unsigned md_len;
    yella_rc yrc;
    attribute* attr;

    if (ftype == YELLA_FILE_TYPE_REGULAR || ftype == YELLA_FILE_TYPE_SYMBOLIC_LINK)
    {
        ctx = EVP_MD_CTX_new();
        EVP_DigestInit_ex(ctx, EVP_sha256(), NULL);
        yrc = yella_apply_function_to_file_contents(name, digest_callback, ctx);
        if (yrc == YELLA_NO_ERROR)
        {
            EVP_DigestFinal_ex(ctx, md, &md_len);
            attr = malloc(sizeof(attribute));
            attr->type = ATTR_TYPE_SHA256;
            attr->value.byte_array.mem = malloc(md_len);
            attr->value.byte_array.sz = md_len;
            memcpy(attr->value.byte_array.mem, md, md_len);
            add_element_attribute(elem, attr);
        }
        EVP_MD_CTX_free(ctx);
    }
}

element* collect_attributes(const UChar* const name, attribute_type* attr_types, size_t attr_type_count)
{
    element* result;
    yella_file_type ftype;
    size_t i;

    if (yella_file_exists(name))
    {
        if (yella_get_file_type(name, &ftype) != YELLA_NO_ERROR)
            return NULL;
        result = create_element(name);
        for (i = 0; i < attr_type_count; i++)
        {
            switch (attr_types[i])
            {
            case ATTR_TYPE_FILE_TYPE:
                handle_file_type(result, ftype);
                break;
            case ATTR_TYPE_SHA256:
                handle_sha256(result, name, ftype);
                break;
            case ATTR_TYPE_POSIX_PERMISSIONS:
                handle_posix_permissions(result, name);
                break;
            }
        }
    }
    else
    {
        result = NULL;
    }
    return result;
}
