#include "ctx.h"
#include "ast.h"
#include "error.h"
#include "mem.h"
#include "prescan.h"
#include "schema.h"
#include "validate.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_JSON_DEPTH 100
#define MAX_AST_NODES 10000
#define MAX_TOKEN_BYTES 10000

#define ERR(fmt, ...)                                                          \
  do {                                                                         \
    jsonv_path_reset(path);                                                    \
    JSONV_ENTER_FIELD(path, "$");                                              \
    JSONV_ERR(errors, path, fmt, ##__VA_ARGS__);                               \
    JSONV_LEAVE(path);                                                         \
  } while (0)

extern const Except MALFORMED_JSON;
extern const Except MAXIMUM_NESTED_DEPTH_REACHED;
extern const Except ARENA_LIMIT_REACHED;
extern const Except MAXIMUM_TOKEN_BYTES_REACHED;
extern const Except MAXIMUM_AST_NODE_REACHED;
extern const Except Mem_Failed;

typedef struct Jsonv_Context {
  Jsonv_error_stack errors;
  Jsonv_path path;
  Stack schema;
  Stack data;
  Stack children;
  size_t allowed_errors_count;
  int error;
} Jsonv_Context;

void jsonv_ctx_print_data(Jsonv_Context *ctx) {
  /*#region*/
  assert(ctx);
  assert(ctx->data.data);
  print_ast(&ctx->data, ctx->data.top, 0);
  /*#endregion*/
}

void jsonv_ctx_print_schema(Jsonv_Context *ctx) {
  /*#region*/
  assert(ctx->schema.data);
  print_schema(&ctx->schema, 0, 0);
  /*#endregion*/
}

bool jsonv_ctx_prepare_schema(Jsonv_Context **ctx, const unsigned char *json,
                              Arena *arena) {
  /*#region*/
  assert(ctx);
  assert(json);
  assert(arena);
  bool out = false;
  if (*ctx == NULL) {
    *ctx = CALLOC(1, sizeof(Jsonv_Context));
    memset(*ctx, 0, sizeof(Jsonv_Context));
  }
  size_t json_length = strlen((char *)json);
  if (json_length == 0)
    return false;

  Jsonv_Context *c = *ctx;
  c->schema.data = NULL;
  Jsonv_path *path = &c->path;
  Jsonv_error_stack *errors = &c->errors;

  Lexer lexer;
  lexer_init(&lexer, json, json_length);

  Stack ast = {0};
  Stack children = {0};
  Stack control = {0};
  Stack control2 = {0};
  Stack results = {0};

  TRY {
    // 1. ALLOCATE SPACE FOR AST
    size_t ast_storage_size = json_length * sizeof(ASTNode);
    void *ast_storage = arena_alloc(arena, ast_storage_size);
    stack_init(&ast, sizeof(ASTNode), ast_storage, ast_storage_size);
    // 2. ALLOCATE SPACE FOR CHILDREN
    size_t children_storage_size = json_length * sizeof(int);
    void *children_storage = arena_alloc(arena, children_storage_size);
    stack_init(&children, sizeof(int), children_storage, children_storage_size);
    // 3. ALLOCATE SPACE FOR CONTROL
    size_t control_storage_size = json_length * sizeof(int);
    void *control_storage = arena_alloc(arena, control_storage_size);
    stack_init(&control, sizeof(int), control_storage, control_storage_size);

    jsonv_ast(&lexer, &ast, &children, &control);
    // 4. ALLOCATE SPACE FOR ASTSCHEMA
    size_t schema_storage_size = json_length * sizeof(SchemaNode);
    void *schema_storage = arena_alloc(arena, schema_storage_size);
    stack_init(&c->schema, sizeof(SchemaNode), schema_storage,
               schema_storage_size);
    // 5. ALLOCATE SPACE FOR CONTROL2
    size_t control2_storage_size = json_length * sizeof(SchemaControl);
    void *control2_storage = arena_alloc(arena, control2_storage_size);
    stack_init(&control2, sizeof(SchemaControl), control2_storage,
               control2_storage_size);
    // 6. ALLOCATE SPACE FOR RESULTS
    size_t results_storage_size = json_length * sizeof(int);
    void *results_storage = arena_alloc(arena, results_storage_size);
    stack_init(&results, sizeof(int), results_storage, results_storage_size);

    int json_root = *(int *)stack_peek(&children, 0);
    out = parse_schema_stack(&ast, json_root, &c->schema, &control2,
                             &results) == 0
              ? true
              : false;
  }
  EXCEPT(MALFORMED_JSON) { ERR("Malformed json."); }
  EXCEPT(Mem_Failed) { ERR("Heap memory allocation failed."); }
  EXCEPT(ARENA_LIMIT_REACHED) { ERR("Arena limit reached"); }
  EXCEPT(MAXIMUM_NESTED_DEPTH_REACHED) { ERR("Maximum nested depth reached."); }
  END_TRY;

  return out;
  /*#endregion*/
}

bool jsonv_ctx_prepare_data(Jsonv_Context **ctx, const unsigned char *json,
                            Arena *arena) {
  /*#region*/
  assert(ctx);
  assert(json);
  assert(arena);
  if (*ctx == NULL) {
    *ctx = CALLOC(1, sizeof(Jsonv_Context));
    memset(*ctx, 0, sizeof(Jsonv_Context));
  }
  size_t json_length = strlen((char *)json);
  if (json_length == 0)
    return false;

  Jsonv_Context *c = *ctx;
  c->data.data = NULL;
  Jsonv_path *path = &c->path;
  Jsonv_error_stack *errors = &c->errors;

  Lexer lexer;
  lexer_init(&lexer, json, json_length);

  c->children.data = NULL;
  Stack control = {0};

  bool out = false;
  TRY {
    // 1. PRE-SCAN
    JsonEstimate est = {0};
    jsonv_prescan((const char *)json, json_length, &est);

    if (est.max_depth > MAX_JSON_DEPTH)
      RAISE(MAXIMUM_NESTED_DEPTH_REACHED);

    if (est.value_count + est.object_count + est.array_count > MAX_AST_NODES)
      RAISE(MAXIMUM_AST_NODE_REACHED);

    if (est.string_bytes > MAX_TOKEN_BYTES)
      RAISE(MAXIMUM_TOKEN_BYTES_REACHED);

    // 2. ALLOCATE SPACE FOR AST
    size_t ast_storage_size = json_length * sizeof(ASTNode);
    void *ast_storage = arena_alloc(arena, ast_storage_size);
    stack_init(&c->data, sizeof(ASTNode), ast_storage, ast_storage_size);
    // 3. ALLOCATE SPACE FOR CHILDREN
    size_t children_storage_size = json_length * sizeof(int);
    void *children_storage = arena_alloc(arena, children_storage_size);
    stack_init(&c->children, sizeof(int), children_storage,
               children_storage_size);
    // 4. ALLOCATE SPACE FOR CONTROL
    size_t control_storage_size = json_length * sizeof(int);
    void *control_storage = arena_alloc(arena, control_storage_size);
    stack_init(&control, sizeof(int), control_storage, control_storage_size);
    jsonv_ast(&lexer, &c->data, &c->children, &control);
    out = true;
  }
  EXCEPT(MALFORMED_JSON) { ERR("Malformed json."); }
  EXCEPT(MAXIMUM_AST_NODE_REACHED) { ERR("Maximum ast node reached."); }
  EXCEPT(MAXIMUM_TOKEN_BYTES_REACHED) { ERR("Maximum token bytes reached."); }
  EXCEPT(Mem_Failed) { ERR("Heap memory allocation failed."); }
  EXCEPT(ARENA_LIMIT_REACHED) { ERR("Arena limit reached"); }
  EXCEPT(MAXIMUM_NESTED_DEPTH_REACHED) { ERR("Maximum nested depth reached."); }
  END_TRY;

  return out;
  /*#endregion*/
}

int jsonv_ctx_validate(Jsonv_Context *ctx) {
  /*#region*/
  assert(ctx);
  assert(ctx->schema.data);
  assert(ctx->data.data);
  int data_root = *(int *)stack_peek(&ctx->children, 0);
  E error = {0};
  E *p = &error;
  int out = validate_against_schema(&ctx->data, data_root, &ctx->schema, 0, &p);
  ctx->error = out;
  if (out != JSONV_SCHEMA_IS_VALID) {
    char *path = get_node_path(&ctx->data, out);
    printf("%s %s\n", path, error.description);
  }
  return out == JSONV_SCHEMA_IS_VALID;
  /*#endregion*/
}

Jsonv_error_stack *jsonv_ctx_errors(Jsonv_Context *ctx) {
  /*#region*/
  assert(ctx);
  return &ctx->errors;
  /*#endregion*/
}

void jsonv_ctx_free(Jsonv_Context *ctx) {
  /*#region*/
  assert(ctx);
  free(ctx);
  /*#endregion*/
}
