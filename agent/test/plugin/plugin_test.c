#include "plugin/plugin.h"
#include <unicode/ustring.h>
#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <stdio.h>
#include <cmocka.h>

yella_rc in_cap_handler(const yella_parcel* const pcl, void* udata)
{
    return YELLA_NO_ERROR;
}

static void copy(void** arg)
{
    yella_plugin* plg;
    yella_plugin_in_cap* in;
    yella_plugin_out_cap* out;
    yella_plugin* plg2;

    plg = yella_create_plugin(u"my dog", u"has fleas", NULL);
    in = yella_create_plugin_in_cap(u"doggies", 72, in_cap_handler, NULL);
    yella_push_back_ptr_vector(in->configs, udsnew(u"tom"));
    yella_push_back_ptr_vector(in->configs, udsnew(u"ze"));
    yella_push_back_ptr_vector(in->configs, udsnew(u"is"));
    yella_push_back_ptr_vector(in->configs, udsnew(u"cool"));
    yella_push_back_ptr_vector(plg->in_caps, in);
    in = yella_create_plugin_in_cap(u"iguanas", 73, in_cap_handler, NULL);
    yella_push_back_ptr_vector(in->configs, udsnew(u"play"));
    yella_push_back_ptr_vector(in->configs, udsnew(u"it"));
    yella_push_back_ptr_vector(plg->in_caps, in);
    in = yella_create_plugin_in_cap(u"monkies", 74, in_cap_handler, NULL);
    yella_push_back_ptr_vector(in->configs, udsnew(u"what"));
    yella_push_back_ptr_vector(plg->in_caps, in);
    yella_push_back_ptr_vector(plg->out_caps, yella_create_plugin_out_cap(u"cry me", 75));
    yella_push_back_ptr_vector(plg->out_caps, yella_create_plugin_out_cap(u"a donkey", 76));
    plg2 = yella_copy_plugin(plg);
    yella_destroy_plugin(plg);
    assert_true(u_strcmp(plg2->name, u"my dog") == 0);
    assert_true(u_strcmp(plg2->version, u"has fleas") == 0);
    assert_int_equal(yella_ptr_vector_size(plg2->in_caps), 3);
    in = yella_ptr_vector_at(plg2->in_caps, 0);
    assert_true(u_strcmp(in->name, u"doggies") == 0);
    assert_int_equal(in->version, 72);
    assert_int_equal(yella_ptr_vector_size(in->configs), 4);
    assert_true(u_strcmp(yella_ptr_vector_at(in->configs, 0), u"tom") == 0);
    assert_true(u_strcmp(yella_ptr_vector_at(in->configs, 1), u"ze") == 0);
    assert_true(u_strcmp(yella_ptr_vector_at(in->configs, 2), u"is") == 0);
    assert_true(u_strcmp(yella_ptr_vector_at(in->configs, 3), u"cool") == 0);
    in = yella_ptr_vector_at(plg2->in_caps, 1);
    assert_true(u_strcmp(in->name, u"iguanas") == 0);
    assert_int_equal(in->version, 73);
    assert_int_equal(yella_ptr_vector_size(in->configs), 2);
    assert_true(u_strcmp(yella_ptr_vector_at(in->configs, 0), u"play") == 0);
    assert_true(u_strcmp(yella_ptr_vector_at(in->configs, 1), u"it") == 0);
    in = yella_ptr_vector_at(plg2->in_caps, 2);
    assert_true(u_strcmp(in->name, u"monkies") == 0);
    assert_int_equal(in->version, 74);
    assert_int_equal(yella_ptr_vector_size(in->configs), 1);
    assert_true(u_strcmp(yella_ptr_vector_at(in->configs, 0), u"what") == 0);
    assert_int_equal(yella_ptr_vector_size(plg2->out_caps), 2);
    out = yella_ptr_vector_at(plg2->out_caps, 0);
    assert_true(u_strcmp(out->name, u"cry me") == 0);
    assert_int_equal(out->version, 75);
    out = yella_ptr_vector_at(plg2->out_caps, 1);
    assert_true(u_strcmp(out->name, u"a donkey") == 0);
    assert_int_equal(out->version, 76);
    yella_destroy_plugin(plg2);
}

static void create(void** arg)
{
    yella_plugin* plg;
    yella_plugin_in_cap* in;
    yella_plugin_out_cap* out;

    plg = yella_create_plugin(u"my dog", u"has fleas", NULL);
    in = yella_create_plugin_in_cap(u"doggies", 72, in_cap_handler, NULL);
    yella_push_back_ptr_vector(in->configs, udsnew(u"tom"));
    yella_push_back_ptr_vector(in->configs, udsnew(u"ze"));
    yella_push_back_ptr_vector(in->configs, udsnew(u"is"));
    yella_push_back_ptr_vector(in->configs, udsnew(u"cool"));
    yella_push_back_ptr_vector(plg->in_caps, in);
    in = yella_create_plugin_in_cap(u"iguanas", 73, in_cap_handler, NULL);
    yella_push_back_ptr_vector(in->configs, udsnew(u"play"));
    yella_push_back_ptr_vector(in->configs, udsnew(u"it"));
    yella_push_back_ptr_vector(plg->in_caps, in);
    in = yella_create_plugin_in_cap(u"monkies", 74, in_cap_handler, NULL);
    yella_push_back_ptr_vector(in->configs, udsnew(u"what"));
    yella_push_back_ptr_vector(plg->in_caps, in);
    yella_push_back_ptr_vector(plg->out_caps, yella_create_plugin_out_cap(u"cry me", 75));
    yella_push_back_ptr_vector(plg->out_caps, yella_create_plugin_out_cap(u"a donkey", 76));
    assert_true(u_strcmp(plg->name, u"my dog") == 0);
    assert_true(u_strcmp(plg->version, u"has fleas") == 0);
    assert_int_equal(yella_ptr_vector_size(plg->in_caps), 3);
    in = yella_ptr_vector_at(plg->in_caps, 0);
    assert_true(u_strcmp(in->name, u"doggies") == 0);
    assert_int_equal(in->version, 72);
    assert_int_equal(yella_ptr_vector_size(in->configs), 4);
    assert_true(u_strcmp(yella_ptr_vector_at(in->configs, 0), u"tom") == 0);
    assert_true(u_strcmp(yella_ptr_vector_at(in->configs, 1), u"ze") == 0);
    assert_true(u_strcmp(yella_ptr_vector_at(in->configs, 2), u"is") == 0);
    assert_true(u_strcmp(yella_ptr_vector_at(in->configs, 3), u"cool") == 0);
    in = yella_ptr_vector_at(plg->in_caps, 1);
    assert_true(u_strcmp(in->name, u"iguanas") == 0);
    assert_int_equal(in->version, 73);
    assert_int_equal(yella_ptr_vector_size(in->configs), 2);
    assert_true(u_strcmp(yella_ptr_vector_at(in->configs, 0), u"play") == 0);
    assert_true(u_strcmp(yella_ptr_vector_at(in->configs, 1), u"it") == 0);
    in = yella_ptr_vector_at(plg->in_caps, 2);
    assert_true(u_strcmp(in->name, u"monkies") == 0);
    assert_int_equal(in->version, 74);
    assert_int_equal(yella_ptr_vector_size(in->configs), 1);
    assert_true(u_strcmp(yella_ptr_vector_at(in->configs, 0), u"what") == 0);
    assert_int_equal(yella_ptr_vector_size(plg->out_caps), 2);
    out = yella_ptr_vector_at(plg->out_caps, 0);
    assert_true(u_strcmp(out->name, u"cry me") == 0);
    assert_int_equal(out->version, 75);
    out = yella_ptr_vector_at(plg->out_caps, 1);
    assert_true(u_strcmp(out->name, u"a donkey") == 0);
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
