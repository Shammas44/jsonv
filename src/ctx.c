#include "ctx.h"
#include "ast.h"
#include "error.h"
#include "mem.h"
#include "print.h"
#include "schema.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define ERR(fmt, ...)                                                          \
  do {                                                                         \
    jsonv_path_reset(path);                                                    \
    JSONV_ENTER_FIELD(path, "$");                                              \
    JSONV_ERR(errors, path, fmt, ##__VA_ARGS__);                               \
    JSONV_LEAVE(path);                                                         \
  } while (0)

extern const Except MALFORMED_JSON;
extern const Except MAXIMUM_NESTED_DEPTH_REACHED;

typedef struct Jsonv_Context {
  Jsonv_error_stack errors;
  Jsonv_path path;
  const Jsonv_SchemaNode *schema;
  Stack data;
  size_t allowed_errors_count;
} Jsonv_Context;

void jsonv_ctx_print_data(Jsonv_Context *ctx) {
  /*#region*/
  assert(ctx);
  assert(ctx->data.data);
  print_ast(&ctx->data, ctx->data.top, 0);
  /*#endregion*/
}

void jsonv_ctx_print_schema(Jsonv_Context *ctx, const char *json) {
  /*#region*/
  assert(ctx);
  assert(ctx->schema);
  jsonv_print_schema_internal(ctx->schema, 0, json);
  /*#endregion*/
}

int jsonv_ctx_prepare_schema(Jsonv_Context **ctx, const char *json,
                             jsmntok_t **tok) {
  /*#region*/
  assert(ctx);
  assert(json);
  assert(tok);
  // if (*ctx == NULL) {
  //   *ctx = CALLOC(1, sizeof(Jsonv_Context));
  // }
  // Jsonv_Context *c = *ctx;
  // Jsonv_error_stack *errors = &c->errors;
  // Jsonv_path *path = &c->path;

  // int tok_count = tokenize(json, tok);
  // if (tok_count < 1) {
  //   ERR("Unable to tokenize json schema.");
  //   return -1;
  // }

  // Jsonv_SchemaNode *ast = parse_schema(json, tok, tok_count, path, errors);

  // if (errors->count > 0) {
  //   return -4;
  // }

  // if (!ast) {
  //   ERR("Unable to parse json schema.");
  //   return -2;
  // }

  // ctx[0]->schema = ast;
  // return tok_count;
  return 0;
  /*#endregion*/
}

bool jsonv_ctx_prepare_data(Jsonv_Context **ctx, const unsigned char *json,
                           Arena *arena) {
  /*#region*/
  assert(ctx);
  assert(json);
  if (*ctx == NULL) {
    *ctx = CALLOC(1, sizeof(Jsonv_Context));
  }
  Jsonv_Context *c = *ctx;
  Jsonv_error_stack *errors = &c->errors;
  Jsonv_path *path = &c->path;
  size_t json_length = strlen((char*)json);
  if(json_length==0) return false;

  // 1. ALLOCATE SPACE FOR AST
  size_t ast_storage_size = json_length * sizeof(ASTNode);
  void *ast_storage = arena_alloc(arena, ast_storage_size);
  stack_init(&c->data, sizeof(ASTNode), ast_storage, ast_storage_size);
  // 2. ALLOCATE SPACE FOR CHILDREN
  size_t children_storage_size = json_length * sizeof(int);
  Stack children;
  void *children_storage = arena_alloc(arena, children_storage_size);
  stack_init(&children, sizeof(int), children_storage, children_storage_size);
  // 3. ALLOCATE SPACE FOR CONTROL
  size_t control_storage_size = json_length * sizeof(int);
  Stack control;
  void *control_storage = arena_alloc(arena, control_storage_size);
  stack_init(&control, sizeof(int), control_storage, control_storage_size);
  // 4. PREPARE LEXER
  Lexer *lexer = ALLOC(lexer_sizeof());
  lexer_init(&lexer, json, json_length);

  bool out;
  TRY {
  out = jsonv_ast(lexer, &c->data, &control, &children);
  }
  EXCEPT(MALFORMED_JSON) { ERR("Malformed json."); }
  EXCEPT(MAXIMUM_NESTED_DEPTH_REACHED) { ERR("Maximum nested depth reached."); }
  END_TRY;

  lexer_free(&lexer);

  return out;
  /*#endregion*/
}

int jsonv_ctx_validate(Jsonv_Context *ctx, const char *schema,
                       const char *data) {
  /*#region*/
  assert(ctx);
  assert(ctx->schema);
  assert(ctx->data.data);
  assert(schema);
  assert(data);
  // const Jsonv_SchemaNode *s = ctx->schema;
  // const Jsonv_DataNode *d = ctx->data;
  // int e = jsonv_validate(schema, s, data, d, &ctx->path, &ctx->errors);
  // jsonv_data_free((Jsonv_DataNode *)ctx->data);
  // ctx->data = NULL;
  // return !e;
  return 0;
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
  if (ctx->schema) {
    jsonv_schema_free((Jsonv_SchemaNode *)ctx->schema);
  }
  free(ctx);
  /*#endregion*/
}

// static Jsonv_SchemaNode *parse_schema(const char *json, jsmntok_t **tok,
//                                       int tok_count, Jsonv_path *path,
//                                       Jsonv_error_stack *errors) {
//   /*#region*/
//   jsonv_tokiterator *iterator = jsonv_tokiterator_new(json, tok, tok_count);
//   jsmntok_t *token = jsonv_tokiterator_current(iterator);
//   jsmntype_t type = token->type;
//   if (type != JSMN_OBJECT)
//     return NULL;
//   Jsonv_SchemaNode *schema =
//       jsonv_compile_schema(json, iterator, NULL, path, errors);
//   jsonv_tokiterator_free(&iterator);
//   return schema;
//   /*#endregion*/
// }
