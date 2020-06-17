#ifndef YELLA_YAML_UTIL_H
#define YELLA_YAML_UTIL_H

#include "export.h"
#include <yaml.h>
#include <unicode/ustring.h>

YELLA_EXPORT void yella_add_yaml_number_mapping(yaml_document_t* doc, int node, const char* const key, int64_t value);
YELLA_EXPORT void yella_add_yaml_string_mapping(yaml_document_t* doc, int node, const char* const key, const UChar* const value);
YELLA_EXPORT void yella_add_yaml_unumber_mapping(yaml_document_t* doc, int node, const UChar* const key, int64_t value);
YELLA_EXPORT void yella_add_yaml_ustring_mapping(yaml_document_t* doc, int node, const UChar* const key, const UChar* const value);
YELLA_EXPORT char* yella_emit_yaml(yaml_document_t* doc);

#endif
