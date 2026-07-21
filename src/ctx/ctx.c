#include "ctx.h"
#include "arena.internal.h"
#include "shape.internal.h"
#include "parser.h"
#include "set.h"
#include "keytree.h"
#include "mem.h"
#include "prescan.h"
#ifdef JSONV_YAML_SUPPORT
#include "yaml_lexer.h"
#include "yaml_parser.h"
#endif
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

static Jsonv_Config default_config = {
  .default_block_size = 1024,
  .max_limit = 65536,
  .shrink_at = 4096,
  .max_depth = 10,
  .max_values = 100,
  .max_objects = 100,
  .max_array = 100,
  .max_string_bytes = 1000
};

struct Jsonv_Context {
  Jsonv_Arena *execution_arena;
  Jsonv_Error last_error;
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
    Jsonv_Error *out_error
) {
  /*#region*/
  assert(schema_arena);
  assert(schema_json);
  
  if (out_error) {
    memset(out_error, 0, sizeof(Jsonv_Error));
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
    int bytecode_length = 0;
    uint8_t *bytecode = compile_schema(schema_arena, (ASTNode *)ast.data, stack_size(&ast), 0, &bytecode_length);
    if (!bytecode) {
      return NULL;
    }

    // 6. ALLOCATE SCHEMA OBJECT
    Jsonv_Schema *schema = (Jsonv_Schema *)arena_alloc(schema_arena, sizeof(Jsonv_Schema));
    if (!schema) return NULL;
    schema->bytecode = bytecode;
    schema->length = (uint32_t)bytecode_length;
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

bool jsonv_schema_compare(Jsonv_Arena *arena, const char *schema_json, size_t json_len, const uint8_t *bytecode, size_t bytecode_len){
  /*#region*/
  return is_compiled_schema_match(arena,schema_json,json_len, bytecode, bytecode_len);
  /*#endregion*/
}

/* ------------------- Context Operations ------------------- */

Jsonv_Context* jsonv_ctx_new(
    Jsonv_Arena *execution_arena,
    const Jsonv_Config *config,
    Jsonv_Arena_Error *error
) {
  /*#region*/
  if (!execution_arena){
    if(error) *error = JSONV_ARENA_ERR_INVALID_ARG;
    return NULL;
  } 
  
  Jsonv_Context *ctx = (Jsonv_Context *)jsonv_arena_alloc(execution_arena, sizeof(Jsonv_Context));

  if(!ctx){
    if(error) *error = jsonv_last_arena_error;
    return NULL;
  }
  
  ctx->execution_arena = execution_arena;
  memset(&ctx->last_error, 0, sizeof(Jsonv_Error));
  ctx->has_error = false;
  
  // Set configurations
  if (config) {
    memcpy(&ctx->config, config, sizeof(Jsonv_Config));
  } else {
    memcpy(&ctx->config, &default_config, sizeof(Jsonv_Config));
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
  
  memset(&ctx->last_error, 0, sizeof(Jsonv_Error));
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

#ifdef JSONV_YAML_SUPPORT
static void yaml_prescan(const char *s, size_t len, JsonEstimate *out) {
  /*#region*/
  memset(out, 0, sizeof(*out));
  out->max_depth = 1;
  
  bool line_start = true;
  size_t current_indent = 0;
  
  size_t start_i = 0;
  if (len >= 3 && (unsigned char)s[0] == 0xEF && (unsigned char)s[1] == 0xBB && (unsigned char)s[2] == 0xBF) {
    start_i = 3;
  }

  for (size_t i = start_i; i < len; i++) {
    char c = s[i];
    
    if (line_start) {
      if (c == ' ') {
        current_indent++;
        continue;
      }
      if (c == '\t') {
        continue;
      }
      line_start = false;
      
      size_t depth = current_indent / 2 + 1;
      if (depth > out->max_depth) {
        out->max_depth = depth;
      }
      current_indent = 0;
    }
    
    if (c == '\n' || c == '\r') {
      line_start = true;
      continue;
    }
    
    if (c == ':') {
      out->object_count++;
      out->value_count += 2;
    } else if (c == '-') {
      out->array_count++;
      out->value_count++;
    }
    
    out->string_bytes++;
  }
  
  if (out->value_count == 0) {
    out->value_count = 16;
  }
  /*#endregion*/
}

bool jsonv_ctx_parse_yaml_data(
    Jsonv_Context *ctx,
    const unsigned char *data_yaml
) {
  /*#region*/
  assert(ctx);
  assert(data_yaml);
  
  memset(&ctx->last_error, 0, sizeof(Jsonv_Error));
  ctx->has_error = false;
  
  size_t yaml_length = strlen((char *)data_yaml);
  if (yaml_length == 0) {
    ctx->last_error.type = Jsonv_Malformed_json;
    snprintf(ctx->last_error.description, sizeof(ctx->last_error.description), "Empty YAML");
    ctx->has_error = true;
    return false;
  }
  
  YamlLexer lexer;
  yaml_lexer_init(&lexer, data_yaml, yaml_length);

  TRY {
    // 1. PRE-SCAN
    JsonEstimate est = {0};
    yaml_prescan((const char *)data_yaml, yaml_length, &est);
    
    // 2. CHECK LIMITS IF CONFIG
    if (ctx->config.max_depth > 0) {
      if (est.max_depth > ctx->config.max_depth) RAISE(MAXIMUM_NESTED_DEPTH_REACHED);
      if (est.value_count > ctx->config.max_values) RAISE(MAXIMUM_VALUES_REACHED);
      if (est.object_count > ctx->config.max_objects) RAISE(MAXIMUM_OBJECT_REACHED);
      if (est.array_count > ctx->config.max_array) RAISE(MAXIMUM_ARRAY_REACHED);
      if (est.string_bytes > ctx->config.max_string_bytes) RAISE(MAXIMUM_TOKEN_BYTES_REACHED);
    }
    
    // 3. COMPILE AST
    yaml_parse_to_ast(ctx->execution_arena, &lexer, yaml_length, est.value_count, &ctx->data, &ctx->data_keytree, &ctx->data_set);

    return true;
  }
  EXCEPT(MALFORMED_JSON) {
    ctx->last_error.type = Jsonv_Malformed_json;
    snprintf(ctx->last_error.description, sizeof(ctx->last_error.description), "Malformed YAML");
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
#endif

bool jsonv_ctx_validate(
    Jsonv_Context *ctx,
    const Jsonv_Schema *schema
) {
  /*#region*/
  assert(ctx);
  assert(schema);
  
  memset(&ctx->last_error, 0, sizeof(Jsonv_Error));
  ctx->has_error = false;

  if (ctx->data.top < 0) {
    ctx->last_error.type = Jsonv_Malformed_json;
    snprintf(ctx->last_error.description, sizeof(ctx->last_error.description), "Empty AST");
    ctx->has_error = true;
    return false;
  }

  bool ok = validate_bytecode(ctx, (ASTNode *)ctx->data.data, schema, sizeof(BytecodeHeader), 0, "", &ctx->last_error);
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
    Shape *exe_root = shape_root();
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
  }
  END_TRY;
  
  return false;
  /*#endregion*/
}

const Jsonv_Error* jsonv_ctx_get_error(const Jsonv_Context *ctx) {
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
  memset(&ctx->last_error, 0, sizeof(Jsonv_Error));
  ctx->has_error = false;
  // Reset the transient execution arena but preserve the Jsonv_Context allocation
  jsonv_arena_reset_to(ctx->execution_arena, sizeof(Jsonv_Context));
  // Clear the thread-local recycled free lists to prevent dangling pointer references
  jsonv_shape_clear_free_lists();
  /*#endregion*/
}
