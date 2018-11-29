#include "plugin/file/attribute.h"
#include "attribute.h"
#include <string.h>

int compare_attributes(const attribute* const lhs, const attribute* const rhs)
{
    int rc;

    rc = lhs->type - rhs->type;
    if (rc == 0)
    {
        switch (lhs->type)
        {
        case ATTR_TYPE_SHA256:
            rc = (int)lhs->value.byte_array.sz - (int)rhs->value.byte_array.sz;
            if (rc == 0)
                rc = memcmp(lhs->value.byte_array.mem, rhs->value.byte_array.mem, lhs->value.byte_array.sz);
            break;
        }
    }
    return rc;
}

void destroy_attribute(attribute* attr)
{
    switch (attr->type)
    {
    case ATTR_TYPE_SHA256:
        free(attr->value.byte_array.mem);
        break;
    }
    free(attr);
}

yella_fb_file_attr_ref_t pack_attribute(const attribute* const attr, flatcc_builder_t* bld)
{
    yella_fb_file_attr_type_enum_t fb_type;

    yella_fb_file_attr_start(bld);
    switch (attr->type)
    {
    case ATTR_TYPE_SHA256:
        fb_type = yella_fb_file_attr_type_SHA256;
        break;
    }
    yella_fb_file_attr_type_add(bld, fb_type);
    switch (attr->type)
    {
    case ATTR_TYPE_SHA256:
        yella_fb_file_attr_bytes_start(bld);
        yella_fb_file_attr_bytes_add(bld,
                                     flatbuffers_uint8_vec_create(bld,
                                                                  attr->value.byte_array.mem,
                                                                  attr->value.byte_array.sz));
        yella_fb_file_attr_bytes_end(bld);
        break;
    }
    return yella_fb_file_attr_end(bld);
}
