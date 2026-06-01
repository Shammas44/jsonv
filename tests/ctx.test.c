#include "ctx.h"
#include "arena.h"
#include "shape.h"
#include "except.h"
#include "utils.h"
#include <criterion/criterion.h>
#include <string.h>

#define T Ctx

typedef Jsonv_Value Value;

static Jsonv_Arena *schema_arena = NULL;
static Jsonv_Arena *execution_arena = NULL;

extern const Except ARENA_LIMIT_REACHED;
extern const Except MALFORMED_JSON;

static void init(void) {
  /*#region*/
  test_init();
  // Allocate arenas: 4KB blocks, 1MB max limit, 12KB trim threshold
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

// -----------------------------------------------------------------------------
// SCHEMA COMPILATION TESTS
// -----------------------------------------------------------------------------

TIMED_TEST(T, compile_valid_schema, init, fini)
/*#region*/
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

const unsigned char *schema_json = (const unsigned char *)"{\"type\": \"object\", \"required\": [\"name\"]}";
E err = {0};

Jsonv_Schema *schema = jsonv_schema_compile(schema_arena, schema_json, &config, &err);
cr_assert_not_null(schema, "Compiling valid schema should succeed");
cr_expect_eq(err.type, 0, "No compilation error type should be set");
/*#endregion*/
END_TIMED_TEST

TIMED_TEST(T, compile_invalid_schema, init, fini)
/*#region*/
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

const unsigned char *schema_json = (const unsigned char *)"{\"type\": \"object\", \"required\": "; // Malformed JSON
E err = {0};

Jsonv_Schema *schema = jsonv_schema_compile(schema_arena, schema_json, &config, &err);
cr_assert_null(schema, "Compiling malformed schema should fail");
cr_expect_str_eq(err.description, "Malformed JSON", "Error description should indicate malformed JSON");
/*#endregion*/
END_TIMED_TEST

// -----------------------------------------------------------------------------
// CONTEXT CREATION AND RESET
// -----------------------------------------------------------------------------

TIMED_TEST(T, context_creation_and_reset, init, fini)
/*#region*/
Jsonv_Config config = {
    .default_block_size = 1024,
    .max_limit = 65536,
    .shrink_at = 4096
};

Jsonv_Context *ctx = jsonv_ctx_create(execution_arena, &config);
cr_assert_not_null(ctx, "Context creation should succeed");

const E *err = jsonv_ctx_get_error(ctx);
cr_assert_not_null(err, "Initial error structure should be non-NULL");
cr_expect_eq(err->type, 0, "Initial error type should be 0");

// Reset context
jsonv_ctx_reset(ctx);
err = jsonv_ctx_get_error(ctx);
cr_expect_eq(err->type, 0, "Error state should remain clear after reset");
/*#endregion*/
END_TIMED_TEST

// -----------------------------------------------------------------------------
// DATA PARSING & VALIDATION
// -----------------------------------------------------------------------------

TIMED_TEST(T, parse_valid_data, init, fini)
/*#region*/
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

Jsonv_Context *ctx = jsonv_ctx_create(execution_arena, &config);
cr_assert_not_null(ctx);

const unsigned char *data_json = (const unsigned char *)"{\"name\": \"jsonv\", \"price\": 12.50}";
Value parsed_val;

bool success = jsonv_ctx_parse_data(ctx, data_json, &parsed_val);
cr_assert(success, "Parsing valid JSON payload should succeed");
cr_expect_eq(parsed_val.tag, JSONV_VAL_OBJ, "Parsed value should be an Object");
/*#endregion*/
END_TIMED_TEST

TIMED_TEST(T, parse_invalid_data, init, fini)
/*#region*/
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

Jsonv_Context *ctx = jsonv_ctx_create(execution_arena, &config);
cr_assert_not_null(ctx);

const unsigned char *data_json = (const unsigned char *)"{\"name\": \"jsonv\", \"price\": }"; // Malformed JSON
Value parsed_val;

bool success = jsonv_ctx_parse_data(ctx, data_json, &parsed_val);
cr_assert(!success, "Parsing invalid JSON payload should fail");

const E *err = jsonv_ctx_get_error(ctx);
cr_expect_str_eq(err->description, "Malformed JSON", "Error description should indicate malformed JSON");
/*#endregion*/
END_TIMED_TEST

TIMED_TEST(T, validate_valid_payload, init, fini)
/*#region*/
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

// 1. Compile schema
const unsigned char *schema_json = (const unsigned char *)"{\"type\": \"object\", \"required\": [\"name\"]}";
E compile_err = {0};
Jsonv_Schema *schema = jsonv_schema_compile(schema_arena, schema_json, &config, &compile_err);
cr_assert_not_null(schema);

// 2. Create Context & Parse Data
Jsonv_Context *ctx = jsonv_ctx_create(execution_arena, &config);
cr_assert_not_null(ctx);

const unsigned char *data_json = (const unsigned char *)"{\"name\": \"TradingEngine\"}";
Value data_val;
bool parse_ok = jsonv_ctx_parse_data(ctx, data_json, &data_val);
cr_assert(parse_ok);

// 3. Validate
bool valid = jsonv_ctx_validate(ctx, schema, data_val);
cr_expect(valid, "Validation of compliant payload should succeed");
/*#endregion*/
END_TIMED_TEST

TIMED_TEST(T, validate_invalid_payload, init, fini)
/*#region*/
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

// 1. Compile schema (expects username to be string)
const unsigned char *schema_json = (const unsigned char *)"{\"type\": \"object\", \"properties\": {\"username\": {\"type\": \"string\"}}}";
E compile_err = {0};
Jsonv_Schema *schema = jsonv_schema_compile(schema_arena, schema_json, &config, &compile_err);
cr_assert_not_null(schema);

// 2. Create Context & Parse Data (contains double instead of string)
Jsonv_Context *ctx = jsonv_ctx_create(execution_arena, &config);
cr_assert_not_null(ctx);

const unsigned char *data_json = (const unsigned char *)"{\"username\": 12345}";
Value data_val;
bool parse_ok = jsonv_ctx_parse_data(ctx, data_json, &data_val);
cr_assert(parse_ok);

// 3. Validate -> Expect Failure
bool valid = jsonv_ctx_validate(ctx, schema, data_val);
cr_assert(!valid, "Validation of non-compliant payload should fail");

const E *err = jsonv_ctx_get_error(ctx);
cr_assert_not_null(err);
cr_expect_str_eq(err->path, "username", "Error path should point to the failing property");
/*#endregion*/
END_TIMED_TEST

// -----------------------------------------------------------------------------
// LOW MEMORY & OOM GRACEFUL FAILURES
// -----------------------------------------------------------------------------

TIMED_TEST(T, low_memory_limits_fail_gracefully, init, fini)
/*#region*/
// Recreate ultra-tiny execution arena (256 bytes) to force OOM
jsonv_arena_destroy(execution_arena);
execution_arena = jsonv_arena_new(128, 256, 256);
cr_assert_not_null(execution_arena);

Jsonv_Config config = {
    .default_block_size = 128,
    .max_limit = 256,
    .shrink_at = 256,
    .max_depth = 10,
    .max_values = 100,
    .max_objects = 100,
    .max_array = 100,
    .max_string_bytes = 1000
};

TRY {
  Jsonv_Context *ctx = jsonv_ctx_create(execution_arena, &config);
  if (ctx) {
    // Large parsing request should quickly exceed 256 bytes and trigger OOM exception cleanly
    const unsigned char *large_json = (const unsigned char *)
        "{\"a\": 1, \"b\": 2, \"c\": 3, \"d\": 4, \"e\": 5, \"f\": 6, \"g\": 7}";
    Value val;
    bool success = jsonv_ctx_parse_data(ctx, large_json, &val);
    cr_assert(!success, "Parsing under restrictive OOM limit should return false");
  }
}
EXCEPT(ARENA_LIMIT_REACHED) {
  // Correctly propagated exception instead of hard crash
  cr_assert(true);
}
END_TRY;
/*#endregion*/
END_TIMED_TEST
