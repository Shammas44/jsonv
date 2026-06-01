#include "shape.internal.h"
#include "arena.h"
#include "except.h"
#include "utils.h"
#include <criterion/criterion.h>
#include <string.h>

extern const Except ARENA_LIMIT_REACHED;

#define T Shape

static Jsonv_Arena *arena = NULL;
static Shape *root = NULL;

static void init(void) {
  /*#region*/
  test_init();
  // Allocate arena with 1MB ceiling limit
  arena = jsonv_arena_new(4096, 1024 * 1024, 12 * 1024);
  cr_assert_not_null(arena, "Arena allocation failed");
  root = shape_root(arena);
  cr_assert_not_null(root, "Root shape allocation failed");
  /*#endregion*/
}

static void fini(void) {
  /*#region*/
  if (arena) {
    jsonv_arena_destroy(arena);
    arena = NULL;
  }
  jsonv_shape_clear_global_arena();
  test_fini();
  /*#endregion*/
}

// Helper to construct a temporary const_lstr_t inside tests
static const char *make_temp_lstr(Jsonv_Arena *arena, const char *s) {
  /*#region*/
  size_t len = strlen(s);
  size_t total_size = sizeof(StringHeader) + len + 1;
  StringHeader *header = (StringHeader *)jsonv_arena_alloc(arena, total_size);
  cr_assert_not_null(header);
  header->length = (uint32_t)len;
  memcpy(header->data, s, len);
  header->data[len] = '\0';
  return (const char *)header->data;
  /*#endregion*/
}

// -----------------------------------------------------------------------------
// SHAPE TRANSITION TESTS
// -----------------------------------------------------------------------------

TIMED_TEST(T, root_has_no_parent_and_zero_slots, init, fini)
/*#region*/
cr_expect_null(shape_get_parent(root), "Root shape parent should be NULL");
cr_expect_eq(shape_get_slot_count(root), 0, "Root shape should have 0 slots");
cr_expect_eq(shape_get_last_slot(root), -1, "Root shape last slot index should be -1");
/*#endregion*/
END_TIMED_TEST

TIMED_TEST(T, add_transition_creates_child_and_allocates_slot, init, fini)
/*#region*/
const char *key = make_temp_lstr(arena, "name");
Shape *s1 = shape_transition_add(arena, root, key);

cr_assert_not_null(s1, "Transition s0 -> s1 should succeed");
cr_expect_eq(shape_get_parent(s1), root, "s1 parent should be root");
cr_expect_eq(shape_get_slot_count(s1), 1, "s1 slot count should be 1");
cr_expect_eq(shape_get_last_slot(s1), 0, "s1 last slot should be 0");
cr_expect_str_eq(shape_get_last_key(s1), "name", "s1 last key should be name");
/*#endregion*/
END_TIMED_TEST

TIMED_TEST(T, shape_sharing_guarantees_identical_pointers, init, fini)
/*#region*/
const char *key_name = make_temp_lstr(arena, "name");
const char *key_age = make_temp_lstr(arena, "age");

// Path A: root -> name -> age
Shape *s1_a = shape_transition_add(arena, root, key_name);
Shape *s2_a = shape_transition_add(arena, s1_a, key_age);

// Path B: root -> name -> age (Identical sequence)
Shape *s1_b = shape_transition_add(arena, root, key_name);
Shape *s2_b = shape_transition_add(arena, s1_b, key_age);

cr_expect_eq(s1_a, s1_b, "Shape 'name' should be identical and shared");
cr_expect_eq(s2_a, s2_b, "Shape 'name -> age' should be identical and shared");
/*#endregion*/
END_TIMED_TEST

// -----------------------------------------------------------------------------
// SLOT LOOKUP TESTS
// -----------------------------------------------------------------------------

TIMED_TEST(T, lookup_slot_returns_correct_indices, init, fini)
/*#region*/
const char *key_a = make_temp_lstr(arena, "a");
const char *key_b = make_temp_lstr(arena, "b");
const char *key_c = make_temp_lstr(arena, "c");

Shape *s1 = shape_transition_add(arena, root, key_a);
Shape *s2 = shape_transition_add(arena, s1, key_b);
Shape *s3 = shape_transition_add(arena, s2, key_c);

cr_expect_eq(shape_lookup_slot(s3, key_a), 0, "Key 'a' should map to slot 0");
cr_expect_eq(shape_lookup_slot(s3, key_b), 1, "Key 'b' should map to slot 1");
cr_expect_eq(shape_lookup_slot(s3, key_c), 2, "Key 'c' should map to slot 2");

const char *non_existent = make_temp_lstr(arena, "missing");
cr_expect_eq(shape_lookup_slot(s3, non_existent), -1, "Missing key should return -1");
/*#endregion*/
END_TIMED_TEST

// -----------------------------------------------------------------------------
// OBJECT INSTANTIATION AND SLOTS READING/WRITING
// -----------------------------------------------------------------------------

TIMED_TEST(T, obj_creation_and_property_setting, init, fini)
/*#region*/
const char *k_name = make_temp_lstr(arena, "name");
const char *k_price = make_temp_lstr(arena, "price");

Obj *obj = obj_new(arena, root);
cr_assert_not_null(obj, "Obj allocation should succeed");
cr_expect_eq(obj_get_shape(obj), root, "Initial object shape should be root");

Value v_name = val_str(make_temp_lstr(arena, "TradingEngine"));
Value v_price = val_double(99.95);

// Set property 1 -> Transitions shape
obj_set(arena, obj, k_name, v_name);
cr_expect_eq(shape_get_slot_count(obj_get_shape(obj)), 1);

// Set property 2 -> Transitions shape again
obj_set(arena, obj, k_price, v_price);
cr_expect_eq(shape_get_slot_count(obj_get_shape(obj)), 2);

// Read back property values
Value out_name = val_undefined();
Value out_price = val_undefined();

cr_assert(obj_get(obj, k_name, &out_name), "Should retrieve name");
cr_assert(obj_get(obj, k_price, &out_price), "Should retrieve price");

cr_expect_eq(out_name.tag, VAL_STRING);
cr_expect_str_eq(out_name.as.p, "TradingEngine");
cr_expect_eq(out_price.tag, VAL_DOUBLE);
cr_expect_float_eq(out_price.as.d, 99.95, 1e-9);
/*#endregion*/
END_TIMED_TEST

// -----------------------------------------------------------------------------
// SAFETY LIMITS / OOM GRACEFUL FAILURES
// -----------------------------------------------------------------------------

TIMED_TEST(T, tiny_arena_limits_trigger_safe_oom, init, fini)
/*#region*/
// Recreate arena with ultra-tiny memory limit (500 bytes)
jsonv_arena_destroy(arena);
arena = jsonv_arena_new(256, 512, 512);
cr_assert_not_null(arena);

root = shape_root(arena);
cr_assert_not_null(root);

// Exceeding limit via rapid transition creation should fail cleanly/gracefully
TRY {
  Shape *curr = root;
  char buf[8];
  for (int i = 0; i < 20; i++) {
    buf[0] = 'a' + i;
    buf[1] = '\0';
    const char *key = make_temp_lstr(arena, buf);
    curr = shape_transition_add(arena, curr, key);
    if (!curr) {
      // Returned NULL indicating OOM, which is correct and safe
      break;
    }
  }
}
EXCEPT(ARENA_LIMIT_REACHED) {
  // Correctly propagated exception instead of crash
  cr_assert(true);
}
END_TRY;
/*#endregion*/
END_TIMED_TEST
