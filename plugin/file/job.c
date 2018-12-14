#include "plugin/file/job.h"
#include "plugin/file/file_name_matcher.h"
#include "plugin/file/collect_attributes.h"
#include "plugin/file/state_db_pool.h"
#include "common/file.h"
#include "common/uds_util.h"
#include "common/text_util.h"
#include "file_builder.h"
#include <unicode/ustring.h>

static bool matches_excludes(const UChar* const name, const yella_ptr_vector* excludes)
{
    int i;

    for (i = 0; i < yella_ptr_vector_size(excludes); i++)
    {
        if (file_name_matches(name, yella_ptr_vector_at(excludes, i)))
            return true;
    }
    return false;
}

static void send_message(const UChar* const name, const element* elem, const job* j, yella_fb_file_condition_enum_t cond)
{
    flatcc_builder_t bld;
    char* utf8;
    uint8_t* raw;
    size_t sz;

    flatcc_builder_init(&bld);
    yella_fb_file_file_state_start_as_root(&bld);
    utf8 = yella_to_utf8(j->agent_api->agent_id);
    yella_fb_file_file_state_sender_create_str(&bld, utf8);
    free(utf8);
    utf8 = yella_to_utf8(j->config_name);
    yella_fb_file_file_state_config_name_create_str(&bld, utf8);
    free(utf8);
    utf8 = yella_to_utf8(name);
    yella_fb_file_file_state_file_name_create_str(&bld, utf8);
    free(utf8);
    yella_fb_file_file_state_cond_add(&bld, cond);
    yella_fb_file_file_state_attrs_add(&bld, pack_element_attributes_to_table(elem, &bld));
    yella_fb_file_file_state_end_as_root(&bld);
    raw = flatcc_builder_finalize_buffer(&bld, &sz);
    flatcc_builder_clear(&bld);
}

static void process_element(const UChar* const name, element* elem, const job* const j, state_db* db)
{
    element* db_elem;
    yella_fb_file_condition_enum_t cond;
    int cmp;

    db_elem = get_element_from_state_db(db, name);
    if (db_elem == NULL)
    {
        if (elem != NULL)
        {
            insert_into_state_db(db, elem);
            cond = yella_fb_file_condition_ADDED;
            cmp = 1;
        }
    }
    else if (elem == NULL)
    {
        delete_from_state_db(db, name);
        cond = yella_fb_file_condition_REMOVED;
        cmp = -1;
    }
    else
    {
        cmp = compare_element_attributes(elem, db_elem);
        if (cmp != 0)
        {
            update_into_state_db(db, elem);
            diff_elements(elem, db_elem);
            cond = yella_fb_file_condition_CHANGED;
        }
    }
    if (cmp != 0)
        send_message(name, elem, j, cond);
}

static void crawl_dir(const UChar* const dir, const UChar* const cur_incl, const job* const j, state_db* db)
{
    yella_directory_iterator* itor;
    const UChar* cur;
    element* existing_elem;
    yella_file_type ftype;

    itor = yella_create_directory_iterator(dir);
    cur = yella_directory_iterator_next(itor);
    while (cur != NULL)
    {
        if (file_name_matches(cur, cur_incl) && !matches_excludes(cur, j->excludes))
            existing_elem = collect_attributes(cur, j->attr_types, j->attr_type_count);
        if (existing_elem != NULL)
            destroy_element(existing_elem);
        if (yella_get_file_type(cur, &ftype) == YELLA_NO_ERROR &&
            ftype == YELLA_FILE_TYPE_DIRECTORY)
        {
            crawl_dir(cur, cur_incl, j, db);
        }
        cur = yella_directory_iterator_next(itor);
    }
}

static void run_one_include(const UChar* const incl, const job* const j, state_db* db)
{
    const UChar* special;
    uds unescaped;
    element* existing_elem;
    UChar* slash;
    uds top_dir;
    yella_file_type ftype;

    special = first_unescaped_special_char(incl);
    if (special == NULL)
    {
        unescaped = unescape_pattern(incl);
        if (!matches_excludes(unescaped, j->excludes))
            existing_elem = collect_attributes(unescaped, j->attr_types, j->attr_type_count);
        udsfree(unescaped);
        if (existing_elem != NULL)
            destroy_element(existing_elem);
    }
    else
    {
        slash = u_strrchr(special, YELLA_DIR_SEP[0]);
        if (slash != NULL)
        {
            top_dir = udsnewlen(incl, slash - incl);
            if (yella_get_file_type(top_dir, &ftype) == YELLA_NO_ERROR &&
                ftype == YELLA_FILE_TYPE_DIRECTORY)
            {
                crawl_dir(top_dir, incl, j, db);
            }
            udsfree(top_dir);
        }
    }
}

job* create_job(const UChar* const cfg, const yella_agent_api* api)
{
    job* result;

    result = calloc(1, sizeof(job));
    result->config_name = udsnew(cfg);
    result->includes = yella_create_uds_ptr_vector();
    result->excludes = yella_create_uds_ptr_vector();
    result->agent_api = api;
    return result;
}

void destroy_job(job* j)
{
    yella_destroy_ptr_vector(j->excludes);
    yella_destroy_ptr_vector(j->includes);
    udsfree(j->config_name);
    free(j->attr_types);
    free(j);
}

void run_job(const job* const j, state_db_pool* db_pool)
{
    int i;
    state_db* db;

    db = get_state_db_from_pool(db_pool, j->config_name);
    if (db != NULL)
    {
        for (i = 0; i < yella_ptr_vector_size(j->includes); i++)
            run_one_include(yella_ptr_vector_at(j->includes, i), j, db);
    }
}
