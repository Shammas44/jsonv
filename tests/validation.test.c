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

TIMED_TEST(T, any_of_validation, init, fini)
/*#region*/
  E err = {0};

  const char *schema = "{\n"
                       "  \"anyOf\": [\n"
                       "    {\"type\": \"string\"},\n"
                       "    {\"type\": \"integer\", \"minimum\": 10}\n"
                       "  ]\n"
                       "}";

  // Strings (validates against the first subschema)
  cr_expect(run_validation(schema, "\"hello\"", &err));
  cr_expect(run_validation(schema, "\"abc\"", &err));

  // Integers >= 10 (validates against the second subschema)
  cr_expect(run_validation(schema, "10", &err));
  cr_expect(run_validation(schema, "15", &err));

  // Integers < 10 (fails both subschemas)
  cr_expect(!run_validation(schema, "5", &err));
  cr_expect_eq(err.type, Jsonv_AnyOf_error);

  // Booleans (fails both subschemas)
  cr_expect(!run_validation(schema, "true", &err));
  cr_expect_eq(err.type, Jsonv_AnyOf_error);
/*#endregion*/
END_TIMED_TEST

TIMED_TEST(T, one_of_validation, init, fini)
/*#region*/
  E err = {0};

  const char *schema = "{\n"
                       "  \"oneOf\": [\n"
                       "    {\"type\": \"integer\", \"multipleOf\": 3},\n"
                       "    {\"type\": \"integer\", \"multipleOf\": 5}\n"
                       "  ]\n"
                       "}";

  // Multiples of 3 only (validates against the first subschema)
  cr_expect(run_validation(schema, "9", &err));
  cr_expect(run_validation(schema, "12", &err));

  // Multiples of 5 only (validates against the second subschema)
  cr_expect(run_validation(schema, "10", &err));
  cr_expect(run_validation(schema, "25", &err));

  // Multiples of both 3 and 5 (fails oneOf because it validates against both)
  cr_expect(!run_validation(schema, "15", &err));
  cr_expect_eq(err.type, Jsonv_OneOf_error);

  // Multiples of neither (fails oneOf because it validates against none)
  cr_expect(!run_validation(schema, "7", &err));
  cr_expect_eq(err.type, Jsonv_OneOf_error);
/*#endregion*/
END_TIMED_TEST

TIMED_TEST(T, if_then_else_validation, init, fini)
/*#region*/
  E err = {0};

  // If the value is an integer, it must be a multiple of 10.
  // Otherwise (if it's not an integer), it must be a string of maxLength 5.
  const char *schema = "{\n"
                       "  \"if\": {\"type\": \"integer\"},\n"
                       "  \"then\": {\"multipleOf\": 10},\n"
                       "  \"else\": {\"type\": \"string\", \"maxLength\": 5}\n"
                       "}";

  // Case 1: integer that is multiple of 10 -> passes
  cr_expect(run_validation(schema, "20", &err));
  cr_expect(run_validation(schema, "100", &err));

  // Case 2: integer that is not multiple of 10 -> fails then
  cr_expect(!run_validation(schema, "15", &err));
  cr_expect_eq(err.type, Jsonv_MultipleOf_error);

  // Case 3: not an integer, but string of maxLength 5 -> passes
  cr_expect(run_validation(schema, "\"hello\"", &err));
  cr_expect(run_validation(schema, "\"abc\"", &err));

  // Case 4: not an integer, but string of length > 5 -> fails else
  cr_expect(!run_validation(schema, "\"hello_world\"", &err));
  cr_expect_eq(err.type, Jsonv_MaxLength_error);

  // Case 5: not an integer and not a string -> fails else
  cr_expect(!run_validation(schema, "true", &err));
  cr_expect_eq(err.type, Jsonv_Type_error);
/*#endregion*/
END_TIMED_TEST

TIMED_TEST(T, property_names_validation, init, fini)
/*#region*/
  E err = {0};

  // Property names must be strings of length between 3 and 5 characters.
  const char *schema = "{\n"
                       "  \"propertyNames\": {\n"
                       "    \"minLength\": 3,\n"
                       "    \"maxLength\": 5\n"
                       "  }\n"
                       "}";

  // Case 1: all property names have lengths between 3 and 5 -> passes
  cr_expect(run_validation(schema, "{\"abc\": 1, \"hello\": 2}", &err));

  // Case 2: a property name has length < 3 -> fails minLength
  cr_expect(!run_validation(schema, "{\"ab\": 1, \"hello\": 2}", &err));
  cr_expect_eq(err.type, Jsonv_MinLength_error);

  // Case 3: a property name has length > 5 -> fails maxLength
  cr_expect(!run_validation(schema, "{\"abc\": 1, \"longer\": 2}", &err));
  cr_expect_eq(err.type, Jsonv_MaxLength_error);
/*#endregion*/
END_TIMED_TEST

TIMED_TEST(T, pattern_properties_validation, init, fini)
/*#region*/
  E err = {0};

  // Property keys starting with "f" must be integers.
  // Property keys starting with "s" must be strings.
  // Additional properties are not allowed.
  const char *schema = "{\n"
                       "  \"patternProperties\": {\n"
                       "    \"^f\": {\"type\": \"integer\"},\n"
                       "    \"^s\": {\"type\": \"string\"}\n"
                       "  },\n"
                       "  \"additionalProperties\": false\n"
                       "}";

  // Case 1: valid keys and values -> passes
  cr_expect(run_validation(schema, "{\"foo\": 42, \"str\": \"hello\"}", &err));

  // Case 2: invalid value for key starting with "f" -> fails type constraint
  cr_expect(!run_validation(schema, "{\"foo\": \"not_an_int\", \"str\": \"hello\"}", &err));
  cr_expect_eq(err.type, Jsonv_Type_error);

  // Case 3: invalid value for key starting with "s" -> fails type constraint
  cr_expect(!run_validation(schema, "{\"foo\": 42, \"str\": true}", &err));
  cr_expect_eq(err.type, Jsonv_Type_error);

  // Case 4: key that starts with neither "f" nor "s" -> fails additionalProperties
  cr_expect(!run_validation(schema, "{\"foo\": 42, \"str\": \"hello\", \"bar\": 1}", &err));
  cr_expect_eq(err.type, Jsonv_AdditionalProperties_error);
/*#endregion*/
END_TIMED_TEST

TIMED_TEST(T, format_validation, init, fini)
/*#region*/
  E err = {0};

  // 1. IPv4 Format
  const char *schema_ipv4 = "{\"format\": \"ipv4\"}";
  cr_expect(run_validation(schema_ipv4, "\"192.168.1.1\"", &err));
  cr_expect(run_validation(schema_ipv4, "\"0.0.0.0\"", &err));
  cr_expect(!run_validation(schema_ipv4, "\"256.0.0.1\"", &err));
  cr_expect_eq(err.type, Jsonv_Format_error);
  cr_expect(!run_validation(schema_ipv4, "\"192.168.01.1\"", &err));
  cr_expect_eq(err.type, Jsonv_Format_error);
  cr_expect(!run_validation(schema_ipv4, "\"not-an-ip\"", &err));
  cr_expect_eq(err.type, Jsonv_Format_error);

  // 2. Email Format
  const char *schema_email = "{\"format\": \"email\"}";
  cr_expect(run_validation(schema_email, "\"user@example.com\"", &err));
  cr_expect(!run_validation(schema_email, "\"userexample.com\"", &err));
  cr_expect_eq(err.type, Jsonv_Format_error);
  cr_expect(!run_validation(schema_email, "\"user@com\"", &err));
  cr_expect_eq(err.type, Jsonv_Format_error);

  // 3. UUID Format
  const char *schema_uuid = "{\"format\": \"uuid\"}";
  cr_expect(run_validation(schema_uuid, "\"123e4567-e89b-12d3-a456-426614174000\"", &err));
  cr_expect(!run_validation(schema_uuid, "\"123e4567-e89b-12d3-a456-42661417400\"", &err)); // too short
  cr_expect_eq(err.type, Jsonv_Format_error);
  cr_expect(!run_validation(schema_uuid, "\"123e4567-e89b-12d3-a456-42661417400g\"", &err)); // non-hex
  cr_expect_eq(err.type, Jsonv_Format_error);

  // 4. Date-Time Format
  const char *schema_dt = "{\"format\": \"date-time\"}";
  cr_expect(run_validation(schema_dt, "\"1985-04-12T23:20:50.52Z\"", &err));
  cr_expect(run_validation(schema_dt, "\"1996-12-19T16:39:57-08:00\"", &err));
  cr_expect(run_validation(schema_dt, "\"2026-06-06T15:19:57+02:00\"", &err));
  cr_expect(!run_validation(schema_dt, "\"1985-04-12T23:20:50\"", &err)); // missing offset
  cr_expect_eq(err.type, Jsonv_Format_error);
  cr_expect(!run_validation(schema_dt, "\"2026-02-29T12:00:00Z\"", &err)); // invalid day for non-leap year
  cr_expect_eq(err.type, Jsonv_Format_error);
/*#endregion*/
END_TIMED_TEST

