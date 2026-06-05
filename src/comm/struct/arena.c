#include "arena.internal.h"
#include "assert.h"
#include "mem.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct Jsonv_ArenaBlock {
  struct Jsonv_ArenaBlock *next;
  size_t capacity;
  size_t used;
  // The compiler will now insert 8 bytes of padding here automatically
  // so that 'data' starts at an offset divisible by 16.
  alignas(ARENA_ALIGNMENT) uint8_t data[];
} Jsonv_ArenaBlock;

typedef struct Jsonv_Arena {
  Jsonv_ArenaBlock *head;
  Jsonv_ArenaBlock *current;

  size_t default_block_size; // Standard allocation unit (e.g., 4KB)
  size_t max_limit; // Hard Ceiling: Stop parsing if we hit this (e.g., 10MB)
  size_t shrink_at; // Soft Ceiling: Free memory if we exceed this (e.g., 1MB)
  size_t total_reserved; // Current total tracked memory
} Jsonv_Arena;

extern const Except ARENA_LIMIT_REACHED;

static size_t align_up(size_t size) {
  /*#region*/
  // Prevent overflow during alignment math
  if (size > SIZE_MAX - (ARENA_ALIGNMENT - 1)) {
    return 0; // Indicates overflow
  }
  return (size + (ARENA_ALIGNMENT - 1)) & ~(ARENA_ALIGNMENT - 1);
  /*#endregion*/
}

static Jsonv_ArenaBlock *arena_create_block(size_t capacity) {
  /*#region*/
  // Safe overflow check for block structure allocation size
  if (capacity > SIZE_MAX - sizeof(Jsonv_ArenaBlock)) {
    return NULL;
  }
  Jsonv_ArenaBlock *block = ALLOC(sizeof(Jsonv_ArenaBlock) + capacity);
  if (block) {
    block->next = NULL;
    block->capacity = capacity;
    block->used = 0;
  }
  return block;
  /*#endregion*/
}

Jsonv_Arena *jsonv_arena_new(size_t default_block_size, size_t max_limit,
                       size_t shrink_at) {
  /*#region*/
  Jsonv_Arena *arena = ALLOC(sizeof(Jsonv_Arena));
  if (!arena)
    return NULL;

  arena->default_block_size = default_block_size;
  arena->max_limit = max_limit;
  arena->shrink_at = shrink_at;

  // Safe check for initial ceiling limits
  if (default_block_size > SIZE_MAX - sizeof(Jsonv_ArenaBlock)) {
    FREE(arena);
    return NULL;
  }
  size_t first_block_size = sizeof(Jsonv_ArenaBlock) + default_block_size;
  if (first_block_size > max_limit) {
    FREE(arena);
    return NULL;
  }

  arena->head = arena_create_block(default_block_size);
  if (!arena->head) {
    FREE(arena);
    return NULL;
  }

  arena->current = arena->head;
  arena->total_reserved = first_block_size;
  return arena;
  /*#endregion*/
}

void *jsonv_arena_alloc(Jsonv_Arena *arena, size_t size) {
  /*#region*/
  size_t aligned_size = align_up(size);
  if (aligned_size == 0 && size > 0) {
    return NULL; // Overflow detected
  }

  // 1. Try current block (prevents capacity - aligned_size underflow)
  if (aligned_size <= arena->current->capacity &&
      arena->current->used <= arena->current->capacity - aligned_size) {
    void *ptr = arena->current->data + arena->current->used;
    arena->current->used += aligned_size;
    return ptr;
  }

  // 2. New block needed
  size_t new_capacity = arena->default_block_size;
  if (aligned_size > new_capacity) {
    new_capacity = aligned_size;
  }

  if (new_capacity > SIZE_MAX - sizeof(Jsonv_ArenaBlock)) {
    return NULL;
  }
  size_t block_struct_size = sizeof(Jsonv_ArenaBlock) + new_capacity;

  // 3. Security Hard Limit Check (protects against addition overflows)
  if (arena->total_reserved > arena->max_limit - block_struct_size) {
    RAISE(ARENA_LIMIT_REACHED);
    return NULL;
  }

  // 4. Recycle or Create
  if (arena->current->next == NULL) {
    Jsonv_ArenaBlock *new_block = arena_create_block(new_capacity);
    if (!new_block)
      return NULL;

    arena->current->next = new_block;
    arena->total_reserved += block_struct_size;
  } else {
    // Reuse existing block (check if it fits)
    if (arena->current->next->capacity < aligned_size) {
      // OPTIMIZATION: Non-destructive insert.
      // We insert the larger block and preserve the downstream chain.
      Jsonv_ArenaBlock *new_block = arena_create_block(new_capacity);
      if (!new_block)
        return NULL;

      new_block->next = arena->current->next;
      arena->current->next = new_block;
      arena->total_reserved += block_struct_size;
    }
  }

  // 5. Advance (and overwrite .used)
  arena->current = arena->current->next;
  void *ptr = arena->current->data;
  arena->current->used = aligned_size;
  return ptr;
  /*#endregion*/
}

void jsonv_arena_reset(Jsonv_Arena *arena) {
  /*#region*/
  jsonv_arena_reset_to(arena, 0);
  /*#endregion*/
}

void jsonv_arena_reset_to(Jsonv_Arena *arena, size_t keep_size) {
  /*#region*/
  // SMART TRIM LOGIC
  if (arena->total_reserved > arena->shrink_at) {
    // 1. Keep the HEAD, free the rest using safe FREE macro
    Jsonv_ArenaBlock *victim = arena->head->next;
    while (victim) {
      Jsonv_ArenaBlock *next = victim->next;
      FREE(victim);
      victim = next;
    }
    arena->head->next = NULL;

    // 2. Reset calculations
    arena->total_reserved = sizeof(Jsonv_ArenaBlock) + arena->head->capacity;
  }

  // Align keep size
  size_t aligned = align_up(keep_size);
  if (aligned > arena->head->capacity) {
    aligned = arena->head->capacity;
  }

  // 3. Reset pointer to start + keep_size
  arena->head->used = aligned;
  arena->current = arena->head;
  /*#endregion*/
}

void jsonv_arena_destroy(Jsonv_Arena *arena) {
  /*#region*/
  if (!arena)
    return;

  Jsonv_ArenaBlock *block = arena->head;
  while (block) {
    Jsonv_ArenaBlock *next = block->next;
    FREE(block);
    block = next;
  }
  FREE(arena);
  /*#endregion*/
}

Jsonv_ArenaBlock *arena_get_head(Jsonv_Arena *arena) {
  /*#region*/
  assert(arena);
  return arena->head;
  /*#endregion*/
}

Jsonv_ArenaBlock *arena_get_current(Jsonv_Arena *arena) {
  /*#region*/
  assert(arena);
  return arena->current;
  /*#endregion*/
}

size_t arena_get_default_block_size(Jsonv_Arena *arena) {
  /*#region*/
  assert(arena);
  return arena->default_block_size;
  /*#endregion*/
}

size_t arena_get_max_limit(Jsonv_Arena *arena) {
  /*#region*/
  assert(arena);
  return arena->default_block_size;
  /*#endregion*/
}

size_t arena_get_shrink_at(Jsonv_Arena *arena) {
  /*#region*/
  assert(arena);
  return arena->shrink_at;
  /*#endregion*/
}

size_t arena_get_total_reserved(Jsonv_Arena *arena) {
  /*#region*/
  assert(arena);
  return arena->total_reserved;
  /*#endregion*/
}

Jsonv_ArenaBlock *arena_block_get_next(Jsonv_ArenaBlock *block) {
  /*#region*/
  assert(block);
  return block->next;
  /*#endregion*/
}

size_t arena_block_get_capacity(Jsonv_ArenaBlock *block) {
  /*#region*/
  assert(block);
  return block->capacity;
  /*#endregion*/
}

size_t arena_block_get_used(Jsonv_ArenaBlock *block) {
  /*#region*/
  assert(block);
  return block->used;
  /*#endregion*/
}
