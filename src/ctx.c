#include "ctx.h"
#include "arr.h"
#include "parser.h"
#include "set.h"
#include "keytree.h"
#include "mem.h"
#include "prescan.h"
#include "schema.h"
#include "path.h"
#include "global.h"
#include "shape.h"
#include "print.h"
#include "obj.h"
#include "validate.h"
#include <assert.h>
#include <math.h>
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

extern bool validate_value(
    Jsonv_Context *ctx,
    const Jsonv_Schema *schema,
    int rule_idx,
    Value val,
    const char *path,
    E *out_err
);

struct Jsonv_Schema {
  SchemaRule *rules;
  int rule_count;
};

struct Jsonv_Context {
  Arena *execution_arena;
  E last_error;
  bool has_error;
  Jsonv_Config config;

  Stack data;
  set_t data_set;
  KeyTreePool data_keytree;
};

/* ------------------- Schema Compilation ------------------- */

Jsonv_Schema* jsonv_schema_compile(
    Arena *schema_arena,
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
  Stack scopes = {0};
  Stack control = {0};
  set_t schema_set;
  KeyTreePool schema_keytree;

  TRY {
    // 1. PRE-SCAN
    JsonEstimate est = {0};
    jsonv_prescan((const char *)schema_json, json_length, &est);
    
    // 2. CHECK LIMITS IF CONFIG
    if (config) {
      if (est.max_depth > config->max_depth) RAISE(MAXIMUM_NESTED_DEPTH_REACHED);
      if (est.value_count > config->max_values) RAISE(MAXIMUM_VALUES_REACHED);
      if (est.object_count > config->max_objects) RAISE(MAXIMUM_OBJECT_REACHED);
      if (est.array_count > config->max_array) RAISE(MAXIMUM_ARRAY_REACHED);
      if (est.string_bytes > config->max_string_bytes) RAISE(MAXIMUM_TOKEN_BYTES_REACHED);
    }
    
    // 3. ALLOCATE SPACE
    size_t ast_storage_size = json_length * sizeof(ASTNode);
    void *ast_storage = arena_alloc(schema_arena, ast_storage_size);
    stack_init(&ast, sizeof(ASTNode), ast_storage, ast_storage_size);

    size_t scopes_storage_size = json_length * sizeof(int);
    void *scopes_storage = arena_alloc(schema_arena, scopes_storage_size);
    stack_init(&scopes, sizeof(int), scopes_storage, scopes_storage_size);

    size_t control_storage_size = json_length * sizeof(int);
    void *control_storage = arena_alloc(schema_arena, control_storage_size);
    stack_init(&control, sizeof(int), control_storage, control_storage_size);

    size_t keys_capacity = set_next_power_of_two(round(1.2 * est.value_count));
    size_t set_storage_size = sizeof(entry_t) * keys_capacity;
    entry_t *set_data = (entry_t *)arena_alloc(schema_arena, set_storage_size);
    set_init(&schema_set, set_data, keys_capacity);

    size_t key_storage_size = sizeof(KeyNode) * keys_capacity;
    KeyNode *keytree_data = (KeyNode *)arena_alloc(schema_arena, key_storage_size);
    key_tree_init(&schema_keytree, keytree_data, keys_capacity);

    // 4. COMPILE AST
    jsonv_ast(&lexer, &ast, &scopes, &control, &schema_set, &schema_keytree);

    // 5. COMPILE SCHEMA
    int rule_count = 0;
    SchemaRule *rules = compile_schema(schema_arena, (ASTNode *)ast.data, stack_size(&ast), 0, &rule_count);
    if (!rules) {
      return NULL;
    }

    // 6. ALLOCATE SCHEMA OBJECT
    Jsonv_Schema *schema = (Jsonv_Schema *)arena_alloc(schema_arena, sizeof(Jsonv_Schema));
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
    Arena *execution_arena,
    const Jsonv_Config *config
) {
  /*#region*/
  if (!execution_arena) return NULL;
  
  Jsonv_Context *ctx = (Jsonv_Context *)arena_alloc(execution_arena, sizeof(Jsonv_Context));
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
    const unsigned char *data_json,
    Value *out_value
) {
  /*#region*/
  assert(ctx);
  assert(data_json);
  
  memset(&ctx->last_error, 0, sizeof(E));
  ctx->has_error = false;
  
  if (out_value) {
    *out_value = val_undefined();
  }
  
  size_t json_length = strlen((char *)data_json);
  if (json_length == 0) {
    ctx->last_error.type = Jsonv_Malformed_json;
    snprintf(ctx->last_error.description, sizeof(ctx->last_error.description), "Empty JSON");
    ctx->has_error = true;
    return false;
  }
  
  Lexer lexer;
  lexer_init(&lexer, data_json, json_length);

  Stack scopes = {0};
  Stack control = {0};

  TRY {
    // 1. PRE-SCAN
    JsonEstimate est = {0};
    jsonv_prescan((const char *)data_json, json_length, &est);
    
    // 2. CHECK LIMITS IF CONFIG
    if (ctx->config.max_depth > 0) {
      if (est.max_depth > ctx->config.max_depth) RAISE(MAXIMUM_NESTED_DEPTH_REACHED);
      if (est.value_count > ctx->config.max_values) RAISE(MAXIMUM_VALUES_REACHED);
      if (est.object_count > ctx->config.max_objects) RAISE(MAXIMUM_OBJECT_REACHED);
      if (est.array_count > ctx->config.max_array) RAISE(MAXIMUM_ARRAY_REACHED);
      if (est.string_bytes > ctx->config.max_string_bytes) RAISE(MAXIMUM_TOKEN_BYTES_REACHED);
    }
    
    // 3. ALLOCATE AST
    size_t ast_storage_size = json_length * sizeof(ASTNode);
    void *ast_storage = arena_alloc(ctx->execution_arena, ast_storage_size);
    stack_init(&ctx->data, sizeof(ASTNode), ast_storage, ast_storage_size);

    size_t scopes_storage_size = json_length * sizeof(int);
    void *scopes_storage = arena_alloc(ctx->execution_arena, scopes_storage_size);
    stack_init(&scopes, sizeof(int), scopes_storage, scopes_storage_size);

    size_t control_storage_size = json_length * sizeof(int);
    void *control_storage = arena_alloc(ctx->execution_arena, control_storage_size);
    stack_init(&control, sizeof(int), control_storage, control_storage_size);

    size_t keys_capacity = set_next_power_of_two(round(1.2 * est.value_count));
    size_t set_storage_size = sizeof(entry_t) * keys_capacity;
    entry_t *set_data = (entry_t *)arena_alloc(ctx->execution_arena, set_storage_size);
    set_init(&ctx->data_set, set_data, keys_capacity);

    size_t key_storage_size = sizeof(KeyNode) * keys_capacity;
    KeyNode *keytree_data = (KeyNode *)arena_alloc(ctx->execution_arena, key_storage_size);
    key_tree_init(&ctx->data_keytree, keytree_data, keys_capacity);

    // 4. COMPILE AST
    jsonv_ast(&lexer, &ctx->data, &scopes, &control, &ctx->data_set, &ctx->data_keytree);

    // 5. CONVERT TO VALUE
    ASTNode *pool = (ASTNode *)ctx->data.data;
    Shape *exe_root = shape_root(ctx->execution_arena);
    if (!exe_root) return false;
    Value val = ast_to_value(pool, &ctx->data_keytree, 0, exe_root, ctx->execution_arena);
    
    if (out_value) {
      *out_value = val;
    }
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
    const Jsonv_Schema *schema,
    Value data_value
) {
  /*#region*/
  assert(ctx);
  assert(schema);
  
  memset(&ctx->last_error, 0, sizeof(E));
  ctx->has_error = false;

  bool ok = validate_value(ctx, schema, 0, data_value, "", &ctx->last_error);
  if (!ok) {
    ctx->has_error = true;
  }
  return ok;
  /*#endregion*/
}

const E* jsonv_ctx_get_error(const Jsonv_Context *ctx) {
  /*#region*/
  if (!ctx) return NULL;
  return &ctx->last_error;
  /*#endregion*/
}

Arena* jsonv_ctx_arena(const Jsonv_Context *ctx) {
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
  // Reset the transient execution arena
  arena_reset(ctx->execution_arena);
  /*#endregion*/
}
