#include "ctx.h"
#ifdef JSONV_YAML_SUPPORT
#include "utils.h"
#include <criterion/criterion.h>
#include <string.h>

#define T YAML

static Jsonv_Context *ctx;
static Jsonv_Arena *arena;

static void init(void) {
  /*#region*/
  test_init();
  Jsonv_Arena_Error err = 0;
  arena = jsonv_arena_new(1024, 65536, 4096);
  cr_assert(arena);
  Jsonv_Config config = {
      .default_block_size = 1024,
      .max_limit = 65536,
      .shrink_at = 4096,
      .max_depth = 10,
      .max_values = 100,
      .max_objects = 100,
      .max_array = 100,
      .max_string_bytes = 1000
  };
  ctx = jsonv_ctx_new(arena, &config, &err);
  cr_assert(ctx);
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

typedef struct {
  uint32_t length;
  char data[];
} StringHeader;

static const char *make_temp_lstr(Jsonv_Arena *arena, const char *s) {
  /*#region*/
  extern void *arena_alloc(Jsonv_Arena *arena, size_t size);
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

// Tracer Bullet Test
TIMED_TEST(T, parse_simple_mapping, init, fini)
/*#region*/
const unsigned char *input = (const unsigned char *)"key: value\n";
bool success = jsonv_ctx_parse_yaml_data(ctx, input);
cr_assert(success);

Jsonv_Value val;
bool has_val = jsonv_ctx_get_value(ctx, &val);
cr_assert(has_val);
cr_assert_eq(val.tag, JSONV_VAL_OBJ);

Jsonv_Value item;
const char *k_key = make_temp_lstr(arena, "key");
bool has_prop = jsonv_obj_get(val.as.p, k_key, &item);
cr_assert(has_prop);
cr_assert_eq(item.tag, JSONV_VAL_STRING);

// Retrieve length-prefixed string details
size_t len = jsonv_val_str_len(item);
cr_assert_eq(len, 5);
cr_assert_str_eq(item.as.p, "value");
/*#endregion*/
END_TIMED_TEST

TIMED_TEST(T, parse_nested_mapping, init, fini)
/*#region*/
const unsigned char *input = (const unsigned char *)
    "outer:\n"
    "  inner: inner_val\n";
bool success = jsonv_ctx_parse_yaml_data(ctx, input);
cr_assert(success);

Jsonv_Value val;
bool has_val = jsonv_ctx_get_value(ctx, &val);
cr_assert(has_val);
cr_assert_eq(val.tag, JSONV_VAL_OBJ);

// Get "outer"
Jsonv_Value outer;
const char *k_outer = make_temp_lstr(arena, "outer");
bool has_outer = jsonv_obj_get(val.as.p, k_outer, &outer);
cr_assert(has_outer);
cr_assert_eq(outer.tag, JSONV_VAL_OBJ);

// Get "inner"
Jsonv_Value inner;
const char *k_inner = make_temp_lstr(arena, "inner");
bool has_inner = jsonv_obj_get(outer.as.p, k_inner, &inner);
cr_assert(has_inner);
cr_assert_eq(inner.tag, JSONV_VAL_STRING);
cr_assert_str_eq(inner.as.p, "inner_val");
/*#endregion*/
END_TIMED_TEST

TIMED_TEST(T, parse_block_sequence, init, fini)
/*#region*/
const unsigned char *input = (const unsigned char *)
    "- item1\n"
    "- item2\n";
bool success = jsonv_ctx_parse_yaml_data(ctx, input);
cr_assert(success);

Jsonv_Value val;
bool has_val = jsonv_ctx_get_value(ctx, &val);
cr_assert(has_val);
cr_assert_eq(val.tag, JSONV_VAL_ARRAY);
cr_assert_eq(jsonv_arr_length(val.as.p), 2);

Jsonv_Value item0, item1;
cr_assert(jsonv_arr_get(val.as.p, 0, &item0));
cr_assert_eq(item0.tag, JSONV_VAL_STRING);
cr_assert_str_eq(item0.as.p, "item1");

cr_assert(jsonv_arr_get(val.as.p, 1, &item1));
cr_assert_eq(item1.tag, JSONV_VAL_STRING);
cr_assert_str_eq(item1.as.p, "item2");
/*#endregion*/
END_TIMED_TEST

TIMED_TEST(T, parse_mixed_structures, init, fini)
/*#region*/
const unsigned char *input = (const unsigned char *)
    "outer_list:\n"
    "  - key1: val1\n"
    "  - key2: val2\n";
bool success = jsonv_ctx_parse_yaml_data(ctx, input);
cr_assert(success);

Jsonv_Value val;
bool has_val = jsonv_ctx_get_value(ctx, &val);
cr_assert(has_val);
cr_assert_eq(val.tag, JSONV_VAL_OBJ);

Jsonv_Value list_val;
const char *k_outer = make_temp_lstr(arena, "outer_list");
cr_assert(jsonv_obj_get(val.as.p, k_outer, &list_val));
cr_assert_eq(list_val.tag, JSONV_VAL_ARRAY);
cr_assert_eq(jsonv_arr_length(list_val.as.p), 2);

Jsonv_Value map0;
cr_assert(jsonv_arr_get(list_val.as.p, 0, &map0));
cr_assert_eq(map0.tag, JSONV_VAL_OBJ);

Jsonv_Value key1_val;
const char *k_key1 = make_temp_lstr(arena, "key1");
cr_assert(jsonv_obj_get(map0.as.p, k_key1, &key1_val));
cr_assert_eq(key1_val.tag, JSONV_VAL_STRING);
cr_assert_str_eq(key1_val.as.p, "val1");
/*#endregion*/
END_TIMED_TEST

TIMED_TEST(T, parse_scalars_and_literals, init, fini)
/*#region*/
const unsigned char *input = (const unsigned char *)
    "int_val: 123\n"
    "float_val: 45.67\n"
    "bool_true: true\n"
    "bool_false: false\n"
    "null_val: null\n";
bool success = jsonv_ctx_parse_yaml_data(ctx, input);
cr_assert(success);

Jsonv_Value val;
bool has_val = jsonv_ctx_get_value(ctx, &val);
cr_assert(has_val);
cr_assert_eq(val.tag, JSONV_VAL_OBJ);

Jsonv_Value item;
cr_assert(jsonv_obj_get(val.as.p, make_temp_lstr(arena, "int_val"), &item));
cr_assert_eq(item.tag, JSONV_VAL_DOUBLE);
cr_assert_eq(item.as.d, 123.0);

cr_assert(jsonv_obj_get(val.as.p, make_temp_lstr(arena, "float_val"), &item));
cr_assert_eq(item.tag, JSONV_VAL_DOUBLE);
cr_assert_eq(item.as.d, 45.67);

cr_assert(jsonv_obj_get(val.as.p, make_temp_lstr(arena, "bool_true"), &item));
cr_assert_eq(item.tag, JSONV_VAL_BOOLEAN);
cr_assert_eq(item.as.boolean, true);

cr_assert(jsonv_obj_get(val.as.p, make_temp_lstr(arena, "bool_false"), &item));
cr_assert_eq(item.tag, JSONV_VAL_BOOLEAN);
cr_assert_eq(item.as.boolean, false);

cr_assert(jsonv_obj_get(val.as.p, make_temp_lstr(arena, "null_val"), &item));
cr_assert_eq(item.tag, JSONV_VAL_NULL);
/*#endregion*/
END_TIMED_TEST

TIMED_TEST(T, parse_flow_style, init, fini)
/*#region*/
const unsigned char *input = (const unsigned char *)
    "flow_map: {a: 1, b: 2}\n"
    "flow_arr: [3, 4]\n";
bool success = jsonv_ctx_parse_yaml_data(ctx, input);
cr_assert(success);

Jsonv_Value val;
bool has_val = jsonv_ctx_get_value(ctx, &val);
cr_assert(has_val);
cr_assert_eq(val.tag, JSONV_VAL_OBJ);

Jsonv_Value flow_map;
cr_assert(jsonv_obj_get(val.as.p, make_temp_lstr(arena, "flow_map"), &flow_map));
cr_assert_eq(flow_map.tag, JSONV_VAL_OBJ);

Jsonv_Value flow_map_a;
cr_assert(jsonv_obj_get(flow_map.as.p, make_temp_lstr(arena, "a"), &flow_map_a));
cr_assert_eq(flow_map_a.tag, JSONV_VAL_DOUBLE);
cr_assert_eq(flow_map_a.as.d, 1.0);

Jsonv_Value flow_arr;
cr_assert(jsonv_obj_get(val.as.p, make_temp_lstr(arena, "flow_arr"), &flow_arr));
cr_assert_eq(flow_arr.tag, JSONV_VAL_ARRAY);

Jsonv_Value flow_arr_0;
cr_assert(jsonv_arr_get(flow_arr.as.p, 0, &flow_arr_0));
cr_assert_eq(flow_arr_0.tag, JSONV_VAL_DOUBLE);
cr_assert_eq(flow_arr_0.as.d, 3.0);
/*#endregion*/
END_TIMED_TEST

TIMED_TEST(T, parse_block_scalars, init, fini)
/*#region*/
  const char *yaml =
      "literal: |\n"
      "  line 1\n"
      "  line 2\n"
      "folded: >\n"
      "  folded 1\n"
      "  folded 2\n"
      "normal: string";

  cr_assert(jsonv_ctx_parse_yaml_data(ctx, (const unsigned char *)yaml));

  Jsonv_Value val;
  bool has_val = jsonv_ctx_get_value(ctx, &val);
  cr_assert(has_val);
  cr_assert_eq(val.tag, JSONV_VAL_OBJ);

  Jsonv_Value literal;
  cr_assert(jsonv_obj_get(val.as.p, make_temp_lstr(arena, "literal"), &literal));
  cr_assert_eq(literal.tag, JSONV_VAL_STRING);
  cr_expect_str_eq(literal.as.p, "line 1\n  line 2\n");

  Jsonv_Value folded;
  cr_assert(jsonv_obj_get(val.as.p, make_temp_lstr(arena, "folded"), &folded));
  cr_assert_eq(folded.tag, JSONV_VAL_STRING);
  cr_expect_str_eq(folded.as.p, "folded 1\n  folded 2\n");

  Jsonv_Value normal;
  cr_assert(jsonv_obj_get(val.as.p, make_temp_lstr(arena, "normal"), &normal));
  cr_assert_eq(normal.tag, JSONV_VAL_STRING);
  cr_expect_str_eq(normal.as.p, "string");
/*#endregion*/
END_TIMED_TEST
#endif // JSONV_YAML_SUPPORT

