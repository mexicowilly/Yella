#include "common/parcel.h"
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <stdlib.h>
#include <cmocka.h>
#include <chucho/logger.h>
#include <unicode/ustring.h>

static void pack_unpack(void** arg)
{
    yella_parcel* pcl;
    uint8_t* packed;
    size_t sz;
    chucho_logger_t* lgr;
    yella_parcel* pcl2;

    pcl = yella_create_parcel(u"doggies", u"monkey boy");
    pcl->sender = udsnew(u"iguana");
    pcl->cmp = YELLA_COMPRESSION_LZ4;
    pcl->seq.major = 75;
    pcl->seq.minor = 57;
    pcl->grp = malloc(sizeof(yella_group));
    pcl->grp->identifier = udsnew(u"What?");
    pcl->grp->disposition = YELLA_GROUP_DISPOSITION_END;
    pcl->payload_size = strlen("pretty nuts");
    pcl->payload = malloc(pcl->payload_size);
    memcpy(pcl->payload, "pretty nuts", pcl->payload_size);
    lgr = chucho_get_logger("parcel_test");
    yella_log_parcel(pcl, lgr);
    packed = yella_pack_parcel(pcl, &sz);
    assert_non_null(packed);
    assert_true(sz > 0);
    pcl2 = yella_unpack_parcel(packed);
    free(packed);
    assert_non_null(pcl2);
    assert_int_equal(pcl->time / 1000, pcl2->time / 1000);
    assert_int_equal(u_strcmp(pcl->sender, pcl2->sender), 0);
    assert_int_equal(u_strcmp(pcl->recipient, pcl2->recipient), 0);
    assert_int_equal(u_strcmp(pcl->type, pcl2->type), 0);
    assert_int_equal(pcl->cmp, pcl2->cmp);
    assert_int_equal(pcl->seq.major, pcl2->seq.major);
    assert_int_equal(pcl->seq.minor, pcl2->seq.minor);
    assert_non_null(pcl2->grp);
    assert_int_equal(u_strcmp(pcl->grp->identifier, pcl2->grp->identifier), 0);
    assert_int_equal(pcl->grp->disposition, pcl2->grp->disposition);
    assert_int_equal(pcl->payload_size, pcl2->payload_size);
    assert_memory_equal(pcl->payload, pcl2->payload, pcl->payload_size);
    yella_log_parcel(pcl2, lgr);
    yella_destroy_parcel(pcl2);
    yella_destroy_parcel(pcl);
    chucho_release_logger(lgr);
}

int main()
{
    const struct CMUnitTest tests[] =
    {
        cmocka_unit_test(pack_unpack)
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}
