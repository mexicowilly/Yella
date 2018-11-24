#include "plugin/file/element.h"
#include "common/sglib.h"
#include "common/uds.h"

typedef struct attr_list
{
    attribute* attr;
    struct attr_list* next;
} attr_list;

struct element
{
    uds name;
    attr_list* attrs;
};

#define ATTR_COMPARATOR(lhs, rhs) (lhs->attr->type - rhs->attr->type)

SGLIB_DEFINE_SORTED_LIST_PROTOTYPES(attr_list, ATTR_COMPARATOR, next);
SGLIB_DEFINE_SORTED_LIST_FUNCTIONS(attr_list, ATTR_COMPARATOR, next);

void add_element_attribute(element* elem, attribute* attr)
{
    attr_list* al;

    al = malloc(sizeof(attr_list));
    al->attr = attr;
    sglib_attr_list_add(&elem->attrs, al);
}

int compare_element_attributes(const element* const lhs, const element* const rhs)
{
    int rc;
    struct sglib_attr_list_iterator litor;
    struct sglib_attr_list_iterator ritor;
    attr_list* lal;
    attr_list* ral;

    rc = sglib_attr_list_len(lhs->attrs) - sglib_attr_list_len(rhs->attrs);
    if (rc == 0)
    {
        for (lal = sglib_attr_list_it_init(&litor, lhs->attrs),
             ral = sglib_attr_list_it_init(&ritor, rhs->attrs);
             lal != NULL && rc == 0;
             lal = sglib_attr_list_it_next(&litor),
             ral = sglib_attr_list_it_next(&ritor))
        {
            rc = compare_attributes(lal->attr, ral->attr);
        }
    }
    return rc;
}

element* create_element(const UChar* const name)
{
    element* result;

    result = calloc(1, sizeof(element));
    result->name = udsnew(name);
    return result;
}

void destroy_element(element* elem)
{
    struct sglib_attr_list_iterator itor;
    attr_list* al;

    udsfree(elem->name);
    for (al = sglib_attr_list_it_init(&itor, elem->attrs);
         al != NULL;
         al = sglib_attr_list_it_next(&itor))
    {
        destroy_attribute(al->attr);
        free(al);
    }
    free(elem);
}
