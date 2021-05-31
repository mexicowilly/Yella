#include "plugin/file/collect_attributes.h"
#include "common/file.h"
#include "common/text_util.h"
#include <stdio.h>
#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <stdlib.h>
#include <cmocka.h>
#include <openssl/evp.h>
#include <plugin/file/attribute.h>

static const UChar* FILE_NAME = u"collect-attr-test-file.txt";

static void file_type(void** arg)
{
    attribute_type tp;
    element* elem1;
    element* elem2;
    attribute* attr;
    chucho_logger_t* lgr;

    tp = ATTR_TYPE_FILE_TYPE;
    lgr = chucho_get_logger("collect_attributes_test");
    elem1 = collect_attributes(FILE_NAME, &tp, 1, lgr);
    chucho_release_logger(lgr);
    assert_non_null(elem1);
    elem2 = create_element(FILE_NAME);
    attr = malloc(sizeof(attribute));
    attr->type = ATTR_TYPE_FILE_TYPE;
    attr->value.integer = YELLA_FILE_TYPE_REGULAR;
    add_element_attribute(elem2, attr);
    assert_int_equal(compare_element_attributes(elem1, elem2), 0);
    destroy_element(elem2);
    destroy_element(elem1);
}

static void non_existent(void** arg)
{
    element* elem;
    attribute_type tp;
    chucho_logger_t* lgr;

    tp = ATTR_TYPE_FILE_TYPE;
    lgr = chucho_get_logger("collect_attributes_test");
    elem = collect_attributes(u"doggies-and-monkies.xxx", &tp, 1, lgr);
    chucho_release_logger(lgr);
    assert_null(elem);
}

static void sha256(void** arg)
{
    attribute_type tp;
    element* elem1;
    element* elem2;
    attribute* attr;
    unsigned char md[EVP_MAX_MD_SIZE];
    unsigned md_len;
    uint8_t* bytes;
    size_t fsize;
    char* utf8;
    FILE* f;
    chucho_logger_t* lgr;

    tp = ATTR_TYPE_SHA256;
    lgr = chucho_get_logger("collect_attributes_test");
    elem1 = collect_attributes(FILE_NAME, &tp, 1, lgr);
    chucho_release_logger(lgr);
    assert_non_null(elem1);
    elem2 = create_element(FILE_NAME);
    attr = malloc(sizeof(attribute));
    attr->type = ATTR_TYPE_SHA256;
    yella_file_size(FILE_NAME, &fsize);
    bytes = malloc(fsize);
    utf8 = yella_to_utf8(FILE_NAME);
    f = fopen(utf8, "rb");
    free(utf8);
    fread(bytes, 1, fsize, f);
    fclose(f);
    md_len = EVP_MAX_MD_SIZE;
    EVP_Digest(bytes, fsize, md, &md_len, EVP_sha256(), NULL);
    free(bytes);
    attr->value.byte_array.mem = malloc(md_len);
    attr->value.byte_array.sz = md_len;
    memcpy(attr->value.byte_array.mem, md, md_len);
    add_element_attribute(elem2, attr);
    assert_int_equal(compare_element_attributes(elem1, elem2), 0);
    destroy_element(elem2);
    destroy_element(elem1);
}

static int set_up(void** arg)
{
    FILE* f;
    char* utf8;
    int i;

    utf8 = yella_to_utf8(FILE_NAME);
    f = fopen(utf8, "wb");
    free(utf8);
    if (f == NULL)
        return -1;
    for (i = 0; i < 1000000; i++)
        fwrite(&i, 1, sizeof(i), f);
    fclose(f);
    return 0;
}

static int tear_down(void** arg)
{
    yella_remove_file(FILE_NAME);
    return 0;
}

int main()
{
    const struct CMUnitTest tests[] =
    {
        cmocka_unit_test(file_type),
        cmocka_unit_test(non_existent),
        cmocka_unit_test(sha256)
    };

    return cmocka_run_group_tests(tests, set_up, tear_down);
}
