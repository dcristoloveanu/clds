// Licensed under the MIT license.See LICENSE file in the project root for full license information.

#include <stdlib.h>
#include <stdbool.h>
#include "windows.h"
#include "azure_c_shared_utility/gballoc.h"
#include "azure_c_shared_utility/xlogging.h"
#include "clds/clds_hazard_pointers.h"
#include "clds/clds_atomics.h"

typedef struct CLDS_HAZARD_POINTER_RECORD_TAG
{
    void* node;
    struct CLDS_HAZARD_POINTER_RECORD_TAG* next;
} CLDS_HAZARD_POINTER_RECORD;

typedef struct CLDS_RECLAIM_LIST_ENTRY_TAG
{
    struct CLDS_RECLAIM_LIST_ENTRY_TAG* next;
    void* node;
    RECLAIM_FUNC reclaim;
} CLDS_RECLAIM_LIST_ENTRY;

typedef struct CLDS_HAZARD_POINTERS_THREAD_TAG
{
    volatile struct CLDS_HAZARD_POINTERS_THREAD_TAG* next;
    CLDS_HAZARD_POINTERS_HANDLE clds_hazard_pointers;
    CLDS_HAZARD_POINTER_RECORD* free_pointers;
    CLDS_HAZARD_POINTER_RECORD* pointers;
    CLDS_RECLAIM_LIST_ENTRY* reclaim_list;
    volatile LONG active;
} CLDS_HAZARD_POINTERS_THREAD;

typedef struct CLDS_HAZARD_POINTERS_TAG
{
    volatile CLDS_HAZARD_POINTERS_THREAD* head;
} CLDS_HAZARD_POINTERS;

CLDS_HAZARD_POINTERS_HANDLE clds_hazard_pointers_create(void)
{
    CLDS_HAZARD_POINTERS_HANDLE clds_hazard_pointers;

    clds_hazard_pointers = (CLDS_HAZARD_POINTERS_HANDLE)malloc(sizeof(CLDS_HAZARD_POINTERS));
    if (clds_hazard_pointers == NULL)
    {
        LogError("Cannot allocate memory for hazard pointers");
    }
    else
    {
        (void)InterlockedExchangePointer((volatile PVOID*)&clds_hazard_pointers->head, NULL);
    }

    return clds_hazard_pointers;
}

void clds_hazard_pointers_destroy(CLDS_HAZARD_POINTERS_HANDLE clds_hazard_pointers)
{
    if (clds_hazard_pointers == NULL)
    {
        LogError("NULL clds_hazard_pointers");
    }
    else
    {
        // free all thread data here
        CLDS_HAZARD_POINTERS_THREAD_HANDLE clds_hazard_pointers_thread = (CLDS_HAZARD_POINTERS_THREAD_HANDLE)InterlockedCompareExchangePointer((volatile PVOID*)&clds_hazard_pointers->head, NULL, NULL);
        while (clds_hazard_pointers_thread != NULL)
        {
            CLDS_HAZARD_POINTERS_THREAD_HANDLE next_clds_hazard_pointers_thread = InterlockedCompareExchangePointer((volatile PVOID*)&clds_hazard_pointers_thread->next, NULL, NULL);
            CLDS_HAZARD_POINTER_RECORD_HANDLE hazard_ptr = InterlockedCompareExchangePointer((volatile PVOID*)&clds_hazard_pointers_thread->pointers, NULL, NULL);
            while (hazard_ptr != NULL)
            {
                CLDS_HAZARD_POINTER_RECORD_HANDLE next_hazard_ptr = InterlockedCompareExchangePointer((volatile PVOID*)&hazard_ptr->next, NULL, NULL);
                free(hazard_ptr);
                hazard_ptr = next_hazard_ptr;
            }

            free(clds_hazard_pointers_thread);
            clds_hazard_pointers_thread = next_clds_hazard_pointers_thread;
        }

        free(clds_hazard_pointers);
    }
}

CLDS_HAZARD_POINTERS_THREAD_HANDLE clds_hazard_pointers_register_thread(CLDS_HAZARD_POINTERS_HANDLE clds_hazard_pointers)
{
    CLDS_HAZARD_POINTERS_THREAD_HANDLE clds_hazard_pointers_thread = malloc(sizeof(CLDS_HAZARD_POINTERS_THREAD));
    if (clds_hazard_pointers_thread == NULL)
    {
        LogError("Failed allocating memory for hazard pointer thread");
    }
    else
    {
        bool restart_needed;

        clds_hazard_pointers_thread->clds_hazard_pointers = clds_hazard_pointers;
        do
        {
            CLDS_HAZARD_POINTERS_THREAD_HANDLE current_threads_head = (CLDS_HAZARD_POINTERS_THREAD_HANDLE)InterlockedCompareExchangePointer((volatile PVOID*)&clds_hazard_pointers->head, NULL, NULL);
            clds_hazard_pointers_thread->next = current_threads_head;
            (void)InterlockedExchangePointer(&clds_hazard_pointers_thread->pointers, NULL);
            (void)InterlockedExchangePointer(&clds_hazard_pointers_thread->free_pointers, NULL);
            (void)InterlockedExchangePointer(&clds_hazard_pointers_thread->reclaim_list, NULL);
            (void)InterlockedExchange(&clds_hazard_pointers_thread->active, 1);
            if (InterlockedCompareExchangePointer((volatile PVOID*)&clds_hazard_pointers->head, clds_hazard_pointers_thread, current_threads_head) != current_threads_head)
            {
                restart_needed = true;
            }
            else
            {
                // done
                restart_needed = false;
            }
        } while (restart_needed);
    }

    return clds_hazard_pointers_thread;
}

void clds_hazard_pointers_unregister_thread(CLDS_HAZARD_POINTERS_THREAD_HANDLE clds_hazard_pointers_thread)
{
    if (clds_hazard_pointers_thread == NULL)
    {
        LogError("NULL clds_hazard_pointers_thread");
    }
    else
    {
        // remove the thread from the thread list
        (void)InterlockedExchange(&clds_hazard_pointers_thread->active, 0);
    }
}

CLDS_HAZARD_POINTER_RECORD_HANDLE clds_hazard_pointers_acquire(CLDS_HAZARD_POINTERS_THREAD_HANDLE clds_hazard_pointers_thread, void* node)
{
    CLDS_HAZARD_POINTER_RECORD_HANDLE result;

    if (clds_hazard_pointers_thread == NULL)
    {
        LogError("NULL clds_hazard_pointers_thread");
        result = NULL;
    }
    else
    {
        bool restart_needed;
        CLDS_HAZARD_POINTER_RECORD_HANDLE hazard_ptr;

        // get a hazard pointer for the node from the free list
        do
        {
            hazard_ptr = InterlockedCompareExchangePointer((volatile PVOID*)&clds_hazard_pointers_thread->free_pointers, NULL, NULL);
            if (hazard_ptr != NULL)
            {
                if (InterlockedCompareExchangePointer((volatile PVOID*)&clds_hazard_pointers_thread->free_pointers, NULL, NULL) != hazard_ptr)
                {
                    restart_needed = true;
                }
                else
                {
                    // got it
                    restart_needed = false;
                }
            }
            else
            {
                // no free one
                restart_needed = false;
            }
        }
        while (restart_needed);

        if (hazard_ptr == NULL)
        {
            // no more pointers in free list, create one
            hazard_ptr = malloc(sizeof(CLDS_HAZARD_POINTER_RECORD));
            if (hazard_ptr == NULL)
            {
                LogError("Error allocating hazard pointer");
                result = NULL;
            }
            else
            {
                CLDS_HAZARD_POINTER_RECORD* current_list_head;

                // add it to the hazard pointer list
                current_list_head = InterlockedCompareExchangePointer(&clds_hazard_pointers_thread->pointers, NULL, NULL);

                hazard_ptr->node = node;
                hazard_ptr->next = current_list_head;
                
                (void)InterlockedExchangePointer(&clds_hazard_pointers_thread->pointers, hazard_ptr);

                // inserted in the used hazard pointer list, we are done
                result = hazard_ptr;
            }
        }
        else
        {
            // the pointer is now set as a hazard pointer
            result = hazard_ptr;
        }
    }

    return result;
}

void clds_hazard_pointers_release(CLDS_HAZARD_POINTERS_THREAD_HANDLE clds_hazard_pointers_thread, CLDS_HAZARD_POINTER_RECORD_HANDLE clds_hazard_pointer_record)
{
    if (clds_hazard_pointer_record == NULL)
    {
        LogError("NULL hazard pointer");
    }
    else
    {
        (void)clds_hazard_pointers_thread;
        (void)InterlockedExchangePointer(&clds_hazard_pointer_record->node, NULL);

        // here we should remove the hazard pointer from the list
    }
}

void clds_hazard_pointers_reclaim(CLDS_HAZARD_POINTERS_THREAD_HANDLE clds_hazard_pointers_thread, void* node, RECLAIM_FUNC reclaim_func)
{
    if ((clds_hazard_pointers_thread == NULL) ||
        (node == NULL))
    {
        LogError("NULL node");
    }
    else
    {
        CLDS_RECLAIM_LIST_ENTRY* reclaim_list_entry = (CLDS_RECLAIM_LIST_ENTRY*)malloc(sizeof(CLDS_RECLAIM_LIST_ENTRY));
        if (reclaim_list_entry == NULL)
        {
            // oops, panic now!
            LogError("Cannot allocate reclaim list entry");
        }
        else
        {
            CLDS_HAZARD_POINTERS_HANDLE clds_hazard_pointers = clds_hazard_pointers_thread->clds_hazard_pointers;

            reclaim_list_entry->next = clds_hazard_pointers_thread->reclaim_list;
            reclaim_list_entry->node = node;
            reclaim_list_entry->reclaim = reclaim_func;

            // add the pointer to the reclaim list, no other thread has access to this list, so no Interlocked needed
            clds_hazard_pointers_thread->reclaim_list = reclaim_list_entry;

            // go through all pointers in the reclaim list
            CLDS_RECLAIM_LIST_ENTRY* current_reclaim_entry = clds_hazard_pointers_thread->reclaim_list;
            CLDS_RECLAIM_LIST_ENTRY* prev_reclaim_entry = NULL;
            while (current_reclaim_entry != NULL)
            {
                // this is the scan for the pointers
                bool reclaim_node = true;

                // go through all hazard pointers of all threads, no thread should be able to get a hazard pointer after this point
                CLDS_HAZARD_POINTERS_THREAD_HANDLE current_thread = (CLDS_HAZARD_POINTERS_THREAD_HANDLE)InterlockedCompareExchangePointer((volatile PVOID*)&clds_hazard_pointers->head, NULL, NULL);
                while (current_thread != NULL)
                {
                    CLDS_HAZARD_POINTERS_THREAD_HANDLE next_thread = (CLDS_HAZARD_POINTERS_THREAD_HANDLE)InterlockedCompareExchangePointer((volatile PVOID*)&current_thread->next, NULL, NULL);
                    if (InterlockedAdd(&current_thread->active, 0) == 1)
                    {
                        // look at the pointers of this thread, if it gets unregistered in the meanwhile we won't care
                        // if it gets registered again we also don't care as for sure it does not have our hazard pointer anymore

                        CLDS_HAZARD_POINTER_RECORD_HANDLE clds_hazard_pointer = InterlockedCompareExchangePointer(&current_thread->pointers, NULL, NULL);
                        while (clds_hazard_pointer != NULL)
                        {
                            CLDS_HAZARD_POINTER_RECORD_HANDLE next_hazard_pointer = InterlockedCompareExchangePointer(&clds_hazard_pointer->next, NULL, NULL);

                            if (InterlockedCompareExchangePointer(&clds_hazard_pointer->node, NULL, NULL) == current_reclaim_entry->node)
                            {
                                // leave it in the reclaim list
                                break;
                            }
                            else
                            {
                                // go on ...
                                clds_hazard_pointer = next_hazard_pointer;
                            }
                        }

                        if (clds_hazard_pointer != NULL)
                        {
                            // found, oops, we should not reclaim this one, leave it in the reclaim list
                            reclaim_node = false;
                            break;
                        }
                        else
                        {
                            // not found, go to the next thread
                        }
                    }
                    else
                    {
                        // not active, skip to the next thread
                    }

                    current_thread = next_thread;
                }

                if (reclaim_node)
                {
                    // node is safe to be reclaimed
                    current_reclaim_entry->reclaim(current_reclaim_entry->node);

                    // now remove it from the reclaim list
                    if (prev_reclaim_entry == NULL)
                    {
                        // this is the head of the reclaim list
                        clds_hazard_pointers_thread->reclaim_list = current_reclaim_entry->next;
                        free(current_reclaim_entry);
                        current_reclaim_entry = clds_hazard_pointers_thread->reclaim_list;
                    }
                    else
                    {
                        free(current_reclaim_entry);
                        prev_reclaim_entry->next = current_reclaim_entry->next;
                    }
                }
                else
                {
                    // not safe, sorry, shall still have it around, move to next node
                    prev_reclaim_entry = current_reclaim_entry;
                    current_reclaim_entry = current_reclaim_entry->next;
                }
            }
        }
    }
}
