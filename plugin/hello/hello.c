#include "plugin/plugin.h"
#include "common/thread.h"
#include "common/settings.h"
#include "common/text_util.h"
#include "hello_reader.h"
#include "hello_builder.h"
#include <string.h>
#include <stdbool.h>
#include <stdio.h>

static yella_rc set_interval_handler(const yella_message_header * mhdr, const uint8_t * msg, size_t sz);

static yella_plugin_in_cap hello_in_cap =
{

    "yella.fb.hello.set_interval",
    1,
    set_interval_handler,
    NULL,
    0
};
static yella_plugin_out_cap hello_out_cap =
{

    "yella.fb.hello.hello",
    1
};
static yella_plugin hello_desc =
{
    "hello",
    "1",
    &hello_in_cap,
    1,
    &hello_out_cap,
    1
};
static bool should_stop;
static yella_thread* thr;
static yella_mutex* guard;
static yella_condition_variable* work_cond;
static unsigned interval_secs;
static yella_agent_api agent_api;
static uint32_t minor_seq;
static char* recipient;
static void* agent;

static yella_rc set_interval_handler(const yella_message_header* const mhdr, const uint8_t* const msg, size_t sz)
{
    yella_fb_hello_set_interval_table_t tbl;

    tbl = yella_fb_hello_set_interval_as_root(msg);
    if (yella_fb_hello_set_interval_seconds_is_present(tbl))
    {
        yella_lock_mutex(guard);
        interval_secs = yella_fb_hello_set_interval_seconds(tbl);
        yella_unlock_mutex(guard);
        recipient = yella_text_dup(mhdr->sender);
    }
    return YELLA_NO_ERROR;
}

static void thr_main(void* uarg)
{
    flatcc_builder_t bld;
    int i = 0;
    char buf[256];
    uint8_t* raw;
    size_t raw_size;
    yella_message_header* mhdr;

    yella_lock_mutex(guard);
    while (true)
    {
        while (!should_stop && interval_secs == 0)
            yella_wait_milliseconds_for_condition_variable(work_cond, guard, 500);
        if (should_stop)
            break;
        snprintf(buf, sizeof(buf), "Hello, World! %i", i++);
        flatcc_builder_init(&bld);
        yella_fb_hello_hello_start_as_root(&bld);
        yella_fb_hello_hello_hello_create_str(&bld, buf);
        yella_fb_hello_hello_end_as_root(&bld);
        raw = flatcc_builder_finalize_buffer(&bld, &raw_size);
        flatcc_builder_clear(&bld);
        mhdr = yella_create_mhdr();
        mhdr->type = yella_text_dup(hello_out_cap.name);
        mhdr->recipient = yella_text_dup(recipient);
        mhdr->seq.minor = ++minor_seq;
        agent_api.send_message(agent, mhdr, raw, raw_size);
        yella_destroy_mhdr(mhdr);
        free(raw);
    }
    yella_unlock_mutex(guard);
    yella_destroy_mhdr(mhdr);
}

static void retrieve_hello_settings(void)
{
    /*
     * This is an example of what all plugins should do.
     * This array is of settings that should be checked.
     */
    /*
    yella_setting_desc descs[] =
    {
        { "hello.interval-secs", YELLA_SETTING_VALUE_UINT }
    };
    */


    /*
     * Here we set the default values.
     */
    /*
    yella_settings_set_uint("hello.interval-secs", 5);
    */
    /*
     * And then you retrieve the settings from the settings doc, which
     * will be destroyed after all plugins are loaded.
     */
    /*
    yella_retrieve_settings(descs, 1);
    */
}

const yella_plugin* plugin_start(const yella_agent_api* api, void* agnt)
{
    memcpy(&agent_api, api, sizeof(agent_api));
    should_stop = false;
    minor_seq = 0;
    agent = agnt;
    recipient = NULL;
    retrieve_hello_settings();
    guard = yella_create_mutex();
    work_cond = yella_create_condition_variable();
    interval_secs = 0;
    thr = yella_create_thread(thr_main, NULL);
    return &hello_desc;
}

const yella_plugin* plugin_status(void)
{

}

yella_rc plugin_stop(void)
{
    yella_lock_mutex(guard);
    should_stop = true;
    yella_signal_condition_variable(work_cond);
    yella_unlock_mutex(guard);
    yella_join_thread(thr);
    yella_destroy_thread(thr);
    yella_destroy_condition_variable(work_cond);
    yella_destroy_mutex(guard);
    free(recipient);
    return YELLA_NO_ERROR;
}
