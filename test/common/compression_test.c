#include "common/compression.h"
#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <stdlib.h>
#include <string.h>
#include <cmocka.h>

static void lz4(void** targ)
{
    size_t sz;
    uint8_t* orig = (uint8_t*)"my dog has fleas";
    uint8_t* cmp;
    uint8_t* decmp;

    sz = strlen((char*)orig) + 1;
    cmp = yella_lz4_compress(orig, &sz);
    assert_non_null(cmp);
    assert_true(sz > 0);
    decmp = yella_lz4_decompress(cmp, &sz);
    free(cmp);
    assert_non_null(decmp);
    assert_int_equal(strlen((char*)orig) + 1, sz);
    assert_memory_equal(decmp, orig, sz);
    free(decmp);
}

int main()
{
    const struct CMUnitTest tests[] =
    {
        cmocka_unit_test(lz4)
    };
    return cmocka_run_group_tests(tests, NULL, NULL);
}
