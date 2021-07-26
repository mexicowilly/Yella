#include "common/yaml_util.h"
#include "common/text_util.h"

typedef struct emitter_str
{
    char* buf;
    size_t size;
} emitter_str;

static int write_handler(void* udata, unsigned char* buf, size_t sz)
{
    emitter_str* result = (emitter_str*)udata;

    result->size += sz;
    result->buf = realloc(result->buf, result->size);
    memcpy(result->buf, buf, sz);
    return 1;
}

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
    free(utf8);
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
    yaml_emitter_t emitter;
    emitter_str result;

    yaml_emitter_initialize(&emitter);
    result.buf = NULL;
    result.size = 0;
    yaml_emitter_set_output(&emitter, write_handler, &result);
    yaml_emitter_set_encoding(&emitter, YAML_UTF8_ENCODING);
    yaml_emitter_set_indent(&emitter, 2);
    yaml_emitter_set_unicode(&emitter, 1);
    yaml_emitter_set_width(&emitter, -1);
    yaml_emitter_open(&emitter);
    yaml_emitter_dump(&emitter, doc);
    yaml_emitter_close(&emitter);
    yaml_emitter_delete(&emitter);
    for (result.size -= 1; result.size >= 0; result.size--)
    {
        if (result.buf[result.size] != '\n' && result.buf[result.size] != '\r')
            break;
    }
    result.buf[result.size + 1] = 0;
    return result.buf;
}
