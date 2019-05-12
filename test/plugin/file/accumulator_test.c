#include "plugin/file/accumulator.h"
#include "common/text_util.h"
#include "common/settings.h"
#include "plugin/plugin.h"
#include "file_reader.h"
#include <stdio.h>
#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <stdlib.h>
#include <cmocka.h>
#include <stdatomic.h>

static atomic_bool should_stop;

static void receive_message(void* agent, yella_message_header* mhdr, uint8_t* msg, size_t sz)
{

}

static int set_up(void** arg)
{
    should_stop = false;
}

static void size(void** arg)
{

}

int main()
{
    const struct CMUnitTest tests[] =
    {
        cmocka_unit_test_setup(size, set_up)
    };

    yella_load_settings_doc();
    yella_destroy_settings_doc();
    yella_settings_set_byte_size(u"agent", u"max-message-size", u"1M");
    yella_settings_set_uint(u"file", u"send-latency-seconds", 10);
    return cmocka_run_group_tests(tests, NULL, NULL);
}
