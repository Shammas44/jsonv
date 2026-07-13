#include "shape.internal.h"
#include "arena.internal.h"
#include "except.h"
#include "utils.h"
#include <criterion/criterion.h>
#include <string.h>
#include <math.h>

#define T Serialize

static Jsonv_Arena *arena = NULL;
static Shape *root = NULL;

static void init(void) {
  /*#region*/
  test_init();
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

TIMED_TEST(T, serialize_null, init, fini)
/*#region*/
Jsonv_Value v = jsonv_val_null();
char buf[32];
int res = jsonv_serialize(v, buf, sizeof(buf));
cr_expect_eq(res, 4);
cr_expect_str_eq(buf, "null");
/*#endregion*/
END_TIMED_TEST

TIMED_TEST(T, serialize_boolean, init, fini)
/*#region*/
Jsonv_Value vt = jsonv_val_bool(true);
Jsonv_Value vf = jsonv_val_bool(false);
char buf[32];
int res = jsonv_serialize(vt, buf, sizeof(buf));
cr_expect_eq(res, 4);
cr_expect_str_eq(buf, "true");

res = jsonv_serialize(vf, buf, sizeof(buf));
cr_expect_eq(res, 5);
cr_expect_str_eq(buf, "false");
/*#endregion*/
END_TIMED_TEST

TIMED_TEST(T, serialize_int, init, fini)
/*#region*/
Jsonv_Value v = jsonv_val_int(123456);
char buf[32];
int res = jsonv_serialize(v, buf, sizeof(buf));
cr_expect_eq(res, 6);
cr_expect_str_eq(buf, "123456");

v = jsonv_val_int(-987);
res = jsonv_serialize(v, buf, sizeof(buf));
cr_expect_eq(res, 4);
cr_expect_str_eq(buf, "-987");
/*#endregion*/
END_TIMED_TEST

TIMED_TEST(T, serialize_double, init, fini)
/*#region*/
Jsonv_Value v = jsonv_val_double(12.34);
char buf[32];
int res = jsonv_serialize(v, buf, sizeof(buf));
cr_expect_gt(res, 0);
cr_expect_str_eq(buf, "12.34");

// NaN double
v = jsonv_val_double(0.0 / 0.0);
res = jsonv_serialize(v, buf, sizeof(buf));
cr_expect_eq(res, -1);
/*#endregion*/
END_TIMED_TEST

TIMED_TEST(T, serialize_string, init, fini)
/*#region*/
const char *str = make_temp_lstr(arena, "hello \"world\" \n");
Jsonv_Value v = jsonv_val_str(str);
char buf[64];
int res = jsonv_serialize(v, buf, sizeof(buf));
cr_assert_eq(res, 20, "Expected 20, got %d. buf: %s", res, buf);
cr_expect_str_eq(buf, "\"hello \\\"world\\\" \\n\"");
/*#endregion*/
END_TIMED_TEST

TIMED_TEST(T, serialize_empty_obj, init, fini)
/*#region*/
Jsonv_Obj *o = jsonv_obj_new(arena, root);
Jsonv_Value v = jsonv_val_obj(o);
char buf[32];
int res = jsonv_serialize(v, buf, sizeof(buf));
cr_expect_eq(res, 2);
cr_expect_str_eq(buf, "{}");
/*#endregion*/
END_TIMED_TEST

TIMED_TEST(T, serialize_simple_obj, init, fini)
/*#region*/
Jsonv_Obj *o = jsonv_obj_new(arena, root);
const char *k_name = make_temp_lstr(arena, "name");
const char *v_name = make_temp_lstr(arena, "bob");
jsonv_obj_set(arena, o, k_name, jsonv_val_str(v_name));
const char *k_age = make_temp_lstr(arena, "age");
jsonv_obj_set(arena, o, k_age, jsonv_val_int(25));

Jsonv_Value v = jsonv_val_obj(o);
char buf[128];
int res = jsonv_serialize(v, buf, sizeof(buf));
cr_assert_eq(res, 23, "Expected 23, got %d. buf: %s", res, buf);
cr_expect_str_eq(buf, "{\"name\":\"bob\",\"age\":25}");
/*#endregion*/
END_TIMED_TEST

TIMED_TEST(T, serialize_empty_arr, init, fini)
/*#region*/
Jsonv_Arr *a = jsonv_arr_new(arena);
Jsonv_Value v = jsonv_val_arr(a);
char buf[32];
int res = jsonv_serialize(v, buf, sizeof(buf));
cr_expect_eq(res, 2);
cr_expect_str_eq(buf, "[]");
/*#endregion*/
END_TIMED_TEST

TIMED_TEST(T, serialize_simple_arr, init, fini)
/*#region*/
Jsonv_Arr *a = jsonv_arr_new(arena);
jsonv_arr_set(arena, a, 0, jsonv_val_int(10));
jsonv_arr_set(arena, a, 1, jsonv_val_bool(false));
jsonv_arr_set(arena, a, 2, jsonv_val_null());

Jsonv_Value v = jsonv_val_arr(a);
char buf[64];
int res = jsonv_serialize(v, buf, sizeof(buf));
cr_assert_eq(res, 15, "Expected 15, got %d. buf: %s", res, buf);
cr_expect_str_eq(buf, "[10,false,null]");
/*#endregion*/
END_TIMED_TEST

TIMED_TEST(T, serialize_size_estimation, init, fini)
/*#region*/
Jsonv_Arr *a = jsonv_arr_new(arena);
jsonv_arr_set(arena, a, 0, jsonv_val_int(10));
jsonv_arr_set(arena, a, 1, jsonv_val_bool(false));

Jsonv_Value v = jsonv_val_arr(a);
// Estimate size
int needed = jsonv_serialize(v, NULL, 0);
cr_expect_eq(needed, 10); // "[10,false]" is 10 chars

// Truncated buffer
char short_buf[6];
int res = jsonv_serialize(v, short_buf, sizeof(short_buf));
cr_expect_eq(res, 10);
cr_expect_str_eq(short_buf, "[10,f"); // 5 chars + '\0'
/*#endregion*/
END_TIMED_TEST

TIMED_TEST(T, serialize_cycles, init, fini)
/*#region*/
// Object cycle
Jsonv_Obj *o = jsonv_obj_new(arena, root);
const char *k_self = make_temp_lstr(arena, "self");
jsonv_obj_set(arena, o, k_self, jsonv_val_obj(o));

Jsonv_Value v = jsonv_val_obj(o);
char buf[128];
int res = jsonv_serialize(v, buf, sizeof(buf));
cr_expect_eq(res, -1);

// Array cycle
Jsonv_Arr *a = jsonv_arr_new(arena);
jsonv_arr_set(arena, a, 0, jsonv_val_arr(a));

v = jsonv_val_arr(a);
res = jsonv_serialize(v, buf, sizeof(buf));
cr_expect_eq(res, -1);
/*#endregion*/
END_TIMED_TEST
