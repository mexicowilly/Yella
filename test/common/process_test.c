#include "common/process.h"
#include "common/uds.h"
#include "common/thread.h"
#include "common/text_util.h"
#include <stdio.h>
#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <stdlib.h>
#include <cmocka.h>

static void one_shot(void** arg)
{
    FILE* f;
    yella_process* proc;
    char buf[BUFSIZ];
    uds theirs;
    uds mine;
    UChar* utf16;

#if defined(YELLA_POSIX)
    f = popen("ls -l", "r");
#endif
    theirs = udsempty();
    while (fgets(buf, sizeof(buf), f) != NULL)
    {
        utf16 = yella_from_utf8(buf);
        udscat(theirs, utf16);
        free(utf16);
    }
    pclose(f);
#if defined(YELLA_POSIX)
    proc = yella_create_process(u"ls -l");
#endif
    assert_non_null(proc);
    mine = udsempty();
    while (fgets(buf, sizeof(buf), yella_process_get_reader(proc)) != NULL)
    {
        utf16 = yella_from_utf8(buf);
        udscat(mine, utf16);
        free(utf16);
    }
    yella_destroy_process(proc);
    assert_true(u_strcmp(mine, theirs) == 0);
    udsfree(mine);
    udsfree(theirs);
}

static void interrupted(void** arg)
{
    yella_process* proc;

    proc = yella_create_process(u"sleep 5");
    assert_non_null(proc);
    yella_sleep_this_thread(2000);
    yella_destroy_process(proc);
}

int main()
{
    const struct CMUnitTest tests[] =
    {
        cmocka_unit_test(one_shot),
        cmocka_unit_test(interrupted)
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}
