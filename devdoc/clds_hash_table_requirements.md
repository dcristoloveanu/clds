# clds_hash_table requirements

## Overview

`clds_hash_table` is module that implements a hash table.
The module provides the following functionality:
- Inserting items in the hash table
- Delete an item from the hash table by its key

All operations can be concurrent with other operations of the same or different kind.

## Exposed API

```c
typedef struct CLDS_HASH_TABLE_TAG* CLDS_HASH_TABLE_HANDLE;
typedef uint64_t (*COMPUTE_HASH_FUNC)(void* key);

MOCKABLE_FUNCTION(, CLDS_HASH_TABLE_HANDLE, clds_hash_table_create, COMPUTE_HASH_FUNC, compute_hash, size_t, initial_bucket_size, CLDS_HAZARD_POINTERS_HANDLE, clds_hazard_pointers);
MOCKABLE_FUNCTION(, void, clds_hash_table_destroy, CLDS_HASH_TABLE_HANDLE, clds_hash_table);
MOCKABLE_FUNCTION(, int, clds_hash_table_insert, CLDS_HASH_TABLE_HANDLE, clds_hash_table, void*, key, void*, value, CLDS_HAZARD_POINTERS_THREAD_HANDLE, clds_hazard_pointers_thread);
MOCKABLE_FUNCTION(, int, clds_hash_table_delete, CLDS_HASH_TABLE_HANDLE, clds_hash_table, void*, key, CLDS_HAZARD_POINTERS_THREAD_HANDLE, clds_hazard_pointers_thread);
```

### clds_hash_table_create

```c
MOCKABLE_FUNCTION(, CLDS_HASH_TABLE_HANDLE, clds_hash_table_create, COMPUTE_HASH_FUNC, compute_hash, size_t, initial_bucket_size, CLDS_HAZARD_POINTERS_HANDLE, clds_hazard_pointers);
```

**SRS_CLDS_HASH_TABLE_01_001: [** `clds_hash_table_create` shall create a new hash table object and on success it shall return a non-NULL handle to the newly created hash table. **]**

**SRS_CLDS_HASH_TABLE_01_002: [** If any error happens, `clds_hash_table_create` shall fail and return NULL. **]**

**SRS_CLDS_HASH_TABLE_01_003: [** If `compute_hash` is NULL, `clds_hash_table_create` shall fail and return NULL. **]**

**SRS_CLDS_HASH_TABLE_01_004: [** If `initial_bucket_size` is 0, `clds_hash_table_create` shall fail and return NULL. **]**

**SRS_CLDS_HASH_TABLE_01_005: [** If `clds_hazard_pointers` is NULL, `clds_hash_table_create` shall fail and return NULL. **]**

### clds_hazard_pointers_destroy

```c
MOCKABLE_FUNCTION(, void, clds_hash_table_destroy, CLDS_HASH_TABLE_HANDLE, clds_hash_table);
```

**SRS_CLDS_HASH_TABLE_01_006: [** `clds_hash_table_destroy` shall free all resources associated with the hash table instance. **]**

**SRS_CLDS_HASH_TABLE_01_007: [** If `clds_hash_table` is NULL, `clds_hash_table_destroy` shall return. **]**

### clds_hash_table_insert

```c
MOCKABLE_FUNCTION(, int, clds_hash_table_insert, CLDS_HASH_TABLE_HANDLE, clds_hash_table, void*, key, void*, value, CLDS_HAZARD_POINTERS_THREAD_HANDLE, clds_hazard_pointers_thread);
```

**SRS_CLDS_HASH_TABLE_01_008: [** `clds_hash_table_insert` shall insert a key/value pair in the hash table. **]**

**SRS_CLDS_HASH_TABLE_01_009: [** On success `clds_hash_table_insert` shall return 0. **]**

**SRS_CLDS_HASH_TABLE_01_010: [** If `clds_hash_table` is NULL, `clds_hash_table_insert` shall fail and return a non-zero value. **]**

**SRS_CLDS_HASH_TABLE_01_011: [** If `key` is NULL, `clds_hash_table_insert` shall fail and return a non-zero value. **]**

**SRS_CLDS_HASH_TABLE_01_012: [** If `clds_hazard_pointers_thread` is NULL, `clds_hash_table_insert` shall fail and return a non-zero value. **]**

### clds_hash_table_delete

```c
MOCKABLE_FUNCTION(, int, clds_hash_table_delete, CLDS_HASH_TABLE_HANDLE, clds_hash_table, void*, key, CLDS_HAZARD_POINTERS_THREAD_HANDLE, clds_hazard_pointers_thread);
```

**SRS_CLDS_HASH_TABLE_01_013: [** `clds_hash_table_insert` shall delete a key from the hash table. **]**

**SRS_CLDS_HASH_TABLE_01_014: [** On success `clds_hash_table_delete` shall return 0. **]**

**SRS_CLDS_HASH_TABLE_01_015: [** If `clds_hash_table` is NULL, `clds_hash_table_delete` shall fail and return a non-zero value. **]**

**SRS_CLDS_HASH_TABLE_01_016: [** If `key` is NULL, `clds_hash_table_delete` shall fail and return a non-zero value. **]**

**SRS_CLDS_HASH_TABLE_01_017: [** If `clds_hazard_pointers_thread` is NULL, `clds_hash_table_delete` shall fail and return a non-zero value. **]**