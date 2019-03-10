#ifndef YELLA_ELEMENT_H__
#define YELLA_ELEMENT_H__

#include "plugin/file/attribute.h"
#include "file_builder.h"
#include <unicode/utypes.h>

typedef struct element element;

YELLA_PRIV_EXPORT void add_element_attribute(element* elem, attribute* attr);
YELLA_PRIV_EXPORT int compare_element_attributes(const element* const lhs, const element* const rhs);
YELLA_PRIV_EXPORT element* create_element(const UChar* const name);
YELLA_PRIV_EXPORT element* create_element_with_attrs(const UChar* const name, const uint8_t* const packed_attrs);
YELLA_PRIV_EXPORT void destroy_element(element* elem);
/* post: elem1 contains the symmetric difference of attributes between the two */
YELLA_PRIV_EXPORT void diff_elements(element* elem1, element* elem2);
YELLA_PRIV_EXPORT const UChar* element_name(const element* const elem);
YELLA_PRIV_EXPORT uint8_t* pack_element_attributes(const element* const elem, size_t* sz);
YELLA_PRIV_EXPORT yella_fb_file_attr_array_ref_t pack_element_attributes_to_table(const element* const elem, flatcc_builder_t* bld);

#endif
