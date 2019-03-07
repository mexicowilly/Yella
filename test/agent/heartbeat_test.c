#include "agent/heartbeat.h"
#include "plugin/plugin.h"
#include "common/text_util.h"
#include "heartbeat_reader.h"
#include <stdlib.h>
#include <setjmp.h>
#include <stdarg.h>
#include <cmocka.h>

yella_rc in_cap_handler(const uint8_t* const msg, size_t sz, void* udata)
{
    return YELLA_NO_ERROR;
}

static void plugin_dtor(void* p, void* udata)
{
    yella_destroy_plugin((yella_plugin*)p);
}

static void simple(void** targ)
{
    yella_ptr_vector* plugins;
    yella_plugin* plg;
    uint8_t* raw;
    size_t sz;
    yella_plugin_in_cap* in;
    yella_fb_heartbeat_table_t hb;
    yella_fb_capability_vec_t caps;
    yella_fb_capability_table_t cap;
    flatbuffers_string_vec_t configs;
    yella_fb_operating_system_table_t os;

    plugins = yella_create_ptr_vector();
    yella_set_ptr_vector_destructor(plugins, plugin_dtor, NULL);
    plg = yella_create_plugin(u"test", u"7.6.5", NULL);
    in = yella_create_plugin_in_cap(u"bite_me", 3, in_cap_handler, NULL);
    yella_push_back_ptr_vector(in->configs, udsnew(u"config 9"));
    yella_push_back_ptr_vector(in->configs, udsnew(u"config 10"));
    yella_push_back_ptr_vector(in->configs, udsnew(u"config 11"));
    yella_push_back_ptr_vector(plg->in_caps, in);
    in = yella_create_plugin_in_cap(u"sweet", 4, in_cap_handler, NULL);
    yella_push_back_ptr_vector(in->configs, udsnew(u"config 12"));
    yella_push_back_ptr_vector(in->configs, udsnew(u"config 13"));
    yella_push_back_ptr_vector(plg->in_caps, in);
    in = yella_create_plugin_in_cap(u"grumpy", 5, in_cap_handler, NULL);
    yella_push_back_ptr_vector(in->configs, udsnew(u"config 14"));
    yella_push_back_ptr_vector(plg->in_caps, in);
    yella_push_back_ptr_vector(plg->out_caps, yella_create_plugin_out_cap(u"what?", 6));
    yella_push_back_ptr_vector(plg->out_caps, yella_create_plugin_out_cap(u"summmmm", 7));
    yella_push_back_ptr_vector(plugins, plg);
    plg = yella_create_plugin(u"chucho", u"4.3.2", NULL);
    in = yella_create_plugin_in_cap(u"you_wish", 1, in_cap_handler, NULL);
    yella_push_back_ptr_vector(plg->in_caps, in);
    in = yella_create_plugin_in_cap(u"sour", 2, in_cap_handler, NULL);
    yella_push_back_ptr_vector(in->configs, udsnew(u"config 17"));
    yella_push_back_ptr_vector(in->configs, udsnew(u"config 18"));
    yella_push_back_ptr_vector(in->configs, udsnew(u"config 19"));
    yella_push_back_ptr_vector(plg->in_caps, in);
    yella_push_back_ptr_vector(plg->out_caps, yella_create_plugin_out_cap(u"scrumpy", 3));
    yella_push_back_ptr_vector(plg->out_caps, yella_create_plugin_out_cap(u"humpy", 8));
    yella_push_back_ptr_vector(plugins, plg);
    raw = create_heartbeat(u"eye dee", plugins, &sz);
    hb = yella_fb_heartbeat_as_root(raw);
    assert_true(yella_fb_heartbeat_id_is_present(hb));
    assert_string_equal(yella_fb_heartbeat_id(hb), "eye dee");
    assert_true(yella_fb_heartbeat_host_is_present(hb));
    os = yella_fb_heartbeat_host(hb);
    assert_true(yella_fb_operating_system_machine_is_present(os));
    assert_true(yella_fb_operating_system_system_is_present(os));
    assert_true(yella_fb_operating_system_version_is_present(os));
    assert_true(yella_fb_operating_system_release_is_present(os));
    assert_true(yella_fb_heartbeat_seconds_since_epoch_is_present(hb));
    caps = yella_fb_heartbeat_in_capabilities(hb);
    assert_int_equal(yella_fb_capability_vec_len(caps), 5);
    cap = yella_fb_capability_vec_at(caps, 0);
    assert_true(yella_fb_capability_name_is_present(cap));
    assert_string_equal(yella_fb_capability_name(cap), "bite_me");
    assert_true(yella_fb_capability_version_is_present(cap));
    assert_int_equal(yella_fb_capability_version(cap), 3);
    assert_true(yella_fb_capability_configurations_is_present(cap));
    configs = yella_fb_capability_configurations(cap);
    assert_int_equal(flatbuffers_string_vec_len(configs), 3);
    assert_string_equal(flatbuffers_string_vec_at(configs, 0), "config 9");
    assert_string_equal(flatbuffers_string_vec_at(configs, 1), "config 10");
    assert_string_equal(flatbuffers_string_vec_at(configs, 2), "config 11");
    cap = yella_fb_capability_vec_at(caps, 1);
    assert_true(yella_fb_capability_name_is_present(cap));
    assert_string_equal(yella_fb_capability_name(cap), "sweet");
    assert_true(yella_fb_capability_version_is_present(cap));
    assert_int_equal(yella_fb_capability_version(cap), 4);
    assert_true(yella_fb_capability_configurations_is_present(cap));
    configs = yella_fb_capability_configurations(cap);
    assert_int_equal(flatbuffers_string_vec_len(configs), 2);
    assert_string_equal(flatbuffers_string_vec_at(configs, 0), "config 12");
    assert_string_equal(flatbuffers_string_vec_at(configs, 1), "config 13");
    cap = yella_fb_capability_vec_at(caps, 2);
    assert_true(yella_fb_capability_name_is_present(cap));
    assert_string_equal(yella_fb_capability_name(cap), "grumpy");
    assert_true(yella_fb_capability_version_is_present(cap));
    assert_int_equal(yella_fb_capability_version(cap), 5);
    assert_true(yella_fb_capability_configurations_is_present(cap));
    configs = yella_fb_capability_configurations(cap);
    assert_int_equal(flatbuffers_string_vec_len(configs), 1);
    assert_string_equal(flatbuffers_string_vec_at(configs, 0), "config 14");
    cap = yella_fb_capability_vec_at(caps, 3);
    assert_true(yella_fb_capability_name_is_present(cap));
    assert_string_equal(yella_fb_capability_name(cap), "you_wish");
    assert_true(yella_fb_capability_version_is_present(cap));
    assert_int_equal(yella_fb_capability_version(cap), 1);
    assert_false(yella_fb_capability_configurations_is_present(cap));
    cap = yella_fb_capability_vec_at(caps, 4);
    assert_true(yella_fb_capability_name_is_present(cap));
    assert_string_equal(yella_fb_capability_name(cap), "sour");
    assert_true(yella_fb_capability_version_is_present(cap));
    assert_int_equal(yella_fb_capability_version(cap), 2);
    assert_true(yella_fb_capability_configurations_is_present(cap));
    configs = yella_fb_capability_configurations(cap);
    assert_int_equal(flatbuffers_string_vec_len(configs), 3);
    assert_string_equal(flatbuffers_string_vec_at(configs, 0), "config 17");
    assert_string_equal(flatbuffers_string_vec_at(configs, 1), "config 18");
    assert_string_equal(flatbuffers_string_vec_at(configs, 2), "config 19");
    caps = yella_fb_heartbeat_out_capabilities(hb);
    assert_int_equal(yella_fb_capability_vec_len(caps), 4);
    cap = yella_fb_capability_vec_at(caps, 0);
    assert_true(yella_fb_capability_name_is_present(cap));
    assert_string_equal(yella_fb_capability_name(cap), "what?");
    assert_true(yella_fb_capability_version_is_present(cap));
    assert_int_equal(yella_fb_capability_version(cap), 6);
    cap = yella_fb_capability_vec_at(caps, 1);
    assert_true(yella_fb_capability_name_is_present(cap));
    assert_string_equal(yella_fb_capability_name(cap), "summmmm");
    assert_true(yella_fb_capability_version_is_present(cap));
    assert_int_equal(yella_fb_capability_version(cap), 7);
    cap = yella_fb_capability_vec_at(caps, 2);
    assert_true(yella_fb_capability_name_is_present(cap));
    assert_string_equal(yella_fb_capability_name(cap), "scrumpy");
    assert_true(yella_fb_capability_version_is_present(cap));
    assert_int_equal(yella_fb_capability_version(cap), 3);
    cap = yella_fb_capability_vec_at(caps, 3);
    assert_true(yella_fb_capability_name_is_present(cap));
    assert_string_equal(yella_fb_capability_name(cap), "humpy");
    assert_true(yella_fb_capability_version_is_present(cap));
    assert_int_equal(yella_fb_capability_version(cap), 8);
    free(raw);
    yella_destroy_ptr_vector(plugins);
}

int main()
{
    const struct CMUnitTest tests[] =
    {
        cmocka_unit_test(simple)
    };

#if defined(YELLA_POSIX)
    setenv("CMOCKA_TEST_ABORT", "1", 1);
#endif
    return cmocka_run_group_tests(tests, NULL, NULL);
}

