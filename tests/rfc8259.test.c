#include "ctx.h"
#include "arena.h"
#include "shape.h"
#include "unescape.h"
#include "utils.h"
#include <criterion/criterion.h>
#include <string.h>

#define T RFC8259

static Jsonv_Arena *schema_arena = NULL;
static Jsonv_Arena *execution_arena = NULL;

static void init(void) {
  /*#region*/
  test_init();
  schema_arena = jsonv_arena_new(4096, 1024 * 1024, 12 * 1024);
  execution_arena = jsonv_arena_new(4096, 1024 * 1024, 12 * 1024);
  cr_assert_not_null(schema_arena);
  cr_assert_not_null(execution_arena);
  /*#endregion*/
}

static void fini(void) {
  /*#region*/
  if (schema_arena) {
    jsonv_arena_destroy(schema_arena);
    schema_arena = NULL;
  }
  if (execution_arena) {
    jsonv_arena_destroy(execution_arena);
    execution_arena = NULL;
  }
  extern void jsonv_shape_clear_global_arena(void);
  jsonv_shape_clear_global_arena();
  test_fini();
  /*#endregion*/
}

TIMED_TEST(T, unescape_module_standalone, init, fini)
/*#region*/
char buf[128];
size_t len;

// Simple escapes
const char *src = "hello \\\"world\\\" \\n \\t \\\\ \\/";
len = jsonv_unescape_string((const unsigned char *)src, strlen(src), buf);
cr_expect_eq(len, 21);
cr_expect_str_eq(buf, "hello \"world\" \n \t \\ /");

// Basic Unicode \uXXXX
len = jsonv_unescape_string((const unsigned char *)"\\u0061\\u0062\\u0063", 18, buf);
cr_expect_eq(len, 3);
cr_expect_str_eq(buf, "abc");

// Surrogate Pair \uD83D\uDE00 -> 😀 (4-byte UTF-8: 0xF0 0x9F 0x98 0x80)
len = jsonv_unescape_string((const unsigned char *)"\\uD83D\\uDE00", 12, buf);
cr_expect_eq(len, 4);
cr_expect_str_eq(buf, "😀");
/*#endregion*/
END_TIMED_TEST

TIMED_TEST(T, top_level_non_object_parsing, init, fini)
/*#region*/
Jsonv_Context *ctx = jsonv_ctx_new(execution_arena, NULL, NULL);
cr_assert_not_null(ctx);

Jsonv_Value val;

// String
cr_assert(jsonv_ctx_parse_data(ctx, (const unsigned char *)"\"hello rfc\""));
cr_assert(jsonv_ctx_get_value(ctx, &val));
cr_expect_eq(val.tag, JSONV_VAL_STRING);
cr_expect_str_eq((const char *)val.as.p, "hello rfc");
jsonv_ctx_reset(ctx);

// Number
cr_assert(jsonv_ctx_parse_data(ctx, (const unsigned char *)"42"));
cr_assert(jsonv_ctx_get_value(ctx, &val));
cr_expect_eq(val.tag, JSONV_VAL_INT);
cr_expect_eq(val.as.i, 42);
jsonv_ctx_reset(ctx);

// Boolean
cr_assert(jsonv_ctx_parse_data(ctx, (const unsigned char *)"true"));
cr_assert(jsonv_ctx_get_value(ctx, &val));
cr_expect_eq(val.tag, JSONV_VAL_BOOLEAN);
cr_expect_eq(val.as.boolean, true);
jsonv_ctx_reset(ctx);

// Null
cr_assert(jsonv_ctx_parse_data(ctx, (const unsigned char *)"null"));
cr_assert(jsonv_ctx_get_value(ctx, &val));
cr_expect_eq(val.tag, JSONV_VAL_NULL);
jsonv_ctx_reset(ctx);

// Array
cr_assert(jsonv_ctx_parse_data(ctx, (const unsigned char *)"[1, 2, 3]"));
cr_assert(jsonv_ctx_get_value(ctx, &val));
cr_expect_eq(val.tag, JSONV_VAL_ARRAY);
cr_expect_eq(jsonv_arr_length((Jsonv_Arr *)val.as.p), 3);
/*#endregion*/
END_TIMED_TEST

TIMED_TEST(T, escaped_keys_and_properties, init, fini)
/*#region*/
Jsonv_Context *ctx = jsonv_ctx_new(execution_arena, NULL, NULL);
cr_assert_not_null(ctx);

const char *json = "{\"k\\\\1\": \"v1\", \"\\u0061_key\": 100}";
cr_assert(jsonv_ctx_parse_data(ctx, (const unsigned char *)json));

Jsonv_Value val;
cr_assert(jsonv_ctx_get_value(ctx, &val));
cr_expect_eq(val.tag, JSONV_VAL_OBJ);

Jsonv_Obj *obj = (Jsonv_Obj *)val.as.p;
Jsonv_Value out_val;
cr_expect(jsonv_obj_get(obj, "k\\1", &out_val));
cr_expect_eq(out_val.tag, JSONV_VAL_STRING);
cr_expect_str_eq((const char *)out_val.as.p, "v1");

cr_expect(jsonv_obj_get(obj, "a_key", &out_val));
cr_expect_eq(out_val.tag, JSONV_VAL_INT);
cr_expect_eq(out_val.as.i, 100);
/*#endregion*/
END_TIMED_TEST

TIMED_TEST(T, arbitrary_precision_numbers_api, init, fini)
/*#region*/
Jsonv_Context *ctx = jsonv_ctx_new(execution_arena, NULL, NULL);
cr_assert_not_null(ctx);

// Large 64-bit int
const char *json_int = "9223372036854775807";
cr_assert(jsonv_ctx_parse_data(ctx, (const unsigned char *)json_int));
Jsonv_Value v_int;
cr_assert(jsonv_ctx_get_value(ctx, &v_int));

int64_t i64_out = 0;
double d_out = 0.0;
cr_assert(jsonv_val_get_int64(v_int, &i64_out));
cr_expect_eq(i64_out, 9223372036854775807LL);
cr_assert(jsonv_val_get_double(v_int, &d_out));
cr_expect_eq(d_out, (double)9223372036854775807LL);
jsonv_ctx_reset(ctx);

// Bignum (overflows int64_t)
const char *json_big = "999999999999999999999999999999";
cr_assert(jsonv_ctx_parse_data(ctx, (const unsigned char *)json_big));
Jsonv_Value v_big;
cr_assert(jsonv_ctx_get_value(ctx, &v_big));
cr_expect_eq(v_big.tag, JSONV_VAL_BIGNUM);

const char *raw_str = NULL;
size_t raw_len = 0;
cr_assert(jsonv_val_get_raw_number(v_big, &raw_str, &raw_len));
cr_expect_eq(raw_len, 30);
cr_expect_str_eq(raw_str, "999999999999999999999999999999");
/*#endregion*/
END_TIMED_TEST

TIMED_TEST(T, unlimited_key_length, init, fini)
/*#region*/
Jsonv_Context *ctx = jsonv_ctx_new(execution_arena, NULL, NULL);
cr_assert_not_null(ctx);

char long_key[701];
memset(long_key, 'a', 700);
long_key[700] = '\0';

char json_buf[1000];
snprintf(json_buf, sizeof(json_buf), "{\"%s\": 42}", long_key);

cr_assert(jsonv_ctx_parse_data(ctx, (const unsigned char *)json_buf));

Jsonv_Value root;
cr_assert(jsonv_ctx_get_value(ctx, &root));
cr_expect_eq(root.tag, JSONV_VAL_OBJ);

Jsonv_Obj *obj = (Jsonv_Obj *)root.as.p;
Jsonv_Value prop;
cr_expect(jsonv_obj_get(obj, long_key, &prop));
cr_expect_eq(prop.tag, JSONV_VAL_INT);
cr_expect_eq(prop.as.i, 42);
/*#endregion*/
END_TIMED_TEST

TIMED_TEST(T, schema_pattern_escaped_regex_validation, init, fini)
/*#region*/
// Test 1: Semver pattern with POSIX ERE digits [0-9]* and escaped dots \\.
const char *semver_schema_json = 
    "{"
    "  \"type\": \"object\","
    "  \"required\": [\"version\"],"
    "  \"properties\": {"
    "    \"version\": { \"type\": \"string\", \"pattern\": \"^(0|[1-9][0-9]*)\\\\.(0|[1-9][0-9]*)\\\\.(0|[1-9][0-9]*)$\" }"
    "  }"
    "}";

Jsonv_Error err = {0};
Jsonv_Schema *schema = jsonv_schema_compile(schema_arena, (const unsigned char *)semver_schema_json, NULL, &err);
cr_assert_not_null(schema, "Schema compilation failed: %s", err.description);

Jsonv_Context *ctx = jsonv_ctx_new(execution_arena, NULL, NULL);
cr_assert_not_null(ctx);

// Valid semver strings
cr_expect(jsonv_ctx_parse_data(ctx, (const unsigned char *)"{\"version\": \"1.0.0\"}"));
cr_expect(jsonv_ctx_validate(ctx, schema));
jsonv_ctx_reset(ctx);

cr_expect(jsonv_ctx_parse_data(ctx, (const unsigned char *)"{\"version\": \"10.20.300\"}"));
cr_expect(jsonv_ctx_validate(ctx, schema));
jsonv_ctx_reset(ctx);

cr_expect(jsonv_ctx_parse_data(ctx, (const unsigned char *)"{\"version\": \"0.1.2\"}"));
cr_expect(jsonv_ctx_validate(ctx, schema));
jsonv_ctx_reset(ctx);

// Invalid semver strings
cr_expect(jsonv_ctx_parse_data(ctx, (const unsigned char *)"{\"version\": \"1.0.0-alpha\"}"));
cr_expect(!jsonv_ctx_validate(ctx, schema));
jsonv_ctx_reset(ctx);

cr_expect(jsonv_ctx_parse_data(ctx, (const unsigned char *)"{\"version\": \"01.0.0\"}"));
cr_expect(!jsonv_ctx_validate(ctx, schema));
jsonv_ctx_reset(ctx);

// Test 2: Escaped backslash in pattern and target data string
const char *path_schema_json = "{\"type\": \"string\", \"pattern\": \"^foo\\\\\\\\bar$\"}";
Jsonv_Schema *path_schema = jsonv_schema_compile(schema_arena, (const unsigned char *)path_schema_json, NULL, &err);
cr_assert_not_null(path_schema, "Path schema compilation failed: %s", err.description);

cr_expect(jsonv_ctx_parse_data(ctx, (const unsigned char *)"\"foo\\\\bar\""));
cr_expect(jsonv_ctx_validate(ctx, path_schema));
jsonv_ctx_reset(ctx);

cr_expect(jsonv_ctx_parse_data(ctx, (const unsigned char *)"\"foobar\""));
cr_expect(!jsonv_ctx_validate(ctx, path_schema));
jsonv_ctx_reset(ctx);

// Test 3: Unicode escape in pattern (\u0061 -> 'a')
const char *unicode_schema_json = "{\"type\": \"string\", \"pattern\": \"^\\u0061+\\\\.$\"}";
Jsonv_Schema *unicode_schema = jsonv_schema_compile(schema_arena, (const unsigned char *)unicode_schema_json, NULL, &err);
cr_assert_not_null(unicode_schema, "Unicode schema compilation failed: %s", err.description);

cr_expect(jsonv_ctx_parse_data(ctx, (const unsigned char *)"\"aaa.\""));
cr_expect(jsonv_ctx_validate(ctx, unicode_schema));
jsonv_ctx_reset(ctx);

cr_expect(jsonv_ctx_parse_data(ctx, (const unsigned char *)"\"bbb.\""));
cr_expect(!jsonv_ctx_validate(ctx, unicode_schema));
jsonv_ctx_reset(ctx);

/*#endregion*/
END_TIMED_TEST
