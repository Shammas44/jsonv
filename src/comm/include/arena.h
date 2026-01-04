#ifndef _JSONV_ARENA_H_INCLUDED
#define _JSONV_ARENA_H_INCLUDED
#include <stdalign.h>
#include <stddef.h>
#include <stdint.h>

#define ARENA_ALIGNMENT 16

typedef struct ArenaBlock {
  struct ArenaBlock *next;
  size_t capacity;
  size_t used;
  // The compiler will now insert 8 bytes of padding here automatically
  // so that 'data' starts at an offset divisible by 16.
  alignas(ARENA_ALIGNMENT) uint8_t data[];
} ArenaBlock;

typedef struct {
  ArenaBlock *head;
  ArenaBlock *current;

  size_t default_block_size; // Standard allocation unit (e.g., 4KB)
  size_t max_limit; // Hard Ceiling: Stop parsing if we hit this (e.g., 10MB)
  size_t shrink_at; // Soft Ceiling: Free memory if we exceed this (e.g., 1MB)
  size_t total_reserved; // Current total tracked memory
} Arena;

/**
 * @param default_block_size Size of new blocks (recommend 4096)
 * @param max_limit          Total memory allowed before returning NULL
 * (Security)
 * @param shrink_at          If memory exceeds this, free excess on reset
 * (Optimization)
 */
Arena *arena_create(size_t default_block_size, size_t max_limit,
                    size_t shrink_at);
void *arena_alloc(Arena *arena, size_t size);
void arena_reset(Arena *arena);
void arena_destroy(Arena *arena);

#endif
