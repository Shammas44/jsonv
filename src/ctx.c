#include "ctx.h"
#include "shape.internal.h"
#include "parser.h"
#include "set.h"
#include "keytree.h"
#include "mem.h"
#include "prescan.h"
#include "schema.h"
#include "validate.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern const Except MALFORMED_JSON;
extern const Except MAXIMUM_NESTED_DEPTH_REACHED;
extern const Except MAXIMUM_OBJECT_REACHED;
extern const Except MAXIMUM_ARRAY_REACHED;
extern const Except MAXIMUM_VALUES_REACHED;
extern const Except ARENA_LIMIT_REACHED;
extern const Except MAXIMUM_TOKEN_BYTES_REACHED;

extern bool validate_ast(
    Jsonv_Context *ctx,
    ASTNode *pool,
    const Jsonv_Schema *schema,
    int rule_idx,
    int node_idx,
    const char *path,
    E *out_err
);

struct Jsonv_Schema {
  SchemaRule *rules;
  int rule_count;
};

struct Jsonv_Context {
  Jsonv_Arena *execution_arena;
  E last_error;
  bool has_error;
  Jsonv_Config config;

  Stack data;
  set_t data_set;
  KeyTreePool data_keytree;
};

/* ------------------- Schema Compilation ------------------- */

Jsonv_Schema* jsonv_schema_compile(
    Jsonv_Arena *schema_arena,
    const unsigned char *schema_json,
    const Jsonv_Config *config,
    E *out_error
) {
  /*#region*/
  assert(schema_arena);
  assert(schema_json);
  
  if (out_error) {
    memset(out_error, 0, sizeof(E));
  }
  
  size_t json_length = strlen((char *)schema_json);
  if (json_length == 0) {
    if (out_error) {
      out_error->type = Jsonv_Malformed_json;
      snprintf(out_error->description, sizeof(out_error->description), "Empty JSON");
    }
    return NULL;
  }
  
  Lexer lexer;
  lexer_init(&lexer, schema_json, json_length);

  Stack ast = {0};
  set_t schema_set;
  KeyTreePool schema_keytree;

  TRY {
    // 1. PRE-SCAN
    JsonEstimate est = {0};
    prescan((const char *)schema_json, json_length, &est);
    
    // 2. CHECK LIMITS IF CONFIG
    if (config) {
      if (est.max_depth > config->max_depth) RAISE(MAXIMUM_NESTED_DEPTH_REACHED);
      if (est.value_count > config->max_values) RAISE(MAXIMUM_VALUES_REACHED);
      if (est.object_count > config->max_objects) RAISE(MAXIMUM_OBJECT_REACHED);
      if (est.array_count > config->max_array) RAISE(MAXIMUM_ARRAY_REACHED);
      if (est.string_bytes > config->max_string_bytes) RAISE(MAXIMUM_TOKEN_BYTES_REACHED);
    }
    
    // 3. COMPILE AST (Consolidated Deep Seam)
    parse_to_ast(schema_arena, &lexer, json_length, est.value_count, &ast, &schema_keytree, &schema_set);

    // 5. COMPILE SCHEMA
    int rule_count = 0;
    SchemaRule *rules = compile_schema(schema_arena, (ASTNode *)ast.data, stack_size(&ast), 0, &rule_count);
    if (!rules) {
      return NULL;
    }

    // 6. ALLOCATE SCHEMA OBJECT
    Jsonv_Schema *schema = (Jsonv_Schema *)jsonv_arena_alloc(schema_arena, sizeof(Jsonv_Schema));
    if (!schema) return NULL;
    schema->rules = rules;
    schema->rule_count = rule_count;
    return schema;
  }
  EXCEPT(MALFORMED_JSON) {
    if (out_error) {
      out_error->type = Jsonv_Malformed_json;
      snprintf(out_error->description, sizeof(out_error->description), "Malformed JSON");
    }
  }
  EXCEPT(Mem_Failed) {
    if (out_error) {
      out_error->type = Jsonv_Mem_Failed;
      snprintf(out_error->description, sizeof(out_error->description), "Memory Allocation Failed");
    }
  }
  EXCEPT(ARENA_LIMIT_REACHED) {
    if (out_error) {
      out_error->type = Jsonv_Arena_Limit_Reached;
      snprintf(out_error->description, sizeof(out_error->description), "Arena Limit Reached");
    }
  }
  EXCEPT(MAXIMUM_NESTED_DEPTH_REACHED) {
    if (out_error) {
      out_error->type = Jsonv_Maximum_Nested_Depth_Reached;
      snprintf(out_error->description, sizeof(out_error->description), "Maximum Nested Depth Reached");
    }
  }
  END_TRY;

  return NULL;
  /*#endregion*/
}

/* ------------------- Context Operations ------------------- */

Jsonv_Context* jsonv_ctx_create(
    Jsonv_Arena *execution_arena,
    const Jsonv_Config *config
) {
  /*#region*/
  if (!execution_arena) return NULL;
  
  Jsonv_Context *ctx = (Jsonv_Context *)jsonv_arena_alloc(execution_arena, sizeof(Jsonv_Context));
  if (!ctx) return NULL;
  
  ctx->execution_arena = execution_arena;
  memset(&ctx->last_error, 0, sizeof(E));
  ctx->has_error = false;
  
  // Set configurations
  if (config) {
    memcpy(&ctx->config, config, sizeof(Jsonv_Config));
  } else {
    memset(&ctx->config, 0, sizeof(Jsonv_Config));
  }
  
  return ctx;
  /*#endregion*/
}

bool jsonv_ctx_parse_data(
    Jsonv_Context *ctx,
    const unsigned char *data_json
) {
  /*#region*/
  assert(ctx);
  assert(data_json);
  
  memset(&ctx->last_error, 0, sizeof(E));
  ctx->has_error = false;
  
  size_t json_length = strlen((char *)data_json);
  if (json_length == 0) {
    ctx->last_error.type = Jsonv_Malformed_json;
    snprintf(ctx->last_error.description, sizeof(ctx->last_error.description), "Empty JSON");
    ctx->has_error = true;
    return false;
  }
  
  Lexer lexer;
  lexer_init(&lexer, data_json, json_length);

  TRY {
    // 1. PRE-SCAN
    JsonEstimate est = {0};
    prescan((const char *)data_json, json_length, &est);
    
    // 2. CHECK LIMITS IF CONFIG
    if (ctx->config.max_depth > 0) {
      if (est.max_depth > ctx->config.max_depth) RAISE(MAXIMUM_NESTED_DEPTH_REACHED);
      if (est.value_count > ctx->config.max_values) RAISE(MAXIMUM_VALUES_REACHED);
      if (est.object_count > ctx->config.max_objects) RAISE(MAXIMUM_OBJECT_REACHED);
      if (est.array_count > ctx->config.max_array) RAISE(MAXIMUM_ARRAY_REACHED);
      if (est.string_bytes > ctx->config.max_string_bytes) RAISE(MAXIMUM_TOKEN_BYTES_REACHED);
    }
    
    // 3. COMPILE AST (Consolidated Deep Seam)
    parse_to_ast(ctx->execution_arena, &lexer, json_length, est.value_count, &ctx->data, &ctx->data_keytree, &ctx->data_set);

    return true;
  }
  EXCEPT(MALFORMED_JSON) {
    ctx->last_error.type = Jsonv_Malformed_json;
    snprintf(ctx->last_error.description, sizeof(ctx->last_error.description), "Malformed JSON");
    ctx->has_error = true;
  }
  EXCEPT(Mem_Failed) {
    ctx->last_error.type = Jsonv_Mem_Failed;
    snprintf(ctx->last_error.description, sizeof(ctx->last_error.description), "Memory Allocation Failed");
    ctx->has_error = true;
  }
  EXCEPT(ARENA_LIMIT_REACHED) {
    ctx->last_error.type = Jsonv_Arena_Limit_Reached;
    snprintf(ctx->last_error.description, sizeof(ctx->last_error.description), "Arena Limit Reached");
    ctx->has_error = true;
    RAISE(ARENA_LIMIT_REACHED);
  }
  EXCEPT(MAXIMUM_NESTED_DEPTH_REACHED) {
    ctx->last_error.type = Jsonv_Maximum_Nested_Depth_Reached;
    snprintf(ctx->last_error.description, sizeof(ctx->last_error.description), "Maximum Nested Depth Reached");
    ctx->has_error = true;
  }
  EXCEPT(MAXIMUM_VALUES_REACHED) {
    ctx->last_error.type = Jsonv_Maximum_Values_Reached;
    snprintf(ctx->last_error.description, sizeof(ctx->last_error.description), "Maximum Values Reached");
    ctx->has_error = true;
  }
  EXCEPT(MAXIMUM_OBJECT_REACHED) {
    ctx->last_error.type = Jsonv_Maximum_Object_Reached;
    snprintf(ctx->last_error.description, sizeof(ctx->last_error.description), "Maximum Object Reached");
    ctx->has_error = true;
  }
  EXCEPT(MAXIMUM_ARRAY_REACHED) {
    ctx->last_error.type = Jsonv_Maximum_Array_Reached;
    snprintf(ctx->last_error.description, sizeof(ctx->last_error.description), "Maximum Array Reached");
    ctx->has_error = true;
  }
  EXCEPT(MAXIMUM_TOKEN_BYTES_REACHED) {
    ctx->last_error.type = Jsonv_Maximum_Token_Bytes_Reached;
    snprintf(ctx->last_error.description, sizeof(ctx->last_error.description), "Maximum Token Bytes Reached");
    ctx->has_error = true;
  }
  END_TRY;

  return false;
  /*#endregion*/
}

bool jsonv_ctx_validate(
    Jsonv_Context *ctx,
    const Jsonv_Schema *schema
) {
  /*#region*/
  assert(ctx);
  assert(schema);
  
  memset(&ctx->last_error, 0, sizeof(E));
  ctx->has_error = false;

  if (ctx->data.top < 0) {
    ctx->last_error.type = Jsonv_Malformed_json;
    snprintf(ctx->last_error.description, sizeof(ctx->last_error.description), "Empty AST");
    ctx->has_error = true;
    return false;
  }

  bool ok = validate_ast(ctx, (ASTNode *)ctx->data.data, schema, 0, 0, "", &ctx->last_error);
  if (!ok) {
    ctx->has_error = true;
  }
  return ok;
  /*#endregion*/
}

bool jsonv_ctx_get_value(
    Jsonv_Context *ctx,
    Value *out_value
) {
  /*#region*/
  assert(ctx);
  assert(out_value);
  
  if (ctx->data.top < 0) {
    *out_value = val_undefined();
    return false;
  }
  
  TRY {
    ASTNode *pool = (ASTNode *)ctx->data.data;
    Shape *exe_root = jsonv_shape_root();
    if (!exe_root) return false;
    *out_value = ast_to_value(pool, &ctx->data_keytree, 0, exe_root, ctx->execution_arena);
    return true;
  }
  EXCEPT(Mem_Failed) {
    ctx->last_error.type = Jsonv_Mem_Failed;
    snprintf(ctx->last_error.description, sizeof(ctx->last_error.description), "Memory Allocation Failed");
    ctx->has_error = true;
  }
  EXCEPT(ARENA_LIMIT_REACHED) {
    ctx->last_error.type = Jsonv_Arena_Limit_Reached;
    snprintf(ctx->last_error.description, sizeof(ctx->last_error.description), "Arena Limit Reached");
    ctx->has_error = true;
    RAISE(ARENA_LIMIT_REACHED);
  }
  END_TRY;
  
  return false;
  /*#endregion*/
}

const E* jsonv_ctx_get_error(const Jsonv_Context *ctx) {
  /*#region*/
  if (!ctx) return NULL;
  return &ctx->last_error;
  /*#endregion*/
}

Jsonv_Arena* jsonv_ctx_arena(const Jsonv_Context *ctx) {
  /*#region*/
  if (!ctx) return NULL;
  return ctx->execution_arena;
  /*#endregion*/
}

void jsonv_ctx_reset(Jsonv_Context *ctx) {
  /*#region*/
  if (!ctx) return;
  memset(&ctx->last_error, 0, sizeof(E));
  ctx->has_error = false;
  // Reset the transient execution arena but preserve the Jsonv_Context allocation
  jsonv_arena_reset_to(ctx->execution_arena, sizeof(Jsonv_Context));
  // Clear the thread-local recycled free lists to prevent dangling pointer references
  jsonv_shape_clear_free_lists();
  /*#endregion*/
}
