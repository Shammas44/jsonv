#include "arena.h"
#include "mem.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

static ArenaBlock *arena_create_block(size_t capacity) {
  /*#region*/
  // Safe overflow check for block structure allocation size
  if (capacity > SIZE_MAX - sizeof(ArenaBlock)) {
    return NULL;
  }
  ArenaBlock *block = ALLOC(sizeof(ArenaBlock) + capacity);
  if (block) {
    block->next = NULL;
    block->capacity = capacity;
    block->used = 0;
  }
  return block;
  /*#endregion*/
}

Arena *arena_new(size_t default_block_size, size_t max_limit,
                 size_t shrink_at) {
  /*#region*/
  Arena *arena = ALLOC(sizeof(Arena));
  if (!arena)
    return NULL;

  arena->default_block_size = default_block_size;
  arena->max_limit = max_limit;
  arena->shrink_at = shrink_at;

  // Safe check for initial ceiling limits
  if (default_block_size > SIZE_MAX - sizeof(ArenaBlock)) {
    FREE(arena);
    return NULL;
  }
  size_t first_block_size = sizeof(ArenaBlock) + default_block_size;
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

void *arena_alloc(Arena *arena, size_t size) {
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

  if (new_capacity > SIZE_MAX - sizeof(ArenaBlock)) {
    return NULL;
  }
  size_t block_struct_size = sizeof(ArenaBlock) + new_capacity;

  // 3. Security Hard Limit Check (protects against addition overflows)
  if (arena->total_reserved > arena->max_limit - block_struct_size) {
    RAISE(ARENA_LIMIT_REACHED);
    return NULL;
  }

  // 4. Recycle or Create
  if (arena->current->next == NULL) {
    ArenaBlock *new_block = arena_create_block(new_capacity);
    if (!new_block)
      return NULL;

    arena->current->next = new_block;
    arena->total_reserved += block_struct_size;
  } else {
    // Reuse existing block (check if it fits)
    if (arena->current->next->capacity < aligned_size) {
      // OPTIMIZATION: Non-destructive insert.
      // We insert the larger block and preserve the downstream chain.
      ArenaBlock *new_block = arena_create_block(new_capacity);
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

void arena_reset(Arena *arena) {
  /*#region*/
  // SMART TRIM LOGIC
  if (arena->total_reserved > arena->shrink_at) {
    // 1. Keep the HEAD, free the rest using safe FREE macro
    ArenaBlock *victim = arena->head->next;
    while (victim) {
      ArenaBlock *next = victim->next;
      FREE(victim);
      victim = next;
    }
    arena->head->next = NULL;

    // 2. Reset calculations
    arena->total_reserved = sizeof(ArenaBlock) + arena->head->capacity;
  } else {
    // OPTIMIZATION: Fast O(1) path.
    // No loop traversal needed! Downstream .used is safely overwritten on demand.
  }

  // 3. Reset pointer to start
  arena->head->used = 0;
  arena->current = arena->head;
  /*#endregion*/
}

void arena_destroy(Arena *arena) {
  /*#region*/
  if (!arena)
    return;

  ArenaBlock *block = arena->head;
  while (block) {
    ArenaBlock *next = block->next;
    FREE(block);
    block = next;
  }
  FREE(arena);
  /*#endregion*/
}
