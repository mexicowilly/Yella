#include "common/message_header.h"
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
    yella_message_header* mhdr;
    uint8_t* pack;
    size_t pack_sz;
    yella_message_header* mhdr2;

    mhdr = yella_create_mhdr();
    assert_non_null(mhdr);
    mhdr->time = time(NULL);
    mhdr->sender = udsnew(u"doggy");
    mhdr->recipient = udsnew(u"iguana");
    mhdr->type = udsnew(u"fun stuff");
    mhdr->cmp = YELLA_COMPRESSION_LZ4;
    mhdr->seq.major = 12;
    mhdr->seq.minor = 13;
    mhdr->grp = malloc(sizeof(yella_group));
    mhdr->grp->disposition = YELLA_GROUP_DISPOSITION_MORE;
    mhdr->grp->identifier = udsnew(u"glucille");
    pack = yella_pack_mhdr(mhdr, &pack_sz);
    assert_non_null(pack);
    assert_true(pack_sz > 0);
    print_message("Packed header size: %zu\n", pack_sz);
    mhdr2 = yella_unpack_mhdr(pack);
    free(pack);
    assert_non_null(mhdr2);
    assert_int_equal(mhdr2->time, mhdr->time);
    assert_true(u_strcmp(mhdr2->sender, mhdr->sender) == 0);
    assert_true(u_strcmp(mhdr2->recipient, mhdr->recipient) == 0);
    assert_true(u_strcmp(mhdr2->type, mhdr->type) == 0);
    assert_int_equal(mhdr2->cmp, mhdr->cmp);
    assert_int_equal(mhdr2->seq.major, mhdr->seq.major);
    assert_int_equal(mhdr2->seq.minor, mhdr->seq.minor);
    assert_non_null(mhdr2->grp);
    assert_int_equal(mhdr2->grp->disposition, mhdr->grp->disposition);
    assert_true(u_strcmp(mhdr2->grp->identifier, mhdr->grp->identifier) == 0);
    yella_destroy_mhdr(mhdr2);
    yella_destroy_mhdr(mhdr);
}

int main()
{
    const struct CMUnitTest tests[] =
    {
        cmocka_unit_test(simple)
    };
    return cmocka_run_group_tests(tests, NULL, NULL);
}
