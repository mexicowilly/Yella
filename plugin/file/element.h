#ifndef YELLA_ELEMENT_H__
#define YELLA_ELEMENT_H__

#include "plugin/file/attribute.h"
#include "file_builder.h"
#include <unicode/utypes.h>

typedef struct element element;

void add_element_attribute(element* elem, attribute* attr);
int compare_element_attributes(const element* const lhs, const element* const rhs);
element* create_element(const UChar* const name);
element* create_element_with_attrs(const UChar* const name, const uint8_t* const packed_attrs);
void destroy_element(element* elem);
/* post: elem1 contains the symmetric difference of attributes between the two */
void diff_elements(element* elem1, element* elem2);
const UChar* element_name(const element* const elem);
uint8_t* pack_element_attributes(const element* const elem, size_t* sz);
yella_fb_file_attr_array_ref_t pack_element_attributes_to_table(const element* const elem, flatcc_builder_t* bld);

#endif
