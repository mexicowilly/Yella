#include "plugin/file/element.h"
#include "common/sglib.h"
#include "common/uds.h"
#include "common/ptr_vector.h"
#include "file_builder.h"
#include <unicode/ustring.h>

typedef struct attr_node
{
    attribute* attr;
    char color;
    struct attr_node* left;
    struct attr_node* right;
} attr_node;

struct element
{
    uds name;
    attr_node* attrs;
};

#define ATTR_COMPARATOR(lhs, rhs) (lhs->attr->type - rhs->attr->type)

SGLIB_DEFINE_RBTREE_PROTOTYPES(attr_node, left, right, color, ATTR_COMPARATOR);
SGLIB_DEFINE_RBTREE_FUNCTIONS(attr_node, left, right, color, ATTR_COMPARATOR);

void add_element_attribute(element* elem, attribute* attr)
{
    attr_node* al;

    al = malloc(sizeof(attr_node));
    al->attr = attr;
    sglib_attr_node_add(&elem->attrs, al);
}

int compare_element_attributes(const element* const lhs, const element* const rhs)
{
    int rc;
    struct sglib_attr_node_iterator litor;
    struct sglib_attr_node_iterator ritor;
    attr_node* lal;
    attr_node* ral;

    rc = sglib_attr_node_len(lhs->attrs) - sglib_attr_node_len(rhs->attrs);
    if (rc == 0)
    {
        for (lal = sglib_attr_node_it_init(&litor, lhs->attrs),
             ral = sglib_attr_node_it_init(&ritor, rhs->attrs);
             lal != NULL && rc == 0;
             lal = sglib_attr_node_it_next(&litor),
             ral = sglib_attr_node_it_next(&ritor))
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

element* create_element_with_attrs(const UChar* const name, const uint8_t* const packed_attrs)
{
    element*  result;
    yella_fb_file_attr_array_table_t tbl;
    yella_fb_file_attr_vec_t attrs;
    int i;

    result = create_element(name);
    tbl = yella_fb_file_attr_array_as_root(packed_attrs);
    attrs = yella_fb_file_attr_array_attrs_get(tbl);
    for (i = 0; i < yella_fb_file_attr_vec_len(attrs); i++)
        add_element_attribute(result, create_attribute_from_table(yella_fb_file_attr_vec_at(attrs, i)));
    return result;
}

void destroy_element(element* elem)
{
    struct sglib_attr_node_iterator itor;
    attr_node* al;

    udsfree(elem->name);
    for (al = sglib_attr_node_it_init(&itor, elem->attrs);
         al != NULL;
         al = sglib_attr_node_it_next(&itor))
    {
        destroy_attribute(al->attr);
        free(al);
    }
    free(elem);
}

void diff_elements(element* elem1, element* elem2)
{
    struct sglib_attr_node_iterator itor;
    attr_node* an;
    attr_node* found;
    yella_ptr_vector* to_delete1;
    yella_ptr_vector* to_delete2;
    int i;

    assert(u_strcmp(elem1->name, elem2->name) == 0);
    to_delete1 = yella_create_ptr_vector();
    /* The first loop covers the case where an attribute is either just
     * in elem1 or different from that in elem2.
     */
    for (an = sglib_attr_node_it_init(&itor, elem1->attrs);
         an != NULL;
         an = sglib_attr_node_it_next(&itor))
    {
        found = sglib_attr_node_find_member(elem2->attrs, an);
        if (found != NULL && compare_attributes(an->attr, found->attr) == 0)
            yella_push_back_ptr_vector(to_delete1, an);
    }
    to_delete2 = yella_create_ptr_vector();
    yella_set_ptr_vector_destructor(to_delete2, NULL, NULL);
    /* The second loop covers attribute that are only in elem2 and
     * not in elem1.
     */
    for (an = sglib_attr_node_it_init(&itor, elem2->attrs);
         an != NULL;
         an = sglib_attr_node_it_next(&itor))
    {
        found = sglib_attr_node_find_member(elem1->attrs, an);
        if (found == NULL)
            yella_push_back_ptr_vector(to_delete2, an);
    }
    for (i = 0; i < yella_ptr_vector_size(to_delete1); i++)
    {
        an = (attr_node*)yella_ptr_vector_at(to_delete1, i);
        sglib_attr_node_delete(&elem1->attrs, an);
        destroy_attribute(an->attr);
    }
    for (i = 0; i < yella_ptr_vector_size(to_delete2); i++)
        sglib_attr_node_add(&elem1->attrs, yella_ptr_vector_at(to_delete2, i));
    yella_destroy_ptr_vector(to_delete1);
    yella_destroy_ptr_vector(to_delete2);
}

const UChar* element_name(const element* const elem)
{
    return elem->name;
}

uint8_t* pack_element_attributes(const element* const elem, size_t* sz)
{
    flatcc_builder_t bld;
    uint8_t* result;
    attr_node* al;
    struct sglib_attr_node_iterator itor;

    flatcc_builder_init(&bld);
    yella_fb_file_attr_array_start_as_root(&bld);
    yella_fb_file_attr_array_attrs_start(&bld);
    for (al = sglib_attr_node_it_init(&itor, elem->attrs);
         al != NULL;
         al = sglib_attr_node_it_next(&itor))
    {
        yella_fb_file_attr_array_attrs_push(&bld, pack_attribute(al->attr, &bld));
    }
    yella_fb_file_attr_array_attrs_end(&bld);
    yella_fb_file_attr_array_end_as_root(&bld);
    result = flatcc_builder_finalize_buffer(&bld, sz);
    flatcc_builder_clear(&bld);
    return result;
}
