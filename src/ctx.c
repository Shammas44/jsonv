#include "ctx.h"
#include "compile.h"
#include "data.h"
#include "error.h"
#include "print.h"
#include "schema.h"
#include "token.h"
#include "validate.h"
#include <assert.h>
#include <jsmn/jsmn.h>
#include <logger/logger.h>
#include <memd/memd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct Jsonv_Context {
  Jsonv_error_stack errors;
  Jsonv_path path;
  const Jsonv_SchemaNode *schema;
  const Jsonv_DataNode *data;
} Jsonv_Context;

static int tokenize(const char *json, jsmntok_t **tokens);
static Jsonv_SchemaNode *parse_schema(const char *json, jsmntok_t **tok,
                                      int tok_count);
static Jsonv_DataNode *parse_data(const char *json, jsmntok_t **tok,
                                  int tok_count);
static Jsonv_Context *ctx_new(Jsonv_Context **ctx);

void jsonv_ctx_print_data(Jsonv_Context *ctx, const char *json) {
  /*#region*/
  assert(ctx);
  assert(ctx->data);
  jsonv_print_data_internal(ctx->data, 0, json);
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
  Jsonv_Context *c = *ctx ? *ctx : ctx_new(ctx);
  Jsonv_error_stack errors = c->errors;
  Jsonv_path path = c->path;

  int tok_count = tokenize(json, tok);
  if (tok_count < 1) {
    JSONV_ENTER_FIELD(&path, "");
    JSONV_ERR(&errors, &path, "Unable to tokenize json schema.");
    JSONV_LEAVE(&path);
    return 1;
  }

  Jsonv_SchemaNode *ast = parse_schema(json, tok, tok_count);

  if (!ast) {
    JSONV_ENTER_FIELD(&path, "");
    JSONV_ERR(&errors, &path, "Unable to parse json schema.");
    JSONV_LEAVE(&path);
    return 1;
  }
  ctx[0]->schema = ast;
  return tok_count;
  /*#endregion*/
}

int jsonv_ctx_prepare_data(Jsonv_Context **ctx, const char *json,
                           jsmntok_t **tok) {
  /*#region*/
  assert(ctx);
  assert(json);
  assert(tok);
  Jsonv_Context *c = *ctx ? *ctx : ctx_new(ctx);
  Jsonv_error_stack errors = c->errors;
  Jsonv_path path = c->path;

  int tok_count = tokenize(json, tok);
  if (tok_count < 1) {
    JSONV_ENTER_FIELD(&path, "");
    JSONV_ERR(&errors, &path, "Unable to tokenize json data.");
    JSONV_LEAVE(&path);
    return tok_count;
  }

  Jsonv_DataNode *ast = parse_data(json, tok, tok_count);

  if (!ast) {
    JSONV_ENTER_FIELD(&path, "");
    JSONV_ERR(&errors, &path, "Unable to parse json data.");
    JSONV_LEAVE(&path);
    return tok_count;
  }
  ctx[0]->data = ast;
  return tok_count;
  /*#endregion*/
}

int jsonv_ctx_validate(Jsonv_Context *ctx, const char *schema,
                       const char *data) {
  /*#region*/
  assert(ctx);
  assert(ctx->schema);
  assert(ctx->data);
  assert(schema);
  assert(data);
  const Jsonv_SchemaNode *s = ctx->schema;
  const Jsonv_DataNode *d = ctx->data;
  int e = jsonv_validate(schema, s, data, d, &ctx->path, &ctx->errors);
  jsonv_data_free((Jsonv_DataNode *)ctx->data);
  ctx->data = NULL;
  return !e;
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
  if (ctx->schema){
    jsonv_schema_free((Jsonv_SchemaNode*)ctx->schema);
  }
  free(ctx);
  /*#endregion*/
}

static Jsonv_Context *ctx_new(Jsonv_Context **ctx) {
  /*#region*/
  *ctx = (Jsonv_Context *)calloc(1, sizeof(Jsonv_Context));
  Jsonv_path path;
  jsonv_path_reset(&path);
  Jsonv_error_stack errors;
  jsonv_error_reset(&errors);
  ctx[0]->path = path;
  ctx[0]->path = path;
  ctx[0]->errors = errors;
  return *ctx;
  /*#endregion*/
}

static int tokenize(const char *json, jsmntok_t **tokens) {
  /*#region*/
  assert(json);
  assert(tokens);
  jsmn_parser parser;
  jsmn_init(&parser);

  int required_tokens = jsmn_parse(&parser, json, strlen(json), NULL, 0);

  *tokens = malloc(sizeof(jsmntok_t) * required_tokens);
  if (!*tokens) {
    return 0;
  }

  jsmn_init(&parser);
  int token_num =
      jsmn_parse(&parser, json, strlen(json), *tokens, required_tokens);

  if (token_num < 1) {
    free(*tokens);
    *tokens = NULL;
    return token_num;
  }

  return token_num;
  /*#endregion*/
}

static Jsonv_SchemaNode *parse_schema(const char *json, jsmntok_t **tok,
                                      int tok_count) {
  /*#region*/
  jsonv_tokiterator *iterator = jsonv_tokiterator_new(json, tok, tok_count);
  jsmntok_t *token = jsonv_tokiterator_current(iterator);
  jsmntype_t type = token->type;
  if (type != JSMN_OBJECT)
    return NULL;
  Jsonv_SchemaNode *schema = jsonv_compile_schema(json, iterator);
  jsonv_tokiterator_free(&iterator);
  return schema;
  /*#endregion*/
}

static Jsonv_DataNode *parse_data(const char *json, jsmntok_t **tok,
                                  int tok_count) {
  /*#region*/
  jsonv_tokiterator *iterator = jsonv_tokiterator_new(json, tok, tok_count);
  jsmntok_t *token = jsonv_tokiterator_current(iterator);
  jsmntype_t type = token->type;
  if (type != JSMN_OBJECT)
    return NULL;
  Jsonv_DataNode *data = jsonv_compile_data(json, iterator, NULL);
  jsonv_tokiterator_free(&iterator);
  return data;
  /*#endregion*/
}
