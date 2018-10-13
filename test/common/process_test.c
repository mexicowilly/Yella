#include "common/process.h"
#include "common/sds.h"
#include "common/thread.h"
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
    sds theirs;
    sds mine;

#if defined(YELLA_POSIX)
    f = popen("ls -l", "r");
#endif
    theirs = sdsempty();
    while (fgets(buf, sizeof(buf), f) != NULL)
        sdscat(theirs, buf);
    pclose(f);
#if defined(YELLA_POSIX)
    proc = yella_create_process("ls -l");
#endif
    assert_non_null(proc);
    mine = sdsempty();
    while (fgets(buf, sizeof(buf), yella_process_get_reader(proc)) != NULL)
        sdscat(mine, buf);
    yella_destroy_process(proc);
    assert_string_equal(mine, theirs);
    sdsfree(mine);
    sdsfree(theirs);
}

static void interrupted(void** arg)
{
    yella_process* proc;

    proc = yella_create_process("sleep 5");
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
