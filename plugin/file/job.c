#include "plugin/file/job.h"
#include "plugin/file/file_name_matcher.h"
#include "plugin/file/collect_attributes.h"
#include "plugin/file/state_db_pool.h"
#include "common/file.h"
#include "common/uds_util.h"
#include "common/text_util.h"
#include <unicode/ustring.h>
#include <sys/param.h>

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

static void process_element(const UChar* const name, element* elem, const job* const j, state_db* db)
{
    element* db_elem;
    yella_fb_file_condition_enum_t cond;
    int cmp;

    cmp = 0;
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
    destroy_element(db_elem);
    if (cmp != 0)
        add_accumulator_message(j->acc, j->recipient, j->config_name, name, elem, cond);
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
        process_element(cur, existing_elem, j, db);
        if (existing_elem != NULL)
            destroy_element(existing_elem);
        if (yella_get_file_type(cur, &ftype) == YELLA_NO_ERROR &&
            ftype == YELLA_FILE_TYPE_DIRECTORY)
        {
            crawl_dir(cur, cur_incl, j, db);
        }
        cur = yella_directory_iterator_next(itor);
    }
    yella_destroy_directory_iterator(itor);
}

static void run_one_include(const UChar* const incl, const job* const j, state_db* db)
{
    const UChar* special;
    uds unescaped;
    element* existing_elem;
    uds top_dir;
    yella_file_type ftype;
    char* utf8;

    special = first_unescaped_special_char(incl);
    if (special == NULL)
    {
        unescaped = unescape_pattern(incl);
        existing_elem = collect_attributes(unescaped, j->attr_types, j->attr_type_count);
        process_element(unescaped, existing_elem, j, db);
        udsfree(unescaped);
        if (existing_elem != NULL)
            destroy_element(existing_elem);
    }
    else
    {
        while (*special != YELLA_DIR_SEP[0] && special >= incl)
            --special;
        if (special >= incl)
        {
            top_dir = udsnewlen(incl, (special == incl) ? 1 : special - incl);
            if (yella_get_file_type(top_dir, &ftype) == YELLA_NO_ERROR &&
                ftype == YELLA_FILE_TYPE_DIRECTORY)
            {
                crawl_dir(top_dir, incl, j, db);
            }
            udsfree(top_dir);
        }
    }
}

job* create_job(const UChar* const cfg_name,
                const UChar* const recipient,
                accumulator* acc)
{
    job* result;

    result = calloc(1, sizeof(job));
    result->config_name = udsnew(cfg_name);
    result->recipient = udsnew(recipient);
    result->acc = acc;
    result->includes = yella_create_uds_ptr_vector();
    result->excludes = yella_create_uds_ptr_vector();
    return result;
}

void destroy_job(job* j)
{
    yella_destroy_ptr_vector(j->excludes);
    yella_destroy_ptr_vector(j->includes);
    udsfree(j->recipient);
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
