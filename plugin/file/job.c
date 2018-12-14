#include "plugin/file/job.h"
#include "plugin/file/file_name_matcher.h"
#include "plugin/file/collect_attributes.h"
#include "plugin/file/state_db_pool.h"
#include "common/file.h"
#include "common/uds_util.h"
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

static void process_and_send(element* elem, const job* const j, state_db* db)
{

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
