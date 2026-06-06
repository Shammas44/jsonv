#include "ctx.h"
#include "arena.h"
#include "shape.h"
#include "except.h"
#include "utils.h"
#include <criterion/criterion.h>
#include <string.h>
#include <stdbool.h>

#define T Validation

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

static bool run_validation(const char *schema_str, const char *data_str, E *out_err) {
  /*#region*/
  char wrapped_schema[2048];
  snprintf(wrapped_schema, sizeof(wrapped_schema), "{\"properties\": {\"value\": %s}}", schema_str);

  char wrapped_data[2048];
  snprintf(wrapped_data, sizeof(wrapped_data), "{\"value\": %s}", data_str);

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

  E compile_err = {0};
  Jsonv_Schema *schema = jsonv_schema_compile(schema_arena, (const unsigned char *)wrapped_schema, &config, &compile_err);
  if (!schema) {
    if (out_err) *out_err = compile_err;
    return false;
  }

  Jsonv_Context *ctx = jsonv_ctx_create(execution_arena, &config);
  if (!ctx) return false;

  bool parse_ok = jsonv_ctx_parse_data(ctx, (const unsigned char *)wrapped_data);
  if (!parse_ok) {
    if (out_err) *out_err = *jsonv_ctx_get_error(ctx);
    return false;
  }

  bool valid = jsonv_ctx_validate(ctx, schema);
  if (!valid && out_err) {
    *out_err = *jsonv_ctx_get_error(ctx);
    if (out_err->path) {
      if (strncmp(out_err->path, "value.", 6) == 0) {
        out_err->path += 6;
      } else if (strncmp(out_err->path, "value[", 6) == 0) {
        out_err->path += 5;
      } else if (strcmp(out_err->path, "value") == 0) {
        out_err->path = "";
      }
    }
  }
  return valid;
  /*#endregion*/
}

TIMED_TEST(T, type_validation, init, fini)
/*#region*/
  E err = {0};

  // 1. String type
  cr_expect(run_validation("{\"type\": \"string\"}", "\"hello\"", &err));
  cr_expect(!run_validation("{\"type\": \"string\"}", "123", &err));
  cr_expect_eq(err.type, Jsonv_Type_error);

  // 2. Number type
  cr_expect(run_validation("{\"type\": \"number\"}", "123.45", &err));
  cr_expect(!run_validation("{\"type\": \"number\"}", "true", &err));

  // 3. Integer type
  cr_expect(run_validation("{\"type\": \"integer\"}", "123", &err));
  cr_expect(!run_validation("{\"type\": \"integer\"}", "123.45", &err));

  // 4. Boolean type
  cr_expect(run_validation("{\"type\": \"boolean\"}", "true", &err));
  cr_expect(!run_validation("{\"type\": \"boolean\"}", "\"true\"", &err));

  // 5. Null type
  cr_expect(run_validation("{\"type\": \"null\"}", "null", &err));
  cr_expect(!run_validation("{\"type\": \"null\"}", "0", &err));

  // 6. Object type
  cr_expect(run_validation("{\"type\": \"object\"}", "{}", &err));
  cr_expect(!run_validation("{\"type\": \"object\"}", "[]", &err));

  // 7. Array type
  cr_expect(run_validation("{\"type\": \"array\"}", "[]", &err));
  cr_expect(!run_validation("{\"type\": \"array\"}", "{}", &err));

  // 8. Multi-type list
  cr_expect(run_validation("{\"type\": [\"string\", \"null\"]}", "\"hello\"", &err));
  cr_expect(run_validation("{\"type\": [\"string\", \"null\"]}", "null", &err));
  cr_expect(!run_validation("{\"type\": [\"string\", \"null\"]}", "123", &err));
/*#endregion*/
END_TIMED_TEST

TIMED_TEST(T, numeric_boundaries, init, fini)
/*#region*/
  E err = {0};

  const char *schema = "{\"type\": \"number\", \"minimum\": 10.5, \"maximum\": 20.5}";

  cr_expect(run_validation(schema, "15.0", &err));
  cr_expect(run_validation(schema, "10.5", &err));
  cr_expect(run_validation(schema, "20.5", &err));

  cr_expect(!run_validation(schema, "10.4", &err));
  cr_expect_eq(err.type, Jsonv_Minimum_error);

  cr_expect(!run_validation(schema, "20.6", &err));
  cr_expect_eq(err.type, Jsonv_Maximum_error);
/*#endregion*/
END_TIMED_TEST

TIMED_TEST(T, multiple_of_validation, init, fini)
/*#region*/
  E err = {0};

  const char *schema = "{\"type\": \"number\", \"multipleOf\": 2.5}";

  cr_expect(run_validation(schema, "5.0", &err));
  cr_expect(run_validation(schema, "7.5", &err));
  cr_expect(run_validation(schema, "0.0", &err));
  cr_expect(run_validation(schema, "-2.5", &err));

  cr_expect(!run_validation(schema, "3.0", &err));
  cr_expect_eq(err.type, Jsonv_MultipleOf_error);
/*#endregion*/
END_TIMED_TEST

TIMED_TEST(T, exclusive_numeric_boundaries, init, fini)
/*#region*/
  E err = {0};

  const char *schema = "{\"type\": \"number\", \"exclusiveMinimum\": 10.5, \"exclusiveMaximum\": 20.5}";

  cr_expect(run_validation(schema, "15.0", &err));
  cr_expect(run_validation(schema, "10.6", &err));
  cr_expect(run_validation(schema, "20.4", &err));

  cr_expect(!run_validation(schema, "10.5", &err));
  cr_expect_eq(err.type, Jsonv_ExclusiveMinimum_error);

  cr_expect(!run_validation(schema, "20.5", &err));
  cr_expect_eq(err.type, Jsonv_ExclusiveMaximum_error);
/*#endregion*/
END_TIMED_TEST

TIMED_TEST(T, string_lengths, init, fini)
/*#region*/
  E err = {0};

  const char *schema = "{\"type\": \"string\", \"minLength\": 3, \"maxLength\": 5}";

  cr_expect(run_validation(schema, "\"abc\"", &err));
  cr_expect(run_validation(schema, "\"abcde\"", &err));

  cr_expect(!run_validation(schema, "\"ab\"", &err));
  cr_expect_eq(err.type, Jsonv_MinLength_error);

  cr_expect(!run_validation(schema, "\"abcdef\"", &err));
  cr_expect_eq(err.type, Jsonv_MaxLength_error);
/*#endregion*/
END_TIMED_TEST

TIMED_TEST(T, pattern_validation, init, fini)
/*#region*/
  E err = {0};

  const char *schema = "{\"type\": \"string\", \"pattern\": \"^a[0-9]+b$\"}";

  cr_expect(run_validation(schema, "\"a123b\"", &err));
  cr_expect(run_validation(schema, "\"a0b\"", &err));

  cr_expect(!run_validation(schema, "\"ab\"", &err));
  cr_expect_eq(err.type, Jsonv_Pattern_error);

  cr_expect(!run_validation(schema, "\"a123bc\"", &err));
  cr_expect_eq(err.type, Jsonv_Pattern_error);
/*#endregion*/
END_TIMED_TEST

TIMED_TEST(T, array_items_constraints, init, fini)
/*#region*/
  E err = {0};

  const char *schema = "{\"type\": \"array\", \"minItems\": 2, \"maxItems\": 3, \"items\": {\"type\": \"integer\"}}";

  cr_expect(run_validation(schema, "[1, 2]", &err));
  cr_expect(run_validation(schema, "[1, 2, 3]", &err));

  cr_expect(!run_validation(schema, "[1]", &err));
  cr_expect_eq(err.type, Jsonv_MinItems_error);

  cr_expect(!run_validation(schema, "[1, 2, 3, 4]", &err));
  cr_expect_eq(err.type, Jsonv_MinItems_error); // Note: API maps to minItems/maxItems

  cr_expect(!run_validation(schema, "[1, \"not-int\"]", &err));
  cr_expect_eq(err.type, Jsonv_Type_error);
/*#endregion*/
END_TIMED_TEST

TIMED_TEST(T, object_constraints, init, fini)
/*#region*/
  E err = {0};

  const char *schema = "{\n"
                       "  \"type\": \"object\",\n"
                       "  \"required\": [\"a\", \"b\"],\n"
                       "  \"properties\": {\n"
                       "    \"a\": {\"type\": \"string\"},\n"
                       "    \"b\": {\"type\": \"integer\"}\n"
                       "  },\n"
                       "  \"additionalProperties\": false\n"
                       "}";

  cr_expect(run_validation(schema, "{\"a\": \"hello\", \"b\": 10}", &err));

  cr_expect(!run_validation(schema, "{\"a\": \"hello\"}", &err));
  cr_expect_eq(err.type, Jsonv_Required_error);

  cr_expect(!run_validation(schema, "{\"a\": 10, \"b\": 10}", &err));
  cr_expect_eq(err.type, Jsonv_Type_error);

  cr_expect(!run_validation(schema, "{\"a\": \"hello\", \"b\": 10, \"c\": true}", &err));
  cr_expect_eq(err.type, Jsonv_AdditionalProperties_error);
/*#endregion*/
END_TIMED_TEST

TIMED_TEST(T, property_count_constraints, init, fini)
/*#region*/
  E err = {0};

  const char *schema = "{\"type\": \"object\", \"minProperties\": 2, \"maxProperties\": 3}";

  cr_expect(run_validation(schema, "{\"a\": 1, \"b\": 2}", &err));
  cr_expect(run_validation(schema, "{\"a\": 1, \"b\": 2, \"c\": 3}", &err));

  cr_expect(!run_validation(schema, "{\"a\": 1}", &err));
  cr_expect_eq(err.type, Jsonv_MinProperties_error);

  cr_expect(!run_validation(schema, "{\"a\": 1, \"b\": 2, \"c\": 3, \"d\": 4}", &err));
  cr_expect_eq(err.type, Jsonv_MaxProperties_error);
/*#endregion*/
END_TIMED_TEST

TIMED_TEST(T, nested_schemas, init, fini)
/*#region*/
  E err = {0};

  const char *schema = "{\n"
                       "  \"type\": \"object\",\n"
                       "  \"properties\": {\n"
                       "    \"user\": {\n"
                       "      \"type\": \"object\",\n"
                       "      \"properties\": {\n"
                       "        \"id\": {\"type\": \"integer\"}\n"
                       "      },\n"
                       "      \"required\": [\"id\"]\n"
                       "    },\n"
                       "    \"tags\": {\n"
                       "      \"type\": \"array\",\n"
                       "      \"items\": {\"type\": \"string\"}\n"
                       "    }\n"
                       "  }\n"
                       "}";

  cr_expect(run_validation(schema, "{\"user\": {\"id\": 123}, \"tags\": [\"a\", \"b\"]}", &err));

  cr_expect(!run_validation(schema, "{\"user\": {}, \"tags\": [\"a\", \"b\"]}", &err));
  cr_expect_eq(err.type, Jsonv_Required_error);

  cr_expect(!run_validation(schema, "{\"user\": {\"id\": 123}, \"tags\": [1, 2]}", &err));
  cr_expect_eq(err.type, Jsonv_Type_error);
/*#endregion*/
END_TIMED_TEST

TIMED_TEST(T, unique_items_validation, init, fini)
/*#region*/
  E err = {0};

  const char *schema = "{\"type\": \"array\", \"uniqueItems\": true}";

  // Unique arrays
  cr_expect(run_validation(schema, "[]", &err));
  cr_expect(run_validation(schema, "[1, 2, 3]", &err));
  cr_expect(run_validation(schema, "[\"a\", \"b\", \"c\"]", &err));
  cr_expect(run_validation(schema, "[true, false, null]", &err));
  cr_expect(run_validation(schema, "[{\"a\": 1}, {\"a\": 2}]", &err));
  cr_expect(run_validation(schema, "[[1, 2], [1, 3]]", &err));

  // Non-unique arrays
  cr_expect(!run_validation(schema, "[1, 2, 2]", &err));
  cr_expect_eq(err.type, Jsonv_UniqueItems_error);

  cr_expect(!run_validation(schema, "[\"a\", \"b\", \"a\"]", &err));
  cr_expect_eq(err.type, Jsonv_UniqueItems_error);

  cr_expect(!run_validation(schema, "[true, true]", &err));
  cr_expect_eq(err.type, Jsonv_UniqueItems_error);

  cr_expect(!run_validation(schema, "[{\"a\": 1}, {\"a\": 1}]", &err));
  cr_expect_eq(err.type, Jsonv_UniqueItems_error);

  cr_expect(!run_validation(schema, "[[1, 2], [1, 2]]", &err));
  cr_expect_eq(err.type, Jsonv_UniqueItems_error);
/*#endregion*/
END_TIMED_TEST

TIMED_TEST(T, contains_validation, init, fini)
/*#region*/
  E err = {0};

  const char *schema = "{\"type\": \"array\", \"contains\": {\"type\": \"integer\", \"minimum\": 5}}";

  // Arrays containing at least one integer >= 5
  cr_expect(run_validation(schema, "[1, 2, 5]", &err));
  cr_expect(run_validation(schema, "[10, \"hello\", true]", &err));
  cr_expect(run_validation(schema, "[5]", &err));

  // Arrays not containing any integer >= 5
  cr_expect(!run_validation(schema, "[]", &err));
  cr_expect_eq(err.type, Jsonv_Contains_error);

  cr_expect(!run_validation(schema, "[1, 2, 3, 4]", &err));
  cr_expect_eq(err.type, Jsonv_Contains_error);

  cr_expect(!run_validation(schema, "[\"hello\", true, null]", &err));
  cr_expect_eq(err.type, Jsonv_Contains_error);
/*#endregion*/
END_TIMED_TEST

TIMED_TEST(T, not_validation, init, fini)
/*#region*/
  E err = {0};

  const char *schema = "{\"not\": {\"type\": \"integer\"}}";

  // Values that are NOT integers
  cr_expect(run_validation(schema, "\"hello\"", &err));
  cr_expect(run_validation(schema, "true", &err));
  cr_expect(run_validation(schema, "123.45", &err));
  cr_expect(run_validation(schema, "null", &err));

  // Values that ARE integers (should fail)
  cr_expect(!run_validation(schema, "123", &err));
  cr_expect_eq(err.type, Jsonv_Not_error);
/*#endregion*/
END_TIMED_TEST

TIMED_TEST(T, all_of_validation, init, fini)
/*#region*/
  E err = {0};

  const char *schema = "{\n"
                       "  \"allOf\": [\n"
                       "    {\"type\": \"string\"},\n"
                       "    {\"minLength\": 5}\n"
                       "  ]\n"
                       "}";

  // Strings of length >= 5
  cr_expect(run_validation(schema, "\"hello\"", &err));
  cr_expect(run_validation(schema, "\"abcdef\"", &err));

  // Non-strings (fails first subschema)
  cr_expect(!run_validation(schema, "123", &err));
  cr_expect_eq(err.type, Jsonv_AllOf_error);

  // Strings of length < 5 (fails second subschema)
  cr_expect(!run_validation(schema, "\"abc\"", &err));
  cr_expect_eq(err.type, Jsonv_AllOf_error);
/*#endregion*/
END_TIMED_TEST




