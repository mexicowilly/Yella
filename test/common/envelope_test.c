#include "common/envelope.h"
#include <unicode/ustring.h>
#include <time.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <stdio.h>
#include <cmocka.h>

static void simple(void** targ)
{
    yella_envelope* env;
    uint8_t* pack;
    size_t pack_sz;
    yella_envelope* env2;

    env = yella_create_envelope(u"iguana", u"doggy", (uint8_t*)"monkey", 7);
    assert_non_null(env);
    pack = yella_pack_envelope(env, &pack_sz);
    assert_non_null(pack);
    assert_true(pack_sz > 0);
    print_message("Packed envelope size: %zu\n", pack_sz);
    env2 = yella_unpack_envelope(pack);
    free(pack);
    assert_non_null(env2);
    assert_true(u_strcmp(env2->sender, env->sender) == 0);
    assert_true(u_strcmp(env2->type, env->type) == 0);
    assert_int_equal(env2->len, env->len);
    assert_string_equal(env2->message, "monkey");
    yella_destroy_envelope(env2);
    yella_destroy_envelope(env);
}

int main()
{
    const struct CMUnitTest tests[] =
    {
        cmocka_unit_test(simple)
    };
    return cmocka_run_group_tests(tests, NULL, NULL);
}
