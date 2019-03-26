#include "plugin/file/attribute.h"
#include "common/file.h"
#include "attribute.h"
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
        default:
            assert(false);
    }
    return result;
}

int compare_attributes(const attribute* const lhs, const attribute* const rhs)
{
    int rc;

    rc = lhs->type - rhs->type;
    if (rc == 0)
    {
        switch (lhs->type)
        {
        case ATTR_TYPE_FILE_TYPE:
            rc = lhs->value.integer - rhs->value.integer;
            break;
        case ATTR_TYPE_SHA256:
            rc = (int)lhs->value.byte_array.sz - (int)rhs->value.byte_array.sz;
            if (rc == 0)
                rc = memcmp(lhs->value.byte_array.mem, rhs->value.byte_array.mem, lhs->value.byte_array.sz);
            break;
        }
    }
    return rc;
}

attribute* create_attribute_from_table(const yella_fb_file_attr_table_t tbl)
{
    attribute* result;
    flatbuffers_uint8_vec_t bytes;

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
        default:
            assert(false);
    }
    return result;
}

yella_fb_file_attr_ref_t pack_attribute(const attribute* const attr, flatcc_builder_t* bld)
{
    yella_fb_file_attr_type_enum_t fb_type;

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
    default:
        assert(false);
    }
    yella_fb_file_attr_type_add(bld, fb_type);
    return yella_fb_file_attr_end(bld);
}
