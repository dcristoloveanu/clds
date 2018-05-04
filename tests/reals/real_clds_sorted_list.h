// Licensed under the MIT license.See LICENSE file in the project root for full license information.

#ifndef REAL_CLDS_SORTED_LIST_H
#define REAL_CLDS_SORTED_LIST_H

#include "macro_utils.h"
#include "clds/clds_sorted_list.h"

#define R2(X) REGISTER_GLOBAL_MOCK_HOOK(X, real_##X);

#define REGISTER_CLDS_SORTED_LIST_GLOBAL_MOCK_HOOKS() \
    FOR_EACH_1(R2, \
        clds_sorted_list_create, \
        clds_sorted_list_destroy, \
        clds_sorted_list_insert, \
        clds_sorted_list_delete_item, \
        clds_sorted_list_delete_key, \
        clds_sorted_list_find_key, \
        clds_sorted_list_node_create, \
        clds_sorted_list_node_inc_ref, \
        clds_sorted_list_node_release \
    )

#ifdef __cplusplus
#include <cstddef>
extern "C"
{
#else
#include <stddef.h>
#endif

CLDS_SORTED_LIST_HANDLE real_clds_sorted_list_create(CLDS_HAZARD_POINTERS_HANDLE clds_hazard_pointers, SORTED_LIST_GET_ITEM_KEY_CB get_item_key_cb, void* get_item_key_cb_context, SORTED_LIST_KEY_COMPARE_CB key_compare_cb, void* key_compare_cb_context);
void real_clds_sorted_list_destroy(CLDS_SORTED_LIST_HANDLE clds_sorted_list);

CLDS_SORTED_LIST_INSERT_RESULT real_clds_sorted_list_insert(CLDS_SORTED_LIST_HANDLE clds_sorted_list, CLDS_HAZARD_POINTERS_THREAD_HANDLE clds_hazard_pointers_thread, CLDS_SORTED_LIST_ITEM* item);
CLDS_SORTED_LIST_DELETE_RESULT real_clds_sorted_list_delete_item(CLDS_SORTED_LIST_HANDLE clds_sorted_list, CLDS_HAZARD_POINTERS_THREAD_HANDLE clds_hazard_pointers_thread, CLDS_SORTED_LIST_ITEM* item);
CLDS_SORTED_LIST_DELETE_RESULT real_clds_sorted_list_delete_key(CLDS_SORTED_LIST_HANDLE clds_sorted_list, CLDS_HAZARD_POINTERS_THREAD_HANDLE clds_hazard_pointers_thread, void* key);
CLDS_SORTED_LIST_REMOVE_RESULT clds_sorted_list_remove_key(CLDS_SORTED_LIST_HANDLE clds_sorted_list, CLDS_HAZARD_POINTERS_THREAD_HANDLE clds_hazard_pointers_thread, void* key, CLDS_SORTED_LIST_ITEM** item);
CLDS_SORTED_LIST_ITEM* real_clds_sorted_list_find_key(CLDS_SORTED_LIST_HANDLE clds_sorted_list, CLDS_HAZARD_POINTERS_THREAD_HANDLE clds_hazard_pointers_thread, void* key);

// helper APIs for creating/destroying a singly linked list node
CLDS_SORTED_LIST_ITEM* real_clds_sorted_list_node_create(size_t node_size, SORTED_LIST_ITEM_CLEANUP_CB item_cleanup_callback, void* item_cleanup_callback_context);
int real_clds_sorted_list_node_inc_ref(CLDS_SORTED_LIST_ITEM* item);
void real_clds_sorted_list_node_release(CLDS_SORTED_LIST_ITEM* item);

#ifdef __cplusplus
}
#endif

#endif // REAL_CLDS_SORTED_LIST_H