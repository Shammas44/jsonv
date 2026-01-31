#include "arena.h"  // Your header file
#include "except.h" // Your header file
#include <criterion/criterion.h>
#include <criterion/logging.h>
#include <criterion/new/assert.h>

// Helper to cast opaque Arena* to our internal struct
Arena *get_internal(Arena *a) { return (Arena *)a; }
extern const Except ARENA_LIMIT_REACHED;

// -----------------------------------------------------------------------------
// TEST FIXTURES
// -----------------------------------------------------------------------------
Arena *arena = NULL;

void setup(void) {
  // Default setup: 4KB blocks, 1MB limit, 12KB trim threshold
  arena = arena_new(4096, 1024 * 1024, 3 * 4096);
}

void teardown(void) {
  if (arena) {
    arena_destroy(arena);
    arena = NULL;
  }
}

// -----------------------------------------------------------------------------
// TEST CASES
// -----------------------------------------------------------------------------

Test(arena, creation, .init = setup, .fini = teardown) {
  cr_assert(arena != NULL, "Arena should be created successfully");

  Arena *internal = get_internal(arena);
  cr_expect(internal->head != NULL, "Head block should be initialized");
  cr_expect(internal->current == internal->head,
            "Current should point to Head");
  cr_expect(internal->total_reserved > 4096,
            "Total reserved should track struct overhead + buffer");
}

Test(arena, basic_allocation, .init = setup, .fini = teardown) {
  void *ptr1 = arena_alloc(arena, 100);
  void *ptr2 = arena_alloc(arena, 200);

  cr_assert(ptr1 != NULL);
  cr_assert(ptr2 != NULL);

  // Ensure pointers are distinct and advancing
  cr_expect(ptr2 > ptr1, "Pointer 2 should be after Pointer 1");

  Arena *internal = get_internal(arena);
  cr_expect(internal->current->used >= 300, "Used counter should increase");
}

Test(arena, alignment, .init = setup, .fini = teardown) {
  // Allocate weird sizes to test alignment
  void *p1 = arena_alloc(arena, 1);
  void *p2 = arena_alloc(arena, 3);
  void *p3 = arena_alloc(arena, 11);

  cr_expect_eq((uintptr_t)p1 % ARENA_ALIGNMENT, 0,
               "Pointer 1 should be 16-byte aligned");
  cr_expect_eq((uintptr_t)p2 % ARENA_ALIGNMENT, 0,
               "Pointer 2 should be 16-byte aligned");
  cr_expect_eq((uintptr_t)p3 % ARENA_ALIGNMENT, 0,
               "Pointer 3 should be 16-byte aligned");
}

Test(arena, chaining_expansion, .init = setup, .fini = teardown) {
  Arena *internal = get_internal(arena);
  ArenaBlock *first_block = internal->current;

  // Fill the first block (Default 4096)
  // We alloc 4000, leaving very little space.
  void *p1 = arena_alloc(arena, 4000);
  cr_assert(p1 != NULL);

  // This next allocation should force a new block creation
  void *p2 = arena_alloc(arena, 500);
  cr_assert(p2 != NULL);

  cr_expect(internal->current != first_block,
            "Arena should have moved to a new block");
  cr_expect(first_block->next == internal->current,
            "Old block should link to new block");
}

Test(arena, memory_limit_enforcement, .init = setup, .fini = teardown) {
  // Re-create arena with very small limit for this test
  arena_destroy(arena);

  // Limit: 5000 bytes. Default Block: 1000 bytes.
  arena = arena_new(1000, 5000, 5000);

  // Consume ~4000 bytes (struct overhead will eat some logic space)
  // Roughly 4 blocks
  for (int i = 0; i < 3; i++) {
    cr_assert(arena_alloc(arena, 800) != NULL, "Should fit in limit");
  }

  void *fail_ptr = NULL;
  TRY {
    // This should push us over 5000 bytes (reserved)
    fail_ptr = arena_alloc(arena, 2000);
  }
  EXCEPT(ARENA_LIMIT_REACHED) {}
  END_TRY;
  cr_assert(fail_ptr == NULL, "Alloc should fail when exceeding max_limit");
}

Test(arena, reset_behavior, .init = setup, .fini = teardown) {
  void *p1 = arena_alloc(arena, 100);
  strcpy((char *)p1, "Test Data");

  arena_reset(arena);

  // After reset, alloc should return the address of the start of the buffer
  // again
  void *p2 = arena_alloc(arena, 100);

  cr_expect(p1 == p2,
            "After reset, allocator should reuse the memory from the start");

  // Check internal counters
  Arena *internal = get_internal(arena);
  cr_expect(internal->current == internal->head,
            "Current should reset to head");
}

Test(arena, smart_trim_logic, .init = setup, .fini = teardown) {
  // 1. Establish baseline memory usage
  Arena *internal = get_internal(arena);
  size_t initial_reserved = internal->total_reserved;

  // 2. Allocate massively to exceed 'shrink_at' threshold
  // Our 'shrink_at' is 12KB. Let's alloc 20KB.
  for (int i = 0; i < 5; i++) {
    arena_alloc(arena, 4096);
  }

  cr_expect(internal->total_reserved > initial_reserved,
            "Memory should have grown");
  cr_expect(internal->total_reserved > internal->shrink_at,
            "Should be above shrink threshold");

  // 3. Reset (Trigger Trim)
  arena_reset(arena);

  // 4. Verification
  // Total reserved should drop back down to approximately the initial size
  // (Head block only)
  cr_expect(
      internal->total_reserved == initial_reserved,
      "Arena should have shrunk back to initial size. Got %zu, expected %zu",
      internal->total_reserved, initial_reserved);

  cr_expect(internal->head->next == NULL, "Extra blocks should be freed");
}

Test(arena, large_allocation_handling, .init = setup, .fini = teardown) {
  // Request something larger than the default block size (4KB)
  // This tests the code path: if (aligned_size > new_capacity)
  void *huge = arena_alloc(arena, 10000);

  cr_assert(huge != NULL);

  Arena *internal = get_internal(arena);
  cr_expect(internal->current->capacity >= 10000,
            "Block capacity should adapt to huge request");
}
