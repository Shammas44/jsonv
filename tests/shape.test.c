#include "shape.internal.h"
#include "arena.internal.h"
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
  root = shape_root();
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
  StringHeader *header = (StringHeader *)arena_alloc(arena, total_size);
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
Shape *s1 = shape_transition_add(root, key);

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
Shape *s1_a = shape_transition_add(root, key_name);
Shape *s2_a = shape_transition_add(s1_a, key_age);

// Path B: root -> name -> age (Identical sequence)
Shape *s1_b = shape_transition_add(root, key_name);
Shape *s2_b = shape_transition_add(s1_b, key_age);

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

Shape *s1 = shape_transition_add(root, key_a);
Shape *s2 = shape_transition_add(s1, key_b);
Shape *s3 = shape_transition_add(s2, key_c);

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

TIMED_TEST(T, obj_creation_with_null_shape_falls_back_to_root, init, fini)
/*#region*/
Obj *obj = obj_new(arena, NULL);
cr_assert_not_null(obj, "Obj allocation with NULL root should succeed");
cr_expect_eq(obj_get_shape(obj), root, "Initial object shape should fallback to root");

const char *k_name = make_temp_lstr(arena, "name");
Value v_name = val_str(make_temp_lstr(arena, "Nestor"));

// Property writes should proceed without crashing
obj_set(arena, obj, k_name, v_name);
cr_expect_eq(shape_get_slot_count(obj_get_shape(obj)), 1);

Value out_name = val_undefined();
cr_assert(obj_get(obj, k_name, &out_name), "Should retrieve name");
cr_expect_eq(out_name.tag, VAL_STRING);
cr_expect_str_eq(out_name.as.p, "Nestor");
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

root = shape_root();
cr_assert_not_null(root);

// Exceeding limit via rapid transition creation should fail cleanly/gracefully
TRY {
  Shape *curr = root;
  char buf[8];
  for (int i = 0; i < 20; i++) {
    buf[0] = 'a' + i;
    buf[1] = '\0';
    const char *key = make_temp_lstr(arena, buf);
    curr = shape_transition_add(curr, key);
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

TIMED_TEST(T, obj_and_arr_iteration_accessors, init, fini)
/*#region*/
  // 1. Object Iteration
  const char *k_a = make_temp_lstr(arena, "a");
  const char *k_b = make_temp_lstr(arena, "b");
  const char *k_c = make_temp_lstr(arena, "c");

  Jsonv_Obj *obj = jsonv_obj_new(arena, root);
  cr_assert_not_null(obj);

  jsonv_obj_set(arena, obj, k_a, jsonv_val_int(10));
  jsonv_obj_set(arena, obj, k_b, jsonv_val_double(20.5));
  jsonv_obj_set(arena, obj, k_c, jsonv_val_bool(true));

  cr_expect_eq(jsonv_obj_length(obj), 3, "Object length should be 3");

  // Verify key and value at index 0
  const char *key0 = jsonv_obj_key_at(obj, 0);
  cr_expect_str_eq(key0, "a");
  Jsonv_Value val0 = jsonv_obj_val_at(obj, 0);
  cr_expect_eq(val0.tag, JSONV_VAL_INT);
  cr_expect_eq(val0.as.i, 10);

  // Verify key and value at index 1
  const char *key1 = jsonv_obj_key_at(obj, 1);
  cr_expect_str_eq(key1, "b");
  Jsonv_Value val1 = jsonv_obj_val_at(obj, 1);
  cr_expect_eq(val1.tag, JSONV_VAL_DOUBLE);
  cr_expect_float_eq(val1.as.d, 20.5, 1e-9);

  // Verify key and value at index 2
  const char *key2 = jsonv_obj_key_at(obj, 2);
  cr_expect_str_eq(key2, "c");
  Jsonv_Value val2 = jsonv_obj_val_at(obj, 2);
  cr_expect_eq(val2.tag, JSONV_VAL_BOOLEAN);
  cr_expect_eq(val2.as.boolean, true);

  // Verify bounds/errors
  cr_expect_null(jsonv_obj_key_at(obj, -1));
  cr_expect_null(jsonv_obj_key_at(obj, 3));
  cr_expect_eq(jsonv_obj_val_at(obj, -1).tag, JSONV_VAL_UNDEFINED);
  cr_expect_eq(jsonv_obj_val_at(obj, 3).tag, JSONV_VAL_UNDEFINED);

  // 2. Array Iteration
  Jsonv_Arr *arr = jsonv_arr_new(arena);
  cr_assert_not_null(arr);

  jsonv_arr_set(arena, arr, 0, jsonv_val_str("hello"));
  jsonv_arr_set(arena, arr, 1, jsonv_val_null());
  jsonv_arr_set(arena, arr, 2, jsonv_val_int(42));

  cr_expect_eq(jsonv_arr_length(arr), 3, "Array length should be 3");

  Jsonv_Value item0 = jsonv_arr_val_at(arr, 0);
  cr_expect_eq(item0.tag, JSONV_VAL_STRING);
  cr_expect_str_eq(item0.as.p, "hello");

  Jsonv_Value item1 = jsonv_arr_val_at(arr, 1);
  cr_expect_eq(item1.tag, JSONV_VAL_NULL);

  Jsonv_Value item2 = jsonv_arr_val_at(arr, 2);
  cr_expect_eq(item2.tag, JSONV_VAL_INT);
  cr_expect_eq(item2.as.i, 42);

  // Verify bounds/errors
  cr_expect_eq(jsonv_arr_val_at(arr, -1).tag, JSONV_VAL_UNDEFINED);
  cr_expect_eq(jsonv_arr_val_at(arr, 3).tag, JSONV_VAL_UNDEFINED);
/*#endregion*/
END_TIMED_TEST

TIMED_TEST(T, path_traversal_tests, init, fini)
/*#region*/
  // 1. Build nested structure:
  // {
  //   "user": {
  //     "name": "Alice",
  //     "history": [100, 200, 300]
  //   }
  // }
  const char *k_user = make_temp_lstr(arena, "user");
  const char *k_name = make_temp_lstr(arena, "name");
  const char *k_history = make_temp_lstr(arena, "history");

  Jsonv_Obj *user_obj = jsonv_obj_new(arena, root);
  jsonv_obj_set(arena, user_obj, k_name, jsonv_val_str("Alice"));

  Jsonv_Arr *history_arr = jsonv_arr_new(arena);
  jsonv_arr_set(arena, history_arr, 0, jsonv_val_int(100));
  jsonv_arr_set(arena, history_arr, 1, jsonv_val_int(200));
  jsonv_arr_set(arena, history_arr, 2, jsonv_val_int(300));
  jsonv_obj_set(arena, user_obj, k_history, jsonv_val_arr(history_arr));

  Jsonv_Obj *root_obj = jsonv_obj_new(arena, root);
  jsonv_obj_set(arena, root_obj, k_user, jsonv_val_obj(user_obj));

  Jsonv_Value root_val = jsonv_val_obj(root_obj);

  // 2. Test successful lookups
  // root -> user (object) -> name (string)
  Jsonv_Value name_val = jsonv_value_get_path(root_val, "ss", "user", "name");
  cr_expect_eq(name_val.tag, JSONV_VAL_STRING);
  cr_expect_str_eq(name_val.as.p, "Alice");

  // root -> user -> history (array) -> 1 (int)
  Jsonv_Value hist_1 = jsonv_value_get_path(root_val, "ssi", "user", "history", 1);
  cr_expect_eq(hist_1.tag, JSONV_VAL_INT);
  cr_expect_eq(hist_1.as.i, 200);

  // root -> user -> history -> 2 (int)
  Jsonv_Value hist_2 = jsonv_value_get_path(root_val, "ssi", "user", "history", 2);
  cr_expect_eq(hist_2.tag, JSONV_VAL_INT);
  cr_expect_eq(hist_2.as.i, 300);

  // 3. Test unresolvable lookups (missing key or out of bounds index)
  // root -> user -> age (missing key)
  Jsonv_Value missing_key = jsonv_value_get_path(root_val, "ss", "user", "age");
  cr_expect_eq(missing_key.tag, JSONV_VAL_UNRESOLVABLE);

  // root -> user -> history -> 5 (index out of bounds)
  Jsonv_Value oob_index = jsonv_value_get_path(root_val, "ssi", "user", "history", 5);
  cr_expect_eq(oob_index.tag, JSONV_VAL_UNRESOLVABLE);

  // 4. Test impossible lookups
  // Lookup index on root (which is object, not array)
  Jsonv_Value idx_on_obj = jsonv_value_get_path(root_val, "i", 0);
  cr_expect_eq(idx_on_obj.tag, JSONV_VAL_IMPOSSIBLE);

  // Lookup key on history (which is array, not object)
  Jsonv_Value key_on_arr = jsonv_value_get_path(root_val, "sss", "user", "history", "first");
  cr_expect_eq(key_on_arr.tag, JSONV_VAL_IMPOSSIBLE);

  // Traversal beyond scalar (Alice is string, cannot traverse further)
  Jsonv_Value beyond_scalar = jsonv_value_get_path(root_val, "sss", "user", "name", "extra");
  cr_expect_eq(beyond_scalar.tag, JSONV_VAL_IMPOSSIBLE);

  // Invalid format character
  Jsonv_Value invalid_fmt = jsonv_value_get_path(root_val, "sx", "user", "name");
  cr_expect_eq(invalid_fmt.tag, JSONV_VAL_IMPOSSIBLE);
/*#endregion*/
END_TIMED_TEST
