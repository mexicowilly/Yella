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
