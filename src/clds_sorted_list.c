// Licensed under the MIT license.See LICENSE file in the project root for full license information.

#include <stdlib.h>
#include <stdbool.h>
#include "azure_c_shared_utility/gballoc.h"
#include "azure_c_shared_utility/xlogging.h"
#include "clds/clds_sorted_list.h"
#include "clds/clds_atomics.h"
#include "clds/clds_hazard_pointers.h"

/* this is a lock free singly linked list implementation */

typedef struct CLDS_SORTED_LIST_TAG
{
    CLDS_HAZARD_POINTERS_HANDLE clds_hazard_pointers;
    volatile CLDS_SORTED_LIST_ITEM* head;
    SORTED_LIST_GET_ITEM_KEY_CB get_item_key_cb;
    void* get_item_key_cb_context;
    SORTED_LIST_KEY_COMPARE_CB key_compare_cb;
    void* key_compare_cb_context;
} CLDS_SORTED_LIST;

typedef int(*SORTED_LIST_ITEM_COMPARE_CB)(void* context, CLDS_SORTED_LIST_ITEM* item1, void* item_compare_target);

static int compare_item_by_ptr(void* context, CLDS_SORTED_LIST_ITEM* item, void* item_compare_target)
{
    int result;

    (void)context;

    if (item == item_compare_target)
    {
        result = 0;
    }
    else
    {
        result = 1;
    }

    return result;
}

static int compare_item_by_key(void* context, CLDS_SORTED_LIST_ITEM* item, void* item_compare_target)
{
    CLDS_SORTED_LIST_HANDLE clds_sorted_list = (CLDS_SORTED_LIST_HANDLE)context;
    // get item key
    void* item_key = clds_sorted_list->get_item_key_cb(clds_sorted_list->get_item_key_cb_context, item);
    return clds_sorted_list->key_compare_cb(clds_sorted_list->key_compare_cb_context, item_key, item_compare_target);
}

static void internal_node_destroy(CLDS_SORTED_LIST_ITEM* item)
{
    if (InterlockedDecrement(&item->ref_count) == 0)
    {
        /* Codes_SRS_CLDS_SORTED_LIST_01_044: [ If `item_cleanup_callback` is NULL, no user callback shall be triggered for the reclaimed item. ]*/
        if (item->item_cleanup_callback != NULL)
        {
            /* Codes_SRS_CLDS_SORTED_LIST_01_043: [ The reclaim function passed to `clds_hazard_pointers_reclaim` shall call the user callback `item_cleanup_callback` that was passed to `clds_sorted_list_node_create`, while passing `item_cleanup_callback_context` and the freed item as arguments. ]*/
            item->item_cleanup_callback(item->item_cleanup_callback_context, item);
        }

        free((void*)item);
    }
}

static void reclaim_list_node(void* node)
{
    internal_node_destroy((CLDS_SORTED_LIST_ITEM*)node);
}

static CLDS_SORTED_LIST_DELETE_RESULT internal_delete(CLDS_SORTED_LIST_HANDLE clds_sorted_list, CLDS_HAZARD_POINTERS_THREAD_HANDLE clds_hazard_pointers_thread, SORTED_LIST_ITEM_COMPARE_CB item_compare_callback, void* item_compare_target)
{
    CLDS_SORTED_LIST_DELETE_RESULT result = CLDS_SORTED_LIST_DELETE_ERROR;

    // check that the node is really in the list and obtain
    bool restart_needed;

    do
    {
        CLDS_HAZARD_POINTER_RECORD_HANDLE previous_hp = NULL;
        volatile CLDS_SORTED_LIST_ITEM* previous_item = NULL;
        volatile CLDS_SORTED_LIST_ITEM** current_item_address = &clds_sorted_list->head;

        do
        {
            // get the current_item value
            volatile CLDS_SORTED_LIST_ITEM* current_item = (volatile CLDS_SORTED_LIST_ITEM*)InterlockedCompareExchangePointer((volatile PVOID*)current_item_address, NULL, NULL);
            if (current_item == NULL)
            {
                if (previous_hp != NULL)
                {
                    // let go of previous hazard pointer
                    clds_hazard_pointers_release(clds_hazard_pointers_thread, previous_hp);
                }

                restart_needed = false;

                /* Codes_SRS_CLDS_SORTED_LIST_01_018: [ If the item does not exist in the list, `clds_sorted_list_delete_item` shall fail and return `CLDS_SORTED_LIST_DELETE_NOT_FOUND`. ]*/
                /* Codes_SRS_CLDS_SORTED_LIST_01_024: [ If the key is not found, `clds_sorted_list_delete_key` shall fail and return `CLDS_SORTED_LIST_DELETE_NOT_FOUND`. ]*/
                result = CLDS_SORTED_LIST_DELETE_NOT_FOUND;
                break;
            }
            else
            {
                // acquire hazard pointer
                CLDS_HAZARD_POINTER_RECORD_HANDLE current_item_hp = clds_hazard_pointers_acquire(clds_hazard_pointers_thread, (void*)((uintptr_t)current_item & ~0x1));
                if (current_item_hp == NULL)
                {
                    if (previous_hp != NULL)
                    {
                        // let go of previous hazard pointer
                        clds_hazard_pointers_release(clds_hazard_pointers_thread, previous_hp);
                    }

                    LogError("Cannot acquire hazard pointer");
                    restart_needed = false;
                    result = CLDS_SORTED_LIST_DELETE_ERROR;
                    break;
                }
                else
                {
                    // now make sure the item has not changed
                    if (InterlockedCompareExchangePointer((volatile PVOID*)current_item_address, (PVOID)current_item, (PVOID)current_item) != (PVOID)((uintptr_t)current_item & ~0x1))
                    {
                        if (previous_hp != NULL)
                        {
                            // let go of previous hazard pointer
                            clds_hazard_pointers_release(clds_hazard_pointers_thread, previous_hp);
                        }

                        // item changed, it is likely that the node is no longer reachable, so we should not use its memory, restart
                        clds_hazard_pointers_release(clds_hazard_pointers_thread, current_item_hp);
                        restart_needed = true;
                        break;
                    }
                    else
                    {
                        int compare_result = item_compare_callback(clds_sorted_list, (CLDS_SORTED_LIST_ITEM*)current_item, item_compare_target);
                        if (compare_result == 0)
                        {
                            // mark the node as deleted
                            // get the next pointer as this is the only place where we keep information
                            volatile CLDS_SORTED_LIST_ITEM* current_next = InterlockedCompareExchangePointer((volatile PVOID*)&current_item->next, NULL, NULL);

                            // mark that the node is deleted
                            if (InterlockedCompareExchangePointer((volatile PVOID*)&current_item->next, (PVOID)((uintptr_t)current_next | 1), (PVOID)current_next) != (PVOID)current_next)
                            {
                                if (previous_hp != NULL)
                                {
                                    // let go of previous hazard pointer
                                    clds_hazard_pointers_release(clds_hazard_pointers_thread, previous_hp);
                                }

                                clds_hazard_pointers_release(clds_hazard_pointers_thread, current_item_hp);

                                // restart
                                restart_needed = true;
                                break;
                            }
                            else
                            {
                                // the current node is marked for deletion, now try to change the previous link to the next value

                                // If in the meanwhile someone would be deleting node A they would have to first set the
                                // deleted flag on it, in which case we'd see the CAS fail

                                if (previous_item == NULL)
                                {
                                    // we are removing the head
                                    if (InterlockedCompareExchangePointer((volatile PVOID*)&clds_sorted_list->head, (PVOID)current_next, (PVOID)current_item) != (PVOID)current_item)
                                    {
                                        // head changed, restart
                                        (void)InterlockedCompareExchangePointer((volatile PVOID*)&current_item->next, (PVOID)current_next, (PVOID)((uintptr_t)current_next | 1));

                                        clds_hazard_pointers_release(clds_hazard_pointers_thread, current_item_hp);

                                        restart_needed = true;
                                        break;
                                    }
                                    else
                                    {
                                        // delete succesfull
                                        clds_hazard_pointers_release(clds_hazard_pointers_thread, current_item_hp);

                                        // reclaim the memory
                                        /* Codes_SRS_CLDS_SORTED_LIST_01_042: [ When an item is deleted it shall be indicated to the hazard pointers instance as reclaimed by calling `clds_hazard_pointers_reclaim`. ]*/
                                        clds_hazard_pointers_reclaim(clds_hazard_pointers_thread, (void*)((uintptr_t)current_item & ~0x1), reclaim_list_node);
                                        restart_needed = false;

                                        /* Codes_SRS_CLDS_SORTED_LIST_01_026: [ On success, `clds_sorted_list_delete_item` shall return `CLDS_SORTED_LIST_DELETE_OK`. ]*/
                                        /* Codes_SRS_CLDS_SORTED_LIST_01_025: [ On success, `clds_sorted_list_delete_key` shall return `CLDS_SORTED_LIST_DELETE_OK`. ]*/
                                        result = CLDS_SORTED_LIST_DELETE_OK;

                                        break;
                                    }
                                }
                                else
                                {
                                    if (InterlockedCompareExchangePointer((volatile PVOID*)&previous_item->next, (PVOID)current_next, (PVOID)current_item) != (PVOID)current_item)
                                    {
                                        // someone is deleting our left node, restart, but first unlock our own delete mark
                                        (void)InterlockedCompareExchangePointer((volatile PVOID*)&current_item->next, (PVOID)current_next, (PVOID)((uintptr_t)current_next | 1));

                                        clds_hazard_pointers_release(clds_hazard_pointers_thread, previous_hp);
                                        clds_hazard_pointers_release(clds_hazard_pointers_thread, current_item_hp);

                                        restart_needed = true;
                                        break;
                                    }
                                    else
                                    {
                                        // delete succesfull, no-one deleted the left node in the meanwhile
                                        clds_hazard_pointers_release(clds_hazard_pointers_thread, previous_hp);
                                        clds_hazard_pointers_release(clds_hazard_pointers_thread, current_item_hp);

                                        // reclaim the memory
                                        /* Codes_SRS_CLDS_SORTED_LIST_01_042: [ When an item is deleted it shall be indicated to the hazard pointers instance as reclaimed by calling `clds_hazard_pointers_reclaim`. ]*/
                                        clds_hazard_pointers_reclaim(clds_hazard_pointers_thread, (void*)((uintptr_t)current_item & ~0x1), reclaim_list_node);

                                        /* Codes_SRS_CLDS_SORTED_LIST_01_026: [ On success, `clds_sorted_list_delete_item` shall return `CLDS_SORTED_LIST_DELETE_OK`. ]*/
                                        /* Codes_SRS_CLDS_SORTED_LIST_01_025: [ On success, `clds_sorted_list_delete_key` shall return `CLDS_SORTED_LIST_DELETE_OK`. ]*/
                                        result = CLDS_SORTED_LIST_DELETE_OK;

                                        restart_needed = false;
                                        break;
                                    }
                                }
                            }
                        }
                        else
                        {
                            // we have a stable pointer to the current item, now simply set the previous to be this
                            if (previous_hp != NULL)
                            {
                                // let go of previous hazard pointer
                                clds_hazard_pointers_release(clds_hazard_pointers_thread, previous_hp);
                            }

                            previous_hp = current_item_hp;
                            previous_item = current_item;
                            current_item_address = (volatile CLDS_SORTED_LIST_ITEM**)&current_item->next;
                        }
                    }
                }
            }
        } while (1);
    } while (restart_needed);

    return result;
}

static CLDS_SORTED_LIST_REMOVE_RESULT internal_remove(CLDS_SORTED_LIST_HANDLE clds_sorted_list, CLDS_HAZARD_POINTERS_THREAD_HANDLE clds_hazard_pointers_thread, SORTED_LIST_ITEM_COMPARE_CB item_compare_callback, void* item_compare_target, CLDS_SORTED_LIST_ITEM** item)
{
    CLDS_SORTED_LIST_REMOVE_RESULT result = CLDS_SORTED_LIST_DELETE_ERROR;

    // check that the node is really in the list and obtain
    bool restart_needed;

    do
    {
        CLDS_HAZARD_POINTER_RECORD_HANDLE previous_hp = NULL;
        volatile CLDS_SORTED_LIST_ITEM* previous_item = NULL;
        volatile CLDS_SORTED_LIST_ITEM** current_item_address = &clds_sorted_list->head;

        do
        {
            // get the current_item value
            volatile CLDS_SORTED_LIST_ITEM* current_item = (volatile CLDS_SORTED_LIST_ITEM*)InterlockedCompareExchangePointer((volatile PVOID*)current_item_address, NULL, NULL);
            if (current_item == NULL)
            {
                if (previous_hp != NULL)
                {
                    // let go of previous hazard pointer
                    clds_hazard_pointers_release(clds_hazard_pointers_thread, previous_hp);
                }

                restart_needed = false;

                /* Codes_SRS_CLDS_SORTED_LIST_01_057: [ If the key is not found, `clds_sorted_list_remove_key` shall fail and return `CLDS_SORTED_LIST_REMOVE_NOT_FOUND`. ]*/
                result = CLDS_SORTED_LIST_REMOVE_NOT_FOUND;
                break;
            }
            else
            {
                // acquire hazard pointer
                CLDS_HAZARD_POINTER_RECORD_HANDLE current_item_hp = clds_hazard_pointers_acquire(clds_hazard_pointers_thread, (void*)((uintptr_t)current_item & ~0x1));
                if (current_item_hp == NULL)
                {
                    if (previous_hp != NULL)
                    {
                        // let go of previous hazard pointer
                        clds_hazard_pointers_release(clds_hazard_pointers_thread, previous_hp);
                    }

                    LogError("Cannot acquire hazard pointer");
                    restart_needed = false;
                    result = CLDS_SORTED_LIST_REMOVE_ERROR;
                    break;
                }
                else
                {
                    // now make sure the item has not changed
                    if (InterlockedCompareExchangePointer((volatile PVOID*)current_item_address, (PVOID)current_item, (PVOID)current_item) != (PVOID)((uintptr_t)current_item & ~0x1))
                    {
                        if (previous_hp != NULL)
                        {
                            // let go of previous hazard pointer
                            clds_hazard_pointers_release(clds_hazard_pointers_thread, previous_hp);
                        }

                        // item changed, it is likely that the node is no longer reachable, so we should not use its memory, restart
                        clds_hazard_pointers_release(clds_hazard_pointers_thread, current_item_hp);
                        restart_needed = true;
                        break;
                    }
                    else
                    {
                        int compare_result = item_compare_callback(clds_sorted_list, (CLDS_SORTED_LIST_ITEM*)current_item, item_compare_target);
                        if (compare_result == 0)
                        {
                            // mark the node as deleted
                            // get the next pointer as this is the only place where we keep information
                            volatile CLDS_SORTED_LIST_ITEM* current_next = InterlockedCompareExchangePointer((volatile PVOID*)&current_item->next, NULL, NULL);

                            // mark that the node is deleted
                            if (InterlockedCompareExchangePointer((volatile PVOID*)&current_item->next, (PVOID)((uintptr_t)current_next | 1), (PVOID)current_next) != (PVOID)current_next)
                            {
                                if (previous_hp != NULL)
                                {
                                    // let go of previous hazard pointer
                                    clds_hazard_pointers_release(clds_hazard_pointers_thread, previous_hp);
                                }

                                clds_hazard_pointers_release(clds_hazard_pointers_thread, current_item_hp);

                                // restart
                                restart_needed = true;
                                break;
                            }
                            else
                            {
                                // the current node is marked for deletion, now try to change the previous link to the next value

                                // If in the meanwhile someone would be deleting node A they would have to first set the
                                // deleted flag on it, in which case we'd see the CAS fail

                                if (previous_item == NULL)
                                {
                                    // we are removing the head
                                    if (InterlockedCompareExchangePointer((volatile PVOID*)&clds_sorted_list->head, (PVOID)current_next, (PVOID)current_item) != (PVOID)current_item)
                                    {
                                        // head changed, restart
                                        (void)InterlockedCompareExchangePointer((volatile PVOID*)&current_item->next, (PVOID)current_next, (PVOID)((uintptr_t)current_next | 1));

                                        clds_hazard_pointers_release(clds_hazard_pointers_thread, current_item_hp);

                                        restart_needed = true;
                                        break;
                                    }
                                    else
                                    {
                                        *item = (CLDS_SORTED_LIST_ITEM*)current_item;
                                        clds_sorted_list_node_inc_ref(*item);

                                        // delete succesfull
                                        clds_hazard_pointers_release(clds_hazard_pointers_thread, current_item_hp);

                                        // reclaim the memory
                                        /* Codes_SRS_CLDS_SORTED_LIST_01_042: [ When an item is deleted it shall be indicated to the hazard pointers instance as reclaimed by calling `clds_hazard_pointers_reclaim`. ]*/
                                        clds_hazard_pointers_reclaim(clds_hazard_pointers_thread, (void*)((uintptr_t)current_item & ~0x1), reclaim_list_node);
                                        restart_needed = false;

                                        result = CLDS_SORTED_LIST_REMOVE_OK;

                                        break;
                                    }
                                }
                                else
                                {
                                    if (InterlockedCompareExchangePointer((volatile PVOID*)&previous_item->next, (PVOID)current_next, (PVOID)current_item) != (PVOID)current_item)
                                    {
                                        // someone is deleting our left node, restart, but first unlock our own delete mark
                                        (void)InterlockedCompareExchangePointer((volatile PVOID*)&current_item->next, (PVOID)current_next, (PVOID)((uintptr_t)current_next | 1));

                                        clds_hazard_pointers_release(clds_hazard_pointers_thread, previous_hp);
                                        clds_hazard_pointers_release(clds_hazard_pointers_thread, current_item_hp);

                                        restart_needed = true;
                                        break;
                                    }
                                    else
                                    {
                                        *item = (CLDS_SORTED_LIST_ITEM*)current_item;
                                        clds_sorted_list_node_inc_ref(*item);

                                        // delete succesfull, no-one deleted the left node in the meanwhile
                                        clds_hazard_pointers_release(clds_hazard_pointers_thread, previous_hp);
                                        clds_hazard_pointers_release(clds_hazard_pointers_thread, current_item_hp);

                                        // reclaim the memory
                                        /* Codes_SRS_CLDS_SORTED_LIST_01_042: [ When an item is deleted it shall be indicated to the hazard pointers instance as reclaimed by calling `clds_hazard_pointers_reclaim`. ]*/
                                        clds_hazard_pointers_reclaim(clds_hazard_pointers_thread, (void*)((uintptr_t)current_item & ~0x1), reclaim_list_node);

                                        result = CLDS_SORTED_LIST_REMOVE_OK;

                                        restart_needed = false;
                                        break;
                                    }
                                }
                            }
                        }
                        else
                        {
                            // we have a stable pointer to the current item, now simply set the previous to be this
                            if (previous_hp != NULL)
                            {
                                // let go of previous hazard pointer
                                clds_hazard_pointers_release(clds_hazard_pointers_thread, previous_hp);
                            }

                            previous_hp = current_item_hp;
                            previous_item = current_item;
                            current_item_address = (volatile CLDS_SORTED_LIST_ITEM**)&current_item->next;
                        }
                    }
                }
            }
        } while (1);
    } while (restart_needed);

    return result;
}

CLDS_SORTED_LIST_HANDLE clds_sorted_list_create(CLDS_HAZARD_POINTERS_HANDLE clds_hazard_pointers, SORTED_LIST_GET_ITEM_KEY_CB get_item_key_cb, void* get_item_key_cb_context, SORTED_LIST_KEY_COMPARE_CB key_compare_cb, void* key_compare_cb_context)
{
    CLDS_SORTED_LIST_HANDLE clds_sorted_list;

    /* Codes_SRS_CLDS_SORTED_LIST_01_049: [ `get_item_key_cb_context` shall be allowed to be NULL. ]*/
    /* Codes_SRS_CLDS_SORTED_LIST_01_050: [ `key_compare_cb_context` shall be allowed to be NULL. ]*/

    /* Codes_SRS_CLDS_SORTED_LIST_01_003: [ If `clds_hazard_pointers` is NULL, `clds_sorted_list_create` shall fail and return NULL. ]*/
    if ((clds_hazard_pointers == NULL) ||
        /* Codes_SRS_CLDS_SORTED_LIST_01_045: [ If `get_item_key_cb` is NULL, `clds_sorted_list_create` shall fail and return NULL. ]*/
        (get_item_key_cb == NULL) ||
        /* Tests_SRS_CLDS_SORTED_LIST_01_046: [ If `key_compare_cb` is NULL, `clds_sorted_list_create` shall fail and return NULL. ]*/
        (key_compare_cb == NULL))
    {
        LogError("Invalid arguments: clds_hazard_pointers = %p, get_item_key_cb = %p, key_compare_cb = %p");
        clds_sorted_list = NULL;
    }
    else
    {
        /* Codes_SRS_CLDS_SORTED_LIST_01_001: [ `clds_sorted_list_create` shall create a new sorted list object and on success it shall return a non-NULL handle to the newly created list. ]*/
        clds_sorted_list = (CLDS_SORTED_LIST_HANDLE)malloc(sizeof(CLDS_SORTED_LIST));
        if (clds_sorted_list == NULL)
        {
            /* Codes_SRS_CLDS_SORTED_LIST_01_002: [ If any error happens, `clds_sorted_list_create` shall fail and return NULL. ]*/
            LogError("Cannot allocate memory for the singly linked list");
        }
        else
        {
            // all ok
            clds_sorted_list->clds_hazard_pointers = clds_hazard_pointers;
            clds_sorted_list->get_item_key_cb = get_item_key_cb;
            clds_sorted_list->get_item_key_cb_context = get_item_key_cb_context;
            clds_sorted_list->key_compare_cb = key_compare_cb;
            clds_sorted_list->key_compare_cb_context = key_compare_cb_context;

            (void)InterlockedExchangePointer((volatile PVOID*)&clds_sorted_list->head, NULL);
        }
    }

    return clds_sorted_list;
}

void clds_sorted_list_destroy(CLDS_SORTED_LIST_HANDLE clds_sorted_list)
{
    if (clds_sorted_list == NULL)
    {
        /* Codes_SRS_CLDS_SORTED_LIST_01_005: [ If `clds_sorted_list` is NULL, `clds_sorted_list_destroy` shall return. ]*/
        LogError("NULL clds_sorted_list");
    }
    else
    {
        CLDS_SORTED_LIST_ITEM* current_item = InterlockedCompareExchangePointer((volatile PVOID*)&clds_sorted_list->head, NULL, NULL);

        /* Codes_SRS_CLDS_SORTED_LIST_01_039: [ Any items still present in the list shall be freed. ]*/
        // go through all the items and free them
        while (current_item != NULL)
        {
            CLDS_SORTED_LIST_ITEM* next_item = InterlockedCompareExchangePointer((volatile PVOID*)&current_item->next, NULL, NULL);

            /* Codes_SRS_CLDS_SORTED_LIST_01_040: [ For each item that is freed, the callback `item_cleanup_callback` passed to `clds_sorted_list_node_create` shall be called, while passing `item_cleanup_callback_context` and the freed item as arguments. ]*/
            /* Codes_SRS_CLDS_SORTED_LIST_01_041: [ If `item_cleanup_callback` is NULL, no user callback shall be triggered for the freed items. ]*/
            internal_node_destroy(current_item);
            current_item = next_item;
        } 

        /* Codes_SRS_CLDS_SORTED_LIST_01_004: [ `clds_sorted_list_destroy` shall free all resources associated with the sorted list instance. ]*/
        free(clds_sorted_list);
    }
}

CLDS_SORTED_LIST_INSERT_RESULT clds_sorted_list_insert(CLDS_SORTED_LIST_HANDLE clds_sorted_list, CLDS_HAZARD_POINTERS_THREAD_HANDLE clds_hazard_pointers_thread, CLDS_SORTED_LIST_ITEM* item)
{
    CLDS_SORTED_LIST_INSERT_RESULT result;

    /* Codes_SRS_CLDS_SORTED_LIST_01_011: [ If `clds_sorted_list` is NULL, `clds_sorted_list_insert` shall fail and return `CLDS_SORTED_LIST_INSERT_ERROR`. ]*/
    if ((clds_sorted_list == NULL) ||
        /* Codes_SRS_CLDS_SORTED_LIST_01_012: [ If `item` is NULL, `clds_sorted_list_insert` shall fail and return `CLDS_SORTED_LIST_INSERT_ERROR`. ]*/
        (item == NULL) ||
        /* Codes_SRS_CLDS_SORTED_LIST_01_013: [ If `clds_hazard_pointers_thread` is NULL, `clds_sorted_list_insert` shall fail and return `CLDS_SORTED_LIST_INSERT_ERROR`. ]*/
        (clds_hazard_pointers_thread == NULL))
    {
        LogError("Invalid arguments: clds_sorted_list = %p, item = %p, clds_hazard_pointers_thread = %p",
            clds_sorted_list, item, clds_hazard_pointers_thread);
        result = CLDS_SORTED_LIST_INSERT_ERROR;
    }
    else
    {
        bool restart_needed;
        void* new_item_key = clds_sorted_list->get_item_key_cb(clds_sorted_list->get_item_key_cb_context, item);

        /* Codes_SRS_CLDS_SORTED_LIST_01_047: [ `clds_sorted_list_insert` shall insert the item at its correct location making sure that items in the list are sorted according to the order given by item keys. ]*/

        do
        {
            CLDS_HAZARD_POINTER_RECORD_HANDLE previous_hp = NULL;
            volatile CLDS_SORTED_LIST_ITEM* previous_item = NULL;
            volatile CLDS_SORTED_LIST_ITEM** current_item_address = &clds_sorted_list->head;
            result = CLDS_SORTED_LIST_INSERT_ERROR;

            do
            {
                // get the current_item value
                volatile CLDS_SORTED_LIST_ITEM* current_item = (volatile CLDS_SORTED_LIST_ITEM*)InterlockedCompareExchangePointer((volatile PVOID*)current_item_address, NULL, NULL);
                if (current_item == NULL)
                {
                    item->next = NULL;

                    // not found, so insert it here
                    if (previous_item != NULL)
                    {
                        // have a previous item
                        if (InterlockedCompareExchangePointer((volatile PVOID*)&previous_item->next, (PVOID)item, NULL) != NULL)
                        {
                            // let go of previous hazard pointer
                            clds_hazard_pointers_release(clds_hazard_pointers_thread, previous_hp);
                            restart_needed = true;
                            break;
                        }
                        else
                        {
                            clds_hazard_pointers_release(clds_hazard_pointers_thread, previous_hp);
                            restart_needed = false;

                            /* Codes_SRS_CLDS_SORTED_LIST_01_010: [ On success `clds_sorted_list_insert` shall return `CLDS_SORTED_LIST_INSERT_OK`. ]*/
                            result = CLDS_SORTED_LIST_INSERT_OK;
                            break;
                        }
                    }
                    else
                    {
                        // no previous item, replace the head
                        if (InterlockedCompareExchangePointer((volatile PVOID*)&clds_sorted_list->head, item, NULL) != NULL)
                        {
                            restart_needed = true;
                            break;
                        }
                        else
                        {
                            // insert done
                            restart_needed = false;

                            /* Codes_SRS_CLDS_SORTED_LIST_01_010: [ On success `clds_sorted_list_insert` shall return `CLDS_SORTED_LIST_INSERT_OK`. ]*/
                            result = CLDS_SORTED_LIST_INSERT_OK;
                            break;
                        }
                    }

                    break;
                }
                else
                {
                    // acquire hazard pointer
                    CLDS_HAZARD_POINTER_RECORD_HANDLE current_item_hp = clds_hazard_pointers_acquire(clds_hazard_pointers_thread, (void*)((uintptr_t)current_item & ~0x1));
                    if (current_item_hp == NULL)
                    {
                        if (previous_hp != NULL)
                        {
                            // let go of previous hazard pointer
                            clds_hazard_pointers_release(clds_hazard_pointers_thread, previous_hp);
                        }

                        LogError("Cannot acquire hazard pointer");
                        restart_needed = false;
                        result = CLDS_SORTED_LIST_INSERT_ERROR;
                        break;
                    }
                    else
                    {
                        // now make sure the item has not changed
                        if (InterlockedCompareExchangePointer((volatile PVOID*)current_item_address, (PVOID)current_item, (PVOID)current_item) != (PVOID)((uintptr_t)current_item & ~0x1))
                        {
                            if (previous_hp != NULL)
                            {
                                // let go of previous hazard pointer
                                clds_hazard_pointers_release(clds_hazard_pointers_thread, previous_hp);
                            }

                            // item changed, it is likely that the node is no longer reachable, so we should not use its memory, restart
                            clds_hazard_pointers_release(clds_hazard_pointers_thread, current_item_hp);
                            restart_needed = true;
                            break;
                        }
                        else
                        {
                            // we are in a stable state, compare the current item key to our key
                            void* current_item_key = clds_sorted_list->get_item_key_cb(clds_sorted_list->get_item_key_cb_context, (struct CLDS_SORTED_LIST_ITEM_TAG*)current_item);
                            int compare_result = clds_sorted_list->key_compare_cb(clds_sorted_list->key_compare_cb_context, new_item_key, current_item_key);

                            if (compare_result == 0)
                            {
                                // item already in the list
                                if (previous_item != NULL)
                                {
                                    clds_hazard_pointers_release(clds_hazard_pointers_thread, previous_hp);
                                }

                                clds_hazard_pointers_release(clds_hazard_pointers_thread, current_item_hp);
                                restart_needed = false;

                                /* Codes_SRS_CLDS_SORTED_LIST_01_048: [ If the item with the given key already exists in the list, `clds_sorted_list_insert` shall fail and return `CLDS_SORTED_LIST_INSERT_KEY_ALREADY_EXISTS`. ]*/
                                result = CLDS_SORTED_LIST_INSERT_KEY_ALREADY_EXISTS;
                                break;
                            }
                            else if (compare_result < 0)
                            {
                                // need to insert between these 2 nodes
                                item->next = current_item;

                                if (previous_item != NULL)
                                {
                                    // have a previous item
                                    if (InterlockedCompareExchangePointer((volatile PVOID*)&previous_item->next, (PVOID)item, (PVOID)current_item) != current_item)
                                    {
                                        // let go of previous hazard pointer
                                        clds_hazard_pointers_release(clds_hazard_pointers_thread, previous_hp);
                                        clds_hazard_pointers_release(clds_hazard_pointers_thread, current_item_hp);
                                        restart_needed = true;
                                        break;
                                    }
                                    else
                                    {
                                        clds_hazard_pointers_release(clds_hazard_pointers_thread, previous_hp);
                                        clds_hazard_pointers_release(clds_hazard_pointers_thread, current_item_hp);
                                        restart_needed = false;

                                        /* Codes_SRS_CLDS_SORTED_LIST_01_010: [ On success `clds_sorted_list_insert` shall return 0. ]*/
                                        result = 0;
                                        break;
                                    }
                                }
                                else
                                {
                                    if (InterlockedCompareExchangePointer((volatile PVOID*)&clds_sorted_list->head, (PVOID)item, (PVOID)current_item) != current_item)
                                    {
                                        // let go of previous hazard pointer
                                        clds_hazard_pointers_release(clds_hazard_pointers_thread, current_item_hp);
                                        restart_needed = true;
                                        break;
                                    }
                                    else
                                    {
                                        clds_hazard_pointers_release(clds_hazard_pointers_thread, current_item_hp);
                                        restart_needed = false;

                                        /* Codes_SRS_CLDS_SORTED_LIST_01_010: [ On success `clds_sorted_list_insert` shall return `CLDS_SORTED_LIST_INSERT_OK`. ]*/
                                        result = CLDS_SORTED_LIST_INSERT_OK;
                                        break;
                                    }
                                }
                            }
                            else // item is less than the current, so move on
                            {
                                // we have a stable pointer to the current item, now simply set the previous to be this
                                if (previous_hp != NULL)
                                {
                                    // let go of previous hazard pointer
                                    clds_hazard_pointers_release(clds_hazard_pointers_thread, previous_hp);
                                }

                                previous_hp = current_item_hp;
                                previous_item = current_item;
                                current_item_address = (volatile CLDS_SORTED_LIST_ITEM**)&current_item->next;
                            }
                        }
                    }
                }
            } while (1);
        } while (restart_needed);
    }

    return result;
}

CLDS_SORTED_LIST_DELETE_RESULT clds_sorted_list_delete_item(CLDS_SORTED_LIST_HANDLE clds_sorted_list, CLDS_HAZARD_POINTERS_THREAD_HANDLE clds_hazard_pointers_thread, CLDS_SORTED_LIST_ITEM* item)
{
    CLDS_SORTED_LIST_DELETE_RESULT result;

    /* Codes_SRS_CLDS_SORTED_LIST_01_015: [ If `clds_sorted_list` is NULL, `clds_sorted_list_delete_item` shall fail and return `CLDS_SORTED_LIST_DELETE_ERROR`. ]*/
    if ((clds_sorted_list == NULL) ||
        /* Codes_SRS_CLDS_SORTED_LIST_01_016: [ If `clds_hazard_pointers_thread` is NULL, `clds_sorted_list_delete_item` shall fail and return `CLDS_SORTED_LIST_DELETE_ERROR`. ]*/
        (clds_hazard_pointers_thread == NULL) ||
        /* Codes_SRS_CLDS_SORTED_LIST_01_017: [ If `item` is NULL, `clds_sorted_list_delete_item` shall fail and return `CLDS_SORTED_LIST_DELETE_ERROR`. ]*/
        (item == NULL))
    {
        LogError("Invalid arguments: clds_sorted_list = %p, clds_hazard_pointers_thread = %p, item = %p",
            clds_sorted_list, clds_hazard_pointers_thread, item);
        result = CLDS_SORTED_LIST_DELETE_ERROR;
    }
    else
    {
        /* Codes_SRS_CLDS_SORTED_LIST_01_014: [ `clds_sorted_list_delete_item` shall delete an item from the list by its pointer. ]*/
        result = internal_delete(clds_sorted_list, clds_hazard_pointers_thread, compare_item_by_ptr, item);
    }

    return result;
}

CLDS_SORTED_LIST_DELETE_RESULT clds_sorted_list_delete_key(CLDS_SORTED_LIST_HANDLE clds_sorted_list, CLDS_HAZARD_POINTERS_THREAD_HANDLE clds_hazard_pointers_thread, void* key)
{
    CLDS_SORTED_LIST_DELETE_RESULT result;

    /* Codes_SRS_CLDS_SORTED_LIST_01_020: [ If `clds_sorted_list` is NULL, `clds_sorted_list_delete_key` shall fail and return `CLDS_SORTED_LIST_DELETE_ERROR`. ]*/
    if ((clds_sorted_list == NULL) ||
        /* Codes_SRS_CLDS_SORTED_LIST_01_021: [ If `clds_hazard_pointers_thread` is NULL, `clds_sorted_list_delete_key` shall fail and return `CLDS_SORTED_LIST_DELETE_ERROR`. ]*/
        (clds_hazard_pointers_thread == NULL) ||
        /* Codes_SRS_CLDS_SORTED_LIST_01_022: [ If `key` is NULL, `clds_sorted_list_delete_key` shall fail and return `CLDS_SORTED_LIST_DELETE_ERROR`. ]*/
        (key == NULL))
    {
        LogError("Invalid arguments: clds_sorted_list = %p, clds_hazard_pointers_thread = %p, key = %p",
            clds_sorted_list, clds_hazard_pointers_thread, key);
        result = CLDS_SORTED_LIST_DELETE_ERROR;
    }
    else
    {
        /* Codes_SRS_CLDS_SORTED_LIST_01_019: [ `clds_sorted_list_delete_key` shall delete an item by its key. ]*/
        result = internal_delete(clds_sorted_list, clds_hazard_pointers_thread, compare_item_by_key, key);
    }

    return result;
}

CLDS_SORTED_LIST_REMOVE_RESULT clds_sorted_list_remove_key(CLDS_SORTED_LIST_HANDLE clds_sorted_list, CLDS_HAZARD_POINTERS_THREAD_HANDLE clds_hazard_pointers_thread, void* key, CLDS_SORTED_LIST_ITEM** item)
{
    CLDS_SORTED_LIST_REMOVE_RESULT result;

    /* Codes_SRS_CLDS_SORTED_LIST_01_053: [ If `clds_sorted_list` is NULL, `clds_sorted_list_remove_key` shall fail and return `CLDS_SORTED_LIST_REMOVE_ERROR`. ]*/
    if ((clds_sorted_list == NULL) ||
        /* Codes_SRS_CLDS_SORTED_LIST_01_055: [ If `clds_hazard_pointers_thread` is NULL, `clds_sorted_list_remove_key` shall fail and return `CLDS_SORTED_LIST_REMOVE_ERROR`. ]*/
        (clds_hazard_pointers_thread == NULL) ||
        /* Codes_SRS_CLDS_SORTED_LIST_01_056: [ If `key` is NULL, `clds_sorted_list_remove_key` shall fail and return `CLDS_SORTED_LIST_REMOVE_ERROR`. ]*/
        (key == NULL))
    {
        LogError("Invalid arguments: clds_sorted_list = %p, clds_hazard_pointers_thread = %p, key = %p",
            clds_sorted_list, clds_hazard_pointers_thread, key);
        result = CLDS_SORTED_LIST_DELETE_ERROR;
    }
    else
    {
        /* Codes_SRS_CLDS_SORTED_LIST_01_051: [ `clds_sorted_list_remove_key` shall delete an remove an item by its key and return the pointer to it. ]*/
        result = internal_remove(clds_sorted_list, clds_hazard_pointers_thread, compare_item_by_key, key, item);
    }

    return result;
}

CLDS_SORTED_LIST_ITEM* clds_sorted_list_find_key(CLDS_SORTED_LIST_HANDLE clds_sorted_list, CLDS_HAZARD_POINTERS_THREAD_HANDLE clds_hazard_pointers_thread, void* key)
{
    CLDS_SORTED_LIST_ITEM* result;

    /* Codes_SRS_CLDS_SORTED_LIST_01_028: [ If `clds_sorted_list` is NULL, `clds_sorted_list_find` shall fail and return NULL. ]*/
    if ((clds_sorted_list == NULL) ||
        /* Codes_SRS_CLDS_SORTED_LIST_01_030: [ If `clds_hazard_pointers_thread` is NULL, `clds_sorted_list_find` shall fail and return NULL. ]*/
        (clds_hazard_pointers_thread == NULL) ||
        /* Codes_SRS_CLDS_SORTED_LIST_01_031: [ If `key` is NULL, `clds_sorted_list_find` shall fail and return NULL. ]*/
        (key == NULL))
    {
        LogError("Invalid arguments: clds_sorted_list = %p, key = %p",
            clds_sorted_list, key);
        result = NULL;
    }
    else
    {
        /* Codes_SRS_CLDS_SORTED_LIST_01_027: [ `clds_sorted_list_find` shall find in the list the first item that matches the criteria given by a user compare function. ]*/

        bool restart_needed;
        result = NULL;

        do
        {
            CLDS_HAZARD_POINTER_RECORD_HANDLE previous_hp = NULL;
            volatile CLDS_SORTED_LIST_ITEM* previous_item = NULL;
            volatile CLDS_SORTED_LIST_ITEM** current_item_address = &clds_sorted_list->head;

            do
            {
                // get the current_item value
                volatile CLDS_SORTED_LIST_ITEM* current_item = (volatile CLDS_SORTED_LIST_ITEM*)InterlockedCompareExchangePointer((volatile PVOID*)current_item_address, NULL, NULL);
                if (current_item == NULL)
                {
                    if (previous_hp != NULL)
                    {
                        // let go of previous hazard pointer
                        clds_hazard_pointers_release(clds_hazard_pointers_thread, previous_hp);
                    }

                    restart_needed = false;
                    result = NULL;
                    break;
                }
                else
                {
                    // acquire hazard pointer
                    CLDS_HAZARD_POINTER_RECORD_HANDLE current_item_hp = clds_hazard_pointers_acquire(clds_hazard_pointers_thread, (void*)((uintptr_t)current_item & ~0x1));
                    if (current_item_hp == NULL)
                    {
                        if (previous_hp != NULL)
                        {
                            // let go of previous hazard pointer
                            clds_hazard_pointers_release(clds_hazard_pointers_thread, previous_hp);
                        }

                        LogError("Cannot acquire hazard pointer");
                        restart_needed = false;
                        result = NULL;
                        break;
                    }
                    else
                    {
                        // now make sure the item has not changed
                        if (InterlockedCompareExchangePointer((volatile PVOID*)current_item_address, (PVOID)current_item, (PVOID)current_item) != (PVOID)((uintptr_t)current_item & ~0x1))
                        {
                            if (previous_hp != NULL)
                            {
                                // let go of previous hazard pointer
                                clds_hazard_pointers_release(clds_hazard_pointers_thread, previous_hp);
                            }

                            // item changed, it is likely that the node is no longer reachable, so we should not use its memory, restart
                            clds_hazard_pointers_release(clds_hazard_pointers_thread, current_item_hp);
                            restart_needed = true;
                            break;
                        }
                        else
                        {
                            void* item_key = clds_sorted_list->get_item_key_cb(clds_sorted_list->get_item_key_cb_context, (struct CLDS_SORTED_LIST_ITEM_TAG*)current_item);
                            int compare_result = clds_sorted_list->key_compare_cb(clds_sorted_list->key_compare_cb_context, key, item_key);
                            if (compare_result == 0)
                            {
                                if (previous_hp != NULL)
                                {
                                    // let go of previous hazard pointer
                                    clds_hazard_pointers_release(clds_hazard_pointers_thread, previous_hp);
                                }

                                // found it
                                (void)InterlockedIncrement(&current_item->ref_count);
                                clds_hazard_pointers_release(clds_hazard_pointers_thread, current_item_hp);

                                /* Codes_SRS_CLDS_SORTED_LIST_01_029: [ On success `clds_sorted_list_find` shall return a non-NULL pointer to the found linked list item. ]*/
                                result = (CLDS_SORTED_LIST_ITEM*)current_item;
                                restart_needed = false;
                                break;
                            }
                            else
                            {
                                // we have a stable pointer to the current item, now simply set the previous to be this
                                if (previous_hp != NULL)
                                {
                                    // let go of previous hazard pointer
                                    clds_hazard_pointers_release(clds_hazard_pointers_thread, previous_hp);
                                }

                                previous_hp = current_item_hp;
                                previous_item = current_item;
                                current_item_address = (volatile CLDS_SORTED_LIST_ITEM**)&current_item->next;
                            }
                        }
                    }
                }
            } while (1);
        } while (restart_needed);
    }

    return result;
}

CLDS_SORTED_LIST_ITEM* clds_sorted_list_node_create(size_t node_size, SORTED_LIST_ITEM_CLEANUP_CB item_cleanup_callback, void* item_cleanup_callback_context)
{
    /* Codes_SRS_CLDS_SORTED_LIST_01_036: [ `item_cleanup_callback` shall be allowed to be NULL. ]*/
    /* Codes_SRS_CLDS_SORTED_LIST_01_037: [ `item_cleanup_callback_context` shall be allowed to be NULL. ]*/

    void* result = malloc(node_size);
    if (result == NULL)
    {
        LogError("Failed allocating memory");
    }
    else
    {
        volatile CLDS_SORTED_LIST_ITEM* item = (volatile CLDS_SORTED_LIST_ITEM*)((unsigned char*)result);
        item->item_cleanup_callback = item_cleanup_callback;
        item->item_cleanup_callback_context = item_cleanup_callback_context;
        (void)InterlockedExchange(&item->ref_count, 1);
        (void)InterlockedExchangePointer((volatile PVOID*)&item->next, NULL);
    }

    return result;
}

int clds_sorted_list_node_inc_ref(CLDS_SORTED_LIST_ITEM* item)
{
    int result;

    if (item == NULL)
    {
        LogError("NULL item");
        result = __FAILURE__;
    }
    else
    {
        (void)InterlockedIncrement(&item->ref_count);
        result = 0;
    }

    return result;
}

void clds_sorted_list_node_release(CLDS_SORTED_LIST_ITEM* item)
{
    if (item == NULL)
    {
        LogError("NULL item");
    }
    else
    {
        internal_node_destroy(item);
    }
}