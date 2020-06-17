#include "common/yaml_util.h"
#include "common/text_util.h"

void yella_add_yaml_number_mapping(yaml_document_t* doc, int node, const char* const key, int64_t value)
{
    int k;
    int v;
    UChar* num_str;
    char* utf8;

    k = yaml_document_add_scalar(doc, NULL, (yaml_char_t*)key, strlen(key), YAML_PLAIN_SCALAR_STYLE);
    num_str = yella_to_string(value);
    utf8 = yella_to_utf8(num_str);
    free(num_str);
    v = yaml_document_add_scalar(doc, NULL, (yaml_char_t*)utf8, strlen(utf8), YAML_PLAIN_SCALAR_STYLE);
    yaml_document_append_mapping_pair(doc, node, k, v);
}

void yella_add_yaml_string_mapping(yaml_document_t* doc, int node, const char* const key, const UChar* const value)
{
    int k;
    int v;
    char* utf8;

    k = yaml_document_add_scalar(doc, NULL, (yaml_char_t*)key, strlen(key), YAML_PLAIN_SCALAR_STYLE);
    utf8 = yella_to_utf8(value);
    v = yaml_document_add_scalar(doc, NULL, (yaml_char_t*)utf8, strlen(utf8), YAML_PLAIN_SCALAR_STYLE);
    free(utf8);
    yaml_document_append_mapping_pair(doc, node, k, v);
}

void yella_add_yaml_unumber_mapping(yaml_document_t* doc, int node, const UChar* const key, int64_t value)
{
    char* utf8;

    utf8 = yella_to_utf8(key);
    yella_add_yaml_number_mapping(doc, node, utf8, value);
    free(utf8);
}

void yella_add_yaml_ustring_mapping(yaml_document_t* doc, int node, const UChar* const key, const UChar* const value)
{
    char* utf8;

    utf8 = yella_to_utf8(key);
    yella_add_yaml_string_mapping(doc, node, utf8, value);
    free(utf8);
}

char* yella_emit_yaml(yaml_document_t* doc)
{
    char* result;
    yaml_emitter_t emitter;
    size_t output_written;

    yaml_emitter_initialize(&emitter);
    output_written = 8 * 1024;
    result = malloc(output_written);
    yaml_emitter_set_output_string(&emitter, (yaml_char_t*)result, output_written - 1, &output_written);
    yaml_emitter_set_encoding(&emitter, YAML_UTF8_ENCODING);
    yaml_emitter_set_indent(&emitter, 2);
    yaml_emitter_set_unicode(&emitter, 1);
    yaml_emitter_open(&emitter);
    yaml_emitter_dump(&emitter, doc);
    result[output_written] = 0;
    yaml_emitter_close(&emitter);
    yaml_emitter_delete(&emitter);
    return result;
}
