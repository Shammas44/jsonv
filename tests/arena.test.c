#include "arena.internal.h"
#include "except.h"
#include "utils.h"
#include <criterion/criterion.h>
#include <criterion/logging.h>
#include <criterion/new/assert.h>
#define T Jsonv_Arena

// Helper to cast opaque Jsonv_Arena* to our internal struct
Jsonv_Arena *get_internal(Jsonv_Arena *a) { return (Jsonv_Arena *)a; }
extern const Except ARENA_LIMIT_REACHED;

// -----------------------------------------------------------------------------
// TEST FIXTURES
// -----------------------------------------------------------------------------
Jsonv_Arena *arena = NULL;

static void init(void) {
  /*#region*/
  test_init();
  // Default setup: 4KB blocks, 1MB limit, 12KB trim threshold
  arena = jsonv_arena_new(4096, 1024 * 1024, 3 * 4096);
  /*#endregion*/
}

static void fini(void) {
  /*#region*/
  if (arena) {
    jsonv_arena_destroy(arena);
    arena = NULL;
  }
  test_fini();
  /*#endregion*/
}

// -----------------------------------------------------------------------------
// TEST CASES
// -----------------------------------------------------------------------------

TIMED_TEST(T, creation, init, fini)
/*#region*/
cr_assert(arena != NULL, "Jsonv_Arena should be created successfully");

Jsonv_Arena *internal = get_internal(arena);
cr_expect(arena_get_head(internal) != NULL, "Head block should be initialized");
cr_expect(arena_get_current(internal) == arena_get_head(internal), "Current should point to Head");
cr_expect(arena_get_total_reserved(internal) > 4096,
          "Total reserved should track struct overhead + buffer");
/*#endregion*/
END_TIMED_TEST

TIMED_TEST(T, basic_allocation, init, fini)
/*#region*/
void *ptr1 = jsonv_arena_alloc(arena, 100);
void *ptr2 = jsonv_arena_alloc(arena, 200);

cr_assert(ptr1 != NULL);
cr_assert(ptr2 != NULL);

// Ensure pointers are distinct and advancing
cr_expect(ptr2 > ptr1, "Pointer 2 should be after Pointer 1");

Jsonv_Arena *internal = get_internal(arena);
Jsonv_ArenaBlock* current = arena_get_current(internal);
cr_expect(arena_block_get_used(current) >= 300, "Used counter should increase");
/*#endregion*/
END_TIMED_TEST

TIMED_TEST(T, alignment, init, fini)
/*#region*/
// Allocate weird sizes to test alignment
void *p1 = jsonv_arena_alloc(arena, 1);
void *p2 = jsonv_arena_alloc(arena, 3);
void *p3 = jsonv_arena_alloc(arena, 11);

cr_expect_eq((uintptr_t)p1 % ARENA_ALIGNMENT, 0,
             "Pointer 1 should be 16-byte aligned");
cr_expect_eq((uintptr_t)p2 % ARENA_ALIGNMENT, 0,
             "Pointer 2 should be 16-byte aligned");
cr_expect_eq((uintptr_t)p3 % ARENA_ALIGNMENT, 0,
             "Pointer 3 should be 16-byte aligned");
/*#endregion*/
END_TIMED_TEST

TIMED_TEST(T, chaining_expansion, init, fini)
/*#region*/
Jsonv_Arena *internal = get_internal(arena);
          Jsonv_ArenaBlock *first_block = arena_get_current(internal);

// Fill the first block (Default 4096)
// We alloc 4000, leaving very little space.
void *p1 = jsonv_arena_alloc(arena, 4000);
cr_assert(p1 != NULL);

// This next allocation should force a new block creation
void *p2 = jsonv_arena_alloc(arena, 500);
cr_assert(p2 != NULL);

Jsonv_ArenaBlock * current = arena_get_current(internal);
cr_expect(current != first_block,
          "Jsonv_Arena should have moved to a new block");
cr_expect(arena_block_get_next(first_block) == current,
          "Old block should link to new block");
/*#endregion*/
END_TIMED_TEST

TIMED_TEST(T, memory_limit_enforcement, init, fini)
/*#region*/
// Re-create arena with very small limit for this test
jsonv_arena_destroy(arena);

// Limit: 5000 bytes. Default Block: 1000 bytes.
arena = jsonv_arena_new(1000, 5000, 5000);

// Consume ~4000 bytes (struct overhead will eat some logic space)
// Roughly 4 blocks
for (int i = 0; i < 3; i++) {
  cr_assert(jsonv_arena_alloc(arena, 800) != NULL, "Should fit in limit");
}

void *fail_ptr = NULL;
TRY {
  // This should push us over 5000 bytes (reserved)
  fail_ptr = jsonv_arena_alloc(arena, 2000);
}
EXCEPT(ARENA_LIMIT_REACHED) {}
END_TRY;
cr_assert(fail_ptr == NULL, "Alloc should fail when exceeding max_limit");
/*#endregion*/
END_TIMED_TEST

TIMED_TEST(T, reset_behavior, init, fini)
/*#region*/
void *p1 = jsonv_arena_alloc(arena, 100);
strcpy((char *)p1, "Test Data");

jsonv_arena_reset(arena);

// After reset, alloc should return the address of the start of the buffer
// again
void *p2 = jsonv_arena_alloc(arena, 100);

cr_expect(p1 == p2,
          "After reset, allocator should reuse the memory from the start");

// Check internal counters
Jsonv_Arena *internal = get_internal(arena);
cr_expect(arena_get_current(internal) == arena_get_head(internal), "Current should reset to head");
/*#endregion*/
END_TIMED_TEST

TIMED_TEST(T, smart_trim_logic, init, fini)
/*#region*/
// 1. Establish baseline memory usage
Jsonv_Arena *internal = get_internal(arena);
          size_t initial_reserved = arena_get_total_reserved(internal);

// 2. Allocate massively to exceed 'shrink_at' threshold
// Our 'shrink_at' is 12KB. Let's alloc 20KB.
for (int i = 0; i < 5; i++) {
  jsonv_arena_alloc(arena, 4096);
}

cr_expect(arena_get_total_reserved(internal) > initial_reserved, "Memory should have grown");
cr_expect(arena_get_total_reserved(internal) > arena_get_shrink_at(internal),
          "Should be above shrink threshold");

// 3. Reset (Trigger Trim)
jsonv_arena_reset(arena);

// 4. Verification
// Total reserved should drop back down to approximately the initial size
// (Head block only)
cr_expect(
  arena_get_total_reserved(internal) == initial_reserved,
    "Jsonv_Arena should have shrunk back to initial size. Got %zu, expected %zu",
  arena_get_total_reserved(internal), initial_reserved);

Jsonv_ArenaBlock *head = arena_get_head(internal);
cr_expect(arena_block_get_next(head) == NULL, "Extra blocks should be freed");
/*#endregion*/
END_TIMED_TEST

TIMED_TEST(T, large_allocation_handling, init, fini)
/*#region*/
// Request something larger than the default block size (4KB)
// This tests the code path: if (aligned_size > new_capacity)
void *huge = jsonv_arena_alloc(arena, 10000);

cr_assert(huge != NULL);

Jsonv_Arena *internal = get_internal(arena);
Jsonv_ArenaBlock*current = arena_get_current(internal);          
          cr_expect(arena_block_get_capacity(current) >= 10000,
          "Block capacity should adapt to huge request");
/*#endregion*/
END_TIMED_TEST

// -----------------------------------------------------------------------------
// CUSTOM ALLOCATOR TESTS
// -----------------------------------------------------------------------------

typedef struct {
  uint8_t buffer[1024];
  size_t offset;
  int reset_called;
  int reset_to_called;
  int destroy_called;
} CustomAllocTestCtx;

static void *custom_alloc_fn(void *user_data, size_t size) {
  CustomAllocTestCtx *ctx = (CustomAllocTestCtx *)user_data;
  size_t aligned = (size + 15) & ~15;
  if (ctx->offset + aligned > sizeof(ctx->buffer)) {
    return NULL;
  }
  void *ptr = &ctx->buffer[ctx->offset];
  ctx->offset += aligned;
  return ptr;
}

static void custom_reset_fn(void *user_data) {
  CustomAllocTestCtx *ctx = (CustomAllocTestCtx *)user_data;
  ctx->offset = 0;
  ctx->reset_called++;
}

static void custom_reset_to_fn(void *user_data, size_t keep_size) {
  CustomAllocTestCtx *ctx = (CustomAllocTestCtx *)user_data;
  ctx->offset = (keep_size + 15) & ~15;
  ctx->reset_to_called++;
}

static void custom_destroy_fn(void *user_data) {
  CustomAllocTestCtx *ctx = (CustomAllocTestCtx *)user_data;
  ctx->destroy_called++;
}

TIMED_TEST(T, custom_allocator, NULL, NULL)
/*#region*/
CustomAllocTestCtx ctx = {0};
Jsonv_Arena_Ops ops = {
  .alloc = custom_alloc_fn,
  .reset = custom_reset_fn,
  .reset_to = custom_reset_to_fn,
  .destroy = custom_destroy_fn
};

Jsonv_Arena *custom_arena = jsonv_arena_new_custom(&ops, &ctx);
cr_assert(custom_arena != NULL);

// 1. Test alloc
void *p1 = jsonv_arena_alloc(custom_arena, 10);
void *p2 = jsonv_arena_alloc(custom_arena, 20);
cr_assert(p1 != NULL);
cr_assert(p2 != NULL);
cr_expect(p2 > p1);
cr_expect_eq(ctx.offset, 48); // 16 (for 10 bytes) + 32 (for 20 bytes)

// 2. Test reset
jsonv_arena_reset(custom_arena);
cr_expect_eq(ctx.offset, 0);
cr_expect_eq(ctx.reset_called, 1);

// 3. Test reset_to
jsonv_arena_reset_to(custom_arena, 12);
cr_expect_eq(ctx.offset, 16);
cr_expect_eq(ctx.reset_to_called, 1);

// 4. Test destroy
jsonv_arena_destroy(custom_arena);
cr_expect_eq(ctx.destroy_called, 1);
/*#endregion*/
END_TIMED_TEST

static void *custom_alloc_err_fn(void *user_data, size_t size) {
  (void)size;
  int *custom_err = (int *)user_data;
  *custom_err = 42; // Set a custom decoupled error code in user space
  return NULL;
}

TIMED_TEST(T, custom_allocator_error_handling, NULL, NULL)
/*#region*/
  int error_code = 0;
  Jsonv_Arena_Ops ops = {
    .alloc = custom_alloc_err_fn,
    .reset = NULL,
    .reset_to = NULL,
    .destroy = NULL
  };

  Jsonv_Arena *custom_arena = jsonv_arena_new_custom(&ops, &error_code);
  cr_assert(custom_arena != NULL);

  // When custom allocator fails (returns NULL), the library maps it to JSONV_ARENA_ERR_ALLOC
  void *p = jsonv_arena_alloc(custom_arena, 10);
  cr_expect(p == NULL);
  cr_expect_eq(jsonv_last_arena_error, JSONV_ARENA_ERR_ALLOC);

  // The custom decoupled error code set in user space is preserved
  cr_expect_eq(error_code, 42);

  jsonv_arena_destroy(custom_arena);
/*#endregion*/
END_TIMED_TEST


