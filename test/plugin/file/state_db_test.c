#include "plugin/file/state_db.h"
#include "common/settings.h"
#include "common/file.h"
#include <stdio.h>
#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <stdlib.h>
#include <cmocka.h>
#include <plugin/file/attribute.h>
#include <unicode/ustring.h>

static void delete(void** arg)
{
    state_db* db;
    element* elem1;
    element* elem2;
    attribute* attr;

    db = create_state_db(u"monkey balls");
    elem1 = create_element(u"funky smalls");
    attr = malloc(sizeof(attribute));
    attr->type = ATTR_TYPE_FILE_TYPE;
    attr->value.int_value = YELLA_FILE_TYPE_REGULAR;
    add_element_attribute(elem1, attr);
    attr = malloc(sizeof(attribute));
    attr->type = ATTR_TYPE_SHA256;
    attr->value.byte_array.mem = malloc(5);
    memcpy(attr->value.byte_array.mem, "hello", 5);
    attr->value.byte_array.sz = 5;
    add_element_attribute(elem1, attr);
    assert_true(insert_into_state_db(db, elem1));
    assert_true(delete_from_state_db(db, u"funky smalls"));
    // Delete doesn't fail if the element is not there, just if there is a db failure
    assert_true(delete_from_state_db(db, u"blah blah"));
    destroy_element(elem1);
    assert_null(get_element_from_state_db(db, u"funky smalls"));
    destroy_state_db(db, STATE_DB_ACTION_REMOVE);
}

static void insert(void** arg)
{
    state_db* db;
    element* elem1;
    element* elem2;
    attribute* attr;

    db = create_state_db(u"monkey balls");
    elem1 = create_element(u"funky smalls");
    attr = malloc(sizeof(attribute));
    attr->type = ATTR_TYPE_FILE_TYPE;
    attr->value.int_value = YELLA_FILE_TYPE_REGULAR;
    add_element_attribute(elem1, attr);
    attr = malloc(sizeof(attribute));
    attr->type = ATTR_TYPE_SHA256;
    attr->value.byte_array.mem = malloc(5);
    memcpy(attr->value.byte_array.mem, "hello", 5);
    attr->value.byte_array.sz = 5;
    add_element_attribute(elem1, attr);
    assert_true(insert_into_state_db(db, elem1));
    assert_false(insert_into_state_db(db, elem1));
    elem2 = get_element_from_state_db(db, u"funky smalls");
    assert_int_equal(compare_element_attributes(elem1, elem2), 0);
    destroy_element(elem2);
    destroy_element(elem1);
    destroy_state_db(db, STATE_DB_ACTION_REMOVE);
}

static void name(void** arg)
{
    state_db* db;

    db = create_state_db(u"monkey balls");
    assert_int_equal(u_strcmp(state_db_name(db), u"monkey balls"), 0);
    assert_int_not_equal(u_strcmp(state_db_name(db), u"jeez"), 0);
    destroy_state_db(db, STATE_DB_ACTION_REMOVE);
}

static void update(void** arg)
{
    state_db* db;
    element* elem1;
    element* elem2;
    attribute* attr;

    db = create_state_db(u"monkey balls");
    elem1 = create_element(u"funky smalls");
    attr = malloc(sizeof(attribute));
    attr->type = ATTR_TYPE_FILE_TYPE;
    attr->value.int_value = YELLA_FILE_TYPE_REGULAR;
    add_element_attribute(elem1, attr);
    attr = malloc(sizeof(attribute));
    attr->type = ATTR_TYPE_SHA256;
    attr->value.byte_array.mem = malloc(5);
    memcpy(attr->value.byte_array.mem, "hello", 5);
    attr->value.byte_array.sz = 5;
    add_element_attribute(elem1, attr);
    assert_true(insert_into_state_db(db, elem1));
    elem2 = get_element_from_state_db(db, u"funky smalls");
    assert_int_equal(compare_element_attributes(elem1, elem2), 0);
    destroy_element(elem2);
    free(attr->value.byte_array.mem);
    attr->value.byte_array.mem = malloc(7);
    memcpy(attr->value.byte_array.mem, "goodbye", 7);
    attr->value.byte_array.sz = 7;
    assert_true(update_into_state_db(db, elem1));
    elem2 = get_element_from_state_db(db, u"funky smalls");
    assert_int_equal(compare_element_attributes(elem1, elem2), 0);
    destroy_element(elem2);
    destroy_element(elem1);
    elem1 = create_element(u"blah blah");
    // Update doesn't fail if the name isn't already in the db.
    assert_true(update_into_state_db(db, elem1));
    destroy_element(elem1);
    destroy_state_db(db, STATE_DB_ACTION_REMOVE);
}

int main()
{
    const struct CMUnitTest tests[] =
    {
        cmocka_unit_test(delete),
        cmocka_unit_test(insert),
        cmocka_unit_test(name),
        cmocka_unit_test(update)
    };

    yella_load_settings_doc();
    yella_destroy_settings_doc();
    yella_settings_set_text(u"file", u"data-dir", u"state-db-test-data");
    yella_remove_all(yella_settings_get_text(u"file", u"data-dir"));
    return cmocka_run_group_tests(tests, NULL, NULL);
}
