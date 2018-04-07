// Licensed under the MIT license.See LICENSE file in the project root for full license information.

#ifndef REAL_CLDS_ST_HASH_SET_H
#define REAL_CLDS_ST_HASH_SET_H

#include "macro_utils.h"
#include "clds/clds_st_hash_set.h"

#define R2(X) REGISTER_GLOBAL_MOCK_HOOK(X, real_##X);

#define REGISTER_CLDS_ST_HASH_SET_GLOBAL_MOCK_HOOKS() \
    FOR_EACH_1(R2, \
        clds_st_hash_set_create, \
        clds_st_hash_set_destroy, \
        clds_st_hash_set_insert, \
        clds_st_hash_set_find \
    )

#ifdef __cplusplus
#include <cstddef>
extern "C"
{
#else
#include <stddef.h>
#endif

CLDS_ST_HASH_SET_HANDLE real_clds_st_hash_set_create(CLDS_ST_HASH_SET_COMPUTE_HASH_FUNC compute_hash, size_t initial_bucket_size);
void real_clds_st_hash_set_destroy(CLDS_ST_HASH_SET_HANDLE clds_st_hash_set);
int real_clds_st_hash_set_insert(CLDS_ST_HASH_SET_HANDLE clds_st_hash_set, void* key);
int real_clds_st_hash_set_find(CLDS_ST_HASH_SET_HANDLE clds_st_hash_set, void* key, bool* key_found);

#ifdef __cplusplus
}
#endif

#endif // REAL_CLDS_ST_HASH_SET_H