#include "plugin/file/job.h"
#include "common/file.h"
#include "common/sglib.h"
#include "common/text_util.h"
#include "common/settings.h"
#include "file_reader.h"
#include <stdio.h>
#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <stdlib.h>
#include <cmocka.h>
#include <unicode/ustdio.h>
#include <unicode/ustring.h>
#include <openssl/evp.h>
#include <plugin/file/attribute.h>

typedef struct attr_node
{
    attribute attr;
    struct attr_node* next;
} attr_node;

typedef struct test_node
{
    uds file_name;
    uds config_name;
    attr_node* attrs;
    yella_fb_file_condition_enum_t cond;
    char color;
    struct test_node* left;
    struct test_node* right;
} test_node;

typedef struct test_data
{
    yella_agent_api api;
    uds data_dir;
    test_node* files;
    state_db_pool* db_pool;
    uds topic;
} test_data;

#define TEST_NODE_COMPARATOR(lhs, rhs) (u_strcmp(lhs->file_name, rhs->file_name))

SGLIB_DEFINE_RBTREE_PROTOTYPES(test_node, left, right, color, TEST_NODE_COMPARATOR);
SGLIB_DEFINE_RBTREE_FUNCTIONS(test_node, left, right, color, TEST_NODE_COMPARATOR);

#define ATTR_COMPARATOR(lhs, rhs) (lhs->attr.type - rhs->attr.type)

SGLIB_DEFINE_SORTED_LIST_PROTOTYPES(attr_node, ATTR_COMPARATOR, next);
SGLIB_DEFINE_SORTED_LIST_FUNCTIONS(attr_node, ATTR_COMPARATOR, next);

static yella_file_type fb_to_file_type(yella_fb_file_file_type_enum_t fbt)
{
    yella_file_type result;

    switch (fbt)
    {
    case yella_fb_file_file_type_BLOCK_SPECIAL:
        result = YELLA_FILE_TYPE_BLOCK_SPECIAL;
        break;
    case yella_fb_file_file_type_CHARACTER_SPECIAL:
        result = YELLA_FILE_TYPE_CHARACTER_SPECIAL;
        break;
    case yella_fb_file_file_type_DIRECTORY:
        result = YELLA_FILE_TYPE_DIRECTORY;
        break;
    case yella_fb_file_file_type_FIFO:
        result = YELLA_FILE_TYPE_FIFO;
        break;
    case yella_fb_file_file_type_SYMBOLIC_LINK:
        result = YELLA_FILE_TYPE_SYMBOLIC_LINK;
        break;
    case yella_fb_file_file_type_REGULAR:
        result = YELLA_FILE_TYPE_REGULAR;
        break;
    case yella_fb_file_file_type_SOCKET:
        result = YELLA_FILE_TYPE_SOCKET;
        break;
    case yella_fb_file_file_type_WHITEOUT:
        result = YELLA_FILE_TYPE_WHITEOUT;
        break;
    default:
        assert(false);
    }
    return result;
}

static void get_sha256(const UChar* const file_name, unsigned char* buf)
{
    unsigned md_len;
    size_t fsz;
    uint8_t* contents;
    FILE* f;
    char* utf8;

    yella_file_size(file_name, &fsz);
    contents = malloc(fsz);
    utf8 = yella_to_utf8(file_name);
    f = fopen(utf8, "rb");
    free(utf8);
    fread(contents, 1, fsz, f);
    fclose(f);
    EVP_Digest(contents, fsz, buf, &md_len, EVP_sha256(), NULL);
    free(contents);
    assert_int_equal(md_len, 32);
}

static void send_message(void* agnt, const UChar* const tpc, const uint8_t* const msg, size_t sz)
{
    yella_fb_file_file_state_table_t st;
    yella_fb_file_attr_vec_t attrv;
    yella_fb_file_attr_table_t attr;
    const char* str;
    test_node to_find;
    UChar* utf16;
    test_node* found;
    test_data* td;
    int i;
    attr_node* found_attr;
    attr_node attr_to_find;

    td = agnt;
    assert_int_equal(u_strcmp(tpc, td->topic), 0);
    st = yella_fb_file_file_state_as_root(msg);
    str = yella_fb_file_file_state_sender(st);
    assert_string_equal(str, "job-test-agent");
    str = yella_fb_file_file_state_file_name(st);
    utf16 = yella_from_utf8(str);
    to_find.file_name = udsnew(utf16);
    free(utf16);
    found = sglib_test_node_find_member(td->files, &to_find);
    assert_non_null(found);
    str = yella_fb_file_file_state_config_name(st);
    utf16 = yella_from_utf8(str);
    assert_int_equal(u_strcmp(utf16, found->config_name), 0);
    free(utf16);
    assert_int_equal(yella_fb_file_file_state_cond(st), found->cond);
    if (yella_fb_file_file_state_attrs_is_present(st))
    {
        attrv = yella_fb_file_attr_array_attrs(yella_fb_file_file_state_attrs(st));
        for (i = 0; i < yella_fb_file_attr_vec_len(attrv); i++)
        {
            attr = yella_fb_file_attr_vec_at(attrv, i);
            switch (yella_fb_file_attr_type(attr))
            {
            case yella_fb_file_attr_type_FILE_TYPE:
                attr_to_find.attr.type = ATTR_TYPE_FILE_TYPE;
                found_attr = sglib_attr_node_find_member(found->attrs, &attr_to_find);
                assert_non_null(found_attr);
                assert_int_equal(fb_to_file_type(yella_fb_file_attr_ftype(attr)), found_attr->attr.value.integer);
                break;
            case yella_fb_file_attr_type_SHA256:
                attr_to_find.attr.type = ATTR_TYPE_SHA256;
                found_attr = sglib_attr_node_find_member(found->attrs, &attr_to_find);
                assert_non_null(found_attr);
                assert_int_equal(flatbuffers_uint8_vec_len(yella_fb_file_attr_bytes(attr)),
                                 found_attr->attr.value.byte_array.sz);
                assert_memory_equal(yella_fb_file_attr_bytes(attr), found_attr->attr.value.byte_array.mem,
                                    found_attr->attr.value.byte_array.sz);
                break;
            }
            sglib_attr_node_delete(&found->attrs, found_attr);
            free(found_attr);
        }
    }
    assert_int_equal(sglib_attr_node_len(found->attrs), 0);
    sglib_test_node_delete(&td->files, found);
    udsfree(found->file_name);
    udsfree(found->config_name);
    free(found);
}

static int set_up(void** arg)
{
    test_data* td;

    td = malloc(sizeof(test_data));
    td->api.agent_id = udsnew(u"job-test-agent");
    td->api.send_message = send_message;
    td->data_dir = udsnew(u"job-test-data");
    td->data_dir = udscat(td->data_dir, YELLA_DIR_SEP);
    td->db_pool = create_state_db_pool();
    td->files = NULL;
    td->topic = udsnew(u"my cool topic");
    yella_remove_all(td->data_dir);
    yella_ensure_dir_exists(td->data_dir);
    *arg = td;
    return 0;
}

static int tear_down(void** arg)
{
    test_data* td;

    td = *arg;
    udsfree(td->api.agent_id);
    destroy_state_db_pool(td->db_pool);
    udsfree(td->topic);
    free(td);
    return 0;
}

static void single(void** arg)
{
    test_data* td;
    job* j;
    uds file_name;
    UFILE* uf;
    test_node* tn;
    attr_node* expect;
    int i;

    td = *arg;
    file_name = udscatprintf(udsempty(), u"%Ssingle", td->data_dir);
    for (i = 0; i < 3; i++)
    {
        if (i == 0 || i == 1)
        {
            uf = u_fopen_u(file_name, "w", NULL, NULL);
            u_fprintf(uf, "%i", i);
            u_fclose(uf);
        }
        else
        {
            yella_remove_file(file_name);
        }
        j = create_job(u"single-cfg", &td->api, td->topic, td);
        yella_push_back_ptr_vector(j->includes, udsdup(file_name));
        j->attr_type_count = 2;
        j->attr_types = malloc(sizeof(attribute_type) * j->attr_type_count);
        j->attr_types[0] = ATTR_TYPE_FILE_TYPE;
        j->attr_types[1] = ATTR_TYPE_SHA256;
        tn = calloc(1, sizeof(test_node));
        if (i == 0)
            tn->cond = yella_fb_file_condition_ADDED;
        else if (i == 1)
            tn->cond = yella_fb_file_condition_CHANGED;
        else
            tn->cond = yella_fb_file_condition_REMOVED;
        tn->file_name = udsdup(file_name);
        tn->config_name = udsdup(j->config_name);
        if (i == 0)
        {
            expect = malloc(sizeof(attr_node));
            expect->attr.type = ATTR_TYPE_FILE_TYPE;
            expect->attr.value.integer = YELLA_FILE_TYPE_REGULAR;
            sglib_attr_node_add(&tn->attrs, expect);
        }
        if (i != 2)
        {
            expect = malloc(sizeof(attr_node));
            expect->attr.type = ATTR_TYPE_SHA256;
            expect->attr.value.byte_array.sz = 32;
            expect->attr.value.byte_array.mem = malloc(expect->attr.value.byte_array.sz);
            get_sha256(file_name, expect->attr.value.byte_array.mem);
            sglib_attr_node_add(&tn->attrs, expect);
        }
        sglib_test_node_add(&td->files, tn);
        run_job(j, td->db_pool);
        destroy_job(j);
        assert_int_equal(sglib_test_node_len(td->files), 0);
    }
    udsfree(file_name);
}

void wild(void** arg)
{
    test_data* td;
    test_node* tn;
    job* j;
    attr_node* expect;
    UFILE* uf;

    td = *arg;
    j = create_job(u"wild-cfg", &td->api, td->topic, td);
    yella_push_back_ptr_vector(j->includes, udscatprintf(udsempty(), u"%S**", td->data_dir));
    j->attr_type_count = 1;
    j->attr_types = malloc(sizeof(attribute_type));
    j->attr_types[0] = ATTR_TYPE_FILE_TYPE;
    tn = calloc(1, sizeof(test_node));
    tn->file_name = udscatprintf(udsempty(), u"%Swild-one", td->data_dir);
    tn->cond = yella_fb_file_condition_ADDED;
    tn->config_name = udsdup(j->config_name);
    expect = malloc(sizeof(attr_node));
    expect->attr.type = ATTR_TYPE_FILE_TYPE;
    expect->attr.value.integer = YELLA_FILE_TYPE_REGULAR;
    sglib_attr_node_add(&tn->attrs, expect);
    sglib_test_node_add(&td->files, tn);
    uf = u_fopen_u(tn->file_name, "w", NULL, NULL);
    u_fclose(uf);
    tn = calloc(1, sizeof(test_node));
    expect = malloc(sizeof(attr_node));
    expect->attr.type = ATTR_TYPE_FILE_TYPE;
    expect->attr.value.integer = YELLA_FILE_TYPE_DIRECTORY;
    tn->file_name = udscatprintf(udsempty(), u"%Swild-dir", td->data_dir);
    tn->cond = yella_fb_file_condition_ADDED;
    tn->config_name = udsdup(j->config_name);
    yella_ensure_dir_exists(tn->file_name);
    sglib_attr_node_add(&tn->attrs, expect);
    sglib_test_node_add(&td->files, tn);
    tn = calloc(1, sizeof(test_node));
    tn->file_name = udscatprintf(udsempty(), u"%Swild-dir/jumpy", td->data_dir);
    tn->cond = yella_fb_file_condition_ADDED;
    tn->config_name = udsdup(j->config_name);
    expect = malloc(sizeof(attr_node));
    expect->attr.type = ATTR_TYPE_FILE_TYPE;
    expect->attr.value.integer = YELLA_FILE_TYPE_REGULAR;
    sglib_attr_node_add(&tn->attrs, expect);
    sglib_test_node_add(&td->files, tn);
    uf = u_fopen_u(tn->file_name, "w", NULL, NULL);
    u_fclose(uf);
    run_job(j, td->db_pool);
    destroy_job(j);
    assert_int_equal(sglib_test_node_len(td->files), 0);
}

int main()
{
    const struct CMUnitTest tests[] =
    {
        cmocka_unit_test_setup_teardown(single, set_up, tear_down),
        cmocka_unit_test_setup_teardown(wild, set_up, tear_down)
    };

    yella_load_settings_doc();
    yella_destroy_settings_doc();
    yella_settings_set_text(u"file", u"data-dir", u"state-db-test-data");
    yella_remove_all(yella_settings_get_text(u"file", u"data-dir"));
    yella_settings_set_uint(u"file", u"max-spool-dbs", 10);
    return cmocka_run_group_tests(tests, NULL, NULL);
}
