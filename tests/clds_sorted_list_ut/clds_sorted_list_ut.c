// Licensed under the MIT license. See LICENSE file in the project root for full license information.

#ifdef __cplusplus
#include <cstdlib>
#else
#include <stdlib.h>
#endif

#include "testrunnerswitcher.h"

void* real_malloc(size_t size)
{
    return malloc(size);
}

void real_free(void* ptr)
{
    free(ptr);
}

#include "umock_c.h"
#include "umocktypes_stdint.h"

#define ENABLE_MOCKS

#include "azure_c_shared_utility/gballoc.h"
#include "clds/clds_st_hash_set.h"
#include "clds/clds_hazard_pointers.h"

#undef ENABLE_MOCKS

#include "clds/clds_sorted_list.h"
#include "../reals/real_clds_st_hash_set.h"
#include "../reals/real_clds_hazard_pointers.h"

static TEST_MUTEX_HANDLE test_serialize_mutex;
static TEST_MUTEX_HANDLE g_dllByDll;

TEST_DEFINE_ENUM_TYPE(CLDS_SORTED_LIST_INSERT_RESULT, CLDS_SORTED_LIST_INSERT_RESULT_VALUES);
IMPLEMENT_UMOCK_C_ENUM_TYPE(CLDS_SORTED_LIST_INSERT_RESULT, CLDS_SORTED_LIST_INSERT_RESULT_VALUES);
TEST_DEFINE_ENUM_TYPE(CLDS_SORTED_LIST_DELETE_RESULT, CLDS_SORTED_LIST_DELETE_RESULT_VALUES);
IMPLEMENT_UMOCK_C_ENUM_TYPE(CLDS_SORTED_LIST_DELETE_RESULT, CLDS_SORTED_LIST_DELETE_RESULT_VALUES);
TEST_DEFINE_ENUM_TYPE(CLDS_SORTED_LIST_REMOVE_RESULT, CLDS_SORTED_LIST_REMOVE_RESULT_VALUES);
IMPLEMENT_UMOCK_C_ENUM_TYPE(CLDS_SORTED_LIST_REMOVE_RESULT, CLDS_SORTED_LIST_REMOVE_RESULT_VALUES);

DEFINE_ENUM_STRINGS(UMOCK_C_ERROR_CODE, UMOCK_C_ERROR_CODE_VALUES)

static void on_umock_c_error(UMOCK_C_ERROR_CODE error_code)
{
    char temp_str[256];
    (void)snprintf(temp_str, sizeof(temp_str), "umock_c reported error :%s", ENUM_TO_STRING(UMOCK_C_ERROR_CODE, error_code));
    ASSERT_FAIL(temp_str);
}

MOCK_FUNCTION_WITH_CODE(, void, test_item_cleanup_func, void*, context, CLDS_SORTED_LIST_ITEM*, item)
MOCK_FUNCTION_END()

typedef struct TEST_ITEM_TAG
{
    uint32_t key;
} TEST_ITEM;

DECLARE_SORTED_LIST_NODE_TYPE(TEST_ITEM)

static void* test_get_item_key(void* context, struct CLDS_SORTED_LIST_ITEM_TAG* item)
{
    TEST_ITEM* test_item = CLDS_SORTED_LIST_GET_VALUE(TEST_ITEM, item);
    (void)context;
    return (void*)(uintptr_t)test_item->key;
}

static int test_key_compare(void* context, void* key1, void* key2)
{
    (void)context;
    return (int)key1 - (int)key2;
}

BEGIN_TEST_SUITE(clds_sorted_list_unittests)

TEST_SUITE_INITIALIZE(suite_init)
{
    int result;

    TEST_INITIALIZE_MEMORY_DEBUG(g_dllByDll);

    test_serialize_mutex = TEST_MUTEX_CREATE();
    ASSERT_IS_NOT_NULL(test_serialize_mutex);

    result = umock_c_init(on_umock_c_error);
    ASSERT_ARE_EQUAL(int, 0, result, "umock_c_init failed");

    result = umocktypes_stdint_register_types();
    ASSERT_ARE_EQUAL(int, 0, result, "umocktypes_stdint_register_types failed");

    REGISTER_CLDS_ST_HASH_SET_GLOBAL_MOCK_HOOKS();
    REGISTER_CLDS_HAZARD_POINTERS_GLOBAL_MOCK_HOOKS();

    REGISTER_GLOBAL_MOCK_HOOK(gballoc_malloc, real_malloc);
    REGISTER_GLOBAL_MOCK_HOOK(gballoc_free, real_free);

    REGISTER_TYPE(CLDS_SORTED_LIST_INSERT_RESULT, CLDS_SORTED_LIST_INSERT_RESULT);
    REGISTER_TYPE(CLDS_SORTED_LIST_DELETE_RESULT, CLDS_SORTED_LIST_DELETE_RESULT);
    REGISTER_TYPE(CLDS_SORTED_LIST_REMOVE_RESULT, CLDS_SORTED_LIST_REMOVE_RESULT);

    REGISTER_UMOCK_ALIAS_TYPE(RECLAIM_FUNC, void*);
    REGISTER_UMOCK_ALIAS_TYPE(CLDS_HAZARD_POINTERS_HANDLE, void*);
    REGISTER_UMOCK_ALIAS_TYPE(CLDS_HAZARD_POINTER_RECORD_HANDLE, void*);
    REGISTER_UMOCK_ALIAS_TYPE(CLDS_HAZARD_POINTERS_THREAD_HANDLE, void*);
    REGISTER_UMOCK_ALIAS_TYPE(CLDS_ST_HASH_SET_COMPUTE_HASH_FUNC, void*);
    REGISTER_UMOCK_ALIAS_TYPE(CLDS_ST_HASH_SET_HANDLE, void*);
}

TEST_SUITE_CLEANUP(suite_cleanup)
{
    umock_c_deinit();

    TEST_MUTEX_DESTROY(test_serialize_mutex);
    TEST_DEINITIALIZE_MEMORY_DEBUG(g_dllByDll);
}

TEST_FUNCTION_INITIALIZE(method_init)
{
    if (TEST_MUTEX_ACQUIRE(test_serialize_mutex))
    {
        ASSERT_FAIL("Could not acquire test serialization mutex.");
    }

    umock_c_reset_all_calls();
}

TEST_FUNCTION_CLEANUP(method_cleanup)
{
    TEST_MUTEX_RELEASE(test_serialize_mutex);
}

/* clds_sorted_list_create */

/* Tests_SRS_CLDS_SORTED_LIST_01_001: [ `clds_sorted_list_create` shall create a new sorted list object and on success it shall return a non-NULL handle to the newly created list. ]*/
TEST_FUNCTION(clds_hash_table_create_succeeds)
{
    // arrange
    CLDS_HAZARD_POINTERS_HANDLE hazard_pointers = clds_hazard_pointers_create();
    CLDS_SORTED_LIST_HANDLE list;
    umock_c_reset_all_calls();

    STRICT_EXPECTED_CALL(malloc(IGNORED_NUM_ARG));

    // act
    list = clds_sorted_list_create(hazard_pointers, test_get_item_key, (void*)0x4242, test_key_compare, (void*)0x4243);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
    ASSERT_IS_NOT_NULL(list);

    // cleanup
    clds_sorted_list_destroy(list);
    clds_hazard_pointers_destroy(hazard_pointers);
}

/* Tests_SRS_CLDS_SORTED_LIST_01_002: [ If any error happens, `clds_sorted_list_create` shall fail and return NULL. ]*/
TEST_FUNCTION(when_allocating_memory_fails_clds_hash_table_create_fails)
{
    // arrange
    CLDS_HAZARD_POINTERS_HANDLE hazard_pointers = clds_hazard_pointers_create();
    CLDS_SORTED_LIST_HANDLE list;
    umock_c_reset_all_calls();

    STRICT_EXPECTED_CALL(malloc(IGNORED_NUM_ARG))
        .SetReturn(NULL);

    // act
    list = clds_sorted_list_create(hazard_pointers, test_get_item_key, (void*)0x4242, test_key_compare, (void*)0x4243);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
    ASSERT_IS_NULL(list);

    // cleanup
    clds_hazard_pointers_destroy(hazard_pointers);
}

/* Tests_SRS_CLDS_SORTED_LIST_01_003: [ If `clds_hazard_pointers` is NULL, `clds_sorted_list_create` shall fail and return NULL. ]*/
TEST_FUNCTION(clds_hash_table_create_with_NULL_clds_hazard_pointers_fails)
{
    // arrange
    CLDS_SORTED_LIST_HANDLE list;

    // act
    list = clds_sorted_list_create(NULL, test_get_item_key, (void*)0x4242, test_key_compare, (void*)0x4243);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
    ASSERT_IS_NULL(list);
}

/* Tests_SRS_CLDS_SORTED_LIST_01_045: [ If `get_item_key_cb` is NULL, `clds_sorted_list_create` shall fail and return NULL. ]*/
TEST_FUNCTION(clds_hash_table_create_with_NULL_get_item_key_cb_fails)
{
    // arrange
    CLDS_HAZARD_POINTERS_HANDLE hazard_pointers = clds_hazard_pointers_create();
    CLDS_SORTED_LIST_HANDLE list;
    umock_c_reset_all_calls();

    // act
    list = clds_sorted_list_create(hazard_pointers, NULL, (void*)0x4242, test_key_compare, (void*)0x4243);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
    ASSERT_IS_NULL(list);

    // cleanup
    clds_hazard_pointers_destroy(hazard_pointers);
}

/* Tests_SRS_CLDS_SORTED_LIST_01_049: [ `get_item_key_cb_context` shall be allowed to be NULL. ]*/
TEST_FUNCTION(clds_hash_table_create_with_NULL_get_item_key_cb_context_succeeds)
{
    // arrange
    CLDS_HAZARD_POINTERS_HANDLE hazard_pointers = clds_hazard_pointers_create();
    CLDS_SORTED_LIST_HANDLE list;
    umock_c_reset_all_calls();

    STRICT_EXPECTED_CALL(malloc(IGNORED_NUM_ARG));

    // act
    list = clds_sorted_list_create(hazard_pointers, test_get_item_key, NULL, test_key_compare, (void*)0x4243);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
    ASSERT_IS_NOT_NULL(list);

    // cleanup
    clds_sorted_list_destroy(list);
    clds_hazard_pointers_destroy(hazard_pointers);
}

/* Tests_SRS_CLDS_SORTED_LIST_01_046: [ If `key_compare_cb` is NULL, `clds_sorted_list_create` shall fail and return NULL. ]*/
TEST_FUNCTION(clds_hash_table_create_with_NULL_key_compare_cb_fails)
{
    // arrange
    CLDS_HAZARD_POINTERS_HANDLE hazard_pointers = clds_hazard_pointers_create();
    CLDS_SORTED_LIST_HANDLE list;
    umock_c_reset_all_calls();

    // act
    list = clds_sorted_list_create(hazard_pointers, test_get_item_key, (void*)0x4242, NULL, (void*)0x4243);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
    ASSERT_IS_NULL(list);

    // cleanup
    clds_hazard_pointers_destroy(hazard_pointers);
}

/* Tests_SRS_CLDS_SORTED_LIST_01_050: [ `key_compare_cb_context` shall be allowed to be NULL. ]*/
TEST_FUNCTION(clds_hash_table_create_with_NULL_key_compare_cb_context_context_succeeds)
{
    // arrange
    CLDS_HAZARD_POINTERS_HANDLE hazard_pointers = clds_hazard_pointers_create();
    CLDS_SORTED_LIST_HANDLE list;
    umock_c_reset_all_calls();

    STRICT_EXPECTED_CALL(malloc(IGNORED_NUM_ARG));

    // act
    list = clds_sorted_list_create(hazard_pointers, test_get_item_key, (void*)0x4242, test_key_compare, NULL);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
    ASSERT_IS_NOT_NULL(list);

    // cleanup
    clds_sorted_list_destroy(list);
    clds_hazard_pointers_destroy(hazard_pointers);
}

/* clds_sorted_list_destroy */

/* Tests_SRS_CLDS_SORTED_LIST_01_004: [ `clds_sorted_list_destroy` shall free all resources associated with the sorted list instance. ]*/
TEST_FUNCTION(clds_sorted_list_destroy_frees_the_allocated_list_resources)
{
    // arrange
    CLDS_HAZARD_POINTERS_HANDLE hazard_pointers = clds_hazard_pointers_create();
    CLDS_SORTED_LIST_HANDLE list;
    list = clds_sorted_list_create(hazard_pointers, test_get_item_key, (void*)0x4242, test_key_compare, (void*)0x4243);
    umock_c_reset_all_calls();

    STRICT_EXPECTED_CALL(free(IGNORED_NUM_ARG));

    // act
    clds_sorted_list_destroy(list);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

    // cleanup
    clds_hazard_pointers_destroy(hazard_pointers);
}

/* Tests_SRS_CLDS_SORTED_LIST_01_005: [ If `clds_sorted_list` is NULL, `clds_sorted_list_destroy` shall return. ]*/
TEST_FUNCTION(clds_sorted_list_destroy_with_NULL_handle_returns)
{
    // arrange

    // act
    clds_sorted_list_destroy(NULL);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
}

/* Tests_SRS_CLDS_SORTED_LIST_01_039: [ Any items still present in the list shall be freed. ]*/
/* Tests_SRS_CLDS_SORTED_LIST_01_040: [ For each item that is freed, the callback `item_cleanup_callback` passed to `clds_sorted_list_node_create` shall be called, while passing `item_cleanup_callback_context` and the freed item as arguments. ]*/
TEST_FUNCTION(clds_sorted_list_destroy_with_1_item_in_the_list_frees_the_item_and_triggers_user_cleanup_callback)
{
    // arrange
    CLDS_HAZARD_POINTERS_HANDLE hazard_pointers = clds_hazard_pointers_create();
    CLDS_HAZARD_POINTERS_THREAD_HANDLE hazard_pointers_thread = clds_hazard_pointers_register_thread(hazard_pointers);
    CLDS_SORTED_LIST_HANDLE list;
    list = clds_sorted_list_create(hazard_pointers, test_get_item_key, (void*)0x4242, test_key_compare, (void*)0x4243);
    CLDS_SORTED_LIST_ITEM* item = CLDS_SORTED_LIST_NODE_CREATE(TEST_ITEM, test_item_cleanup_func, (void*)0x4242);
    TEST_ITEM* item_payload = CLDS_SORTED_LIST_GET_VALUE(TEST_ITEM, item);
    item_payload->key = 0x42;
    (void)clds_sorted_list_insert(list, hazard_pointers_thread, item);
    umock_c_reset_all_calls();

    STRICT_EXPECTED_CALL(test_item_cleanup_func((void*)0x4242, item));
    STRICT_EXPECTED_CALL(free(IGNORED_NUM_ARG));
    STRICT_EXPECTED_CALL(free(IGNORED_NUM_ARG));

    // act
    clds_sorted_list_destroy(list);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

    // cleanup
    clds_hazard_pointers_destroy(hazard_pointers);
}

/* Tests_SRS_CLDS_SORTED_LIST_01_039: [ Any items still present in the list shall be freed. ]*/
/* Tests_SRS_CLDS_SORTED_LIST_01_040: [ For each item that is freed, the callback `item_cleanup_callback` passed to `clds_sorted_list_node_create` shall be called, while passing `item_cleanup_callback_context` and the freed item as arguments. ]*/
TEST_FUNCTION(clds_sorted_list_destroy_with_2_items_in_the_list_frees_the_item_and_triggers_user_cleanup_callback)
{
    // arrange
    CLDS_HAZARD_POINTERS_HANDLE hazard_pointers = clds_hazard_pointers_create();
    CLDS_HAZARD_POINTERS_THREAD_HANDLE hazard_pointers_thread = clds_hazard_pointers_register_thread(hazard_pointers);
    CLDS_SORTED_LIST_HANDLE list;
    list = clds_sorted_list_create(hazard_pointers, test_get_item_key, (void*)0x4242, test_key_compare, (void*)0x4243);
    CLDS_SORTED_LIST_ITEM* item_1 = CLDS_SORTED_LIST_NODE_CREATE(TEST_ITEM, test_item_cleanup_func, (void*)0x4242);
    CLDS_SORTED_LIST_ITEM* item_2 = CLDS_SORTED_LIST_NODE_CREATE(TEST_ITEM, test_item_cleanup_func, (void*)0x4242);
    TEST_ITEM* item_1_payload = CLDS_SORTED_LIST_GET_VALUE(TEST_ITEM, item_1);
    TEST_ITEM* item_2_payload = CLDS_SORTED_LIST_GET_VALUE(TEST_ITEM, item_2);
    item_1_payload->key = 0x42;
    item_2_payload->key = 0x43;
    (void)clds_sorted_list_insert(list, hazard_pointers_thread, item_1);
    (void)clds_sorted_list_insert(list, hazard_pointers_thread, item_2);
    umock_c_reset_all_calls();

    STRICT_EXPECTED_CALL(clds_st_hash_set_create(IGNORED_PTR_ARG, IGNORED_NUM_ARG)).IgnoreAllCalls();
    STRICT_EXPECTED_CALL(clds_st_hash_set_destroy(IGNORED_PTR_ARG)).IgnoreAllCalls();
    STRICT_EXPECTED_CALL(clds_st_hash_set_find(IGNORED_PTR_ARG, IGNORED_PTR_ARG, IGNORED_PTR_ARG)).IgnoreAllCalls();
    STRICT_EXPECTED_CALL(test_item_cleanup_func((void*)0x4242, item_1));
    STRICT_EXPECTED_CALL(free(IGNORED_NUM_ARG));
    STRICT_EXPECTED_CALL(test_item_cleanup_func((void*)0x4242, item_2));
    STRICT_EXPECTED_CALL(free(IGNORED_NUM_ARG));
    STRICT_EXPECTED_CALL(free(IGNORED_NUM_ARG));

    // act
    clds_sorted_list_destroy(list);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

    // cleanup
    clds_hazard_pointers_destroy(hazard_pointers);
}

/* Tests_SRS_CLDS_SORTED_LIST_01_041: [ If `item_cleanup_callback` is NULL, no user callback shall be triggered for the freed items. ]*/
TEST_FUNCTION(clds_sorted_list_destroy_with_NULL_item_cleanup_callback_does_not_trigger_any_callback_for_the_freed_items)
{
    // arrange
    CLDS_HAZARD_POINTERS_HANDLE hazard_pointers = clds_hazard_pointers_create();
    CLDS_HAZARD_POINTERS_THREAD_HANDLE hazard_pointers_thread = clds_hazard_pointers_register_thread(hazard_pointers);
    CLDS_SORTED_LIST_HANDLE list;
    list = clds_sorted_list_create(hazard_pointers, test_get_item_key, (void*)0x4242, test_key_compare, (void*)0x4243);
    CLDS_SORTED_LIST_ITEM* item = CLDS_SORTED_LIST_NODE_CREATE(TEST_ITEM, NULL, NULL);
    TEST_ITEM* item_payload = CLDS_SORTED_LIST_GET_VALUE(TEST_ITEM, item);
    item_payload->key = 0x42;
    (void)clds_sorted_list_insert(list, hazard_pointers_thread, item);
    umock_c_reset_all_calls();

    STRICT_EXPECTED_CALL(free(IGNORED_NUM_ARG));
    STRICT_EXPECTED_CALL(free(IGNORED_NUM_ARG));

    // act
    clds_sorted_list_destroy(list);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

    // cleanup
    clds_hazard_pointers_destroy(hazard_pointers);
}

/* clds_sorted_list_insert */

/* Tests_SRS_CLDS_SORTED_LIST_01_009: [ `clds_sorted_list_insert` inserts an item in the list. ]*/
/* Tests_SRS_CLDS_SORTED_LIST_01_010: [ On success `clds_sorted_list_insert` shall return `CLDS_SORTED_LIST_INSERT_OK`. ]*/
TEST_FUNCTION(clds_sorted_list_insert_succeeds)
{
    // arrange
    CLDS_HAZARD_POINTERS_HANDLE hazard_pointers = clds_hazard_pointers_create();
    CLDS_HAZARD_POINTERS_THREAD_HANDLE hazard_pointers_thread = clds_hazard_pointers_register_thread(hazard_pointers);
    CLDS_SORTED_LIST_HANDLE list;
    CLDS_SORTED_LIST_INSERT_RESULT result;
    list = clds_sorted_list_create(hazard_pointers, test_get_item_key, (void*)0x4242, test_key_compare, (void*)0x4243);
    CLDS_SORTED_LIST_ITEM* item = CLDS_SORTED_LIST_NODE_CREATE(TEST_ITEM, test_item_cleanup_func, (void*)0x4242);
    TEST_ITEM* item_payload = CLDS_SORTED_LIST_GET_VALUE(TEST_ITEM, item);
    item_payload->key = 0x42;
    umock_c_reset_all_calls();

    // act
    result = clds_sorted_list_insert(list, hazard_pointers_thread, item);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
    ASSERT_ARE_EQUAL(CLDS_SORTED_LIST_INSERT_RESULT, CLDS_SORTED_LIST_INSERT_OK, result);

    // cleanup
    clds_sorted_list_destroy(list);
    clds_hazard_pointers_destroy(hazard_pointers);
}

/* Tests_SRS_CLDS_SORTED_LIST_01_011: [ If `clds_sorted_list` is NULL, `clds_sorted_list_insert` shall fail and return `CLDS_SORTED_LIST_INSERT_ERROR`. ]*/
TEST_FUNCTION(clds_sorted_list_insert_with_NULL_singly_linked_list_fails)
{
    // arrange
    CLDS_HAZARD_POINTERS_HANDLE hazard_pointers = clds_hazard_pointers_create();
    CLDS_HAZARD_POINTERS_THREAD_HANDLE hazard_pointers_thread = clds_hazard_pointers_register_thread(hazard_pointers);
    CLDS_SORTED_LIST_INSERT_RESULT result;
    CLDS_SORTED_LIST_ITEM* item = CLDS_SORTED_LIST_NODE_CREATE(TEST_ITEM, test_item_cleanup_func, (void*)0x4242);
    TEST_ITEM* item_payload = CLDS_SORTED_LIST_GET_VALUE(TEST_ITEM, item);
    item_payload->key = 0x42;
    umock_c_reset_all_calls();

    // act
    result = clds_sorted_list_insert(NULL, hazard_pointers_thread, item);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
    ASSERT_ARE_EQUAL(CLDS_SORTED_LIST_INSERT_RESULT, CLDS_SORTED_LIST_INSERT_ERROR, result);

    // cleanup
    clds_hazard_pointers_destroy(hazard_pointers);
}

/* Tests_SRS_CLDS_SORTED_LIST_01_012: [ If `item` is NULL, `clds_sorted_list_insert` shall fail and return `CLDS_SORTED_LIST_INSERT_ERROR`. ]*/
TEST_FUNCTION(clds_sorted_list_insert_with_NULL_item_fails)
{
    // arrange
    CLDS_HAZARD_POINTERS_HANDLE hazard_pointers = clds_hazard_pointers_create();
    CLDS_HAZARD_POINTERS_THREAD_HANDLE hazard_pointers_thread = clds_hazard_pointers_register_thread(hazard_pointers);
    CLDS_SORTED_LIST_HANDLE list;
    CLDS_SORTED_LIST_INSERT_RESULT result;
    list = clds_sorted_list_create(hazard_pointers, test_get_item_key, (void*)0x4242, test_key_compare, (void*)0x4243);
    umock_c_reset_all_calls();

    // act
    result = clds_sorted_list_insert(list, hazard_pointers_thread, NULL);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
    ASSERT_ARE_EQUAL(CLDS_SORTED_LIST_INSERT_RESULT, CLDS_SORTED_LIST_INSERT_ERROR, result);

    // cleanup
    clds_sorted_list_destroy(list);
    clds_hazard_pointers_destroy(hazard_pointers);
}

/* Tests_SRS_CLDS_SORTED_LIST_01_013: [ If `clds_hazard_pointers_thread` is NULL, `clds_sorted_list_insert` shall fail and return `CLDS_SORTED_LIST_INSERT_ERROR`. ]*/
TEST_FUNCTION(clds_sorted_list_insert_with_NULL_hazard_pointers_thread_handle_fails)
{
    // arrange
    CLDS_HAZARD_POINTERS_HANDLE hazard_pointers = clds_hazard_pointers_create();
    CLDS_SORTED_LIST_HANDLE list;
    CLDS_SORTED_LIST_INSERT_RESULT result;
    list = clds_sorted_list_create(hazard_pointers, test_get_item_key, (void*)0x4242, test_key_compare, (void*)0x4243);
    CLDS_SORTED_LIST_ITEM* item = CLDS_SORTED_LIST_NODE_CREATE(TEST_ITEM, test_item_cleanup_func, (void*)0x4242);
    TEST_ITEM* item_payload = CLDS_SORTED_LIST_GET_VALUE(TEST_ITEM, item);
    item_payload->key = 0x42;
    umock_c_reset_all_calls();

    // act
    result = clds_sorted_list_insert(list, NULL, item);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
    ASSERT_ARE_EQUAL(CLDS_SORTED_LIST_INSERT_RESULT, CLDS_SORTED_LIST_INSERT_ERROR, result);

    // cleanup
    clds_sorted_list_destroy(list);
    clds_hazard_pointers_destroy(hazard_pointers);
}

/* Tests_SRS_CLDS_SORTED_LIST_01_009: [ `clds_sorted_list_insert` inserts an item in the list. ]*/
/* Tests_SRS_CLDS_SORTED_LIST_01_010: [ On success `clds_sorted_list_insert` shall return `CLDS_SORTED_LIST_INSERT_OK`. ]*/
/* Tests_SRS_CLDS_SORTED_LIST_01_047: [ `clds_sorted_list_insert` shall insert the item at its correct location making sure that items in the list are sorted according to the order given by item keys. ]*/
TEST_FUNCTION(clds_sorted_list_insert_2_items_in_order_succeeds)
{
    // arrange
    CLDS_HAZARD_POINTERS_HANDLE hazard_pointers = clds_hazard_pointers_create();
    CLDS_HAZARD_POINTERS_THREAD_HANDLE hazard_pointers_thread = clds_hazard_pointers_register_thread(hazard_pointers);
    CLDS_SORTED_LIST_HANDLE list;
    CLDS_SORTED_LIST_INSERT_RESULT result_1;
    CLDS_SORTED_LIST_INSERT_RESULT result_2;
    list = clds_sorted_list_create(hazard_pointers, test_get_item_key, (void*)0x4242, test_key_compare, (void*)0x4243);
    CLDS_SORTED_LIST_ITEM* item_1 = CLDS_SORTED_LIST_NODE_CREATE(TEST_ITEM, test_item_cleanup_func, (void*)0x4242);
    CLDS_SORTED_LIST_ITEM* item_2 = CLDS_SORTED_LIST_NODE_CREATE(TEST_ITEM, test_item_cleanup_func, (void*)0x4242);
    TEST_ITEM* item_1_payload = CLDS_SORTED_LIST_GET_VALUE(TEST_ITEM, item_1);
    TEST_ITEM* item_2_payload = CLDS_SORTED_LIST_GET_VALUE(TEST_ITEM, item_2);
    item_1_payload->key = 0x42;
    item_2_payload->key = 0x43;
    umock_c_reset_all_calls();

    STRICT_EXPECTED_CALL(clds_hazard_pointers_acquire(IGNORED_PTR_ARG, IGNORED_PTR_ARG)).IgnoreAllCalls();
    STRICT_EXPECTED_CALL(clds_hazard_pointers_release(IGNORED_PTR_ARG, IGNORED_PTR_ARG)).IgnoreAllCalls();

    // act
    result_1 = clds_sorted_list_insert(list, hazard_pointers_thread, item_1);
    result_2 = clds_sorted_list_insert(list, hazard_pointers_thread, item_2);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
    ASSERT_ARE_EQUAL(CLDS_SORTED_LIST_INSERT_RESULT, CLDS_SORTED_LIST_INSERT_OK, result_1);
    ASSERT_ARE_EQUAL(CLDS_SORTED_LIST_INSERT_RESULT, CLDS_SORTED_LIST_INSERT_OK, result_2);

    // cleanup
    clds_sorted_list_destroy(list);
    clds_hazard_pointers_destroy(hazard_pointers);
}

/* Tests_SRS_CLDS_SORTED_LIST_01_009: [ `clds_sorted_list_insert` inserts an item in the list. ]*/
/* Tests_SRS_CLDS_SORTED_LIST_01_010: [ On success `clds_sorted_list_insert` shall return `CLDS_SORTED_LIST_INSERT_OK`. ]*/
/* Tests_SRS_CLDS_SORTED_LIST_01_047: [ `clds_sorted_list_insert` shall insert the item at its correct location making sure that items in the list are sorted according to the order given by item keys. ]*/
TEST_FUNCTION(clds_sorted_list_insert_2_items_in_reverse_order_succeeds)
{
    // arrange
    CLDS_HAZARD_POINTERS_HANDLE hazard_pointers = clds_hazard_pointers_create();
    CLDS_HAZARD_POINTERS_THREAD_HANDLE hazard_pointers_thread = clds_hazard_pointers_register_thread(hazard_pointers);
    CLDS_SORTED_LIST_HANDLE list;
    CLDS_SORTED_LIST_INSERT_RESULT result_1;
    CLDS_SORTED_LIST_INSERT_RESULT result_2;
    list = clds_sorted_list_create(hazard_pointers, test_get_item_key, (void*)0x4242, test_key_compare, (void*)0x4243);
    CLDS_SORTED_LIST_ITEM* item_1 = CLDS_SORTED_LIST_NODE_CREATE(TEST_ITEM, test_item_cleanup_func, (void*)0x4242);
    CLDS_SORTED_LIST_ITEM* item_2 = CLDS_SORTED_LIST_NODE_CREATE(TEST_ITEM, test_item_cleanup_func, (void*)0x4242);
    TEST_ITEM* item_1_payload = CLDS_SORTED_LIST_GET_VALUE(TEST_ITEM, item_1);
    TEST_ITEM* item_2_payload = CLDS_SORTED_LIST_GET_VALUE(TEST_ITEM, item_2);
    item_1_payload->key = 0x43;
    item_2_payload->key = 0x42;
    umock_c_reset_all_calls();

    STRICT_EXPECTED_CALL(clds_hazard_pointers_acquire(IGNORED_PTR_ARG, IGNORED_PTR_ARG)).IgnoreAllCalls();
    STRICT_EXPECTED_CALL(clds_hazard_pointers_release(IGNORED_PTR_ARG, IGNORED_PTR_ARG)).IgnoreAllCalls();

    // act
    result_1 = clds_sorted_list_insert(list, hazard_pointers_thread, item_1);
    result_2 = clds_sorted_list_insert(list, hazard_pointers_thread, item_2);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
    ASSERT_ARE_EQUAL(CLDS_SORTED_LIST_INSERT_RESULT, CLDS_SORTED_LIST_INSERT_OK, result_1);
    ASSERT_ARE_EQUAL(CLDS_SORTED_LIST_INSERT_RESULT, CLDS_SORTED_LIST_INSERT_OK, result_2);

    // cleanup
    clds_sorted_list_destroy(list);
    clds_hazard_pointers_destroy(hazard_pointers);
}

/* Tests_SRS_CLDS_SORTED_LIST_01_048: [ If the item with the given key already exists in the list, `clds_sorted_list_insert` shall fail and return `CLDS_SORTED_LIST_INSERT_KEY_ALREADY_EXISTS`. ]*/
TEST_FUNCTION(clds_sorted_list_insert_for_a_key_that_already_exists_fails)
{
    // arrange
    CLDS_HAZARD_POINTERS_HANDLE hazard_pointers = clds_hazard_pointers_create();
    CLDS_HAZARD_POINTERS_THREAD_HANDLE hazard_pointers_thread = clds_hazard_pointers_register_thread(hazard_pointers);
    CLDS_SORTED_LIST_HANDLE list;
    CLDS_SORTED_LIST_INSERT_RESULT result;
    list = clds_sorted_list_create(hazard_pointers, test_get_item_key, (void*)0x4242, test_key_compare, (void*)0x4243);
    CLDS_SORTED_LIST_ITEM* item_1 = CLDS_SORTED_LIST_NODE_CREATE(TEST_ITEM, test_item_cleanup_func, (void*)0x4242);
    CLDS_SORTED_LIST_ITEM* item_2 = CLDS_SORTED_LIST_NODE_CREATE(TEST_ITEM, test_item_cleanup_func, (void*)0x4242);
    TEST_ITEM* item_1_payload = CLDS_SORTED_LIST_GET_VALUE(TEST_ITEM, item_1);
    TEST_ITEM* item_2_payload = CLDS_SORTED_LIST_GET_VALUE(TEST_ITEM, item_2);
    item_1_payload->key = 0x42;
    item_2_payload->key = 0x42;
    (void)clds_sorted_list_insert(list, hazard_pointers_thread, item_1);
    umock_c_reset_all_calls();

    STRICT_EXPECTED_CALL(clds_hazard_pointers_acquire(IGNORED_PTR_ARG, IGNORED_PTR_ARG)).IgnoreAllCalls();
    STRICT_EXPECTED_CALL(clds_hazard_pointers_release(IGNORED_PTR_ARG, IGNORED_PTR_ARG)).IgnoreAllCalls();

    // act
    result = clds_sorted_list_insert(list, hazard_pointers_thread, item_2);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
    ASSERT_ARE_EQUAL(CLDS_SORTED_LIST_INSERT_RESULT, CLDS_SORTED_LIST_INSERT_KEY_ALREADY_EXISTS, result);

    // cleanup
    CLDS_SORTED_LIST_NODE_RELEASE(TEST_ITEM, item_2);
    clds_sorted_list_destroy(list);
    clds_hazard_pointers_destroy(hazard_pointers);
}

/* clds_sorted_list_delete_item */

/* Tests_SRS_CLDS_SORTED_LIST_01_014: [ `clds_sorted_list_delete_item` shall delete an item from the list by its pointer. ]*/
/* Tests_SRS_CLDS_SORTED_LIST_01_026: [ On success, `clds_sorted_list_delete_item` shall return `CLDS_SORTED_LIST_DELETE_OK`. ]*/
/* Tests_SRS_CLDS_SORTED_LIST_01_042: [ When an item is deleted it shall be indicated to the hazard pointers instance as reclaimed by calling `clds_hazard_pointers_reclaim`. ]*/
/* Tests_SRS_CLDS_SORTED_LIST_01_043: [ The reclaim function passed to `clds_hazard_pointers_reclaim` shall call the user callback `item_cleanup_callback` that was passed to `clds_sorted_list_node_create`, while passing `item_cleanup_callback_context` and the freed item as arguments. ]*/
TEST_FUNCTION(clds_sorted_list_delete_item_deletes_the_item)
{
    // arrange
    CLDS_HAZARD_POINTERS_HANDLE hazard_pointers = clds_hazard_pointers_create();
    CLDS_HAZARD_POINTERS_THREAD_HANDLE hazard_pointers_thread = clds_hazard_pointers_register_thread(hazard_pointers);
    CLDS_SORTED_LIST_HANDLE list = clds_sorted_list_create(hazard_pointers, test_get_item_key, (void*)0x4242, test_key_compare, (void*)0x4243);
    CLDS_SORTED_LIST_ITEM* item = CLDS_SORTED_LIST_NODE_CREATE(TEST_ITEM, test_item_cleanup_func, (void*)0x4242);
    CLDS_SORTED_LIST_DELETE_RESULT result;
    TEST_ITEM* item_payload = CLDS_SORTED_LIST_GET_VALUE(TEST_ITEM, item);
    item_payload->key = 0x42;
    (void)clds_hazard_pointers_set_reclaim_threshold(hazard_pointers, 1);
    (void)clds_sorted_list_insert(list, hazard_pointers_thread, item);
    umock_c_reset_all_calls();

    STRICT_EXPECTED_CALL(clds_hazard_pointers_acquire(IGNORED_PTR_ARG, IGNORED_PTR_ARG)).IgnoreAllCalls();
    STRICT_EXPECTED_CALL(clds_hazard_pointers_release(IGNORED_PTR_ARG, IGNORED_PTR_ARG)).IgnoreAllCalls();
    STRICT_EXPECTED_CALL(clds_st_hash_set_create(IGNORED_PTR_ARG, IGNORED_NUM_ARG)).IgnoreAllCalls();
    STRICT_EXPECTED_CALL(clds_st_hash_set_destroy(IGNORED_PTR_ARG)).IgnoreAllCalls();
    STRICT_EXPECTED_CALL(clds_st_hash_set_find(IGNORED_PTR_ARG, IGNORED_PTR_ARG, IGNORED_PTR_ARG)).IgnoreAllCalls();
    STRICT_EXPECTED_CALL(clds_hazard_pointers_reclaim(hazard_pointers_thread, item, IGNORED_PTR_ARG));
    STRICT_EXPECTED_CALL(test_item_cleanup_func((void*)0x4242, item));
    STRICT_EXPECTED_CALL(free(item));

    // act
    result = clds_sorted_list_delete_item(list, hazard_pointers_thread, item);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
    ASSERT_ARE_EQUAL(CLDS_SORTED_LIST_DELETE_RESULT, CLDS_SORTED_LIST_DELETE_OK, result);

    // cleanup
    clds_sorted_list_destroy(list);
    clds_hazard_pointers_destroy(hazard_pointers);
}

/* Tests_SRS_CLDS_SORTED_LIST_01_014: [ `clds_sorted_list_delete_item` shall delete an item from the list by its pointer. ]*/
/* Tests_SRS_CLDS_SORTED_LIST_01_026: [ On success, `clds_sorted_list_delete_item` shall return `CLDS_SORTED_LIST_DELETE_OK`. ]*/
/* Tests_SRS_CLDS_SORTED_LIST_01_042: [ When an item is deleted it shall be indicated to the hazard pointers instance as reclaimed by calling `clds_hazard_pointers_reclaim`. ]*/
/* Tests_SRS_CLDS_SORTED_LIST_01_043: [ The reclaim function passed to `clds_hazard_pointers_reclaim` shall call the user callback `item_cleanup_callback` that was passed to `clds_sorted_list_node_create`, while passing `item_cleanup_callback_context` and the freed item as arguments. ]*/
TEST_FUNCTION(clds_sorted_list_delete_for_the_2nd_item_succeeds)
{
    // arrange
    CLDS_HAZARD_POINTERS_HANDLE hazard_pointers = clds_hazard_pointers_create();
    CLDS_HAZARD_POINTERS_THREAD_HANDLE hazard_pointers_thread = clds_hazard_pointers_register_thread(hazard_pointers);
    CLDS_SORTED_LIST_HANDLE list = clds_sorted_list_create(hazard_pointers, test_get_item_key, (void*)0x4242, test_key_compare, (void*)0x4243);
    CLDS_SORTED_LIST_ITEM* item_1 = CLDS_SORTED_LIST_NODE_CREATE(TEST_ITEM, test_item_cleanup_func, (void*)0x4242);
    CLDS_SORTED_LIST_ITEM* item_2 = CLDS_SORTED_LIST_NODE_CREATE(TEST_ITEM, test_item_cleanup_func, (void*)0x4242);
    TEST_ITEM* item_1_payload = CLDS_SORTED_LIST_GET_VALUE(TEST_ITEM, item_1);
    TEST_ITEM* item_2_payload = CLDS_SORTED_LIST_GET_VALUE(TEST_ITEM, item_2);
    item_1_payload->key = 0x42;
    item_2_payload->key = 0x43;
    CLDS_SORTED_LIST_DELETE_RESULT result;
    (void)clds_hazard_pointers_set_reclaim_threshold(hazard_pointers, 1);
    (void)clds_sorted_list_insert(list, hazard_pointers_thread, item_1);
    (void)clds_sorted_list_insert(list, hazard_pointers_thread, item_2);
    umock_c_reset_all_calls();

    STRICT_EXPECTED_CALL(clds_hazard_pointers_acquire(IGNORED_PTR_ARG, IGNORED_PTR_ARG)).IgnoreAllCalls();
    STRICT_EXPECTED_CALL(clds_hazard_pointers_release(IGNORED_PTR_ARG, IGNORED_PTR_ARG)).IgnoreAllCalls();
    STRICT_EXPECTED_CALL(clds_st_hash_set_create(IGNORED_PTR_ARG, IGNORED_NUM_ARG)).IgnoreAllCalls();
    STRICT_EXPECTED_CALL(clds_st_hash_set_insert(IGNORED_PTR_ARG, IGNORED_PTR_ARG)).IgnoreAllCalls();
    STRICT_EXPECTED_CALL(clds_st_hash_set_destroy(IGNORED_PTR_ARG)).IgnoreAllCalls();
    STRICT_EXPECTED_CALL(clds_st_hash_set_find(IGNORED_PTR_ARG, IGNORED_PTR_ARG, IGNORED_PTR_ARG)).IgnoreAllCalls();
    STRICT_EXPECTED_CALL(clds_hazard_pointers_reclaim(hazard_pointers_thread, item_2, IGNORED_PTR_ARG));
    STRICT_EXPECTED_CALL(test_item_cleanup_func((void*)0x4242, item_2));
    STRICT_EXPECTED_CALL(free(item_2));

    // act
    result = clds_sorted_list_delete_item(list, hazard_pointers_thread, item_2);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
    ASSERT_ARE_EQUAL(CLDS_SORTED_LIST_DELETE_RESULT, CLDS_SORTED_LIST_DELETE_OK, result);

    // cleanup
    clds_sorted_list_destroy(list);
    clds_hazard_pointers_destroy(hazard_pointers);
}

/* Tests_SRS_CLDS_SORTED_LIST_01_014: [ `clds_sorted_list_delete_item` shall delete an item from the list by its pointer. ]*/
/* Tests_SRS_CLDS_SORTED_LIST_01_026: [ On success, `clds_sorted_list_delete_item` shall return `CLDS_SORTED_LIST_DELETE_OK`. ]*/
/* Tests_SRS_CLDS_SORTED_LIST_01_042: [ When an item is deleted it shall be indicated to the hazard pointers instance as reclaimed by calling `clds_hazard_pointers_reclaim`. ]*/
/* Tests_SRS_CLDS_SORTED_LIST_01_043: [ The reclaim function passed to `clds_hazard_pointers_reclaim` shall call the user callback `item_cleanup_callback` that was passed to `clds_sorted_list_node_create`, while passing `item_cleanup_callback_context` and the freed item as arguments. ]*/
TEST_FUNCTION(clds_sorted_list_delete_for_the_oldest_out_of_2_items_succeeds)
{
    // arrange
    CLDS_HAZARD_POINTERS_HANDLE hazard_pointers = clds_hazard_pointers_create();
    CLDS_HAZARD_POINTERS_THREAD_HANDLE hazard_pointers_thread = clds_hazard_pointers_register_thread(hazard_pointers);
    CLDS_SORTED_LIST_HANDLE list = clds_sorted_list_create(hazard_pointers, test_get_item_key, (void*)0x4242, test_key_compare, (void*)0x4243);
    CLDS_SORTED_LIST_ITEM* item_1 = CLDS_SORTED_LIST_NODE_CREATE(TEST_ITEM, test_item_cleanup_func, (void*)0x4242);
    CLDS_SORTED_LIST_ITEM* item_2 = CLDS_SORTED_LIST_NODE_CREATE(TEST_ITEM, test_item_cleanup_func, (void*)0x4242);
    CLDS_SORTED_LIST_DELETE_RESULT result;
    TEST_ITEM* item_1_payload = CLDS_SORTED_LIST_GET_VALUE(TEST_ITEM, item_1);
    TEST_ITEM* item_2_payload = CLDS_SORTED_LIST_GET_VALUE(TEST_ITEM, item_2);
    item_1_payload->key = 0x42;
    item_2_payload->key = 0x43;
    (void)clds_hazard_pointers_set_reclaim_threshold(hazard_pointers, 1);
    (void)clds_sorted_list_insert(list, hazard_pointers_thread, item_1);
    (void)clds_sorted_list_insert(list, hazard_pointers_thread, item_2);
    umock_c_reset_all_calls();

    STRICT_EXPECTED_CALL(clds_hazard_pointers_acquire(IGNORED_PTR_ARG, IGNORED_PTR_ARG)).IgnoreAllCalls();
    STRICT_EXPECTED_CALL(clds_hazard_pointers_release(IGNORED_PTR_ARG, IGNORED_PTR_ARG)).IgnoreAllCalls();
    STRICT_EXPECTED_CALL(clds_st_hash_set_create(IGNORED_PTR_ARG, IGNORED_NUM_ARG)).IgnoreAllCalls();
    STRICT_EXPECTED_CALL(clds_st_hash_set_insert(IGNORED_PTR_ARG, IGNORED_PTR_ARG)).IgnoreAllCalls();
    STRICT_EXPECTED_CALL(clds_st_hash_set_destroy(IGNORED_PTR_ARG)).IgnoreAllCalls();
    STRICT_EXPECTED_CALL(clds_st_hash_set_find(IGNORED_PTR_ARG, IGNORED_PTR_ARG, IGNORED_PTR_ARG)).IgnoreAllCalls();
    STRICT_EXPECTED_CALL(clds_hazard_pointers_reclaim(hazard_pointers_thread, item_1, IGNORED_PTR_ARG));
    STRICT_EXPECTED_CALL(test_item_cleanup_func((void*)0x4242, item_1));
    STRICT_EXPECTED_CALL(free(item_1));

    // act
    result = clds_sorted_list_delete_item(list, hazard_pointers_thread, item_1);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
    ASSERT_ARE_EQUAL(CLDS_SORTED_LIST_DELETE_RESULT, CLDS_SORTED_LIST_DELETE_OK, result);

    // cleanup
    clds_sorted_list_destroy(list);
    clds_hazard_pointers_destroy(hazard_pointers);
}

/* Tests_SRS_CLDS_SORTED_LIST_01_014: [ `clds_sorted_list_delete_item` shall delete an item from the list by its pointer. ]*/
/* Tests_SRS_CLDS_SORTED_LIST_01_026: [ On success, `clds_sorted_list_delete_item` shall return `CLDS_SORTED_LIST_DELETE_OK`. ]*/
/* Tests_SRS_CLDS_SORTED_LIST_01_042: [ When an item is deleted it shall be indicated to the hazard pointers instance as reclaimed by calling `clds_hazard_pointers_reclaim`. ]*/
/* Tests_SRS_CLDS_SORTED_LIST_01_043: [ The reclaim function passed to `clds_hazard_pointers_reclaim` shall call the user callback `item_cleanup_callback` that was passed to `clds_sorted_list_node_create`, while passing `item_cleanup_callback_context` and the freed item as arguments. ]*/
TEST_FUNCTION(clds_sorted_list_delete_for_the_2nd_out_of_3_items_succeeds)
{
    // arrange
    CLDS_HAZARD_POINTERS_HANDLE hazard_pointers = clds_hazard_pointers_create();
    CLDS_HAZARD_POINTERS_THREAD_HANDLE hazard_pointers_thread = clds_hazard_pointers_register_thread(hazard_pointers);
    CLDS_SORTED_LIST_HANDLE list = clds_sorted_list_create(hazard_pointers, test_get_item_key, (void*)0x4242, test_key_compare, (void*)0x4243);
    CLDS_SORTED_LIST_ITEM* item_1 = CLDS_SORTED_LIST_NODE_CREATE(TEST_ITEM, test_item_cleanup_func, (void*)0x4242);
    CLDS_SORTED_LIST_ITEM* item_2 = CLDS_SORTED_LIST_NODE_CREATE(TEST_ITEM, test_item_cleanup_func, (void*)0x4242);
    CLDS_SORTED_LIST_ITEM* item_3 = CLDS_SORTED_LIST_NODE_CREATE(TEST_ITEM, test_item_cleanup_func, (void*)0x4242);
    TEST_ITEM* item_1_payload = CLDS_SORTED_LIST_GET_VALUE(TEST_ITEM, item_1);
    TEST_ITEM* item_2_payload = CLDS_SORTED_LIST_GET_VALUE(TEST_ITEM, item_2);
    TEST_ITEM* item_3_payload = CLDS_SORTED_LIST_GET_VALUE(TEST_ITEM, item_3);
    item_1_payload->key = 0x42;
    item_2_payload->key = 0x43;
    item_3_payload->key = 0x44;
    CLDS_SORTED_LIST_DELETE_RESULT result;
    (void)clds_hazard_pointers_set_reclaim_threshold(hazard_pointers, 1);
    (void)clds_sorted_list_insert(list, hazard_pointers_thread, item_1);
    (void)clds_sorted_list_insert(list, hazard_pointers_thread, item_2);
    (void)clds_sorted_list_insert(list, hazard_pointers_thread, item_3);
    umock_c_reset_all_calls();

    STRICT_EXPECTED_CALL(clds_hazard_pointers_acquire(IGNORED_PTR_ARG, IGNORED_PTR_ARG)).IgnoreAllCalls();
    STRICT_EXPECTED_CALL(clds_hazard_pointers_release(IGNORED_PTR_ARG, IGNORED_PTR_ARG)).IgnoreAllCalls();
    STRICT_EXPECTED_CALL(clds_st_hash_set_create(IGNORED_PTR_ARG, IGNORED_NUM_ARG)).IgnoreAllCalls();
    STRICT_EXPECTED_CALL(clds_st_hash_set_destroy(IGNORED_PTR_ARG)).IgnoreAllCalls();
    STRICT_EXPECTED_CALL(clds_st_hash_set_find(IGNORED_PTR_ARG, IGNORED_PTR_ARG, IGNORED_PTR_ARG)).IgnoreAllCalls();
    STRICT_EXPECTED_CALL(clds_hazard_pointers_reclaim(hazard_pointers_thread, item_2, IGNORED_PTR_ARG));
    STRICT_EXPECTED_CALL(test_item_cleanup_func((void*)0x4242, item_2));
    STRICT_EXPECTED_CALL(free(item_2));

    // act
    result = clds_sorted_list_delete_item(list, hazard_pointers_thread, item_2);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
    ASSERT_ARE_EQUAL(CLDS_SORTED_LIST_DELETE_RESULT, CLDS_SORTED_LIST_DELETE_OK, result);

    // cleanup
    clds_sorted_list_destroy(list);
    clds_hazard_pointers_destroy(hazard_pointers);
}

/* Tests_SRS_CLDS_SORTED_LIST_01_014: [ `clds_sorted_list_delete_item` shall delete an item from the list by its pointer. ]*/
/* Tests_SRS_CLDS_SORTED_LIST_01_026: [ On success, `clds_sorted_list_delete_item` shall return `CLDS_SORTED_LIST_DELETE_OK`. ]*/
/* Tests_SRS_CLDS_SORTED_LIST_01_042: [ When an item is deleted it shall be indicated to the hazard pointers instance as reclaimed by calling `clds_hazard_pointers_reclaim`. ]*/
/* Tests_SRS_CLDS_SORTED_LIST_01_044: [ If `item_cleanup_callback` is NULL, no user callback shall be triggered for the reclaimed item. ]*/
TEST_FUNCTION(clds_sorted_list_delete_deletes_the_item_NULL_cleanup_callback)
{
    // arrange
    CLDS_HAZARD_POINTERS_HANDLE hazard_pointers = clds_hazard_pointers_create();
    CLDS_HAZARD_POINTERS_THREAD_HANDLE hazard_pointers_thread = clds_hazard_pointers_register_thread(hazard_pointers);
    CLDS_SORTED_LIST_HANDLE list = clds_sorted_list_create(hazard_pointers, test_get_item_key, (void*)0x4242, test_key_compare, (void*)0x4243);
    CLDS_SORTED_LIST_ITEM* item = CLDS_SORTED_LIST_NODE_CREATE(TEST_ITEM, NULL, NULL);
    CLDS_SORTED_LIST_DELETE_RESULT result;
    TEST_ITEM* item_payload = CLDS_SORTED_LIST_GET_VALUE(TEST_ITEM, item);
    item_payload->key = 0x42;
    (void)clds_hazard_pointers_set_reclaim_threshold(hazard_pointers, 1);
    (void)clds_sorted_list_insert(list, hazard_pointers_thread, item);
    umock_c_reset_all_calls();

    STRICT_EXPECTED_CALL(clds_hazard_pointers_acquire(IGNORED_PTR_ARG, IGNORED_PTR_ARG)).IgnoreAllCalls();
    STRICT_EXPECTED_CALL(clds_hazard_pointers_release(IGNORED_PTR_ARG, IGNORED_PTR_ARG)).IgnoreAllCalls();
    STRICT_EXPECTED_CALL(clds_st_hash_set_create(IGNORED_PTR_ARG, IGNORED_NUM_ARG)).IgnoreAllCalls();
    STRICT_EXPECTED_CALL(clds_st_hash_set_destroy(IGNORED_PTR_ARG)).IgnoreAllCalls();
    STRICT_EXPECTED_CALL(clds_st_hash_set_find(IGNORED_PTR_ARG, IGNORED_PTR_ARG, IGNORED_PTR_ARG)).IgnoreAllCalls();
    STRICT_EXPECTED_CALL(clds_hazard_pointers_reclaim(hazard_pointers_thread, item, IGNORED_PTR_ARG));
    STRICT_EXPECTED_CALL(free(item));

    // act
    result = clds_sorted_list_delete_item(list, hazard_pointers_thread, item);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
    ASSERT_ARE_EQUAL(CLDS_SORTED_LIST_DELETE_RESULT, CLDS_SORTED_LIST_DELETE_OK, result);

    // cleanup
    clds_sorted_list_destroy(list);
    clds_hazard_pointers_destroy(hazard_pointers);
}

/* Tests_SRS_CLDS_SORTED_LIST_01_014: [ `clds_sorted_list_delete_item` shall delete an item from the list by its pointer. ]*/
/* Tests_SRS_CLDS_SORTED_LIST_01_026: [ On success, `clds_sorted_list_delete_item` shall return `CLDS_SORTED_LIST_DELETE_OK`. ]*/
/* Tests_SRS_CLDS_SORTED_LIST_01_042: [ When an item is deleted it shall be indicated to the hazard pointers instance as reclaimed by calling `clds_hazard_pointers_reclaim`. ]*/
/* Tests_SRS_CLDS_SORTED_LIST_01_044: [ If `item_cleanup_callback` is NULL, no user callback shall be triggered for the reclaimed item. ]*/
TEST_FUNCTION(clds_sorted_list_delete_for_the_2nd_item_succeeds_NULL_cleanup_callback)
{
    // arrange
    CLDS_HAZARD_POINTERS_HANDLE hazard_pointers = clds_hazard_pointers_create();
    CLDS_HAZARD_POINTERS_THREAD_HANDLE hazard_pointers_thread = clds_hazard_pointers_register_thread(hazard_pointers);
    CLDS_SORTED_LIST_HANDLE list = clds_sorted_list_create(hazard_pointers, test_get_item_key, (void*)0x4242, test_key_compare, (void*)0x4243);
    CLDS_SORTED_LIST_ITEM* item_1 = CLDS_SORTED_LIST_NODE_CREATE(TEST_ITEM, NULL, NULL);
    CLDS_SORTED_LIST_ITEM* item_2 = CLDS_SORTED_LIST_NODE_CREATE(TEST_ITEM, NULL, NULL);
    CLDS_SORTED_LIST_DELETE_RESULT result;
    TEST_ITEM* item_1_payload = CLDS_SORTED_LIST_GET_VALUE(TEST_ITEM, item_1);
    TEST_ITEM* item_2_payload = CLDS_SORTED_LIST_GET_VALUE(TEST_ITEM, item_2);
    item_1_payload->key = 0x42;
    item_2_payload->key = 0x43;
    (void)clds_hazard_pointers_set_reclaim_threshold(hazard_pointers, 1);
    (void)clds_sorted_list_insert(list, hazard_pointers_thread, item_1);
    (void)clds_sorted_list_insert(list, hazard_pointers_thread, item_2);
    umock_c_reset_all_calls();

    STRICT_EXPECTED_CALL(clds_hazard_pointers_acquire(IGNORED_PTR_ARG, IGNORED_PTR_ARG)).IgnoreAllCalls();
    STRICT_EXPECTED_CALL(clds_hazard_pointers_release(IGNORED_PTR_ARG, IGNORED_PTR_ARG)).IgnoreAllCalls();
    STRICT_EXPECTED_CALL(clds_st_hash_set_create(IGNORED_PTR_ARG, IGNORED_NUM_ARG)).IgnoreAllCalls();
    STRICT_EXPECTED_CALL(clds_st_hash_set_destroy(IGNORED_PTR_ARG)).IgnoreAllCalls();
    STRICT_EXPECTED_CALL(clds_st_hash_set_find(IGNORED_PTR_ARG, IGNORED_PTR_ARG, IGNORED_PTR_ARG)).IgnoreAllCalls();
    STRICT_EXPECTED_CALL(clds_hazard_pointers_reclaim(hazard_pointers_thread, item_2, IGNORED_PTR_ARG));
    STRICT_EXPECTED_CALL(free(item_2));

    // act
    result = clds_sorted_list_delete_item(list, hazard_pointers_thread, item_2);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
    ASSERT_ARE_EQUAL(CLDS_SORTED_LIST_DELETE_RESULT, CLDS_SORTED_LIST_DELETE_OK, result);

    // cleanup
    clds_sorted_list_destroy(list);
    clds_hazard_pointers_destroy(hazard_pointers);
}

/* Tests_SRS_CLDS_SORTED_LIST_01_014: [ `clds_sorted_list_delete_item` shall delete an item from the list by its pointer. ]*/
/* Tests_SRS_CLDS_SORTED_LIST_01_026: [ On success, `clds_sorted_list_delete_item` shall return `CLDS_SORTED_LIST_DELETE_OK`. ]*/
/* Tests_SRS_CLDS_SORTED_LIST_01_042: [ When an item is deleted it shall be indicated to the hazard pointers instance as reclaimed by calling `clds_hazard_pointers_reclaim`. ]*/
/* Tests_SRS_CLDS_SORTED_LIST_01_044: [ If `item_cleanup_callback` is NULL, no user callback shall be triggered for the reclaimed item. ]*/
TEST_FUNCTION(clds_sorted_list_delete_for_the_oldest_out_of_2_items_succeeds_NULL_cleanup_callback)
{
    // arrange
    CLDS_HAZARD_POINTERS_HANDLE hazard_pointers = clds_hazard_pointers_create();
    CLDS_HAZARD_POINTERS_THREAD_HANDLE hazard_pointers_thread = clds_hazard_pointers_register_thread(hazard_pointers);
    CLDS_SORTED_LIST_HANDLE list = clds_sorted_list_create(hazard_pointers, test_get_item_key, (void*)0x4242, test_key_compare, (void*)0x4243);
    CLDS_SORTED_LIST_ITEM* item_1 = CLDS_SORTED_LIST_NODE_CREATE(TEST_ITEM, NULL, NULL);
    CLDS_SORTED_LIST_ITEM* item_2 = CLDS_SORTED_LIST_NODE_CREATE(TEST_ITEM, NULL, NULL);
    CLDS_SORTED_LIST_DELETE_RESULT result;
    TEST_ITEM* item_1_payload = CLDS_SORTED_LIST_GET_VALUE(TEST_ITEM, item_1);
    TEST_ITEM* item_2_payload = CLDS_SORTED_LIST_GET_VALUE(TEST_ITEM, item_2);
    item_1_payload->key = 0x42;
    item_2_payload->key = 0x43;
    (void)clds_hazard_pointers_set_reclaim_threshold(hazard_pointers, 1);
    (void)clds_sorted_list_insert(list, hazard_pointers_thread, item_1);
    (void)clds_sorted_list_insert(list, hazard_pointers_thread, item_2);
    umock_c_reset_all_calls();

    STRICT_EXPECTED_CALL(clds_hazard_pointers_acquire(IGNORED_PTR_ARG, IGNORED_PTR_ARG)).IgnoreAllCalls();
    STRICT_EXPECTED_CALL(clds_hazard_pointers_release(IGNORED_PTR_ARG, IGNORED_PTR_ARG)).IgnoreAllCalls();
    STRICT_EXPECTED_CALL(clds_st_hash_set_create(IGNORED_PTR_ARG, IGNORED_NUM_ARG)).IgnoreAllCalls();
    STRICT_EXPECTED_CALL(clds_st_hash_set_destroy(IGNORED_PTR_ARG)).IgnoreAllCalls();
    STRICT_EXPECTED_CALL(clds_st_hash_set_find(IGNORED_PTR_ARG, IGNORED_PTR_ARG, IGNORED_PTR_ARG)).IgnoreAllCalls();
    STRICT_EXPECTED_CALL(clds_hazard_pointers_reclaim(hazard_pointers_thread, item_1, IGNORED_PTR_ARG));
    STRICT_EXPECTED_CALL(free(item_1));

    // act
    result = clds_sorted_list_delete_item(list, hazard_pointers_thread, item_1);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
    ASSERT_ARE_EQUAL(CLDS_SORTED_LIST_DELETE_RESULT, CLDS_SORTED_LIST_DELETE_OK, result);

    // cleanup
    clds_sorted_list_destroy(list);
    clds_hazard_pointers_destroy(hazard_pointers);
}

/* Tests_SRS_CLDS_SORTED_LIST_01_014: [ `clds_sorted_list_delete_item` shall delete an item from the list by its pointer. ]*/
/* Tests_SRS_CLDS_SORTED_LIST_01_026: [ On success, `clds_sorted_list_delete_item` shall return `CLDS_SORTED_LIST_DELETE_OK`. ]*/
/* Tests_SRS_CLDS_SORTED_LIST_01_042: [ When an item is deleted it shall be indicated to the hazard pointers instance as reclaimed by calling `clds_hazard_pointers_reclaim`. ]*/
/* Tests_SRS_CLDS_SORTED_LIST_01_044: [ If `item_cleanup_callback` is NULL, no user callback shall be triggered for the reclaimed item. ]*/
TEST_FUNCTION(clds_sorted_list_delete_for_the_2nd_out_of_3_items_succeeds_NULL_cleanup_callback)
{
    // arrange
    CLDS_HAZARD_POINTERS_HANDLE hazard_pointers = clds_hazard_pointers_create();
    CLDS_HAZARD_POINTERS_THREAD_HANDLE hazard_pointers_thread = clds_hazard_pointers_register_thread(hazard_pointers);
    CLDS_SORTED_LIST_HANDLE list = clds_sorted_list_create(hazard_pointers, test_get_item_key, (void*)0x4242, test_key_compare, (void*)0x4243);
    CLDS_SORTED_LIST_ITEM* item_1 = CLDS_SORTED_LIST_NODE_CREATE(TEST_ITEM, NULL, NULL);
    CLDS_SORTED_LIST_ITEM* item_2 = CLDS_SORTED_LIST_NODE_CREATE(TEST_ITEM, NULL, NULL);
    CLDS_SORTED_LIST_ITEM* item_3 = CLDS_SORTED_LIST_NODE_CREATE(TEST_ITEM, NULL, NULL);
    CLDS_SORTED_LIST_DELETE_RESULT result;
    TEST_ITEM* item_1_payload = CLDS_SORTED_LIST_GET_VALUE(TEST_ITEM, item_1);
    TEST_ITEM* item_2_payload = CLDS_SORTED_LIST_GET_VALUE(TEST_ITEM, item_2);
    TEST_ITEM* item_3_payload = CLDS_SORTED_LIST_GET_VALUE(TEST_ITEM, item_3);
    item_1_payload->key = 0x42;
    item_2_payload->key = 0x43;
    item_3_payload->key = 0x44;
    (void)clds_hazard_pointers_set_reclaim_threshold(hazard_pointers, 1);
    (void)clds_sorted_list_insert(list, hazard_pointers_thread, item_1);
    (void)clds_sorted_list_insert(list, hazard_pointers_thread, item_2);
    (void)clds_sorted_list_insert(list, hazard_pointers_thread, item_3);
    umock_c_reset_all_calls();

    STRICT_EXPECTED_CALL(clds_hazard_pointers_acquire(IGNORED_PTR_ARG, IGNORED_PTR_ARG)).IgnoreAllCalls();
    STRICT_EXPECTED_CALL(clds_hazard_pointers_release(IGNORED_PTR_ARG, IGNORED_PTR_ARG)).IgnoreAllCalls();
    STRICT_EXPECTED_CALL(clds_st_hash_set_create(IGNORED_PTR_ARG, IGNORED_NUM_ARG)).IgnoreAllCalls();
    STRICT_EXPECTED_CALL(clds_st_hash_set_destroy(IGNORED_PTR_ARG)).IgnoreAllCalls();
    STRICT_EXPECTED_CALL(clds_st_hash_set_find(IGNORED_PTR_ARG, IGNORED_PTR_ARG, IGNORED_PTR_ARG)).IgnoreAllCalls();
    STRICT_EXPECTED_CALL(clds_hazard_pointers_reclaim(hazard_pointers_thread, item_2, IGNORED_PTR_ARG));
    STRICT_EXPECTED_CALL(free(item_2));

    // act
    result = clds_sorted_list_delete_item(list, hazard_pointers_thread, item_2);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
    ASSERT_ARE_EQUAL(CLDS_SORTED_LIST_DELETE_RESULT, CLDS_SORTED_LIST_DELETE_OK, result);

    // cleanup
    clds_sorted_list_destroy(list);
    clds_hazard_pointers_destroy(hazard_pointers);
}

/* Tests_SRS_CLDS_SORTED_LIST_01_015: [ If `clds_sorted_list` is NULL, `clds_sorted_list_delete_item` shall fail and return `CLDS_SORTED_LIST_DELETE_ERROR`. ]*/
TEST_FUNCTION(clds_sorted_list_delete_with_NULL_list_fails)
{
    // arrange
    CLDS_HAZARD_POINTERS_HANDLE hazard_pointers = clds_hazard_pointers_create();
    CLDS_HAZARD_POINTERS_THREAD_HANDLE hazard_pointers_thread = clds_hazard_pointers_register_thread(hazard_pointers);
    CLDS_SORTED_LIST_HANDLE list = clds_sorted_list_create(hazard_pointers, test_get_item_key, (void*)0x4242, test_key_compare, (void*)0x4243);
    CLDS_SORTED_LIST_ITEM* item = CLDS_SORTED_LIST_NODE_CREATE(TEST_ITEM, test_item_cleanup_func, (void*)0x4242);
    CLDS_SORTED_LIST_DELETE_RESULT result;
    TEST_ITEM* item_payload = CLDS_SORTED_LIST_GET_VALUE(TEST_ITEM, item);
    item_payload->key = 0x42;
    (void)clds_sorted_list_insert(list, hazard_pointers_thread, item);
    umock_c_reset_all_calls();

    // act
    result = clds_sorted_list_delete_item(NULL, hazard_pointers_thread, item);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
    ASSERT_ARE_EQUAL(CLDS_SORTED_LIST_DELETE_RESULT, CLDS_SORTED_LIST_DELETE_ERROR, result);

    // cleanup
    clds_sorted_list_destroy(list);
    clds_hazard_pointers_destroy(hazard_pointers);
}

/* Tests_SRS_CLDS_SORTED_LIST_01_016: [ If `clds_hazard_pointers_thread` is NULL, `clds_sorted_list_delete_item` shall fail and return `CLDS_SORTED_LIST_DELETE_ERROR`. ]*/
TEST_FUNCTION(clds_sorted_list_delete_with_NULL_clds_hazard_pointers_thread_fails)
{
    // arrange
    CLDS_HAZARD_POINTERS_HANDLE hazard_pointers = clds_hazard_pointers_create();
    CLDS_HAZARD_POINTERS_THREAD_HANDLE hazard_pointers_thread = clds_hazard_pointers_register_thread(hazard_pointers);
    CLDS_SORTED_LIST_HANDLE list = clds_sorted_list_create(hazard_pointers, test_get_item_key, (void*)0x4242, test_key_compare, (void*)0x4243);
    CLDS_SORTED_LIST_ITEM* item = CLDS_SORTED_LIST_NODE_CREATE(TEST_ITEM, NULL, NULL);
    CLDS_SORTED_LIST_DELETE_RESULT result;
    (void)clds_sorted_list_insert(list, hazard_pointers_thread, item);
    TEST_ITEM* item_payload = CLDS_SORTED_LIST_GET_VALUE(TEST_ITEM, item);
    item_payload->key = 0x42;
    umock_c_reset_all_calls();

    // act
    result = clds_sorted_list_delete_item(list, NULL, item);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
    ASSERT_ARE_EQUAL(CLDS_SORTED_LIST_DELETE_RESULT, CLDS_SORTED_LIST_DELETE_ERROR, result);

    // cleanup
    clds_sorted_list_destroy(list);
    clds_hazard_pointers_destroy(hazard_pointers);
}

/* Tests_SRS_CLDS_SORTED_LIST_01_017: [ If `item` is NULL, `clds_sorted_list_delete_item` shall fail and return `CLDS_SORTED_LIST_DELETE_ERROR`. ]*/
TEST_FUNCTION(clds_sorted_list_delete_with_NULL_item_fails)
{
    // arrange
    CLDS_HAZARD_POINTERS_HANDLE hazard_pointers = clds_hazard_pointers_create();
    CLDS_HAZARD_POINTERS_THREAD_HANDLE hazard_pointers_thread = clds_hazard_pointers_register_thread(hazard_pointers);
    CLDS_SORTED_LIST_HANDLE list;
    CLDS_SORTED_LIST_DELETE_RESULT result;
    list = clds_sorted_list_create(hazard_pointers, test_get_item_key, (void*)0x4242, test_key_compare, (void*)0x4243);
    CLDS_SORTED_LIST_ITEM* item = CLDS_SORTED_LIST_NODE_CREATE(TEST_ITEM, test_item_cleanup_func, (void*)0x4242);
    TEST_ITEM* item_payload = CLDS_SORTED_LIST_GET_VALUE(TEST_ITEM, item);
    item_payload->key = 0x42;
    (void)clds_sorted_list_insert(list, hazard_pointers_thread, item);
    umock_c_reset_all_calls();

    // act
    result = clds_sorted_list_delete_item(list, hazard_pointers_thread, NULL);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
    ASSERT_ARE_EQUAL(CLDS_SORTED_LIST_DELETE_RESULT, CLDS_SORTED_LIST_DELETE_ERROR, result);

    // cleanup
    clds_sorted_list_destroy(list);
    clds_hazard_pointers_destroy(hazard_pointers);
}

/* Tests_SRS_CLDS_SORTED_LIST_01_018: [ If the item does not exist in the list, `clds_sorted_list_delete_item` shall fail and return `CLDS_SORTED_LIST_DELETE_NOT_FOUND`. ]*/
TEST_FUNCTION(clds_sorted_list_delete_on_an_empty_lists_returns_NOT_FOUND)
{
    // arrange
    CLDS_HAZARD_POINTERS_HANDLE hazard_pointers = clds_hazard_pointers_create();
    CLDS_HAZARD_POINTERS_THREAD_HANDLE hazard_pointers_thread = clds_hazard_pointers_register_thread(hazard_pointers);
    CLDS_SORTED_LIST_HANDLE list = clds_sorted_list_create(hazard_pointers, test_get_item_key, (void*)0x4242, test_key_compare, (void*)0x4243);
    CLDS_SORTED_LIST_ITEM* item = CLDS_SORTED_LIST_NODE_CREATE(TEST_ITEM, NULL, NULL);
    CLDS_SORTED_LIST_DELETE_RESULT result;
    umock_c_reset_all_calls();

    // act
    result = clds_sorted_list_delete_item(list, hazard_pointers_thread, item);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
    ASSERT_ARE_EQUAL(CLDS_SORTED_LIST_DELETE_RESULT, CLDS_SORTED_LIST_DELETE_NOT_FOUND, result);

    // cleanup
    CLDS_SORTED_LIST_NODE_RELEASE(TEST_ITEM, item);
    clds_sorted_list_destroy(list);
    clds_hazard_pointers_destroy(hazard_pointers);
}

/* Tests_SRS_CLDS_SORTED_LIST_01_018: [ If the item does not exist in the list, `clds_sorted_list_delete_item` shall fail and return `CLDS_SORTED_LIST_DELETE_NOT_FOUND`. ]*/
TEST_FUNCTION(clds_sorted_list_delete_when_the_item_is_not_in_the_list_returns_NOT_FOUND)
{
    // arrange
    CLDS_HAZARD_POINTERS_HANDLE hazard_pointers = clds_hazard_pointers_create();
    CLDS_HAZARD_POINTERS_THREAD_HANDLE hazard_pointers_thread = clds_hazard_pointers_register_thread(hazard_pointers);
    CLDS_SORTED_LIST_HANDLE list = clds_sorted_list_create(hazard_pointers, test_get_item_key, (void*)0x4242, test_key_compare, (void*)0x4243);
    CLDS_SORTED_LIST_ITEM* item_1 = CLDS_SORTED_LIST_NODE_CREATE(TEST_ITEM, test_item_cleanup_func, (void*)0x4242);
    CLDS_SORTED_LIST_ITEM* item_2 = CLDS_SORTED_LIST_NODE_CREATE(TEST_ITEM, test_item_cleanup_func, (void*)0x4242);
    CLDS_SORTED_LIST_DELETE_RESULT result;
    TEST_ITEM* item_1_payload = CLDS_SORTED_LIST_GET_VALUE(TEST_ITEM, item_1);
    TEST_ITEM* item_2_payload = CLDS_SORTED_LIST_GET_VALUE(TEST_ITEM, item_2);
    item_1_payload->key = 0x42;
    item_2_payload->key = 0x43;
    (void)clds_sorted_list_insert(list, hazard_pointers_thread, item_1);
    umock_c_reset_all_calls();

    STRICT_EXPECTED_CALL(clds_hazard_pointers_acquire(IGNORED_PTR_ARG, IGNORED_PTR_ARG)).IgnoreAllCalls();
    STRICT_EXPECTED_CALL(clds_hazard_pointers_release(IGNORED_PTR_ARG, IGNORED_PTR_ARG)).IgnoreAllCalls();

    // act
    result = clds_sorted_list_delete_item(list, hazard_pointers_thread, item_2);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
    ASSERT_ARE_EQUAL(CLDS_SORTED_LIST_DELETE_RESULT, CLDS_SORTED_LIST_DELETE_NOT_FOUND, result);

    // cleanup
    CLDS_SORTED_LIST_NODE_RELEASE(TEST_ITEM, item_2);
    clds_sorted_list_destroy(list);
    clds_hazard_pointers_destroy(hazard_pointers);
}

/* Tests_SRS_CLDS_SORTED_LIST_01_018: [ If the item does not exist in the list, `clds_sorted_list_delete_item` shall fail and return `CLDS_SORTED_LIST_DELETE_NOT_FOUND`. ]*/
TEST_FUNCTION(clds_sorted_list_delete_twice_on_the_same_item_returns_NOT_FOUND)
{
    // arrange
    CLDS_HAZARD_POINTERS_HANDLE hazard_pointers = clds_hazard_pointers_create();
    CLDS_HAZARD_POINTERS_THREAD_HANDLE hazard_pointers_thread = clds_hazard_pointers_register_thread(hazard_pointers);
    CLDS_SORTED_LIST_HANDLE list = clds_sorted_list_create(hazard_pointers, test_get_item_key, (void*)0x4242, test_key_compare, (void*)0x4243);
    CLDS_SORTED_LIST_ITEM* item = CLDS_SORTED_LIST_NODE_CREATE(TEST_ITEM, test_item_cleanup_func, (void*)0x4242);
    CLDS_SORTED_LIST_DELETE_RESULT result;
    TEST_ITEM* item_payload = CLDS_SORTED_LIST_GET_VALUE(TEST_ITEM, item);
    item_payload->key = 0x42;
    (void)CLDS_SORTED_LIST_NODE_INC_REF(TEST_ITEM, item);
    (void)clds_sorted_list_insert(list, hazard_pointers_thread, item);
    (void)clds_sorted_list_delete_item(list, hazard_pointers_thread, item);
    umock_c_reset_all_calls();

    // act
    result = clds_sorted_list_delete_item(list, hazard_pointers_thread, item);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
    ASSERT_ARE_EQUAL(CLDS_SORTED_LIST_DELETE_RESULT, CLDS_SORTED_LIST_DELETE_NOT_FOUND, result);

    // cleanup
    CLDS_SORTED_LIST_NODE_RELEASE(TEST_ITEM, item);
    clds_sorted_list_destroy(list);
    clds_hazard_pointers_destroy(hazard_pointers);
}

/* clds_sorted_list_delete_key */

/* Tests_SRS_CLDS_SORTED_LIST_01_019: [ `clds_sorted_list_delete_key` shall delete an item by its key. ]*/
/* Tests_SRS_CLDS_SORTED_LIST_01_025: [ On success, `clds_sorted_list_delete_key` shall return `CLDS_SORTED_LIST_DELETE_OK`. ]*/
/* Tests_SRS_CLDS_SORTED_LIST_01_043: [ The reclaim function passed to `clds_hazard_pointers_reclaim` shall call the user callback `item_cleanup_callback` that was passed to `clds_sorted_list_node_create`, while passing `item_cleanup_callback_context` and the freed item as arguments. ]*/
TEST_FUNCTION(clds_sorted_list_delete_key_succeeds)
{
    // arrange
    CLDS_HAZARD_POINTERS_HANDLE hazard_pointers = clds_hazard_pointers_create();
    CLDS_HAZARD_POINTERS_THREAD_HANDLE hazard_pointers_thread = clds_hazard_pointers_register_thread(hazard_pointers);
    CLDS_SORTED_LIST_HANDLE list = clds_sorted_list_create(hazard_pointers, test_get_item_key, (void*)0x4242, test_key_compare, (void*)0x4243);
    CLDS_SORTED_LIST_ITEM* item = CLDS_SORTED_LIST_NODE_CREATE(TEST_ITEM, test_item_cleanup_func, (void*)0x4242);
    CLDS_SORTED_LIST_DELETE_RESULT result;
    TEST_ITEM* item_payload = CLDS_SORTED_LIST_GET_VALUE(TEST_ITEM, item);
    item_payload->key = 0x42;
    (void)clds_hazard_pointers_set_reclaim_threshold(hazard_pointers, 1);
    (void)clds_sorted_list_insert(list, hazard_pointers_thread, item);
    umock_c_reset_all_calls();

    STRICT_EXPECTED_CALL(clds_hazard_pointers_acquire(IGNORED_PTR_ARG, IGNORED_PTR_ARG)).IgnoreAllCalls();
    STRICT_EXPECTED_CALL(clds_hazard_pointers_release(IGNORED_PTR_ARG, IGNORED_PTR_ARG)).IgnoreAllCalls();
    STRICT_EXPECTED_CALL(clds_st_hash_set_create(IGNORED_PTR_ARG, IGNORED_NUM_ARG)).IgnoreAllCalls();
    STRICT_EXPECTED_CALL(clds_st_hash_set_destroy(IGNORED_PTR_ARG)).IgnoreAllCalls();
    STRICT_EXPECTED_CALL(clds_st_hash_set_find(IGNORED_PTR_ARG, IGNORED_PTR_ARG, IGNORED_PTR_ARG)).IgnoreAllCalls();
    STRICT_EXPECTED_CALL(clds_hazard_pointers_reclaim(hazard_pointers_thread, item, IGNORED_PTR_ARG));
    STRICT_EXPECTED_CALL(test_item_cleanup_func((void*)0x4242, item));
    STRICT_EXPECTED_CALL(free(item));

    // act
    result = clds_sorted_list_delete_key(list, hazard_pointers_thread, (void*)0x42);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
    ASSERT_ARE_EQUAL(CLDS_SORTED_LIST_DELETE_RESULT, CLDS_SORTED_LIST_DELETE_OK, result);

    // cleanup
    clds_sorted_list_destroy(list);
    clds_hazard_pointers_destroy(hazard_pointers);
}

/* Tests_SRS_CLDS_SORTED_LIST_01_019: [ `clds_sorted_list_delete_key` shall delete an item by its key. ]*/
/* Tests_SRS_CLDS_SORTED_LIST_01_025: [ On success, `clds_sorted_list_delete_key` shall return `CLDS_SORTED_LIST_DELETE_OK`. ]*/
/* Tests_SRS_CLDS_SORTED_LIST_01_044: [ If `item_cleanup_callback` is NULL, no user callback shall be triggered for the reclaimed item. ]*/
TEST_FUNCTION(clds_sorted_list_delete_key_succeeds_with_NULL_item_cleanup_callback)
{
    // arrange
    CLDS_HAZARD_POINTERS_HANDLE hazard_pointers = clds_hazard_pointers_create();
    CLDS_HAZARD_POINTERS_THREAD_HANDLE hazard_pointers_thread = clds_hazard_pointers_register_thread(hazard_pointers);
    CLDS_SORTED_LIST_HANDLE list = clds_sorted_list_create(hazard_pointers, test_get_item_key, (void*)0x4242, test_key_compare, (void*)0x4243);
    CLDS_SORTED_LIST_ITEM* item = CLDS_SORTED_LIST_NODE_CREATE(TEST_ITEM, NULL, (void*)0x4242);
    TEST_ITEM* item_payload = CLDS_SORTED_LIST_GET_VALUE(TEST_ITEM, item);
    item_payload->key = 0x42;
    CLDS_SORTED_LIST_DELETE_RESULT result;
    (void)clds_hazard_pointers_set_reclaim_threshold(hazard_pointers, 1);
    (void)clds_sorted_list_insert(list, hazard_pointers_thread, item);
    umock_c_reset_all_calls();

    STRICT_EXPECTED_CALL(clds_hazard_pointers_acquire(IGNORED_PTR_ARG, IGNORED_PTR_ARG)).IgnoreAllCalls();
    STRICT_EXPECTED_CALL(clds_hazard_pointers_release(IGNORED_PTR_ARG, IGNORED_PTR_ARG)).IgnoreAllCalls();
    STRICT_EXPECTED_CALL(clds_st_hash_set_create(IGNORED_PTR_ARG, IGNORED_NUM_ARG)).IgnoreAllCalls();
    STRICT_EXPECTED_CALL(clds_st_hash_set_destroy(IGNORED_PTR_ARG)).IgnoreAllCalls();
    STRICT_EXPECTED_CALL(clds_st_hash_set_find(IGNORED_PTR_ARG, IGNORED_PTR_ARG, IGNORED_PTR_ARG)).IgnoreAllCalls();
    STRICT_EXPECTED_CALL(clds_hazard_pointers_reclaim(hazard_pointers_thread, item, IGNORED_PTR_ARG));
    STRICT_EXPECTED_CALL(free(item));

    // act
    result = clds_sorted_list_delete_key(list, hazard_pointers_thread, (void*)0x42);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
    ASSERT_ARE_EQUAL(CLDS_SORTED_LIST_DELETE_RESULT, CLDS_SORTED_LIST_DELETE_OK, result);

    // cleanup
    clds_sorted_list_destroy(list);
    clds_hazard_pointers_destroy(hazard_pointers);
}

/* Tests_SRS_CLDS_SORTED_LIST_01_020: [ If `clds_sorted_list` is NULL, `clds_sorted_list_delete_key` shall fail and return `CLDS_SORTED_LIST_DELETE_ERROR`. ]*/
TEST_FUNCTION(clds_sorted_list_delete_key_with_NULL_clds_sorted_list_fails)
{
    // arrange
    CLDS_HAZARD_POINTERS_HANDLE hazard_pointers = clds_hazard_pointers_create();
    CLDS_HAZARD_POINTERS_THREAD_HANDLE hazard_pointers_thread = clds_hazard_pointers_register_thread(hazard_pointers);
    CLDS_SORTED_LIST_HANDLE list = clds_sorted_list_create(hazard_pointers, test_get_item_key, (void*)0x4242, test_key_compare, (void*)0x4243);
    CLDS_SORTED_LIST_ITEM* item = CLDS_SORTED_LIST_NODE_CREATE(TEST_ITEM, test_item_cleanup_func, (void*)0x4242);
    CLDS_SORTED_LIST_DELETE_RESULT result;
    TEST_ITEM* item_payload = CLDS_SORTED_LIST_GET_VALUE(TEST_ITEM, item);
    item_payload->key = 0x42;
    (void)clds_hazard_pointers_set_reclaim_threshold(hazard_pointers, 1);
    (void)clds_sorted_list_insert(list, hazard_pointers_thread, item);
    umock_c_reset_all_calls();

    // act
    result = clds_sorted_list_delete_key(NULL, hazard_pointers_thread, (void*)0x42);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
    ASSERT_ARE_EQUAL(CLDS_SORTED_LIST_DELETE_RESULT, CLDS_SORTED_LIST_DELETE_ERROR, result);

    // cleanup
    clds_sorted_list_destroy(list);
    clds_hazard_pointers_destroy(hazard_pointers);
}

/* Tests_SRS_CLDS_SORTED_LIST_01_021: [ If `clds_hazard_pointers_thread` is NULL, `clds_sorted_list_delete_key` shall fail and return `CLDS_SORTED_LIST_DELETE_ERROR`. ]*/
TEST_FUNCTION(clds_sorted_list_delete_key_with_NULL_hazard_pointers_thread_fails)
{
    // arrange
    CLDS_HAZARD_POINTERS_HANDLE hazard_pointers = clds_hazard_pointers_create();
    CLDS_HAZARD_POINTERS_THREAD_HANDLE hazard_pointers_thread = clds_hazard_pointers_register_thread(hazard_pointers);
    CLDS_SORTED_LIST_HANDLE list = clds_sorted_list_create(hazard_pointers, test_get_item_key, (void*)0x4242, test_key_compare, (void*)0x4243);
    CLDS_SORTED_LIST_ITEM* item = CLDS_SORTED_LIST_NODE_CREATE(TEST_ITEM, test_item_cleanup_func, (void*)0x4242);
    CLDS_SORTED_LIST_DELETE_RESULT result;
    TEST_ITEM* item_payload = CLDS_SORTED_LIST_GET_VALUE(TEST_ITEM, item);
    item_payload->key = 0x42;
    (void)clds_hazard_pointers_set_reclaim_threshold(hazard_pointers, 1);
    (void)clds_sorted_list_insert(list, hazard_pointers_thread, item);
    umock_c_reset_all_calls();

    // act
    result = clds_sorted_list_delete_key(list, NULL, (void*)0x42);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
    ASSERT_ARE_EQUAL(CLDS_SORTED_LIST_DELETE_RESULT, CLDS_SORTED_LIST_DELETE_ERROR, result);

    // cleanup
    clds_sorted_list_destroy(list);
    clds_hazard_pointers_destroy(hazard_pointers);
}

/* Tests_SRS_CLDS_SORTED_LIST_01_022: [ If `key` is NULL, `clds_sorted_list_delete_key` shall fail and return `CLDS_SORTED_LIST_DELETE_ERROR`. ]*/
TEST_FUNCTION(clds_sorted_list_delete_key_with_NULL_key_fails)
{
    // arrange
    CLDS_HAZARD_POINTERS_HANDLE hazard_pointers = clds_hazard_pointers_create();
    CLDS_HAZARD_POINTERS_THREAD_HANDLE hazard_pointers_thread = clds_hazard_pointers_register_thread(hazard_pointers);
    CLDS_SORTED_LIST_HANDLE list = clds_sorted_list_create(hazard_pointers, test_get_item_key, (void*)0x4242, test_key_compare, (void*)0x4243);
    CLDS_SORTED_LIST_ITEM* item = CLDS_SORTED_LIST_NODE_CREATE(TEST_ITEM, test_item_cleanup_func, (void*)0x4242);
    CLDS_SORTED_LIST_DELETE_RESULT result;
    TEST_ITEM* item_payload = CLDS_SORTED_LIST_GET_VALUE(TEST_ITEM, item);
    item_payload->key = 0x42;
    (void)clds_hazard_pointers_set_reclaim_threshold(hazard_pointers, 1);
    (void)clds_sorted_list_insert(list, hazard_pointers_thread, item);
    umock_c_reset_all_calls();

    // act
    result = clds_sorted_list_delete_key(list, hazard_pointers_thread, NULL);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
    ASSERT_ARE_EQUAL(CLDS_SORTED_LIST_DELETE_RESULT, CLDS_SORTED_LIST_DELETE_ERROR, result);

    // cleanup
    clds_sorted_list_destroy(list);
    clds_hazard_pointers_destroy(hazard_pointers);
}

/* Tests_SRS_CLDS_SORTED_LIST_01_019: [ `clds_sorted_list_delete_key` shall delete an item by its key. ]*/
/* Tests_SRS_CLDS_SORTED_LIST_01_025: [ On success, `clds_sorted_list_delete_key` shall return `CLDS_SORTED_LIST_DELETE_OK`. ]*/
/* Tests_SRS_CLDS_SORTED_LIST_01_043: [ The reclaim function passed to `clds_hazard_pointers_reclaim` shall call the user callback `item_cleanup_callback` that was passed to `clds_sorted_list_node_create`, while passing `item_cleanup_callback_context` and the freed item as arguments. ]*/
TEST_FUNCTION(clds_sorted_list_delete_key_deletes_the_second_item)
{
    // arrange
    CLDS_HAZARD_POINTERS_HANDLE hazard_pointers = clds_hazard_pointers_create();
    CLDS_HAZARD_POINTERS_THREAD_HANDLE hazard_pointers_thread = clds_hazard_pointers_register_thread(hazard_pointers);
    CLDS_SORTED_LIST_HANDLE list = clds_sorted_list_create(hazard_pointers, test_get_item_key, (void*)0x4242, test_key_compare, (void*)0x4243);
    CLDS_SORTED_LIST_ITEM* item_1 = CLDS_SORTED_LIST_NODE_CREATE(TEST_ITEM, test_item_cleanup_func, (void*)0x4242);
    CLDS_SORTED_LIST_ITEM* item_2 = CLDS_SORTED_LIST_NODE_CREATE(TEST_ITEM, test_item_cleanup_func, (void*)0x4242);
    TEST_ITEM* item_1_payload = CLDS_SORTED_LIST_GET_VALUE(TEST_ITEM, item_1);
    TEST_ITEM* item_2_payload = CLDS_SORTED_LIST_GET_VALUE(TEST_ITEM, item_2);
    item_1_payload->key = 0x42;
    item_2_payload->key = 0x43;
    CLDS_SORTED_LIST_DELETE_RESULT result;
    (void)clds_hazard_pointers_set_reclaim_threshold(hazard_pointers, 1);
    (void)clds_sorted_list_insert(list, hazard_pointers_thread, item_1);
    (void)clds_sorted_list_insert(list, hazard_pointers_thread, item_2);
    umock_c_reset_all_calls();

    STRICT_EXPECTED_CALL(clds_hazard_pointers_acquire(IGNORED_PTR_ARG, IGNORED_PTR_ARG)).IgnoreAllCalls();
    STRICT_EXPECTED_CALL(clds_hazard_pointers_release(IGNORED_PTR_ARG, IGNORED_PTR_ARG)).IgnoreAllCalls();
    STRICT_EXPECTED_CALL(clds_st_hash_set_create(IGNORED_PTR_ARG, IGNORED_NUM_ARG)).IgnoreAllCalls();
    STRICT_EXPECTED_CALL(clds_st_hash_set_destroy(IGNORED_PTR_ARG)).IgnoreAllCalls();
    STRICT_EXPECTED_CALL(clds_st_hash_set_find(IGNORED_PTR_ARG, IGNORED_PTR_ARG, IGNORED_PTR_ARG)).IgnoreAllCalls();
    STRICT_EXPECTED_CALL(clds_hazard_pointers_reclaim(hazard_pointers_thread, item_2, IGNORED_PTR_ARG));
    STRICT_EXPECTED_CALL(test_item_cleanup_func((void*)0x4242, item_2));
    STRICT_EXPECTED_CALL(free(item_2));

    // act
    result = clds_sorted_list_delete_key(list, hazard_pointers_thread, (void*)0x43);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
    ASSERT_ARE_EQUAL(CLDS_SORTED_LIST_DELETE_RESULT, CLDS_SORTED_LIST_DELETE_OK, result);

    // cleanup
    clds_sorted_list_destroy(list);
    clds_hazard_pointers_destroy(hazard_pointers);
}

/* Tests_SRS_CLDS_SORTED_LIST_01_019: [ `clds_sorted_list_delete_key` shall delete an item by its key. ]*/
/* Tests_SRS_CLDS_SORTED_LIST_01_025: [ On success, `clds_sorted_list_delete_key` shall return `CLDS_SORTED_LIST_DELETE_OK`. ]*/
/* Tests_SRS_CLDS_SORTED_LIST_01_043: [ The reclaim function passed to `clds_hazard_pointers_reclaim` shall call the user callback `item_cleanup_callback` that was passed to `clds_sorted_list_node_create`, while passing `item_cleanup_callback_context` and the freed item as arguments. ]*/
TEST_FUNCTION(clds_sorted_list_delete_key_deletes_the_oldest_out_of_2_items)
{
    // arrange
    CLDS_HAZARD_POINTERS_HANDLE hazard_pointers = clds_hazard_pointers_create();
    CLDS_HAZARD_POINTERS_THREAD_HANDLE hazard_pointers_thread = clds_hazard_pointers_register_thread(hazard_pointers);
    CLDS_SORTED_LIST_HANDLE list = clds_sorted_list_create(hazard_pointers, test_get_item_key, (void*)0x4242, test_key_compare, (void*)0x4243);
    CLDS_SORTED_LIST_ITEM* item_1 = CLDS_SORTED_LIST_NODE_CREATE(TEST_ITEM, test_item_cleanup_func, (void*)0x4242);
    CLDS_SORTED_LIST_ITEM* item_2 = CLDS_SORTED_LIST_NODE_CREATE(TEST_ITEM, test_item_cleanup_func, (void*)0x4242);
    CLDS_SORTED_LIST_DELETE_RESULT result;
    TEST_ITEM* item_1_payload = CLDS_SORTED_LIST_GET_VALUE(TEST_ITEM, item_1);
    TEST_ITEM* item_2_payload = CLDS_SORTED_LIST_GET_VALUE(TEST_ITEM, item_2);
    item_1_payload->key = 0x42;
    item_2_payload->key = 0x43;
    (void)clds_hazard_pointers_set_reclaim_threshold(hazard_pointers, 1);
    (void)clds_sorted_list_insert(list, hazard_pointers_thread, item_1);
    (void)clds_sorted_list_insert(list, hazard_pointers_thread, item_2);
    umock_c_reset_all_calls();

    STRICT_EXPECTED_CALL(clds_hazard_pointers_acquire(IGNORED_PTR_ARG, IGNORED_PTR_ARG)).IgnoreAllCalls();
    STRICT_EXPECTED_CALL(clds_hazard_pointers_release(IGNORED_PTR_ARG, IGNORED_PTR_ARG)).IgnoreAllCalls();
    STRICT_EXPECTED_CALL(clds_st_hash_set_create(IGNORED_PTR_ARG, IGNORED_NUM_ARG)).IgnoreAllCalls();
    STRICT_EXPECTED_CALL(clds_st_hash_set_destroy(IGNORED_PTR_ARG)).IgnoreAllCalls();
    STRICT_EXPECTED_CALL(clds_st_hash_set_find(IGNORED_PTR_ARG, IGNORED_PTR_ARG, IGNORED_PTR_ARG)).IgnoreAllCalls();
    STRICT_EXPECTED_CALL(clds_hazard_pointers_reclaim(hazard_pointers_thread, item_1, IGNORED_PTR_ARG));
    STRICT_EXPECTED_CALL(test_item_cleanup_func((void*)0x4242, item_1));
    STRICT_EXPECTED_CALL(free(item_1));

    // act
    result = clds_sorted_list_delete_key(list, hazard_pointers_thread, (void*)0x42);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
    ASSERT_ARE_EQUAL(CLDS_SORTED_LIST_DELETE_RESULT, CLDS_SORTED_LIST_DELETE_OK, result);

    // cleanup
    clds_sorted_list_destroy(list);
    clds_hazard_pointers_destroy(hazard_pointers);
}

/* Tests_SRS_CLDS_SORTED_LIST_01_019: [ `clds_sorted_list_delete_key` shall delete an item by its key. ]*/
/* Tests_SRS_CLDS_SORTED_LIST_01_025: [ On success, `clds_sorted_list_delete_key` shall return `CLDS_SORTED_LIST_DELETE_OK`. ]*/
/* Tests_SRS_CLDS_SORTED_LIST_01_043: [ The reclaim function passed to `clds_hazard_pointers_reclaim` shall call the user callback `item_cleanup_callback` that was passed to `clds_sorted_list_node_create`, while passing `item_cleanup_callback_context` and the freed item as arguments. ]*/
TEST_FUNCTION(clds_sorted_list_delete_key_deletes_the_2nd_out_of_3_items)
{
    // arrange
    CLDS_HAZARD_POINTERS_HANDLE hazard_pointers = clds_hazard_pointers_create();
    CLDS_HAZARD_POINTERS_THREAD_HANDLE hazard_pointers_thread = clds_hazard_pointers_register_thread(hazard_pointers);
    CLDS_SORTED_LIST_HANDLE list = clds_sorted_list_create(hazard_pointers, test_get_item_key, (void*)0x4242, test_key_compare, (void*)0x4243);
    CLDS_SORTED_LIST_ITEM* item_1 = CLDS_SORTED_LIST_NODE_CREATE(TEST_ITEM, test_item_cleanup_func, (void*)0x4242);
    CLDS_SORTED_LIST_ITEM* item_2 = CLDS_SORTED_LIST_NODE_CREATE(TEST_ITEM, test_item_cleanup_func, (void*)0x4242);
    CLDS_SORTED_LIST_ITEM* item_3 = CLDS_SORTED_LIST_NODE_CREATE(TEST_ITEM, test_item_cleanup_func, (void*)0x4242);
    CLDS_SORTED_LIST_DELETE_RESULT result;
    TEST_ITEM* item_1_payload = CLDS_SORTED_LIST_GET_VALUE(TEST_ITEM, item_1);
    TEST_ITEM* item_2_payload = CLDS_SORTED_LIST_GET_VALUE(TEST_ITEM, item_2);
    TEST_ITEM* item_3_payload = CLDS_SORTED_LIST_GET_VALUE(TEST_ITEM, item_3);
    item_1_payload->key = 0x42;
    item_2_payload->key = 0x43;
    item_3_payload->key = 0x44;
    (void)clds_hazard_pointers_set_reclaim_threshold(hazard_pointers, 1);
    (void)clds_sorted_list_insert(list, hazard_pointers_thread, item_1);
    (void)clds_sorted_list_insert(list, hazard_pointers_thread, item_2);
    (void)clds_sorted_list_insert(list, hazard_pointers_thread, item_3);
    umock_c_reset_all_calls();

    STRICT_EXPECTED_CALL(clds_hazard_pointers_acquire(IGNORED_PTR_ARG, IGNORED_PTR_ARG)).IgnoreAllCalls();
    STRICT_EXPECTED_CALL(clds_hazard_pointers_release(IGNORED_PTR_ARG, IGNORED_PTR_ARG)).IgnoreAllCalls();
    STRICT_EXPECTED_CALL(clds_st_hash_set_create(IGNORED_PTR_ARG, IGNORED_NUM_ARG)).IgnoreAllCalls();
    STRICT_EXPECTED_CALL(clds_st_hash_set_destroy(IGNORED_PTR_ARG)).IgnoreAllCalls();
    STRICT_EXPECTED_CALL(clds_st_hash_set_find(IGNORED_PTR_ARG, IGNORED_PTR_ARG, IGNORED_PTR_ARG)).IgnoreAllCalls();
    STRICT_EXPECTED_CALL(clds_hazard_pointers_reclaim(hazard_pointers_thread, item_2, IGNORED_PTR_ARG));
    STRICT_EXPECTED_CALL(test_item_cleanup_func((void*)0x4242, item_2));
    STRICT_EXPECTED_CALL(free(item_2));

    // act
    result = clds_sorted_list_delete_key(list, hazard_pointers_thread, (void*)0x43);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
    ASSERT_ARE_EQUAL(CLDS_SORTED_LIST_DELETE_RESULT, CLDS_SORTED_LIST_DELETE_OK, result);

    // cleanup
    clds_sorted_list_destroy(list);
    clds_hazard_pointers_destroy(hazard_pointers);
}

/* Tests_SRS_CLDS_SORTED_LIST_01_024: [ If the key is not found, `clds_sorted_list_delete_key` shall fail and return `CLDS_SORTED_LIST_DELETE_NOT_FOUND`. ]*/
TEST_FUNCTION(clds_sorted_list_delete_key_on_an_empty_list_yields_NOT_FOUND)
{
    // arrange
    CLDS_HAZARD_POINTERS_HANDLE hazard_pointers = clds_hazard_pointers_create();
    CLDS_HAZARD_POINTERS_THREAD_HANDLE hazard_pointers_thread = clds_hazard_pointers_register_thread(hazard_pointers);
    CLDS_SORTED_LIST_HANDLE list = clds_sorted_list_create(hazard_pointers, test_get_item_key, (void*)0x4242, test_key_compare, (void*)0x4243);
    CLDS_SORTED_LIST_DELETE_RESULT result;
    (void)clds_hazard_pointers_set_reclaim_threshold(hazard_pointers, 1);
    umock_c_reset_all_calls();

    STRICT_EXPECTED_CALL(clds_hazard_pointers_acquire(IGNORED_PTR_ARG, IGNORED_PTR_ARG)).IgnoreAllCalls();
    STRICT_EXPECTED_CALL(clds_hazard_pointers_release(IGNORED_PTR_ARG, IGNORED_PTR_ARG)).IgnoreAllCalls();
    STRICT_EXPECTED_CALL(clds_st_hash_set_create(IGNORED_PTR_ARG, IGNORED_NUM_ARG)).IgnoreAllCalls();
    STRICT_EXPECTED_CALL(clds_st_hash_set_destroy(IGNORED_PTR_ARG)).IgnoreAllCalls();
    STRICT_EXPECTED_CALL(clds_st_hash_set_find(IGNORED_PTR_ARG, IGNORED_PTR_ARG, IGNORED_PTR_ARG)).IgnoreAllCalls();

    // act
    result = clds_sorted_list_delete_key(list, hazard_pointers_thread, (void*)0x42);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
    ASSERT_ARE_EQUAL(CLDS_SORTED_LIST_DELETE_RESULT, CLDS_SORTED_LIST_DELETE_NOT_FOUND, result);

    // cleanup
    clds_sorted_list_destroy(list);
    clds_hazard_pointers_destroy(hazard_pointers);
}

/* Tests_SRS_CLDS_SORTED_LIST_01_024: [ If the key is not found, `clds_sorted_list_delete_key` shall fail and return `CLDS_SORTED_LIST_DELETE_NOT_FOUND`. ]*/
TEST_FUNCTION(clds_sorted_list_delete_key_after_the_item_was_deleted_yields_NOT_FOUND)
{
    // arrange
    CLDS_HAZARD_POINTERS_HANDLE hazard_pointers = clds_hazard_pointers_create();
    CLDS_HAZARD_POINTERS_THREAD_HANDLE hazard_pointers_thread = clds_hazard_pointers_register_thread(hazard_pointers);
    CLDS_SORTED_LIST_HANDLE list = clds_sorted_list_create(hazard_pointers, test_get_item_key, (void*)0x4242, test_key_compare, (void*)0x4243);
    CLDS_SORTED_LIST_ITEM* item = CLDS_SORTED_LIST_NODE_CREATE(TEST_ITEM, test_item_cleanup_func, (void*)0x4242);
    CLDS_SORTED_LIST_DELETE_RESULT result;
    TEST_ITEM* item_payload = CLDS_SORTED_LIST_GET_VALUE(TEST_ITEM, item);
    item_payload->key = 0x42;
    (void)clds_hazard_pointers_set_reclaim_threshold(hazard_pointers, 1);
    (void)clds_sorted_list_insert(list, hazard_pointers_thread, item);
    (void)clds_sorted_list_delete_item(list, hazard_pointers_thread, item);
    umock_c_reset_all_calls();

    STRICT_EXPECTED_CALL(clds_hazard_pointers_acquire(IGNORED_PTR_ARG, IGNORED_PTR_ARG)).IgnoreAllCalls();
    STRICT_EXPECTED_CALL(clds_hazard_pointers_release(IGNORED_PTR_ARG, IGNORED_PTR_ARG)).IgnoreAllCalls();
    STRICT_EXPECTED_CALL(clds_st_hash_set_create(IGNORED_PTR_ARG, IGNORED_NUM_ARG)).IgnoreAllCalls();
    STRICT_EXPECTED_CALL(clds_st_hash_set_destroy(IGNORED_PTR_ARG)).IgnoreAllCalls();
    STRICT_EXPECTED_CALL(clds_st_hash_set_find(IGNORED_PTR_ARG, IGNORED_PTR_ARG, IGNORED_PTR_ARG)).IgnoreAllCalls();

    // act
    result = clds_sorted_list_delete_key(list, hazard_pointers_thread, (void*)0x43);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
    ASSERT_ARE_EQUAL(CLDS_SORTED_LIST_DELETE_RESULT, CLDS_SORTED_LIST_DELETE_NOT_FOUND, result);

    // cleanup
    clds_sorted_list_destroy(list);
    clds_hazard_pointers_destroy(hazard_pointers);
}

/* clds_sorted_list_remove_key */

/* Tests_SRS_CLDS_SORTED_LIST_01_051: [ `clds_sorted_list_remove_key` shall delete an remove an item by its key and return the pointer to it. ]*/
/* Tests_SRS_CLDS_SORTED_LIST_01_052: [ On success, `clds_sorted_list_remove_key` shall return `CLDS_SORTED_LIST_REMOVE_OK`. ]*/
/* Tests_SRS_CLDS_SORTED_LIST_01_054: [ On success, the found item shall be returned in the `item` argument. ]*/
TEST_FUNCTION(clds_sorted_list_remove_key_succeeds)
{
    // arrange
    CLDS_HAZARD_POINTERS_HANDLE hazard_pointers = clds_hazard_pointers_create();
    CLDS_HAZARD_POINTERS_THREAD_HANDLE hazard_pointers_thread = clds_hazard_pointers_register_thread(hazard_pointers);
    CLDS_SORTED_LIST_HANDLE list = clds_sorted_list_create(hazard_pointers, test_get_item_key, (void*)0x4242, test_key_compare, (void*)0x4243);
    CLDS_SORTED_LIST_ITEM* item = CLDS_SORTED_LIST_NODE_CREATE(TEST_ITEM, test_item_cleanup_func, (void*)0x4242);
    CLDS_SORTED_LIST_ITEM* removed_item;
    CLDS_SORTED_LIST_REMOVE_RESULT result;
    TEST_ITEM* item_payload = CLDS_SORTED_LIST_GET_VALUE(TEST_ITEM, item);
    item_payload->key = 0x42;
    (void)clds_hazard_pointers_set_reclaim_threshold(hazard_pointers, 1);
    (void)clds_sorted_list_insert(list, hazard_pointers_thread, item);
    umock_c_reset_all_calls();

    STRICT_EXPECTED_CALL(clds_hazard_pointers_acquire(IGNORED_PTR_ARG, IGNORED_PTR_ARG)).IgnoreAllCalls();
    STRICT_EXPECTED_CALL(clds_hazard_pointers_release(IGNORED_PTR_ARG, IGNORED_PTR_ARG)).IgnoreAllCalls();
    STRICT_EXPECTED_CALL(clds_st_hash_set_create(IGNORED_PTR_ARG, IGNORED_NUM_ARG)).IgnoreAllCalls();
    STRICT_EXPECTED_CALL(clds_st_hash_set_destroy(IGNORED_PTR_ARG)).IgnoreAllCalls();
    STRICT_EXPECTED_CALL(clds_st_hash_set_find(IGNORED_PTR_ARG, IGNORED_PTR_ARG, IGNORED_PTR_ARG)).IgnoreAllCalls();
    STRICT_EXPECTED_CALL(clds_hazard_pointers_reclaim(hazard_pointers_thread, item, IGNORED_PTR_ARG));

    // act
    result = clds_sorted_list_remove_key(list, hazard_pointers_thread, (void*)0x42, &removed_item);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
    ASSERT_ARE_EQUAL(CLDS_SORTED_LIST_REMOVE_RESULT, CLDS_SORTED_LIST_REMOVE_OK, result);

    // cleanup
    CLDS_SORTED_LIST_NODE_RELEASE(TEST_ITEM, removed_item);
    clds_sorted_list_destroy(list);
    clds_hazard_pointers_destroy(hazard_pointers);
}

/* Tests_SRS_CLDS_SORTED_LIST_01_051: [ `clds_sorted_list_remove_key` shall delete an remove an item by its key and return the pointer to it. ]*/
/* Tests_SRS_CLDS_SORTED_LIST_01_052: [ On success, `clds_sorted_list_remove_key` shall return `CLDS_SORTED_LIST_REMOVE_OK`. ]*/
/* Tests_SRS_CLDS_SORTED_LIST_01_054: [ On success, the found item shall be returned in the `item` argument. ]*/
TEST_FUNCTION(clds_sorted_list_remove_key_succeeds_with_NULL_item_cleanup_callback)
{
    // arrange
    CLDS_HAZARD_POINTERS_HANDLE hazard_pointers = clds_hazard_pointers_create();
    CLDS_HAZARD_POINTERS_THREAD_HANDLE hazard_pointers_thread = clds_hazard_pointers_register_thread(hazard_pointers);
    CLDS_SORTED_LIST_HANDLE list = clds_sorted_list_create(hazard_pointers, test_get_item_key, (void*)0x4242, test_key_compare, (void*)0x4243);
    CLDS_SORTED_LIST_ITEM* item = CLDS_SORTED_LIST_NODE_CREATE(TEST_ITEM, NULL, (void*)0x4242);
    CLDS_SORTED_LIST_ITEM* removed_item;
    TEST_ITEM* item_payload = CLDS_SORTED_LIST_GET_VALUE(TEST_ITEM, item);
    item_payload->key = 0x42;
    CLDS_SORTED_LIST_REMOVE_RESULT result;
    (void)clds_hazard_pointers_set_reclaim_threshold(hazard_pointers, 1);
    (void)clds_sorted_list_insert(list, hazard_pointers_thread, item);
    umock_c_reset_all_calls();

    STRICT_EXPECTED_CALL(clds_hazard_pointers_acquire(IGNORED_PTR_ARG, IGNORED_PTR_ARG)).IgnoreAllCalls();
    STRICT_EXPECTED_CALL(clds_hazard_pointers_release(IGNORED_PTR_ARG, IGNORED_PTR_ARG)).IgnoreAllCalls();
    STRICT_EXPECTED_CALL(clds_st_hash_set_create(IGNORED_PTR_ARG, IGNORED_NUM_ARG)).IgnoreAllCalls();
    STRICT_EXPECTED_CALL(clds_st_hash_set_destroy(IGNORED_PTR_ARG)).IgnoreAllCalls();
    STRICT_EXPECTED_CALL(clds_st_hash_set_find(IGNORED_PTR_ARG, IGNORED_PTR_ARG, IGNORED_PTR_ARG)).IgnoreAllCalls();
    STRICT_EXPECTED_CALL(clds_hazard_pointers_reclaim(hazard_pointers_thread, item, IGNORED_PTR_ARG));

    // act
    result = clds_sorted_list_remove_key(list, hazard_pointers_thread, (void*)0x42, &removed_item);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
    ASSERT_ARE_EQUAL(CLDS_SORTED_LIST_REMOVE_RESULT, CLDS_SORTED_LIST_REMOVE_OK, result);

    // cleanup
    CLDS_SORTED_LIST_NODE_RELEASE(TEST_ITEM, removed_item);
    clds_sorted_list_destroy(list);
    clds_hazard_pointers_destroy(hazard_pointers);
}

/* Tests_SRS_CLDS_SORTED_LIST_01_053: [ If `clds_sorted_list` is NULL, `clds_sorted_list_remove_key` shall fail and return `CLDS_SORTED_LIST_REMOVE_ERROR`. ]*/
TEST_FUNCTION(clds_sorted_list_remove_key_with_NULL_clds_sorted_list_fails)
{
    // arrange
    CLDS_HAZARD_POINTERS_HANDLE hazard_pointers = clds_hazard_pointers_create();
    CLDS_HAZARD_POINTERS_THREAD_HANDLE hazard_pointers_thread = clds_hazard_pointers_register_thread(hazard_pointers);
    CLDS_SORTED_LIST_HANDLE list = clds_sorted_list_create(hazard_pointers, test_get_item_key, (void*)0x4242, test_key_compare, (void*)0x4243);
    CLDS_SORTED_LIST_ITEM* item = CLDS_SORTED_LIST_NODE_CREATE(TEST_ITEM, test_item_cleanup_func, (void*)0x4242);
    CLDS_SORTED_LIST_ITEM* removed_item;
    CLDS_SORTED_LIST_REMOVE_RESULT result;
    TEST_ITEM* item_payload = CLDS_SORTED_LIST_GET_VALUE(TEST_ITEM, item);
    item_payload->key = 0x42;
    (void)clds_hazard_pointers_set_reclaim_threshold(hazard_pointers, 1);
    (void)clds_sorted_list_insert(list, hazard_pointers_thread, item);
    umock_c_reset_all_calls();

    // act
    result = clds_sorted_list_remove_key(NULL, hazard_pointers_thread, (void*)0x42, &removed_item);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
    ASSERT_ARE_EQUAL(CLDS_SORTED_LIST_REMOVE_RESULT, CLDS_SORTED_LIST_DELETE_ERROR, result);

    // cleanup
    clds_sorted_list_destroy(list);
    clds_hazard_pointers_destroy(hazard_pointers);
}

/* Tests_SRS_CLDS_SORTED_LIST_01_055: [ If `clds_hazard_pointers_thread` is NULL, `clds_sorted_list_remove_key` shall fail and return `CLDS_SORTED_LIST_REMOVE_ERROR`. ]*/
TEST_FUNCTION(clds_sorted_list_remove_key_with_NULL_hazard_pointers_thread_fails)
{
    // arrange
    CLDS_HAZARD_POINTERS_HANDLE hazard_pointers = clds_hazard_pointers_create();
    CLDS_HAZARD_POINTERS_THREAD_HANDLE hazard_pointers_thread = clds_hazard_pointers_register_thread(hazard_pointers);
    CLDS_SORTED_LIST_HANDLE list = clds_sorted_list_create(hazard_pointers, test_get_item_key, (void*)0x4242, test_key_compare, (void*)0x4243);
    CLDS_SORTED_LIST_ITEM* item = CLDS_SORTED_LIST_NODE_CREATE(TEST_ITEM, test_item_cleanup_func, (void*)0x4242);
    CLDS_SORTED_LIST_ITEM* removed_item;
    CLDS_SORTED_LIST_REMOVE_RESULT result;
    TEST_ITEM* item_payload = CLDS_SORTED_LIST_GET_VALUE(TEST_ITEM, item);
    item_payload->key = 0x42;
    (void)clds_hazard_pointers_set_reclaim_threshold(hazard_pointers, 1);
    (void)clds_sorted_list_insert(list, hazard_pointers_thread, item);
    umock_c_reset_all_calls();

    // act
    result = clds_sorted_list_remove_key(list, NULL, (void*)0x42, &removed_item);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
    ASSERT_ARE_EQUAL(CLDS_SORTED_LIST_REMOVE_RESULT, CLDS_SORTED_LIST_REMOVE_ERROR, result);

    // cleanup
    clds_sorted_list_destroy(list);
    clds_hazard_pointers_destroy(hazard_pointers);
}

/* Tests_SRS_CLDS_SORTED_LIST_01_056: [ If `key` is NULL, `clds_sorted_list_remove_key` shall fail and return `CLDS_SORTED_LIST_REMOVE_ERROR`. ]*/
TEST_FUNCTION(clds_sorted_list_remove_key_with_NULL_key_fails)
{
    // arrange
    CLDS_HAZARD_POINTERS_HANDLE hazard_pointers = clds_hazard_pointers_create();
    CLDS_HAZARD_POINTERS_THREAD_HANDLE hazard_pointers_thread = clds_hazard_pointers_register_thread(hazard_pointers);
    CLDS_SORTED_LIST_HANDLE list = clds_sorted_list_create(hazard_pointers, test_get_item_key, (void*)0x4242, test_key_compare, (void*)0x4243);
    CLDS_SORTED_LIST_ITEM* item = CLDS_SORTED_LIST_NODE_CREATE(TEST_ITEM, test_item_cleanup_func, (void*)0x4242);
    CLDS_SORTED_LIST_ITEM* removed_item;
    CLDS_SORTED_LIST_REMOVE_RESULT result;
    TEST_ITEM* item_payload = CLDS_SORTED_LIST_GET_VALUE(TEST_ITEM, item);
    item_payload->key = 0x42;
    (void)clds_hazard_pointers_set_reclaim_threshold(hazard_pointers, 1);
    (void)clds_sorted_list_insert(list, hazard_pointers_thread, item);
    umock_c_reset_all_calls();

    // act
    result = clds_sorted_list_remove_key(list, hazard_pointers_thread, NULL, &removed_item);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
    ASSERT_ARE_EQUAL(CLDS_SORTED_LIST_REMOVE_RESULT, CLDS_SORTED_LIST_REMOVE_ERROR, result);

    // cleanup
    clds_sorted_list_destroy(list);
    clds_hazard_pointers_destroy(hazard_pointers);
}

/* Tests_SRS_CLDS_SORTED_LIST_01_051: [ `clds_sorted_list_remove_key` shall delete an remove an item by its key and return the pointer to it. ]*/
/* Tests_SRS_CLDS_SORTED_LIST_01_052: [ On success, `clds_sorted_list_remove_key` shall return `CLDS_SORTED_LIST_REMOVE_OK`. ]*/
/* Tests_SRS_CLDS_SORTED_LIST_01_054: [ On success, the found item shall be returned in the `item` argument. ]*/
/* Tests_SRS_CLDS_SORTED_LIST_01_043: [ The reclaim function passed to `clds_hazard_pointers_reclaim` shall call the user callback `item_cleanup_callback` that was passed to `clds_sorted_list_node_create`, while passing `item_cleanup_callback_context` and the freed item as arguments. ]*/
TEST_FUNCTION(clds_sorted_list_remove_key_deletes_the_second_item)
{
    // arrange
    CLDS_HAZARD_POINTERS_HANDLE hazard_pointers = clds_hazard_pointers_create();
    CLDS_HAZARD_POINTERS_THREAD_HANDLE hazard_pointers_thread = clds_hazard_pointers_register_thread(hazard_pointers);
    CLDS_SORTED_LIST_HANDLE list = clds_sorted_list_create(hazard_pointers, test_get_item_key, (void*)0x4242, test_key_compare, (void*)0x4243);
    CLDS_SORTED_LIST_ITEM* item_1 = CLDS_SORTED_LIST_NODE_CREATE(TEST_ITEM, test_item_cleanup_func, (void*)0x4242);
    CLDS_SORTED_LIST_ITEM* item_2 = CLDS_SORTED_LIST_NODE_CREATE(TEST_ITEM, test_item_cleanup_func, (void*)0x4242);
    CLDS_SORTED_LIST_ITEM* removed_item;
    TEST_ITEM* item_1_payload = CLDS_SORTED_LIST_GET_VALUE(TEST_ITEM, item_1);
    TEST_ITEM* item_2_payload = CLDS_SORTED_LIST_GET_VALUE(TEST_ITEM, item_2);
    item_1_payload->key = 0x42;
    item_2_payload->key = 0x43;
    CLDS_SORTED_LIST_REMOVE_RESULT result;
    (void)clds_hazard_pointers_set_reclaim_threshold(hazard_pointers, 1);
    (void)clds_sorted_list_insert(list, hazard_pointers_thread, item_1);
    (void)clds_sorted_list_insert(list, hazard_pointers_thread, item_2);
    umock_c_reset_all_calls();

    STRICT_EXPECTED_CALL(clds_hazard_pointers_acquire(IGNORED_PTR_ARG, IGNORED_PTR_ARG)).IgnoreAllCalls();
    STRICT_EXPECTED_CALL(clds_hazard_pointers_release(IGNORED_PTR_ARG, IGNORED_PTR_ARG)).IgnoreAllCalls();
    STRICT_EXPECTED_CALL(clds_st_hash_set_create(IGNORED_PTR_ARG, IGNORED_NUM_ARG)).IgnoreAllCalls();
    STRICT_EXPECTED_CALL(clds_st_hash_set_destroy(IGNORED_PTR_ARG)).IgnoreAllCalls();
    STRICT_EXPECTED_CALL(clds_st_hash_set_find(IGNORED_PTR_ARG, IGNORED_PTR_ARG, IGNORED_PTR_ARG)).IgnoreAllCalls();
    STRICT_EXPECTED_CALL(clds_hazard_pointers_reclaim(hazard_pointers_thread, item_2, IGNORED_PTR_ARG));

    // act
    result = clds_sorted_list_remove_key(list, hazard_pointers_thread, (void*)0x43, &removed_item);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
    ASSERT_ARE_EQUAL(CLDS_SORTED_LIST_REMOVE_RESULT, CLDS_SORTED_LIST_REMOVE_OK, result);

    // cleanup
    CLDS_SORTED_LIST_NODE_RELEASE(TEST_ITEM, removed_item);
    clds_sorted_list_destroy(list);
    clds_hazard_pointers_destroy(hazard_pointers);
}

/* Tests_SRS_CLDS_SORTED_LIST_01_051: [ `clds_sorted_list_remove_key` shall delete an remove an item by its key and return the pointer to it. ]*/
/* Tests_SRS_CLDS_SORTED_LIST_01_052: [ On success, `clds_sorted_list_remove_key` shall return `CLDS_SORTED_LIST_REMOVE_OK`. ]*/
/* Tests_SRS_CLDS_SORTED_LIST_01_054: [ On success, the found item shall be returned in the `item` argument. ]*/
/* Tests_SRS_CLDS_SORTED_LIST_01_043: [ The reclaim function passed to `clds_hazard_pointers_reclaim` shall call the user callback `item_cleanup_callback` that was passed to `clds_sorted_list_node_create`, while passing `item_cleanup_callback_context` and the freed item as arguments. ]*/
TEST_FUNCTION(clds_sorted_list_remove_key_deletes_the_oldest_out_of_2_items)
{
    // arrange
    CLDS_HAZARD_POINTERS_HANDLE hazard_pointers = clds_hazard_pointers_create();
    CLDS_HAZARD_POINTERS_THREAD_HANDLE hazard_pointers_thread = clds_hazard_pointers_register_thread(hazard_pointers);
    CLDS_SORTED_LIST_HANDLE list = clds_sorted_list_create(hazard_pointers, test_get_item_key, (void*)0x4242, test_key_compare, (void*)0x4243);
    CLDS_SORTED_LIST_ITEM* item_1 = CLDS_SORTED_LIST_NODE_CREATE(TEST_ITEM, test_item_cleanup_func, (void*)0x4242);
    CLDS_SORTED_LIST_ITEM* item_2 = CLDS_SORTED_LIST_NODE_CREATE(TEST_ITEM, test_item_cleanup_func, (void*)0x4242);
    CLDS_SORTED_LIST_REMOVE_RESULT result;
    CLDS_SORTED_LIST_ITEM* removed_item;
    TEST_ITEM* item_1_payload = CLDS_SORTED_LIST_GET_VALUE(TEST_ITEM, item_1);
    TEST_ITEM* item_2_payload = CLDS_SORTED_LIST_GET_VALUE(TEST_ITEM, item_2);
    item_1_payload->key = 0x42;
    item_2_payload->key = 0x43;
    (void)clds_hazard_pointers_set_reclaim_threshold(hazard_pointers, 1);
    (void)clds_sorted_list_insert(list, hazard_pointers_thread, item_1);
    (void)clds_sorted_list_insert(list, hazard_pointers_thread, item_2);
    umock_c_reset_all_calls();

    STRICT_EXPECTED_CALL(clds_hazard_pointers_acquire(IGNORED_PTR_ARG, IGNORED_PTR_ARG)).IgnoreAllCalls();
    STRICT_EXPECTED_CALL(clds_hazard_pointers_release(IGNORED_PTR_ARG, IGNORED_PTR_ARG)).IgnoreAllCalls();
    STRICT_EXPECTED_CALL(clds_st_hash_set_create(IGNORED_PTR_ARG, IGNORED_NUM_ARG)).IgnoreAllCalls();
    STRICT_EXPECTED_CALL(clds_st_hash_set_destroy(IGNORED_PTR_ARG)).IgnoreAllCalls();
    STRICT_EXPECTED_CALL(clds_st_hash_set_find(IGNORED_PTR_ARG, IGNORED_PTR_ARG, IGNORED_PTR_ARG)).IgnoreAllCalls();
    STRICT_EXPECTED_CALL(clds_hazard_pointers_reclaim(hazard_pointers_thread, item_1, IGNORED_PTR_ARG));
    
    // act
    result = clds_sorted_list_remove_key(list, hazard_pointers_thread, (void*)0x42, &removed_item);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
    ASSERT_ARE_EQUAL(CLDS_SORTED_LIST_REMOVE_RESULT, CLDS_SORTED_LIST_REMOVE_OK, result);

    // cleanup
    CLDS_SORTED_LIST_NODE_RELEASE(TEST_ITEM, removed_item);
    clds_sorted_list_destroy(list);
    clds_hazard_pointers_destroy(hazard_pointers);
}

/* Tests_SRS_CLDS_SORTED_LIST_01_051: [ `clds_sorted_list_remove_key` shall delete an remove an item by its key and return the pointer to it. ]*/
/* Tests_SRS_CLDS_SORTED_LIST_01_052: [ On success, `clds_sorted_list_remove_key` shall return `CLDS_SORTED_LIST_REMOVE_OK`. ]*/
/* Tests_SRS_CLDS_SORTED_LIST_01_054: [ On success, the found item shall be returned in the `item` argument. ]*/
/* Tests_SRS_CLDS_SORTED_LIST_01_043: [ The reclaim function passed to `clds_hazard_pointers_reclaim` shall call the user callback `item_cleanup_callback` that was passed to `clds_sorted_list_node_create`, while passing `item_cleanup_callback_context` and the freed item as arguments. ]*/
TEST_FUNCTION(clds_sorted_list_remove_key_deletes_the_2nd_out_of_3_items)
{
    // arrange
    CLDS_HAZARD_POINTERS_HANDLE hazard_pointers = clds_hazard_pointers_create();
    CLDS_HAZARD_POINTERS_THREAD_HANDLE hazard_pointers_thread = clds_hazard_pointers_register_thread(hazard_pointers);
    CLDS_SORTED_LIST_HANDLE list = clds_sorted_list_create(hazard_pointers, test_get_item_key, (void*)0x4242, test_key_compare, (void*)0x4243);
    CLDS_SORTED_LIST_ITEM* item_1 = CLDS_SORTED_LIST_NODE_CREATE(TEST_ITEM, test_item_cleanup_func, (void*)0x4242);
    CLDS_SORTED_LIST_ITEM* item_2 = CLDS_SORTED_LIST_NODE_CREATE(TEST_ITEM, test_item_cleanup_func, (void*)0x4242);
    CLDS_SORTED_LIST_ITEM* item_3 = CLDS_SORTED_LIST_NODE_CREATE(TEST_ITEM, test_item_cleanup_func, (void*)0x4242);
    CLDS_SORTED_LIST_REMOVE_RESULT result;
    CLDS_SORTED_LIST_ITEM* removed_item;
    TEST_ITEM* item_1_payload = CLDS_SORTED_LIST_GET_VALUE(TEST_ITEM, item_1);
    TEST_ITEM* item_2_payload = CLDS_SORTED_LIST_GET_VALUE(TEST_ITEM, item_2);
    TEST_ITEM* item_3_payload = CLDS_SORTED_LIST_GET_VALUE(TEST_ITEM, item_3);
    item_1_payload->key = 0x42;
    item_2_payload->key = 0x43;
    item_3_payload->key = 0x44;
    (void)clds_hazard_pointers_set_reclaim_threshold(hazard_pointers, 1);
    (void)clds_sorted_list_insert(list, hazard_pointers_thread, item_1);
    (void)clds_sorted_list_insert(list, hazard_pointers_thread, item_2);
    (void)clds_sorted_list_insert(list, hazard_pointers_thread, item_3);
    umock_c_reset_all_calls();

    STRICT_EXPECTED_CALL(clds_hazard_pointers_acquire(IGNORED_PTR_ARG, IGNORED_PTR_ARG)).IgnoreAllCalls();
    STRICT_EXPECTED_CALL(clds_hazard_pointers_release(IGNORED_PTR_ARG, IGNORED_PTR_ARG)).IgnoreAllCalls();
    STRICT_EXPECTED_CALL(clds_st_hash_set_create(IGNORED_PTR_ARG, IGNORED_NUM_ARG)).IgnoreAllCalls();
    STRICT_EXPECTED_CALL(clds_st_hash_set_destroy(IGNORED_PTR_ARG)).IgnoreAllCalls();
    STRICT_EXPECTED_CALL(clds_st_hash_set_find(IGNORED_PTR_ARG, IGNORED_PTR_ARG, IGNORED_PTR_ARG)).IgnoreAllCalls();
    STRICT_EXPECTED_CALL(clds_hazard_pointers_reclaim(hazard_pointers_thread, item_2, IGNORED_PTR_ARG));

    // act
    result = clds_sorted_list_remove_key(list, hazard_pointers_thread, (void*)0x43, &removed_item);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
    ASSERT_ARE_EQUAL(CLDS_SORTED_LIST_REMOVE_RESULT, CLDS_SORTED_LIST_REMOVE_OK, result);

    // cleanup
    CLDS_SORTED_LIST_NODE_RELEASE(TEST_ITEM, removed_item);
    clds_sorted_list_destroy(list);
    clds_hazard_pointers_destroy(hazard_pointers);
}

/* Tests_SRS_CLDS_SORTED_LIST_01_057: [ If the key is not found, `clds_sorted_list_remove_key` shall fail and return `CLDS_SORTED_LIST_REMOVE_NOT_FOUND`. ]*/
TEST_FUNCTION(clds_sorted_list_remove_key_on_an_empty_list_yields_NOT_FOUND)
{
    // arrange
    CLDS_HAZARD_POINTERS_HANDLE hazard_pointers = clds_hazard_pointers_create();
    CLDS_HAZARD_POINTERS_THREAD_HANDLE hazard_pointers_thread = clds_hazard_pointers_register_thread(hazard_pointers);
    CLDS_SORTED_LIST_HANDLE list = clds_sorted_list_create(hazard_pointers, test_get_item_key, (void*)0x4242, test_key_compare, (void*)0x4243);
    CLDS_SORTED_LIST_REMOVE_RESULT result;
    CLDS_SORTED_LIST_ITEM* removed_item;
    (void)clds_hazard_pointers_set_reclaim_threshold(hazard_pointers, 1);
    umock_c_reset_all_calls();

    STRICT_EXPECTED_CALL(clds_hazard_pointers_acquire(IGNORED_PTR_ARG, IGNORED_PTR_ARG)).IgnoreAllCalls();
    STRICT_EXPECTED_CALL(clds_hazard_pointers_release(IGNORED_PTR_ARG, IGNORED_PTR_ARG)).IgnoreAllCalls();
    STRICT_EXPECTED_CALL(clds_st_hash_set_create(IGNORED_PTR_ARG, IGNORED_NUM_ARG)).IgnoreAllCalls();
    STRICT_EXPECTED_CALL(clds_st_hash_set_destroy(IGNORED_PTR_ARG)).IgnoreAllCalls();
    STRICT_EXPECTED_CALL(clds_st_hash_set_find(IGNORED_PTR_ARG, IGNORED_PTR_ARG, IGNORED_PTR_ARG)).IgnoreAllCalls();

    // act
    result = clds_sorted_list_remove_key(list, hazard_pointers_thread, (void*)0x42, &removed_item);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
    ASSERT_ARE_EQUAL(CLDS_SORTED_LIST_REMOVE_RESULT, CLDS_SORTED_LIST_REMOVE_NOT_FOUND, result);

    // cleanup
    clds_sorted_list_destroy(list);
    clds_hazard_pointers_destroy(hazard_pointers);
}

/* Tests_SRS_CLDS_SORTED_LIST_01_057: [ If the key is not found, `clds_sorted_list_remove_key` shall fail and return `CLDS_SORTED_LIST_REMOVE_NOT_FOUND`. ]*/
TEST_FUNCTION(clds_sorted_list_remove_key_after_the_item_was_deleted_yields_NOT_FOUND)
{
    // arrange
    CLDS_HAZARD_POINTERS_HANDLE hazard_pointers = clds_hazard_pointers_create();
    CLDS_HAZARD_POINTERS_THREAD_HANDLE hazard_pointers_thread = clds_hazard_pointers_register_thread(hazard_pointers);
    CLDS_SORTED_LIST_HANDLE list = clds_sorted_list_create(hazard_pointers, test_get_item_key, (void*)0x4242, test_key_compare, (void*)0x4243);
    CLDS_SORTED_LIST_ITEM* item = CLDS_SORTED_LIST_NODE_CREATE(TEST_ITEM, test_item_cleanup_func, (void*)0x4242);
    CLDS_SORTED_LIST_REMOVE_RESULT result;
    CLDS_SORTED_LIST_ITEM* removed_item;
    TEST_ITEM* item_payload = CLDS_SORTED_LIST_GET_VALUE(TEST_ITEM, item);
    item_payload->key = 0x42;
    (void)clds_hazard_pointers_set_reclaim_threshold(hazard_pointers, 1);
    (void)clds_sorted_list_insert(list, hazard_pointers_thread, item);
    (void)clds_sorted_list_delete_item(list, hazard_pointers_thread, item);
    umock_c_reset_all_calls();

    STRICT_EXPECTED_CALL(clds_hazard_pointers_acquire(IGNORED_PTR_ARG, IGNORED_PTR_ARG)).IgnoreAllCalls();
    STRICT_EXPECTED_CALL(clds_hazard_pointers_release(IGNORED_PTR_ARG, IGNORED_PTR_ARG)).IgnoreAllCalls();
    STRICT_EXPECTED_CALL(clds_st_hash_set_create(IGNORED_PTR_ARG, IGNORED_NUM_ARG)).IgnoreAllCalls();
    STRICT_EXPECTED_CALL(clds_st_hash_set_destroy(IGNORED_PTR_ARG)).IgnoreAllCalls();
    STRICT_EXPECTED_CALL(clds_st_hash_set_find(IGNORED_PTR_ARG, IGNORED_PTR_ARG, IGNORED_PTR_ARG)).IgnoreAllCalls();

    // act
    result = clds_sorted_list_remove_key(list, hazard_pointers_thread, (void*)0x43, &removed_item);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
    ASSERT_ARE_EQUAL(CLDS_SORTED_LIST_REMOVE_RESULT, CLDS_SORTED_LIST_REMOVE_NOT_FOUND, result);

    // cleanup
    clds_sorted_list_destroy(list);
    clds_hazard_pointers_destroy(hazard_pointers);
}

/* clds_sorted_list_find_key */

/* Tests_SRS_CLDS_SORTED_LIST_01_027: [ `clds_sorted_list_find_key` shall find in the list the first item that matches the criteria given by a user compare function. ]*/
/* Tests_SRS_CLDS_SORTED_LIST_01_029: [ On success `clds_sorted_list_find_key` shall return a non-NULL pointer to the found linked list item. ]*/
TEST_FUNCTION(clds_sorted_list_find_key_succeeds_in_finding_an_item)
{
    // arrange
    CLDS_HAZARD_POINTERS_HANDLE hazard_pointers = clds_hazard_pointers_create();
    CLDS_HAZARD_POINTERS_THREAD_HANDLE hazard_pointers_thread = clds_hazard_pointers_register_thread(hazard_pointers);
    CLDS_SORTED_LIST_HANDLE list = clds_sorted_list_create(hazard_pointers, test_get_item_key, (void*)0x4242, test_key_compare, (void*)0x4243);
    CLDS_SORTED_LIST_ITEM* item = CLDS_SORTED_LIST_NODE_CREATE(TEST_ITEM, test_item_cleanup_func, (void*)0x4242);
    CLDS_SORTED_LIST_ITEM* result;
    TEST_ITEM* item_payload = CLDS_SORTED_LIST_GET_VALUE(TEST_ITEM, item);
    item_payload->key = 0x42;
    (void)clds_hazard_pointers_set_reclaim_threshold(hazard_pointers, 1);
    (void)clds_sorted_list_insert(list, hazard_pointers_thread, item);
    umock_c_reset_all_calls();

    STRICT_EXPECTED_CALL(clds_hazard_pointers_acquire(IGNORED_PTR_ARG, IGNORED_PTR_ARG)).IgnoreAllCalls();
    STRICT_EXPECTED_CALL(clds_hazard_pointers_release(IGNORED_PTR_ARG, IGNORED_PTR_ARG)).IgnoreAllCalls();
    STRICT_EXPECTED_CALL(clds_st_hash_set_create(IGNORED_PTR_ARG, IGNORED_NUM_ARG)).IgnoreAllCalls();
    STRICT_EXPECTED_CALL(clds_st_hash_set_destroy(IGNORED_PTR_ARG)).IgnoreAllCalls();
    STRICT_EXPECTED_CALL(clds_st_hash_set_find(IGNORED_PTR_ARG, IGNORED_PTR_ARG, IGNORED_PTR_ARG)).IgnoreAllCalls();

    // act
    result = clds_sorted_list_find_key(list, hazard_pointers_thread, (void*)0x42);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
    ASSERT_ARE_EQUAL(void_ptr, item, result);

    // cleanup
    clds_sorted_list_destroy(list);
    clds_hazard_pointers_destroy(hazard_pointers);
}

/* Tests_SRS_CLDS_SORTED_LIST_01_027: [ `clds_sorted_list_find_key` shall find in the list the first item that matches the criteria given by a user compare function. ]*/
/* Tests_SRS_CLDS_SORTED_LIST_01_029: [ On success `clds_sorted_list_find_key` shall return a non-NULL pointer to the found linked list item. ]*/
TEST_FUNCTION(clds_sorted_list_find_key_finds_the_2nd_out_of_3_added_items)
{
    // arrange
    CLDS_HAZARD_POINTERS_HANDLE hazard_pointers = clds_hazard_pointers_create();
    CLDS_HAZARD_POINTERS_THREAD_HANDLE hazard_pointers_thread = clds_hazard_pointers_register_thread(hazard_pointers);
    CLDS_SORTED_LIST_HANDLE list = clds_sorted_list_create(hazard_pointers, test_get_item_key, (void*)0x4242, test_key_compare, (void*)0x4243);
    CLDS_SORTED_LIST_ITEM* item_1 = CLDS_SORTED_LIST_NODE_CREATE(TEST_ITEM, test_item_cleanup_func, (void*)0x4242);
    CLDS_SORTED_LIST_ITEM* item_2 = CLDS_SORTED_LIST_NODE_CREATE(TEST_ITEM, test_item_cleanup_func, (void*)0x4242);
    CLDS_SORTED_LIST_ITEM* item_3 = CLDS_SORTED_LIST_NODE_CREATE(TEST_ITEM, test_item_cleanup_func, (void*)0x4242);
    CLDS_SORTED_LIST_ITEM* result;
    TEST_ITEM* item_1_payload = CLDS_SORTED_LIST_GET_VALUE(TEST_ITEM, item_1);
    TEST_ITEM* item_2_payload = CLDS_SORTED_LIST_GET_VALUE(TEST_ITEM, item_2);
    TEST_ITEM* item_3_payload = CLDS_SORTED_LIST_GET_VALUE(TEST_ITEM, item_3);
    item_1_payload->key = 0x42;
    item_2_payload->key = 0x43;
    item_3_payload->key = 0x44;
    (void)clds_hazard_pointers_set_reclaim_threshold(hazard_pointers, 1);
    (void)clds_sorted_list_insert(list, hazard_pointers_thread, item_1);
    (void)clds_sorted_list_insert(list, hazard_pointers_thread, item_2);
    (void)clds_sorted_list_insert(list, hazard_pointers_thread, item_3);
    umock_c_reset_all_calls();

    STRICT_EXPECTED_CALL(clds_hazard_pointers_acquire(IGNORED_PTR_ARG, IGNORED_PTR_ARG)).IgnoreAllCalls();
    STRICT_EXPECTED_CALL(clds_hazard_pointers_release(IGNORED_PTR_ARG, IGNORED_PTR_ARG)).IgnoreAllCalls();
    STRICT_EXPECTED_CALL(clds_st_hash_set_create(IGNORED_PTR_ARG, IGNORED_NUM_ARG)).IgnoreAllCalls();
    STRICT_EXPECTED_CALL(clds_st_hash_set_destroy(IGNORED_PTR_ARG)).IgnoreAllCalls();
    STRICT_EXPECTED_CALL(clds_st_hash_set_find(IGNORED_PTR_ARG, IGNORED_PTR_ARG, IGNORED_PTR_ARG)).IgnoreAllCalls();

    // act
    result = clds_sorted_list_find_key(list, hazard_pointers_thread, (void*)0x43);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
    ASSERT_ARE_EQUAL(void_ptr, item_2, result);

    // cleanup
    clds_sorted_list_destroy(list);
    clds_hazard_pointers_destroy(hazard_pointers);
}

/* Tests_SRS_CLDS_SORTED_LIST_01_027: [ `clds_sorted_list_find_key` shall find in the list the first item that matches the criteria given by a user compare function. ]*/
/* Tests_SRS_CLDS_SORTED_LIST_01_029: [ On success `clds_sorted_list_find_key` shall return a non-NULL pointer to the found linked list item. ]*/
TEST_FUNCTION(clds_sorted_list_find_key_finds_the_last_added_item)
{
    // arrange
    CLDS_HAZARD_POINTERS_HANDLE hazard_pointers = clds_hazard_pointers_create();
    CLDS_HAZARD_POINTERS_THREAD_HANDLE hazard_pointers_thread = clds_hazard_pointers_register_thread(hazard_pointers);
    CLDS_SORTED_LIST_HANDLE list = clds_sorted_list_create(hazard_pointers, test_get_item_key, (void*)0x4242, test_key_compare, (void*)0x4243);
    CLDS_SORTED_LIST_ITEM* item_1 = CLDS_SORTED_LIST_NODE_CREATE(TEST_ITEM, test_item_cleanup_func, (void*)0x4242);
    CLDS_SORTED_LIST_ITEM* item_2 = CLDS_SORTED_LIST_NODE_CREATE(TEST_ITEM, test_item_cleanup_func, (void*)0x4242);
    CLDS_SORTED_LIST_ITEM* item_3 = CLDS_SORTED_LIST_NODE_CREATE(TEST_ITEM, test_item_cleanup_func, (void*)0x4242);
    CLDS_SORTED_LIST_ITEM* result;
    TEST_ITEM* item_1_payload = CLDS_SORTED_LIST_GET_VALUE(TEST_ITEM, item_1);
    TEST_ITEM* item_2_payload = CLDS_SORTED_LIST_GET_VALUE(TEST_ITEM, item_2);
    TEST_ITEM* item_3_payload = CLDS_SORTED_LIST_GET_VALUE(TEST_ITEM, item_3);
    item_1_payload->key = 0x42;
    item_2_payload->key = 0x43;
    item_3_payload->key = 0x44;
    (void)clds_hazard_pointers_set_reclaim_threshold(hazard_pointers, 1);
    (void)clds_sorted_list_insert(list, hazard_pointers_thread, item_1);
    (void)clds_sorted_list_insert(list, hazard_pointers_thread, item_2);
    (void)clds_sorted_list_insert(list, hazard_pointers_thread, item_3);
    umock_c_reset_all_calls();

    STRICT_EXPECTED_CALL(clds_hazard_pointers_acquire(IGNORED_PTR_ARG, IGNORED_PTR_ARG)).IgnoreAllCalls();
    STRICT_EXPECTED_CALL(clds_hazard_pointers_release(IGNORED_PTR_ARG, IGNORED_PTR_ARG)).IgnoreAllCalls();
    STRICT_EXPECTED_CALL(clds_st_hash_set_create(IGNORED_PTR_ARG, IGNORED_NUM_ARG)).IgnoreAllCalls();
    STRICT_EXPECTED_CALL(clds_st_hash_set_destroy(IGNORED_PTR_ARG)).IgnoreAllCalls();
    STRICT_EXPECTED_CALL(clds_st_hash_set_find(IGNORED_PTR_ARG, IGNORED_PTR_ARG, IGNORED_PTR_ARG)).IgnoreAllCalls();

    // act
    result = clds_sorted_list_find_key(list, hazard_pointers_thread, (void*)0x44);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
    ASSERT_ARE_EQUAL(void_ptr, item_3, result);

    // cleanup
    clds_sorted_list_destroy(list);
    clds_hazard_pointers_destroy(hazard_pointers);
}

/* Tests_SRS_CLDS_SORTED_LIST_01_028: [ If `clds_sorted_list` is NULL, `clds_sorted_list_find_key` shall fail and return NULL. ]*/
TEST_FUNCTION(clds_sorted_list_find_key_with_NULL_clds_sorted_list_fails)
{
    // arrange
    CLDS_HAZARD_POINTERS_HANDLE hazard_pointers = clds_hazard_pointers_create();
    CLDS_HAZARD_POINTERS_THREAD_HANDLE hazard_pointers_thread = clds_hazard_pointers_register_thread(hazard_pointers);
    CLDS_SORTED_LIST_HANDLE list = clds_sorted_list_create(hazard_pointers, test_get_item_key, (void*)0x4242, test_key_compare, (void*)0x4243);
    CLDS_SORTED_LIST_ITEM* item = CLDS_SORTED_LIST_NODE_CREATE(TEST_ITEM, test_item_cleanup_func, (void*)0x4242);
    CLDS_SORTED_LIST_ITEM* result;
    TEST_ITEM* item_payload = CLDS_SORTED_LIST_GET_VALUE(TEST_ITEM, item);
    item_payload->key = 0x42;
    (void)clds_hazard_pointers_set_reclaim_threshold(hazard_pointers, 1);
    (void)clds_sorted_list_insert(list, hazard_pointers_thread, item);
    umock_c_reset_all_calls();

    // act
    result = clds_sorted_list_find_key(NULL, hazard_pointers_thread, (void*)0x42);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
    ASSERT_IS_NULL(result);

    // cleanup
    clds_sorted_list_destroy(list);
    clds_hazard_pointers_destroy(hazard_pointers);
}

/* Tests_SRS_CLDS_SORTED_LIST_01_030: [ If `clds_hazard_pointers_thread` is NULL, `clds_sorted_list_find_key` shall fail and return NULL. ]*/
TEST_FUNCTION(clds_sorted_list_find_key_with_NULL_hazard_pointers_thread_fails)
{
    // arrange
    CLDS_HAZARD_POINTERS_HANDLE hazard_pointers = clds_hazard_pointers_create();
    CLDS_HAZARD_POINTERS_THREAD_HANDLE hazard_pointers_thread = clds_hazard_pointers_register_thread(hazard_pointers);
    CLDS_SORTED_LIST_HANDLE list = clds_sorted_list_create(hazard_pointers, test_get_item_key, (void*)0x4242, test_key_compare, (void*)0x4243);
    CLDS_SORTED_LIST_ITEM* item = CLDS_SORTED_LIST_NODE_CREATE(TEST_ITEM, test_item_cleanup_func, (void*)0x4242);
    CLDS_SORTED_LIST_ITEM* result;
    TEST_ITEM* item_payload = CLDS_SORTED_LIST_GET_VALUE(TEST_ITEM, item);
    item_payload->key = 0x42;
    (void)clds_hazard_pointers_set_reclaim_threshold(hazard_pointers, 1);
    (void)clds_sorted_list_insert(list, hazard_pointers_thread, item);
    umock_c_reset_all_calls();

    // act
    result = clds_sorted_list_find_key(list, NULL, (void*)0x42);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
    ASSERT_IS_NULL(result);

    // cleanup
    clds_sorted_list_destroy(list);
    clds_hazard_pointers_destroy(hazard_pointers);
}

/* Tests_SRS_CLDS_SORTED_LIST_01_031: [ If `key` is NULL, `clds_sorted_list_find_key` shall fail and return NULL. ]*/
TEST_FUNCTION(clds_sorted_list_find_key_with_NULL_key_fails)
{
    // arrange
    CLDS_HAZARD_POINTERS_HANDLE hazard_pointers = clds_hazard_pointers_create();
    CLDS_HAZARD_POINTERS_THREAD_HANDLE hazard_pointers_thread = clds_hazard_pointers_register_thread(hazard_pointers);
    CLDS_SORTED_LIST_HANDLE list = clds_sorted_list_create(hazard_pointers, test_get_item_key, (void*)0x4242, test_key_compare, (void*)0x4243);
    CLDS_SORTED_LIST_ITEM* item = CLDS_SORTED_LIST_NODE_CREATE(TEST_ITEM, test_item_cleanup_func, (void*)0x4242);
    CLDS_SORTED_LIST_ITEM* result;
    TEST_ITEM* item_payload = CLDS_SORTED_LIST_GET_VALUE(TEST_ITEM, item);
    item_payload->key = 0x42;
    (void)clds_hazard_pointers_set_reclaim_threshold(hazard_pointers, 1);
    (void)clds_sorted_list_insert(list, hazard_pointers_thread, item);
    umock_c_reset_all_calls();

    // act
    result = clds_sorted_list_find_key(list, hazard_pointers_thread, NULL);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
    ASSERT_IS_NULL(result);

    // cleanup
    clds_sorted_list_destroy(list);
    clds_hazard_pointers_destroy(hazard_pointers);
}

/* Tests_SRS_CLDS_SORTED_LIST_01_033: [ If no item satisfying the user compare function is found in the list, `clds_sorted_list_find_key` shall fail and return NULL. ]*/
TEST_FUNCTION(clds_sorted_list_find_key_on_an_empty_list_returns_NULL)
{
    // arrange
    CLDS_HAZARD_POINTERS_HANDLE hazard_pointers = clds_hazard_pointers_create();
    CLDS_HAZARD_POINTERS_THREAD_HANDLE hazard_pointers_thread = clds_hazard_pointers_register_thread(hazard_pointers);
    CLDS_SORTED_LIST_HANDLE list = clds_sorted_list_create(hazard_pointers, test_get_item_key, (void*)0x4242, test_key_compare, (void*)0x4243);
    CLDS_SORTED_LIST_ITEM* result;
    (void)clds_hazard_pointers_set_reclaim_threshold(hazard_pointers, 1);
    umock_c_reset_all_calls();

    STRICT_EXPECTED_CALL(clds_hazard_pointers_acquire(IGNORED_PTR_ARG, IGNORED_PTR_ARG)).IgnoreAllCalls();
    STRICT_EXPECTED_CALL(clds_hazard_pointers_release(IGNORED_PTR_ARG, IGNORED_PTR_ARG)).IgnoreAllCalls();
    STRICT_EXPECTED_CALL(clds_st_hash_set_create(IGNORED_PTR_ARG, IGNORED_NUM_ARG)).IgnoreAllCalls();
    STRICT_EXPECTED_CALL(clds_st_hash_set_destroy(IGNORED_PTR_ARG)).IgnoreAllCalls();
    STRICT_EXPECTED_CALL(clds_st_hash_set_find(IGNORED_PTR_ARG, IGNORED_PTR_ARG, IGNORED_PTR_ARG)).IgnoreAllCalls();

    // act
    result = clds_sorted_list_find_key(list, hazard_pointers_thread, (void*)0x42);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
    ASSERT_IS_NULL(result);

    // cleanup
    clds_sorted_list_destroy(list);
    clds_hazard_pointers_destroy(hazard_pointers);
}

/* Tests_SRS_CLDS_SORTED_LIST_01_033: [ If no item satisfying the user compare function is found in the list, `clds_sorted_list_find_key` shall fail and return NULL. ]*/
TEST_FUNCTION(clds_sorted_list_find_key_after_the_item_was_deleted_returns_NULL)
{
    // arrange
    CLDS_HAZARD_POINTERS_HANDLE hazard_pointers = clds_hazard_pointers_create();
    CLDS_HAZARD_POINTERS_THREAD_HANDLE hazard_pointers_thread = clds_hazard_pointers_register_thread(hazard_pointers);
    CLDS_SORTED_LIST_HANDLE list = clds_sorted_list_create(hazard_pointers, test_get_item_key, (void*)0x4242, test_key_compare, (void*)0x4243);
    CLDS_SORTED_LIST_ITEM* item = CLDS_SORTED_LIST_NODE_CREATE(TEST_ITEM, test_item_cleanup_func, (void*)0x4242);
    CLDS_SORTED_LIST_ITEM* result;
    TEST_ITEM* item_payload = CLDS_SORTED_LIST_GET_VALUE(TEST_ITEM, item);
    item_payload->key = 0x42;
    (void)clds_hazard_pointers_set_reclaim_threshold(hazard_pointers, 1);
    (void)clds_sorted_list_insert(list, hazard_pointers_thread, item);
    (void)clds_sorted_list_delete_item(list, hazard_pointers_thread, item);
    umock_c_reset_all_calls();

    STRICT_EXPECTED_CALL(clds_hazard_pointers_acquire(IGNORED_PTR_ARG, IGNORED_PTR_ARG)).IgnoreAllCalls();
    STRICT_EXPECTED_CALL(clds_hazard_pointers_release(IGNORED_PTR_ARG, IGNORED_PTR_ARG)).IgnoreAllCalls();
    STRICT_EXPECTED_CALL(clds_st_hash_set_create(IGNORED_PTR_ARG, IGNORED_NUM_ARG)).IgnoreAllCalls();
    STRICT_EXPECTED_CALL(clds_st_hash_set_destroy(IGNORED_PTR_ARG)).IgnoreAllCalls();
    STRICT_EXPECTED_CALL(clds_st_hash_set_find(IGNORED_PTR_ARG, IGNORED_PTR_ARG, IGNORED_PTR_ARG)).IgnoreAllCalls();

    // act
    result = clds_sorted_list_find_key(list, hazard_pointers_thread, (void*)0x42);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
    ASSERT_IS_NULL(result);

    // cleanup
    clds_sorted_list_destroy(list);
    clds_hazard_pointers_destroy(hazard_pointers);
}

/* Tests_SRS_CLDS_SORTED_LIST_01_033: [ If no item satisfying the user compare function is found in the list, `clds_sorted_list_find_key` shall fail and return NULL. ]*/
TEST_FUNCTION(clds_sorted_list_find_key_when_the_item_is_not_in_the_list_returns_NULL)
{
    // arrange
    CLDS_HAZARD_POINTERS_HANDLE hazard_pointers = clds_hazard_pointers_create();
    CLDS_HAZARD_POINTERS_THREAD_HANDLE hazard_pointers_thread = clds_hazard_pointers_register_thread(hazard_pointers);
    CLDS_SORTED_LIST_HANDLE list = clds_sorted_list_create(hazard_pointers, test_get_item_key, (void*)0x4242, test_key_compare, (void*)0x4243);
    CLDS_SORTED_LIST_ITEM* item_1 = CLDS_SORTED_LIST_NODE_CREATE(TEST_ITEM, test_item_cleanup_func, (void*)0x4242);
    CLDS_SORTED_LIST_ITEM* item_2 = CLDS_SORTED_LIST_NODE_CREATE(TEST_ITEM, test_item_cleanup_func, (void*)0x4242);
    CLDS_SORTED_LIST_ITEM* item_3 = CLDS_SORTED_LIST_NODE_CREATE(TEST_ITEM, test_item_cleanup_func, (void*)0x4242);
    CLDS_SORTED_LIST_ITEM* result;
    TEST_ITEM* item_1_payload = CLDS_SORTED_LIST_GET_VALUE(TEST_ITEM, item_1);
    TEST_ITEM* item_2_payload = CLDS_SORTED_LIST_GET_VALUE(TEST_ITEM, item_2);
    TEST_ITEM* item_3_payload = CLDS_SORTED_LIST_GET_VALUE(TEST_ITEM, item_3);
    item_1_payload->key = 0x42;
    item_2_payload->key = 0x43;
    item_3_payload->key = 0x44;
    (void)clds_hazard_pointers_set_reclaim_threshold(hazard_pointers, 1);
    (void)clds_sorted_list_insert(list, hazard_pointers_thread, item_2);
    (void)clds_sorted_list_insert(list, hazard_pointers_thread, item_3);
    umock_c_reset_all_calls();

    STRICT_EXPECTED_CALL(clds_hazard_pointers_acquire(IGNORED_PTR_ARG, IGNORED_PTR_ARG)).IgnoreAllCalls();
    STRICT_EXPECTED_CALL(clds_hazard_pointers_release(IGNORED_PTR_ARG, IGNORED_PTR_ARG)).IgnoreAllCalls();
    STRICT_EXPECTED_CALL(clds_st_hash_set_create(IGNORED_PTR_ARG, IGNORED_NUM_ARG)).IgnoreAllCalls();
    STRICT_EXPECTED_CALL(clds_st_hash_set_destroy(IGNORED_PTR_ARG)).IgnoreAllCalls();
    STRICT_EXPECTED_CALL(clds_st_hash_set_find(IGNORED_PTR_ARG, IGNORED_PTR_ARG, IGNORED_PTR_ARG)).IgnoreAllCalls();

    // act
    result = clds_sorted_list_find_key(list, hazard_pointers_thread, (void*)0x42);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
    ASSERT_IS_NULL(result);

    // cleanup
    CLDS_SORTED_LIST_NODE_RELEASE(TEST_ITEM, item_1);
    clds_sorted_list_destroy(list);
    clds_hazard_pointers_destroy(hazard_pointers);
}

/* Tests_SRS_CLDS_SORTED_LIST_01_034: [ `clds_sorted_list_find_key` shall return a pointer to the item with the reference count already incremented so that it can be safely used by the caller. ]*/
TEST_FUNCTION(clds_sorted_list_find_key_result_has_the_ref_count_incremented)
{
    // arrange
    CLDS_HAZARD_POINTERS_HANDLE hazard_pointers = clds_hazard_pointers_create();
    CLDS_HAZARD_POINTERS_THREAD_HANDLE hazard_pointers_thread = clds_hazard_pointers_register_thread(hazard_pointers);
    CLDS_SORTED_LIST_HANDLE list = clds_sorted_list_create(hazard_pointers, test_get_item_key, (void*)0x4242, test_key_compare, (void*)0x4243);
    CLDS_SORTED_LIST_ITEM* item = CLDS_SORTED_LIST_NODE_CREATE(TEST_ITEM, test_item_cleanup_func, (void*)0x4242);
    CLDS_SORTED_LIST_ITEM* result;
    TEST_ITEM* item_payload = CLDS_SORTED_LIST_GET_VALUE(TEST_ITEM, item);
    item_payload->key = 0x42;
    (void)clds_hazard_pointers_set_reclaim_threshold(hazard_pointers, 1);
    (void)clds_sorted_list_insert(list, hazard_pointers_thread, item);
    result = clds_sorted_list_find_key(list, hazard_pointers_thread, (void*)0x42);
    umock_c_reset_all_calls();

    STRICT_EXPECTED_CALL(clds_hazard_pointers_acquire(IGNORED_PTR_ARG, IGNORED_PTR_ARG)).IgnoreAllCalls();
    STRICT_EXPECTED_CALL(clds_hazard_pointers_release(IGNORED_PTR_ARG, IGNORED_PTR_ARG)).IgnoreAllCalls();
    STRICT_EXPECTED_CALL(clds_st_hash_set_create(IGNORED_PTR_ARG, IGNORED_NUM_ARG)).IgnoreAllCalls();
    STRICT_EXPECTED_CALL(clds_st_hash_set_destroy(IGNORED_PTR_ARG)).IgnoreAllCalls();
    STRICT_EXPECTED_CALL(clds_st_hash_set_find(IGNORED_PTR_ARG, IGNORED_PTR_ARG, IGNORED_PTR_ARG)).IgnoreAllCalls();
    STRICT_EXPECTED_CALL(clds_hazard_pointers_reclaim(hazard_pointers_thread, item, IGNORED_PTR_ARG));

    // no item cleanup and free should happen here

    // act
    (void)clds_sorted_list_delete_item(list, hazard_pointers_thread, item);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

    // cleanup
    clds_sorted_list_destroy(list);
    clds_hazard_pointers_destroy(hazard_pointers);
}

/* clds_sorted_list_node_create */

/* Tests_SRS_CLDS_SORTED_LIST_01_036: [ `item_cleanup_callback` shall be allowed to be NULL. ]*/
TEST_FUNCTION(clds_sorted_list_insert_with_NULL_item_cleanup_callback_succeeds)
{
    // arrange
    CLDS_SORTED_LIST_ITEM* item;

    STRICT_EXPECTED_CALL(malloc(IGNORED_NUM_ARG));

    // act
    item = CLDS_SORTED_LIST_NODE_CREATE(TEST_ITEM, NULL, (void*)0x4242);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
    ASSERT_IS_NOT_NULL(item);

    // cleanup
    CLDS_SORTED_LIST_NODE_RELEASE(TEST_ITEM, item);
}

/* Tests_SRS_CLDS_SORTED_LIST_01_037: [ `item_cleanup_callback_context` shall be allowed to be NULL. ]*/
TEST_FUNCTION(clds_sorted_list_insert_with_NULL_item_cleanup_callback_context_succeeds)
{
    // arrange
    CLDS_SORTED_LIST_ITEM* item;

    STRICT_EXPECTED_CALL(malloc(IGNORED_NUM_ARG));

    // act
    item = CLDS_SORTED_LIST_NODE_CREATE(TEST_ITEM, test_item_cleanup_func, NULL);

    // assert
    ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());
    ASSERT_IS_NOT_NULL(item);

    // cleanup
    CLDS_SORTED_LIST_NODE_RELEASE(TEST_ITEM, item);
}

END_TEST_SUITE(clds_sorted_list_unittests)