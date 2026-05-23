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

#define KB(x) 1024 * x
#define MB(x) 1024 * 1024 * x

#define ERR(exception)                                                         \
  do {                                                                         \
    c->e.path = NULL;                                                          \
    c->e.type = exception.type;                                                \
    snprintf(c->e.description, 100, "%s", exception.reason);                   \
  } while (0)

extern const Except MALFORMED_JSON;
extern const Except MAXIMUM_NESTED_DEPTH_REACHED;
extern const Except MAXIMUM_OBJECT_REACHED;
extern const Except MAXIMUM_ARRAY_REACHED;
extern const Except MAXIMUM_VALUES_REACHED;
extern const Except ARENA_LIMIT_REACHED;
extern const Except MAXIMUM_TOKEN_BYTES_REACHED;

typedef struct Jsonv_Context {
  Stack data;
  // Stack schema;
  int error;
  E e; // TODO rename
  Jsonv_Ctx_Config config;
  Arena *schema_arena; // TODO rename
  Arena *data_arena;   // TODO rename
  set_t data_set;
  set_t schema_set;
  KeyTreePool data_keytree;
  KeyTreePool schema_keytree;
} Jsonv_Context;

static void reset_error(Jsonv_Context *ctx) {
  /*#region*/
  ctx->error = -1;
  if (ctx->e.path)
    free(ctx->e.path);
  /*#endregion*/
}

bool jsonv_ctx_init(Jsonv_Context **ctx, Arena *schema_arena,
                    Jsonv_Ctx_Config *config) {
  /*#region*/
  assert(ctx);
  assert(schema_arena);
  static Jsonv_Ctx_Config def = {
      .default_block_size = KB(4),
      .max_limit = MB(1),
      .shrink_at = KB(12),
      .max_depth = 100,
      .max_values = 10000,
      .max_objects = 10000,
      .max_array = 10000,
      .max_string_bytes = 10000,
  };
  *ctx = malloc(sizeof(Jsonv_Context));
  if (*ctx == NULL)
    return false;
  memset(*ctx, 0, sizeof(Jsonv_Context));
  Jsonv_Context *c = *ctx;
  c->schema_arena=schema_arena;
  memcpy(&c->config, &def, sizeof(Jsonv_Ctx_Config));

  if (config) {
    if (config->default_block_size > 0)
      c->config.default_block_size = config->default_block_size;
    if (config->max_limit > 0)
      c->config.max_limit = config->max_limit;
    if (config->shrink_at > 0)
      c->config.shrink_at = config->shrink_at;
    if (config->max_depth > 0)
      c->config.max_depth = config->max_depth;
    if (config->max_values > 0)
      c->config.max_values = config->max_values;
    if (config->max_objects > 0)
      c->config.max_objects = config->max_objects;
    if (config->max_array > 0)
      c->config.max_array = config->max_array;
    if (config->max_string_bytes > 0)
      c->config.max_string_bytes = config->max_string_bytes;
  }
  c->data_arena = arena_new(c->config.default_block_size, c->config.max_limit,
                            c->config.shrink_at);
  if (c->data_arena == NULL) {
    free(*ctx);
    return false;
  }
  return true;
  /*#endregion*/
}

void jsonv_ctx_print_data(Jsonv_Context *ctx) {
  /*#region*/
  assert(ctx);
  assert(ctx->data.data);
  print_ast(&ctx->data, 0, 0);
  /*#endregion*/
}

void jsonv_ctx_print_schema(Jsonv_Context *ctx) {
  /*#region*/
  (void)(ctx);
  // assert(ctx->schema.data);
  // print_schema_rules(&ctx->schema, 0, 0);
  /*#endregion*/
}

bool jsonv_ctx_prepare_schema(Jsonv_Context **ctx, const unsigned char *json) {
  /*#region*/
  assert(ctx);
  assert(json);
  bool out = false;
  size_t json_length = strlen((char *)json);
  if (json_length == 0)
    return false;

  Jsonv_Context *c = *ctx;
  Arena *arena = c->schema_arena;
  reset_error(c);

  Lexer lexer;
  lexer_init(&lexer, json, json_length);

  Stack ast = {0};
  Stack scopes = {0};
  Stack control = {0};

  TRY {
    // 1. PRE-SCAN
    JsonEstimate est = {0};
    jsonv_prescan((const char *)json, json_length, &est);
    // 2. ALLOCATE SPACE FOR AST
    size_t ast_storage_size = json_length * sizeof(ASTNode);
    void *ast_storage = arena_alloc(arena, ast_storage_size);
    stack_init(&ast, sizeof(ASTNode), ast_storage, ast_storage_size);
    // 3. ALLOCATE SPACE FOR SCOPES
    size_t scopes_storage_size = json_length * sizeof(int);
    void *scopes_storage = arena_alloc(arena, scopes_storage_size);
    stack_init(&scopes, sizeof(int), scopes_storage, scopes_storage_size);
    // 4. ALLOCATE SPACE FOR CONTROL
    size_t control_storage_size = json_length * sizeof(int);
    void *control_storage = arena_alloc(arena, control_storage_size);
    stack_init(&control, sizeof(int), control_storage, control_storage_size);
    // 5. ALLOCATE SPACE FOR SET
    size_t keys_capacity = set_next_power_of_two(round(1.2 * est.value_count));
    size_t set_storage_size = sizeof(entry_t) * keys_capacity;
    entry_t *set = arena_alloc(arena, set_storage_size);
    set_init(&c->schema_set, set, keys_capacity);
    // 6. ALLOCATE SPACE FOR KEYTREE
    size_t key_storage_size = sizeof(KeyNode) * keys_capacity;
    KeyNode *keytree = arena_alloc(arena, key_storage_size);
    key_tree_init(&c->schema_keytree, keytree, keys_capacity);
    // 7. COMPILE AST
    jsonv_ast(&lexer, &ast, &scopes, &control, &c->schema_set, &c->schema_keytree);
    // 8. COMPILE SCHEMA
    int count = 0;
    SchemaRule *s = compile_schema((ASTNode*)ast.data,  0, &count); 
    print_schema_rules(s, count);
    return s ? true: false;
  }
  EXCEPT(MALFORMED_JSON) { ERR(MALFORMED_JSON); }
  EXCEPT(Mem_Failed) { ERR(Mem_Failed); }
  EXCEPT(ARENA_LIMIT_REACHED) { ERR(ARENA_LIMIT_REACHED); }
  EXCEPT(MAXIMUM_NESTED_DEPTH_REACHED) { ERR(MAXIMUM_NESTED_DEPTH_REACHED); }
  END_TRY;
  return out;
  /*#endregion*/
}

bool jsonv_ctx_prepare_data(Jsonv_Context **ctx, const unsigned char *json) {
  /*#region*/
  assert(ctx);
  assert(json);
  size_t json_length = strlen((char *)json);
  if (json_length == 0)
    return false;

  Jsonv_Context *c = *ctx;
  Arena *arena = c->data_arena;
  arena_reset(arena);
  assert(c);
  reset_error(c);
  c->data.data = NULL;

  Lexer lexer;
  lexer_init(&lexer, json, json_length);

  Stack control = {0};
  Stack scopes = {0};
  scopes.data = NULL;

  bool out = false;
  TRY {
    // 1. PRE-SCAN
    JsonEstimate est = {0};
    jsonv_prescan((const char *)json, json_length, &est);

    if (est.max_depth > c->config.max_depth)
      RAISE(MAXIMUM_NESTED_DEPTH_REACHED);

    if (est.value_count > c->config.max_values)
      RAISE(MAXIMUM_VALUES_REACHED);

    if (est.object_count > c->config.max_objects)
      RAISE(MAXIMUM_OBJECT_REACHED);

    if (est.array_count > c->config.max_array)
      RAISE(MAXIMUM_ARRAY_REACHED);

    if (est.string_bytes > c->config.max_string_bytes)
      RAISE(MAXIMUM_TOKEN_BYTES_REACHED);

    // 2. ALLOCATE SPACE FOR AST
    size_t ast_storage_size = json_length * sizeof(ASTNode);
    void *ast_storage = arena_alloc(arena, ast_storage_size);
    stack_init(&c->data, sizeof(ASTNode), ast_storage, ast_storage_size);
    // 3. ALLOCATE SPACE FOR SCOPES
    size_t scopes_storage_size = json_length * sizeof(int);
    void *scopes_storage = arena_alloc(arena, scopes_storage_size);
    stack_init(&scopes, sizeof(int), scopes_storage,
               scopes_storage_size);
    // 4. ALLOCATE SPACE FOR CONTROL
    size_t control_storage_size = json_length * sizeof(int);
    void *control_storage = arena_alloc(arena, control_storage_size);
    stack_init(&control, sizeof(int), control_storage, control_storage_size);
    // 5. ALLOCATE SPACE FOR SET
    size_t keys_capacity = set_next_power_of_two(round(1.2 * est.value_count));
    size_t set_storage_size = sizeof(entry_t) * keys_capacity;
    entry_t *set = arena_alloc(arena, set_storage_size);

    set_init(&c->data_set, set, keys_capacity);
    // 6. ALLOCATE SPACE FOR KEYTREE
    size_t key_storage_size = sizeof(KeyNode) * keys_capacity;
    KeyNode *keytree = arena_alloc(arena, key_storage_size);
    key_tree_init(&c->data_keytree, keytree, keys_capacity);
    jsonv_ast(&lexer, &c->data, &scopes, &control, &c->data_set, &c->data_keytree);
    out = true;
  }
  EXCEPT(MALFORMED_JSON) { ERR(MALFORMED_JSON); }
  EXCEPT(MAXIMUM_OBJECT_REACHED) { ERR(MAXIMUM_OBJECT_REACHED); }
  EXCEPT(MAXIMUM_ARRAY_REACHED) { ERR(MAXIMUM_ARRAY_REACHED); }
  EXCEPT(MAXIMUM_VALUES_REACHED) { ERR(MAXIMUM_VALUES_REACHED); }
  EXCEPT(MAXIMUM_TOKEN_BYTES_REACHED) { ERR(MAXIMUM_TOKEN_BYTES_REACHED); }
  EXCEPT(Mem_Failed) { ERR(Mem_Failed); }
  EXCEPT(ARENA_LIMIT_REACHED) { ERR(ARENA_LIMIT_REACHED); }
  EXCEPT(MAXIMUM_NESTED_DEPTH_REACHED) { ERR(MAXIMUM_NESTED_DEPTH_REACHED); }
  END_TRY;

  Stack *nodes = &c->data;
  ASTNode *pool = (ASTNode *)nodes->data;
  // print_ast_alphabetical(pool,&c->data_keytree,0,0);
  Value v;
  Value o = ast_to_value(pool,&c->data_keytree,0,_g_root);
  Shape * s = ((Obj*)(o.as.p))->shape;
  print_shape(s);

  if (obj_get(o.as.p , "name", &v)) {
    printf("o.name = ");
    print_value(v);
    printf("\n");
  }
  if (obj_get(o.as.p , "price", &v)) {
    printf("o.price = ");
    print_value(v);
    printf("\n");
  }
  if (obj_get(o.as.p , "description", &v)) {
    if (obj_get(v.as.p , "prices", &v)) {
      Arr *k = v.as.p;
      printf("o.description.price[0] = ");
      print_value(k->items[0]);
      printf("\n");
    }
  }
  v = value_get_path(o, "ssi", "description", "prices", 1);
  if(v.tag == VAL_DOUBLE){
  printf("o.description.price[1] = ");
  print_value(v);
  printf("\n");
  }
  return out;
  /*#endregion*/
}

int jsonv_ctx_validate(Jsonv_Context *ctx) {
  /*#region*/
  assert(ctx);
  // assert(ctx->schema.data);
  // assert(ctx->data.data);
  // Jsonv_Context *c = ctx;
  // int out;
  // TRY {
  //   reset_error(ctx);
  //   int data_root = *(int *)stack_peek(&ctx->children, 0);
  //   E error = {0};
  //   E *p = &error;
  //   out = validate_against_schema(&ctx->data, data_root, &ctx->schema, 0, &p);
  //   ctx->error = out;
  //   if (out != JSONV_SCHEMA_IS_VALID) {
  //     size_t buffSize = c->config.max_string_bytes * c->config.max_depth;
  //     char *path = get_node_path(&ctx->data, out, buffSize);
  //     if(!path) RAISE(Mem_Failed);
  //     error.path = path;
  //     ctx->e = error;
  //   }
  // }
  // EXCEPT(Mem_Failed) { ERR(Mem_Failed); }
  // EXCEPT(ARENA_LIMIT_REACHED) { ERR(ARENA_LIMIT_REACHED); }
  // END_TRY;
  // return out == JSONV_SCHEMA_IS_VALID;
  return 0;
  /*#endregion*/
}

E *jsonv_ctx_error(Jsonv_Context *ctx) {
  /*#region*/
  assert(ctx);
  return &ctx->e;
  /*#endregion*/
}

void jsonv_ctx_free(Jsonv_Context *ctx) {
  /*#region*/
  assert(ctx);
  if (ctx->e.path)
    free(ctx->e.path);
  arena_destroy(ctx->data_arena);
  free(ctx);
  /*#endregion*/
}
