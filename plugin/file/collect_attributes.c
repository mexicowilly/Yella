#include "plugin/file/collect_attributes.h"
#include "common/file.h"
#include "common/text_util.h"
#include "attribute.h"
#include <openssl/evp.h>

static bool contains_attr_type(attribute_type tp, attribute_type* attr_types, size_t attr_type_count)
{
    int i;

    for (i = 0; i < attr_type_count; i++)
    {
        if (attr_types[i] == tp)
            return true;
    }
    return false;
}

static void digest_callback(const uint8_t* const buf, size_t sz, void* udata)
{
    EVP_DigestUpdate((EVP_MD_CTX*)udata, buf, sz);
}

element* collect_attributes(const UChar* const name, attribute_type* attr_types, size_t attr_type_count)
{
    element* result;
    attribute* attr;
    yella_file_type ftype;
    EVP_MD_CTX ctx;
    unsigned char md[EVP_MAX_MD_SIZE];
    unsigned md_len;
    yella_rc yrc;

    if (yella_file_exists(name))
    {
        if (yella_get_file_type(name, &ftype) != YELLA_NO_ERROR)
            return NULL;
        result = create_element(name);
        if (contains_attr_type(ATTR_TYPE_FILE_TYPE, attr_types, attr_type_count))
        {
            attr = malloc(sizeof(attribute));
            attr->type = ATTR_TYPE_FILE_TYPE;
            attr->value.int_value = ftype;
            add_element_attribute(result, attr);
        }
        if ((ftype == YELLA_FILE_TYPE_REGULAR || ftype == YELLA_FILE_TYPE_SYMBOLIC_LINK) &&
            contains_attr_type(ATTR_TYPE_SHA256, attr_types, attr_type_count))
        {
            EVP_MD_CTX_init(&ctx);
            EVP_DigestInit_ex(&ctx, EVP_sha256(), NULL);
            yrc = yella_apply_function_to_file_contents(name, digest_callback, &ctx);
            if (yrc == YELLA_NO_ERROR)
            {
                EVP_DigestFinal_ex(&ctx, md, &md_len);
                attr = malloc(sizeof(attribute));
                attr->type = ATTR_TYPE_SHA256;
                attr->value.byte_array.mem = malloc(md_len);
                attr->value.byte_array.sz = md_len;
                memcpy(attr->value.byte_array.mem, md, md_len);
                add_element_attribute(result, attr);
            }
            EVP_MD_CTX_cleanup(&ctx);
        }
    }
    else
    {
        result = NULL;
    }
    return result;
}