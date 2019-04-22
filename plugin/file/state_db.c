#include "plugin/file/state_db.h"
#include "common/settings.h"
#include "common/file.h"
#include "common/macro_util.h"
#include "common/text_util.h"
#include "file_reader.h"
#include <sqlite3.h>
#include <openssl/evp.h>
#include <unicode/ustring.h>
#include <chucho/logger.h>
#include <chucho/log.h>

enum
{
    STMT_INSERT,
    STMT_DELETE,
    STMT_UPDATE,
    STMT_SELECT_ATTRS
};

struct state_db
{
    chucho_logger_t* lgr;
    sqlite3* db;
    sqlite3_stmt* stmts[4];
    uds name;
};

static uds create_db_name(const UChar* const config_name)
{
    uds result;
    uint8_t sha1[EVP_MAX_MD_SIZE];
    unsigned sz;
    unsigned i;

    result = udsnew(yella_settings_get_dir(u"file", u"data-dir"));
    yella_ensure_dir_exists(result);
    if (result[0] != 0 && result[u_strlen(result) - 1] != YELLA_DIR_SEP[0])
        result = udscat(result, YELLA_DIR_SEP);
    EVP_Digest(config_name, u_strlen(config_name) * sizeof(UChar), sha1, &sz, EVP_sha1(), NULL);
    for (i = 0; i < sz; i++)
        result = udscatprintf(result, u"%02x", sha1[i]);
    result = udscat(result, u".sqlite");
    return result;
}

state_db* create_state_db(const UChar* const config_name)
{
    uds name;
    int i;
    int rc;
    state_db* st;
    char* sqlerr;
    /* These must remain in sync with the enum values */
    char* sqls[] =
    {
       "INSERT INTO 'state' (name, attributes) VALUES (?1, ?2);",
       "DELETE FROM 'state' WHERE name = ?1;",
       "UPDATE 'state' SET attributes = ?1 WHERE name = ?2;",
       "SELECT attributes FROM 'state' WHERE name = ?1;"
    };

    st = calloc(1, sizeof(state_db));
    st->lgr = chucho_get_logger("yella.file.db");
    name = create_db_name(config_name);
    rc = sqlite3_open16(name, &st->db);
    udsfree(name);
    if (rc != SQLITE_OK)
    {
        CHUCHO_C_FATAL(st->lgr, "Unable to create SQLite database: %s", sqlite3_errstr(rc));
        destroy_state_db(st, STATE_DB_ACTION_REMOVE);
        return NULL;
    }
    sqlite3_extended_result_codes(st->db, 1);
    rc = sqlite3_exec(st->db,
                      "CREATE TABLE IF NOT EXISTS 'state' (name TEXT UNIQUE, attributes BLOB);",
                      NULL,
                      NULL,
                      &sqlerr);
    if (rc != SQLITE_OK)
    {
        CHUCHO_C_FATAL(st->lgr, "Unable to create 'state' table: %s", sqlerr);
        sqlite3_free(sqlerr);
        destroy_state_db(st, STATE_DB_ACTION_REMOVE);
        return NULL;
    }
    for (i = 0; i < YELLA_ARRAY_SIZE(sqls); i++)
    {
        rc = sqlite3_prepare_v3(st->db,
                                sqls[i],
                                -1,
                                SQLITE_PREPARE_PERSISTENT,
                                &st->stmts[i],
                                NULL);
        if (rc != SQLITE_OK)
        {
            CHUCHO_C_FATAL(st->lgr, "Unable to prepare statement '%s': %s", st->stmts[i], sqlite3_errmsg(st->db));
            destroy_state_db(st, STATE_DB_ACTION_REMOVE);
            return NULL;
        }
    }
    st->name = udsnew(config_name);
    return st;
}

bool delete_from_state_db(state_db* st, const UChar* const elem_name)
{
    int rc;
    bool result;
    char* utf8;

    sqlite3_bind_text16(st->stmts[STMT_DELETE], 1, elem_name, -1, SQLITE_STATIC);
    rc = sqlite3_step(st->stmts[STMT_DELETE]);
    if (rc == SQLITE_DONE)
    {
        result = true;
    }
    else
    {
        utf8 = yella_to_utf8(elem_name);
        CHUCHO_C_ERROR(st->lgr, "Error deleting '%s': %s", utf8, sqlite3_errmsg(st->db));
        free(utf8);
        result = false;
    }
    sqlite3_clear_bindings(st->stmts[STMT_DELETE]);
    sqlite3_reset(st->stmts[STMT_DELETE]);
    return result;

}

void destroy_state_db(state_db* st, state_db_removal_action ra)
{
    int i;
    uds name;
    char* utf8;

    for (i = 0; i < YELLA_ARRAY_SIZE(st->stmts); i++)
        sqlite3_finalize(st->stmts[i]);
    sqlite3_close(st->db);
    if (ra == STATE_DB_ACTION_REMOVE)
    {
        name = create_db_name(st->name);
        yella_remove_file(name);
        udsfree(name);
    }
    chucho_release_logger(st->lgr);
    udsfree(st->name);
    free(st);
}

element* get_element_from_state_db(state_db* st, const UChar* const elem_name)
{
    int rc;
    char* utf8;
    element* result;

    result = NULL;
    sqlite3_bind_text16(st->stmts[STMT_SELECT_ATTRS], 1, elem_name, -1, SQLITE_STATIC);
    rc = sqlite3_step(st->stmts[STMT_SELECT_ATTRS]);
    if (rc == SQLITE_ROW)
    {
        result = create_element_with_attrs(elem_name, sqlite3_column_blob(st->stmts[STMT_SELECT_ATTRS], 0));
    }
    else if (rc != SQLITE_DONE)
    {
        utf8 = yella_to_utf8(elem_name);
        CHUCHO_C_DEBUG(st->lgr, "Error getting attributes for '%s': %s", utf8, sqlite3_errmsg(st->db));
        free(utf8);
    }
    sqlite3_clear_bindings(st->stmts[STMT_SELECT_ATTRS]);
    sqlite3_reset(st->stmts[STMT_SELECT_ATTRS]);
    return result;
}

bool insert_into_state_db(state_db* st, const element* const elem)
{
    uint8_t* attrs;
    size_t sz;
    int rc;
    bool result;
    char* utf8;

    attrs = pack_element_attributes(elem, &sz);
    sqlite3_bind_text16(st->stmts[STMT_INSERT], 1, element_name(elem), -1, SQLITE_STATIC);
    sqlite3_bind_blob(st->stmts[STMT_INSERT], 2, attrs, sz, SQLITE_STATIC);
    rc = sqlite3_step(st->stmts[STMT_INSERT]);
    free(attrs);
    if (rc == SQLITE_DONE)
    {
        result = true;
    }
    else
    {
        utf8 = yella_to_utf8(element_name(elem));
        CHUCHO_C_ERROR(st->lgr, "Error inserting '%s': %s", utf8, sqlite3_errmsg(st->db));
        free(utf8);
        result = false;
    }
    sqlite3_clear_bindings(st->stmts[STMT_INSERT]);
    sqlite3_reset(st->stmts[STMT_INSERT]);
    return result;
}

bool update_into_state_db(state_db* st, const element* const elem)
{
    uint8_t* attrs;
    size_t sz;
    int rc;
    bool result;
    char* utf8;

    attrs = pack_element_attributes(elem, &sz);
    sqlite3_bind_blob(st->stmts[STMT_UPDATE], 1, attrs, sz, SQLITE_STATIC);
    sqlite3_bind_text16(st->stmts[STMT_UPDATE], 2, element_name(elem), -1, SQLITE_STATIC);
    rc = sqlite3_step(st->stmts[STMT_UPDATE]);
    free(attrs);
    if (rc == SQLITE_DONE)
    {
        result = true;
    }
    else
    {
        utf8 = yella_to_utf8(element_name(elem));
        CHUCHO_C_ERROR(st->lgr, "Error updating '%s': %s", utf8, sqlite3_errmsg(st->db));
        free(utf8);
        result = false;
    }
    sqlite3_clear_bindings(st->stmts[STMT_UPDATE]);
    sqlite3_reset(st->stmts[STMT_UPDATE]);
    return result;
}

const UChar* state_db_name(const state_db* const sdb)
{
    return sdb->name;
}
