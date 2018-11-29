#ifndef YELLA_ELEMENT_H__
#define YELLA_ELEMENT_H__

#include "plugin/file/attribute.h"
#include <unicode/utypes.h>

typedef struct element element;

void add_element_attribute(element* elem, attribute* attr);
int compare_element_attributes(const element* const lhs, const element* const rhs);
element* create_element(const UChar* const name);
void destroy_element(element* elem);
const UChar* element_name(const element* const elem);
uint8_t* pack_element_attributes(const element* const elem, size_t* sz);

#endif
