#ifndef _JSONV_ARENA_INTERNAL_H
#define _JSONV_ARENA_INTERNAL_H
#include <stdalign.h>
#include <stddef.h>
#include <stdint.h>
#include "arena.h"

#define T Jsonv_Arena
#define ARENA_ALIGNMENT 16

typedef struct Jsonv_ArenaBlock Jsonv_ArenaBlock;

// =========== Arena getter
Jsonv_ArenaBlock *arena_get_head(T *arena);
Jsonv_ArenaBlock *arena_get_current(T *arena);
size_t arena_get_default_block_size(T *arena);
size_t arena_get_max_limit(T *arena);
size_t arena_get_shrink_at(T *arena);
size_t arena_get_total_reserved(T *arena);

// =========== Arena block getter
Jsonv_ArenaBlock *arena_block_get_next(Jsonv_ArenaBlock *block);
size_t arena_block_get_capacity(Jsonv_ArenaBlock *block);
size_t arena_block_get_used(Jsonv_ArenaBlock *block);

#undef T
#endif
