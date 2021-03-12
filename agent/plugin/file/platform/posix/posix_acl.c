#include "plugin/file/posix_acl.h"
#include "plugin/file/user_group.h"
#include "common/text_util.h"
#include <chucho/log.h>
#include <acl/libacl.h>
#include <errno.h>

static void acl_entry_destructor(void* elem, void* udata)
{
    posix_acl_entry* entry = (posix_acl_entry*)elem;

    free(entry->usr_grp.name);
    free(entry);
}

static int qsort_compare_posix_acl_entries(const void* lhs, const void* rhs)
{
    const posix_acl_entry* e1;
    const posix_acl_entry* e2;
    int result;

    e1 = *(const posix_acl_entry**)lhs;
    e2 = *(const posix_acl_entry**)rhs;
    result = e1->type - e2->type;
    if (result == 0)
    {
        result = (int64_t)e1->usr_grp.id - (int64_t)e2->usr_grp.id;
        if (result == 0)
            result = u_strcmp(e1->usr_grp.name, e2->usr_grp.name);
    }
    return result;
}

yella_ptr_vector* get_posix_acl(const UChar* const file_name, chucho_logger_t* lgr)
{
    acl_t acl;
    char* utf8;
    acl_entry_t entry;
    yella_ptr_vector* result;
    posix_acl_entry* result_entry;
    acl_tag_t tag;
    void* qualifier;
    acl_permset_t permset;

    result = NULL;
    utf8 = yella_to_utf8(file_name);
    acl = acl_get_file(utf8, ACL_TYPE_ACCESS);
    if (acl == NULL)
    {
        CHUCHO_C_ERROR(lgr, "Error getting ACL for '%s': %s", utf8, strerror(errno));
    }
    else
    {
        result = yella_create_ptr_vector();
        yella_set_ptr_vector_destructor(result, acl_entry_destructor, NULL);
        if (acl_get_entry(acl, ACL_FIRST_ENTRY, &entry))
        {
            do
            {
                acl_get_tag_type(entry, &tag);
                if (tag == ACL_USER)
                {
                    qualifier = acl_get_qualifier(entry);
                    result_entry = malloc(sizeof(posix_acl_entry));
                    result_entry->type = PACL_ENTRY_TYPE_USER;
                    result_entry->usr_grp.id = *(uid_t*)qualifier;
                    acl_free(qualifier);
                    result_entry->usr_grp.name = get_user_name(result_entry->usr_grp.id, lgr);
                    acl_get_permset(entry, &permset);
                    result_entry->perm.read = acl_get_perm(permset, ACL_READ) ? true : false;
                    result_entry->perm.write = acl_get_perm(permset, ACL_WRITE) ? true : false;
                    result_entry->perm.execute = acl_get_perm(permset, ACL_EXECUTE) ? true : false;
                    yella_push_back_ptr_vector(result, result_entry);

                }
                else if (tag == ACL_GROUP)
                {
                    qualifier = acl_get_qualifier(entry);
                    result_entry = malloc(sizeof(posix_acl_entry));
                    result_entry->type = PACL_ENTRY_TYPE_GROUP;
                    result_entry->usr_grp.id = *(gid_t*)qualifier;
                    acl_free(qualifier);
                    result_entry->usr_grp.name = get_user_name(result_entry->usr_grp.id, lgr);
                    result_entry->perm.read = acl_get_perm(permset, ACL_READ) ? true : false;
                    result_entry->perm.write = acl_get_perm(permset, ACL_WRITE) ? true : false;
                    result_entry->perm.execute = acl_get_perm(permset, ACL_EXECUTE) ? true : false;
                    yella_push_back_ptr_vector(result, result_entry);
                }
            } while (acl_get_entry(acl, ACL_NEXT_ENTRY, &entry));
            qsort(yella_ptr_vector_data(result), yella_ptr_vector_size(result), sizeof(void*), qsort_compare_posix_acl_entries);
        }
        acl_free(acl);
    }
    free(utf8);
    return result;
}
