#include "plugin/plugin.h"
#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <stdio.h>
#include <cmocka.h>

yella_rc in_cap_handler(const yella_message_header* const mhdr, const uint8_t* const msg, size_t sz)
{
    return YELLA_NO_ERROR;
}

static void copy(void** arg)
{
    yella_plugin* plg;
    yella_plugin_in_cap* in;
    yella_plugin_out_cap* out;
    yella_plugin* plg2;

    plg = yella_create_plugin("my dog", "has fleas");
    in = yella_create_plugin_in_cap("doggies", 72, in_cap_handler);
    yella_push_back_ptr_vector(in->configs, sdsnew("tom"));
    yella_push_back_ptr_vector(in->configs, sdsnew("ze"));
    yella_push_back_ptr_vector(in->configs, sdsnew("is"));
    yella_push_back_ptr_vector(in->configs, sdsnew("cool"));
    yella_push_back_ptr_vector(plg->in_caps, in);
    in = yella_create_plugin_in_cap("iguanas", 73, in_cap_handler);
    yella_push_back_ptr_vector(in->configs, sdsnew("play"));
    yella_push_back_ptr_vector(in->configs, sdsnew("it"));
    yella_push_back_ptr_vector(plg->in_caps, in);
    in = yella_create_plugin_in_cap("monkies", 74, in_cap_handler);
    yella_push_back_ptr_vector(in->configs, sdsnew("what"));
    yella_push_back_ptr_vector(plg->in_caps, in);
    yella_push_back_ptr_vector(plg->out_caps, yella_create_plugin_out_cap("cry me", 75));
    yella_push_back_ptr_vector(plg->out_caps, yella_create_plugin_out_cap("a donkey", 76));
    plg2 = yella_copy_plugin(plg);
    yella_destroy_plugin(plg);
    assert_string_equal(plg2->name, "my dog");
    assert_string_equal(plg2->version, "has fleas");
    assert_int_equal(yella_ptr_vector_size(plg2->in_caps), 3);
    in = yella_ptr_vector_at(plg2->in_caps, 0);
    assert_string_equal(in->name, "doggies");
    assert_int_equal(in->version, 72);
    assert_int_equal(yella_ptr_vector_size(in->configs), 4);
    assert_string_equal(yella_ptr_vector_at(in->configs, 0), "tom");
    assert_string_equal(yella_ptr_vector_at(in->configs, 1), "ze");
    assert_string_equal(yella_ptr_vector_at(in->configs, 2), "is");
    assert_string_equal(yella_ptr_vector_at(in->configs, 3), "cool");
    in = yella_ptr_vector_at(plg2->in_caps, 1);
    assert_string_equal(in->name, "iguanas");
    assert_int_equal(in->version, 73);
    assert_int_equal(yella_ptr_vector_size(in->configs), 2);
    assert_string_equal(yella_ptr_vector_at(in->configs, 0), "play");
    assert_string_equal(yella_ptr_vector_at(in->configs, 1), "it");
    in = yella_ptr_vector_at(plg2->in_caps, 2);
    assert_string_equal(in->name, "monkies");
    assert_int_equal(in->version, 74);
    assert_int_equal(yella_ptr_vector_size(in->configs), 1);
    assert_string_equal(yella_ptr_vector_at(in->configs, 0), "what");
    assert_int_equal(yella_ptr_vector_size(plg2->out_caps), 2);
    out = yella_ptr_vector_at(plg2->out_caps, 0);
    assert_string_equal(out->name, "cry me");
    assert_int_equal(out->version, 75);
    out = yella_ptr_vector_at(plg2->out_caps, 1);
    assert_string_equal(out->name, "a donkey");
    assert_int_equal(out->version, 76);
    yella_destroy_plugin(plg2);
}

static void create(void** arg)
{
    yella_plugin* plg;
    yella_plugin_in_cap* in;
    yella_plugin_out_cap* out;

    plg = yella_create_plugin("my dog", "has fleas");
    in = yella_create_plugin_in_cap("doggies", 72, in_cap_handler);
    yella_push_back_ptr_vector(in->configs, sdsnew("tom"));
    yella_push_back_ptr_vector(in->configs, sdsnew("ze"));
    yella_push_back_ptr_vector(in->configs, sdsnew("is"));
    yella_push_back_ptr_vector(in->configs, sdsnew("cool"));
    yella_push_back_ptr_vector(plg->in_caps, in);
    in = yella_create_plugin_in_cap("iguanas", 73, in_cap_handler);
    yella_push_back_ptr_vector(in->configs, sdsnew("play"));
    yella_push_back_ptr_vector(in->configs, sdsnew("it"));
    yella_push_back_ptr_vector(plg->in_caps, in);
    in = yella_create_plugin_in_cap("monkies", 74, in_cap_handler);
    yella_push_back_ptr_vector(in->configs, sdsnew("what"));
    yella_push_back_ptr_vector(plg->in_caps, in);
    yella_push_back_ptr_vector(plg->out_caps, yella_create_plugin_out_cap("cry me", 75));
    yella_push_back_ptr_vector(plg->out_caps, yella_create_plugin_out_cap("a donkey", 76));
    assert_string_equal(plg->name, "my dog");
    assert_string_equal(plg->version, "has fleas");
    assert_int_equal(yella_ptr_vector_size(plg->in_caps), 3);
    in = yella_ptr_vector_at(plg->in_caps, 0);
    assert_string_equal(in->name, "doggies");
    assert_int_equal(in->version, 72);
    assert_int_equal(yella_ptr_vector_size(in->configs), 4);
    assert_string_equal(yella_ptr_vector_at(in->configs, 0), "tom");
    assert_string_equal(yella_ptr_vector_at(in->configs, 1), "ze");
    assert_string_equal(yella_ptr_vector_at(in->configs, 2), "is");
    assert_string_equal(yella_ptr_vector_at(in->configs, 3), "cool");
    in = yella_ptr_vector_at(plg->in_caps, 1);
    assert_string_equal(in->name, "iguanas");
    assert_int_equal(in->version, 73);
    assert_int_equal(yella_ptr_vector_size(in->configs), 2);
    assert_string_equal(yella_ptr_vector_at(in->configs, 0), "play");
    assert_string_equal(yella_ptr_vector_at(in->configs, 1), "it");
    in = yella_ptr_vector_at(plg->in_caps, 2);
    assert_string_equal(in->name, "monkies");
    assert_int_equal(in->version, 74);
    assert_int_equal(yella_ptr_vector_size(in->configs), 1);
    assert_string_equal(yella_ptr_vector_at(in->configs, 0), "what");
    assert_int_equal(yella_ptr_vector_size(plg->out_caps), 2);
    out = yella_ptr_vector_at(plg->out_caps, 0);
    assert_string_equal(out->name, "cry me");
    assert_int_equal(out->version, 75);
    out = yella_ptr_vector_at(plg->out_caps, 1);
    assert_string_equal(out->name, "a donkey");
    assert_int_equal(out->version, 76);
    yella_destroy_plugin(plg);
}

int main()
{
    const struct CMUnitTest tests[] =
    {
        cmocka_unit_test(copy),
        cmocka_unit_test(create)
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}
