#include "arena.h"
#include "mem.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern const Except ARENA_LIMIT_REACHED;

static size_t align_up(size_t size) {
  /*#region*/
  return (size + (ARENA_ALIGNMENT - 1)) & ~(ARENA_ALIGNMENT - 1);
  /*#endregion*/
}

static ArenaBlock *arena_create_block(size_t capacity) {
  /*#region*/
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

  // Check initial limit
  size_t first_block_size = sizeof(ArenaBlock) + default_block_size;
  if (first_block_size > max_limit) {
    free(arena);
    return NULL;
  }

  arena->head = arena_create_block(default_block_size);
  if (!arena->head) {
    free(arena);
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

  // 1. Try current block
  if (arena->current->used + aligned_size <= arena->current->capacity) {
    void *ptr = arena->current->data + arena->current->used;
    arena->current->used += aligned_size;
    return ptr;
  }

  // 2. New block needed
  size_t new_capacity = arena->default_block_size;
  if (aligned_size > new_capacity) {
    new_capacity = aligned_size;
  }

  size_t block_struct_size = sizeof(ArenaBlock) + new_capacity;

  // 3. Security Check
  if (arena->total_reserved + block_struct_size > arena->max_limit) {
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
      // If the recycled block is too small, we must replace it.
      // (Simple strategy: destroy the old chain from here and make a new one)
      ArenaBlock *victim = arena->current->next;
      while (victim) {
        ArenaBlock *next = victim->next;
        arena->total_reserved -= (sizeof(ArenaBlock) + victim->capacity);
        free(victim);
        victim = next;
      }

      // Create fresh fitting block
      ArenaBlock *new_block = arena_create_block(new_capacity);
      if (!new_block)
        return NULL;

      arena->current->next = new_block;
      arena->total_reserved += block_struct_size;
    }
  }

  // 5. Advance
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
    // 1. Keep the HEAD, free the rest
    ArenaBlock *victim = arena->head->next;
    while (victim) {
      ArenaBlock *next = victim->next;
      free(victim);
      victim = next;
    }
    arena->head->next = NULL;

    // 2. Reset calculations
    arena->total_reserved = sizeof(ArenaBlock) + arena->head->capacity;
  } else {
    // STANDARD RESET (Fast path)
    // Just zero out the used counters, keep memory for reuse
    ArenaBlock *block = arena->head;
    while (block) {
      block->used = 0;
      block = block->next;
    }
  }

  // 3. Reset pointer to start
  arena->head->used = 0;
  arena->current = arena->head;
  /*#endregion*/
}

void arena_destroy(Arena *arena) {
  /*#region*/
  ArenaBlock *block = arena->head;
  while (block) {
    ArenaBlock *next = block->next;
    free(block);
    block = next;
  }
  free(arena);
  /*#endregion*/
}
