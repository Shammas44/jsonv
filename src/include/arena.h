#ifndef _JSONV_ARENA_H
#define _JSONV_ARENA_H

#include "macro.h"
#include <stdalign.h>
#include <stddef.h>
#include <stdint.h>

#define T Jsonv_Arena
#define JSONV_ARENA_ALIGNMENT 16

typedef struct T T;

/**
 * @description Allocate a new arena on heap
 * @param default_block_size Size of new blocks (recommend 4096)
 * @param max_limit          Total memory allowed before returning NULL
 * (Security)
 * @param shrink_at          If memory exceeds this, free excess on reset
 * (Optimization)
 * @return Opaque pointer to allocated arena
 */
JSONV_API T *jsonv_arena_new(size_t default_block_size, size_t max_limit,
                             size_t shrink_at);
/**
 * @description Allocate space on an arena
 * @param size number of bytes to allocate
 * @return pointer to the allocated memory
 */
JSONV_API void *jsonv_arena_alloc(T *arena, size_t size);
/**
 * @description Erease all arena's blocks data
 */
JSONV_API void jsonv_arena_reset(T *arena);
JSONV_API void jsonv_arena_reset_to(T *arena, size_t keep_size);
/**
 * @description Deallocate an arena
 */
JSONV_API void jsonv_arena_destroy(T *arena);

#undef T
#endif
